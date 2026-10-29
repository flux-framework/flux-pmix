#!/bin/sh

test_description='Test that MPI_Abort works.'

. `dirname $0`/sharness.sh

MPI_VERSION=${FLUX_BUILD_DIR}/t/src/mpi_version
MPI_ABORT=${FLUX_BUILD_DIR}/t/src/mpi_abort

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

if ! ${MPI_VERSION} >/dev/null 2>&1; then
    skip_all='skipping openmpi abort tests - MPI unavailable'
    test_done
fi

test_under_flux 2

# See the comment in t1000-ompi-basic.t for why the multi-node cases do
# not run on ompi v6 and later.
if test $(${MPI_VERSION} --major) -lt 6; then
	test_set_prereq MULTINODE_TCP
fi

# Open MPI reaches the pmix server the long way around.  MPI_Abort()
# calls ompi_mpi_abort(), which builds the peer list and hands it to
# ompi_rte_abort_peers() - a stub that discards it and returns - so the
# code falls through to ompi_rte_abort(), which calls PMIx_Abort() with
# no procs.  openpmix hands that to the server's abort callback, which is
# ours, and the plugin raises a fatal job exception.  v4 arrives by a
# different road (opal_pmix.abort in the rte/pmix component) but ends in
# the same callback.
#
# t0005-abort.t calls PMIx_Abort() directly and can vary its arguments.
# These tests cover the rest of the path: that a real MPI's MPI_Abort()
# still reaches the callback at all.  That route is Open MPI's to change,
# and this is the only place a change in it would be noticed.

# The job exit code is not the abort status.  The plugin does not
# propagate 'status' to the exit code (see src/shell/plugins/abort.c),
# and the job may exit with 128 + SIGTERM once the shells are cleaned
# up.  Use ! rather than test_expect_code, and not test_must_fail, which
# does not accept 128 + signum as failure.

# Get a pmix server trace into the CI output in case it is needed to
# debug a failure here.
test_expect_success '1n1p MPI_Abort with pmix server tracing enabled' '
	! run_timeout 60 flux run -N1 -n1 -overbose=2 \
		${MPI_ABORT} 0 2>abort_1n1p.err &&
	grep "called PMIx_Abort" abort_1n1p.err
'

test_expect_success '1n2p MPI_Abort on rank 0 raises a job exception' '
	! run_timeout 60 flux run -N1 -n2 \
		${MPI_ABORT} 0 2>abort_rank0.err &&
	grep "Rank 0 is going to MPI_Abort now" abort_rank0.err &&
	grep "exception" abort_rank0.err
'

test_expect_success '1n2p exception names the aborting rank and status' '
	grep "\.0 called PMIx_Abort (status=42)" abort_rank0.err
'

test_expect_success '1n2p MPI_Abort on rank 1 raises a job exception' '
	! run_timeout 60 flux run -N1 -n2 \
		${MPI_ABORT} 1 2>abort_rank1.err &&
	grep "Rank 1 is going to MPI_Abort now" abort_rank1.err &&
	grep "\.1 called PMIx_Abort (status=42)" abort_rank1.err
'

test_expect_success MULTINODE_TCP '2n4p MPI_Abort on the last rank works' '
	! run_timeout 60 flux run -N2 -n4 \
		${MPI_ABORT} 3 2>abort_2n4p.err &&
	grep "Rank 3 is going to MPI_Abort now" abort_2n4p.err &&
	grep "\.3 called PMIx_Abort (status=42)" abort_2n4p.err
'

test_done
