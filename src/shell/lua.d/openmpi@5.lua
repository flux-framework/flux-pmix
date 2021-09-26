-------------------------------------------------------------
-- Copyright 2021 Lawrence Livermore National Security, LLC
-- (c.f. AUTHORS, NOTICE.LLNS, COPYING)
--
-- This file is part of the Flux resource manager framework.
-- For details, see https://github.com/flux-framework.
--
-- SPDX-License-Identifier: LGPL-3.0
-------------------------------------------------------------

local mpi, version = shell.getopt_with_version ("mpi")

if mpi ~= "openmpi" or version ~= "5" then return end

plugin.load ("pmix/pmix.so")
