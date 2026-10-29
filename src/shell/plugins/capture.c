/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* capture.c - capture a client's socket traffic to a file
 *
 * The shim (src/capture) copies each client's socket traffic to
 * a channel.  This reads the channel, frames the shim's records, and
 * writes each one to a pcapng file, where each connection is an
 * interface and each read or write is a packet.  Nothing is decoded
 * here: the capture holds the bytes, and a dissector reads them.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <flux/core.h>
#include <flux/shell.h>

#include "src/common/libpcapng/pcapng.h"
#include "src/capture/shim.h"

#include "capture.h"

/* How much of a task's channel may be buffered while waiting for the
 * rest of a record.  A record holds one read() or write(), and a
 * client's are small; the job-info reply of a large job is the
 * biggest and is well under this.
 */
#define MAX_BUFFER (4 * 1024 * 1024)

/* A connection, identified by (pid, fd): the fd alone is not unique,
 * because LD_PRELOAD is inherited across fork and a child has its
 * own descriptor table.
 */
struct conn {
    uint32_t pid;
    uint32_t fd;
    int rank;
    int ifid;                   // interface in the capture
    struct conn *next;
};

/* Reassembly of a task's channel.  A record is a fixed header and a
 * payload, and the channel is a byte stream, so a record can arrive
 * split across reads.
 */
struct chan {
    flux_shell_task_t *task;
    int rank;
    uint8_t *buf;
    size_t len;
    size_t size;
    bool stopped;               // records are no longer framed
    struct chan *next;
};

struct capture {
    flux_shell_t *shell;
    struct conn *conns;
    struct chan *chans;
    struct pcapng_writer *writer;
    char *path;
    int bufsize;                // channel read buffer, or 0 for default
};

static struct conn *conn_lookup (struct capture *cap,
                                 uint32_t pid,
                                 uint32_t fd,
                                 int rank)
{
    struct conn *c;
    char name[64];

    for (c = cap->conns; c != NULL; c = c->next) {
        if (c->pid == pid && c->fd == fd)
            return c;
    }
    if (!(c = calloc (1, sizeof (*c))))
        return NULL;
    c->pid = pid;
    c->fd = fd;
    c->rank = rank;
    snprintf (name, sizeof (name), "rank%d-pid%u-fd%u", rank, pid, fd);
    if ((c->ifid = pcapng_writer_interface (cap->writer, name, NULL)) < 0) {
        free (c);
        return NULL;
    }
    c->next = cap->conns;
    cap->conns = c;
    return c;
}

/* A connection is forgotten when it closes, so that a later one on
 * the same (pid, fd) is a new interface.
 */
static void conn_remove (struct capture *cap, struct conn *conn)
{
    struct conn **cp = &cap->conns;

    while (*cp) {
        if (*cp == conn) {
            *cp = conn->next;
            free (conn);
            return;
        }
        cp = &(*cp)->next;
    }
}

static struct chan *chan_lookup (struct capture *cap, flux_shell_task_t *task)
{
    struct chan *ch;

    for (ch = cap->chans; ch != NULL; ch = ch->next) {
        if (ch->task == task)
            return ch;
    }
    if (!(ch = calloc (1, sizeof (*ch))))
        return NULL;
    ch->task = task;
    ch->rank = -1;
    (void)flux_shell_task_info_unpack (task, "{s:i}", "rank", &ch->rank);
    ch->next = cap->chans;
    cap->chans = ch;
    return ch;
}

static int chan_append (struct chan *ch, const void *data, size_t len)
{
    if (ch->len + len > MAX_BUFFER) {
        errno = EOVERFLOW;
        return -1;
    }
    if (ch->len + len > ch->size) {
        size_t size = ch->size ? ch->size * 2 : 4096;
        while (size < ch->len + len)
            size *= 2;
        uint8_t *buf = realloc (ch->buf, size);
        if (!buf)
            return -1;
        ch->buf = buf;
        ch->size = size;
    }
    memcpy (ch->buf + ch->len, data, len);
    ch->len += len;
    return 0;
}

static void chan_consume (struct chan *ch, size_t len)
{
    if (len >= ch->len)
        ch->len = 0;
    else {
        memmove (ch->buf, ch->buf + len, ch->len - len);
        ch->len -= len;
    }
}

static void chan_reset (struct chan *ch)
{
    free (ch->buf);
    ch->buf = NULL;
    ch->len = ch->size = 0;
}

/* The shim's header fields are little-endian (shim.h).
 */
static uint32_t get_u32 (const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t get_u64 (const uint8_t *p)
{
    return (uint64_t)get_u32 (p) | ((uint64_t)get_u32 (p + 4) << 32);
}

/* Capture one record.  The connection is an interface, added on first
 * sight, and open and close are empty packets on it so that the file
 * shows the connection's lifetime.  A write by the client is a sent
 * packet and a read is a received one.
 */
static int handle_record (struct capture *cap,
                          const uint8_t *hdr,
                          const void *data,
                          size_t len,
                          int rank)
{
    uint32_t pid = get_u32 (hdr + 4);
    uint32_t fd = get_u32 (hdr + 8);
    uint32_t op = get_u32 (hdr + 12);
    uint64_t usec = get_u64 (hdr + 16);
    struct conn *c;
    enum pcapng_dir dir;
    const char *what;
    char info[64];

    switch (op) {
        case PMIX_CAPTURE_OP_WRITE:
            dir = PCAPNG_DIR_SENT;
            what = "write";
            break;
        case PMIX_CAPTURE_OP_READ:
            dir = PCAPNG_DIR_RECV;
            what = "read";
            break;
        case PMIX_CAPTURE_OP_OPEN:
            dir = PCAPNG_DIR_UNKNOWN;
            what = "open";
            break;
        case PMIX_CAPTURE_OP_CLOSE:
            dir = PCAPNG_DIR_UNKNOWN;
            what = "close";
            break;
        default:
            return 0;               // a record from a newer shim
    }
    if (!(c = conn_lookup (cap, pid, fd, rank)))
        return -1;
    snprintf (info, sizeof (info), "rank %d %s", rank, what);
    if (pcapng_writer_packet (cap->writer,
                              c->ifid,
                              usec,
                              dir,
                              "pmix",
                              info,
                              data,
                              len) < 0)
        return -1;
    if (op == PMIX_CAPTURE_OP_CLOSE)
        conn_remove (cap, c);
    return 0;
}

/* Data arrived on a task's capture channel.  Take as many whole records
 * as the buffer holds; anything left is the front of a record whose
 * rest has not arrived, and stays for the next read.
 *
 * The read must happen on every call, whatever is done with the data:
 * the shim keeps writing, and an unread channel fills and blocks the
 * task on its next write.  So after the capture stops, for this task
 * or for the file, the data is read and dropped.
 */
static void capture_chan_cb (flux_shell_task_t *task,
                           const char *stream,
                           void *arg)
{
    struct capture *cap = arg;
    flux_subprocess_t *sp;
    struct chan *ch;
    const char *buf;
    int len;

    if (!(sp = flux_shell_task_subprocess (task)))
        return;
    if ((len = flux_subprocess_read (sp, stream, &buf)) <= 0)
        return;
    if (!cap->writer)
        return;
    if (!(ch = chan_lookup (cap, task))) {
        shell_warn ("out of memory tracing rank");
        return;
    }
    if (ch->stopped)
        return;
    if (chan_append (ch, buf, len) < 0) {
        /* A record longer than any the shim sends, or the shim is not
         * writing records at all.  Nothing after this point can be
         * framed.
         */
        shell_warn ("%d: capture channel overflow, capture stopped", ch->rank);
        chan_reset (ch);
        ch->stopped = true;
        return;
    }
    while (ch->len >= PMIX_CAPTURE_HDR_SIZE) {
        const uint8_t *h = ch->buf;
        uint32_t rlen = get_u32 (h + 24);

        if (get_u32 (h) != PMIX_CAPTURE_MAGIC) {
            shell_warn ("%d: capture channel out of sync, capture stopped",
                        ch->rank);
            chan_reset (ch);
            ch->stopped = true;
            return;
        }
        if (ch->len < PMIX_CAPTURE_HDR_SIZE + rlen)
            return;                 // wait for the rest
        if (handle_record (cap,
                           h,
                           h + PMIX_CAPTURE_HDR_SIZE,
                           rlen,
                           ch->rank) < 0) {
            shell_warn ("%s: %s, capture stopped",
                        cap->path,
                        strerror (errno));
            pcapng_writer_destroy (cap->writer);
            cap->writer = NULL;
            return;
        }
        chan_consume (ch, PMIX_CAPTURE_HDR_SIZE + rlen);
    }
}

/* The shim lives in libexecdir once installed, but the test suite
 * runs the plugin out of the build tree, where nothing is installed.
 * As in flux-core, treat the plugin as in-tree when it was loaded
 * from under the build directory, and use the build-tree shim then.
 */
static const char *shim_path (void)
{
    Dl_info info;
    char *builddir;
    const char *path = PMIX_CAPTURE_SHIM;

    if (!dladdr ((void *)shim_path, &info) || !info.dli_fname)
        return path;
    if (!(builddir = realpath (ABS_TOP_BUILDDIR, NULL)))
        return path;
    size_t len = strlen (builddir);
    if (strncmp (info.dli_fname, builddir, len) == 0
        && info.dli_fname[len] == '/')
        path = PMIX_CAPTURE_SHIM_INTREE;
    free (builddir);
    return path;
}

int capture_task_init (struct capture *cap, flux_shell_task_t *task)
{
    flux_cmd_t *cmd;
    const char *preload;
    const char *shim = shim_path ();

    if (!(cmd = flux_shell_task_cmd (task)))
        return -1;
    if (flux_cmd_add_channel (cmd, PMIX_CAPTURE_ENV) < 0)
        return -1;
    /* The records are binary, so the callback must run on any data,
     * not on a newline.
     */
    if (flux_cmd_setopt (cmd, PMIX_CAPTURE_ENV "_LINE_BUFFER", "false") < 0)
        return -1;
    /* The channel's read buffer defaults to 4 MiB, so a record is
     * nearly always read whole.  A test can shrink it to force
     * records to arrive in pieces and so exercise reassembly, which
     * otherwise depends on a scheduling race that rarely happens.
     */
    if (cap->bufsize > 0) {
        char val[16];
        snprintf (val, sizeof (val), "%d", cap->bufsize);
        if (flux_cmd_setopt (cmd, PMIX_CAPTURE_ENV "_BUFSIZE", val) < 0)
            return -1;
    }
    if (flux_shell_task_channel_subscribe (task,
                                           PMIX_CAPTURE_ENV,
                                           capture_chan_cb,
                                           cap) < 0)
        return -1;
    /* Prepend rather than replace: a job may already be using
     * LD_PRELOAD for its own purposes.
     */
    if ((preload = flux_cmd_getenv (cmd, "LD_PRELOAD"))) {
        if (flux_cmd_setenvf (cmd,
                              1,
                              "LD_PRELOAD",
                              "%s:%s",
                              shim,
                              preload) < 0)
            return -1;
    }
    else if (flux_cmd_setenvf (cmd, 1, "LD_PRELOAD", "%s", shim) < 0)
        return -1;
    return 0;
}

void capture_destroy (struct capture *cap)
{
    if (cap) {
        int saved_errno = errno;
        struct conn *c = cap->conns;
        while (c) {
            struct conn *next = c->next;
            free (c);
            c = next;
        }
        struct chan *ch = cap->chans;
        while (ch) {
            struct chan *next = ch->next;
            chan_reset (ch);
            free (ch);
            ch = next;
        }
        if (cap->writer && pcapng_writer_destroy (cap->writer) < 0)
            shell_warn ("%s: %s", cap->path, strerror (errno));
        free (cap->path);
        free (cap);
        errno = saved_errno;
    }
}

/* -o pmix.wirecapture=PATH writes every connection's bytes to a
 * pcapng file.  PATH is a mustache template, so one file per shell
 * is "{{id}}-{{node.id}}.pcapng"; a job of more than one shell needs
 * that, since each shell writes its own.
 */
struct capture *capture_create (flux_shell_t *shell)
{
    struct capture *cap;
    const char *template = NULL;

    int bufsize = 0;

    if (flux_shell_getopt_unpack (shell,
                                  "pmix",
                                  "{s?s s?i}",
                                  "wirecapture", &template,
                                  "wirecapture-bufsize", &bufsize) < 0) {
        shell_log_error ("pmix.wirecapture must be a string and"
                         " pmix.wirecapture-bufsize an integer");
        return NULL;
    }
    if (!template) {
        errno = 0;
        return NULL;
    }
    if (!(cap = calloc (1, sizeof (*cap))))
        return NULL;
    cap->shell = shell;
    cap->bufsize = bufsize;
    if (!(cap->path = flux_shell_mustache_render (shell, template)))
        goto error;
    if (!(cap->writer = pcapng_writer_create (cap->path,
                                                 "flux-pmix"))) {
        shell_log_errno ("%s", cap->path);
        goto error;
    }
    /* Each task has its own channel and the reactor reads whichever
     * is ready, so records arrive out of time order across tasks.
     * Sort on close, so that the file merges with others as is.
     */
    pcapng_writer_set_sorted (cap->writer, true);
    return cap;
error:
    capture_destroy (cap);
    return NULL;
}

// vi:ts=4 sw=4 expandtab
