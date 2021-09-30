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
#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <flux/optparse.h>

#include "log.h"


#include "log.h"

const char *opt_usage = "[OPTIONS]";

static struct optparse_option opts[] = {
    { .name = "count", .has_arg = 1, .arginfo = "TEXT",
      .usage = "number of ping pong round trips (default 1)",
    },
    OPTPARSE_TABLE_END,
};


int main (int argc, char *argv[])
{
    optparse_t *p;
    int optindex;
    int rank = -1;
    int size;
    char name[32];
    int count;
    int limit;
    int peer_rank;

    MPI_Init (&argc, &argv);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);

    if (!(p = optparse_create ("mpi_pingpong"))
        || optparse_add_option_table (p, opts) != OPTPARSE_SUCCESS
        || optparse_set (p, OPTPARSE_USAGE, opt_usage) != OPTPARSE_SUCCESS)
        log_msg_exit ("error setting up option parsing");
    if ((optindex = optparse_parse_args (p, argc, argv)) < 0)
        return 1;
    if (optindex != argc) {
        optparse_print_usage (p);
        return 1;
    }
    limit = optparse_get_int (p, "count", 1);
    if (limit < 1)
        log_msg_exit ("count must be at least 1");

    snprintf (name, sizeof (name), "%d", rank);
    log_init (name);

    if (size != 2)
        log_msg_exit ("this program requires exactly 2 processes");

    if (rank == 0)
        log_msg ("MPI_Init completed");


    MPI_Barrier (MPI_COMM_WORLD);
    if (rank == 0)
        log_msg ("MPI_Barrier completed");

    peer_rank = (rank + 1) % 2;
    for (count = 0; count < limit*2; count++) {
        if (count % 2 == rank)
            MPI_Send (&count, 1, MPI_INT, peer_rank, 0, MPI_COMM_WORLD);
        else {
            int ncount;
            MPI_Recv (&ncount, 1, MPI_INT, peer_rank, 0, MPI_COMM_WORLD,
                      MPI_STATUS_IGNORE);
            if (ncount != count)
                log_msg_exit ("bad data received (%d != %d)", ncount, count);
        }
        if (rank == 0 && count % 2 == 1)
            log_msg ("pingpong seq=%d completed", count / 2);
    }

    MPI_Finalize ();
    if (rank == 0)
        log_msg ("MPI_Finalize completed");
    return 0;
}

// vi: ts=4 sw=4 expandtab

