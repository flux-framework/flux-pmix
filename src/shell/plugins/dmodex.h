/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PX_DMODEX_H
#define _PX_DMODEX_H

#include <pmix.h>
#include <pmix_server.h>

/* Create context that allows dmodex_server_cb() to work.
 * N.B. ensure pmix thread is not running when create/destroy are called.
 */
struct dmodex *dmodex_create (flux_shell_t *shell, struct interthread *it);
void dmodex_destroy (struct dmodex *dx);

/* Server direct_modex callback registered with PMIx_server_init().
 */
int dmodex_server_cb (const pmix_proc_t *proc,
                      const pmix_info_t info[],
                      size_t ninfo,
                      pmix_modex_cbfunc_t cbfunc,
                      void *cbdata);



#endif // _PX_DMODEX_H

// vi:ts=4 sw=4 expandtab
