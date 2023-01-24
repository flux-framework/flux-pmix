/************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* unsetenv_glob.c - unset environment variables matching a glob pattern
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <argz.h>
#include <envz.h>
#include <fnmatch.h>
#include <errno.h>

extern char **environ;

static char *split (const char *s, int delim)
{
    char *key, *val;

    if (!(key = strdup (s)))
        return NULL;
    if ((val = strchr (key, delim)))
        *val++ = '\0';
    return key;
}

static int unsetenv_glob_entry (const char *entry, const char *pattern)
{
    char *name;
    int rc;
    int count = 0;
    int saved_errno;

    if (!(name = split (entry, '=')))
        goto error;
    rc = fnmatch (pattern, name, 0);
    if (rc != 0 && rc != FNM_NOMATCH) {
        errno = EINVAL;
        goto error;
    }
    if (rc == 0) {
        if (unsetenv (name) < 0)
            goto error;
        count++;
    }
    free (name);
    return count;
error:
    saved_errno = errno;
    free (name);
    errno = saved_errno;
    return -1;
}

int unsetenv_glob (const char *pattern)
{
    char *envz = NULL;
    size_t envz_len = 0;
    const char *entry;
    int count = 0;
    int rc;

    if (pattern == NULL) {
        errno = EINVAL;
        return -1;
    }
    if (argz_create (environ, &envz, &envz_len) != 0) {
        errno = ENOMEM;
        return -1;
    }
    entry = argz_next (envz, envz_len, NULL);
    while (entry) {
        rc = unsetenv_glob_entry (entry, pattern);
        if (rc < 0) {
            int saved_errno = errno;
            free (envz);
            errno = saved_errno;
            return -1;
        }
        count += rc;
        entry = argz_next (envz, envz_len, entry);
    }
    free (envz);
    return count;
}

// vi:ts=4 sw=4 expandtab
