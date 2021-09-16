/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
 \************************************************************/

/* version.c - print pmix version
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <pmix.h>
#include <stdio.h>

int main (int argc, char **argv)
{
    printf ("%s\n", PMIx_Get_version ());
    return 0;
}

// vi:ts=4 sw=4 expandtab
