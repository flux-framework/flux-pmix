AM_CFLAGS = \
	$(WARNING_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS)

AM_LDFLAGS = \
	$(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = \
	-I$(top_srcdir)

check_PROGRAMS = \
	barrier \
	bizcard \
	mpi_hello \
	mpi_version \
	mpi_abort

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

mpi_hello_SOURCES = mpi_hello.c
mpi_hello_CPPFLAGS = $(AM_CPPFLAGS) $(OMPI_CFLAGS)
mpi_hello_LDADD = $(test_ldadd) $(OMPI_LIBS)

mpi_version_SOURCES = mpi_version.c
mpi_version_CPPFLAGS = $(AM_CPPFLAGS) $(OMPI_CFLAGS)
mpi_version_LDADD = $(test_ldadd) $(OMPI_LIBS)

mpi_abort_SOURCES = mpi_abort.c
mpi_abort_CPPFLAGS = $(AM_CPPFLAGS) $(OMPI_CFLAGS)
mpi_abort_LDADD = $(test_ldadd) $(OMPI_LIBS)
