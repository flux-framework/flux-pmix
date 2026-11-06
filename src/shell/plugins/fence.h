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

/* Create the context for fence_server_cb().
 * This creates a jobdict named "pmix" with exchange tree fanout k.
 * A k of 0 selects the jobdict default.  Call this at shell.init.
 * N.B. The pmix thread must not be running during create or destroy.
 */
struct fence *fence_create (flux_plugin_t *p, struct interthread *it, int k);
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
