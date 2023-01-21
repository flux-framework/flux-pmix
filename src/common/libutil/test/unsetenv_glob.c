/************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <stdlib.h>
#include <errno.h>

#include "tap.h"
#include "src/common/libutil/unsetenv_glob.h"

int main (int argc, char *argv[])
{
    plan (NO_PLAN);

    if (setenv ("ENVGLOB_A", "x", 1) < 0
        || setenv ("ENVGLOB_B", "x", 1) < 0
        || setenv ("ENVGLOB_BB", "x", 1) < 0
        || setenv ("ENVGLOB_C", "x", 1) < 0)
        BAIL_OUT ("unable to set test env vars");

    ok (unsetenv_glob ("ENVGLOB_A") == 1,
        "unsetenv_glob with non-glob pattern works");
    ok (!getenv ("ENVGLOB_A"),
        "target variable is unset");
    ok (getenv ("ENVGLOB_B") && getenv ("ENVGLOB_BB") && getenv ("ENVGLOB_C"),
        "non-target variables remain set");

    ok (unsetenv_glob ("ENVGLOB_B*") == 2,
        "unsetenv_glob wtih glob pattern works");
    ok (!getenv ("ENVGLOB_B") && !getenv ("ENVGLOB_BB"),
        "target variables are unset");
    ok (getenv ("ENVGLOB_C") != NULL,
        "non-target variable remains set");

    ok (unsetenv_glob ("") == 0,
        "unsetenv_glob pattern=\"\" returns 0");
    errno = 0;
    ok (unsetenv_glob (NULL) < 0,
        "unsetenv_glob NULL fails with EINVAL");

    done_testing ();
}

// vi:ts=4 sw=4 expandtab
