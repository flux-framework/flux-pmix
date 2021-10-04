#!/bin/sh

test_description='Exercise server error callback for event notification'

. `dirname $0`/sharness.sh

NOTIFY=${FLUX_BUILD_DIR}/t/src/notify

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

# N.B. if these tests start hanging, perhaps openpmix has caught up
# with the spec (see comment in plugin notify.c)
test_expect_success '1n1p event notify triggers warning on stderr' '
	run_timeout 30 flux mini run \
		${NOTIFY} --status=69 2>warn.err &&
	grep event-status=69 warn.err
'

test_expect_success '1n1p event notify with message works' '
	run_timeout 30 flux mini run \
		${NOTIFY} --status=69 --message="lorem ipsum" 2>message.err &&
	grep "lorem ipsum" message.err
'

test_done
