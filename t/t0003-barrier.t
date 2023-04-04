#!/bin/sh

test_description='Exercise empty fence (barrier).'

. `dirname $0`/sharness.sh

BARRIER=${FLUX_BUILD_DIR}/t/src/barrier
VERSION=${FLUX_BUILD_DIR}/t/src/version
export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

test_expect_success 'print pmix library version' '
	${VERSION}
'

# Single-shell barriers do not trigger fence server upcall,
# so the 1n2p tests are just checking openpmix behavior

test_expect_success '1n2p barrier works' '
	run_timeout 30 flux run -N1 -n2 \
		${BARRIER}
'

test_expect_success '1n2p barrier tolerates pmix.timeout=2' '
	run_timeout 30 flux run -N1 -n2 -overbose=2 \
		${BARRIER} --timeout=2
'

test_expect_success '1n2p barrier tolerates pmix.collect=false' '
	run_timeout 30 flux run -N1 -n2 \
		${BARRIER} --collect-data=false
'

# This works but it shouldn't per spec v5 sec 3.2.10
test_expect_success XFAIL '1n2p barrier rejects required unknown option' '
	test_must_fail run_timeout 30 flux run -N1 -n2 \
		${BARRIER} --unknown=+42
'

test_expect_success '1n2p barrier tolerates pmix.collect.gen=false' '
	run_timeout 30 flux run -N1 -n2 \
		${BARRIER} --collect-job-info=false
'

test_expect_success '1n2p barrier with procs subset works' '
	run_timeout 30 flux run -N1 -n2 \
		${BARRIER} --procs=0
'

# Multi-shell barrier

test_expect_success '2n2p barrier works' '
	run_timeout 30 flux run -N2 -n2 \
		-overbose=2 \
		${BARRIER}
'

test_expect_success '2n2p barrier tolerates optional pmix.timeout=2' '
	run_timeout 30 flux run -N2 -n2 \
		-overbose=2 \
		${BARRIER} --timeout=2
'

test_expect_success '2n2p barrier tolerates optional pmix.collect=false' '
	run_timeout 30 flux run -N2 -n2 \
		${BARRIER} --collect-data=false
'

test_expect_success '2n2p barrier tolerates optional pmix.collect.gen=false' '
	run_timeout 30 flux run -N2 -n2 \
		${BARRIER} --collect-job-info=false
'

# This works but it shouldn't per spec v5 sec 3.2.10
# N.B. PMIX_INFO_REQD flag is clear when it reaches our callback
test_expect_success XFAIL '2n2p barrier rejects required pmix.collect.gen=false' '
	test_must_fail run_timeout 30 flux run -N2 -n2 \
		-overbose=2 \
		${BARRIER} --collect-job-info=+false
'

test_expect_success '2n2p barrier tolerates optional unknown attr' '
	run_timeout 30 flux run -N2 -n2 \
		${BARRIER} --unknown=42
'

# N.B. This hangs in openpmix and we never get our fence upcall
test_expect_success XFAIL '2n2p barrier with procs subset fails' '
	test_must_fail run_timeout 30 flux run -N2 -n2 \
		-overbose=2 \
		${BARRIER} --procs=1
'

# This exercises the "subset" code path in our upcall, but only
# because it assumes non-wildcard proc entry is a subset
test_expect_success '2n2p barrier with procs explictly set fails' '
	test_must_fail run_timeout 30 flux run -N2 -n2 \
		-overbose=2 \
		${BARRIER} --procs=0-1
'

# Try a larger size for fun

test_expect_success '2n3p barrier works' '
	run_timeout 30 flux run -N2 -n3 \
		-overbose=2 \
		${BARRIER}
'

test_expect_success '2n4p barrier works' '
	run_timeout 30 flux run -N2 -n4 \
		-overbose=2 \
		${BARRIER}
'

test_done
