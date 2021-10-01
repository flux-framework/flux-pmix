#!/bin/sh

test_description='Test openmpi with mpibench.'

PLUGINPATH=${FLUX_BUILD_DIR}/src/shell/plugins/.libs
MPIBENCH="${FLUX_BUILD_DIR}/t/mpibench/mpibench -i5 -C -e1K -m 1M"

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

. `dirname $0`/sharness.sh


if ! test_have_prereq LONGTEST; then
    skip_all='skipping mpibench due to missing LONGTEST prereq'
    test_done
fi

test_under_flux 2

cat >tests.dat<<EOT
60 1 2 Barrier
60 2 3 Barrier
60 2 4 Barrier
60 1 2 Bcast
60 2 3 Bcast
60 2 4 Bcast
60 1 2 Alltoall
60 2 3 Alltoall
60 2 4 Alltoall
60 1 2 Allgather
60 2 3 Allgather
60 2 4 Allgather
60 1 2 Gather
60 2 3 Gather
60 2 4 Gather
60 1 2 Scatter
60 2 3 Scatter
60 2 4 Scatter
60 1 2 Allreduce
60 2 3 Allreduce
60 2 4 Allreduce
60 1 2 Reduce
60 2 3 Reduce
60 2 4 Reduce
EOT

run_mpibench() {
	local timeout=$1
	local nnodes=$2
	local ntasks=$3
	local op=$4
	run_timeout $timeout flux mini run -N$nnodes -n$ntasks \
		${MPIBENCH} ${op}
}


while read timeout nnodes ntasks op; do
        testname="${nnodes}n${ntasks}p mpibench ${op}"
        if test $timeout -lt 0; then
                prereq=XFAIL
        else
                prereq=""
        fi
        test_expect_success $prereq "$testname" \
                "run_mpibench $timeout $nnodes $ntasks $op"
done <tests.dat

test_done
