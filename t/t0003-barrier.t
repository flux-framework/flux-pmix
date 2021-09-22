#!/bin/sh

test_description='Exercise empty fence (barrier).'

PLUGINPATH=${FLUX_BUILD_DIR}/src/shell/plugins/.libs

. `dirname $0`/sharness.sh

BARRIER=${FLUX_BUILD_DIR}/t/src/barrier

test_under_flux 2

test_expect_success 'print pmix library version' '
	${VERSION}
'

test_expect_success 'create rc.lua script' "
	cat >rc.lua <<-EOT
	plugin.load (\"$PLUGINPATH/pmix.so\")
	EOT
"

# Single-shell barriers do not trigger fence server upcall,
# so the 1n2p tests are just checking openpmix behavior

test_expect_success '1n2p barrier works' '
	run_timeout 30 flux mini run -N1 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER}
'

test_expect_success '1n2p barrier tolerates pmix.timeout=2' '
	run_timeout 30 flux mini run -N1 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER} --timeout=2s
'

test_expect_success '1n2p barrier tolerates pmix.collect=false' '
	run_timeout 30 flux mini run -N1 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER} --collect-data=false
'

# This works but it shouldn't per spec v5 sec 3.2.10
test_expect_success XFAIL '1n2p barrier rejects required unknown option' '
	test_must_fail run_timeout 30 flux mini run -N1 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER} --unknown=+42
'

test_expect_success '1n2p barrier tolerates pmix.collect.gen=false' '
	run_timeout 30 flux mini run -N1 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER} --collect-job-info=false
'

test_expect_success '1n2p barrier with procs subset works' '
	run_timeout 30 flux mini run -N1 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER} --procs=0
'

# Multi-shell barrier

test_expect_success '2n2p barrier works' '
	run_timeout 30 flux mini run -N2 -n2 \
		-overbose=2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER}
'

test_expect_success '2n2p barrier tolerates optional pmix.timeout=2' '
	run_timeout 30 flux mini run -N2 -n2 \
		-overbose=2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER} --timeout=2s
'

test_expect_success '2n2p barrier tolerates optional pmix.collect=false' '
	run_timeout 30 flux mini run -N2 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER} --collect-data=false
'

test_expect_success '2n2p barrier tolerates optional pmix.collect.gen=false' '
	run_timeout 30 flux mini run -N2 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER} --collect-job-info=false
'

# This works but it shouldn't per spec v5 sec 3.2.10
# N.B. PMIX_INFO_REQD flag is clear when it reaches our callback
test_expect_success XFAIL '2n2p barrier rejects required pmix.collect.gen=false' '
	test_must_fail run_timeout 30 flux mini run -N2 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		-overbose=2 \
		${BARRIER} --collect-job-info=+false
'

test_expect_success '2n2p barrier tolerates optional unknown attr' '
	run_timeout 30 flux mini run -N2 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER} --unknown=42
'

# N.B. This hangs in openpmix and we never get our fence upcall
test_expect_success XFAIL '2n2p barrier with procs subset fails' '
	test_must_fail run_timeout 30 flux mini run -N2 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		-overbose=2 \
		${BARRIER} --procs=1
'

# This exercises the "subset" code path in our upcall, but only
# because it assumes non-wildcard proc entry is a subset
test_expect_success '2n2p barrier with procs explictly set fails' '
	test_must_fail run_timeout 30 flux mini run -N2 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		-overbose=2 \
		${BARRIER} --procs=0-1
'

# Try a larger size for fun

test_expect_success '2n4p barrier works' '
	run_timeout 30 flux mini run -N2 -n4 \
		-overbose=2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER}
'

test_done
