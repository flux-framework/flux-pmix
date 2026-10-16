/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <stdlib.h>
#include <string.h>

#include "tap.h"
#include "src/common/libutil/getenv_prefix.h"

int main (int argc, char *argv[])
{
    const char *val;

    plan (NO_PLAN);

    if (unsetenv ("ENVPFX_A") < 0
        || unsetenv ("ENVPFX_LONGER") < 0)
        BAIL_OUT ("unable to clear test env vars");

    ok (getenv_prefix ("ENVPFX_") == NULL,
        "getenv_prefix returns NULL when nothing matches");

    if (setenv ("ENVPFX_A", "avalue", 1) < 0)
        BAIL_OUT ("unable to set test env vars");

    val = getenv_prefix ("ENVPFX_A");
    ok (val != NULL && !strcmp (val, "avalue"),
        "getenv_prefix finds an exact name match");

    val = getenv_prefix ("ENVPFX_");
    ok (val != NULL && !strcmp (val, "avalue"),
        "getenv_prefix finds a name by prefix");

    /* The variable the probe cares about is named for a wire protocol,
     * e.g. PMIX_SERVER_URI41, so a longer name must match a shorter
     * prefix.
     */
    if (setenv ("ENVPFX_LONGER", "lvalue", 1) < 0)
        BAIL_OUT ("unable to set test env vars");
    ok (getenv_prefix ("ENVPFX_LONG") != NULL,
        "getenv_prefix matches a name longer than the prefix");

    ok (getenv_prefix ("ENVPFX_NOPE") == NULL,
        "getenv_prefix returns NULL when only a longer prefix is given");

    ok (getenv_prefix (NULL) == NULL,
        "getenv_prefix prefix=NULL returns NULL");

    done_testing ();
}

// vi:ts=4 sw=4 expandtab
