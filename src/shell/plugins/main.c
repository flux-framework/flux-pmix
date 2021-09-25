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
#include <flux/shell.h>

#include <pmix_server.h>
#include <pmix.h>

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
    const char *job_tmpdir;
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
        if ((rc = PMIx_server_finalize ()) != PMIX_SUCCESS)
            shell_warn ("PMIx_server_finalize: %s", PMIx_Error_string (rc));
        notify_destroy (px->notify);
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
                       flux_shell_t *shell)
{
    char *s;

    if (!(s = maps_lpeers_create (shell)))
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
    if ((rc = PMIx_generate_regex (raw, &cooked) != PMIX_SUCCESS)) {
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
                         flux_shell_t *shell)
{
    char *raw;
    char *cooked;
    int rc;

    if (!(raw = maps_proc_create (shell)))
        return -1;
    shell_debug ("proc_map = %s", raw);
    if ((rc = PMIx_generate_ppn (raw, &cooked) != PMIX_SUCCESS)) {
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

static int px_init (flux_plugin_t *p,
                    const char *topic,
                    flux_plugin_arg_t *arg,
                    void *data)
{
    flux_shell_t *shell = flux_plugin_get_shell (p);
    struct px *px;
    int rc;
    pmix_info_t info[2] = { 0 };
    const char *s;
    struct infovec *iv;

    if (!(px = calloc (1, sizeof (*px)))
        || flux_plugin_aux_set (p, "px", px, (flux_free_f)px_destroy) < 0) {
        px_destroy (px);
        return -1;
    }
    px->shell = shell;

    if (flux_shell_info_unpack (shell,
                                "{s:I s:i}",
                                "jobid", &px->id,
                                "rank", &px->shell_rank) < 0)
        return -1;
    if (flux_job_id_encode (px->id, "f58", px->nspace, sizeof (px->nspace)) < 0)
        return -1;
    if (flux_shell_rank_info_unpack (shell,
                                     px->shell_rank,
                                     "{s:i}",
                                     "ntasks",
                                     &px->local_nprocs) < 0)
        return -1;
    if (flux_shell_jobspec_info_unpack (shell,
                                        "{s:i}",
                                        "ntasks",
                                        &px->total_nprocs) < 0)
        return -1;
    if (!(px->job_tmpdir = flux_shell_getenv (shell, "FLUX_JOB_TMPDIR")))
        return -1;

    if (px->shell_rank == 0) {
        const char *s = PMIx_Get_version ();
        const char *cp = strchr (s, ',');
        int len = cp ? cp - s : strlen (s);
        shell_debug ("server outsourced to %.*s", len, s);
    }
    if (!(px->it = interthread_create (shell)))
        return -1;
    if (!(px->fence = fence_create (shell, px->it)))
        return -1;
    server_callbacks.fence_nb = fence_server_cb;
    if (!(px->abort = abort_create (shell, px->it)))
        return -1;
    server_callbacks.abort = abort_server_cb;
    if (!(px->dmodex = dmodex_create (shell, px->it)))
        return -1;
    server_callbacks.direct_modex = dmodex_server_cb;

    strncpy (info[0].key, PMIX_SERVER_TMPDIR, PMIX_MAX_KEYLEN);
    info[0].value.type = PMIX_STRING;
    info[0].value.data.string = (char *)px->job_tmpdir;

    strncpy (info[1].key, PMIX_SERVER_RANK, PMIX_MAX_KEYLEN);
    info[1].value.type = PMIX_PROC_RANK;
    info[1].value.data.rank = px->shell_rank;

    if ((rc = PMIx_server_init (&server_callbacks, info, 2)) != PMIX_SUCCESS) {
        shell_warn ("PMIx_server_init: %s", PMIx_Error_string (rc));
        return -1;
    }

    if (!(px->notify = notify_create (shell, px->it)))
        return -1;

    if (!(iv = infovec_create ())
        || set_lpeers (iv, PMIX_LOCAL_PEERS, shell) < 0
        || set_node_map (iv, PMIX_NODE_MAP, shell) < 0
        || set_proc_map (iv, PMIX_PROC_MAP, shell) < 0
        || infovec_set_str (iv, PMIX_NSDIR, px->job_tmpdir) < 0
        || infovec_set_u32 (iv, PMIX_JOB_NUM_APPS, 1) < 0
        || infovec_set_str (iv, PMIX_TMPDIR, px->job_tmpdir) < 0
        || infovec_set_u32 (iv, PMIX_LOCAL_SIZE, px->local_nprocs) < 0
        || infovec_set_u32 (iv, PMIX_UNIV_SIZE, px->total_nprocs) < 0
        || infovec_set_u32 (iv, PMIX_JOB_SIZE, px->total_nprocs) < 0)
        goto error;

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
    pmix_proc_t proc = { 0 };
    char **env = NULL;
    int rank;
    int rc;

    if (!(shell = flux_plugin_get_shell (p))
        || !(px = flux_plugin_aux_get (p, "px"))
        || !(task = flux_shell_current_task (shell))
        || !(cmd = flux_shell_task_cmd (task))
        || flux_shell_task_info_unpack (task, "{s:i}", "rank", &rank) < 0)
        return -1;

    proc.rank = rank;
    strncpy (proc.nspace, px->nspace, PMIX_MAX_NSLEN);

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
            if (value)
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
    if (flux_plugin_set_name (p, "pmix") < 0
        || flux_plugin_add_handler (p, "shell.init", px_init, NULL) < 0
        || flux_plugin_add_handler (p, "task.init",  px_task_init, NULL) < 0) {
        return -1;
    }
    return 0;
}

// vi:ts=4 sw=4 expandtab
