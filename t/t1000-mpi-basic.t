#!/bin/sh

test_description='Test MPI bootstrap.

The tests run against whichever PMIx-capable MPI the tree was built
with.  Most apply to any MPI; the OMPI and MPICH prereqs mark the ones
that look for behavior particular to one.
'

. `dirname $0`/sharness.sh

MPI_VERSION=${FLUX_BUILD_DIR}/t/src/mpi_version
MPI_HELLO=${FLUX_BUILD_DIR}/t/src/mpi_hello
MPI_PINGPONG=${FLUX_BUILD_DIR}/t/src/mpi_pingpong

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

if ! ${MPI_VERSION} >/dev/null 2>&1; then
    skip_all='skipping MPI bootstrap tests - MPI unavailable'
    test_done
fi

test_under_flux 2

MPI_FLAVOR=$(${MPI_VERSION} --flavor)
case $MPI_FLAVOR in
    ompi) test_set_prereq OMPI ;;
    mpich) test_set_prereq MPICH ;;
esac

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
# ever gives each shell its own address.  MPICH's ch3:nemesis has no such
# rule.
if test $MPI_FLAVOR != ompi -o $(${MPI_VERSION} --major) -lt 6; then
	test_set_prereq MULTINODE_TCP
fi

test_expect_success 'MPI version is reported' '
	${MPI_VERSION} >version.out &&
	test_debug "cat version.out" &&
	test -s version.out
'

test_expect_success 'capture the job environment' '
	run_timeout 30 flux run \
		printenv >printenv.out
'

test_expect_success OMPI 'verify deprecated flux pmix/schizo plugins are not requested' '
	test_must_fail grep OMPI_MCA_pmix=flux printenv.out &&
	test_must_fail grep OMPI_MCA_schizo=flux printenv.out
'

test_expect_success 'sanity check pmix environment' '
	grep ^PMIX_NAMESPACE printenv.out
'

test_expect_success '1n1p hello (singleton init)' '
	run_timeout 30 flux run -N1 -n1 \
		${MPI_HELLO} >hello_1n1p.out &&
	grep "There are 1 tasks" hello_1n1p.out
'

test_expect_success '1n2p hello' '
	run_timeout 30 flux run -N1 -n2 \
		${MPI_HELLO} >hello_1n2p.out &&
	grep "There are 2 tasks" hello_1n2p.out
'

# MPICH builds its node map at init from the PMI-1 process mapping
# the server publishes as pmix.anlmap, when it fetches that key and
# the server provides it; otherwise it asks PMIx_Resolve_nodes() and
# PMIx_Resolve_peers() for each node, which the server's node realm
# answers.  -o pmix.nomap withholds the key so the second path is
# covered whichever one the MPI takes by default.
test_expect_success MULTINODE_TCP '2n2p hello' '
	run_timeout 30 flux run -N2 -n2 \
		${MPI_HELLO} >hello_2n2p.out &&
	grep "There are 2 tasks" hello_2n2p.out
'
test_expect_success MULTINODE_TCP '2n2p hello without the process mapping' '
	run_timeout 30 flux run -N2 -n2 -o pmix.nomap \
		${MPI_HELLO} >hello_2n2p_nomap.out &&
	grep "There are 2 tasks" hello_2n2p_nomap.out
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
test_expect_success '1n2p hello with --tasks-per-node' '
	run_timeout 30 flux run -N1 --tasks-per-node=2 \
		${MPI_HELLO} >hello_tpn_1n2p.out &&
	grep "There are 2 tasks" hello_tpn_1n2p.out
'

test_expect_success MULTINODE_TCP '2n4p hello with --tasks-per-node' '
	run_timeout 30 flux run -N2 --tasks-per-node=2 \
		${MPI_HELLO} >hello_tpn_2n4p.out &&
	grep "There are 4 tasks" hello_tpn_2n4p.out
'

test_expect_success '1n2p hello with --tasks-per-core' '
	run_timeout 30 flux run -N1 --tasks-per-core=2 --cores=1 \
		${MPI_HELLO} >hello_tpc_1n2p.out &&
	grep "There are 2 tasks" hello_tpc_1n2p.out
'

# see issue #27.  For MPICH, uneven placement also means the process
# mapping is a vector with more than one block.
test_expect_success MULTINODE_TCP '2n3p hello doesnt hang' '
	run_timeout 60 flux run -N2 -n3 \
		-overbose=2 \
		${MPI_HELLO} >hello_2n3p.out &&
	grep "There are 3 tasks" hello_2n3p.out
'

# see issue #26
test_expect_success OMPI,MULTINODE_TCP '2n4p hello reports no system call errors' '
	run_timeout 30 flux run -N2 -n4 \
		${MPI_HELLO} 2>2n4p_hello.err &&
	test_must_fail grep "System call:" 2n4p_hello.err
'

# MPICH exchanges business cards in one collecting fence at init.  The
# status at the end of the trace line is the server's spelling of
# success, PMIX_SUCCESS on openpmix 5.0 and SUCCESS before.
test_expect_success MPICH '2n4p business card fence completes' '
	run_timeout 30 flux run -N2 -n4 -overbose=2 \
		${MPI_HELLO} 2>fence.err >hello_fence.out &&
	grep "There are 4 tasks" hello_fence.out &&
	grep "completed pmix exchange 0: .*SUCCESS$" fence.err
'

test_expect_success '1n2p pingpong works' '
	run_timeout 30 flux run -N1 -n2 \
		${MPI_PINGPONG}
'
test_expect_success MULTINODE_TCP '2n2p pingpong works' '
	run_timeout 30 flux run -N2 -n2 \
		${MPI_PINGPONG}
'

# MPICH loads the topology the server shares with PMIx_Load_topology().
# That object belongs to the client library, which frees it in
# PMIx_Finalize() from openpmix 5.0.10 and 6.1 on.  MPICH 5.0.1 and
# later destroy it too, so on those clients every rank aborts at exit:
# with "free(): double free detected" when the client discovered the
# topology itself, and with an hwloc refcount assertion when it parsed
# the server's XML.  A rank that adopted the server's shared memory
# segment survives, which is why a single node job can pass.  Cover
# each way the topology can reach the client: shared (restricted or
# whole node), and not shared, where the client discovers the node.
test_expect_success MPICH 'mpich finalizes with the restricted topology' '
	run_timeout 30 flux run -N1 -n2 -o pmix.topo=restrict \
		${MPI_HELLO} >topo_restrict.out &&
	grep "completed MPI_Finalize" topo_restrict.out
'
test_expect_success MPICH 'mpich finalizes with the node topology' '
	run_timeout 30 flux run -N1 -n2 -o pmix.topo=node \
		${MPI_HELLO} >topo_node.out &&
	grep "completed MPI_Finalize" topo_node.out
'
test_expect_success MPICH 'mpich finalizes with no shared topology' '
	run_timeout 30 flux run -N1 -n2 -o pmix.topo=none \
		${MPI_HELLO} >topo_none.out &&
	grep "completed MPI_Finalize" topo_none.out
'

test_done
