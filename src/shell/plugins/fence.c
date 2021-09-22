/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* fence.c - handle fence_nb callback from openpmix server
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
#include "exchange.h"

#include "fence.h"

struct fence {
    flux_shell_t *shell;
    struct interthread *it;
    struct exchange *exchange;
    int trace_flag;
};

struct fence_call {
    pmix_proc_t *procs;
    size_t nprocs;
    pmix_info_t *info;
    size_t ninfo;
    pmix_modex_cbfunc_t cbfunc;
    void *cbdata;
};

/* This is for the benefit of server callbacks that don't have
 * a way to be passed a user-supplied opaque pointer.
 */
static struct fence *global_fence_ctx;

static void fence_call_destroy (struct fence_call *fxcall)
{
    if (fxcall) {
        int saved_errno = errno;
        free (fxcall->procs);
        codec_info_array_destroy (fxcall->info, fxcall->ninfo);
        free (fxcall);
        errno = saved_errno;
    }
}

static struct fence_call *fence_call_create (void)
{
    struct fence_call *fxcall;

    if (!(fxcall = calloc (1, sizeof (*fxcall))))
        return NULL;
    return fxcall;
}

static void exchange_exit_cb (struct exchange *xcg, void *arg)
{
    struct fence_call *fxcall = arg;
    void *data = NULL;
    size_t ndata = 0;
    int status = PMIX_ERROR;

    if (exchange_has_error (xcg)) {
        shell_warn ("pmix exchange failed");
        goto done;
    }
    if (exchange_get_data (xcg, &data, &ndata) < 0) {
        shell_warn ("error accessing pmix exchanged data");
        goto done;
    }
    shell_trace ("completed pmix exchange");
    status = PMIX_SUCCESS;
done:
    // N.B. pmix calls 'free' on data when fence is complete
    fxcall->cbfunc (status, data, ndata, fxcall->cbdata, free, data);
}

static void fence_shell_cb (const flux_msg_t *msg, void *arg)
{
    struct fence *fx = arg;
    json_t *xprocs;
    json_t *xinfo;
    json_t *xdata;
    json_t *xcbfunc;
    json_t *xcbdata;
    struct fence_call *fxcall;

    if (!(fxcall = fence_call_create ())
        || flux_msg_unpack (msg,
                            "{s:o s:o s:o s:o s:o}",
                            "procs", &xprocs,
                            "info", &xinfo,
                            "data", &xdata,
                            "cbfunc", &xcbfunc,
                            "cbdata", &xcbdata) < 0
        || codec_proc_array_decode (xprocs, &fxcall->procs, &fxcall->nprocs) < 0
        || codec_info_array_decode (xinfo, &fxcall->info, &fxcall->ninfo) < 0
        || codec_pointer_decode (xcbfunc, (void **)&fxcall->cbfunc) < 0
        || codec_pointer_decode (xcbdata, &fxcall->cbdata) < 0
        || fxcall->cbfunc == NULL) {
        shell_warn ("error unpacking interthread fence_upcall message");
        fence_call_destroy (fxcall);
        return;
    }
    /* N.B. there is no need to decode 'xdata' as we would need to
     * re-encode it for the exchange anyway.
     */
    if (exchange_enter_base64_string (fx->exchange,
                                      xdata,
                                      exchange_exit_cb,
                                      fxcall) < 0) {
        shell_warn ("error initiating pmix exchange");
        fxcall->cbfunc (PMIX_ERROR, NULL, 0, fxcall->cbdata, NULL, NULL);
        fence_call_destroy (fxcall);
    }
    if (fx->trace_flag)
        shell_trace ("starting pmix exchange");
}

int fence_server_cb (const pmix_proc_t procs[],
                     size_t nprocs,
                     const pmix_info_t info[],
                     size_t ninfo,
                     char *data,
                     size_t ndata,
                     pmix_modex_cbfunc_t cbfunc,
                     void *cbdata)
{
    struct fence *fx = global_fence_ctx;
    json_t *xprocs = NULL;
    json_t *xinfo = NULL;
    json_t *xdata = NULL;
    json_t *xcbfunc = NULL;
    json_t *xcbdata = NULL;
    int rc = PMIX_SUCCESS;

    if (!(xprocs = codec_proc_array_encode (procs, nprocs))
        || !(xinfo = codec_info_array_encode (info, ninfo))
        || !(xdata = codec_data_encode (data, ndata))
        || !(xcbfunc = codec_pointer_encode (cbfunc))
        || !(xcbdata = codec_pointer_encode (cbdata))
        || interthread_send_pack (fx->it,
                                  "fence_upcall",
                                  "{s:O s:O s:O s:O s:O}",
                                  "procs", xprocs,
                                  "info", xinfo,
                                  "data", xdata,
                                  "cbfunc", xcbfunc,
                                  "cbdata", xcbdata) < 0) {
        fprintf (stderr, "error sending fence_upcall interthread message\n");
        rc = PMIX_ERROR;
    }
    json_decref (xprocs);
    json_decref (xinfo);
    json_decref (xdata);
    json_decref (xcbfunc);
    json_decref (xcbdata);
    return rc;
}

void fence_destroy (struct fence *fx)
{
    if (fx) {
        int saved_errno = errno;
        exchange_destroy (fx->exchange);
        free (fx);
        errno = saved_errno;
        global_fence_ctx = NULL;
    }
}

struct fence *fence_create (flux_shell_t *shell, struct interthread *it)
{
    flux_t *h = flux_shell_get_flux (shell);
    struct fence *fx;

    if (!(fx = calloc (1, sizeof (*fx))))
        return NULL;
    fx->shell = shell;
    fx->it = it;
    fx->trace_flag = 1; // stuck on for now
    if (interthread_register (it, "fence_upcall", fence_shell_cb, fx) < 0)
        goto error;
    if (!(fx->exchange = exchange_create (shell, 0)))
        goto error;
    global_fence_ctx = fx;
    return fx;
error:
    fence_destroy (fx);
    return NULL;
}

// vi:ts=4 sw=4 expandtab
