/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PX_MAPS_H
#define _PX_MAPS_H

#include <flux/shell.h>

char *maps_node_create (flux_shell_t *shell);

/* Return this shell's node name as it appears in the node map, which
 * is the hostname, made unique by appending the shell rank when the
 * same host appears more than once.  The caller must free the result.
 * Returns NULL with errno set on failure.
 */
char *maps_node_name (flux_shell_t *shell);

#endif // _PX_MAPS_H

// vi:ts=4 sw=4 expandtab
