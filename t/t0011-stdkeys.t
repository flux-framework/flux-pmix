#!/bin/sh

test_description='Check the reserved keys the PMIx Standard requires

The Standard (Server-Specific Interfaces, PMIx_server_register_nspace)
lists the session, job, application, node, and process realm keys a
host must provide, and permits the server library to derive any of
them from the others.  The plugin provides some and relies on the
library to derive the rest.  This test asks for each required key the
way a client would, so that the set a client can count on is pinned
down regardless of which side produced it.

Each test names the realm and says whether the key is provided by the
plugin, derived by openpmix, or defaulted.  A from-scratch server
must produce all three kinds itself.
'

. `dirname $0`/sharness.sh

GETKEY=${FLUX_BUILD_DIR}/t/src/getkey

export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

# Each job is 2 nodes x 3 tasks: ranks 0,1 on node 0 and rank 2 on
# node 1, so that local and node ranks differ from global ranks and
# the two nodes differ in size.
#
# getkey --proc=* asks about the job (wildcard rank); getkey with no
# --proc asks about the calling rank; --proc=N asks about rank N,
# which from rank 2 is a remote rank; --node=NAME asks the node realm
# about NAME.

run2n3p() {
	run_timeout 30 flux run -N2 -n3 "$@"
}

# getkey_cmp KEY EXPECTED-FILE [getkey options]
# runs getkey on every rank with --label-io and compares sorted output
getkey_cmp() {
	key=$1; exp=$2; shift 2
	run2n3p ${GETKEY} --label-io "$@" $key | sort -n >$key.out &&
	test_cmp $exp $key.out
}

test_expect_success 'identify the client library' '
	${FLUX_BUILD_DIR}/t/src/version >client.version &&
	cat client.version
'

#
# Session realm
#
test_expect_success 'session: pmix.univ.size (provided)' '
	cat >univ.exp <<-EOT &&
	0: 3
	1: 3
	2: 3
	EOT
	getkey_cmp pmix.univ.size univ.exp --proc=*
'
test_expect_success 'session: pmix.max.size (derived)' '
	getkey_cmp pmix.max.size univ.exp --proc=*
'
test_expect_success 'session: pmix.session.id (defaulted)' '
	run2n3p ${GETKEY} --proc=* --rank=0 pmix.session.id
'

#
# Job realm
#
test_expect_success 'job: pmix.jobid (provided) matches the flux jobid' '
	run2n3p ${GETKEY} --proc=* --rank=0 pmix.jobid >jobid.out &&
	flux job last >jobid.exp &&
	test_cmp jobid.exp jobid.out
'
test_expect_success 'job: pmix.job.size (provided)' '
	getkey_cmp pmix.job.size univ.exp --proc=*
'
test_expect_success 'job: pmix.job.napps (provided)' '
	cat >napps.exp <<-EOT &&
	0: 1
	1: 1
	2: 1
	EOT
	getkey_cmp pmix.job.napps napps.exp --proc=*
'
test_expect_success 'job: pmix.num.nodes (derived from the node map)' '
	cat >nnodes.exp <<-EOT &&
	0: 2
	1: 2
	2: 2
	EOT
	getkey_cmp pmix.num.nodes nnodes.exp --proc=*
'
test_expect_success 'job: pmix.nlist (derived from the node map)' '
	run2n3p ${GETKEY} --proc=* --rank=0 pmix.nlist >nlist.out &&
	test $(tr , "\n" <nlist.out | wc -l) -eq 2
'
test_expect_success 'job: pmix.tmpdir (provided)' '
	run2n3p ${GETKEY} --proc=* --rank=0 pmix.tmpdir >tmpdir.out &&
	grep jobtmp tmpdir.out
'
test_expect_success 'job: pmix.tdir.rmclean (provided)' '
	run2n3p ${GETKEY} --proc=* --rank=0 pmix.tdir.rmclean >rmclean.out &&
	echo true >rmclean.exp &&
	test_cmp rmclean.exp rmclean.out
'
# The PMI-1 process mapping of RFC 13.  Optional in the Standard;
# MPICH uses it for its node map when present and resolves the map
# itself otherwise.  Passed through unchanged by openpmix.  For 2
# nodes x 3 tasks the blocks are (start 0, 1 node, 2 tasks) and
# (start 1, 1 node, 1 task).
test_expect_success 'job: pmix.anlmap (provided)' '
	echo "(vector,(0,1,2),(1,1,1))" >anlmap.exp &&
	run2n3p ${GETKEY} --proc=* --rank=0 pmix.anlmap >anlmap.out &&
	test_cmp anlmap.exp anlmap.out
'
# The maps are the compact form the plugin hands openpmix, which
# expands them into the per-node and per-rank keys below.  They are
# not themselves retrievable, and nothing asks for them.
test_expect_success 'job: pmix.nmap and pmix.pmap are not retrievable' '
	test_must_fail run2n3p ${GETKEY} --proc=* --rank=0 pmix.nmap &&
	test_must_fail run2n3p ${GETKEY} --proc=* --rank=0 pmix.pmap
'

#
# Application realm (one app, so only the two individual entries)
#
test_expect_success 'app: pmix.appnum (provided)' '
	cat >appnum.exp <<-EOT &&
	0: 0
	1: 0
	2: 0
	EOT
	getkey_cmp pmix.appnum appnum.exp --proc=*
'
test_expect_success 'app: pmix.wdir (provided) is the job cwd' '
	run2n3p ${GETKEY} --proc=* --rank=0 pmix.wdir >wdir.out &&
	pwd >wdir.exp &&
	test_cmp wdir.exp wdir.out
'

#
# Node realm, own node
#
test_expect_success 'node: pmix.hname (provided)' '
	run2n3p ${GETKEY} --label-io pmix.hname | sort -n >hname.out &&
	test $(wc -l <hname.out) -eq 3
'
test_expect_success 'node: pmix.nodeid (provided)' '
	cat >nodeid.exp <<-EOT &&
	0: 0
	1: 0
	2: 1
	EOT
	getkey_cmp pmix.nodeid nodeid.exp
'
test_expect_success 'node: pmix.local.size (provided)' '
	cat >lsize.exp <<-EOT &&
	0: 2
	1: 2
	2: 1
	EOT
	getkey_cmp pmix.local.size lsize.exp
'
test_expect_success 'node: pmix.lpeers (provided)' '
	cat >lpeers.exp <<-EOT &&
	0: 0,1
	1: 0,1
	2: 2
	EOT
	getkey_cmp pmix.lpeers lpeers.exp
'
test_expect_success 'node: pmix.lldr (derived from lpeers)' '
	cat >lldr.exp <<-EOT &&
	0: 0
	1: 0
	2: 2
	EOT
	getkey_cmp pmix.lldr lldr.exp
'

#
# Node realm, another node by name.  The Standard says any node may
# be targeted with PMIX_NODE_INFO plus PMIX_HOSTNAME or PMIX_NODEID.
# What a client finds for a remote node depends on its version: a
# 4.2 client resolves lpeers, local.size, and lldr this way, and
# 5.0 through 7.0 clients resolve none of them, whatever the server
# registered.  PMIx_Resolve_peers(), which MPICH uses, takes a
# different path in the client and works on all of them.  Recorded,
# not asserted.
#
test_expect_success 'node: remote node by name (version dependent)' '
	for key in pmix.lpeers pmix.local.size pmix.lldr pmix.nodeid; do
		if run2n3p ${GETKEY} --rank=2 --node=$(head -1 hname.out | cut -d" " -f2) $key >remote.$key.out 2>&1; then
			echo "$key: $(cat remote.$key.out)"
		else
			echo "$key: NOT FOUND"
		fi
	done >remote-node.out &&
	cat remote-node.out
'

#
# Process realm, own rank
#
test_expect_success 'proc: pmix.lrank (provided)' '
	cat >lrank.exp <<-EOT &&
	0: 0
	1: 1
	2: 0
	EOT
	getkey_cmp pmix.lrank lrank.exp
'
test_expect_success 'proc: pmix.nrank (provided)' '
	getkey_cmp pmix.nrank lrank.exp
'
test_expect_success 'proc: pmix.nodeid (provided)' '
	getkey_cmp pmix.nodeid nodeid.exp
'
test_expect_success 'proc: pmix.appnum (provided)' '
	getkey_cmp pmix.appnum appnum.exp
'
# The Standard lists a rank's hostname as optional, and the plugin does
# not register it; openpmix derives it from the proc map.  MPICH
# requires it for every rank: its init queries a node realm key for
# each rank in turn, which the client answers by first looking up that
# rank's hostname, and 5.0 through 6.1 clients crash if the lookup
# misses (see the MPICH pattern below).  So it is checked for every
# rank from every rank, not just the caller's own.  See #153.
#
test_expect_success 'proc: pmix.hname (derived; required by MPICH)' '
	cat >phname.exp <<-EOT &&
	0: 0 1 2
	1: 0 1 2
	2: 0 1 2
	EOT
	for r in 0 1 2; do
		run2n3p ${GETKEY} --label-io --proc=$r pmix.hname || return 1
	done | sort -n | awk "{print \$1}" | uniq -c \
		| awk "\$1 == 3 {print \$2, \"0 1 2\"}" >phname.out &&
	test_cmp phname.exp phname.out
'

#
# Process realm, a remote rank.  These are what the server must be
# able to answer about any rank in the job.  Rank 2 asks about rank 0.
#
test_expect_success 'proc: remote pmix.lrank' '
	echo "2: 0" >rlrank.exp &&
	getkey_cmp pmix.lrank rlrank.exp --rank=2 --proc=0
'
test_expect_success 'proc: remote pmix.nrank' '
	getkey_cmp pmix.nrank rlrank.exp --rank=2 --proc=0
'
test_expect_success 'proc: remote pmix.nodeid' '
	echo "2: 0" >rnodeid.exp &&
	getkey_cmp pmix.nodeid rnodeid.exp --rank=2 --proc=0
'
test_expect_success 'proc: remote pmix.appnum' '
	echo "2: 0" >rappnum.exp &&
	getkey_cmp pmix.appnum rappnum.exp --rank=2 --proc=0
'
# A rank's own hostname is the real one; another rank's is its name
# in the node map, which the testsuite uniquifies when shells share
# a host (see maps.c).  So compare against the node list, not against
# what rank 0 says of itself.
test_expect_success 'proc: remote pmix.hname is its node in the node list' '
	run2n3p ${GETKEY} --rank=2 --proc=0 pmix.hname >rhname.out &&
	run2n3p ${GETKEY} --proc=* --rank=0 pmix.nlist | cut -d, -f1 >node0.exp &&
	test_cmp node0.exp rhname.out
'

#
# The MPICH init pattern: a node realm key asked of every rank in the
# job with no qualifiers (src/util/mpir_pmix.inc, PMIX_FABRIC_COORDINATES),
# expecting NOT_FOUND where the server has nothing.  On 5.0 through
# 6.1 clients the lookup crashes in libpmix if the target rank has no
# cached hostname, so this is also what the hostname check above
# protects.  getkey exits 1 on NOT_FOUND; anything else is a failure.
#
test_expect_success 'MPICH pattern: node realm key for every rank is NOT_FOUND, not a crash' '
	for r in 0 1 2; do
		run2n3p ${GETKEY} --label-io --proc=$r pmix.fab.coord \
			>fab.$r.out 2>&1
		test $? -eq 1 || return 1
		test $(grep -c NOT_FOUND fab.$r.out) -eq 3 || return 1
	done
'

#
# Keys the Standard lists as required that are not retrievable today,
# with no client known to ask.  A from-scratch server may want them;
# recorded so a change in either direction is noticed.
#
#   pmix.rank, pmix.grank, pmix.apprank   process identity (the client
#                                         has these from PMIx_Init)
#   pmix.reinc, pmix.spawned              process history
#   pmix.procdir, pmix.pkgrank, pmix.locstr, pmix.cpuset
#                                         local process placement
#   pmix.node.size, pmix.local.procs, pmix.ndoversubscr
#                                         node realm
#   pmix.app.size, pmix.apptldr, pmix.argv
#                                         app realm (one app)
#
test_expect_success 'listed-but-absent keys are absent' '
	for key in pmix.rank pmix.grank pmix.apprank pmix.reinc pmix.spawned \
		   pmix.procdir pmix.pkgrank pmix.locstr pmix.cpuset \
		   pmix.node.size pmix.local.procs pmix.ndoversubscr; do
		test_must_fail run2n3p ${GETKEY} --rank=0 $key || return 1
	done &&
	for key in pmix.app.size pmix.apptldr pmix.argv; do
		test_must_fail run2n3p ${GETKEY} --proc=* --rank=0 $key || return 1
	done
'

test_done
