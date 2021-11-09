/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 \************************************************************/

/* notify.c - call PMIx_Notify_event()
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <pmix.h>
#include <stdio.h>
#include <flux/optparse.h>

#include "log.h"

const char *opt_usage = "[OPTIONS]";

static struct optparse_option opts[] = {
    { .name = "status", .has_arg = 1, .arginfo = "INT",
      .usage = "set event status code (default=0)",
    },
    { .name = "message", .has_arg = 1, .arginfo = "TEXT",
      .usage = "set event message (default=none)",
    },
    OPTPARSE_TABLE_END,
};

int main (int argc, char **argv)
{
    optparse_t *p;
    int optindex;
    pmix_proc_t self;
    int event_status;
    const char *event_message;
    pmix_info_t info[1];
    size_t ninfo = 0;
    int rc;

    /* Parse args
     */
    if (!(p = optparse_create ("abort"))
        || optparse_add_option_table (p, opts) != OPTPARSE_SUCCESS
        || optparse_set (p, OPTPARSE_USAGE, opt_usage) != OPTPARSE_SUCCESS)
        log_msg_exit ("error setting up option parsing");
    if ((optindex = optparse_parse_args (p, argc, argv)) < 0)
        return 1;
    if (optindex != argc) {
        optparse_print_usage (p);
        return 1;
    }
    event_status = optparse_get_int (p, "status", 0);
    event_message = optparse_get_str (p, "message", NULL);

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

    /* Add any info options
     */
    if (event_message) {
        strncpy (info[ninfo].key, PMIX_EVENT_TEXT_MESSAGE, PMIX_MAX_KEYLEN);
        info[ninfo].value.type = PMIX_STRING;
        info[ninfo].value.data.string = (char *)event_message;
        ninfo++;
    }
    if ((rc = PMIx_Notify_event (event_status,
                                 &self,
                                 PMIX_RANGE_GLOBAL,
                                 info,
                                 ninfo,
                                 NULL,
                                 NULL)) != PMIX_SUCCESS)
        log_msg_exit ("PMIx_Notify_event: %s", PMIx_Error_string (rc));

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
