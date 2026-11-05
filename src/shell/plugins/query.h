/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PX_QUERY_H
#define _PX_QUERY_H

#include <pmix.h>
#include <pmix_server.h>

struct query *query_create (flux_shell_t *shell, struct interthread *it);
void query_destroy (struct query *q);

int query_server_cb (pmix_proc_t *proc,
                     pmix_query_t *queries,
                     size_t nqueries,
                     pmix_info_cbfunc_t cbfunc,
                     void *cbdata);

#endif // _PX_QUERY_H

// vi:ts=4 sw=4 expandtab
