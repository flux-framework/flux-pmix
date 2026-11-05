/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* query.c - answer PMIx_Query_info() for what the server cannot
 *
 * The server answers what it can from its own data and hands the
 * rest here (standard 17.3.18).  The answer is an info array with
 * one entry per key found, and the status says whether every key
 * was found, some were, or none (standard 7.1.2).  Each key is
 * looked up on its own, so a query of several keys is answered
 * for the ones known.
 *
 * No key is answered yet: this is the plumbing from the server
 * thread to the shell thread and back, as dmodex.c has, so that
 * a key can be answered from the shell's data when there is one
 * to answer.  Until then every query is NOT_FOUND, which the
 * standard says is the answer for a key the host does not have.
 */
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <jansson.h>
#include <flux/core.h>
#include <flux/shell.h>
#include <pmix.h>
#include <pmix_server.h>

#include "codec.h"
#include "infovec.h"
#include "interthread.h"
#include "query.h"

/* The standard's status for a query partly answered is 4.0 and later;
 * 3.2 has the older spelling of the same value.
 */
#ifndef PMIX_ERR_PARTIAL_SUCCESS
#define PMIX_ERR_PARTIAL_SUCCESS PMIX_QUERY_PARTIAL_SUCCESS
#endif

struct query {
    flux_shell_t *shell;
    struct interthread *it;
};

struct query_call {
    pmix_proc_t proc;
    pmix_query_t *queries;
    size_t nqueries;
    pmix_info_cbfunc_t cbfunc;
    void *cbdata;
    struct infovec *results;
};

/* This is for the benefit of server callbacks that don't have
 * a way to be passed a user-supplied opaque pointer.
 */
static struct query *global_query_ctx;

/* The server borrows the results until it has packed the reply,
 * then calls this to let them go.
 */
static void query_call_release (void *arg)
{
    struct query_call *qc = arg;
    if (qc) {
        int saved_errno = errno;
        codec_query_array_destroy (qc->queries, qc->nqueries);
        infovec_destroy (qc->results);
        free (qc);
        errno = saved_errno;
    }
}

static struct query_call *query_call_create (void)
{
    struct query_call *qc;

    if (!(qc = calloc (1, sizeof (*qc)))
        || !(qc->results = infovec_create ())) {
        query_call_release (qc);
        return NULL;
    }
    return qc;
}

/* Look up one key.  Return 0 if answered into qc->results, -1 if not.
 */
static int query_key (struct query *q,
                      struct query_call *qc,
                      const pmix_query_t *query,
                      const char *key)
{
    return -1;
}

static void query_shell_cb (const flux_msg_t *msg, void *arg)
{
    struct query *q = arg;
    json_t *xproc;
    json_t *xqueries;
    json_t *xcbfunc;
    json_t *xcbdata;
    struct query_call *qc;
    size_t nkeys = 0;
    size_t nfound = 0;
    int rc;

    /* The requesting process is waiting on this answer, and the only
     * thing that will give it one is cbfunc: the server callback has
     * already returned PMIX_SUCCESS, so openpmix will not release
     * the request itself.  Decode the callback before anything that
     * can fail, so a bad message cannot leave the request outstanding.
     */
    if (!(qc = query_call_create ())
        || flux_msg_unpack (msg,
                            "{s:o s:o s:o s:o}",
                            "proc", &xproc,
                            "queries", &xqueries,
                            "cbfunc", &xcbfunc,
                            "cbdata", &xcbdata) < 0
        || codec_pointer_decode (xcbfunc, (void **)&qc->cbfunc) < 0
        || codec_pointer_decode (xcbdata, &qc->cbdata) < 0) {
        shell_warn ("error unpacking query_upcall interthread message");
        query_call_release (qc);
        return;
    }
    if (codec_proc_decode (xproc, &qc->proc) < 0
        || codec_query_array_decode (xqueries,
                                     &qc->queries,
                                     &qc->nqueries) < 0) {
        shell_warn ("error decoding query_upcall interthread message");
        rc = PMIX_ERROR;
        goto done;
    }
    for (size_t i = 0; i < qc->nqueries; i++) {
        pmix_query_t *query = &qc->queries[i];
        for (size_t j = 0; query->keys && query->keys[j]; j++) {
            nkeys++;
            if (query_key (q, qc, query, query->keys[j]) == 0)
                nfound++;
            else
                shell_debug ("query_upcall from %s.%d: %s not found",
                             qc->proc.nspace,
                             qc->proc.rank,
                             query->keys[j]);
        }
    }
    /* Standard 7.1.2: SUCCESS when all keys were found, PARTIAL_SUCCESS
     * when some were, NOT_FOUND when none.  The server packs the
     * array as is; a 5.0.7 or later client takes the partial one.
     */
    if (nfound == nkeys)
        rc = PMIX_SUCCESS;
    else if (nfound > 0)
        rc = PMIX_ERR_PARTIAL_SUCCESS;
    else
        rc = PMIX_ERR_NOT_FOUND;
done:
    if (qc->cbfunc) {
        qc->cbfunc (rc,
                    infovec_info (qc->results),
                    infovec_count (qc->results),
                    qc->cbdata,
                    query_call_release,
                    qc);
    }
    else
        query_call_release (qc);
}

int query_server_cb (pmix_proc_t *proc,
                     pmix_query_t *queries,
                     size_t nqueries,
                     pmix_info_cbfunc_t cbfunc,
                     void *cbdata)
{
    struct query *q = global_query_ctx;
    json_t *xproc  = NULL;
    json_t *xqueries = NULL;
    json_t *xcbfunc = NULL;
    json_t *xcbdata = NULL;
    int rc = PMIX_SUCCESS;

    if (!(xproc = codec_proc_encode (proc))
        || !(xqueries = codec_query_array_encode (queries, nqueries))
        || !(xcbfunc = codec_pointer_encode (cbfunc))
        || !(xcbdata = codec_pointer_encode (cbdata))
        || interthread_send_pack (q->it,
                                  "query_upcall",
                                  "{s:O s:O s:O s:O}",
                                  "proc", xproc,
                                  "queries", xqueries,
                                  "cbfunc", xcbfunc,
                                  "cbdata", xcbdata) < 0) {
        fprintf (stderr, "error sending query_upcall interthread message\n");
        rc = PMIX_ERROR;
    }
    json_decref (xproc);
    json_decref (xqueries);
    json_decref (xcbfunc);
    json_decref (xcbdata);
    return rc;
}

void query_destroy (struct query *q)
{
    if (q) {
        int saved_errno = errno;
        free (q);
        errno = saved_errno;
        global_query_ctx = NULL;
    }
}

struct query *query_create (flux_shell_t *shell, struct interthread *it)
{
    struct query *q;

    if (!(q = calloc (1, sizeof (*q))))
        return NULL;
    q->shell = shell;
    q->it = it;
    if (interthread_register (it, "query_upcall", query_shell_cb, q) < 0)
        goto error;
    global_query_ctx = q;
    return q;
error:
    query_destroy (q);
    return NULL;
}

// vi:ts=4 sw=4 expandtab
