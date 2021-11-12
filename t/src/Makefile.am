AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(FLUX_OPTPARSE_CFLAGS) \
	$(FLUX_IDSET_CFLAGS) \
	$(OMPI_CFLAGS) \
	$(PMIX_CFLAGS)

check_PROGRAMS = \
	barrier \
	bizcard \
	version \
	getkey \
	abort \
	notify \
	mpi_hello \
	mpi_version \
	mpi_abort \
	mpi_pingpong

check_LTLIBRARIES = libtestutil.la

libtestutil_la_SOURCES = \
	log.c \
	log.h \
	monotime.c \
	monotime.h

test_ldadd = \
	$(builddir)/libtestutil.la \
	$(top_builddir)/src/common/libutil/libutil.la \
	$(FLUX_OPTPARSE_LIBS) \
	$(FLUX_IDSET_LIBS) \
	$(OMPI_LIBS) \
	$(PMIX_LIBS)

barrier_SOURCES = barrier.c
barrier_LDADD = $(test_ldadd)

bizcard_SOURCES = bizcard.c
bizcard_LDADD = $(test_ldadd)

version_SOURCES = version.c
version_LDADD = $(test_ldadd)

getkey_SOURCES = getkey.c
getkey_LDADD = $(test_ldadd)

abort_SOURCES = abort.c
abort_LDADD = $(test_ldadd)

notify_SOURCES = notify.c
notify_LDADD = $(test_ldadd)

mpi_hello_SOURCES = mpi_hello.c
mpi_hello_LDADD = $(test_ldadd)

mpi_version_SOURCES = mpi_version.c
mpi_version_LDADD = $(test_ldadd)

mpi_abort_SOURCES = mpi_abort.c
mpi_abort_LDADD = $(test_ldadd)

mpi_pingpong_SOURCES = mpi_pingpong.c
mpi_pingpong_LDADD = $(test_ldadd)
