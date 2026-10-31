#!/bin/sh

test_description='Check the reserved keys a PMIx client may expect

PMIx Standard v5.0, chapter "Reserved Keys", says reserved keys "are
provided by the host environment and the PMIx server, and are
required to be available at client start of execution", and its
realm subsections (session, job, application, node, process) define
each key, the realm it belongs to, and how PMIx_Get selects a target
other than the default.  This test asks for each key the way a
client would, so that the set a client can count on is pinned down.
It cites the client-facing text throughout: how the server came by a
key is of no concern to the client.

Each test names the realm and says whether the plugin provides the
key or openpmix defaults it.  Where a key the chapter defines is not
retrievable, that is recorded, so a change either way shows up.

The rules are those of v5.0 of the Standard.  An openpmix 3.2 or 4.2
client implements the Standard contemporary with its release line
(v3.2, v4.2), and where the two versions describe retrieval
differently the test asks each client in the form its version
documents, with the citation.  Where a client falls short of its own
version, the test skips and says so.
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
# which from rank 2 is a remote rank.  --session, --app, and
# --node=NAME add the qualifier that selects that realm: v4.2 requires
# it for session and application realm keys, and v5.0 accepts it, so
# those keys are asked for that way on every client.

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

# The client library is what getkey links; the version tool links the
# same one, and PMIx_Get_version() returns "3.2.5a1" or "OpenPMIx
# 5.0.7a1 (...)".  The server library is what the shell plugin links;
# PMIx_server_setup_fork() puts its version in each task's environment
# as PMIX_VERSION.  The two may differ (--with-pmix-server), so each
# is recorded, and emitted as a TAP diagnostic, which the automake TAP
# driver copies into the .log that CI keeps.  Anything the test
# commands print goes to the .output file instead, which CI does not
# show.
diag() {
	echo "# $*"
}
test_expect_success 'identify the openpmix client library' '
	${FLUX_BUILD_DIR}/t/src/version >openpmix-client.version
'
diag "openpmix client: $(cat openpmix-client.version)"
# OPENPMIX_SERVER_VERSION is the version configure found for the plugin,
# so the server must report it.  If it does not, another flux-pmix
# (an installed package, loaded by the system initrc) is answering
# instead of this tree, and nothing below would be testing what it
# claims to.
test_expect_success 'identify the openpmix server library' '
	run_timeout 30 flux run printenv PMIX_VERSION >openpmix-server.version &&
	test "$(cat openpmix-server.version)" = "$OPENPMIX_SERVER_VERSION"
'
diag "openpmix server: $(cat openpmix-server.version)"
# The server also tells each task which data stores it offers, in
# order of preference, as PMIX_GDS_MODULE.  The first one a client has
# is the one it uses.
test_expect_success 'identify the data stores the server offers' '
	run_timeout 30 flux run printenv PMIX_GDS_MODULE >server.stores
'
diag "server data stores: $(cat server.stores)"
# A few cases below depend on the client library's version, and one on
# the data store the server offers: the server registers the key
# either way, and it is the client's PMIx_Get, or the store's packing,
# that does or does not deliver it.  Each such case says why, citing
# the Standard contemporary with the client's release line: v3.2 for
# openpmix 3.2, v4.2 for 4.2, and v5.0 for 5.0 and later.  (The
# "PMIx Standard" level in openpmix's own version string is not a
# guide: it is a field in its VERSION file that only the 5.0 release
# branch kept current, so 6.x and 7.0 still report the 4.1 they
# inherited from 2021.)
OPENPMIX_CLIENT_MAJOR=$(${FLUX_BUILD_DIR}/t/src/version \
	| sed -n 's/^\(OpenPMIx \)\{0,1\}\([0-9]*\)\..*/\2/p')
if test "${OPENPMIX_CLIENT_MAJOR:-0}" -ge 4; then
	test_set_prereq OPENPMIX4_CLIENT
fi
if test "${OPENPMIX_CLIENT_MAJOR:-0}" -ge 5; then
	test_set_prereq OPENPMIX5_CLIENT
fi
OPENPMIX_SERVER_MAJOR=$(echo "$OPENPMIX_SERVER_VERSION" | cut -d. -f1)
if test "${OPENPMIX_CLIENT_MAJOR:-0}" -ge 4 \
	|| test "${OPENPMIX_SERVER_MAJOR:-0}" -lt 4; then
	test_set_prereq APP_REALM_WDIR
fi

#
# Session realm ("Session realm attributes"): PMIX_UNIV_SIZE and
# PMIX_SESSION_ID.  (PMIX_MAX_PROCS is defined in every realm with a
# different meaning in each, and the plugin registers only
# PMIX_UNIV_SIZE; what openpmix synthesizes for it varies by version
# and is not checked.)
#
test_expect_success 'session: pmix.univ.size (provided)' '
	cat >univ.exp <<-EOT &&
	0: 3
	1: 3
	2: 3
	EOT
	getkey_cmp pmix.univ.size univ.exp --session --proc=*
'
# The Standard assigns no meaning to the value for a job in no session,
# so only retrieval is checked.  Skipped on openpmix 4.2 and earlier
# clients: they keep the id as the session object's identity rather
# than as a key in its info, so their lookup finds nothing.  (A 4.2
# client on the ds21 store does return it, because that store copies
# the registered entry verbatim, but that is the store's accident, not
# the client's answer, and is not relied on.)  5.0 and later answer
# from the session id recorded at startup (pmix_client_get.c, since
# 14a3fd63c).
test_expect_success OPENPMIX5_CLIENT 'session: pmix.session.id (provided)' '
	run2n3p ${GETKEY} --session --proc=* --rank=0 pmix.session.id
'

#
# Job realm ("Job realm attributes"): PMIX_JOBID, PMIX_JOB_SIZE,
# PMIX_JOB_NUM_APPS, PMIX_NUM_NODES, PMIX_NODE_LIST, PMIX_TMPDIR.
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
test_expect_success 'job: pmix.num.nodes (provided)' '
	cat >nnodes.exp <<-EOT &&
	0: 2
	1: 2
	2: 2
	EOT
	getkey_cmp pmix.num.nodes nnodes.exp --proc=*
'
test_expect_success 'job: pmix.nlist (provided)' '
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
# The node and proc maps are not in the "Reserved Keys" chapter at
# all: they are inputs a host hands a server library (chapter
# "Server-Specific Interfaces", PMIx_server_register_nspace), which
# consumes them to build what the realms above describe.  A client
# has no claim on them, and none asks.
test_expect_success 'job: pmix.nmap and pmix.pmap are not retrievable' '
	test_must_fail run2n3p ${GETKEY} --proc=* --rank=0 pmix.nmap &&
	test_must_fail run2n3p ${GETKEY} --proc=* --rank=0 pmix.pmap
'

#
# Application realm ("Application realm attributes"): PMIX_APPNUM
# identifies the app; PMIX_WDIR is its working directory.  One app.
#
# v4.2 says application realm keys "can only be retrieved by including
# the PMIX_APP_INFO attribute", with the wildcard rank selecting the
# caller's own application; v5.0 defaults the key to its realm and
# accepts the same form.  Asked that way, both keys are found on every
# openpmix version and data store.  (An unqualified get for pmix.wdir
# misses on a 4.x client using ds21, and a qualified get naming the
# caller's own rank makes a 4.x server wait for a commit that never
# comes; neither is the documented form.)
#
# pmix.wdir is skipped for a 3.2 client on a 4.x server, where it is
# not found: a 4.x server hands a ds21 client its app keys only inside
# PMIX_APP_INFO_ARRAY (dstore_base.c, _store_job_info), which that
# client's store does not unpack.  pmix.appnum is still found.
test_expect_success 'app: pmix.appnum (provided)' '
	cat >appnum.exp <<-EOT &&
	0: 0
	1: 0
	2: 0
	EOT
	getkey_cmp pmix.appnum appnum.exp --app --proc=*
'
test_expect_success APP_REALM_WDIR 'app: pmix.wdir (provided) is the job cwd' '
	run2n3p ${GETKEY} --app --proc=* --rank=0 pmix.wdir >wdir.out &&
	pwd >wdir.exp &&
	test_cmp wdir.exp wdir.out
'

#
# Node realm, own node ("Node realm keys"): PMIX_HOSTNAME,
# PMIX_NODEID, PMIX_LOCAL_SIZE, PMIX_LOCAL_PEERS, PMIX_LOCAL_PROCS,
# PMIX_LOCALLDR, PMIX_NODE_OVERSUBSCRIBED.  The target node defaults
# to the one the caller is on.
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
# Two documented ways to ask about the caller's node.  v5.0 ("Node
# realm keys") says these keys default to the node realm whatever
# rank is named, so a client asks with its own rank.  v3.2
# ("Node-level information") also allows that form, but says that
# "all other information is retrievable using a rank of
# PMIX_RANK_WILDCARD when the information being retrieved refers to
# something non-rank specific (e.g., number of processes on a node)",
# and that wildcard form is the one openpmix 3.2 and 4.2 clients
# implement.  Each client is asked in the form its Standard documents;
# the registration is the same for both.
if test_have_prereq OPENPMIX5_CLIENT; then
	NODEPROC=
else
	NODEPROC=--proc=*
fi
test_expect_success 'node: pmix.local.size (provided)' '
	cat >lsize.exp <<-EOT &&
	0: 2
	1: 2
	2: 1
	EOT
	getkey_cmp pmix.local.size lsize.exp $NODEPROC
'
test_expect_success 'node: pmix.lpeers (provided)' '
	cat >lpeers.exp <<-EOT &&
	0: 0,1
	1: 0,1
	2: 2
	EOT
	getkey_cmp pmix.lpeers lpeers.exp $NODEPROC
'
test_expect_success 'node: pmix.lldr (provided)' '
	cat >lldr.exp <<-EOT &&
	0: 0
	1: 0
	2: 2
	EOT
	getkey_cmp pmix.lldr lldr.exp $NODEPROC
'
# The local peers as an array of procs; getkey prints the ranks.
# Skipped on openpmix 3.2 clients: a node realm get consults a fixed
# list of node keys (pmix_check_node_info), and in 3.2 that list is
# only PMIX_LOCAL_PEERS, PMIX_LOCAL_SIZE, PMIX_NODE_SIZE, and
# PMIX_LOCALLDR, so these two are stored and never found.  v3.2 of
# the Standard defines PMIX_LOCAL_PROCS (Chap_API_Struct) but not
# PMIX_NODE_OVERSUBSCRIBED, so the first is a shortfall against the
# client's own Standard and the second is not.
test_expect_success OPENPMIX4_CLIENT 'node: pmix.lprocs (provided)' '
	getkey_cmp pmix.lprocs lpeers.exp $NODEPROC
'
test_expect_success OPENPMIX4_CLIENT 'node: pmix.ndosub (provided) is false with a core per task' '
	cat >ndosub.exp <<-EOT &&
	0: false
	1: false
	2: false
	EOT
	getkey_cmp pmix.ndosub ndosub.exp $NODEPROC
'
test_expect_success OPENPMIX4_CLIENT 'node: pmix.ndosub (provided) is true with 3 tasks on a core' '
	cat >ndosub3.exp <<-EOT &&
	0: true
	1: true
	2: true
	EOT
	run_timeout 30 flux run -N1 --cores=1 --tasks-per-core=3 \
		${GETKEY} --label-io $NODEPROC pmix.ndosub | sort -n >ndosub3.out &&
	test_cmp ndosub3.exp ndosub3.out
'

#
# Node realm, another node by name.  "Node realm keys" says any node
# may be targeted with PMIX_NODE_INFO plus PMIX_HOSTNAME or
# PMIX_NODEID.  What a client finds for a remote node depends on its
# version: a 4.2 client resolves lpeers, local.size, and lldr this
# way, and 5.0 through 7.0 clients resolve none of them, whatever the
# server registered.  PMIx_Resolve_peers(), which MPICH uses, takes a
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
# Process realm, own rank ("Process realm attributes"): PMIX_RANK,
# PMIX_GLOBAL_RANK, PMIX_APP_RANK, PMIX_LOCAL_RANK, PMIX_NODE_RANK,
# PMIX_NODEID, PMIX_APPNUM, PMIX_HOSTNAME, PMIX_SPAWNED,
# PMIX_REINCARNATION.  The target is the rank the proc argument
# names, and defaults to the caller.
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
# One application and no rank offset, so both equal the rank.
test_expect_success 'proc: pmix.grank (provided)' '
	cat >grank.exp <<-EOT &&
	0: 0
	1: 1
	2: 2
	EOT
	getkey_cmp pmix.grank grank.exp
'
test_expect_success 'proc: pmix.apprank (provided)' '
	getkey_cmp pmix.apprank grank.exp
'
test_expect_success 'proc: pmix.spawned (provided)' '
	cat >spawned.exp <<-EOT &&
	0: false
	1: false
	2: false
	EOT
	getkey_cmp pmix.spawned spawned.exp
'
test_expect_success 'proc: pmix.reinc (provided)' '
	getkey_cmp pmix.reinc appnum.exp
'
# A rank's hostname is a process realm key a client may ask for, and
# MPICH does, for every rank: its init queries a node realm key for
# each rank in turn, which the client answers by first looking up that
# rank's hostname, and 5.0 through 6.1 clients crash if the lookup
# misses (see the MPICH pattern below).  So it is checked for every
# rank from every rank, not just the caller's own.  See #153.
#
test_expect_success 'proc: pmix.hname (provided; required by MPICH)' '
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
# Process realm, a remote rank.  "Process realm attributes" says the
# target is whatever rank the proc argument names, so these must be
# answerable about any rank in the job.  Rank 2 asks about rank 0.
#
test_expect_success 'proc: remote pmix.lrank' '
	echo "2: 0" >rlrank.exp &&
	getkey_cmp pmix.lrank rlrank.exp --rank=2 --proc=0
'
test_expect_success 'proc: remote pmix.nrank' '
	getkey_cmp pmix.nrank rlrank.exp --rank=2 --proc=0
'
# Skipped on openpmix 4.2 and earlier clients.  The plugin registers
# PMIX_NODEID in both realms: in each rank's proc info, where v5.0
# lists it, and in each node's info array, where v4.2 lists it.  A 4.2
# client treats the key as node realm only (v4.2: "the keys for
# node-related information are not shared across other realms"), so
# it passes over the per-rank entry, resolves the rank to a node, and
# then looks in that node's key list, where the nodeid is the node
# object's identity rather than a key, and finds nothing.  5.0 fixed
# both halves.  No registration reaches this.
test_expect_success OPENPMIX5_CLIENT 'proc: remote pmix.nodeid' '
	echo "2: 0" >rnodeid.exp &&
	getkey_cmp pmix.nodeid rnodeid.exp --rank=2 --proc=0
'
test_expect_success 'proc: remote pmix.appnum' '
	echo "2: 0" >rappnum.exp &&
	getkey_cmp pmix.appnum rappnum.exp --rank=2 --proc=0
'
test_expect_success 'proc: remote pmix.grank and pmix.apprank' '
	echo "2: 1" >rgrank.exp &&
	getkey_cmp pmix.grank rgrank.exp --rank=2 --proc=1 &&
	getkey_cmp pmix.apprank rgrank.exp --rank=2 --proc=1
'
test_expect_success 'proc: remote pmix.spawned and pmix.reinc' '
	echo "2: false" >rspawned.exp &&
	getkey_cmp pmix.spawned rspawned.exp --rank=2 --proc=0 &&
	getkey_cmp pmix.reinc rappnum.exp --rank=2 --proc=0
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
# protects.  getkey exits 1 when the get fails (NOT_FOUND, or
# PROC_ENTRY_NOT_FOUND from 7.0); a signal is anything else.
#
test_expect_success 'MPICH pattern: node realm key for every rank fails, no crash' '
	for r in 0 1 2; do
		run2n3p ${GETKEY} --label-io --proc=$r pmix.fab.coord \
			>fab.$r.out 2>&1
		test $? -eq 1 || return 1
		test $(grep -c "PMIx_Get pmix.fab.coord" fab.$r.out) -eq 3 \
			|| return 1
	done
'

#
#
# Keys the "Reserved Keys" chapter defines that the plugin does not
# yet provide.  Each is asserted as the Standard describes it and
# skipped on a prerequisite that names what is missing on our side;
# providing the key is then a matter of removing the prerequisite.
# Values that are host specific are checked for retrieval only.
#
# NEED_CPUSET: PMIX_CPUSET, PMIX_LOCALITY_STRING, and PMIX_PACKAGE_RANK
# describe where each local process is bound.  The shell binds tasks in
# its affinity plugin but does not expose the per-task cpuset, and the
# server needs the node topology (PMIX_TOPOLOGY2 at init) to generate
# the locality string from it.
#
# NEED_PROCDIR: PMIX_PROCDIR is a per-process directory under
# PMIX_NSDIR that the shell does not create.
#
# NEED_NODE_SIZE: PMIX_NODE_SIZE counts processes across all jobs on
# the node, which a shell, knowing only its own job, cannot report.
#
test_expect_success NEED_CPUSET 'proc: pmix.cpuset (not yet provided)' '
	run2n3p ${GETKEY} pmix.cpuset
'
test_expect_success NEED_CPUSET 'proc: pmix.locstr (not yet provided)' '
	run2n3p ${GETKEY} pmix.locstr
'
test_expect_success NEED_CPUSET 'proc: pmix.pkgrank (not yet provided)' '
	run2n3p ${GETKEY} pmix.pkgrank
'
test_expect_success NEED_PROCDIR 'proc: pmix.pdir (not yet provided)' '
	run2n3p ${GETKEY} pmix.pdir
'
# With one job on each node this equals the local size.
test_expect_success NEED_NODE_SIZE 'node: pmix.node.size (not yet provided)' '
	getkey_cmp pmix.node.size lsize.exp $NODEPROC
'

#
# Keys that are absent by design.  PMIX_RANK is the identifying first
# element of each proc info array, which openpmix consumes; a client
# has its rank from PMIx_Init.  The app realm array, with
# PMIX_APP_SIZE, PMIX_APPLDR, and PMIX_APP_ARGV, is required only with
# more than one application ("Application realm attributes"), and this
# is one.
#
test_expect_success 'keys absent by design are absent' '
	test_must_fail run2n3p ${GETKEY} --rank=0 pmix.rank &&
	for key in pmix.app.size pmix.aldr pmix.argv; do
		test_must_fail run2n3p ${GETKEY} --app --proc=* --rank=0 $key \
			|| return 1
	done
'

test_done
