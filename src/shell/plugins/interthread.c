/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* interthread.c - message channel from pmix server thread -> shell thread
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <jansson.h>
#include <flux/core.h>

#include "src/common/libutil/strlcpy.h"

#include "interthread.h"

#define MAX_HANDLERS 32

struct handler {
    char topic[32];
    interthread_msg_handler_f cb;
    void *arg;
};

struct interthread {
    flux_t *send;
    flux_t *recv;
    flux_watcher_t *w;
    struct handler handlers[MAX_HANDLERS];
    int handler_count;
    int verbose;
};

int interthread_register (struct interthread *it,
                          const char *topic,
                          interthread_msg_handler_f cb,
                          void *arg)
{
    struct handler *handler;

    if (it->handler_count == MAX_HANDLERS) {
        errno = ENOSPC;
        return -1;
    }
    handler = &it->handlers[it->handler_count++];
    strlcpy (handler->topic, topic, sizeof (handler->topic));
    handler->cb = cb;
    handler->arg = arg;
    return 0;
}

int interthread_send_pack (struct interthread *it,
                           const char *name,
                           const char *fmt, ...)
{
    flux_msg_t *msg;
    va_list ap;
    int rc;

    if (!(msg = flux_msg_create (FLUX_MSGTYPE_REQUEST))
        || flux_msg_set_topic (msg, name) < 0)
        goto error;

    va_start (ap, fmt);
    rc = flux_msg_vpack (msg, fmt, ap);
    va_end (ap);
    if (rc < 0)
        goto error;

    if (flux_send_new (it->send, &msg, 0) < 0)
        goto error;

    return 0;
error:
    flux_msg_decref (msg);
    return -1;
}

static void interthread_recv (flux_reactor_t *r,
                              flux_watcher_t *w,
                              int revents,
                              void *arg)
{
    struct interthread *it = arg;
    flux_msg_t *msg;
    const char *topic;
    int i;

    if (!(msg = flux_recv (it->recv, FLUX_MATCH_ANY, 0)))
        return;
    if (flux_msg_get_topic (msg, &topic) < 0) {
        shell_warn ("interthread receive decode error - message dropped");
        flux_msg_decref (msg);
        return;
    }
    if (it->verbose > 1) {
        const char *payload;
        if (flux_msg_get_payload (msg, (const void **)&payload, NULL) == 0)
            shell_trace ("pmix server %s %s", topic, payload);
    }
    for (i = 0; i < it->handler_count; i++) {
        if (!strcmp (topic, it->handlers[i].topic))
            break;
    }
    if (i < it->handler_count)
        it->handlers[i].cb (msg, it->handlers[i].arg);
    else
        shell_warn ("unhandled interthread topic %s", topic);
    flux_msg_decref (msg);
}

void interthread_destroy (struct interthread *it)
{
    if (it) {
        int saved_errno = errno;
        flux_watcher_destroy (it->w);
        flux_close (it->send);
        flux_close (it->recv);
        free (it);
        errno = saved_errno;
    }
}

struct interthread *interthread_create (flux_shell_t *shell)
{
    flux_t *h = flux_shell_get_flux (shell);
    struct interthread *it;

    if (!(it = calloc (1, sizeof (*it))))
        return NULL;
    if (!(it->send = flux_open ("interthread://pmix", 0))
        || !(it->recv = flux_open ("interthread://pmix", 0))
        || !(it->w = flux_handle_watcher_create (flux_get_reactor (h),
                                                 it->recv,
                                                 FLUX_POLLIN,
                                                 interthread_recv,
                                                 it)))
        goto error;
    flux_watcher_start (it->w);
    (void)flux_shell_getopt_unpack (shell, "verbose", "i", &it->verbose);
    return it;
error:
    interthread_destroy (it);
    return NULL;
}

// vi:ts=4 sw=4 expandtab
