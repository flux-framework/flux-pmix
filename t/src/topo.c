/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* topo.c - report the topology a client gets from its PMIx server
 *
 * Prints one line per rank in key=value form:
 *   source     the pmix_topology_t source string (e.g. hwloc:2.12.0)
 *   ncores     cores in the topology
 *   npus       PUs in the topology
 *   cpuset     the topology cpuset as a list
 *   thissystem whether hwloc will bind against it
 *   via        how the client library obtained it: shmem (adopted the
 *              server's segment), xml (parsed the server's XML), or
 *              self (discovered the node itself)
 *   bound      the calling process's own binding as a list, or
 *              "unbound" if the whole set
 *
 * The topology comes from PMIx_Load_topology(), which is v4.0 of the
 * Standard.  A 3.2 client has no such call and adopts nothing from
 * its server; with that client the tool does what a 3.2-era Open MPI
 * does and loads the server's XML (PMIX_HWLOC_XML_V2) itself, or
 * discovers the node if the server offered none.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <hwloc.h>
#include <pmix.h>

#include "log.h"

static char *get_xml (pmix_proc_t *self)
{
    pmix_proc_t proc;
    pmix_value_t *val;
    char *xml = NULL;

    snprintf (proc.nspace, sizeof (proc.nspace), "%s", self->nspace);
    proc.rank = PMIX_RANK_WILDCARD;
    if (PMIx_Get (&proc, PMIX_HWLOC_XML_V2, NULL, 0, &val) == PMIX_SUCCESS) {
        if (val->type == PMIX_STRING && val->data.string)
            xml = strdup (val->data.string);
        PMIX_VALUE_RELEASE (val);
    }
    return xml;
}

static bool shmem_mapped (void)
{
    FILE *f;
    char line[1024];
    bool mapped = false;

    if ((f = fopen ("/proc/self/maps", "r"))) {
        while (fgets (line, sizeof (line), f)) {
            if (strstr (line, "/hwloc.sm")) {
                mapped = true;
                break;
            }
        }
        fclose (f);
    }
    return mapped;
}

/* How did the client library come by the topology?  There is no API
 * for that, so look at the evidence: an adopted shared memory segment
 * is mapped from the server's hwloc.sm file; otherwise the server
 * offered XML and the client parsed it; otherwise the client found
 * nothing and discovered the node itself.
 */
static const char *via (pmix_proc_t *self)
{
    char *xml;

    if (shmem_mapped ())
        return "shmem";
    if ((xml = get_xml (self))) {
        free (xml);
        return "xml";
    }
    return "self";
}

#ifdef PMIX_TOPOLOGY2
static hwloc_topology_t load_topology (pmix_proc_t *self, const char **source)
{
    pmix_topology_t topo = PMIX_TOPOLOGY_STATIC_INIT;
    int rc;

    if ((rc = PMIx_Load_topology (&topo)) != PMIX_SUCCESS)
        log_msg_exit ("PMIx_Load_topology: %s", PMIx_Error_string (rc));
    if (!topo.source || strncmp (topo.source, "hwloc", 5) != 0)
        log_msg_exit ("unexpected topology source %s",
                      topo.source ? topo.source : "(null)");
    *source = topo.source;
    return topo.topology; // library-managed; PMIx_Finalize releases it
}
#else
static hwloc_topology_t load_topology (pmix_proc_t *self, const char **source)
{
    hwloc_topology_t hw;
    char *xml = get_xml (self);

    if (hwloc_topology_init (&hw) < 0)
        log_msg_exit ("hwloc_topology_init failed");
    if (xml) {
        if (hwloc_topology_set_xmlbuffer (hw, xml, strlen (xml) + 1) < 0
            || hwloc_topology_set_flags (hw,
                                         HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM) < 0)
            log_msg_exit ("error setting topology from server XML");
    }
    if (hwloc_topology_load (hw) < 0)
        log_msg_exit ("hwloc_topology_load failed");
    free (xml);
    *source = "hwloc";
    return hw;
}
#endif

int main (int argc, char **argv)
{
    pmix_proc_t self;
    const char *source;
    hwloc_topology_t hw;
    hwloc_bitmap_t set;
    char *cpuset = NULL;
    char *bound = NULL;
    int rc;

    if ((rc = PMIx_Init (&self, NULL, 0)) != PMIX_SUCCESS)
        log_msg_exit ("PMIx_Init: %s", PMIx_Error_string (rc));
    log_init ("topo");

    hw = load_topology (&self, &source);

    if (!(set = hwloc_bitmap_alloc ()))
        log_msg_exit ("out of memory");
    hwloc_bitmap_list_asprintf (&cpuset, hwloc_topology_get_topology_cpuset (hw));
    if (hwloc_get_cpubind (hw, set, HWLOC_CPUBIND_PROCESS) < 0)
        bound = strdup ("error");
    else if (hwloc_bitmap_isequal (set, hwloc_topology_get_topology_cpuset (hw)))
        bound = strdup ("unbound");
    else
        hwloc_bitmap_list_asprintf (&bound, set);

    printf ("%d: source=%s ncores=%d npus=%d cpuset=%s thissystem=%d"
            " via=%s bound=%s\n",
            self.rank,
            source,
            hwloc_get_nbobjs_by_type (hw, HWLOC_OBJ_CORE),
            hwloc_get_nbobjs_by_type (hw, HWLOC_OBJ_PU),
            cpuset,
            hwloc_topology_is_thissystem (hw),
            via (&self),
            bound);
    fflush (stdout);

    free (bound);
    free (cpuset);
    hwloc_bitmap_free (set);
    if ((rc = PMIx_Finalize (NULL, 0)) != PMIX_SUCCESS)
        log_msg_exit ("PMIx_Finalize: %s", PMIx_Error_string (rc));
    return 0;
}

// vi:ts=4 sw=4 expandtab
