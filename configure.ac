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

# The server is the openpmix the shell plugin links; the clients are
# the test programs and the upmi plugin.  Each may be pinned to a prefix.
# A pinned server with no pinned client still takes the default client,
# which stands in for the MPIs the server will meet in production.
PMIX_SERVER_MIN=3.2.3
PMIX_CLIENT_MIN=3.2.3
AC_ARG_WITH([pmix-server],
  [AS_HELP_STRING([--with-pmix-server=PREFIX],
    [Build the server against the openpmix installed under PREFIX])],
  [],
  [with_pmix_server=yes]
)
AC_ARG_WITH([pmix-client],
  [AS_HELP_STRING([--with-pmix-client=PREFIX],
    [Build PMIx clients against the openpmix installed under PREFIX])],
  [],
  [with_pmix_client=yes]
)
AS_IF([test "$with_pmix_server" = no -o "$with_pmix_client" = no],
  [AC_MSG_ERROR([flux-pmix requires openpmix])])

X_AC_PKG_PREFIX([PMIX_CLIENT], [$with_pmix_client], [pmix >= $PMIX_CLIENT_MIN],
  [AC_MSG_NOTICE([client openpmix $PMIX_CLIENT_VERSION in $PMIX_CLIENT_PREFIX])],
  [AC_MSG_ERROR([no openpmix >= $PMIX_CLIENT_MIN for the client; see --with-pmix-client=PREFIX])])
X_AC_PKG_PREFIX([PMIX], [$with_pmix_server], [pmix >= $PMIX_SERVER_MIN],
  [AC_MSG_NOTICE([server openpmix $PMIX_VERSION in $PMIX_PREFIX])],
  [AC_MSG_ERROR([no openpmix >= $PMIX_SERVER_MIN for the server; see --with-pmix-server=PREFIX])])
PMIX_SERVER_VERSION=$PMIX_VERSION
AC_SUBST(PMIX_SERVER_VERSION)

# The plugin loads a topology that libpmix then walks, and both resolve
# hwloc through the one copy flux-shell already loaded, so all three must
# share a soname.  hwloc 2.x is one soname; its struct layout has been
# stable since 2.4.  Keep topo.c on 2.0-era API so an older runtime
# hwloc than the build one still has every symbol.
PKG_CHECK_MODULES([HWLOC], [hwloc >= 2.0 hwloc < 3.0])
PKG_CHECK_MODULES([JANSSON], [jansson >= 2.10], [], [])

# The MPI tests build against a PMIx-capable MPI: Open MPI, or an MPICH
# configured --with-pmi=pmix.  Either may be pinned to a prefix, as the
# openpmix libraries are.  The tests learn which MPI they got at runtime.
AC_ARG_WITH([mpi],
  [AS_HELP_STRING([--with-mpi=PREFIX],
    [Build the MPI tests against the MPI installed under PREFIX
     (default: the one pkg-config finds, or none)])],
  [],
  [with_mpi=yes]
)
# --without-openmpi predates --with-mpi and may still be in use
AC_ARG_WITH([openmpi], [], [with_mpi=$with_openmpi])
have_mpi=no
AS_IF([test "$with_mpi" != no], [
  X_AC_PKG_PREFIX([MPI], [$with_mpi], [ompi >= 3.0.0], [have_mpi=yes],
    [X_AC_PKG_PREFIX([MPI], [$with_mpi], [mpich >= 4.0], [have_mpi=yes],
      [AS_IF([test "$with_mpi" != yes],
        [AC_MSG_ERROR([no Open MPI or MPICH under $with_mpi])])])])
])
AS_IF([test "$have_mpi" = yes], [
  AC_MSG_NOTICE([MPI tests use $MPI_MODULE $MPI_VERSION in $MPI_PREFIX])
  AC_DEFINE(HAVE_MPI, [1], [whether to build mpi tests])
])
AM_CONDITIONAL([HAVE_MPI], [test "$have_mpi" = yes])

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

# Wireshark loads Lua plugins from an unversioned directory under
# libdir, so the dissector installed there is found without setup.
AC_ARG_WITH([wireshark-plugindir],
  [AS_HELP_STRING([--with-wireshark-plugindir=DIR],
    [install the Wireshark dissector in DIR [LIBDIR/wireshark/plugins]])],
  [wireshark_plugindir=$withval],
  [wireshark_plugindir=$libdir/wireshark/plugins])
AC_SUBST(wireshark_plugindir)

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
  t/etc/rc.lua \
  doc/Makefile \
  doc/test/Makefile \
)

AC_OUTPUT
