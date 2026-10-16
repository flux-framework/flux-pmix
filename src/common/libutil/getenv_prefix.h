/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _GETENV_PREFIX_H
#define _GETENV_PREFIX_H

/* Return the value of the first environment variable whose name begins
 * with 'prefix', or NULL if there is none.  The environment is unordered,
 * so if several match, which one is returned is unspecified.
 */
const char *getenv_prefix (const char *prefix);

#endif // !_GETENV_PREFIX_H

// vi:ts=4 sw=4 expandtab
