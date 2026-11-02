#!/bin/sh

test_description='Test openmpi bootstrap.'

. `dirname $0`/sharness.sh

MPI_VERSION=${FLUX_BUILD_DIR}/t/src/mpi_version
MPI_HELLO=${FLUX_BUILD_DIR}/t/src/mpi_hello
MPI_PINGPONG=${FLUX_BUILD_DIR}/t/src/mpi_pingpong

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

if ! ${MPI_VERSION} >/dev/null 2>&1; then
    skip_all='skipping openmpi bootstrap tests - MPI unavailable'
    test_done
fi

test_under_flux 2

# A flux job runs one shell per node, each hosting its own pmix server, so
# two shells on one host are two pmix nodes that happen to share a network
# stack.  Open MPI v6 drops any peer address that is also configured
# locally, since on a real cluster such an address cannot reach another
# node (see ompi commit cf1642e018).  Here it can - the peer really is on
# this host - but ompi has been told the peer is remote, so the rule
# applies and no tcp pairing survives.  Nothing in the job is wrong: the
# configuration is only reachable when several shells share a host, which
# is how the testsuite simulates a multi-node job.
#
# Skip the multi-node cases there rather than assert the old behavior.
# Single node cases are unaffected - the peer is local, the rule does not
# apply - and the multi-node cases return on their own if the simulation
# ever gives each shell its own address.
if test $(${MPI_VERSION} --major) -lt 6; then
	test_set_prereq MULTINODE_TCP
fi

test_expect_success 'capture the job environment' '
	run_timeout 30 flux run \
		printenv >printenv.out
'

test_expect_success 'verify deprecated flux pmix/schizo plugins are not requested' '
	test_must_fail grep OMPI_MCA_pmix=flux printenv.out &&
	test_must_fail grep OMPI_MCA_schizo=flux printenv.out
'

test_expect_success 'sanity check pmix environment' '
	grep ^PMIX_NAMESPACE printenv.out
'

test_expect_success '1n2p ompi hello' '
	run_timeout 30 flux run -N1 -n2 \
		${MPI_HELLO} >hello_1n2p.out &&
	grep "There are 2 tasks" hello_1n2p.out
'

test_expect_success MULTINODE_TCP '2n2p ompi hello' '
	run_timeout 30 flux run -N2 -n2 \
		${MPI_HELLO} >hello_2n2p.out &&
	grep "There are 2 tasks" hello_2n2p.out
'

# Useful debugging runes:
# --env=PMIX_MCA_pmix_client_fence_verbose=100 \
# --env=OMPI_MCA_shmem_base_verbose=100 \
# --env=OMPI_MCA_btl_base_verbose=100 \
# --env=OMPI_MCA_btl_tcp_if_exclude=docker0,lo \

# see issue #130
# per-resource task placement (--tasks-per-node, --tasks-per-core) expands
# the task count via a shell option rather than the jobspec, so the pmix
# plugin must source the job size from shell info, not jobspec info.
test_expect_success '1n2p ompi hello with --tasks-per-node' '
	run_timeout 30 flux run -N1 --tasks-per-node=2 \
		${MPI_HELLO} >hello_tpn_1n2p.out &&
	grep "There are 2 tasks" hello_tpn_1n2p.out
'

test_expect_success MULTINODE_TCP '2n4p ompi hello with --tasks-per-node' '
	run_timeout 30 flux run -N2 --tasks-per-node=2 \
		${MPI_HELLO} >hello_tpn_2n4p.out &&
	grep "There are 4 tasks" hello_tpn_2n4p.out
'

test_expect_success '1n2p ompi hello with --tasks-per-core' '
	run_timeout 30 flux run -N1 --tasks-per-core=2 --cores=1 \
		${MPI_HELLO} >hello_tpc_1n2p.out &&
	grep "There are 2 tasks" hello_tpc_1n2p.out
'

# see issue #27
test_expect_success MULTINODE_TCP '2n3p ompi hello doesnt hang' '
	run_timeout 60 flux run -N2 -n3 \
		-overbose=2 \
		${MPI_HELLO} >hello_2n3p.out &&
	grep "There are 3 tasks" hello_2n3p.out
'

# see issue #26
test_expect_success MULTINODE_TCP '2n4p ompi hello reports no system call errors' '
        run_timeout 30 flux run -N2 -n4 \
                ${MPI_HELLO} 2>2n4p_hello.err &&
        test_must_fail grep "System call:" 2n4p_hello.err
'

test_expect_success '1n2p ompi pingpong works' '
        run_timeout 30 flux run -N1 -n2 \
                ${MPI_PINGPONG}
'
test_expect_success MULTINODE_TCP '2n2p ompi pingpong works' '
        run_timeout 30 flux run -N2 -n2 \
                ${MPI_PINGPONG}
'

test_done
