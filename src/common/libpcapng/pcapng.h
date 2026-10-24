/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* pcapng.h - write pcapng capture files
 *
 * This is the capture format of Wireshark and tcpdump, described in
 * draft-ietf-opsawg-pcapng.  A file is a sequence of blocks: a
 * section header, an interface description for each capture source,
 * and an enhanced packet block for each packet, which names its
 * interface and carries a timestamp and the packet bytes.
 *
 * Only the blocks needed for a capture of one process's connections
 * are written.  Each connection is an interface, so that Wireshark
 * can filter on it and follow its stream, and each read or write is
 * a packet.
 *
 * Packets are written with the "exported PDU" link type, which is for
 * bytes captured above the network layer.  Each packet begins with a
 * list of tags, among them the name of the Wireshark dissector that
 * understands the payload and the direction the bytes travelled, and
 * a reader with no dissector of that name still shows the packet.
 *
 * Timestamps are in microseconds since the epoch, which is the
 * default resolution of the format.
 *
 * There is no reader: Wireshark's own tools (tshark, capinfos,
 * mergecap) read the files, and the tests use those.
 */

#ifndef _PCAPNG_H
#define _PCAPNG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

enum pcapng_dir {
    PCAPNG_DIR_UNKNOWN = -1,
    PCAPNG_DIR_SENT = 0,        // by the process that owns the interface
    PCAPNG_DIR_RECV = 1,
};

struct pcapng_writer;

/* Create a file at 'path', truncating any existing one, and write
 * the section header.  'application' names the writer in the header,
 * and may be NULL.
 */
struct pcapng_writer *pcapng_writer_create (const char *path,
                                            const char *application);

/* As above, but on an open stream, which the writer does not close.
 */
struct pcapng_writer *pcapng_writer_fcreate (FILE *f,
                                             const char *application);

/* Add an interface and return its id, which packets name.  'name'
 * and 'description' show in Wireshark and may be NULL.  Ids start
 * at zero and count up in the order added.
 */
int pcapng_writer_interface (struct pcapng_writer *w,
                             const char *name,
                             const char *description);

/* Write a packet of 'len' bytes on interface 'ifid'.  'dissector' is
 * the name of the Wireshark dissector for the payload, and 'info' is
 * text for the Info column; either may be NULL.  Zero-length packets
 * are allowed, so that an event with no data may still be recorded.
 */
int pcapng_writer_packet (struct pcapng_writer *w,
                          int ifid,
                          uint64_t usec,
                          enum pcapng_dir dir,
                          const char *dissector,
                          const char *info,
                          const void *data,
                          size_t len);

/* Write packets in timestamp order rather than the order given.  The
 * packets are held in memory until the writer is destroyed, so this
 * suits a capture that fits there, and it must be set before the
 * first packet.  A reader such as mergecap takes each file to be in
 * order already, so a writer fed from more than one source wants this.
 */
void pcapng_writer_set_sorted (struct pcapng_writer *w, bool sorted);

/* Flush and close.  Returns -1 with errno set if the final write
 * fails, so a caller that cares can find out.
 */
int pcapng_writer_destroy (struct pcapng_writer *w);

#endif /* !_PCAPNG_H */

// vi:ts=4 sw=4 expandtab
