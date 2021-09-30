/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 \************************************************************/

/* getkey.c - call PMIx_Get() on specified key,
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <pmix.h>
#include <stdio.h>

#include <flux/optparse.h>

#include "log.h"

const char *opt_usage = "[OPTIONS] key";

static struct optparse_option opts[] = {
    { .name = "proc", .has_arg = 1, .arginfo = "RANK|*",
      .usage = "Get key from RANK (default=self)",
    },
    { .name = "rank", .has_arg = 1, .arginfo = "RANK",
      .usage = "Perform the getkey only on RANK (default=all)",
    },
    { .name = "label-io", .has_arg = 0,
      .usage = "Add rank labels",
    },
    OPTPARSE_TABLE_END,
};

static void getkey (pmix_proc_t *self,
                    pmix_proc_t *proc,
                    const char *key,
                    optparse_t *p)
{
    pmix_value_t *val;
    int rc;
    char prefix[16] = { 0 };

    if (optparse_hasopt (p, "label-io"))
        snprintf (prefix, sizeof (prefix), "%d: ", self->rank);

    if ((rc = PMIx_Get (proc, key, NULL, 0, &val)) != PMIX_SUCCESS)
        log_msg_exit ("PMIx_Get %s: %s", key, PMIx_Error_string (rc));

    switch (val->type) {
        case PMIX_BOOL:
            printf ("%s%s\n", prefix, val->data.flag ? "true" : "false");
            break;
        case PMIX_UINT32:
            printf ("%s%lu\n", prefix, (unsigned long)val->data.uint32);
            break;
        case PMIX_UINT16:
            printf ("%s%hu\n", prefix, (unsigned short)val->data.uint16);
            break;
        case PMIX_PROC_RANK:
            printf ("%s%lu\n", prefix, (unsigned long)val->data.rank);
            break;
        case PMIX_STRING:
            printf ("%s%s\n", prefix, val->data.string);
            break;
        default:
            log_msg_exit ("Error: I don't know this type yet: %d", val->type);
    }

    PMIX_VALUE_RELEASE (val);
}

int main (int argc, char **argv)
{
    optparse_t *p;
    int optindex;
    pmix_proc_t self;
    pmix_proc_t proc = { 0 };
    const char *key;
    int rc;
    int rank;

    /* Parse args
     */
    if (!(p = optparse_create ("getkey"))
        || optparse_add_option_table (p, opts) != OPTPARSE_SUCCESS
        || optparse_set (p, OPTPARSE_USAGE, opt_usage) != OPTPARSE_SUCCESS)
        log_msg_exit ("error setting up option parsing");
    if ((optindex = optparse_parse_args (p, argc, argv)) < 0)
        return 1;
    if (optindex != argc - 1) {
        optparse_print_usage (p);
        return 1;
    }
    key = argv[optindex++];

    /* Initialize and set log prefix
     */
    char name[512];
    if ((rc = PMIx_Init (&self, NULL, 0)) != PMIX_SUCCESS)
        log_msg_exit ("PMIx_Init: %s", PMIx_Error_string (rc));
    snprintf (name, sizeof (name), "%s.%d", self.nspace, self.rank);
    log_init (name);

    /* Parse --proc and --rank
     */
    snprintf (proc.nspace, sizeof (proc.nspace), "%s", self.nspace);
    proc.rank = self.rank;
    if (optparse_hasopt (p, "proc")) {
        const char *s = optparse_get_str (p, "proc", NULL);
        if (!strcmp (s, "*"))
            proc.rank = PMIX_RANK_WILDCARD;
        else
            proc.rank = strtoul (s, NULL, 10);
    }
    rank = optparse_get_int (p, "rank", -1);

    /* Get key
     */
    if (rank == -1 || rank == self.rank)
        getkey (&self, &proc, key, p);

    /* Finalize
     */
    if ((rc = PMIx_Finalize (NULL, 0)))
        log_msg_exit ("PMIx_Finalize: %s", PMIx_Error_string (rc));
    optparse_destroy (p);
    return 0;
}

// vi:ts=4 sw=4 expandtab
