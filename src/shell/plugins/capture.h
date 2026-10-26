/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _SHELL_PMIX_CAPTURE_H
#define _SHELL_PMIX_CAPTURE_H

#include <flux/shell.h>

struct capture;

/* Arrange for 'task' to be captured: add the channel the shim writes
 * to, set LD_PRELOAD and the channel's name in the task environment,
 * and subscribe to what comes back.
 *
 * Returns 0 on success, -1 on failure with errno set.  A failure
 * here should not stop the task from running - a capture is
 * diagnostic - so the caller is expected to warn and continue.
 */
int capture_task_init (struct capture *cap, flux_shell_task_t *task);

void capture_destroy (struct capture *cap);

/* Returns NULL with errno set on failure.  Returns NULL with errno
 * zero if the pmix.wirecapture shell option is not set, which is not an
 * error: the caller then never calls capture_task_init().
 */
struct capture *capture_create (flux_shell_t *shell);

#endif /* !_SHELL_PMIX_CAPTURE_H */

// vi:ts=4 sw=4 expandtab
