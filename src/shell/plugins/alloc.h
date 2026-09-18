/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PX_ALLOC_H
#define _PX_ALLOC_H

#include <pmix.h>
#include <pmix_server.h>
#include "interthread.h"

/* Create context that allows alloc_server_cb() to work.
 * N.B. ensure pmix thread is not running when create/destroy are called.
 */
struct alloc *alloc_create (flux_shell_t *shell, struct interthread *it);
void alloc_destroy (struct alloc *alloc);

/* Server alloc callback registered with PMIx_server_init().
 */
int alloc_server_cb (const pmix_proc_t *client,
                    pmix_alloc_directive_t directive,
                    const pmix_info_t data[], size_t ndata,
                    pmix_info_cbfunc_t cbfunc, void *cbdata);

#endif // _PX_ALLOC_H

// vi:ts=4 sw=4 expandtab