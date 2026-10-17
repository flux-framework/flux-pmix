/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* main.c - main entry point for flux pmix shell plugin
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <argz.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <flux/shell.h>
#include <flux/taskmap.h>
#include <flux/idset.h>

#include <pmix_server.h>
#include <pmix.h>

#ifndef PMIX_PROC_INFO_ARRAY
#define PMIX_PROC_INFO_ARRAY PMIX_PROC_DATA // needed for pmix 3.2
#endif

#include "src/common/libutil/strlcpy.h"

#include "infovec.h"
#include "maps.h"
#include "interthread.h"
#include "fence.h"
#include "abort.h"
#include "notify.h"
#include "dmodex.h"

struct px {
    flux_shell_t *shell;
    flux_jobid_t id;
    char nspace[PMIX_MAX_NSLEN + 1];
    int shell_rank;
    int local_nprocs;
    int total_nprocs;
    const struct taskmap *taskmap;
    const char *job_tmpdir;
    bool server_initialized;
    struct interthread *it;
    struct fence *fence;
    struct abort *abort;
    struct notify *notify;
    struct dmodex *dmodex;
};

static pmix_server_module_t server_callbacks;

static void px_destroy (struct px *px)
{
    if (px) {
        int rc;
        int saved_errno = errno;
        notify_destroy (px->notify);
        if (px->server_initialized) {
            if ((rc = PMIx_server_finalize ()) != PMIX_SUCCESS)
                shell_warn ("PMIx_server_finalize: %s", PMIx_Error_string (rc));
        }
        abort_destroy (px->abort);
        fence_destroy (px->fence);
        dmodex_destroy (px->dmodex);
        interthread_destroy (px->it);
        free (px);
        errno = saved_errno;
    }
}

static int set_lpeers (struct infovec *iv,
                       const char *key,
                       const struct taskmap *taskmap,
                       int shell_rank)
{
    const struct idset *ids;
    char *s;

    if (!(ids = taskmap_taskids (taskmap, shell_rank))
        || !(s = idset_encode (ids, 0)))
        return -1;
    shell_debug ("local_peers = %s", s);
    if (infovec_set_str_new (iv, key, s) < 0) { // steals s
        free (s);
        return -1;
    }
    return 0;
}

static int set_node_map (struct infovec *iv,
                         const char *key,
                         flux_shell_t *shell)
{
    char *raw;
    char *cooked;
    int rc;

    if (!(raw = maps_node_create (shell)))
        return -1;
    shell_debug ("node_map = %s", raw);
    if ((rc = PMIx_generate_regex (raw, &cooked)) != PMIX_SUCCESS) {
        free (raw);
        shell_warn ("PMIx_generate_regex: %s", PMIx_Error_string (rc));
        return -1;
    }
    if (infovec_set_str_new (iv, key, cooked) < 0) { // steals cooked
        free (cooked);
        free (raw);
        return -1;
    }
    free (raw);
    return 0;
}

static int set_proc_map (struct infovec *iv,
                         const char *key,
                         const struct taskmap *taskmap)
{
    char *raw;
    char *cooked;
    int rc;

    if (!(raw = taskmap_encode (taskmap, TASKMAP_ENCODE_RAW_DERANGED)))
        return -1;
    shell_debug ("proc_map = %s", raw);
    if ((rc = PMIx_generate_ppn (raw, &cooked)) != PMIX_SUCCESS) {
        free (raw);
        shell_warn ("PMIx_generate_ppn: %s", PMIx_Error_string (rc));
        return -1;
    }
    if (infovec_set_str_new (iv, key, cooked) < 0) { // steals cooked
        free (cooked);
        free (raw);
        return -1;
    }
    free (raw);
    return 0;
}

static int get_local_rank (const struct taskmap *taskmap,
                           int nodeid,
                           int rank)
{
    const struct idset *ranks;
    unsigned int i;
    int local_rank = 0;

    if (!(ranks = taskmap_taskids (taskmap, nodeid)))
        return -1;
    i = idset_first (ranks);
    while (i != IDSET_INVALID_ID) {
        if (i == rank)
            return local_rank;
        local_rank++;
        i = idset_next (ranks, i);
    }
    return -1;
}

static int set_proc_infos (struct infovec *iv,
                           const char *key,
                           struct px *px)
{
    struct infovec *ri;

    for (int i = 0; i < px->total_nprocs; i++) {
        int nodeid;
        int local_rank;

        if ((nodeid = taskmap_nodeid (px->taskmap, i)) < 0
            || (local_rank = get_local_rank (px->taskmap, nodeid, i)) < 0)
            return -1;

        if (!(ri = infovec_create ())
            || infovec_set_rank (ri, PMIX_RANK, i) < 0
            || infovec_set_u32 (ri, PMIX_NODEID, nodeid) < 0
            || infovec_set_u16 (ri, PMIX_LOCAL_RANK, local_rank) < 0
            || infovec_set_u16 (ri, PMIX_NODE_RANK, local_rank) < 0
            || infovec_set_infovec_new (iv, key, ri) < 0) {
            shell_warn ("error setting %s for rank %d", key, i);
            infovec_destroy (ri);
            return -1;
        }
    }
    return 0;
}

static bool member_of_csv (const char *list, const char *name)
{
    char *argz = NULL;
    size_t argz_len;

    if (argz_create_sep (list, ',', &argz, &argz_len) == 0) {
        const char *entry = NULL;

        while ((entry = argz_next (argz, argz_len, entry))) {
            if (!strcmp (entry, name)) {
                free (argz);
                return true;
            }
        }
        free (argz);
    }
    return false;
}

static int px_init (flux_plugin_t *p,
                    const char *topic,
                    flux_plugin_arg_t *arg,
                    void *data)
{
    flux_shell_t *shell = flux_plugin_get_shell (p);
    const char *pmi_opt = NULL;
    struct px *px;
    int rc;
    pmix_info_t info[3];
    struct infovec *iv;
    char cwd[PATH_MAX + 1];

    if (flux_shell_getopt_unpack (shell, "pmi", "s", &pmi_opt) < 0) {
        shell_log_error ("pmi shell option must be a string");
        return -1;
    }
    if (!pmi_opt || !member_of_csv (pmi_opt, "pmix"))
        return 0; // plugin disabled

    shell_debug ("server is enabled");

    /* flux-pmix does not yet share hwloc with MPI, which forces MPI to
     * go looking for it, sometimes at great cost to performance.
     * Until we can address flux-framework/flux-pmix#31 properly,
     * tell flux-core to share a hwloc xml file instead.
     */
    if (flux_shell_setopt_pack (shell, "hwloc", "{s:i}", "xmlfile", 1) < 0)
        shell_warn ("unable to set Flux hwloc.xmlfile shell option");

    if (!(px = calloc (1, sizeof (*px)))
        || flux_plugin_aux_set (p, "px", px, (flux_free_f)px_destroy) < 0) {
        px_destroy (px);
        return -1;
    }
    px->shell = shell;

    if (flux_shell_info_unpack (shell,
                                "{s:I s:i s:i}",
                                "jobid", &px->id,
                                "rank", &px->shell_rank,
                                "ntasks", &px->total_nprocs) < 0)
        return -1;
    shell_debug ("jobid = %ju", (uintmax_t)px->id);
    shell_debug ("shell_rank = %d", px->shell_rank);
    if (flux_job_id_encode (px->id, "f58", px->nspace, sizeof (px->nspace)) < 0)
        return -1;
    if (flux_shell_rank_info_unpack (shell,
                                     px->shell_rank,
                                     "{s:i}",
                                     "ntasks",
                                     &px->local_nprocs) < 0)
        return -1;
    shell_debug ("local_nprocs = %d", px->local_nprocs);
    shell_debug ("total_nprocs = %d", px->total_nprocs);
    if (!(px->job_tmpdir = flux_shell_getenv (shell, "FLUX_JOB_TMPDIR")))
        return -1;

    if (!(px->taskmap = flux_shell_get_taskmap (shell))) {
        shell_log_error ("failed to get taskmap");
        return -1;
    }

    if (px->shell_rank == 0) {
        const char *s = PMIx_Get_version ();
        const char *cp = strchr (s, ',');
        int len = cp ? cp - s : strlen (s);
        shell_debug ("server outsourced to %.*s", len, s);
    }
    if (!(px->it = interthread_create (shell))) {
        shell_log_error ("could not create interthread message channel ");
        return -1;
    }
    if (!(px->fence = fence_create (shell, px->it))) {
        shell_log_error ("could not create fence handler");
        return -1;
    }
    server_callbacks.fence_nb = fence_server_cb;
    if (!(px->abort = abort_create (shell, px->it))) {
        shell_log_error ("could not create abort handler");
        return -1;
    }
    server_callbacks.abort = abort_server_cb;
    if (!(px->dmodex = dmodex_create (shell, px->it))) {
        shell_log_error ("could not create dmodex handler");
        return -1;
    }
    server_callbacks.direct_modex = dmodex_server_cb;

    /* Offer clients only the hash store, which sends job data to each
     * client over its socket, and not a shared memory store the client
     * would map.
     *
     * The set offered is every gds component the server opens, chosen
     * from the PMIX_MCA_gds environment variable, so that is set here
     * before init.  The PMIX_GDS_MODULE info key does not do this: it
     * names the server's own internal store, which is hash regardless.
     *
     * The shared memory stores pack the segment's connection blob once,
     * with the server's own encoding, and hand that copy to every
     * client.  A client that announced a different encoding - a 3.x
     * client on a 5.x server, say - cannot read it and fails in
     * PMIx_Init.  The hash store packs each reply for the client that
     * asked, so it does not have this problem.  (openpmix 5.0.x,
     * gds_shmem2.c cache_connection_info_for_job_shmem2.)
     *
     * This is a performance tradeoff that depends on the workload.
     * Shared memory makes every PMIx_Get after startup a local read,
     * at the cost of pushing the whole job data image through the
     * socket once per client - ~186 KiB against ~800 bytes for hash
     * in a 1-node 4-task job here.  Open MPI's bootstrap, as traced,
     * does one or two gets per rank and otherwise fences, so for it
     * the shared memory setup has nothing to amortize over.  A
     * workload doing many gets per rank could see it differently.
     *
     * Overwrite rather than defer to an existing setting: the point
     * is that the choice not be left to the environment.
     */
    if (setenv ("PMIX_MCA_gds", "hash", 1) < 0) {
        shell_log_error ("setenv PMIX_MCA_gds: %s", strerror (errno));
        return -1;
    }

    strlcpy (info[0].key, PMIX_SERVER_TMPDIR, sizeof (info[0].key));
    info[0].value.type = PMIX_STRING;
    info[0].value.data.string = (char *)px->job_tmpdir;
    info[0].flags = 0;

    strlcpy (info[1].key, PMIX_SERVER_RANK, sizeof (info[1].key));
    info[1].value.type = PMIX_PROC_RANK;
    info[1].value.data.rank = px->shell_rank;
    info[1].flags = 0;

    /* Ensure pmix sockets are restricted to localhost to restrict the
     * impact of issues with psec authentication, which is not under our
     * control.  openpmix would otherwise choose the interface based on
     * ptl MCA parameters, which may be set in the environment or in a
     * user owned configuration file.  This info key is applied after
     * those and overrides them.
     * N.B. openpmix fails init rather than letting this win if an
     * if_exclude setting is also in effect.
     * N.B. the subnet alone would not be safe.  openpmix resolves it by
     * walking the addressed interfaces, so in a container namespace
     * where loopback is down or unaddressed it matches nothing, and an
     * include list that resolves to nothing is dropped entirely - the
     * listener then falls back to any interface, including a routable
     * one.  The device name is matched by link instead of by address,
     * so it still rejects the routable interface in that case and init
     * fails rather than quietly opening up.  Hence both forms.
     */
    strlcpy (info[2].key, PMIX_TCP_IF_INCLUDE, sizeof (info[2].key));
    info[2].value.type = PMIX_STRING;
    info[2].value.data.string = "lo,127.0.0.0/8";
    info[2].flags = 0;

    if ((rc = PMIx_server_init (&server_callbacks, info, 3)) != PMIX_SUCCESS) {
        shell_warn ("PMIx_server_init: %s", PMIx_Error_string (rc));
        return -1;
    }
    px->server_initialized = true;

    if (!(px->notify = notify_create (shell, px->it))) {
        shell_log_error ("could not create notify handler");
        return -1;
    }

    /* N.B. the shell is started in the job's working directory, so
     * getcwd() is the value spawned processes would have inherited
     * anyway.  Register it because the spec requires it, not to change
     * what any process sees.
     */
    if (!getcwd (cwd, sizeof (cwd))) {
        shell_log_error ("getcwd: %s", strerror (errno));
        return -1;
    }

    if (!(iv = infovec_create ())
        || infovec_set_str (iv, PMIX_JOBID, px->nspace) < 0
        || infovec_set_str (iv, PMIX_WDIR, cwd) < 0
        || set_lpeers (iv, PMIX_LOCAL_PEERS, px->taskmap, px->shell_rank) < 0
        || set_node_map (iv, PMIX_NODE_MAP, shell) < 0
        || set_proc_map (iv, PMIX_PROC_MAP, px->taskmap) < 0
        || infovec_set_bool (iv, PMIX_TDIR_RMCLEAN, true) < 0
        || infovec_set_u32 (iv, PMIX_JOB_NUM_APPS, 1) < 0
        || infovec_set_str (iv, PMIX_TMPDIR, px->job_tmpdir) < 0
        || infovec_set_u32 (iv, PMIX_LOCAL_SIZE, px->local_nprocs) < 0
        || infovec_set_u32 (iv, PMIX_UNIV_SIZE, px->total_nprocs) < 0
        || infovec_set_u32 (iv, PMIX_JOB_SIZE, px->total_nprocs) < 0
        || infovec_set_u32 (iv, PMIX_APPNUM, 0) < 0
        || set_proc_infos (iv, PMIX_PROC_INFO_ARRAY, px) < 0) {
        shell_log_error ("error creating namespace");
        goto error;
    }

    if ((rc = PMIx_server_register_nspace (px->nspace,
                                           px->local_nprocs,
                                           infovec_info (iv),
                                           infovec_count (iv),
                                           NULL,
                                           NULL)) != PMIX_OPERATION_SUCCEEDED) {
        shell_warn ("PMIx_server_register_nspace: %s", PMIx_Error_string (rc));
        goto error;
    }
    infovec_destroy (iv);
    return 0;
error:
    infovec_destroy (iv);
    return -1;
}

static int px_task_init (flux_plugin_t *p,
                         const char *topic,
                         flux_plugin_arg_t *args,
                         void *arg)
{
    flux_shell_t *shell;
    struct px *px;
    flux_shell_task_t *task;
    flux_cmd_t *cmd;
    pmix_proc_t proc;
    char **env = NULL;
    int rank;
    int rc;

    if (!(px = flux_plugin_aux_get (p, "px")))
        return 0; // plugin disabled
    if (!(shell = flux_plugin_get_shell (p))
        || !(task = flux_shell_current_task (shell))
        || !(cmd = flux_shell_task_cmd (task))
        || flux_shell_task_info_unpack (task, "{s:i}", "rank", &rank) < 0)
        return -1;

    proc.rank = rank;
    snprintf (proc.nspace, sizeof (proc.nspace), "%s", px->nspace);

    /* Fetch this task's PMIX_* environment and add to task's
     * subprocess command.
     */
    if ((rc = PMIx_server_setup_fork (&proc, &env)) != PMIX_SUCCESS) {
        shell_warn ("PMIx_server_setup_fork %s.%d: %s",
                    proc.nspace,
                    proc.rank,
                    PMIx_Error_string (rc));
        return -1;
    }
    if (env) {
        int i;
        for (i = 0; env[i] != NULL; i++) {
            char *name = env[i];
            char *value = strchr (name, '=');
            if (!value) { // no '=' separator - nothing to set
                shell_warn ("ignoring malformed environment entry %s", name);
                continue;
            }
            *value++ = '\0';
            if (flux_cmd_setenvf (cmd, 1, name, "%s", value) < 0) {
                shell_warn ("flux_cmd_setenvf %s failed", name);
                return -1;
            }
        }
        free (env);
    }

    /* Allow task rank and uid/gid (same as ours) to connect to
     * the server tcp port.
     * SECURITY: By default openpmix uses its "native" authentication method
     * which we should verify does some something meaningful to prevent other
     * users from claiming to be this user on connect.
     */
    if ((rc = PMIx_server_register_client (&proc,
                                           getuid (),
                                           getgid (),
                                           NULL,
                                           NULL,
                                           NULL)) != PMIX_OPERATION_SUCCEEDED) {
        shell_warn ("PMIx_server_register_client %s.%d: %s",
                    proc.nspace,
                    proc.rank,
                    PMIx_Error_string (rc));
        return -1;
    }
    return 0;
}

int flux_plugin_init (flux_plugin_t *p)
{
    if (flux_plugin_set_name (p, FLUX_SHELL_PLUGIN_NAME) < 0
        || flux_plugin_add_handler (p, "shell.init", px_init, NULL) < 0
        || flux_plugin_add_handler (p, "task.init",  px_task_init, NULL) < 0) {
        return -1;
    }

    return 0;
}

// vi:ts=4 sw=4 expandtab
