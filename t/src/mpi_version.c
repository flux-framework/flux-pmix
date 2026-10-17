/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#if HAVE_MPI
#include <mpi.h>
#endif
#include <stdio.h>
#include <string.h>

int main (int argc, char *argv[])
{
#if HAVE_MPI
    char version[MPI_MAX_LIBRARY_VERSION_STRING];
    int len;

    /* The library version string has no portable format, so tests that
     * need to compare versions ask for the major number instead.  A
     * non-Open MPI library reports 0, which reads as "older than any
     * version we need to work around".
     */
    if (argc == 2 && !strcmp (argv[1], "--major")) {
#ifdef OMPI_MAJOR_VERSION
        printf ("%d\n", OMPI_MAJOR_VERSION);
#else
        printf ("0\n");
#endif
        return 0;
    }

    MPI_Get_library_version (version, &len);
    if (len < 0) {
        fprintf (stderr, "MPI_Get_library_version failed\n");
        return -1;
    }
    printf ("%s\n", version);
    return 0;
#else
    fprintf (stderr, "MPI is not configured");
    return -1;
#endif
}

// vi: ts=4 sw=4 expandtab

