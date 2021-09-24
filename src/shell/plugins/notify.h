/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PX_NOTIFY_H
#define _PX_NOTIFY_H

#include <pmix.h>
#include <pmix_server.h>
#include "interthread.h"

/* N.B. notify_create() must be called after PMIx_server_init().
 */
struct notify *notify_create (flux_shell_t *shell, struct interthread *it);
void notify_destroy (struct notify *notify);

#endif // _PX_NOTIFY_H

// vi:ts=4 sw=4 expandtab
