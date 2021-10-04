#!/bin/sh

test_description='Exercise the business card exchange use case.'

. `dirname $0`/sharness.sh

BIZCARD=${FLUX_BUILD_DIR}/t/src/bizcard

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

test_expect_success 'print pmix library version' '
	${VERSION}
'

test_expect_success '1n2p bizcard exchange works' '
       run_timeout 30 flux mini run -N1 -n2 \
               ${BIZCARD} 1
'

test_expect_success '2n2p bizcard exchange works' '
       run_timeout 30 flux mini run -N2 -n2 \
	       -overbose=2 \
               ${BIZCARD} 1
'

test_expect_success '2n3p bizcard exchange works' '
       run_timeout 30 flux mini run -N2 -n3 \
	       -overbose=2 \
               ${BIZCARD} 1
'

test_expect_success '2n4p bizcard exchange works' '
       run_timeout 30 flux mini run -N2 -n4 \
               ${BIZCARD} 1
'

test_done
