#!/bin/sh

test_description='Exercise handler for PMIx_Abort'

. `dirname $0`/sharness.sh

ABORT=${FLUX_BUILD_DIR}/t/src/abort

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

test_expect_success '1n1p abort on rank 0 works' '
	! run_timeout 60 flux mini run \
		${ABORT} --status=42 --rank=0 --message=abort-test 2>abort.err
'

test_expect_success 'stderr contains abort message and exit code' '
	grep abort-test abort.err &&
	grep 42 abort.err
'

test_expect_success '1n2p abort on rank 1 works' '
	! run_timeout 60 flux mini run -n2 \
		${ABORT} --status=42 --rank=1 --message=abort-test
'

test_expect_success '2n2p abort on rank 1 works' '
	! run_timeout 60 flux mini run -N2 -n2 \
		${ABORT} --status=42 --rank=1 --message=abort-test
'

test_expect_success '1n1p abort on rank 0 works with no message' '
	! run_timeout 60 flux mini run \
		${ABORT} --status=42 --rank=0
'

test_done
