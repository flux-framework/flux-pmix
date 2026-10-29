/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 \************************************************************/

/* pmixlog.c - call PMIx_Log()
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <pmix.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <flux/optparse.h>

#include "src/common/libutil/strlcpy.h"

#include "log.h"

const char *opt_usage = "[OPTIONS] MESSAGE";

static struct optparse_option opts[] = {
    { .name = "stdout", .has_arg = 0,
      .usage = "log to stdout instead of stderr",
    },
    { .name = "channel", .has_arg = 1, .arginfo = "KEY",
      .usage = "log to the named channel instead of stderr (repeatable)",
    },
    { .name = "priority", .has_arg = 1, .arginfo = "INT",
      .usage = "set PMIX_LOG_SYSLOG_PRI",
    },
    { .name = "once", .has_arg = 0,
      .usage = "set PMIX_LOG_ONCE",
    },
    { .name = "timestamp", .has_arg = 0,
      .usage = "set PMIX_LOG_GENERATE_TIMESTAMP (optional directive)",
    },
    { .name = "no-agg", .has_arg = 0,
      .usage = "set PMIX_LOG_AGG false, so openpmix does not drop repeats",
    },
    { .name = "key", .has_arg = 1, .arginfo = "FILE",
      .usage = "set PMIX_LOG_KEY (with --val, allows de-duplication)",
    },
    { .name = "val", .has_arg = 1, .arginfo = "TOPIC",
      .usage = "set PMIX_LOG_VAL (with --key, allows de-duplication)",
    },
    { .name = "rank", .has_arg = 1, .arginfo = "INT",
      .usage = "only call PMIx_Log on this rank (default=all ranks)",
    },
    { .name = "count", .has_arg = 1, .arginfo = "INT",
      .usage = "call PMIx_Log this many times (default=1)",
    },
    { .name = "expect", .has_arg = 1, .arginfo = "NAME",
      .usage = "expect this PMIx_Log status (default=PMIX_SUCCESS)",
    },
    OPTPARSE_TABLE_END,
};

/* openpmix 3.2 has no log tags.  The strings are those of later
 * versions, so a server that knows them is exercised.
 */
#ifndef PMIX_LOG_KEY
#define PMIX_LOG_KEY "pmix.log.key"
#endif
#ifndef PMIX_LOG_VAL
#define PMIX_LOG_VAL "pmix.log.val"
#endif

/* openpmix 4.2 and later drop repeats of a tag before the server log
 * callback is reached, which hides the shell's own de-duplication from
 * the testsuite.  PMIX_LOG_AGG false turns that off.  openpmix 3.2 has
 * neither the attribute nor the behavior, so the string is defined here
 * and that version simply ignores the directive.
 */
#ifndef PMIX_LOG_AGG
#define PMIX_LOG_AGG "pmix.log.agg"
#endif

/* The spelling of PMIx_Error_string() output varies by openpmix
 * version, so the expected status is named here.  Only the statuses
 * the testsuite asks for are listed, and each must be defined by
 * every openpmix version the plugin builds against.
 */
static const struct {
    const char *name;
    pmix_status_t status;
} status_names[] = {
    { "PMIX_SUCCESS", PMIX_SUCCESS },
    { "PMIX_ERR_NOT_SUPPORTED", PMIX_ERR_NOT_SUPPORTED },
    { "PMIX_ERR_BAD_PARAM", PMIX_ERR_BAD_PARAM },
};

static pmix_status_t status_lookup (const char *name)
{
    for (int i = 0; i < sizeof (status_names) / sizeof (status_names[0]); i++)
        if (!strcmp (status_names[i].name, name))
            return status_names[i].status;
    log_msg_exit ("unknown status name %s", name);
}

static void set_str (pmix_info_t *info, const char *key, const char *s)
{
    memset (info, 0, sizeof (*info));
    strlcpy (info->key, key, sizeof (info->key));
    info->value.type = PMIX_STRING;
    info->value.data.string = (char *)s;
}

static void set_int (pmix_info_t *info, const char *key, int i)
{
    memset (info, 0, sizeof (*info));
    strlcpy (info->key, key, sizeof (info->key));
    info->value.type = PMIX_INT;
    info->value.data.integer = i;
}

static void set_bool (pmix_info_t *info, const char *key, bool b)
{
    memset (info, 0, sizeof (*info));
    strlcpy (info->key, key, sizeof (info->key));
    info->value.type = PMIX_BOOL;
    info->value.data.flag = b;
}

int main (int argc, char **argv)
{
    optparse_t *p;
    int optindex;
    pmix_proc_t self;
    const char *message;
    const char *channel;
    const char *key;
    const char *val;
    int rank;
    int count;
    pmix_status_t expect;
    pmix_info_t data[8];
    size_t ndata = 0;
    pmix_info_t dirs[8];
    size_t ndirs = 0;
    int rc;

    /* Parse args
     */
    if (!(p = optparse_create ("pmixlog"))
        || optparse_add_option_table (p, opts) != OPTPARSE_SUCCESS
        || optparse_set (p, OPTPARSE_USAGE, opt_usage) != OPTPARSE_SUCCESS)
        log_msg_exit ("error setting up option parsing");
    if ((optindex = optparse_parse_args (p, argc, argv)) < 0)
        return 1;
    if (optindex != argc - 1) {
        optparse_print_usage (p);
        return 1;
    }
    message = argv[optindex];
    key = optparse_get_str (p, "key", NULL);
    val = optparse_get_str (p, "val", NULL);
    rank = optparse_get_int (p, "rank", -1);
    count = optparse_get_int (p, "count", 1);
    expect = status_lookup (optparse_get_str (p, "expect", "PMIX_SUCCESS"));

    /* Initialize and set log prefix to nspace.rank
     */
    char name[512];
    if ((rc = PMIx_Init (&self, NULL, 0)) != PMIX_SUCCESS)
        log_msg_exit ("PMIx_Init: %s", PMIx_Error_string (rc));
    snprintf (name, sizeof (name), "%s.%d", self.nspace, self.rank);
    log_init (name);
    if (self.rank == 0)
        log_msg ("completed PMIx_Init.");

    if (optparse_hasopt (p, "channel")) {
        while ((channel = optparse_getopt_next (p, "channel"))) {
            if (ndata == sizeof (data) / sizeof (data[0]))
                log_msg_exit ("too many channels");
            set_str (&data[ndata++], channel, message);
        }
    }
    else {
        set_str (&data[ndata++],
                 optparse_hasopt (p, "stdout")
                 ? PMIX_LOG_STDOUT : PMIX_LOG_STDERR,
                 message);
    }
    if (key)
        set_str (&dirs[ndirs++], PMIX_LOG_KEY, key);
    if (val)
        set_str (&dirs[ndirs++], PMIX_LOG_VAL, val);
    if (optparse_hasopt (p, "priority"))
        set_int (&dirs[ndirs++],
                 PMIX_LOG_SYSLOG_PRI,
                 optparse_get_int (p, "priority", 0));
    if (optparse_hasopt (p, "once"))
        set_bool (&dirs[ndirs++], PMIX_LOG_ONCE, true);
    if (optparse_hasopt (p, "no-agg"))
        set_bool (&dirs[ndirs++], PMIX_LOG_AGG, false);
    if (optparse_hasopt (p, "timestamp"))
        set_bool (&dirs[ndirs++], PMIX_LOG_GENERATE_TIMESTAMP, true);

    if (rank == -1 || rank == self.rank) {
        for (int i = 0; i < count; i++) {
            rc = PMIx_Log (data, ndata, dirs, ndirs);
            if (rc != expect)
                log_msg_exit ("PMIx_Log: %s", PMIx_Error_string (rc));
        }
    }

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
