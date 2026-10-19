/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* maps.c - construct inputs for PMIx_generate_ppn() and PMIx_generate_regex()
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <argz.h>
#include <flux/shell.h>
#include <flux/idset.h>
#include <flux/hostlist.h>

#include "maps.h"

static bool contains_duplicates (struct hostlist *hl)
{
    struct hostlist *hl2;
    bool result = false;

    if (!(hl2 = hostlist_copy (hl)))
        return false;
    hostlist_uniq (hl2);
    if (hostlist_count (hl) > hostlist_count (hl2))
        result = true;
    hostlist_destroy (hl2);
    return result;
}

/* Fetch the nodelist from R as a hostlist, and say whether it holds
 * any host more than once, which is when the names have to be made
 * unique.  The caller owns the hostlist.
 */
static struct hostlist *nodelist_get (flux_shell_t *shell, bool *uniqify)
{
    json_t *nodelist;
    size_t index;
    json_t *value;
    struct hostlist *hl;

    if (flux_shell_info_unpack (shell,
                                "{s:{s:{s:o}}}",
                                "R",
                                  "execution",
                                    "nodelist", &nodelist) < 0)
        return NULL;
    if (!(hl = hostlist_create ()))
        return NULL;
    json_array_foreach (nodelist, index, value) {
        const char *s = json_string_value (value);
        if (!s || hostlist_append (hl, s) < 0) {
            hostlist_destroy (hl);
            return NULL;
        }
    }
    *uniqify = contains_duplicates (hl);
    return hl;
}

char *maps_node_name (flux_shell_t *shell)
{
    struct hostlist *hl;
    bool uniqify;
    int rank;
    const char *node;
    char *result = NULL;

    if (flux_shell_info_unpack (shell, "{s:i}", "rank", &rank) < 0)
        return NULL;
    if (!(hl = nodelist_get (shell, &uniqify)))
        return NULL;
    if (!(node = hostlist_nth (hl, rank))) {
        errno = ENOENT;
        goto done;
    }
    if (uniqify) {
        if (asprintf (&result, "%s%d", node, rank) < 0)
            result = NULL;
    }
    else
        result = strdup (node);
done:
    hostlist_destroy (hl);
    return result;
}

/* First fetch the nodelist from R and convert it to a hostlist.
 * Then encode the hostlist to a string with no range compression
 * (we have to do that manually with an argz).
 */
char *maps_node_create (flux_shell_t *shell)
{
    size_t index;
    struct hostlist *hl;
    char *argz = NULL;
    size_t argz_len = 0;
    const char *node;
    bool uniqify;

    if (!(hl = nodelist_get (shell, &uniqify)))
        return NULL;
    index = 0;
    node = hostlist_first (hl);
    while (node) {
        char newnode[HOST_NAME_MAX + 1];
        if (uniqify) {
            snprintf (newnode, sizeof (newnode), "%s%zu", node, index++);
            node = newnode;
        }
        if (argz_add (&argz, &argz_len, node) != 0)
            goto error;
        node = hostlist_next (hl);
    }
    hostlist_destroy (hl);
    argz_stringify (argz, argz_len, ',');
    return argz;
error:
    free (argz);
    hostlist_destroy (hl);
    return NULL;
}

// vi:ts=4 sw=4 expandtab
