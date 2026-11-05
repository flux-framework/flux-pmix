#!/bin/sh

test_description='Exercise the server query callback

PMIx_Query_info (Standard 7.1) asks the server for a list of keys.
The server answers what it can and hands the rest to the host
(17.3.18).  The host answers no key yet, so a key that reaches it is
PMIX_ERR_NOT_FOUND, which 7.1.2 says is the status when none of the
requested data was available.  The keys asked for here are ones no
client has, so each query reaches the host.
'

. `dirname $0`/sharness.sh

QUERY=${FLUX_BUILD_DIR}/t/src/query

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

test_expect_success '1n1p query of an unknown key reaches the host' '
	run_timeout 30 flux run -N1 -n1 -overbose=2 \
		${QUERY} --expect=PMIX_ERR_NOT_FOUND \
		pmix.qry.psetnum 2>unknown.err &&
	grep "query_upcall" unknown.err &&
	grep "pmix.qry.psetnum not found" unknown.err
'

test_expect_success '2n2p query of an unknown key fails on each rank' '
	run_timeout 30 flux run -N2 -n2 \
		${QUERY} --expect=PMIX_ERR_NOT_FOUND \
		pmix.qry.psetnum 2>unknown2.err &&
	test $(grep -c "PMIx_Query_info:" unknown2.err) -eq 2
'

test_expect_success '1n1p query of two unknown keys is one upcall' '
	run_timeout 30 flux run -N1 -n1 -overbose=2 \
		${QUERY} --expect=PMIX_ERR_NOT_FOUND \
		pmix.qry.psetnum pmix.qry.psets 2>two.err &&
	test $(grep -c "pmix server query_upcall" two.err) -eq 1 &&
	grep "pmix.qry.psetnum not found" two.err &&
	grep "pmix.qry.psets not found" two.err
'

# A reserved key with no PMIX_PROCID qualifier is one the client does
# not resolve either, so it reaches the host like any other.
test_expect_success '1n1p query of a reserved key without a target reaches the host' '
	run_timeout 30 flux run -N1 -n1 -overbose=2 \
		${QUERY} --expect=PMIX_ERR_NOT_FOUND \
		pmix.job.size 2>notarget.err &&
	grep "pmix.job.size not found" notarget.err
'

test_done
