/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* abort.c - handle abort callback from openpmix server
 *
 * User calls PMIx_Abort (status, msg, NULL, 0)
 *
 * This is translated into shell_die() using 'status' as the exit code.
 * shell_die() raises a fatal exception on the job before calling exit(3).
 * (Probably the czmq atexit handler for the interthread sockets will complain.)
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

#include "abort.h"

struct abort {
    flux_shell_t *shell;
    struct interthread *it;
    int trace_flag;
};

/* This is for the benefit of server callbacks that don't have
 * a way to be passed a user-supplied opaque pointer.
 */
static struct abort *global_abort_ctx;

static void abort_shell_cb (const flux_msg_t *msg, void *arg)
{
    json_t *xproc;
    json_t *xserver_object;
    json_t *xprocs;
    json_t *xcbfunc;
    json_t *xcbdata;
    pmix_proc_t proc;
    void *server_object;
    int status;
    pmix_proc_t *procs;
    size_t nprocs;
    const char *message;
    pmix_op_cbfunc_t cbfunc;
    void *cbdata;

    if (flux_msg_unpack (msg,
                         "{s:o s:o s:i s:s s:o s:o s:o}",
                         "proc", &xproc,
                         "server_object", &xserver_object,
                         "status", &status,
                         "msg", &message,
                         "procs", &xprocs,
                         "cbfunc", &xcbfunc,
                         "cbdata", &xcbdata) < 0
        || codec_proc_decode (xproc, &proc) < 0
        || codec_pointer_decode (xserver_object, &server_object) < 0
        || codec_proc_array_decode (xprocs, &procs, &nprocs) < 0
        || codec_pointer_decode (xcbfunc, (void **)&cbfunc) < 0
        || codec_pointer_decode (xcbdata, &cbdata) < 0) {
        shell_warn ("error unpacking interthread abort_upcall message");
        return;
    }
    if (cbfunc)
        cbfunc (PMIX_SUCCESS, cbdata);
    shell_die (status,
               "%s.%d called PMIx_Abort: %s",
               proc.nspace,
               proc.rank,
               message);
    free (procs);
}

int abort_server_cb (const pmix_proc_t *proc,
                     void *server_object,
                     int status,
                     const char msg[],
                     pmix_proc_t procs[],
                     size_t nprocs,
                     pmix_op_cbfunc_t cbfunc,
                     void *cbdata)
{
    struct abort *abort = global_abort_ctx;
    json_t *xproc = NULL;
    json_t *xserver_object = NULL;
    json_t *xprocs = NULL;
    json_t *xcbfunc = NULL;
    json_t *xcbdata = PMIX_SUCCESS;
    int rc = PMIX_SUCCESS;

    if (!(xproc = codec_proc_encode (proc))
        || !(xserver_object = codec_pointer_encode (server_object))
        || !(xprocs = codec_proc_array_encode (procs, nprocs))
        || !(xcbfunc = codec_pointer_encode (cbfunc))
        || !(xcbdata = codec_pointer_encode (cbdata))
        || interthread_send_pack (abort->it,
                                  "abort_upcall",
                                  "{s:O s:O s:i s:s s:O s:O s:O}",
                                  "proc", xproc,
                                  "server_object", xserver_object,
                                  "status", status,
                                  "msg", msg ? msg : "(no message)",
                                  "procs", xprocs,
                                  "cbfunc", xcbfunc,
                                  "cbdata", xcbdata) < 0) {
        fprintf (stderr, "error sending abort_upcall interthread message\n");
        rc = PMIX_ERROR;
    }
    json_decref (xproc);
    json_decref (xserver_object);
    json_decref (xprocs);
    json_decref (xcbfunc);
    json_decref (xcbdata);

    return rc;
}

void abort_destroy (struct abort *abort)
{
    if (abort) {
        int saved_errno = errno;
        free (abort);
        errno = saved_errno;
        global_abort_ctx = NULL;
    }
}

struct abort *abort_create (flux_shell_t *shell, struct interthread *it)
{
    struct abort *abort;

    if (!(abort = calloc (1, sizeof (*abort))))
        return NULL;
    abort->shell = shell;
    abort->it = it;
    abort->trace_flag = 1; // stuck on for now
    if (interthread_register (it, "abort_upcall", abort_shell_cb, abort) < 0)
        goto error;
    global_abort_ctx = abort;
    return abort;
error:
    abort_destroy (abort);
    return NULL;
}

// vi:ts=4 sw=4 expandtab
