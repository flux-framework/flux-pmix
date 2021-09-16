AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)

check_PROGRAMS = \
	barrier \
	bizcard

check_LTLIBRARIES = libtestutil.la

libtestutil_la_SOURCES = \
	log.c \
	log.h \
	monotime.c \
	monotime.h

test_ldadd = \
	$(builddir)/libtestutil.la

barrier_SOURCES = barrier.c
barrier_CPPFLAGS = $(AM_CPPFLAGS) $(PMIX_CFLAGS)
barrier_LDADD = $(test_ldadd) $(PMIX_LIBS)

bizcard_SOURCES = bizcard.c
bizcard_CPPFLAGS = $(AM_CPPFLAGS) $(PMIX_CFLAGS)
bizcard_LDADD = $(test_ldadd) $(PMIX_LIBS)
