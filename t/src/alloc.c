/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 \************************************************************/

/* alloc.c - call PMIx_Allocation_request()
 */

#include <stdio.h>
#include <stdlib.h>
#include "pmix.h"

int main(int argc, char *argv[]){
    int rc;
    size_t nresults;
    pmix_proc_t myproc;
    pmix_info_t *info, *results;

    if (PMIX_SUCCESS != (rc = PMIx_Init(&myproc, NULL, 0))){
        fprintf(stderr, "Error in PMIx_Init: %d\n", rc);
        return 1;
    }

    PMIX_INFO_CREATE(info, 1);
    PMIX_INFO_LOAD(&info[0], "test", "test", PMIX_STRING);
    printf("Client ns %s rank %d sending allocation request to PMIx server\n", myproc.nspace, myproc.rank);
    if (PMIX_SUCCESS
        != (rc = PMIx_Allocation_request(PMIX_ALLOC_NEW, info, 1, &results, &nresults))) {
        fprintf(stderr, "Client ns %s rank %d: PMIx_Allocation_request_nb failed: %d\n",
                myproc.nspace, myproc.rank, rc);
        PMIX_INFO_FREE(info, 1);
        return 1;
    }
    for(int n = 0; n < nresults; n++){
        if(results[n].value.type == PMIX_STRING){
            printf("Client recevived allocation result[%d] %s -> %s\n", n, results[n].key, results[n].value.data.string);
        }
    }
    PMIX_INFO_FREE(info, 1);

    if (PMIX_SUCCESS != (rc = PMIx_Finalize(NULL, 0))){
        fprintf(stderr, "Error in PMIx_Finalize: %d\n", rc);
        return 1;
    }

    return 0;
}

// vi:ts=4 sw=4 expandtab