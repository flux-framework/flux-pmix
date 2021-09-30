/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* dmodex.c - handle direct_modex callback from openpmix server
 *
 * This is just the interthread plumbing at this point.
 * We'll see if the lack of direct_modex implementation seems
 * to be causing any real problems before filling in the rest.
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

#include "dmodex.h"

struct dmodex {
    flux_shell_t *shell;
    struct interthread *it;
};

struct dmodex_call {
    pmix_proc_t proc;
    pmix_info_t *info;
    size_t ninfo;
    int shell_rank;
    pmix_modex_cbfunc_t cbfunc;
    void *cbdata;
};

/* This is for the benefit of server callbacks that don't have
 * a way to be passed a user-supplied opaque pointer.
 */
static struct dmodex *global_dmodex_ctx;

static void dmodex_call_destroy (struct dmodex_call *dxcall)
{
    if (dxcall) {
        int saved_errno = errno;
        codec_info_array_destroy (dxcall->info, dxcall->ninfo);
        free (dxcall);
        errno = saved_errno;
    };
}

static struct dmodex_call *dmodex_call_create (void)
{
    struct dmodex_call *dxcall;

    if (!(dxcall = calloc (1, sizeof (*dxcall))))
        return NULL;
    return dxcall;
}

/* Find the shell rank that hosts proc 'rank', or return -1 if not found.
 */
static int lookup_shell_rank (flux_shell_t *shell, int rank)
{
    int shell_size;
    int shell_rank;
    int base_rank = 0;

    if (flux_shell_info_unpack (shell, "{s:i}", "size", &shell_size) < 0)
        goto error;
    for (shell_rank = 0; shell_rank < shell_size; shell_rank++) {
        int ntasks;
        if (flux_shell_rank_info_unpack (shell,
                                         shell_rank,
                                         "{s:i}",
                                         "ntasks", &ntasks) < 0)
            goto error;
        if (rank >= base_rank && rank < base_rank + ntasks)
            return shell_rank;
        base_rank += ntasks;
    }
error:
    errno = ENOENT;
    return -1;
}

static void dmodex_shell_cb (const flux_msg_t *msg, void *arg)
{
    struct dmodex *dx = arg;
    json_t *xproc;
    json_t *xinfo;
    json_t *xcbfunc;
    json_t *xcbdata;
    struct dmodex_call *dxcall;
    int rc;

    if (!(dxcall = dmodex_call_create ())
        || flux_msg_unpack (msg,
                            "{s:o s:o s:o s:o}",
                            "proc", &xproc,
                            "info", &xinfo,
                            "cbfunc", &xcbfunc,
                            "cbdata", &xcbdata) < 0
        || codec_proc_decode (xproc, &dxcall->proc) < 0
        || codec_info_array_decode (xinfo, &dxcall->info, &dxcall->ninfo) < 0
        || codec_pointer_decode (xcbfunc, (void **)&dxcall->cbfunc) < 0
        || codec_pointer_decode (xcbdata, &dxcall->cbdata) < 0) {
        shell_warn ("error unpacking dmodex_upcall interthread message");
        dmodex_call_destroy (dxcall);
        return;
    }
    if ((dxcall->shell_rank = lookup_shell_rank (dx->shell,
                                                 dxcall->proc.rank)) < 0) {
        rc = PMIX_ERR_PROC_ENTRY_NOT_FOUND;
        goto error;
    }
    rc = PMIX_ERR_NOT_IMPLEMENTED;
error:
    shell_warn ("dmodex_upcall for %s.%d on shell rank %d: %s",
                dxcall->proc.nspace,
                dxcall->proc.rank,
                dxcall->shell_rank,
                PMIx_Error_string (rc));
    if (dxcall->cbfunc)
        dxcall->cbfunc (rc, NULL, 0, dxcall->cbdata, NULL, NULL);
    dmodex_call_destroy (dxcall);
}

int dmodex_server_cb (const pmix_proc_t *proc,
                      const pmix_info_t info[],
                      size_t ninfo,
                      pmix_modex_cbfunc_t cbfunc,
                      void *cbdata)
{
    struct dmodex *dx = global_dmodex_ctx;
    json_t *xproc  = NULL;
    json_t *xinfo = NULL;
    json_t *xcbfunc = NULL;
    json_t *xcbdata = NULL;
    int rc = PMIX_SUCCESS;

    if (!(xproc = codec_proc_encode (proc))
        || !(xinfo = codec_info_array_encode (info, ninfo))
        || !(xcbfunc = codec_pointer_encode (cbfunc))
        || !(xcbdata = codec_pointer_encode (cbdata))
        || interthread_send_pack (dx->it,
                                  "dmodex_upcall",
                                  "{s:O s:O s:O s:O}",
                                  "proc", xproc,
                                  "info", xinfo,
                                  "cbfunc", xcbfunc,
                                  "cbdata", xcbdata) < 0) {
        fprintf (stderr, "error sending dmodex_upcall interthread message\n");
        rc = PMIX_ERROR;
    }
    json_decref (xproc);
    json_decref (xinfo);
    json_decref (xcbfunc);
    json_decref (xcbdata);
    return rc;
}

void dmodex_destroy (struct dmodex *dx)
{
    if (dx) {
        int saved_errno = errno;
        free (dx);
        errno = saved_errno;
        global_dmodex_ctx = NULL;
    }
}

struct dmodex *dmodex_create (flux_shell_t *shell, struct interthread *it)
{
    flux_t *h = flux_shell_get_flux (shell);
    struct dmodex *dx;

    if (!(dx = calloc (1, sizeof (*dx))))
        return NULL;
    dx->shell = shell;
    dx->it = it;
    if (interthread_register (it,
                              "dmodex_upcall",
                              dmodex_shell_cb,
                              dx) < 0)
        goto error;
    global_dmodex_ctx = dx;
    return dx;
error:
    dmodex_destroy (dx);
    return NULL;
}

// vi:ts=4 sw=4 expandtab
