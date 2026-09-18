#!/bin/sh

test_description='Exercise empty alloc.'

. `dirname $0`/sharness.sh

ALLOC=${FLUX_BUILD_DIR}/t/src/alloc
VERSION=${FLUX_BUILD_DIR}/t/src/version
export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

test_expect_success 'print pmix library version' '
	${VERSION}
'

# Single-shell barriers do not trigger fence server upcall,
# so the 1n2p tests are just checking openpmix behavior

test_expect_success 'alloc works' '
	run_timeout 30 flux run -o verbose=3 \
		${ALLOC}
'

test_done
