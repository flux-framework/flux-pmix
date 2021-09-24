/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PX_ABORT_H
#define _PX_ABORT_H

#include <pmix.h>
#include <pmix_server.h>
#include "interthread.h"

/* Create context that allows abort_server_cb() to work.
 * N.B. ensure pmix thread is not running when create/destroy are called.
 */
struct abort *abort_create (flux_shell_t *shell, struct interthread *it);
void abort_destroy (struct abort *abort);

/* Server abort callback registered with PMIx_server_init().
 */
int abort_server_cb (const pmix_proc_t *proc,
                     void *server_object,
                     int status,
                     const char msg[],
                     pmix_proc_t procs[],
                     size_t nprocs,
                     pmix_op_cbfunc_t cbfunc,
                     void *cbdata);

#endif // _PX_ABORT_H

// vi:ts=4 sw=4 expandtab
