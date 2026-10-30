/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* log.c - handle log callback from openpmix server
 *
 * PMIx_Log() data entries on the stderr and stdout channels are
 * written to the calling task's stream.  Entries on the syslog and
 * job record channels go to the shell log, which is the job's record
 * and the system log of a server embedded in the job.  Since those
 * channels are one destination, a message on both is written once,
 * at the syslog priority.  Other channels fail.
 *
 * Open MPI tags each help message with PMIX_LOG_KEY and PMIX_LOG_VAL.
 * A shell writes the first message with a tag and drops the rest.
 * Some openpmix versions also drop repeats before the callback, so the
 * shell cannot count on seeing them.  To drop repeats across shells,
 * a shell asks shell 0 before writing its first.  Shell 0 says yes to
 * the first and counts the rest, and at exit logs one line per tag
 * that was dropped.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <jansson.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>
#include <flux/core.h>
#include <flux/shell.h>
#include <pmix.h>
#include <pmix_server.h>

#include "codec.h"
#include "interthread.h"

#include "log.h"

/* openpmix sets this internal flag on a data entry it has already
 * handled, so the entry is skipped here.
 */
#define PX_INFO_OP_COMPLETE 0x80000000

/* openpmix 3.2 has no log tags.  The strings are those of later
 * versions, so a client that sends them is served.
 */
#ifndef PMIX_LOG_KEY
#define PMIX_LOG_KEY "pmix.log.key"
#endif
#ifndef PMIX_LOG_VAL
#define PMIX_LOG_VAL "pmix.log.val"
#endif

struct log_tuple {
    char *key;
    char *val;
    int count;
    struct log_tuple *next;
};

struct log_pending;

struct log {
    flux_shell_t *shell;
    struct interthread *it;
    int shell_rank;
    bool dedup;
    struct log_tuple *tuples;    // tags seen on this shell
    struct log_tuple *jobtuples; // shell 0 only: tags seen on any shell
    struct log_pending *asks;    // asks of shell 0 that have not answered
};

/* One PMIx_Log() call.  The client is blocked until cbfunc is called,
 * and its streams close when it exits, so cbfunc waits for shell 0's
 * answers.
 */
struct log_call {
    struct log *log;
    pmix_op_cbfunc_t cbfunc;
    void *cbdata;
    int rc;
    int pending;
};

struct log_pending {
    struct log_call *lc;
    int rank;
    char *stream;
    char *msg;
    flux_future_t *f;
    struct log_pending *next;
};

/* This is for the benefit of server callbacks that don't have
 * a way to be passed a user-supplied opaque pointer.
 */
static struct log *global_log_ctx;

static void log_call_release (struct log_call *lc)
{
    if (lc && --lc->pending == 0) {
        if (lc->cbfunc)
            lc->cbfunc (lc->rc, lc->cbdata); // release the calling process
        free (lc);
    }
}

static int task_write (flux_shell_t *shell,
                       int rank,
                       const char *stream,
                       const char *msg)
{
    flux_plugin_arg_t *args;
    char rankstr[16];
    size_t len = strlen (msg);
    bool nl = len > 0 && msg[len - 1] == '\n';
    int rc = -1;

    snprintf (rankstr, sizeof (rankstr), "%d", rank);
    if (!(args = flux_plugin_arg_create ())
        || flux_plugin_arg_pack (args,
                                 FLUX_PLUGIN_ARG_IN,
                                 "{s:s s:s s:s+}",
                                 "stream", stream,
                                 "rank", rankstr,
                                 "data", msg, nl ? "" : "\n") < 0)
        goto done;
    rc = flux_shell_plugstack_call (shell, "shell.output", args);
done:
    flux_plugin_arg_destroy (args);
    return rc;
}

/* Count a (key, val) tag in a list.  Return true if it is new.
 */
static bool tuple_first (struct log_tuple **list,
                         const char *key,
                         const char *val)
{
    struct log_tuple *t;

    for (t = *list; t; t = t->next) {
        if (!strcmp (t->key, key) && !strcmp (t->val, val)) {
            t->count++;
            return false;
        }
    }
    if (!(t = calloc (1, sizeof (*t)))
        || !(t->key = strdup (key))
        || !(t->val = strdup (val))) {
        if (t)
            free (t->key);
        free (t);
        return true; // not tracked, so not dropped
    }
    t->count = 1;
    t->next = *list;
    *list = t;
    return true;
}

static void tuples_destroy (struct log_tuple *t)
{
    while (t) {
        struct log_tuple *next = t->next;
        free (t->key);
        free (t->val);
        free (t);
        t = next;
    }
}

void log_summary (struct log *log)
{
    struct log_tuple *t;

    if (!log)
        return;
    for (t = log->jobtuples; t; t = t->next) {
        if (t->count > 1) {
            shell_log ("%d more node%s sent help message %s / %s",
                       t->count - 1,
                       t->count > 2 ? "s" : "",
                       t->key,
                       t->val);
        }
    }
}

static void log_request_cb (flux_t *h,
                            flux_msg_handler_t *mh,
                            const flux_msg_t *msg,
                            void *arg)
{
    struct log *log = arg;
    const char *key;
    const char *val;

    if (flux_request_unpack (msg,
                             NULL,
                             "{s:s s:s}",
                             "key", &key,
                             "val", &val) < 0) {
        if (flux_respond_error (h, msg, errno, NULL) < 0)
            shell_warn ("error responding to pmix-log request");
        return;
    }
    if (flux_respond_pack (h,
                           msg,
                           "{s:b}",
                           "write", tuple_first (&log->jobtuples, key, val)) < 0)
        shell_warn ("error responding to pmix-log request");
}

/* Remove an ask from the list.  There is at most one per tag per
 * shell, so the list is short.
 */
static void log_pending_unlink (struct log *log, struct log_pending *lp)
{
    struct log_pending **pp = &log->asks;

    while (*pp) {
        if (*pp == lp) {
            *pp = lp->next;
            break;
        }
        pp = &(*pp)->next;
    }
}

static void log_pending_destroy (struct log_pending *lp)
{
    if (lp) {
        int saved_errno = errno;
        free (lp->stream);
        free (lp->msg);
        free (lp);
        errno = saved_errno;
    }
}

static void log_response_cb (flux_future_t *f, void *arg)
{
    struct log_pending *lp = arg;
    int write;

    if (flux_rpc_get_unpack (f, "{s:b}", "write", &write) < 0) {
        shell_warn ("pmix-log request failed: %s", future_strerror (f, errno));
        write = 1; // not dropped
    }
    if (write
        && task_write (lp->lc->log->shell, lp->rank, lp->stream, lp->msg) < 0)
        shell_warn ("error writing log message from rank %d", lp->rank);
    log_pending_unlink (lp->lc->log, lp);
    log_call_release (lp->lc);
    log_pending_destroy (lp);
    flux_future_destroy (f);
}

/* Ask shell 0 whether to write a tagged message.  If the ask fails,
 * write it.
 */
static void log_ask (struct log_call *lc,
                     int rank,
                     const char *stream,
                     const char *msg,
                     const char *key,
                     const char *val)
{
    struct log *log = lc->log;
    struct log_pending *lp;
    flux_future_t *f;

    if (!(lp = calloc (1, sizeof (*lp)))
        || !(lp->stream = strdup (stream))
        || !(lp->msg = strdup (msg)))
        goto error;
    lp->lc = lc;
    lp->rank = rank;
    if (!(f = flux_shell_rpc_pack (log->shell,
                                   "pmix-log",
                                   0,
                                   0,
                                   "{s:s s:s}",
                                   "key", key,
                                   "val", val))
        || flux_future_then (f, -1, log_response_cb, lp) < 0) {
        flux_future_destroy (f);
        goto error;
    }
    lp->f = f;
    lp->next = log->asks;
    log->asks = lp;
    lc->pending++;
    return;
error:
    shell_warn ("error asking about log message from rank %d", rank);
    log_pending_destroy (lp);
    if (task_write (log->shell, rank, stream, msg) < 0)
        shell_warn ("error writing log message from rank %d", rank);
}

static void log_write (struct log_call *lc,
                       int rank,
                       const char *stream,
                       const char *msg,
                       const char *key,
                       const char *val)
{
    struct log *log = lc->log;

    if (key && val && log->dedup) {
        if (!tuple_first (&log->tuples, key, val))
            return;
        if (log->shell_rank > 0) {
            log_ask (lc, rank, stream, msg, key, val);
            return;
        }
        if (!tuple_first (&log->jobtuples, key, val))
            return;
    }
    if (task_write (log->shell, rank, stream, msg) < 0)
        shell_warn ("error writing log message from rank %d", rank);
}

static const char *channel_stream (const char *channel)
{
    if (!strcmp (channel, PMIX_LOG_STDERR))
        return "stderr";
    if (!strcmp (channel, PMIX_LOG_STDOUT))
        return "stdout";
    return NULL;
}

static bool channel_is_syslog (const char *channel)
{
    return !strcmp (channel, PMIX_LOG_SYSLOG)
        || !strcmp (channel, PMIX_LOG_LOCAL_SYSLOG)
        || !strcmp (channel, PMIX_LOG_GLOBAL_SYSLOG);
}

/* Shell log levels are syslog priorities.
 */
static void shell_write (int rank, int pri, const char *msg)
{
    size_t len = strlen (msg);

    if (len > 0 && msg[len - 1] == '\n')
        len--;
    flux_shell_log (FLUX_SHELL_PLUGIN_NAME,
                    pri,
                    __FILE__,
                    __LINE__,
                    "rank %d: %.*s",
                    rank,
                    (int)len,
                    msg);
}

/* The progress thread could not send a log request: complete it
 * with an error and say so in the shell log.
 */
static void log_error_cb (const flux_msg_t *msg, void *arg)
{
    json_t *xclient;
    json_t *xcbfunc;
    json_t *xcbdata;
    pmix_proc_t client;
    pmix_op_cbfunc_t cbfunc;
    void *cbdata;

    if (flux_msg_unpack (msg,
                         "{s:o s:o s:o}",
                         "client", &xclient,
                         "cbfunc", &xcbfunc,
                         "cbdata", &xcbdata) < 0
        || codec_pointer_decode (xcbfunc, (void **)&cbfunc) < 0
        || codec_pointer_decode (xcbdata, &cbdata) < 0) {
        shell_warn ("error unpacking interthread log_error message");
        return;
    }
    if (codec_proc_decode (xclient, &client) == 0)
        shell_warn ("log request from %s.%u failed",
                    client.nspace,
                    (unsigned)client.rank);
    else
        shell_warn ("log request failed");
    if (cbfunc)
        cbfunc (PMIX_ERROR, cbdata);
}

static void log_shell_cb (const flux_msg_t *msg, void *arg)
{
    struct log *log = arg;
    json_t *xclient;
    json_t *xdata;
    json_t *xdirs;
    json_t *xcbfunc;
    json_t *xcbdata;
    pmix_proc_t client;
    pmix_info_t *data = NULL;
    size_t ndata = 0;
    pmix_info_t *dirs = NULL;
    size_t ndirs = 0;
    struct log_call *lc;
    const char *key = NULL;
    const char *val = NULL;
    int pri = LOG_ERR;
    bool once = false;

    /* The logging process is blocked until cbfunc is called.
     * Decode the callback before anything that can fail, so that
     * a bad message cannot leave the process blocked.
     */
    if (!(lc = calloc (1, sizeof (*lc)))
        || flux_msg_unpack (msg,
                            "{s:o s:o s:o s:o s:o}",
                            "client", &xclient,
                            "data", &xdata,
                            "dirs", &xdirs,
                            "cbfunc", &xcbfunc,
                            "cbdata", &xcbdata) < 0
        || codec_pointer_decode (xcbfunc, (void **)&lc->cbfunc) < 0
        || codec_pointer_decode (xcbdata, &lc->cbdata) < 0) {
        shell_warn ("error unpacking interthread log_upcall message");
        free (lc);
        return;
    }
    lc->log = log;
    lc->rc = PMIX_SUCCESS;
    lc->pending = 1;
    if (codec_proc_decode (xclient, &client) < 0
        || codec_info_array_decode (xdata, &data, &ndata) < 0
        || codec_info_array_decode (xdirs, &dirs, &ndirs) < 0) {
        shell_warn ("error decoding interthread log_upcall message");
        lc->rc = PMIX_ERROR;
        goto done;
    }
    for (size_t i = 0; i < ndirs; i++) {
        if (!strcmp (dirs[i].key, PMIX_LOG_KEY)) {
            if (dirs[i].value.type == PMIX_STRING)
                key = dirs[i].value.data.string;
        }
        else if (!strcmp (dirs[i].key, PMIX_LOG_VAL)) {
            if (dirs[i].value.type == PMIX_STRING)
                val = dirs[i].value.data.string;
        }
        else if (!strcmp (dirs[i].key, PMIX_LOG_SYSLOG_PRI)) {
            if (dirs[i].value.type == PMIX_INT
                && dirs[i].value.data.integer >= LOG_EMERG
                && dirs[i].value.data.integer <= LOG_DEBUG)
                pri = dirs[i].value.data.integer;
        }
        else if (!strcmp (dirs[i].key, PMIX_LOG_ONCE)) {
            if (dirs[i].value.type == PMIX_BOOL)
                once = dirs[i].value.data.flag;
        }
    }
    /* PMIX_LOG_ONCE: log to the first channel that is supported.
     */
    for (size_t i = 0; i < ndata; i++) {
        const char *stream = channel_stream (data[i].key);
        bool syslog = channel_is_syslog (data[i].key);
        bool jrec = !strcmp (data[i].key, PMIX_LOG_JOB_RECORD);
        int level = jrec ? LOG_NOTICE : pri;
        bool written = false;

        if ((data[i].flags & PX_INFO_OP_COMPLETE))
            continue;
        if (!stream && !syslog && !jrec) {
            shell_debug ("log channel %s is not supported", data[i].key);
            lc->rc = PMIX_ERR_NOT_SUPPORTED;
            continue;
        }
        if (data[i].value.type != PMIX_STRING) {
            shell_warn ("log %s has wrong type=%d",
                        data[i].key,
                        data[i].value.type);
            lc->rc = PMIX_ERR_BAD_PARAM;
            continue;
        }
        if (stream) {
            log_write (lc,
                       client.rank,
                       stream,
                       data[i].value.data.string,
                       key,
                       val);
        }
        else {
            for (size_t j = 0; j < ndata; j++) {
                bool jsyslog = channel_is_syslog (data[j].key);

                if (j == i
                    || (!jsyslog && strcmp (data[j].key, PMIX_LOG_JOB_RECORD))
                    || data[j].value.type != PMIX_STRING
                    || strcmp (data[j].value.data.string,
                               data[i].value.data.string))
                    continue;
                if (j < i)
                    written = true;
                if (jsyslog)
                    level = pri;
            }
            if (!written)
                shell_write (client.rank, level, data[i].value.data.string);
        }
        if (once) {
            lc->rc = PMIX_SUCCESS;
            break;
        }
    }
done:
    log_call_release (lc);
    codec_info_array_destroy (data, ndata);
    codec_info_array_destroy (dirs, ndirs);
}

void log_server_cb (const pmix_proc_t *client,
                    const pmix_info_t data[],
                    size_t ndata,
                    const pmix_info_t directives[],
                    size_t ndirs,
                    pmix_op_cbfunc_t cbfunc,
                    void *cbdata)
{
    struct log *log = global_log_ctx;
    json_t *xclient = NULL;
    json_t *xdata = NULL;
    json_t *xdirs = NULL;
    json_t *xcbfunc = NULL;
    json_t *xcbdata = NULL;

    /* This callback returns void, so the only way to complete the
     * request is through cbfunc.  It must not be called from here:
     * openpmix before 6.1 holds a non-recursive lock across this
     * upcall that cbfunc reacquires, so an inline call deadlocks the
     * progress thread.  On failure, send the shell thread only what
     * it needs to call cbfunc and say who failed, encoded first so
     * that it survives a failure in the larger encodes.  If even
     * that cannot be sent, drop the request; its completion never
     * arrives.
     */
    if (!(xcbfunc = codec_pointer_encode (cbfunc))
        || !(xcbdata = codec_pointer_encode (cbdata))
        || !(xclient = codec_proc_encode (client))
        || !(xdata = codec_info_array_encode (data, ndata))
        || !(xdirs = codec_info_array_encode (directives, ndirs))
        || interthread_send_pack (log->it,
                                  "log_upcall",
                                  "{s:O s:O s:O s:O s:O}",
                                  "client", xclient,
                                  "data", xdata,
                                  "dirs", xdirs,
                                  "cbfunc", xcbfunc,
                                  "cbdata", xcbdata) < 0) {
        if (!xcbfunc
            || !xcbdata
            || !xclient
            || interthread_send_pack (log->it,
                                      "log_error",
                                      "{s:O s:O s:O}",
                                      "client", xclient,
                                      "cbfunc", xcbfunc,
                                      "cbdata", xcbdata) < 0)
            fprintf (stderr,
                     "pmix log request from %s.%u dropped\n",
                     client->nspace,
                     (unsigned)client->rank);
    }
    json_decref (xclient);
    json_decref (xdata);
    json_decref (xdirs);
    json_decref (xcbfunc);
    json_decref (xcbdata);
}

/* Drop the asks that shell 0 has not answered.  A task blocks in
 * PMIx_Log() until its ask completes, so the list is empty unless a
 * task was killed while blocked.  Destroying a future cancels its
 * callback, which would otherwise run against this freed log.  The
 * message is not written and cbfunc is not called: the task is gone,
 * and PMIx_server_finalize() has already run.
 */
static void log_asks_flush (struct log *log)
{
    struct log_pending *lp;

    while ((lp = log->asks)) {
        log->asks = lp->next;
        flux_future_destroy (lp->f);
        lp->lc->cbfunc = NULL;
        log_call_release (lp->lc);
        log_pending_destroy (lp);
    }
}

void log_destroy (struct log *log)
{
    if (log) {
        int saved_errno = errno;
        log_asks_flush (log);
        tuples_destroy (log->tuples);
        tuples_destroy (log->jobtuples);
        free (log);
        errno = saved_errno;
        global_log_ctx = NULL;
    }
}

struct log *log_create (flux_shell_t *shell, struct interthread *it)
{
    struct log *log;
    const char *dedup = NULL;

    if (!(log = calloc (1, sizeof (*log))))
        return NULL;
    log->shell = shell;
    log->it = it;
    log->dedup = true;
    if (flux_shell_info_unpack (shell, "{s:i}", "rank", &log->shell_rank) < 0)
        goto error;
    if (flux_shell_getopt_unpack (shell,
                                  "pmix",
                                  "{s?s}",
                                  "log-dedup", &dedup) < 0
        || (dedup
            && strcmp (dedup, "off") != 0
            && strcmp (dedup, "on") != 0)) {
        shell_log_error ("pmix.log-dedup must be \"on\" or \"off\"");
        goto error;
    }
    if (dedup && !strcmp (dedup, "off"))
        log->dedup = false;
    if (interthread_register (it, "log_upcall", log_shell_cb, log) < 0
        || interthread_register (it, "log_error", log_error_cb, log) < 0)
        goto error;
    if (log->shell_rank == 0
        && flux_shell_service_register (shell,
                                        "pmix-log",
                                        log_request_cb,
                                        log) < 0)
        goto error;
    global_log_ctx = log;
    return log;
error:
    log_destroy (log);
    return NULL;
}

// vi:ts=4 sw=4 expandtab
