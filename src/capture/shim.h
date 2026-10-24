/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PMIX_CAPTURE_H
#define _PMIX_CAPTURE_H

/* shim.h - the record the capture shim writes to the shell
 *
 * The shim is interposed on a client's socket calls and forwards what
 * it sees to the shell over one channel in raw, unframed form.
 *
 * Every field is little-endian, which allows the data to be parsed on a
 * different system, portably.
 */

#include <stdint.h>

/* Set in the client's environment to the fd of the channel.
 */
#define PMIX_CAPTURE_ENV "FLUX_PMIX_CAPTURE_FD"

#define PMIX_CAPTURE_MAGIC 0x50585452     /* "PXTR" */

enum pmix_capture_op {
    PMIX_CAPTURE_OP_READ = 0,     // client received these bytes
    PMIX_CAPTURE_OP_WRITE = 1,    // client sent these bytes
    PMIX_CAPTURE_OP_OPEN = 2,     // connection established, no data
    PMIX_CAPTURE_OP_CLOSE = 3,    // connection closed, no data
};

/* A record, followed by 'len' bytes of payload.
 *
 * 'conn' distinguishes concurrent connections.  A process may hold
 * more than one, since a forked child that calls PMIx_Init()
 * connects again, and their streams must not be reassembled into
 * each other.  It is the fd, which is unique while open; a record
 * with op CLOSE ends its life, and a later connection may reuse the
 * number.
 *
 * 'pid' distinguishes processes, since LD_PRELOAD is inherited across
 * fork and the child shares the channel.
 */
struct pmix_capture_hdr {
    uint32_t magic;
    uint32_t pid;
    uint32_t conn;
    uint32_t op;
    uint64_t usec;      // CLOCK_REALTIME microseconds
    uint32_t len;
    uint32_t pad;       // zero
};

#define PMIX_CAPTURE_HDR_SIZE 32

#endif /* !_PMIX_CAPTURE_H */

// vi:ts=4 sw=4 expandtab
