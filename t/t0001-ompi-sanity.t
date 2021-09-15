#!/bin/sh

test_description='Sanity check ompi and prterun'

. `dirname $0`/sharness.sh

PRTERUN=$(which prterun)
if ! test -x ${PRTERUN}; then
	skip_all='skipping openmpi tests, prterun not available/configured'
	test_done
fi
PRTERUN="${PRTERUN} --map-by :OVERSUBSCRIBE"

MPI_HELLO=${FLUX_BUILD_DIR}/t/src/mpi_hello
MPI_VERSION=${FLUX_BUILD_DIR}/t/src/mpi_version
BARRIER=${FLUX_BUILD_DIR}/t/src/barrier
BIZCARD=${FLUX_BUILD_DIR}/t/src/bizcard

test_expect_success 'print openmpi version' '
	${MPI_VERSION}
'

test_expect_success 'prterun mpi_hello' '
	${PRTERUN} --n 8 ${MPI_HELLO}
'

test_expect_success 'prterun barrier' '
	${PRTERUN} --n 8 ${BARRIER}
'

test_expect_success 'prterun bizcard' '
	${PRTERUN} --n 8 ${BIZCARD} 1
'

test_done
