AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)

# The shim is dlopen'd into a client by the dynamic linker, not
# linked against, so it installs beside the other private helpers
# rather than in libdir.  It is versionless because the plugin finds
# it by an exact path, which configure fixes at build time.
pmix_libexec_LTLIBRARIES = \
	pmix-capture.la

pmix_capture_la_SOURCES = \
	shim.c \
	shim.h

pmix_capture_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module \
	-avoid-version \
	-shared \
	--disable-static \
	-Wl,--version-script=$(srcdir)/shim.map

EXTRA_DIST = shim.map

EXTRA_pmix_capture_la_DEPENDENCIES = \
	shim.map

pmix_capture_la_LIBADD = \
	$(top_builddir)/src/common/libutil/libutil.la \
	-ldl \
	-lpthread
