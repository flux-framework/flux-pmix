#!/bin/sh

test_description='Test the openpmix version check'

. `dirname $0`/sharness.sh

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 1

# The check compares the openpmix the plugin loads with the one it was
# built against.  Whether they match depends on the environment, so
# these tests only check that the option is handled and the runtime
# version is understood.
test_expect_success 'the runtime openpmix version is parsed' '
	flux run -N1 -n1 -o verbose=1 true 2>version.err &&
	test_must_fail grep "cannot parse openpmix version" version.err
'
test_expect_success 'pmix.version-check=off is accepted' '
	flux run -N1 -n1 -o pmix.version-check=off true
'
test_expect_success 'pmix.version-check=on is accepted' '
	flux run -N1 -n1 -o pmix.version-check=on true
'
test_expect_success 'pmix.version-check rejects other values' '
	test_must_fail flux run -N1 -n1 -o pmix.version-check=maybe true \
		2>version-bad.err &&
	grep "must be \"on\" or \"off\"" version-bad.err
'

# Test a major mismatch by running the plugin on the client openpmix,
# when the build has two majors.  LD_LIBRARY_PATH in the job shell
# redirects the plugin if its toolchain emitted RUNPATH, not RPATH.
# Ask ldd, which follows the loader's rules, instead of guessing.
#
# ldd also names the client library.  Versions look like "3.2.5a1"
# or "OpenPMIx 5.0.7a1 (...)".
VERSION=${FLUX_BUILD_DIR}/t/src/version
PLUGIN=${FLUX_BUILD_DIR}/src/shell/plugins/.libs/pmix.so
pmix_lib_of() {
	ldd "$1" 2>/dev/null | sed -n 's/.*libpmix\.so\.2 => \([^ ]*\).*/\1/p'
}
client_version=$(${VERSION} | sed -n 's/^\(OpenPMIx \)\{0,1\}\([0-9.]*\).*/\2/p')
client_major=${client_version%%.*}
server_major=${OPENPMIX_SERVER_VERSION%%.*}
client_lib=$(pmix_lib_of ${VERSION})
client_libdir=$(dirname "${client_lib:-/}")
# Record the decision in the CI log, so a skip is not taken for coverage.
diag() {
	echo "# $*"
}
diag "openpmix server $OPENPMIX_SERVER_VERSION, client $client_version"
if test -z "$server_major" -o -z "$client_major" -o ! -f "$client_lib"; then
	diag "cannot identify both openpmix libraries: mismatch not tested"
elif test "$server_major" = "$client_major"; then
	diag "both openpmix are major $server_major: mismatch not tested"
elif test "$(LD_LIBRARY_PATH=$client_libdir pmix_lib_of ${PLUGIN})" \
	!= "$client_lib"; then
	diag "LD_LIBRARY_PATH does not redirect the plugin: mismatch not tested"
else
	diag "testing the plugin with the client openpmix"
	test_set_prereq CROSS_MAJOR
	# An older library may lack what the plugin calls, so only a
	# newer one is run.
	if test "$server_major" -lt "$client_major"; then
		test_set_prereq CROSS_MAJOR_NEWER_LIB
	fi
fi

test_expect_success CROSS_MAJOR 'create a job shell with the client openpmix' '
	cat >shell.sh <<-EOT &&
	#!/bin/sh
	LD_LIBRARY_PATH=$client_libdir exec $(flux config builtin shell_path) "\$@"
	EOT
	chmod +x shell.sh
'
test_expect_success CROSS_MAJOR 'a major version mismatch is an error' '
	test_must_fail flux run -N1 -n1 -Sexec.job_shell=$(pwd)/shell.sh \
		true 2>mismatch.err &&
	grep "the major versions must match" mismatch.err
'
test_expect_success CROSS_MAJOR 'the server was the client openpmix' '
	grep "running $client_version:" mismatch.err
'
test_expect_success CROSS_MAJOR_NEWER_LIB 'pmix.version-check=off runs anyway' '
	flux run -N1 -n1 -Sexec.job_shell=$(pwd)/shell.sh \
		-o pmix.version-check=off -o verbose=2 true 2>off.err &&
	grep "outsourced to.*$client_version" off.err
'

test_done
