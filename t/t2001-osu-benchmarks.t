#!/bin/sh

test_description='Test openmpi with OSU micro benchmarks.'

OSU_MPI=${FLUX_BUILD_DIR}/t/osu-micro-benchmarks/mpi

. `dirname $0`/sharness.sh

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

if ! test_have_prereq LONGTEST; then
    skip_all='skipping OSU micro benchmarks due to missing LONGTEST prereq'
    test_done
fi

test_under_flux 2

cat >tests.dat<<EOT
120 2 4 startup/osu_init
120 2 4 startup/osu_hello
300 1 2 pt2pt/osu_latency
300 2 2 pt2pt/osu_latency
300 2 2 pt2pt/osu_latency_mp
300 2 2 pt2pt/osu_latency_mt
300 2 4 pt2pt/osu_multi_lat
300 2 2 pt2pt/osu_bw
300 2 2 pt2pt/osu_bibw
300 2 2 pt2pt/osu_mbw_mr
300 2 4 collective/osu_allgather
300 2 4 collective/osu_allgatherv
300 2 4 collective/osu_allreduce
300 2 3 collective/osu_alltoall
300 2 4 collective/osu_alltoall
300 2 4 collective/osu_alltoallv
300 2 4 collective/osu_barrier
300 2 4 collective/osu_bcast
300 2 4 collective/osu_gather
300 2 4 collective/osu_gatherv
300 2 4 collective/osu_reduce
300 2 4 collective/osu_reduce_scatter
300 2 4 collective/osu_scatter
300 2 4 collective/osu_scatterv
300 2 4 collective/osu_iallgather
300 2 4 collective/osu_iallgatherv
300 2 4 collective/osu_iallreduce
300 2 4 collective/osu_ialltoall
300 2 4 collective/osu_ialltoallv
300 2 4 collective/osu_ialltoallw
300 2 4 collective/osu_ibarrier
300 2 4 collective/osu_ibcast
300 2 4 collective/osu_igather
300 2 4 collective/osu_igatherv
300 2 4 collective/osu_ireduce
300 2 4 collective/osu_iscatter
300 2 4 collective/osu_iscatterv
300 2 2 one-sided/osu_put_latency
300 2 2 one-sided/osu_put_bw
300 2 2 one-sided/osu_put_bibw
300 2 2 one-sided/osu_get_latency
300 2 2 one-sided/osu_get_bw
300 2 2 one-sided/osu_acc_latency
300 2 2 one-sided/osu_fop_latency
300 2 2 one-sided/osu_cas_latency
-1 2 2 one-sided/osu_get_acc_latency
EOT

cat >slowtests.dat<<EOT2
EOT2

run_osutest() {
	local timeout=$1
	local nnodes=$2
	local ntasks=$3
	local cmd=$4
	run_timeout $timeout flux mini run -N$nnodes -n$ntasks \
		${OSU_MPI}/${cmd}
}

while read timeout nnodes ntasks cmd; do
	testname="${nnodes}n${ntasks}p ${cmd}"
	if test $timeout -lt 0; then
		prereq=XFAIL
	else
		prereq=""
	fi
	test_expect_success $prereq "$testname" \
		"run_osutest $timeout $nnodes $ntasks $cmd"
done <tests.dat

test_done
