#!/bin/sh

test_description='Test that the pmix server is restricted to localhost'

. `dirname $0`/sharness.sh

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

# openpmix selects the listener interface from ptl MCA parameters, which
# may be set in ~/.pmix/mca-params.conf.  The shell runs as the user and
# reads that file, so the user could place the pmix server on a routable
# interface, which we consider unsafe.  The plugin pins the listener to
# loopback to prevent that.
IFACES=$(ip -o -4 addr show scope global 2>/dev/null \
	| awk '{print $2}' | sort -u | paste -sd,)
if test -z "$IFACES"; then
	skip_all='no non-loopback interface to test with'
	test_done
fi

HOME=${SHARNESS_TRASH_DIRECTORY:-$(pwd)}/pmixhome
mkdir -p ${HOME}/.pmix
echo "ptl_base_if_include = ${IFACES}" >${HOME}/.pmix/mca-params.conf
export HOME

test_under_flux 1

test_expect_success 'capture environment with plugin loaded' '
	flux run -overbose=2 printenv >env.out
'

test_expect_success 'server is listening on localhost' '
	grep PMIX_SERVER_URI env.out | cut -d= -f2 | sort | uniq >uri.out &&
	test $(wc -l <uri.out) -eq 1 &&
	grep tcp4://127.0.0.1: uri.out
'

test_done
