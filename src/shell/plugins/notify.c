/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* notify.c - register and handle error notification callback
 *
 * Any notifications (from openpmix internals or from users) are
 * logged to the shell's stderr via shell_warn().  The spec has
 * much bigger plans for event notifications, but the goal here
 * is just to capture any useful errors from openpmix internals.
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

#include "notify.h"

struct notify {
    flux_shell_t *shell;
    struct interthread *it;
    int id;
};

/* This is for the benefit of server callbacks that don't have
 * a way to be passed a user-supplied opaque pointer.
 */
static struct notify *global_notify_ctx;

static void notify_shell_cb (const flux_msg_t *msg, void *arg)
{
    int id;
    int status;
    json_t *xsource;
    json_t *xinfo;
    json_t *xresults;
    json_t *xcbfunc;
    json_t *xcbdata;
    pmix_proc_t source;
    pmix_info_t *info = NULL;
    size_t ninfo = 0;
    pmix_info_t *results = NULL;
    size_t nresults = 0;
    pmix_event_notification_cbfunc_fn_t cbfunc;
    void *cbdata;
    int i;
    const char *message = NULL;

    if (flux_msg_unpack (msg,
                         "{s:o s:i s:o s:o s:o s:o s:o}",
                         "id", &id,
                         "status", &status,
                         "source", &xsource,
                         "info", &xinfo,
                         "results", &xresults,
                         "cbfunc", &xcbfunc,
                         "cbdata", &xcbdata) < 0
        || codec_proc_decode (xsource, &source) < 0
        || codec_info_array_decode (xinfo, &info, &ninfo) < 0
        || codec_info_array_decode (xresults, &results, &nresults) < 0
        || codec_pointer_decode (xcbfunc, (void **)&cbfunc) < 0
        || codec_pointer_decode (xcbdata, &cbdata) < 0) {
        shell_warn ("error unpacking interthread abort_upcall message");
        goto done;
    }
    for (i = 0; i < ninfo; i++) {
        if (!strcmp (info[i].key, PMIX_EVENT_TEXT_MESSAGE)) {
            if (info[i].value.type == PMIX_STRING)
                message = info[i].value.data.string;
        }
    }
    shell_warn ("notify source=%s.%d event-status=%d%s%s",
               source.nspace,
               source.rank,
               status,
               message ? " " : "",
               message ? message : "");
#if 0
    /* Calling the callback seems to cause a segfault in
     * the server progress_local_event_hdlr().  Perhaps we're doing it wrong.
     * In test, PMIx_Notify_event() is released anyway, contrary to v5 spec.
     * Revisit if that test starts hanging.
     */
    if (cbfunc)
        cbfunc (PMIX_EVENT_ACTION_COMPLETE, NULL, 0, NULL, cbdata, NULL);
#endif
done:
    codec_info_array_destroy (info, ninfo);
    codec_info_array_destroy (results, nresults);
}


static void notify_server_cb (size_t evhdlr_registration_id,
                             pmix_status_t status,
                             const pmix_proc_t *source,
                             pmix_info_t info[],
                             size_t ninfo,
                             pmix_info_t results[],
                             size_t nresults,
                             pmix_event_notification_cbfunc_fn_t cbfunc,
                             void *cbdata)
{
    struct notify *notify = global_notify_ctx;

    json_t *xsource = NULL;
    json_t *xinfo = NULL;
    json_t *xresults = NULL;
    json_t *xcbfunc = NULL;
    json_t *xcbdata = NULL;

    if (!(xsource = codec_proc_encode (source))
        || !(xinfo = codec_info_array_encode (info, ninfo))
        || !(xresults = codec_info_array_encode (results, nresults))
        || !(xcbfunc = codec_pointer_encode (cbfunc))
        || !(xcbdata = codec_pointer_encode (cbdata))
        || interthread_send_pack (notify->it,
                                  "notify_upcall",
                                  "{s:i s:i s:O s:O s:O s:O s:O}",
                                  "id", evhdlr_registration_id,
                                  "status", status,
                                  "source", xsource,
                                  "info", xinfo,
                                  "results", xresults,
                                  "cbfunc", xcbfunc,
                                  "cbdata", xcbdata) < 0) {
        fprintf (stderr, "error sending notify_upcall interthread message\n");
        return;
    }
    json_decref (xsource);
    json_decref (xinfo);
    json_decref (xresults);
    json_decref (xcbfunc);
    json_decref (xcbdata);
}

void notify_destroy (struct notify *notify)
{
    if (notify) {
        int saved_errno = errno;
        if (notify->id >= 0)
            PMIx_Deregister_event_handler (notify->id, NULL, NULL);
        free (notify);
        errno = saved_errno;
        global_notify_ctx = NULL;
    }
}

struct notify *notify_create (flux_shell_t *shell, struct interthread *it)
{
    struct notify *notify;

    if (!(notify = calloc (1, sizeof (*notify))))
        return NULL;
    notify->shell = shell;
    notify->it = it;
    if ((notify->id = PMIx_Register_event_handler (NULL,
                                                   0,
                                                   NULL,
                                                   0,
                                                   notify_server_cb,
                                                   NULL,
                                                   NULL)) < 0) {
        shell_warn ("PMIx_Register_event_handler: %s",
                    PMIx_Error_string (-1 * notify->id));
        goto error;
    }
    if (interthread_register (it, "notify_upcall", notify_shell_cb, notify) < 0)
        goto error;
    global_notify_ctx = notify;
    return notify;
error:
    notify_destroy (notify);
    return NULL;
}

// vi:ts=4 sw=4 expandtab
