/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 \************************************************************/

/* query.c - PMIx_Query_info the keys named on the command line
 *
 * The keys go in one query.  The nonblocking form is used so that
 * a 3.2 client, which has no blocking form, can run it.  Each
 * result is printed as key=value.  The status is checked against
 * --expect, named rather than spelled since PMIx_Error_string()
 * varies by openpmix version.
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <pmix.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <flux/optparse.h>

#include "src/common/libutil/strlcpy.h"
#include "log.h"

#ifndef PMIX_ERR_PARTIAL_SUCCESS
#define PMIX_ERR_PARTIAL_SUCCESS PMIX_QUERY_PARTIAL_SUCCESS
#endif

const char *opt_usage = "[OPTIONS] key [key...]";

static struct optparse_option opts[] = {
    { .name = "refresh", .has_arg = 0,
      .usage = "Add the PMIX_QUERY_REFRESH_CACHE qualifier",
    },
    { .name = "expect", .has_arg = 1, .arginfo = "NAME",
      .usage = "expect this status (default=PMIX_SUCCESS)",
    },
    { .name = "rank", .has_arg = 1, .arginfo = "RANK",
      .usage = "Query only on RANK (default=all)",
    },
    { .name = "proc", .has_arg = 1, .arginfo = "RANK|*",
      .usage = "Add a PMIX_PROCID qualifier naming RANK in this namespace",
    },
    OPTPARSE_TABLE_END,
};

static const struct {
    const char *name;
    pmix_status_t status;
} status_names[] = {
    { "PMIX_SUCCESS", PMIX_SUCCESS },
    { "PMIX_ERR_NOT_FOUND", PMIX_ERR_NOT_FOUND },
    { "PMIX_ERR_PARTIAL_SUCCESS", PMIX_ERR_PARTIAL_SUCCESS },
    { "PMIX_ERR_NOT_SUPPORTED", PMIX_ERR_NOT_SUPPORTED },
    { "PMIX_ERR_UNREACH", PMIX_ERR_UNREACH },
};

static pmix_status_t status_lookup (const char *name)
{
    for (int i = 0; i < sizeof (status_names) / sizeof (status_names[0]); i++)
        if (!strcmp (status_names[i].name, name))
            return status_names[i].status;
    log_msg_exit ("unknown status name %s", name);
}

struct wait {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool done;
    pmix_status_t status;
};

static void print_info (const pmix_info_t *info)
{
    const pmix_value_t *val = &info->value;

    switch (val->type) {
        case PMIX_BOOL:
            printf ("%s=%s\n", info->key, val->data.flag ? "true" : "false");
            break;
        case PMIX_STRING:
            printf ("%s=%s\n", info->key, val->data.string);
            break;
        case PMIX_SIZE:
            printf ("%s=%zu\n", info->key, val->data.size);
            break;
        case PMIX_UINT32:
            printf ("%s=%lu\n", info->key, (unsigned long)val->data.uint32);
            break;
        case PMIX_UINT16:
            printf ("%s=%hu\n", info->key, (unsigned short)val->data.uint16);
            break;
        default:
            printf ("%s=<type %d>\n", info->key, (int)val->type);
            break;
    }
}

static void info_cb (pmix_status_t status,
                     pmix_info_t *info,
                     size_t ninfo,
                     void *cbdata,
                     pmix_release_cbfunc_t release_fn,
                     void *release_cbdata)
{
    struct wait *w = cbdata;

    for (size_t i = 0; i < ninfo; i++)
        print_info (&info[i]);
    if (release_fn)
        release_fn (release_cbdata);
    pthread_mutex_lock (&w->lock);
    w->status = status;
    w->done = true;
    pthread_cond_signal (&w->cond);
    pthread_mutex_unlock (&w->lock);
}

int main (int argc, char **argv)
{
    optparse_t *p;
    int optindex;
    pmix_proc_t self;
    pmix_query_t query;
    pmix_info_t qual[2];
    pmix_proc_t target;
    size_t nqual = 0;
    struct wait w;
    pmix_status_t expect;
    int rank;
    int rc;

    if (!(p = optparse_create ("query"))
        || optparse_add_option_table (p, opts) != OPTPARSE_SUCCESS
        || optparse_set (p, OPTPARSE_USAGE, opt_usage) != OPTPARSE_SUCCESS)
        log_msg_exit ("error setting up option parsing");
    if ((optindex = optparse_parse_args (p, argc, argv)) < 0)
        return 1;
    if (optindex >= argc) {
        optparse_print_usage (p);
        return 1;
    }
    expect = status_lookup (optparse_get_str (p, "expect", "PMIX_SUCCESS"));

    char name[512];
    if ((rc = PMIx_Init (&self, NULL, 0)) != PMIX_SUCCESS)
        log_msg_exit ("PMIx_Init: %s", PMIx_Error_string (rc));
    snprintf (name, sizeof (name), "%s.%d", self.nspace, self.rank);
    log_init (name);

    rank = optparse_get_int (p, "rank", -1);
    if (rank == -1 || rank == self.rank) {
        memset (&query, 0, sizeof (query));
        query.keys = argv + optindex; // argv is NULL terminated
        memset (qual, 0, sizeof (qual));
        if (optparse_hasopt (p, "refresh")) {
            strlcpy (qual[nqual].key,
                     PMIX_QUERY_REFRESH_CACHE,
                     sizeof (qual[nqual].key));
            qual[nqual].value.type = PMIX_BOOL;
            qual[nqual].value.data.flag = true;
            nqual++;
        }
        if (optparse_hasopt (p, "proc")) {
            const char *s = optparse_get_str (p, "proc", NULL);
            strlcpy (target.nspace, self.nspace, sizeof (target.nspace));
            if (!strcmp (s, "*"))
                target.rank = PMIX_RANK_WILDCARD;
            else
                target.rank = strtoul (s, NULL, 10);
            strlcpy (qual[nqual].key, PMIX_PROCID, sizeof (qual[nqual].key));
            qual[nqual].value.type = PMIX_PROC;
            qual[nqual].value.data.proc = &target;
            nqual++;
        }
        /* A non-NULL qualifiers with nqual 0 makes the client scan
         * for an end marker, so hand it NULL when there are none.
         */
        query.qualifiers = nqual > 0 ? qual : NULL;
        query.nqual = nqual;
        pthread_mutex_init (&w.lock, NULL);
        pthread_cond_init (&w.cond, NULL);
        w.done = false;
        w.status = PMIX_SUCCESS;
        rc = PMIx_Query_info_nb (&query, 1, info_cb, &w);
        if (rc == PMIX_SUCCESS) {
            pthread_mutex_lock (&w.lock);
            while (!w.done)
                pthread_cond_wait (&w.cond, &w.lock);
            pthread_mutex_unlock (&w.lock);
            rc = w.status;
        }
        if (rc != expect)
            log_msg_exit ("PMIx_Query_info: %s", PMIx_Error_string (rc));
        log_msg ("PMIx_Query_info: %s", PMIx_Error_string (rc));
    }

    if ((rc = PMIx_Finalize (NULL, 0)))
        log_msg_exit ("PMIx_Finalize: %s", PMIx_Error_string (rc));
    optparse_destroy (p);
    return 0;
}

// vi:ts=4 sw=4 expandtab
