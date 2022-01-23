flux-pmix version 0.2.0 - 2022-01-22
------------------------------------

## Fixes

 * plugin: call notify_destroy() early (Fixes hang on RHEL7/spack) (#45)
 * use strlcpy instead of strncpy (#53)
 * build: fix build with gcc 4.x, add centos7 to ci (#49)

## Testsuite

 * docker: adjust focal Dockerfile for upstream package changes (#56)

## Cleanup

 * README: update LLNL-CODE (#52)
 * mergify: replace deprecated strict merge strategy (#51)

flux-pmix version 0.1.0 - 2021-10-15
------------------------------------

This is the first release of the pmix job shell plugin for flux.
At this point, real world testing has been minimal.
