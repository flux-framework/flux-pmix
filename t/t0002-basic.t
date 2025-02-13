#!/bin/sh

test_description='Basic plugin tests'

. `dirname $0`/sharness.sh

VERSION=${FLUX_BUILD_DIR}/t/src/version
GETKEY=${FLUX_BUILD_DIR}/t/src/getkey

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

test_expect_success 'print pmix library version' '
	${VERSION}
'

test_expect_success 'pmix server is active' '
	flux run printenv PMIX_RANK
'
test_expect_success 'pmix server is inactive with -opmi=off' '
	test_must_fail flux run -opmi=off printenv PMIX_RANK
'
test_expect_success 'pmix server is active with -opmi=pmix,unknown' '
	flux run -opmi=pmix,unknown printenv PMIX_RANK
'
test_expect_success 'pmix server is active with -opmi=unknown,pmix' '
	flux run -opmi=unknown,pmix printenv PMIX_RANK
'

test_expect_success 'capture environment with plugin loaded' '
	flux run -overbose=2 printenv >env.out
'

test_expect_success 'PMIX_SERVER_URI* variables all have the same value' '
	grep PMIX_SERVER_URI env.out | cut -d= -f2 | sort | uniq >uri.out &&
	test $(wc -l <uri.out) -eq 1
'

test_expect_success 'server is listening on localhost' '
	grep tcp4://127.0.0.1: uri.out
'

test_expect_success 'PMIX_SERVER_TMPDIR == FLUX_JOB_TMPDIR' '
	grep FLUX_JOB_TMPDIR <env.out|cut -d= -f2 >jobtmp.out &&
	grep PMIX_SERVER_TMPDIR <env.out|cut -d= -f2 >srvtmp.out &&
	test -s jobtmp.out &&
	test_cmp jobtmp.out srvtmp.out
'

test_expect_success '2n4p pmix.job.size is set correctly' '
	cat >pmix.job.size.exp <<-EOT &&
	0: 4
	1: 4
	2: 4
	3: 4
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --proc=* --label-io pmix.job.size \
			| sort -n >pmix.job.size.out &&
	test_cmp pmix.job.size.exp pmix.job.size.out
'

test_expect_success '2n4p pmix.univ.size is set correctly' '
	cat >pmix.univ.size.exp <<-EOT &&
	0: 4
	1: 4
	2: 4
	3: 4
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --proc=* --label-io pmix.univ.size \
			| sort -n >pmix.univ.size.out &&
	test_cmp pmix.univ.size.exp pmix.univ.size.out
'
test_expect_success '2n3p pmix.local.size is set correctly' '
	cat >2n3p.pmix.local.size.exp <<-EOT &&
	0: 2
	1: 2
	2: 1
	EOT
	run_timeout 30 flux run -N2 -n3 \
		${GETKEY} --proc=* --label-io pmix.local.size \
			| sort -n >2n3p.pmix.local.size.out &&
	test_cmp 2n3p.pmix.local.size.exp 2n3p.pmix.local.size.out
'
test_expect_success '2n4p pmix.local.size is set correctly' '
	cat >pmix.local.size.exp <<-EOT &&
	0: 2
	1: 2
	2: 2
	3: 2
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --proc=* --label-io pmix.local.size \
			| sort -n >pmix.local.size.out &&
	test_cmp pmix.local.size.exp pmix.local.size.out
'
test_expect_success 'pmix.tmpdir is set' '
	run_timeout 30 flux run -n1 \
		${GETKEY} --proc=* pmix.tmpdir
'
test_expect_success 'pmix.job.napps is set to 1' '
	cat >pmix.job.napps.exp <<-EOT &&
	1
	EOT
	run_timeout 30 flux run -n1 \
		${GETKEY} --proc=* pmix.job.napps >pmix.job.napps.out &&
	test_cmp pmix.job.napps.exp pmix.job.napps.out
'
# pmix.nsdir appears to be automatically set in openpmix 4.2.9, but not
# in the earlier releases we were testing (up to 4.2.4).
test_expect_success 'pmix.nsdir is NOT set' '
	test_might_fail flux run \
		${GETKEY} --proc=* pmix.nsdir
'
test_expect_success '2n4p pmix.hname is set' '
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --label-io pmix.hname
'

test_expect_success '2n3p pmix.lpeers is set correctly' '
	cat >2n3p.pmix.lpeers.exp <<-EOT &&
	0: 0,1
	1: 0,1
	2: 2
	EOT
	run_timeout 30 flux run -N2 -n3 \
		${GETKEY} --proc=* --label-io pmix.lpeers \
			| sort -n >2n3p.pmix.lpeers.out &&
	test_cmp 2n3p.pmix.lpeers.exp 2n3p.pmix.lpeers.out
'
test_expect_success '2n4p pmix.lpeers is set correctly' '
	cat >pmix.lpeers.exp <<-EOT &&
	0: 0,1
	1: 0,1
	2: 2,3
	3: 2,3
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --proc=* --label-io pmix.lpeers \
			| sort -n >pmix.lpeers.out &&
	test_cmp pmix.lpeers.exp pmix.lpeers.out
'
test_expect_success '2n4p pmix.nlist is set' '
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --proc=* --label-io pmix.nlist
'
test_expect_success '2n4p pmix.num.nodes is set correctly' '
	cat >pmix.num.nodes.exp <<-EOT &&
	0: 2
	1: 2
	2: 2
	3: 2
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --proc=* --label-io pmix.num.nodes \
			| sort -n >pmix.num.nodes.out &&
	test_cmp pmix.num.nodes.exp pmix.num.nodes.out
'
test_expect_success '2n3p pmix.nodeid is set correctly' '
	cat >2n3p.pmix.nodeid.exp <<-EOT &&
	0: 0
	1: 0
	2: 1
	EOT
	run_timeout 30 flux run -N2 -n3 \
		${GETKEY} --label-io pmix.nodeid \
			| sort -n >2n3p.pmix.nodeid.out &&
	test_cmp 2n3p.pmix.nodeid.exp 2n3p.pmix.nodeid.out
'
test_expect_success '2n4p pmix.nodeid is set correctly' '
	cat >pmix.nodeid.exp <<-EOT &&
	0: 0
	1: 0
	2: 1
	3: 1
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --label-io pmix.nodeid \
			| sort -n >pmix.nodeid.out &&
	test_cmp pmix.nodeid.exp pmix.nodeid.out
'
test_expect_success '2n3p pmix.lrank is set correctly' '
	cat >2n3p.pmix.lrank.exp <<-EOT &&
	0: 0
	1: 1
	2: 0
	EOT
	run_timeout 30 flux run -N2 -n3 \
		${GETKEY} --label-io pmix.lrank \
			| sort -n >2n3p.pmix.lrank.out &&
	test_cmp 2n3p.pmix.lrank.exp 2n3p.pmix.lrank.out
'
test_expect_success '2n4p pmix.lrank is set correctly' '
	cat >pmix.lrank.exp <<-EOT &&
	0: 0
	1: 1
	2: 0
	3: 1
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --label-io pmix.lrank \
			| sort -n >pmix.lrank.out &&
	test_cmp pmix.lrank.exp pmix.lrank.out
'
test_expect_success '2n3p pmix.srv.rank is set correctly' '
	cat >2n3p.pmix.srv.rank.exp <<-EOT &&
	0: 0
	1: 0
	2: 1
	EOT
	run_timeout 30 flux run -N2 -n3 \
		${GETKEY} --label-io pmix.srv.rank \
			| sort -n >2n3p.pmix.srv.rank.out &&
	test_cmp 2n3p.pmix.srv.rank.exp 2n3p.pmix.srv.rank.out
'
test_expect_success '2n4p pmix.srv.rank is set correctly' '
	cat >pmix.srv.rank.exp <<-EOT &&
	0: 0
	1: 0
	2: 1
	3: 1
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --label-io pmix.srv.rank \
			| sort -n >pmix.srv.rank.out &&
	test_cmp pmix.srv.rank.exp pmix.srv.rank.out
'
test_expect_success '2n4p pmix.appnum is set correctly' '
	cat >pmix.appnum.exp <<-EOT &&
	0: 0
	1: 0
	2: 0
	3: 0
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --proc=* --label-io pmix.appnum \
			| sort -n >pmix.appnum.out &&
	test_cmp pmix.appnum.exp pmix.appnum.out
'
test_expect_success '2n4p pmix.job.napps is set correctly' '
	cat >pmix.job.napps.exp <<-EOT &&
	0: 1
	1: 1
	2: 1
	3: 1
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --proc=* --label-io pmix.job.napps \
			| sort -n >pmix.job.napps.out &&
	test_cmp pmix.job.napps.exp pmix.job.napps.out
'
test_expect_success '2n3p pmix.nrank is set correctly' '
	cat >2n3p.pmix.nrank.exp <<-EOT &&
	0: 0
	1: 1
	2: 0
	EOT
	run_timeout 30 flux run -N2 -n3 \
		${GETKEY} --label-io pmix.nrank \
			| sort -n >2n3p.pmix.nrank.out &&
	test_cmp 2n3p.pmix.nrank.exp 2n3p.pmix.nrank.out
'
test_expect_success '2n4p pmix.nrank is set correctly' '
	cat >pmix.nrank.exp <<-EOT &&
	0: 0
	1: 1
	2: 0
	3: 1
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --label-io pmix.nrank \
			| sort -n >pmix.nrank.out &&
	test_cmp pmix.nrank.exp pmix.nrank.out
'
test_expect_success '1n1p pmix.tdir.rmclean is true' '
	cat >pmix.tdir.rmclean.exp <<-EOT &&
	true
	EOT
	flux run \
		${GETKEY} --proc=* pmix.tdir.rmclean >pmix.tdir.rmclean.out &&
	test_cmp pmix.tdir.rmclean.exp pmix.tdir.rmclean.out
'
test_expect_success '2n4p pmix.max.size is set correctly' '
	cat >pmix.max.size.exp <<-EOT &&
	0: 4
	1: 4
	2: 4
	3: 4
	EOT
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --proc=* --label-io pmix.max.size \
			| sort -n >pmix.max.size.out &&
	test_cmp pmix.max.size.exp pmix.max.size.out
'
test_expect_success '2n4p pmix.jobid is set' '
	run_timeout 30 flux run -N2 -n4 \
		${GETKEY} --proc=* --label-io pmix.jobid
'

test_done
