##
# Prologue
##
AC_INIT([flux-pmix],
        m4_esyscmd([test -n "$FLUX_PMIX_VERSION" && printf $FLUX_PMIX_VERSION || git describe --always | awk '/.*/ {sub(/^v/, ""); printf "%s",$1; exit}']))
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config])
AC_CONFIG_SRCDIR([NEWS.md])
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([subdir-objects tar-pax foreign])
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([config/config.h])
AM_MAINTAINER_MODE([enable])

AC_DEFINE([_GNU_SOURCE], 1,
          [Define _GNU_SOURCE so that we get all necessary prototypes])

##
# Generate project versions from PACKAGE_VERSION (set from git describe above)
##
AX_SPLIT_VERSION
AX_POINT_VERSION=$(echo $AX_POINT_VERSION | $SED 's/-.*$//')
AC_SUBST([AX_MAJOR_VERSION])
AC_SUBST([AX_MINOR_VERSION])
AC_SUBST([AX_POINT_VERSION])

##
# Initialize pkg-config for PKG_CHECK_MODULES to avoid conditional issues
##
PKG_PROG_PKG_CONFIG

# Checks for programs
##
m4_version_prereq(2.70, [AC_PROG_CC], [AC_PROG_CC_C99])
AM_PROG_CC_C_O
AX_COMPILER_VENDOR
AX_COMPILER_VERSION
AS_CASE($ax_cv_c_compiler_vendor,
  [clang | gnu], [
    WARNING_CFLAGS="-Wall -Werror -Wno-strict-aliasing -Wno-error=deprecated-declarations"
  ]
)
saved_CFLAGS="$CFLAGS"
CFLAGS="-Wstringop-overread $CFLAGS"
AC_MSG_CHECKING([whether compiler accepts -Wstringop-overread])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [
  AC_MSG_RESULT([yes])
  WARNING_CFLAGS="$WARNING_CFLAGS -Wno-error=stringop-overread"
  ],[
  AC_MSG_RESULT([no])
])
CFLAGS="$saved_CFLAGS"
AC_SUBST([WARNING_CFLAGS])

LT_INIT

PKG_CHECK_MODULES([PMIX], [pmix >= 3.2.3])
PKG_CHECK_MODULES([JANSSON], [jansson >= 2.10], [], [])

AC_ARG_WITH([openmpi],
  [AS_HELP_STRING([--without-openmpi],
    [Build without Open MPI])],
  [],
  [with_openmpi=yes]
)

AS_IF([test "x$with_openmpi" != xno],
  [PKG_CHECK_MODULES([OMPI], [ompi >= 3.0.0], [have_ompi=yes], [have_ompi=no])]
)
if test "${have_ompi}" = "yes"; then
  OMPI_PREFIX=`pkg-config --variable=prefix ompi`
  if test -z "${OMPI_PREFIX}"; then
    AC_MSG_ERROR([failed to determine ompi prefix from pkg-config])
  fi
  AC_SUBST(OMPI_PREFIX)
  AC_DEFINE(HAVE_MPI, [1], [whether to build mpi tests])
fi
AM_CONDITIONAL([HAVE_MPI], [test "x$have_ompi" = "xyes"])

X_AC_CHECK_PTHREADS

AX_FLUX_CORE

AC_ARG_ENABLE([docs],
              AS_HELP_STRING([--disable-docs], [disable building docs]))
AM_PATH_PYTHON([3.6])
AS_IF([test "x$enable_docs" != "xno"], [
            AM_CHECK_PYMOD(sphinx,
                           [Version(sphinx.__version__) >= Version ('1.6.7')],
                           [sphinx=true],
                           [sphinx=false; AC_MSG_WARN([could not find sphinx to generate docs, version 1.6.7+ required])]
                           )
            AM_CHECK_PYMOD(docutils,
                           [Version(docutils.__version__) >= Version ('0.11.0')],
                           [docutils=true],
                           [docutils=false; AC_MSG_WARN([could not find docutils to generate docs, version 0.11.0+ required])]
                           )
])
#  If --enable-docs=yes, but no doc generator found,
#   then error immediately:
#
AS_IF([test "x$enable_docs" = "xyes" -a "x$sphinx" = "xfalse"],[
  AC_MSG_ERROR([--enable-docs used but no document generator found!])
])
AS_IF([test "x$enable_docs" = "xyes" -a "x$docutils" = "xfalse"],[
  AC_MSG_ERROR([--enable-docs used but docutils not found!])
])
AM_CONDITIONAL([ENABLE_DOCS], [test "x$sphinx" = "xtrue" -a "x$docutils" = "xtrue"])
AC_CHECK_PROG(ASPELL,[aspell],[aspell])
AX_CODE_COVERAGE

AS_VAR_SET(shell_lua_mpidir, $sysconfdir/flux/shell/lua.d/mpi)
AC_SUBST(shell_lua_mpidir)

AS_VAR_SET(shell_plugindir, $libdir/flux/shell/plugins)
AC_SUBST(shell_plugindir)

AS_VAR_SET(upmi_plugindir, $libdir/flux/upmi/plugins)
AC_SUBST(upmi_plugindir)

AS_VAR_SET(pmix_libexecdir, $libexecdir/flux-pmix)
AC_SUBST(pmix_libexecdir)

fluxplugin_ldflags="-avoid-version -export-symbols-regex '^flux_plugin_init\$\$' --disable-static -shared -export-dynamic"
AC_SUBST(fluxplugin_ldflags)

##
# Epilogue
##
AC_CONFIG_FILES( \
  Makefile \
  src/common/libtap/Makefile \
  src/common/libccan/Makefile \
  src/common/libutil/Makefile \
  src/common/libpcapng/Makefile \
  src/capture/Makefile \
  src/shell/plugins/Makefile \
  src/upmi/Makefile \
  t/Makefile \
  t/sharness.d/00-setup.sh \
  t/src/Makefile \
  t/osu-micro-benchmarks/Makefile \
  t/etc/rc.lua \
  doc/Makefile \
  doc/test/Makefile \
)

AC_OUTPUT
