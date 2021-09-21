/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PX_INTERTHREAD_H
#define _PX_INTERTHREAD_H

#include <flux/shell.h>

struct interthread *interthread_create (flux_shell_t *shell);
void interthread_destroy (struct interthread *it);

typedef void (*interthread_msg_handler_f)(const flux_msg_t *msg, void *arg);

int interthread_register (struct interthread *it,
                          const char *topic,
                          interthread_msg_handler_f cb,
                          void *arg);

int interthread_send_pack (struct interthread *it,
                           const char *name,
                           const char *fmt, ...);

#endif // _PX_INTERTHREAD_H

// vi:ts=4 sw=4 expandtab
