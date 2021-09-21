/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PX_FENCE_H
#define _PX_FENCE_H

#include <pmix.h>
#include <pmix_server.h>
#include "interthread.h"

/* Create context that allows fence_server_cb() to work.
 * N.B. ensure pmix thread is not running when create/destroy are called.
 */
struct fence *fence_create (flux_shell_t *shell, struct interthread *it);
void fence_destroy (struct fence *dx);

/* Server fence_nb callback registered with PMIx_server_init().
 */
int fence_server_cb (const pmix_proc_t proc[],
                     size_t nprocs,
                     const pmix_info_t info[],
                     size_t ninfo,
                     char *data,
                     size_t ndata,
                     pmix_modex_cbfunc_t cbfunc,
                     void *cbdata);

#endif // _PX_FENCE_H

// vi:ts=4 sw=4 expandtab
