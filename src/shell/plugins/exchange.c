/************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* exchange.c - sync local dict across shells
 *
 * Derived from shell/pmi/pmi_exchange.c in flux-core.
 *
 * This version concatenates json arrays of base64 strings (in random order)
 * instead of merging json dictionaries, to fit the pmix fence data model.
 * In addition, the interfaces are made more convenient for use by pmix
 * callbacks: entry function takes a json base64 string, while exit callback
 * accessor gets the fully decoded, concatenated blob.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <jansson.h>
#include <flux/core.h>
#include <flux/shell.h>

#include "codec.h"

#include "exchange.h"

#define DEFAULT_TREE_K 2

struct session {
    json_t *data_in;                // arrays of gathered base64 strings
    json_t *data_out;
    exchange_exit_f exit_cb;        // callback for exchange completion
    void *exit_cb_arg;

    struct flux_msglist *requests;  // pending requests from children
    flux_future_t *f;               // pending request to parent

    struct exchange *xcg;
    bool local;                     // exchange() was called on this shell
    bool has_error;                 // an error occurred
};

struct exchange {
    flux_shell_t *shell;
    int size;
    int rank;
    uint32_t parent_rank;
    int child_count;

    struct session *session;
};

static void exchange_response_completion (flux_future_t *f, void *arg);

/* borrow a couple of well tested functions from kary.c in flux-core
 */
#define KARY_NONE   (~(uint32_t)0)
// Return the parent of i or KARY_NONE if i has no parent.
static uint32_t kary_parentof (int k, uint32_t i)
{
    if (i == 0 || k <= 0)
        return KARY_NONE;
    if (k == 1)
        return i - 1;
    return (k + (i + 1) - 2) / k - 1;
}
// Return the jth child of i or KARY_NONE if i has no such child.
static uint32_t kary_childof (int k, uint32_t size, uint32_t i, int j)
{
    uint32_t n;

    if (k > 0 && j >= 0 && j < k) {
        n = k*(i + 1) - (k - 2) + j - 1;
        if (n < size)
            return n;
    }
    return KARY_NONE;
}

static void session_destroy (struct session *ses)
{
    if (ses) {
        int saved_errno = errno;
        flux_msglist_destroy (ses->requests);
        flux_future_destroy (ses->f);
        json_decref (ses->data_in);
        json_decref (ses->data_out);
        free (ses);
        errno = saved_errno;
    }
}

static struct session *session_create (struct exchange *xcg)
{
    struct session *ses;

    if (!(ses = calloc (1, sizeof (*ses))))
        return NULL;
    ses->xcg = xcg;
    if (!(ses->requests = flux_msglist_create ()))
        goto error;
    if (!(ses->data_in = json_array ())) {
        errno = ENOMEM;
        goto error;
    }
    return ses;
error:
    session_destroy (ses);
    return NULL;
}

static void session_process (struct session *ses)
{
    struct exchange *xcg = ses->xcg;
    flux_t *h = flux_shell_get_flux (ses->xcg->shell);
    const flux_msg_t *msg;

    if (ses->has_error)
        goto done;

    /* Awaiting self or child input?
     */
    if (!ses->local || flux_msglist_count (ses->requests) < xcg->child_count)
        return;

    /* Send exchange request, if needed.
     */
    if (xcg->rank > 0 && !ses->f) {
        flux_future_t *f;

        if (!(f = flux_shell_rpc_pack (xcg->shell,
                                       "pmix-exchange",
                                       xcg->parent_rank,
                                       0,
                                       "{s:O}",
                                       "data", ses->data_in))
                || flux_future_then (f,
                                     -1,
                                     exchange_response_completion,
                                     xcg) < 0) {
            flux_future_destroy (f);
            shell_warn ("error sending pmix-exchange request");
            ses->has_error = 1;
            goto done;
        }
        ses->f = f;
    }

    /* Awaiting parent response?
     */
    if (ses->f && !flux_future_is_ready (ses->f))
        return;

    if (xcg->rank == 0);
        ses->data_out = json_incref (ses->data_in);

    /* Send exchange response(s), if needed.
     */
    while ((msg = flux_msglist_pop (ses->requests))) {
        if (flux_respond_pack (h, msg, "{s:O}", "data", ses->data_out) < 0) {
            shell_warn ("error responding to pmix-exchange request");
            flux_msg_decref (msg);
            ses->has_error = 1;
            goto done;
        }
        flux_msg_decref (msg);
    }
done:
    ses->exit_cb (xcg, ses->exit_cb_arg);
    session_destroy (ses);
    xcg->session = NULL;
}

/* parent shell has responded to pmix-exchange request.
 */
static void exchange_response_completion (flux_future_t *f, void *arg)
{
    struct exchange *xcg = arg;

    if (flux_rpc_get_unpack (f, "{s:O}", "data", &xcg->session->data_out) < 0) {
        shell_warn ("pmix-exchange request: %s", future_strerror (f, errno));
        xcg->session->has_error = 1;
    }
    session_process (xcg->session);
}

/* child shell sent a pmix-exchange request
 */
static void exchange_request_cb (flux_t *h,
                                 flux_msg_handler_t *mh,
                                 const flux_msg_t *msg,
                                 void *arg)
{
    struct exchange *xcg = arg;
    json_t *data;
    const char *errstr = NULL;

    if (flux_request_unpack (msg, NULL, "{s:o}", "data", &data) < 0)
        goto error;
    if (!xcg->session) {
        if (!(xcg->session = session_create (xcg)))
            goto error;
    }
    if (flux_msglist_count (xcg->session->requests) == xcg->child_count) {
        errstr = "exchange received too many child requests";
        errno = EINPROGRESS;
        goto error;
    }
    if (json_array_extend (xcg->session->data_in, data) < 0) {
        errstr = "exchange request failed to extend data_in array";
        errno = ENOMEM;
        goto error;
    }
    if (flux_msglist_append (xcg->session->requests, msg) < 0) {
        errstr = "exchange request failed to save pending request";
        goto error;
    }
    session_process (xcg->session);
    return;
error:
    if (flux_respond_error (h, msg, errno, errstr) < 0)
        shell_warn ("error responding to pmix-exchange request: %s",
                    flux_strerror (errno));
}

/* this shell is ready to exchange.
 */
int exchange_enter_base64_string (struct exchange *xcg,
                                  json_t *data,
                                  exchange_exit_f exit_cb,
                                  void *exit_cb_arg)
{
    if (!xcg
        || !data
        || !exit_cb
        || !json_is_string (data)) {
        errno = EINVAL;
        return -1;
    }
    if (!xcg->session) {
        if (!(xcg->session = session_create (xcg)))
            return -1;
    }
    if (xcg->session->local) {
        errno = EINPROGRESS;
        return -1;
    }
    xcg->session->exit_cb = exit_cb;
    xcg->session->exit_cb_arg = exit_cb_arg;
    xcg->session->local = 1;
    if (json_array_append (xcg->session->data_in, data) < 0) {
        errno = ENOMEM;
        return -1;
    }
    session_process (xcg->session);
    return 0;
}

/* Helper for exchange_create() - calculate the number of children of
 * 'rank' in a 'size' tree of degree 'k'.
 */
static int child_count (int k, int rank, int size)
{
    int i;
    int count = 0;

    for (i = 0; i < k; i++) {
        if (kary_childof (k, size, rank, i) != KARY_NONE)
            count++;
    }
    return count;
}

struct exchange *exchange_create (flux_shell_t *shell, int k)
{
    struct exchange *xcg;

    if (!(xcg = calloc (1, sizeof (*xcg))))
        return NULL;
    xcg->shell = shell;
    if (flux_shell_info_unpack (shell,
                                "{s:i s:i}",
                                "size", &xcg->size,
                                "rank", &xcg->rank) < 0)
        goto error;
    if (k <= 0)
        k = DEFAULT_TREE_K;
    else if (k > xcg->size) {
        k = xcg->size;
        if (xcg->rank == 0)
            shell_warn ("requested exchange fanout too large, using k=%d", k);
    }
    else {
        if (xcg->rank == 0)
            shell_debug ("using k=%d", k);
    }
    xcg->parent_rank = kary_parentof (k, xcg->rank);
    xcg->child_count = child_count (k, xcg->rank, xcg->size);

    if (flux_shell_service_register (shell,
                                     "pmix-exchange",
                                     exchange_request_cb,
                                     xcg) < 0)
        goto error;
    return xcg;
error:
    exchange_destroy (xcg);
    return NULL;
}

void exchange_destroy (struct exchange *xcg)
{
    if (xcg) {
        int saved_errno = errno;
        session_destroy (xcg->session);
        free (xcg);
        errno = saved_errno;
    }
}

bool exchange_has_error (struct exchange *xcg)
{
    return xcg->session->has_error ? true : false;
}

/* Convert internal array of base64 json strings to one continguous
 * data blob that the caller must free.
 */
int exchange_get_data (struct exchange *xcg, void **datap, size_t *sizep)
{
    size_t bufsize = 0;
    size_t index;
    json_t *value;
    uint8_t *data = NULL;
    size_t size = 0;

    // compute buffer size and allocate
    json_array_foreach (xcg->session->data_out, index, value) {
        ssize_t chunksize = codec_data_decode_bufsize (value);
        if (chunksize < 0)
            return -1;
        bufsize += chunksize;
    }
    if (!(data = malloc (bufsize)))
        return -1;

    // decode array to buffer
    json_array_foreach (xcg->session->data_out, index, value) {
        ssize_t chunksize = codec_data_decode_tobuf (value,
                                                     data + size,
                                                     bufsize - size);
        if (chunksize < 0)
            goto error;
        size += chunksize;
    }
    *datap = data;
    *sizep = size;
    return 0;
error:
    free (data);
    return -1;
}

/* vi: ts=4 sw=4 expandtab
 */
