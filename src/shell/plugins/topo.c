/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* topo.c - give the server a topology to share with its clients
 *
 * The shell already holds the node's hwloc XML (fetched once from the
 * resource module, so no shell pays for discovery).  Load it here and
 * hand it to PMIx_server_init() as PMIX_TOPOLOGY2 with
 * PMIX_SERVER_SHARE_TOPOLOGY.  openpmix exports it for clients as a
 * shared memory segment (PMIX_HWLOC_SHMEM_*) under the server tmpdir
 * and as XML (PMIX_HWLOC_XML_V2) in the job-level data, and a client
 * adopts one of those at PMIx_Init() rather than discovering the node.
 *
 * The XML describes this node, so the topology is loaded with
 * HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM: without it hwloc treats an XML
 * topology as foreign and makes cpu binding a no-op.  A client that
 * loads the shared XML sets the flag itself, and an adopted segment
 * carries it from the writer, so clients get a bindable topology
 * either way.
 *
 * PMIX_TOPOLOGY2 is v4.0 of the Standard.  openpmix 3.2 takes the
 * topology as an XML string in PMIX_HWLOC_XML_V2 instead, and shares
 * the segment when asked with PMIX_HWLOC_SHARE_TOPO.  Its other inputs
 * (an object in PMIX_TOPOLOGY, a path in PMIX_TOPOLOGY_FILE, or a NULL
 * XML string asking it to export) crash the server or publish keys its
 * clients cannot unpack, so the string is the one form used.  Its own
 * clients adopt nothing in PMIx_Init, but Open MPI 4.1 reads the keys
 * from job info itself, and a newer client adopts them in PMIx_Init.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <hwloc.h>
#include <flux/shell.h>
#include <pmix.h>

#include "src/common/libutil/strlcpy.h"

#include "topo.h"

struct topo {
    hwloc_topology_t hwloc;
#ifdef PMIX_TOPOLOGY2
    pmix_topology_t pmix;
#else
    char *xml;              // what 3.2 takes; kept for the server's lifetime
#endif
};

void topo_destroy (struct topo *topo)
{
    if (topo) {
        int saved_errno = errno;
        if (topo->hwloc)
            hwloc_topology_destroy (topo->hwloc);
#ifdef PMIX_TOPOLOGY2
        free (topo->pmix.source);
#else
        free (topo->xml);
#endif
        free (topo);
        errno = saved_errno;
    }
}

/* Narrow the topology to the cores R allocates to this shell, which
 * the shell reports as a core id list in its rank info.  This is the
 * job's slice of the node whether or not anything bound the shell:
 * the shell's own cpu binding is a side effect of the affinity
 * builtin, which may be off or replaced by another plugin, and on a
 * shared node an unbound shell can run anywhere.  The affinity
 * builtin starts from the same list.
 *
 * A core R names that the topology lacks is not fatal: warn and share
 * the node, as the affinity builtin does when it cannot bind.
 */
static int restrict_to_cores (hwloc_topology_t topo, const char *cores)
{
    hwloc_bitmap_t coreset = NULL;
    hwloc_bitmap_t cpuset = NULL;
    int depth;
    int rc = -1;

    if (!(coreset = hwloc_bitmap_alloc ()) || !(cpuset = hwloc_bitmap_alloc ()))
        goto done;
    if (hwloc_bitmap_list_sscanf (coreset, cores) < 0) {
        errno = EINVAL;
        goto done;
    }
    depth = hwloc_get_type_depth (topo, HWLOC_OBJ_CORE);
    if (depth < 0) {
        errno = EINVAL;
        goto done;
    }
    for (int i = hwloc_bitmap_first (coreset);
         i >= 0;
         i = hwloc_bitmap_next (coreset, i)) {
        hwloc_obj_t core = hwloc_get_obj_by_depth (topo, depth, i);
        if (!core || !core->cpuset) {
            errno = ENOENT;
            goto done;
        }
        hwloc_bitmap_or (cpuset, cpuset, core->cpuset);
    }
    if (hwloc_topology_restrict (topo, cpuset, 0) < 0)
        goto done;
    rc = 0;
done:
    hwloc_bitmap_free (coreset);
    hwloc_bitmap_free (cpuset);
    return rc;
}

struct topo *topo_create (flux_shell_t *shell, bool do_restrict)
{
    struct topo *topo;
    const char *xml;

    if (!(topo = calloc (1, sizeof (*topo))))
        return NULL;
    if (flux_shell_get_hwloc_xml (shell, &xml) < 0)
        goto error;
    if (hwloc_topology_init (&topo->hwloc) < 0)
        goto error;
    if (hwloc_topology_set_io_types_filter (topo->hwloc,
                                            HWLOC_TYPE_FILTER_KEEP_IMPORTANT) < 0
        || hwloc_topology_set_flags (topo->hwloc,
                                     HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM) < 0)
        goto error;
    /* Some hwloc/libxml combinations reject a length that includes
     * the NUL, so retry without it, as flux-core's rhwloc does.
     */
    if (hwloc_topology_set_xmlbuffer (topo->hwloc, xml, strlen (xml) + 1) < 0
        && hwloc_topology_set_xmlbuffer (topo->hwloc, xml, strlen (xml)) < 0)
        goto error;
    if (hwloc_topology_load (topo->hwloc) < 0)
        goto error;
    if (do_restrict) {
        const char *cores;
        if (flux_shell_rank_info_unpack (shell,
                                         -1,
                                         "{s:{s:s}}",
                                         "resources",
                                           "cores", &cores) < 0)
            goto error;
        if (restrict_to_cores (topo->hwloc, cores) < 0)
            shell_warn ("cannot restrict topology to cores %s: %s",
                        cores,
                        strerror (errno));
    }
#ifdef PMIX_TOPOLOGY2
    /* openpmix labels a discovered topology "hwloc:<version>" and
     * MPICH checks for the "hwloc" prefix, so match that form.  Name
     * the hwloc that built the object, which is the one the shell
     * loaded, not the one this plugin was compiled against.
     */
    unsigned v = hwloc_get_api_version ();
    if (asprintf (&topo->pmix.source,
                  "hwloc:%u.%u.%u",
                  v >> 16,
                  (v >> 8) & 0xff,
                  v & 0xff) < 0) {
        topo->pmix.source = NULL;
        goto error;
    }
    topo->pmix.topology = topo->hwloc;
#else
    char *buf;
    int len;
    if (hwloc_topology_export_xmlbuffer (topo->hwloc, &buf, &len, 0) < 0)
        goto error;
    topo->xml = strdup (buf);
    hwloc_free_xmlbuffer (topo->hwloc, buf);
    if (!topo->xml)
        goto error;
#endif
    return topo;
error:
    topo_destroy (topo);
    return NULL;
}

int topo_server_info (struct topo *topo, pmix_info_t *info, size_t avail)
{
    int n = 0;

    if (avail < 2) {
        errno = ENOSPC;
        return -1;
    }
#ifdef PMIX_TOPOLOGY2
    strlcpy (info[n].key, PMIX_TOPOLOGY2, sizeof (info[n].key));
    info[n].value.type = PMIX_TOPO;
    info[n].value.data.topo = &topo->pmix;
    info[n].flags = 0;
    n++;
    strlcpy (info[n].key, PMIX_SERVER_SHARE_TOPOLOGY, sizeof (info[n].key));
    info[n].value.type = PMIX_BOOL;
    info[n].value.data.flag = true;
    info[n].flags = 0;
    n++;
#else
    strlcpy (info[n].key, PMIX_HWLOC_XML_V2, sizeof (info[n].key));
    info[n].value.type = PMIX_STRING;
    info[n].value.data.string = topo->xml;
    info[n].flags = 0;
    n++;
    strlcpy (info[n].key, PMIX_HWLOC_SHARE_TOPO, sizeof (info[n].key));
    info[n].value.type = PMIX_BOOL;
    info[n].value.data.flag = true;
    info[n].flags = 0;
    n++;
#endif
    return n;
}

// vi:ts=4 sw=4 expandtab
