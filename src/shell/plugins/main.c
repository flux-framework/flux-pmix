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

struct px {
    flux_shell_t *shell;
    flux_jobid_t id;
    char nspace[PMIX_MAX_NSLEN + 1];
    int shell_rank;
    int local_nprocs;
    int total_nprocs;
    const char *job_tmpdir;
};

static void px_destroy (struct px *px)
{
    if (px) {
        int rc;
        int saved_errno = errno;
        if ((rc = PMIx_server_finalize ()))
            shell_warn ("PMIx_server_finalize: %s", PMIx_Error_string (rc));
        free (px);
        errno = saved_errno;
    }
}

static int px_init (flux_plugin_t *p,
                    const char *topic,
                    flux_plugin_arg_t *arg,
                    void *data)
{
    flux_shell_t *shell = flux_plugin_get_shell (p);
    struct px *px;
    int rc;
    pmix_info_t info = { 0 };
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

    strncpy (info.key, PMIX_SERVER_TMPDIR, PMIX_MAX_KEYLEN);
    info.value.type = PMIX_STRING;
    info.value.data.string = (char *)px->job_tmpdir;

    if ((rc = PMIx_server_init (NULL, &info, 1)) != PMIX_SUCCESS) {
        shell_warn ("PMIx_server_init: %s", PMIx_Error_string (rc));
        return -1;
    }

    if (!(iv = infovec_create ())
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
