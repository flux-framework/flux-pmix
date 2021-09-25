#!/bin/sh

test_description='Exercise server direct_modex callback'

PLUGINPATH=${FLUX_BUILD_DIR}/src/shell/plugins/.libs

. `dirname $0`/sharness.sh

GETKEY=${FLUX_BUILD_DIR}/t/src/getkey

test_under_flux 2

test_expect_success 'create rc.lua script' "
	cat >rc.lua <<-EOT
	plugin.load (\"$PLUGINPATH/pmix.so\")
	EOT
"

# Trigger a direct_modex callback to ensure plumbing works
test_expect_success '2n2p fetch a nonexistent key on another rank triggers direct_modex' '
	test_must_fail flux mini run -N2 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		-overbose=2 \
		${GETKEY} --proc=0 --rank=1 rank0.nokey 2>rank0.nokey.err &&
	grep dmodex_upcall rank0.nokey.err
'

test_done
