/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _SHELL_PMIX_TOPO_H
#define _SHELL_PMIX_TOPO_H

#include <flux/shell.h>
#include <pmix.h>

struct topo;

/* Load the shell's cached hwloc XML into a topology object for the
 * server to hand out.  If 'do_restrict' is true, the topology is cut
 * down to the cores R allocates to this shell, so that clients see
 * the job's slice of the node rather than the whole node.
 *
 * Returns NULL with errno set on failure.
 */
struct topo *topo_create (flux_shell_t *shell, bool do_restrict);

/* The server borrows the topology, so destroy only after
 * PMIx_server_finalize().
 */
void topo_destroy (struct topo *topo);

/* Append the PMIx_server_init() info entries that hand the topology to
 * the server and ask it to share with clients.  'info' has room for
 * 'avail' entries.  Returns the number added, or -1 with errno set if
 * there is not enough room.
 */
int topo_server_info (struct topo *topo, pmix_info_t *info, size_t avail);

#endif /* !_SHELL_PMIX_TOPO_H */

// vi:ts=4 sw=4 expandtab
