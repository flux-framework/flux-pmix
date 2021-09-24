#!/bin/sh

test_description='Exercise server error callback for event notification'

PLUGINPATH=${FLUX_BUILD_DIR}/src/shell/plugins/.libs

. `dirname $0`/sharness.sh

NOTIFY=${FLUX_BUILD_DIR}/t/src/notify

test_under_flux 2

test_expect_success 'create rc.lua script' "
	cat >rc.lua <<-EOT
	plugin.load (\"$PLUGINPATH/pmix.so\")
	EOT
"

# N.B. if these tests start hanging, perhaps openpmix has caught up
# with the spec (see comment in plugin notify.c)
test_expect_success '1n1p event notify triggers warning on stderr' '
	run_timeout 30 flux mini run \
		-ouserrc=$(pwd)/rc.lua \
		${NOTIFY} --status=69 2>warn.err &&
	grep event-status=69 warn.err
'

test_expect_success '1n1p event notify with message works' '
	run_timeout 30 flux mini run \
		-ouserrc=$(pwd)/rc.lua \
		${NOTIFY} --status=69 --message="lorem ipsum" 2>message.err &&
	grep "lorem ipsum" message.err
'

test_done
