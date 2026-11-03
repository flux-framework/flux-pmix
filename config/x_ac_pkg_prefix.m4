#  SYNOPSIS:
#    X_AC_PKG_PREFIX(VARIABLE, PREFIX-OR-YES, MODULES, [ACTION-IF-FOUND],
#                    [ACTION-IF-NOT-FOUND])
#
#  DESCRIPTION:
#    PKG_CHECK_MODULES for a package that may be pinned to an install
#    prefix with --with-NAME=PREFIX, in the usual configure idiom:
#    --with-NAME or --with-NAME=yes searches the pkg-config path, and
#    --with-NAME=PREFIX searches PREFIX/lib/pkgconfig and
#    PREFIX/lib64/pkgconfig first, and accepts only a package installed
#    under PREFIX, so one elsewhere on the path can neither be chosen
#    over it nor stand in for it when PREFIX has none.  The rest of
#    the path stays, since the package's own dependencies may be
#    there.  The caller handles --without-NAME before calling.
#
#    MODULES is a pkg-config module list, which may name a minimum
#    version.  On success VARIABLE_CFLAGS and VARIABLE_LIBS are set as
#    PKG_CHECK_MODULES sets them, and VARIABLE_MODULE, VARIABLE_VERSION,
#    and VARIABLE_PREFIX are set and substituted from the package.  The
#    pkg-config environment is restored before the actions run, so
#    they may call pkg-config to query the package themselves only if
#    they take the prefix into account.
#
#    ACTION-IF-NOT-FOUND defaults to an error naming the prefix.

AC_DEFUN([X_AC_PKG_PREFIX], [
  AC_REQUIRE([PKG_PROG_PKG_CONFIG])
  x_ac_pinned=no
  AS_IF([test "$2" != yes], [
    AS_IF([test ! -d "$2"], [AC_MSG_ERROR([$2: not a directory])])
    x_ac_saved_path="$PKG_CONFIG_PATH"
    PKG_CONFIG_PATH="$2/lib/pkgconfig:$2/lib64/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    export PKG_CONFIG_PATH
    x_ac_pinned=yes
  ])
  # the module list may carry a version constraint; the name is first
  x_ac_module="$3"
  x_ac_module=${x_ac_module%% *}
  PKG_CHECK_MODULES([$1], [$3], [
    x_ac_found=yes
    $1[_MODULE]=$x_ac_module
    $1[_VERSION]=$($PKG_CONFIG --modversion $x_ac_module)
    $1[_PREFIX]=$($PKG_CONFIG --variable=prefix $x_ac_module)
    AC_SUBST($1[_MODULE])
    AC_SUBST($1[_VERSION])
    AC_SUBST($1[_PREFIX])
    # a pinned package must be the one under the prefix, not one the
    # rest of the path supplied
    AS_IF([test "$x_ac_pinned" = yes -a "$[$1][_PREFIX]" != "$2"],
      [x_ac_found=no])
  ], [x_ac_found=no])
  AS_IF([test "$x_ac_pinned" = yes], [
    PKG_CONFIG_PATH="$x_ac_saved_path"
    export PKG_CONFIG_PATH
  ])
  AS_IF([test "$x_ac_found" = yes],
    [$4],
    [m4_default([$5], [AC_MSG_ERROR([no $3 found under $2])])])
])
