#!/bin/sh

test_description='Exercise empty fence (barrier).'

. `dirname $0`/sharness.sh

BARRIER=${FLUX_BUILD_DIR}/t/src/barrier
VERSION=${FLUX_BUILD_DIR}/t/src/version
export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

test_expect_success 'print pmix library version' '
	${VERSION}
'

# Single-shell barriers do not trigger fence server upcall,
# so the 1n2p tests are just checking openpmix behavior

test_expect_success '1n2p barrier works' '
	run_timeout 30 flux run -N1 -n2 \
		${BARRIER}
'

test_expect_success '1n2p barrier tolerates pmix.timeout=2' '
	run_timeout 30 flux run -N1 -n2 -overbose=2 \
		${BARRIER} --timeout=2
'

test_expect_success '1n2p barrier tolerates pmix.collect=false' '
	run_timeout 30 flux run -N1 -n2 \
		${BARRIER} --collect-data=false
'

# This works but it shouldn't per spec v5 sec 3.2.10
test_expect_success XFAIL '1n2p barrier rejects required unknown option' '
	test_must_fail run_timeout 30 flux run -N1 -n2 \
		${BARRIER} --unknown=+42
'

test_expect_success '1n2p barrier tolerates pmix.collect.gen=false' '
	run_timeout 30 flux run -N1 -n2 \
		${BARRIER} --collect-job-info=false
'

test_expect_success '1n2p barrier with procs subset works' '
	run_timeout 30 flux run -N1 -n2 \
		${BARRIER} --procs=0
'

# Multi-shell barrier

test_expect_success '2n2p barrier works' '
	run_timeout 30 flux run -N2 -n2 \
		-overbose=2 \
		${BARRIER}
'

test_expect_success '2n2p barrier tolerates optional pmix.timeout=2' '
	run_timeout 30 flux run -N2 -n2 \
		-overbose=2 \
		${BARRIER} --timeout=2
'

test_expect_success '2n2p barrier tolerates optional pmix.collect=false' '
	run_timeout 30 flux run -N2 -n2 \
		${BARRIER} --collect-data=false
'

test_expect_success '2n2p barrier tolerates optional pmix.collect.gen=false' '
	run_timeout 30 flux run -N2 -n2 \
		${BARRIER} --collect-job-info=false
'

# This works but it shouldn't per spec v5 sec 3.2.10
# N.B. PMIX_INFO_REQD flag is clear when it reaches our callback
test_expect_success XFAIL '2n2p barrier rejects required pmix.collect.gen=false' '
	test_must_fail run_timeout 30 flux run -N2 -n2 \
		-overbose=2 \
		${BARRIER} --collect-job-info=+false
'

test_expect_success '2n2p barrier tolerates optional unknown attr' '
	run_timeout 30 flux run -N2 -n2 \
		${BARRIER} --unknown=42
'

# N.B. This hangs in openpmix and we never get our fence upcall
test_expect_success XFAIL '2n2p barrier with procs subset fails' '
	test_must_fail run_timeout 30 flux run -N2 -n2 \
		-overbose=2 \
		${BARRIER} --procs=1
'

# This exercises the "subset" code path in our upcall, but only
# because it assumes non-wildcard proc entry is a subset
test_expect_success '2n2p barrier with procs explictly set fails' '
	test_must_fail run_timeout 30 flux run -N2 -n2 \
		-overbose=2 \
		${BARRIER} --procs=0-1
'

# Try a larger size for fun

test_expect_success '2n3p barrier works' '
	run_timeout 30 flux run -N2 -n3 \
		-overbose=2 \
		${BARRIER}
'

test_expect_success '2n4p barrier works' '
	run_timeout 30 flux run -N2 -n4 \
		-overbose=2 \
		${BARRIER}
'

# The server offers clients only the hash store.  The shared memory
# stores hand every client a connection blob packed in the server's
# own encoding, which a client that announced a different one cannot
# read.
#
# A client is made to speak v3 by leaving it only the v3 server URI
# variable.  Which variables a client looks for depends on its
# version (RFC 54 "Encoding Selection"), so every variable but
# PMIX_SERVER_URI3 is removed rather than a fixed list: unsetting
# only URI41 and URI4 leaves a v6 or later client its URI61, and it
# never drops to v3.
test_expect_success 'server offers only the hash store' '
	flux run -N1 -n1 printenv PMIX_GDS_MODULE >gds.out &&
	test_cmp - gds.out <<-EOT
	hash
	EOT
'

test_expect_success '1n2p barrier works with a v3 client' '
	cat >v3wrap.sh <<-EOT &&
	#!/bin/sh
	for v in \$(env | sed -n "s/^\\(PMIX_SERVER_URI[0-9]*\\)=.*/\\1/p"); do
	    case \$v in PMIX_SERVER_URI3) ;; *) unset \$v ;; esac
	done
	exec "\$@"
	EOT
	chmod +x v3wrap.sh &&
	run_timeout 30 flux run -N1 -n2 \
		./v3wrap.sh ${BARRIER}
'

test_done
