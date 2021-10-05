#!/bin/sh

test_description='Exercise server direct_modex callback'

. `dirname $0`/sharness.sh

GETKEY=${FLUX_BUILD_DIR}/t/src/getkey

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

# Trigger a direct_modex callback to ensure plumbing works
test_expect_success '2n2p fetch a nonexistent key on another rank triggers direct_modex' '
	test_must_fail flux mini run -N2 -n2 \
		-overbose=2 \
		${GETKEY} --proc=0 --rank=1 rank0.nokey 2>rank0.nokey.err &&
	grep dmodex_upcall rank0.nokey.err
'

test_done
