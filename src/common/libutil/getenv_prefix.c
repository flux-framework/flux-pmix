/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* getenv_prefix.c - look up an environment variable by name prefix
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>

#include "getenv_prefix.h"

extern char **environ;

const char *getenv_prefix (const char *prefix)
{
    size_t len;

    if (prefix == NULL)
        return NULL;
    len = strlen (prefix);
    for (int i = 0; environ[i] != NULL; i++) {
        if (strncmp (environ[i], prefix, len) == 0) {
            const char *val = strchr (environ[i], '=');
            /* An entry with no '=' is not a variable assignment.
             */
            if (val)
                return val + 1;
        }
    }
    return NULL;
}

// vi:ts=4 sw=4 expandtab
