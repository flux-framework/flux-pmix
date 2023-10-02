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
#include <pthread.h>
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
    struct flux_msglist *queue;
    pthread_mutex_t lock;
    flux_watcher_t *w;
    struct handler handlers[MAX_HANDLERS];
    int handler_count;
    bool trace_flag;
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

    pthread_mutex_lock (&it->lock);
    rc = flux_msglist_append (it->queue, msg);
    pthread_mutex_unlock (&it->lock);
    if (rc < 0)
        goto error;

    flux_msg_decref (msg);
    return 0;
error:
    flux_msg_decref (msg);
    return -1;
}

const flux_msg_t *pop_queue_locked (struct interthread *it)
{
    const flux_msg_t *msg;

    pthread_mutex_lock (&it->lock);
    msg = flux_msglist_pop (it->queue);
    pthread_mutex_unlock (&it->lock);

    return msg;
}

static void interthread_recv (flux_reactor_t *r,
                              flux_watcher_t *w,
                              int revents,
                              void *arg)
{
    struct interthread *it = arg;
    const flux_msg_t *msg;
    const char *topic;
    int i;

    /* flux_msglist_pollfd() is edge triggered so when the reactor watcher
     * is triggered, all available messages should be consumed.
     */
    while ((msg = pop_queue_locked (it))) {
        if (flux_msg_get_topic (msg, &topic) < 0) {
            shell_warn ("interthread receive decode error - message dropped");
            flux_msg_decref (msg);
            continue;
        }
        if (it->trace_flag) {
            const char *payload;
            int size;
            if (flux_msg_get_payload (msg, (const void **)&payload, &size) == 0
                && size > 0)
                shell_trace ("pmix server %s %.*s", topic, size - 1, payload);
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
}

void interthread_destroy (struct interthread *it)
{
    if (it) {
        int saved_errno = errno;
        flux_watcher_destroy (it->w);
        flux_msglist_destroy (it->queue);
        pthread_mutex_destroy (&it->lock);
        free (it);
        errno = saved_errno;
    }
}

struct interthread *interthread_create (flux_shell_t *shell)
{
    flux_t *h = flux_shell_get_flux (shell);
    struct interthread *it;
    int fd;

    if (!(it = calloc (1, sizeof (*it))))
        return NULL;
    pthread_mutex_init (&it->lock, NULL);
    if (!(it->queue = flux_msglist_create ()))
        goto error;
    if ((fd = flux_msglist_pollfd (it->queue)) < 0)
        goto error;
    if (!(it->w = flux_fd_watcher_create (flux_get_reactor (h),
                                          fd,
                                          FLUX_POLLIN,
                                          interthread_recv,
                                          it)))
        goto error;
    flux_watcher_start (it->w);
    it->trace_flag = 1; // temporarily force this on
    return it;
error:
    interthread_destroy (it);
    return NULL;
}

// vi:ts=4 sw=4 expandtab
