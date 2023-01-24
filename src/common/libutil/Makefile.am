AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)

noinst_LTLIBRARIES = libutil.la
libutil_la_SOURCES = \
	strlcpy.c \
	strlcpy.h \
	unsetenv_glob.c \
	unsetenv_glob.h

TESTS =	test_unsetenv_glob.t

test_ldadd = \
	$(top_builddir)/src/common/libutil/libutil.la \
	$(top_builddir)/src/common/libtap/libtap.la
test_cppflags = \
	-I$(top_srcdir)/src/common/libtap \
	$(AM_CPPFLAGS)

check_PROGRAMS = $(TESTS)

TEST_EXTENSIONS = .t
T_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/config/tap-driver.sh

test_unsetenv_glob_t_SOURCES = test/unsetenv_glob.c
test_unsetenv_glob_t_CPPFLAGS = $(test_cppflags)
test_unsetenv_glob_t_LDADD = $(test_ldadd)
