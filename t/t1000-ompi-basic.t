#!/bin/sh

test_description='Test openmpi bootstrap.'

PLUGINPATH=${FLUX_BUILD_DIR}/src/shell/plugins/.libs

. `dirname $0`/sharness.sh

MPI_HELLO=${FLUX_BUILD_DIR}/t/src/mpi_hello

test_under_flux 2

test_expect_success 'create rc.lua script' "
	cat >rc.lua <<-EOT
	plugin.load (\"$PLUGINPATH/pmix.so\")
	EOT
"

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

test_done
