#!/bin/sh

test_description='Check that jobs use the flux-pmix in this tree

An installed flux-pmix would be loaded by the system shell initrc,
along with the openpmix it links, so test_under_flux replaces that
initrc with t/shell/initrc.lua, and the upmi plugin search path is set
to the tree for the same reason.  Check that both took, so that a
failure here, not puzzling ones later, says when they have not.'

. `dirname $0`/sharness.sh

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 1

test_expect_success 'the testsuite initrc is the job shell initrc' '
	test "$(flux getattr conf.shell_initrc)" = "$FLUX_TEST_SHELL_INITRC"
'
# PMIx_server_setup_fork() gives each task the server library version.
test_expect_success 'the server runs on the openpmix this tree is built with' '
	run_timeout 30 flux run printenv PMIX_VERSION >server.version &&
	test "$(cat server.version)" = "$OPENPMIX_SERVER_VERSION"
'
test_expect_success 'the shell loads exactly one pmix shell plugin' '
	run_timeout 30 flux run -o verbose=2 true 2>verbose.err &&
	test $(grep -c "server outsourced to" verbose.err) -eq 1
'

# The client side: libupmi loads its pmix plugin from
# FLUX_PMI_CLIENT_SEARCHPATH, set by 01-setup.sh.  Ask the loader which
# file it opened, since flux pmi does not say.
UPMI_PLUGIN=${FLUX_BUILD_DIR}/src/upmi/.libs/pmix.so
test_expect_success 'flux pmi loads the upmi plugin in this tree' '
	run_timeout 30 flux run -o pmi=pmix env LD_DEBUG=files \
		flux pmi --method=pmix barrier 2>ld.out &&
	grep "file=$UPMI_PLUGIN " ld.out &&
	test_must_fail grep "upmi/plugins/pmix.so" ld.out
'

test_done
