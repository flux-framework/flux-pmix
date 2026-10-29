#!/bin/sh

test_description='Check the node topology the server shares with clients

The server hands the hwloc topology held by the shell to openpmix to
share, so a client gets the node without discovering it.  By default
the topology is restricted to the cores allocated to the job on the
node.

The topo tool reports what a client gets: the number of cores, whether
hwloc will bind against it (thissystem), how it was obtained (shmem,
xml, or self for its own discovery), and the binding of the calling
process.  A client adopts the shared memory segment when the address
the server chose is free in the client and parses the XML otherwise,
so the test accepts either.
'

. `dirname $0`/sharness.sh

TOPO=${FLUX_BUILD_DIR}/t/src/topo

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 1

GETKEY=${FLUX_BUILD_DIR}/t/src/getkey

# Restriction is to cores, which R allocates; a core may have more
# than one PU.  The node must have more than one core for a restricted
# topology to differ from the whole node.
NCORES=$(flux resource list -no {ncores})
if test "$NCORES" -gt 1; then
	test_set_prereq MULTICORE
fi

# The topo tool prints one line per rank, e.g.
#   0: source=hwloc ncores=1 npus=2 cpuset=5-6 thissystem=1 via=xml bound=5
# field VAR NAME extracts NAME from the rank 0 line of file VAR.
field() {
	sed -n "s/^0: .*$2=\([^ ]*\).*/\1/p" $1
}

test_expect_success 'client gets the shared topology' '
	flux run -n1 ${TOPO} >default.out &&
	test_debug "cat default.out" &&
	via=$(field default.out via) &&
	test "$via" = shmem -o "$via" = xml
'
test_expect_success 'shared topology is bindable' '
	test "$(field default.out thissystem)" = 1
'
test_expect_success 'shared topology source is hwloc' '
	case "$(field default.out source)" in hwloc*) true;; *) false;; esac
'
test_expect_success 'HWLOC_XMLFILE is not set in the job environment' '
	test_must_fail flux run -n1 printenv HWLOC_XMLFILE
'
test_expect_success MULTICORE 'shared topology is restricted to the job' '
	flux run -n1 -c1 ${TOPO} >restrict.out &&
	test_debug "cat restrict.out" &&
	test "$(field restrict.out ncores)" = 1
'
test_expect_success MULTICORE 'pmix.topo=restrict is the default' '
	flux run -n1 -c1 -o pmix.topo=restrict ${TOPO} >explicit.out &&
	test "$(field explicit.out ncores)" = 1
'
test_expect_success 'pmix.topo=node shares the whole node' '
	flux run -n1 -c1 -o pmix.topo=node ${TOPO} >node.out &&
	test_debug "cat node.out" &&
	test "$(field node.out ncores)" = $NCORES
'
test_expect_success MULTICORE 'restriction does not depend on cpu-affinity' '
	flux run -n1 -c1 -o cpu-affinity=off ${TOPO} >unbound.out &&
	test_debug "cat unbound.out" &&
	test "$(field unbound.out ncores)" = 1
'
test_expect_success MULTICORE 'per-task binding is visible in the shared topology' '
	flux run -n2 -o cpu-affinity=per-task ${TOPO} | sort >pertask.out &&
	test_debug "cat pertask.out" &&
	b0=$(sed -n "s/^0: .*bound=\([^ ]*\).*/\1/p" pertask.out) &&
	b1=$(sed -n "s/^1: .*bound=\([^ ]*\).*/\1/p" pertask.out) &&
	test "$b0" != unbound && test "$b1" != unbound && test "$b0" != "$b1"
'
test_expect_success 'pmix.topo=none shares nothing' '
	flux run -n1 -o pmix.topo=none ${TOPO} >none.out &&
	test_debug "cat none.out" &&
	test "$(field none.out via)" = self
'
# What the server published, as a 3.2-era client (Open MPI 4.1) reads it:
# the XML string, and the shared memory segment under the job tmpdir.
test_expect_success 'server publishes the topology XML' '
	flux run -n1 ${GETKEY} --proc=* pmix.hwlocxml2 >xml.out &&
	grep -q "<object type=\"Machine\"" xml.out
'
test_expect_success 'server publishes the shared memory segment' '
	flux run -n1 ${GETKEY} --proc=* pmix.hwlocfile >file.out &&
	grep -q /hwloc.sm file.out
'
test_expect_success 'pmix.topo=none publishes nothing' '
	test_must_fail flux run -n1 -o pmix.topo=none \
		${GETKEY} --proc=* pmix.hwlocxml2
'
test_expect_success 'pmix.topo rejects an unknown value' '
	test_must_fail flux run -n1 -o pmix.topo=foo true 2>topo.err &&
	grep "expected restrict, node, or none" topo.err
'
test_expect_success 'pmix.topo rejects a non-string value' '
	test_must_fail flux run -n1 -o pmix.topo=42 true 2>type.err &&
	grep "must be a string" type.err
'
test_done
