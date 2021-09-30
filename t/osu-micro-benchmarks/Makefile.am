AM_CFLAGS = \
        $(WARNING_CFLAGS) \
        $(CODE_COVERAGE_CFLAGS) \
	-Wno-error=switch \
	-Wno-error=format \
	-Wno-error=format-extra-args \
	-Wno-error=unused-variable \
	-Wno-error=maybe-uninitialized

AM_LDFLAGS = \
        $(CODE_COVERAGE_LIBS)

AM_CPPFLAGS = \
        -I$(top_srcdir) \
	-I$(top_srcdir)/t/osu-micro-benchmarks/util \
	$(OMPI_CFLAGS)

check_PROGRAMS = \
	mpi/startup/osu_init \
	mpi/startup/osu_hello \
	mpi/pt2pt/osu_bw \
	mpi/pt2pt/osu_bibw \
	mpi/pt2pt/osu_latency \
	mpi/pt2pt/osu_mbw_mr \
	mpi/pt2pt/osu_multi_lat \
	mpi/pt2pt/osu_latency_mt \
	mpi/pt2pt/osu_latency_mp \
	mpi/collective/osu_allgatherv \
	mpi/collective/osu_scatterv \
	mpi/collective/osu_gather \
	mpi/collective/osu_gatherv \
	mpi/collective/osu_reduce_scatter \
	mpi/collective/osu_barrier \
	mpi/collective/osu_reduce \
	mpi/collective/osu_allreduce \
	mpi/collective/osu_bcast \
	mpi/collective/osu_alltoall \
	mpi/collective/osu_alltoallv \
	mpi/collective/osu_ialltoall \
	mpi/collective/osu_ialltoallv \
	mpi/collective/osu_ialltoallw \
	mpi/collective/osu_ibarrier \
	mpi/collective/osu_ibcast \
	mpi/collective/osu_igather \
	mpi/collective/osu_igatherv \
	mpi/collective/osu_allgather \
	mpi/collective/osu_iallgather \
	mpi/collective/osu_iallgatherv \
	mpi/collective/osu_scatter \
	mpi/collective/osu_iscatter \
	mpi/collective/osu_iscatterv \
	mpi/collective/osu_iallreduce \
	mpi/collective/osu_ireduce \
	mpi/one-sided/osu_put_latency \
	mpi/one-sided/osu_put_bw \
	mpi/one-sided/osu_put_bibw \
	mpi/one-sided/osu_get_latency \
	mpi/one-sided/osu_get_bw \
	mpi/one-sided/osu_acc_latency \
	mpi/one-sided/osu_fop_latency \
	mpi/one-sided/osu_cas_latency \
	mpi/one-sided/osu_get_acc_latency

check_LTLIBRARIES = util/libutil.la

util_libutil_la_SOURCES = \
	util/osu_util.c \
	util/osu_util.h \
	util/osu_util_mpi.c \
	util/osu_util_mpi.h

test_ldadd = \
	$(builddir)/util/libutil.la \
	$(OMPI_LIBS)


mpi_startup_osu_init_SOURCES = mpi/startup/osu_init.c
mpi_startup_osu_init_LDADD = $(test_ldadd)

mpi_startup_osu_hello_SOURCES = mpi/startup/osu_hello.c
mpi_startup_osu_hello_LDADD = $(test_ldadd)

mpi_pt2pt_osu_bw_SOURCES = mpi/pt2pt/osu_bw.c
mpi_pt2pt_osu_bw_LDADD = $(test_ldadd)

mpi_pt2pt_osu_bibw_SOURCES = mpi/pt2pt/osu_bibw.c
mpi_pt2pt_osu_bibw_LDADD = $(test_ldadd)

mpi_pt2pt_osu_latency_SOURCES = mpi/pt2pt/osu_latency.c
mpi_pt2pt_osu_latency_LDADD = $(test_ldadd)

mpi_pt2pt_osu_mbw_mr_SOURCES = mpi/pt2pt/osu_mbw_mr.c
mpi_pt2pt_osu_mbw_mr_LDADD = $(test_ldadd)

mpi_pt2pt_osu_multi_lat_SOURCES = mpi/pt2pt/osu_multi_lat.c
mpi_pt2pt_osu_multi_lat_LDADD = $(test_ldadd)

mpi_pt2pt_osu_latency_mt_SOURCES = mpi/pt2pt/osu_latency_mt.c
mpi_pt2pt_osu_latency_mt_LDADD = $(test_ldadd)

mpi_pt2pt_osu_latency_mp_SOURCES = mpi/pt2pt/osu_latency_mp.c
mpi_pt2pt_osu_latency_mp_LDADD = $(test_ldadd)

mpi_collective_osu_allgatherv_SOURCES = mpi/collective/osu_allgatherv.c
mpi_collective_osu_allgatherv_LDADD = $(test_ldadd)

mpi_collective_osu_scatterv_SOURCES = mpi/collective/osu_scatterv.c
mpi_collective_osu_scatterv_LDADD = $(test_ldadd)

mpi_collective_osu_gather_SOURCES = mpi/collective/osu_gather.c
mpi_collective_osu_gather_LDADD = $(test_ldadd)

mpi_collective_osu_gatherv_SOURCES = mpi/collective/osu_gatherv.c
mpi_collective_osu_gatherv_LDADD = $(test_ldadd)

mpi_collective_osu_reduce_scatter_SOURCES = mpi/collective/osu_reduce_scatter.c
mpi_collective_osu_reduce_scatter_LDADD = $(test_ldadd)

mpi_collective_osu_barrier_SOURCES = mpi/collective/osu_barrier.c
mpi_collective_osu_barrier_LDADD = $(test_ldadd)

mpi_collective_osu_reduce_SOURCES = mpi/collective/osu_reduce.c
mpi_collective_osu_reduce_LDADD = $(test_ldadd)

mpi_collective_osu_allreduce_SOURCES = mpi/collective/osu_allreduce.c
mpi_collective_osu_allreduce_LDADD = $(test_ldadd)

mpi_collective_osu_bcast_SOURCES = mpi/collective/osu_bcast.c
mpi_collective_osu_bcast_LDADD = $(test_ldadd)

mpi_collective_osu_alltoall_SOURCES = mpi/collective/osu_alltoall.c
mpi_collective_osu_alltoall_LDADD = $(test_ldadd)

mpi_collective_osu_alltoallv_SOURCES = mpi/collective/osu_alltoallv.c
mpi_collective_osu_alltoallv_LDADD = $(test_ldadd)

mpi_collective_osu_ialltoall_SOURCES = mpi/collective/osu_ialltoall.c
mpi_collective_osu_ialltoall_LDADD = $(test_ldadd)

mpi_collective_osu_ialltoallv_SOURCES = mpi/collective/osu_ialltoallv.c
mpi_collective_osu_ialltoallv_LDADD = $(test_ldadd)

mpi_collective_osu_ialltoallw_SOURCES = mpi/collective/osu_ialltoallw.c
mpi_collective_osu_ialltoallw_LDADD = $(test_ldadd)

mpi_collective_osu_ibarrier_SOURCES = mpi/collective/osu_ibarrier.c
mpi_collective_osu_ibarrier_LDADD = $(test_ldadd)

mpi_collective_osu_ibcast_SOURCES = mpi/collective/osu_ibcast.c
mpi_collective_osu_ibcast_LDADD = $(test_ldadd)

mpi_collective_osu_igather_SOURCES = mpi/collective/osu_igather.c
mpi_collective_osu_igather_LDADD = $(test_ldadd)

mpi_collective_osu_igatherv_SOURCES = mpi/collective/osu_igatherv.c
mpi_collective_osu_igatherv_LDADD = $(test_ldadd)

mpi_collective_osu_allgather_SOURCES = mpi/collective/osu_allgather.c
mpi_collective_osu_allgather_LDADD = $(test_ldadd)

mpi_collective_osu_iallgather_SOURCES = mpi/collective/osu_iallgather.c
mpi_collective_osu_iallgather_LDADD = $(test_ldadd)

mpi_collective_osu_iallgatherv_SOURCES = mpi/collective/osu_iallgatherv.c
mpi_collective_osu_iallgatherv_LDADD = $(test_ldadd)

mpi_collective_osu_scatter_SOURCES = mpi/collective/osu_scatter.c
mpi_collective_osu_scatter_LDADD = $(test_ldadd)

mpi_collective_osu_iscatter_SOURCES = mpi/collective/osu_iscatter.c
mpi_collective_osu_iscatter_LDADD = $(test_ldadd)

mpi_collective_osu_iscatterv_SOURCES = mpi/collective/osu_iscatterv.c
mpi_collective_osu_iscatterv_LDADD = $(test_ldadd)

mpi_collective_osu_ireduce_SOURCES = mpi/collective/osu_ireduce.c
mpi_collective_osu_ireduce_LDADD = $(test_ldadd)

mpi_collective_osu_iallreduce_SOURCES = mpi/collective/osu_iallreduce.c
mpi_collective_osu_iallreduce_LDADD = $(test_ldadd)

mpi_one_sided_osu_put_latency_SOURCES = mpi/one-sided/osu_put_latency.c
mpi_one_sided_osu_put_latency_LDADD = $(test_ldadd)

mpi_one_sided_osu_put_bw_SOURCES = mpi/one-sided/osu_put_bw.c
mpi_one_sided_osu_put_bw_LDADD = $(test_ldadd)

mpi_one_sided_osu_put_bibw_SOURCES = mpi/one-sided/osu_put_bibw.c
mpi_one_sided_osu_put_bibw_LDADD = $(test_ldadd)

mpi_one_sided_osu_get_latency_SOURCES = mpi/one-sided/osu_get_latency.c
mpi_one_sided_osu_get_latency_LDADD = $(test_ldadd)

mpi_one_sided_osu_get_bw_SOURCES = mpi/one-sided/osu_get_bw.c
mpi_one_sided_osu_get_bw_LDADD = $(test_ldadd)

mpi_one_sided_osu_acc_latency_SOURCES = mpi/one-sided/osu_acc_latency.c
mpi_one_sided_osu_acc_latency_LDADD = $(test_ldadd)

mpi_one_sided_osu_fop_latency_SOURCES = mpi/one-sided/osu_fop_latency.c
mpi_one_sided_osu_fop_latency_LDADD = $(test_ldadd)

mpi_one_sided_osu_cas_latency_SOURCES = mpi/one-sided/osu_cas_latency.c
mpi_one_sided_osu_cas_latency_LDADD = $(test_ldadd)

mpi_one_sided_osu_get_acc_latency_SOURCES = mpi/one-sided/osu_get_acc_latency.c
mpi_one_sided_osu_get_acc_latency_LDADD = $(test_ldadd)

EXTRA_DIST = \
	README \
	COPYRIGHT \
	CHANGES
