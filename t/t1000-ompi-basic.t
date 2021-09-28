#!/bin/sh

test_description='Test openmpi bootstrap.'

PLUGINPATH=${FLUX_BUILD_DIR}/src/shell/plugins/.libs

. `dirname $0`/sharness.sh

MPI_HELLO=${FLUX_BUILD_DIR}/t/src/mpi_hello

test_under_flux 2

test_expect_success 'create rc.lua script' "
	cat >rc.lua <<-EOT
	plugin.load (\"$PLUGINPATH/pmix.so\")
	shell.setenv (\"OMPI_MCA_btl_tcp_if_include\", \"lo\")
	EOT
"

test_expect_success 'capture the job environment' '
	run_timeout 30 flux mini run \
		-ouserrc=$(pwd)/rc.lua \
		-ompi=openmpi@5 \
		printenv >printenv.out
'

test_expect_success 'verify deprecated flux pmix/schizo plugins are not requested' '
	test_must_fail grep OMPI_MCA_pmix=flux printenv.out &&
	test_must_fail grep OMPI_MCA_schizo=flux printenv.out
'

test_expect_success 'sanity check pmix environment' '
	grep ^PMIX_NAMESPACE printenv.out
'

test_expect_success '1n2p ompi hello' '
	run_timeout 30 flux mini run -N1 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		-ompi=openmpi@5 \
		${MPI_HELLO}
'

test_expect_success '2n2p ompi hello' '
	run_timeout 30 flux mini run -N2 -n2 \
		-ouserrc=$(pwd)/rc.lua \
		-ompi=openmpi@5 \
		${MPI_HELLO}
'

# Useful debugging runes:
# --env=PMIX_MCA_pmix_client_fence_verbose=100 \
# --env=OMPI_MCA_shmem_base_verbose=100 \
# --env=OMPI_MCA_btl_base_verbose=100 \
# --env=OMPI_MCA_btl_tcp_if_exclude=docker0,lo \

# see issue #27
test_expect_success '2n3p ompi hello doesnt hang' '
	run_timeout 60 flux mini run -N2 -n3 \
		-ouserrc=$(pwd)/rc.lua \
		-ompi=openmpi@5 \
		-overbose=2 \
		${MPI_HELLO}
'

# see issue #26
test_expect_success '2n4p ompi hello reports no system call errors' '
        run_timeout 30 flux mini run -N2 -n4 \
                -ouserrc=$(pwd)/rc.lua \
                -ompi=openmpi@5 \
                ${MPI_HELLO} 2>2n4p_hello.err &&
        test_must_fail grep "System call:" 2n4p_hello.err
'

test_done
