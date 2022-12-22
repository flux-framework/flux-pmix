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

int main (int argc, char *argv[])
{
#if HAVE_MPI
    char version[MPI_MAX_LIBRARY_VERSION_STRING];
    int len;

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

