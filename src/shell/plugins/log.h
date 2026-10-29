/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PX_LOG_H
#define _PX_LOG_H

#include <flux/shell.h>
#include <pmix_server.h>

#include "interthread.h"

struct log *log_create (flux_shell_t *shell, struct interthread *it);
void log_destroy (struct log *log);

/* Log one line per help message that was dropped as a repeat.
 */
void log_summary (struct log *log);

void log_server_cb (const pmix_proc_t *client,
                    const pmix_info_t data[],
                    size_t ndata,
                    const pmix_info_t directives[],
                    size_t ndirs,
                    pmix_op_cbfunc_t cbfunc,
                    void *cbdata);

#endif // _PX_LOG_H

// vi:ts=4 sw=4 expandtab
