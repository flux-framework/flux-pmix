#!/bin/sh

test_description='Check out libupmi pmix plugin.'

. `dirname $0`/sharness.sh

VERSION=${FLUX_BUILD_DIR}/t/src/version
export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

# N.B. this also affects the test_under_flux broker bootstrap,
# so be sure 'simple' comes first so the broker finds flux-start's pmi server
export FLUX_PMI_CLIENT_METHODS="simple pmix libpmi single"
export FLUX_PMI_CLIENT_SEARCHPATH=${FLUX_BUILD_DIR}/src/upmi/.libs

test_under_flux 2

# broker.boot-method was added with upmi dso plugin support
if ! flux getattr broker.boot-method; then
    skip_all='skipping upmi tests - flux lacks external upmi plugin support'
    test_done
fi

test_expect_success 'print pmix library version' '
	${VERSION}
'

test_expect_success '1n2p barrier works and selected pmix' '
	run_timeout 30 flux mini run -opmi=off -N1 -n2 --label-io \
		flux pmi -v barrier 2>1n2p-barrier.err &&
	grep "1: pmix: barrier: success" 1n2p-barrier.err
'

test_expect_success '2n2p barrier works' '
	run_timeout 30 flux mini run -opmi=off -N2 -n2 --label-io \
		flux pmi -v barrier
'

test_expect_success '2n3p barrier works' '
	run_timeout 30 flux mini run -opmi=off -N2 -n3 --label-io \
		flux pmi -v barrier
'

test_expect_success '2n4p barrier works' '
	run_timeout 30 flux mini run -opmi=off -N2 -n4 --label-io \
		flux pmi -v barrier
'

test_expect_success '1n2p exchange works' '
	run_timeout 30 flux mini run -opmi=off -N1 -n2 --label-io \
		flux pmi -v exchange
'

test_expect_success '2n4p exchange works' '
	run_timeout 30 flux mini run -opmi=off -N2 -n4 --label-io \
		flux pmi -v exchange
'
test_expect_success 'get of unknown key fails' '
	test_must_fail flux mini run -opmi=off \
		flux pmi -v get notakey
'
test_expect_success 'flux launches flux with pmix' '
	cat >method.exp <<-EOT &&
	pmix
	EOT
	flux mini run -opmi=off \
		flux start flux getattr broker.boot-method >method.out &&
	test_cmp method.exp method.out
'

test_done
