#!/bin/sh

test_description='Exercise the business card exchange use case.'

PLUGINPATH=${FLUX_BUILD_DIR}/src/shell/plugins/.libs

. `dirname $0`/sharness.sh

BARRIER=${FLUX_BUILD_DIR}/t/src/barrier
BIZCARD=${FLUX_BUILD_DIR}/t/src/bizcard

test_under_flux 2

test_expect_success 'print pmix library version' '
	${VERSION}
'

test_expect_success 'create rc.lua script' "
	cat >rc.lua <<-EOT
	plugin.load (\"$PLUGINPATH/pmix.so\")
	EOT
"

test_expect_success '1n2p barrier works' '
	run_timeout 30 flux mini run -N1 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		${BARRIER}
'

test_expect_success '1n2p bizcard exchange works' '
       run_timeout 30 flux mini run -N1 -n2 \
               -ouserrc=$(pwd)/rc.lua \
               ${BIZCARD} 1
'


test_done
