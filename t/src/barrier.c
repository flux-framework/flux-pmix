/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 \************************************************************/

/* barrier.c - time a PMIx_Fence() with no data
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <pmix.h>
#include <stdio.h>
#include <flux/optparse.h>
#include <flux/idset.h>

#include "log.h"
#include "monotime.h"

const char *opt_usage = "[OPTIONS]";

static struct optparse_option opts[] = {
    { .name = "unknown", .has_arg = 1, .arginfo = "[+]INT",
      .usage = "set pmix.unknown attribute to the specified value",
    },
    { .name = "collect-data", .has_arg = 1, .arginfo = "[+]true|false",
      .usage = "set pmix.collect attribute to the specified value,"
               " optional unless prefixed with a '+'",
    },
    { .name = "collect-job-info", .has_arg = 1, .arginfo = "[+]true|false",
      .usage = "set pmix.collect.gen attribute to the specified value",
    },
    { .name = "timeout", .has_arg = 1, .arginfo = "[+]SECONDS",
      .usage = "set pmix.timeout attribute to the specified value",
    },
    { .name = "procs", .has_arg = 1, .arginfo = "IDSET",
      .usage = "Fence over set of proc ranks (default=all)",
    },
    OPTPARSE_TABLE_END,
};

void set_info_bool (pmix_info_t *info,
                    const char *name,
                    int flags,
                    const char *optarg)
{
    bool value = true;
    if (!strcmp (optarg, "false"))
        value = false;
    strncpy (info->key, name, PMIX_MAX_KEYLEN);
    info->flags = flags;
    info->value.type = PMIX_BOOL;
    info->value.data.flag = value;
}

void set_info_int (pmix_info_t *info,
                   const char *name,
                   int flags,
                   int optarg)
{
    strncpy (info->key, name, PMIX_MAX_KEYLEN);
    info->flags = flags;
    info->value.type = PMIX_INT;
    info->value.data.flag = optarg;
}

size_t parse_info_opts (optparse_t *p, pmix_info_t *infos, size_t ninfo)
{
    size_t count = 0;

    if (optparse_hasopt (p, "collect-data")) {
        const char *arg = optparse_get_str (p, "collect-data", "true");
        int flags = 0;
        if (*arg == '+') {
            flags |= PMIX_INFO_REQD;
            arg++;
        }
        if (count == ninfo)
            log_err_exit ("too many attribute options");
        set_info_bool (&infos[count++], PMIX_COLLECT_DATA, flags, arg);
    }
    if (optparse_hasopt (p, "collect-job-info")) {
        const char *arg = optparse_get_str (p, "collect-job-info", "true");
        int flags = 0;
        if (*arg == '+') {
            flags |= PMIX_INFO_REQD;
            arg++;
        }
        if (count == ninfo)
            log_err_exit ("too many attribute options");
        set_info_bool (&infos[count++],
                       PMIX_COLLECT_GENERATED_JOB_INFO,
                       flags,
                       arg);
    }
    if (optparse_hasopt (p, "timeout")) {
        const char *arg = optparse_get_str (p, "timeout", "0");
        int flags = 0;
        if (*arg == '+') {
            flags |= PMIX_INFO_REQD;
            arg++;
        }
        if (count == ninfo)
            log_err_exit ("too many attribute options");
        set_info_int (&infos[count++],
                      PMIX_TIMEOUT,
                      flags,
                      strtol (arg, NULL, 10));
    }
    if (optparse_hasopt (p, "unknown")) {
        const char *arg = optparse_get_str (p, "unknown", "0");
        int flags = 0;
        if (*arg == '+') {
            flags |= PMIX_INFO_REQD;
            arg++;
        }
        set_info_int (&infos[count++],
                      "pmix.unknown",
                      flags,
                      strtol (arg, NULL, 10));
    }
    return count;
}

size_t parse_procs_opt (optparse_t *p, pmix_proc_t *self, pmix_proc_t **procsp)
{
    size_t nprocs;
    const char *s;
    struct idset *ids;
    pmix_proc_t *procs;
    unsigned int id;

    if (!(s = optparse_get_str (p, "procs", NULL)))
        return 0;
    if (!(ids = idset_decode (s)))
        log_err_exit ("could not make an idset from '%s'", s);
    nprocs = idset_count (ids);
    if (!(procs = calloc (nprocs, sizeof (procs[0]))))
        log_err_exit ("allocating procs");
    int index = 0;
    id = idset_first (ids);
    while (id != IDSET_INVALID_ID) {
        snprintf (procs[index].nspace,
                  sizeof (procs[index].nspace),
                  "%s",
                  self->nspace);
        procs[index].rank = id;
        index++;
        id = idset_next (ids, id);
    }
    idset_destroy (ids);
    *procsp = procs;
    return nprocs;

}

int main (int argc, char **argv)
{
    optparse_t *p;
    int optindex;
    pmix_proc_t self;
    int rc;

    /* Parse args
     */
    if (!(p = optparse_create ("barrier"))
        || optparse_add_option_table (p, opts) != OPTPARSE_SUCCESS
        || optparse_set (p, OPTPARSE_USAGE, opt_usage) != OPTPARSE_SUCCESS)
        log_msg_exit ("error setting up option parsing");
    if ((optindex = optparse_parse_args (p, argc, argv)) < 0)
        return 1;
    if (optindex != argc) {
        optparse_print_usage (p);
        return 1;
    }

    /* Initialize and set log prefix to nspace.rank
     */
    char name[512];
    if ((rc = PMIx_Init (&self, NULL, 0)) != PMIX_SUCCESS)
        log_msg_exit ("PMIx_Init: %s", PMIx_Error_string (rc));
    snprintf (name, sizeof (name), "%s.%d", self.nspace, self.rank);
    log_init (name);
    if (self.rank == 0)
        log_msg ("completed PMIx_Init.");

    /* Get the size and print it so we know the test wired up.
     */
    pmix_proc_t proc;
    pmix_value_t *valp;
    strncpy (proc.nspace, self.nspace, PMIX_MAX_NSLEN);
    proc.nspace[PMIX_MAX_NSLEN] = '\0';
    proc.rank = PMIX_RANK_WILDCARD;
    if ((rc = PMIx_Get (&proc, PMIX_JOB_SIZE, NULL, 0, &valp)) != PMIX_SUCCESS)
        log_msg_exit ("PMIx_Get %s: %s", PMIX_JOB_SIZE, PMIx_Error_string (rc));
    if (self.rank == 0)
        log_msg ("there are %d tasks",
                 valp->type == PMIX_UINT32 ? valp->data.uint32 : -1);
    PMIX_VALUE_RELEASE (valp);

    /* Time the fence
     */
    struct timespec t;
    monotime (&t);
    pmix_info_t info[8] = { 0 };
    pmix_proc_t *procs = NULL;
    size_t ninfo = parse_info_opts (p, info, sizeof (info) / sizeof (info[0]));
    size_t nprocs = parse_procs_opt (p, &self, &procs);

    if ((rc = PMIx_Fence (procs, nprocs, info, ninfo)))
        log_msg_exit ("PMIx_Fence: %s", PMIx_Error_string (rc));
    if (self.rank == 0)
        log_msg ("completed barrier in %0.3fs.", monotime_since (t) / 1000);

    free (procs);

    /* Finalize
     */
    if ((rc = PMIx_Finalize (NULL, 0)))
        log_msg_exit ("PMIx_Finalize: %s", PMIx_Error_string (rc));
    if (self.rank == 0)
        log_msg ("completed PMIx_Finalize.");

    optparse_destroy (p);
    return 0;
}

// vi:ts=4 sw=4 expandtab
