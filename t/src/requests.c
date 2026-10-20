/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 \************************************************************/

/* requests.c - send one of each request the server need not implement
 *
 * The replies are expected to be PMIX_ERR_NOT_SUPPORTED, or whatever
 * the server says; what matters is that the requests go on the wire
 * with the client library's own packing, so that a capture of this
 * program exercises the dissector on every command.  Each call is
 * logged with its status and none is fatal.  Requests that the
 * client library is too old to have are logged as skipped, so that
 * the test can tell which to expect.
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <pmix.h>
#include <pmix_version.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/libutil/strlcpy.h"
#include "log.h"

static void report (const char *name, pmix_status_t rc)
{
    log_msg ("%s: %s", name, PMIx_Error_string (rc));
}

/* REQUESTS_STOP=NAME ends the run after the request named, which
 * narrows a hang to one request.
 */
static void stop_after (const char *name)
{
    const char *stop = getenv ("REQUESTS_STOP");
    if (stop && !strcmp (stop, name)) {
        log_msg ("stopping after %s", name);
        PMIx_Finalize (NULL, 0);
        exit (0);
    }
}

static void __attribute__ ((unused)) skipped (const char *name)
{
    log_msg ("%s: skipped, not in this client", name);
}

/* The blocking forms of some calls changed signature between 3.x
 * and 4.x; the nonblocking forms did not, so those are used with a
 * callback that just hands the status back.
 */
struct wait {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool done;
    pmix_status_t status;
};

static void wait_init (struct wait *w)
{
    pthread_mutex_init (&w->lock, NULL);
    pthread_cond_init (&w->cond, NULL);
    w->done = false;
    w->status = PMIX_SUCCESS;
}

static void wait_done (struct wait *w, pmix_status_t status)
{
    pthread_mutex_lock (&w->lock);
    w->status = status;
    w->done = true;
    pthread_cond_signal (&w->cond);
    pthread_mutex_unlock (&w->lock);
}

static pmix_status_t wait_for (struct wait *w, pmix_status_t rc)
{
    if (rc != PMIX_SUCCESS)
        return rc;
    pthread_mutex_lock (&w->lock);
    while (!w->done)
        pthread_cond_wait (&w->cond, &w->lock);
    pthread_mutex_unlock (&w->lock);
    return w->status;
}

static void info_cb (pmix_status_t status,
                     pmix_info_t *info,
                     size_t ninfo,
                     void *cbdata,
                     pmix_release_cbfunc_t release_fn,
                     void *release_cbdata)
{
    if (release_fn)
        release_fn (release_cbdata);
    wait_done (cbdata, status);
}

static void __attribute__ ((unused)) cred_cb (pmix_status_t status,
                     pmix_byte_object_t *credential,
                     pmix_info_t info[],
                     size_t ninfo,
                     void *cbdata)
{
    wait_done (cbdata, status);
}

static void __attribute__ ((unused)) valid_cb (pmix_status_t status,
                      pmix_info_t info[],
                      size_t ninfo,
                      void *cbdata)
{
    wait_done (cbdata, status);
}

static void info_string (pmix_info_t *info, const char *key, const char *s)
{
    memset (info, 0, sizeof (*info));
    strlcpy (info->key, key, sizeof (info->key));
    info->value.type = PMIX_STRING;
    info->value.data.string = (char *)s;
}

static void info_bool (pmix_info_t *info, const char *key, bool b)
{
    memset (info, 0, sizeof (*info));
    strlcpy (info->key, key, sizeof (info->key));
    info->value.type = PMIX_BOOL;
    info->value.data.flag = b;
}

int main (int argc, char **argv)
{
    pmix_proc_t self;
    pmix_proc_t wild;
    pmix_info_t info[2];
    pmix_info_t *results = NULL;
    size_t nresults = 0;
    struct wait w;
    int rc;

    char name[512];
    if ((rc = PMIx_Init (&self, NULL, 0)) != PMIX_SUCCESS)
        log_msg_exit ("PMIx_Init: %s", PMIx_Error_string (rc));
    snprintf (name, sizeof (name), "%s.%d", self.nspace, self.rank);
    log_init (name);

    strlcpy (wild.nspace, self.nspace, sizeof (wild.nspace));
    wild.rank = PMIX_RANK_WILDCARD;

    /* publish, lookup, unpublish */
    info_string (&info[0], "test.key", "test value");
    report ("PMIx_Publish", PMIx_Publish (info, 1));
    stop_after ("PMIx_Publish");
    pmix_pdata_t pdata;
    PMIX_PDATA_CONSTRUCT (&pdata);
    strlcpy (pdata.key, "test.key", sizeof (pdata.key));
    report ("PMIx_Lookup", PMIx_Lookup (&pdata, 1, NULL, 0));
    stop_after ("PMIx_Lookup");
    PMIX_PDATA_DESTRUCT (&pdata);
    char *keys[] = { "test.key", NULL };
    report ("PMIx_Unpublish", PMIx_Unpublish (keys, NULL, 0));
    stop_after ("PMIx_Unpublish");

    /* spawn */
    pmix_app_t app;
    char *app_argv[] = { "/bin/true", "--flag", NULL };
    char *app_env[] = { "FOO=bar", NULL };
    PMIX_APP_CONSTRUCT (&app);
    app.cmd = "/bin/true";
    app.argv = app_argv;
    app.env = app_env;
    app.cwd = "/tmp";
    app.maxprocs = 2;
    pmix_nspace_t newns;
    info_bool (&info[0], PMIX_SPAWN_TOOL, false);
    report ("PMIx_Spawn", PMIx_Spawn (info, 1, &app, 1, newns));
    stop_after ("PMIx_Spawn");

    /* connect, disconnect */
    report ("PMIx_Connect", PMIx_Connect (&wild, 1, NULL, 0));
    stop_after ("PMIx_Connect");
    report ("PMIx_Disconnect", PMIx_Disconnect (&wild, 1, NULL, 0));
    stop_after ("PMIx_Disconnect");

    /* query (the blocking form is 4.0 and later) */
#if PMIX_NUMERIC_VERSION >= 0x00040000
    pmix_query_t query;
    char *query_keys[] = { PMIX_QUERY_PSET_NAMES, NULL };
    pmix_info_t qual[1];
    PMIX_QUERY_CONSTRUCT (&query);
    query.keys = query_keys;
    query.qualifiers = qual;
    query.nqual = 1;
    info_bool (&qual[0], PMIX_QUERY_REFRESH_CACHE, true);
    report ("PMIx_Query_info", PMIx_Query_info (&query, 1,
                                                &results, &nresults));
    stop_after ("PMIx_Query_info");
#else
    skipped ("PMIx_Query_info");
#endif

    /* log */
    info_string (&info[0], PMIX_LOG_STDERR, "hello from requests");
    report ("PMIx_Log", PMIx_Log (info, 1, NULL, 0));
    stop_after ("PMIx_Log");

    /* allocate */
    info_string (&info[0], PMIX_ALLOC_ID, "alloc-1");
    report ("PMIx_Allocation_request",
            PMIx_Allocation_request (PMIX_ALLOC_NEW, info, 1,
                                     &results, &nresults));
    stop_after ("PMIx_Allocation_request");

    /* job control */
    info_bool (&info[0], PMIX_JOB_CTRL_PAUSE, true);
    wait_init (&w);
    report ("PMIx_Job_control",
            wait_for (&w, PMIx_Job_control_nb (&self, 1, info, 1,
                                               info_cb, &w)));
    stop_after ("PMIx_Job_control");

    /* monitor */
    pmix_info_t monitor;
    info_string (&monitor, PMIX_MONITOR_HEARTBEAT, "");
    wait_init (&w);
    report ("PMIx_Process_monitor",
            wait_for (&w, PMIx_Process_monitor_nb (&monitor,
                                                   PMIX_MONITOR_HEARTBEAT_ALERT,
                                                   NULL, 0, info_cb, &w)));
    stop_after ("PMIx_Process_monitor");

    /* credentials (4.0 and later).  A 3.x client that is refused a
     * credential frees an unconstructed byte object and aborts, so
     * these are not sent to one.
     */
#if PMIX_NUMERIC_VERSION >= 0x00040000
    pmix_byte_object_t cred = { .bytes = "abc", .size = 3 };
    wait_init (&w);
    report ("PMIx_Get_credential",
            wait_for (&w, PMIx_Get_credential_nb (NULL, 0, cred_cb, &w)));
    stop_after ("PMIx_Get_credential");
    wait_init (&w);
    report ("PMIx_Validate_credential",
            wait_for (&w, PMIx_Validate_credential_nb (&cred, NULL, 0,
                                                       valid_cb, &w)));
    stop_after ("PMIx_Validate_credential");
#else
    skipped ("PMIx_Get_credential");
    skipped ("PMIx_Validate_credential");
#endif

    /* groups (5.0 and later).  A 4.x client never returns from a
     * construct that the server refuses, nor from finalize with one
     * outstanding, so it is sent only from 5.0.  Leave and destruct
     * of a group that was never constructed crash the 5.0 client
     * library, so they are not sent.
     */
#if PMIX_NUMERIC_VERSION >= 0x00050000
    info_bool (&info[0], PMIX_GROUP_ASSIGN_CONTEXT_ID, true);
    report ("PMIx_Group_construct",
            PMIx_Group_construct ("grp", &self, 1, info, 1,
                                  &results, &nresults));
    stop_after ("PMIx_Group_construct");
#else
    skipped ("PMIx_Group_construct");
#endif

    /* resolve (5.0 and later).  Earlier clients answer these from
     * their own data, or with a get that a 4.x client waits on
     * forever when the server has no answer.
     */
#if PMIX_NUMERIC_VERSION >= 0x00050000
    pmix_proc_t *procs = NULL;
    size_t nprocs = 0;
    report ("PMIx_Resolve_peers",
            PMIx_Resolve_peers (NULL, self.nspace, &procs, &nprocs));
    stop_after ("PMIx_Resolve_peers");
    char *nodelist = NULL;
    report ("PMIx_Resolve_nodes", PMIx_Resolve_nodes (self.nspace,
                                                      &nodelist));
    stop_after ("PMIx_Resolve_nodes");
#else
    skipped ("PMIx_Resolve_peers");
    skipped ("PMIx_Resolve_nodes");
#endif

    /* session control (5.0 and later) */
#if PMIX_NUMERIC_VERSION >= 0x00050000
    info_bool (&info[0], PMIX_SESSION_PAUSE, true);
    report ("PMIx_Session_control",
            PMIx_Session_control (0, info, 1, NULL, NULL));
    stop_after ("PMIx_Session_control");
#else
    skipped ("PMIx_Session_control");
#endif

    /* fabric (5.0 and later).  A 4.x client never returns from a
     * fabric-register that the server refuses.
     */
#if PMIX_NUMERIC_VERSION >= 0x00050000
    pmix_fabric_t fabric;
    PMIX_FABRIC_CONSTRUCT (&fabric);
    report ("PMIx_Fabric_register", PMIx_Fabric_register (&fabric, NULL, 0));
    stop_after ("PMIx_Fabric_register");
#else
    skipped ("PMIx_Fabric_register");
#endif

    if ((rc = PMIx_Finalize (NULL, 0)))
        log_msg_exit ("PMIx_Finalize: %s", PMIx_Error_string (rc));
    return 0;
}
