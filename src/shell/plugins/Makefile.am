AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)
AM_CPPFLAGS = \
	-I$(top_srcdir)

shell_plugin_LTLIBRARIES = \
	pmix.la

pmix_la_SOURCES = \
	main.c \
	infovec.h \
	infovec.c \
	maps.h \
	maps.c \
	codec.h \
	codec.c \
	interthread.h \
	interthread.c \
	exchange.h \
	exchange.c \
	fence.h \
	fence.c \
	abort.h \
	abort.c \
	notify.h \
	notify.c \
	dmodex.h \
	dmodex.c
pmix_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(FLUX_CORE_CFLAGS) \
	$(FLUX_IDSET_CFLAGS) \
	$(FLUX_HOSTLIST_CFLAGS) \
	$(PMIX_CFLAGS) \
	$(JANSSON_CFLAGS)
pmix_la_LIBADD = \
	$(FLUX_CORE_LIBS) \
	$(PMIX_LIBS) \
	$(FLUX_IDSET_LIBS) \
	$(FLUX_HOSTLIST_LIBS) \
	$(JANSSON_LIBS) \
	$(top_builddir)/src/common/libccan/libccan.la
pmix_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(fluxplugin_ldflags) \
	-module

TESTS = \
	test_infovec.t \
	test_codec.t

test_ldadd = \
	$(top_builddir)/src/common/libtap/libtap.la \
	$(top_builddir)/src/common/libccan/libccan.la

test_ldflags = \
        -no-install

test_cppflags = \
        -I$(top_srcdir)/src/common/libtap \
        $(AM_CPPFLAGS)

check_PROGRAMS = $(TESTS)

test_infovec_t_SOURCES = \
        infovec.c \
        infovec.h \
        test/infovec.c
test_infovec_t_CPPFLAGS = \
        $(PMIX_CFLAGS) \
        $(test_cppflags)
test_infovec_t_LDADD = \
        $(test_ldadd)
test_infovec_t_LDFLAGS = \
        $(test_ldflags)

test_codec_t_SOURCES = \
        codec.c \
        codec.h \
        test/codec.c
test_codec_t_CPPFLAGS = \
        $(PMIX_CFLAGS) \
        $(JANSSON_CFLAGS) \
        $(test_cppflags)
test_codec_t_LDADD = \
        $(test_ldadd) \
	$(JANSSON_LIBS)
test_codec_t_LDFLAGS = \
        $(test_ldflags)
