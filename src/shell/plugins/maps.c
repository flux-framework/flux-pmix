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
#include <jansson.h>
#include <argz.h>
#include <flux/shell.h>
#include <flux/idset.h>
#include <flux/hostlist.h>

#include "maps.h"

static char *derange_idset (const char *in)
{
    struct idset *ids;
    char *out;

    if (!(ids = idset_decode (in)))
        return NULL;
    out = idset_encode (ids, 0);
    idset_destroy (ids);
    return out;
}

char *maps_lpeers_create (flux_shell_t *shell)
{
    int shell_rank;
    const char *taskids;

    if (flux_shell_info_unpack (shell, "{s:i}", "rank", &shell_rank) < 0
        || flux_shell_rank_info_unpack (shell,
                                        shell_rank,
                                        "{s:s}",
                                        "taskids", &taskids) < 0)
        return NULL;
    return derange_idset (taskids);
}

/* Iterate over each shell, creating an idset of ranks.
 * Create a semicolon delimited list of idsets, where each entry
 * represents a set of ranks on a shell.
 * E.g. "0,1;2,3" means ranks 0,1 on shell 0; ranks 2,3 on shell 1.
 */
char *maps_proc_create (flux_shell_t *shell)
{
    int shell_size;
    char *argz = NULL;
    size_t argz_len = 0;
    const char *taskids;
    char *s = NULL;

    if (flux_shell_info_unpack (shell, "{s:i}", "size", &shell_size) < 0)
        return NULL;
    for (int shell_rank = 0; shell_rank < shell_size; shell_rank++) {
        if (flux_shell_rank_info_unpack (shell,
                                         shell_rank,
                                         "{s:s}",
                                         "taskids", &taskids) < 0
            || !(s = derange_idset (taskids))
            || argz_add (&argz, &argz_len, s) != 0) {
            free (s);
            free (argz);
            return NULL;
        }
        free (s);
    }
    argz_stringify (argz, argz_len, ';');
    return argz;
}

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

/* First fetch the nodelist from R and convert it to a hostlist.
 * Then encode the hostlist to a string with no range compression
 * (we have to do that manually with an argz).
 */
char *maps_node_create (flux_shell_t *shell)
{
    json_t *nodelist;
    size_t index;
    json_t *value;
    struct hostlist *hl = NULL;
    char *argz = NULL;
    size_t argz_len = 0;
    const char *node;
    char *s;
    bool uniqify = false;

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
        if (!s || hostlist_append (hl, s) < 0)
            goto error;
    }
    if (contains_duplicates (hl))
        uniqify = true;
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
