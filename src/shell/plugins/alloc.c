/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* alloc.c - handle alloc callback from openpmix server
 *
 * User calls PMIx_Allocation_request (directive, info, ninfo, results, nresults);
 *
 * This is forwarded to the scheduler.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <jansson.h>
#include <flux/core.h>
#include <flux/shell.h>
#include <pmix.h>
#include <pmix_server.h>

#include "codec.h"
#include "interthread.h"

#include "alloc.h"

struct alloc {
    flux_shell_t *shell;
    struct interthread *it;
    int trace_flag;
};

struct alloc_caddy_t {
    pmix_info_t *results;
    size_t nresults;
    pmix_info_cbfunc_t cbfunc;
    void *cbdata;
};

/* This is for the benefit of server callbacks that don't have
 * a way to be passed a user-supplied opaque pointer.
 */
static struct alloc *global_alloc_ctx;

void pmix_release_cbfunc (void *data){
    struct alloc_caddy_t *alloc_caddy = (struct alloc_caddy_t *) data;
    if(0 < alloc_caddy->nresults) {
        PMIX_INFO_FREE(alloc_caddy->results, alloc_caddy->nresults);
    }
    free(alloc_caddy);
}

/* Receives REQUEST for dynamic allocation from pmix_server thread.
 * TODO: Forward REQUEST to shell
 * For now, we immediately send a RESPONSE back to the client
 */
static void alloc_shell_cb (const flux_msg_t *msg, void *arg)
{
    json_t *xcbfunc = NULL;
    json_t *xcbdata = NULL;
    json_t *xdata = NULL;
    pmix_alloc_directive_t directive;
    pmix_info_t *data;
    size_t ndata;
    pmix_info_cbfunc_t cbfunc;
    void *cbdata;
    const char *nspace;
    int rank;

    struct alloc_caddy_t *alloc_caddy;

    shell_debug("handling allocation request");

    if(0 > flux_msg_unpack (msg,
                         "{s:s s:i s:i s:O s:O s:O}",
                         "nspace", &nspace,
                         "rank", &rank,
                         "directive", &directive,
                         "data", &xdata,
                         "cbfunc", &xcbfunc,
                         "cbdata", &xcbdata)
            || 0 > codec_info_array_decode (xdata, &data, &ndata)
            || 0 > codec_pointer_decode (xcbfunc, (void **)&cbfunc)
            || 0 > codec_pointer_decode (xcbdata, &cbdata)){
        shell_warn ("error unpacking interthread alloc_upcall message");
        return;
    }

    /* TODO: Once flux shell supports dynamic allocations, forward to shell 0.
     * For now, just echo back the provided attriutes to the client.
     */
    alloc_caddy = malloc(sizeof(struct alloc_caddy_t));
    alloc_caddy->results = data;
    alloc_caddy->nresults = ndata;
    alloc_caddy->cbfunc = cbfunc;
    alloc_caddy->cbdata = cbdata;

    if(NULL != alloc_caddy->cbfunc){
        shell_debug("calling back into PMIx server with alloc response");
        alloc_caddy->cbfunc(PMIX_SUCCESS,
                            alloc_caddy->results,
                            alloc_caddy->nresults,
                            alloc_caddy->cbdata,
                            pmix_release_cbfunc,
                            (void *) alloc_caddy);
    }
    shell_debug("done handling allocation request from client %s:%d\n", nspace, rank);
}


/* Recives REQUEST for dynamic allocation request from client.
 * Forwards REQUEST to shell thread
 */
int alloc_server_cb (const pmix_proc_t *client,
                    pmix_alloc_directive_t directive,
                    const pmix_info_t data[], size_t ndata,
                    pmix_info_cbfunc_t cbfunc, void *cbdata)
{
    struct alloc *alloc = global_alloc_ctx;
    json_t *xdata = NULL;
    json_t *xcbfunc = NULL;
    json_t *xcbdata = NULL;
    int rc = PMIX_SUCCESS;

    shell_debug("recived allocation request from %s:%d", client->nspace, client->rank);

    if (!(xdata = codec_info_array_encode (data, ndata))
        || !(xcbfunc = codec_pointer_encode (cbfunc))
        || !(xcbdata = codec_pointer_encode (cbdata))
        || interthread_send_pack (alloc->it,
                                  "alloc_upcall",
                                  "{s:s s:i s:i s:O s:O s:O}",
                                  "nspace", client->nspace,
                                  "rank", client->rank,
                                  "directive", directive,
                                  "data", xdata,
                                  "cbfunc", xcbfunc,
                                  "cbdata", xcbdata) < 0) {
        fprintf (stderr, "error sending alloc_upcall interthread message\n");
        rc = PMIX_ERROR;
    }
    json_decref (xdata);
    json_decref (xcbfunc);
    json_decref (xcbdata);

    shell_debug("shifting thread to handle allocation request");

    return rc;
}

void alloc_destroy (struct alloc *alloc)
{
    if (alloc) {
        int saved_errno = errno;
        free (alloc);
        errno = saved_errno;
        global_alloc_ctx = NULL;
    }
}

struct alloc *alloc_create (flux_shell_t *shell, struct interthread *it)
{
    struct alloc *alloc;


    if (!(alloc = calloc (1, sizeof (*alloc))))
        return NULL;
    alloc->shell = shell;
    alloc->it = it;
    alloc->trace_flag = 1; // stuck on for now
    if (interthread_register (it, "alloc_upcall", alloc_shell_cb, alloc) < 0)
        goto error;
    global_alloc_ctx = alloc;
    return alloc;
error:
    alloc_destroy (alloc);
    return NULL;
}