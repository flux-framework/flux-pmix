#!/bin/sh

test_description='Basic plugin tests on a single shell'

PLUGINPATH=${FLUX_BUILD_DIR}/src/shell/plugins/.libs

. `dirname $0`/sharness.sh

BARRIER=${FLUX_BUILD_DIR}/t/src/barrier
BIZCARD=${FLUX_BUILD_DIR}/t/src/bizcard
VERSION=${FLUX_BUILD_DIR}/t/src/version
GETKEY=${FLUX_BUILD_DIR}/t/src/getkey

test_under_flux 1

test_expect_success 'print pmix library version' '
	${VERSION}
'

test_expect_success 'create ompi_rc.lua script' "
	cat >ompi_rc.lua <<-EOT
	plugin.load (\"$PLUGINPATH/pmix.so\")
	EOT
"

test_expect_success 'verify that plugin is loaded by ompi_rc.lua' '
	flux mini run -o userrc=$(pwd)/ompi_rc.lua /bin/true
'

test_expect_success 'capture environment with plugin loaded' '
	flux mini run -o userrc=$(pwd)/ompi_rc.lua printenv >env.out
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

test_expect_success 'pmix.job.size is set correctly on rank 0' '
	cat >size.exp <<-EOT
	2
	EOT
	run_timeout 30 flux mini run -n2 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} --proc=* --rank=0 pmix.job.size >size0.out &&
	test_cmp size.exp size0.out
'
test_expect_success 'pmix.job.size is set correctly on rank 1' '
	run_timeout 30 flux mini run -n2 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} --proc=* --rank=1 pmix.job.size >size1.out &&
	test_cmp size.exp size1.out
'
test_expect_success 'pmix.univ.size 2 procs is 2' '
	cat >univ.exp <<-EOT
	2
	EOT
	run_timeout 30 flux mini run -n2 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} --proc=* pmix.univ.size >univ.out &&
	test_cmp univ.exp univ.out
'
test_expect_success 'pmix.local.size 2 procs 1 shell is 2' '
	cat >localsize.exp <<-EOT
	2
	EOT
	run_timeout 30 flux mini run -n2 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} --proc=* pmix.local.size >localsize.out &&
	test_cmp localsize.exp localsize.out
'
test_expect_success 'pmix.tmpdir is set' '
	run_timeout 30 flux mini run -n1 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} --proc=* pmix.tmpdir
'
test_expect_success 'pmix.job.napps is set to 1' '
	cat >napps.exp <<-EOT
	1
	EOT
	run_timeout 30 flux mini run -n2 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} --proc=* pmix.job.napps >napps.out
'
test_expect_success 'pmix.nsdir is set' '
	run_timeout 30 flux mini run -n1 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} --proc=* pmix.nsdir
'
test_expect_success 'pmix.hname is set' '
	run_timeout 30 flux mini run -n1 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} pmix.hname
'
test_expect_success 'pmix.lpeers is set' '
	run_timeout 30 flux mini run -n1 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} --proc=* pmix.lpeers
'
test_expect_success 'pmix.nlist is set' '
	run_timeout 30 flux mini run -n1 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} --proc=* pmix.nlist
'
test_expect_success 'pmix.num.nodes is set' '
	run_timeout 30 flux mini run -n1 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} --proc=* pmix.num.nodes
'
test_expect_success 'pmix.nodeid is set' '
	run_timeout 30 flux mini run -n1 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} pmix.nodeid
'
test_expect_success 'pmix.lrank is set' '
	run_timeout 30 flux mini run -n1 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} pmix.lrank
'
test_expect_success 'pmix.srv.rank is set to 0' '
	cat >srvrank.exp <<-EOT &&
	0
	EOT
	run_timeout 30 flux mini run -n1 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${GETKEY} pmix.srv.rank >srvrank.out &&
	test_cmp srvrank.exp srvrank.out
'

test_expect_success 'pmix barrier works' '
	run_timeout 30 flux mini run -n2 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${BARRIER}
'

test_expect_success 'pmix bizcard works' '
	run_timeout 30 flux mini run -n2 \
		-ouserrc=$(pwd)/ompi_rc.lua \
		${BIZCARD} 1
'

test_done
