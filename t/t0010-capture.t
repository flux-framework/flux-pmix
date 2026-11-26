#!/bin/sh

test_description='Test the pmix.wirecapture shell option.'

. `dirname $0`/sharness.sh

BARRIER=${FLUX_BUILD_DIR}/t/src/barrier
REQUESTS=${FLUX_BUILD_DIR}/t/src/requests
export FLUX_SHELL_RC_PATH=${FLUX_BUILD_DIR}/t/etc

test_under_flux 2

test_expect_success 'LD_PRELOAD is not set unless capture is requested' '
	run_timeout 30 flux run -N1 -n1 printenv >env.out &&
	test_must_fail grep ^LD_PRELOAD env.out
'

# The plugin is loaded from the build tree here (see t/etc/rc.lua),
# so it must point LD_PRELOAD at the build-tree shim rather than the
# one in libexecdir, which need not exist.
test_expect_success 'uninstalled plugin preloads the build-tree shim' '
	run_timeout 30 flux run -N1 -n1 -o pmix.wirecapture=x.pcapng \
		printenv LD_PRELOAD >preload.out &&
	echo ${FLUX_BUILD_DIR}/src/capture/.libs/pmix-capture.so \
		>preload.exp &&
	test_cmp preload.exp preload.out
'

# pmix.wirecapture writes each connection's bytes to a pcapng file,
# one per shell, which Wireshark can read.  Without tshark, only the
# file's existence and size can be checked here.
test_expect_success 'pmix.wirecapture writes a capture file per shell' '
	run_timeout 30 flux run -N2 -n2 \
		-o pmix.wirecapture="cap-{{node.id}}.pcapng" ${BARRIER} &&
	test -s cap-0.pcapng &&
	test -s cap-1.pcapng
'

test_expect_success 'capture logs nothing to the shell' '
	run_timeout 30 flux run -N1 -n1 \
		-o pmix.wirecapture=quiet.pcapng ${BARRIER} 2>quiet.err &&
	test -s quiet.pcapng &&
	test_must_fail grep "pmix:" quiet.err
'

test_expect_success 'pmix.wirecapture to an unwritable path warns' '
	run_timeout 30 flux run -N1 -n1 \
		-o pmix.wirecapture=/nonexistent/dir/x.pcapng \
		${BARRIER} 2>badpath.err &&
	grep "could not enable pmix capture" badpath.err
'

test_expect_success 'captured job produces the same result as uncaptured' '
	run_timeout 30 flux run -N1 -n2 ${BARRIER} >plain.out 2>/dev/null &&
	run_timeout 30 flux run -N1 -n2 -o pmix.wirecapture=same.pcapng \
		${BARRIER} >captured.out 2>/dev/null &&
	test_cmp plain.out captured.out
'

# Each connection is an interface named for its rank, pid, and fd;
# open and close are empty packets; reads are received and writes
# sent.  The direction is read from the pcapng packet flags, which
# every tshark has; the exported-PDU tags for direction and info
# text are understood only from Wireshark 3.6 and 4.2.  The open
# and the close have no direction.
test_expect_success HAVE_TSHARK 'tshark reads the capture' '
	tshark -r cap-0.pcapng -T fields -E separator=/t \
		-e frame.interface_name -e frame.p2p_dir >cap-0.tsv &&
	test $(grep -c "^rank0-pid[0-9]*-fd[0-9]*	$" cap-0.tsv) -eq 2 &&
	grep -q "^rank0-pid[0-9]*-fd[0-9]*	0$" cap-0.tsv &&
	grep -q "^rank0-pid[0-9]*-fd[0-9]*	1$" cap-0.tsv
'

# Timestamps are 64-bit microseconds split across two words, so a
# mistake there puts a packet decades away from now.
test_expect_success HAVE_TSHARK 'tshark reads a timestamp from this hour' '
	tshark -r cap-0.pcapng -T fields -e frame.time_epoch \
		| head -1 | cut -d. -f1 >epoch.out &&
	now=$(date +%s) &&
	test $(cat epoch.out) -gt $((now - 3600)) &&
	test $(cat epoch.out) -le $now
'

test_expect_success HAVE_TSHARK 'tshark reads the dissector name' '
	tshark -r cap-0.pcapng -T fields -e exported_pdu.prot_name \
		| sort -u >proto.out &&
	test "$(cat proto.out)" = "pmix"
'

if test_have_prereq HAVE_TSHARK \
	&& tshark -G fields | grep -q exported_pdu.col_info_str; then
	test_set_prereq TSHARK_INFO_TAG
fi

test_expect_success TSHARK_INFO_TAG 'tshark reads the info text of each packet' '
	tshark -r cap-0.pcapng -T fields -E separator=/t \
		-e frame.p2p_dir -e exported_pdu.col_info_str >info.tsv &&
	grep -q "^	rank 0 open$" info.tsv &&
	grep -q "^0	rank 0 write$" info.tsv &&
	grep -q "^1	rank 0 read$" info.tsv &&
	grep -q "^	rank 0 close$" info.tsv
'

# The first write is the connect request, whose header begins
# with the peer index, -1 for a client that has none yet.
test_expect_success HAVE_TSHARK 'capture holds the raw bytes of each message' '
	tshark -r cap-0.pcapng -T fields -e exported_pdu.exported_pdu \
		-Y "frame.p2p_dir == 0" | head -1 >first.hex &&
	test "$(cut -c1-8 first.hex)" = "ffffffff"
'

# A task that writes something other than records to the channel
# puts it out of sync, and the capture for that task stops.  The
# shell must go on reading the channel regardless, or the task blocks
# on its next write and never exits: here the task writes 8 MiB of
# garbage, twice the channel buffer, and must still finish.
test_expect_success 'a task that corrupts its channel still exits' '
	run_timeout 30 flux run -N1 -n1 -o pmix.wirecapture=sync.pcapng \
		bash -c "head -c 8388608 /dev/zero >&\$FLUX_PMIX_CAPTURE_FD" \
		2>sync.err &&
	grep "out of sync, capture stopped" sync.err
'

# Tasks' records reach the shell in no particular order, so the
# plugin sorts them, which mergecap needs its inputs to be.  Two
# tasks is what a CI node has cores for.
test_expect_success HAVE_CAPINFOS 'capture is in time order' '
	run_timeout 30 flux run -N1 -n2 \
		-o pmix.wirecapture=four.pcapng ${BARRIER} &&
	capinfos -oT four.pcapng >order.out &&
	grep "True" order.out
'

test_expect_success HAVE_MERGECAP,HAVE_TSHARK 'captures of a job merge' '
	mergecap -w merged.pcapng cap-0.pcapng cap-1.pcapng &&
	tshark -r merged.pcapng -T fields -e frame.interface_name \
		| sort -u >ifaces.out &&
	test $(wc -l <ifaces.out) -eq 2 &&
	grep -q ^rank0- ifaces.out &&
	grep -q ^rank1- ifaces.out
'

# The Wireshark dissector reads the capture.  It reassembles each
# connection's bytes into messages and decodes them.  The hash store
# is asked for so that the job information is on the wire, where the
# tests look for it; the shared memory stores pass it through a
# segment instead.
#
# An installed flux-pmix puts a copy of the dissector in the global
# plugin directory, where tshark loads it at startup, and a second
# copy of a protocol fails to load.  Point the global directory at
# an empty one so the source tree's copy is the only one.  (Sharness
# has already pointed HOME, and so the personal directory, at the
# trash directory.)
#
# A distribution can build tshark without Lua (el9 does), in which
# case the dissector cannot run and its tests are skipped.
DISSECTOR=${FLUX_SOURCE_DIR}/src/capture/pmix.lua
mkdir -p noplugins
export WIRESHARK_PLUGIN_DIR=$(pwd)/noplugins
if test_have_prereq HAVE_TSHARK \
	&& tshark -X lua_script:/dev/null -v >/dev/null 2>&1; then
	test_set_prereq TSHARK_LUA
fi
tshark_pmix() {
	tshark -X lua_script:${DISSECTOR} -T fields -e _ws.col.Info "$@" \
		| grep -v "(continued)" | grep -v "^rank [0-9]* "
}
# The dissector must decode whatever a correct server sends, and what
# that is varies with the server's openpmix.  The encoding, and with it
# message lengths, depend on what the server offers; a 3.2 server
# closes the connection without sending the finalize reply; only a 4.0
# or later server puts the session array in the job-info reply; and
# servers answer the 5.0 resolve requests with different statuses.  So
# the checks below look for messages decoded and named, with any value
# where versions differ, and require of every capture that nothing in
# it failed to decode, which holds on every version.
LEN="[0-9]*"
STATUS="-\{0,1\}[0-9]*"
tshark_clean() {
	tshark -X lua_script:${DISSECTOR} -r "$1" -T fields -e frame.number \
		-Y "pmix.undecoded || pmix.malformed || _ws.malformed || _ws.lua.error" \
		>"$1.bad" &&
	test_must_be_empty "$1.bad"
}
# A finalize reply need not be sent, but must decode if it is.
finalize_reply_ok() {
	if grep -q "finalize-reply" "$1"; then
		grep "< tag=$LEN len=$LEN finalize-reply status=0" "$1"
	fi
}

test_expect_success TSHARK_LUA 'dissector decodes the connect handshake' '
	run_timeout 30 flux run -N1 -n1 -o pmix.store=hash \
		-o pmix.wirecapture=hash.pcapng ${BARRIER} &&
	tshark_pmix -r hash.pcapng >dis.out &&
	grep "> connect .*security=native encoding=v[0-9]* mode=undescribed" dis.out &&
	grep "< connect-response status=0 pindex=0" dis.out &&
	tshark_clean hash.pcapng
'

test_expect_success TSHARK_LUA 'dissector decodes requests and replies' '
	grep "> tag=101 len=$LEN job-info$" dis.out &&
	grep "< tag=101 len=$LEN job-info-reply nspace=.* keys$" dis.out &&
	grep "> tag=102 len=$LEN fence procs=\[.*\.\*\]" dis.out &&
	grep "< tag=102 len=$LEN fence-reply status=0" dis.out &&
	grep "> tag=103 len=$LEN finalize$" dis.out &&
	finalize_reply_ok dis.out
'

# The label is the namespace and rank from the connect request; the
# namespace is the jobid, as F58 or plain depending on the locale.
test_expect_success TSHARK_LUA 'dissector labels messages with the process' '
	grep -c "^[^ ]*\.0: " dis.out >count.out &&
	test $(cat count.out) -eq $(grep -c "^" dis.out)
'

test_expect_success TSHARK_LUA 'dissector decodes the job-info reply fields' '
	tshark -X lua_script:${DISSECTOR} -r hash.pcapng -V \
		-Y "pmix.reply_to == 0" >jobinfo.out &&
	grep "pmix.job.size (UINT32): 1" jobinfo.out &&
	grep "pmix.node.arr (DATA_ARRAY): INFO\[" jobinfo.out &&
	grep "pmix.pblob (BYTE_OBJECT): rank 0, .* keys" jobinfo.out &&
	grep "pmix.lrank (UINT16): 0" jobinfo.out
'

test_expect_success TSHARK_LUA 'dissector decodes a v3 capture' '
	run_timeout 30 flux run -N1 -n1 -o pmix.wire=v3 -o pmix.store=hash \
		-o pmix.wirecapture=v3.pcapng ${BARRIER} &&
	tshark_pmix -r v3.pcapng >v3.out &&
	grep "> connect .*encoding=v3 " v3.out &&
	grep "job-info-reply nspace=.* keys$" v3.out &&
	grep "fence-reply status=0" v3.out &&
	finalize_reply_ok v3.out &&
	tshark_clean v3.pcapng
'

# The requests program sends one of each request the server need not
# implement, so this covers the decoders of the whole command set,
# on the client's own packing.
# A client library older than 4.0 lacks some of the requests, and the
# program says which it skipped, so only those it sent are looked for.
test_expect_success TSHARK_LUA 'dissector decodes every request' '
	run_timeout 30 flux run -N1 -n1 -o pmix.store=hash \
		-o pmix.wirecapture=req.pcapng ${REQUESTS} >req.log 2>&1 &&
	tshark_pmix -r req.pcapng >req.out &&
	grep "> .* publish uid=[0-9]* {test.key=" req.out &&
	grep "> .* lookup uid=[0-9]* keys=\[\"test.key\"\]" req.out &&
	grep "> .* unpublish uid=" req.out &&
	grep "> .* spawn {pmix.spwn.tool=false} apps=1" req.out &&
	grep "> .* connect procs=\[" req.out &&
	grep "> .* disconnect procs=\[" req.out &&
	grep "> .* log time=.* data=1" req.out &&
	grep "> .* allocate directive=new {pmix.alloc.id=" req.out &&
	grep "> .* job-control targets=\[.*{pmix.jctrl.pause=true}" req.out &&
	grep "> .* monitor pmix.monitor.mbeat=.* error=" req.out &&
	! grep "undecoded" req.out &&
	tshark_clean req.pcapng
'

sent() {
	grep -q "$1: [A-Z]" req.log
}

test_expect_success TSHARK_LUA 'dissector decodes the 4.0 and later requests' '
	if sent PMIx_Query_info; then
		grep "> .* query keys=\[\"pmix.qry.psets\"\] qualifiers=1" req.out &&
		grep "< .* query-reply status=.* results=0" req.out
	fi &&
	if sent PMIx_Get_credential; then
		grep "> .* get-credential$" req.out &&
		grep "> .* validate-credential credential=<3 bytes>" req.out
	fi &&
	if sent PMIx_Group_construct; then
		grep "> .* group-construct group=\"grp\" procs=\[.*actxid=true}" req.out
	fi &&
	if sent PMIx_Fabric_register; then
		grep "> .* fabric-register$" req.out
	fi
'

# A 5.0 client has PMIx_Session_control() but fails it locally, since
# the command is 6.0, and the resolve calls are sent only from 5.0,
# so these are checked only when seen.  How a server answers a resolve
# varies with its version (one older than 5.0 does not know the
# command), so a reply's contents are looked for only on success.
test_expect_success TSHARK_LUA 'dissector decodes the later requests when sent' '
	if grep -q "> .* session-control" req.out; then
		grep "> .* session-control session=0 {pmix.ssn.pause=true}" req.out
	fi &&
	if grep -q "> .* resolve-peers" req.out; then
		grep "> .* resolve-peers node=.* nspace=" req.out &&
		grep "< .* resolve-peers-reply status=$STATUS" req.out &&
		if grep -q "resolve-peers-reply status=0" req.out; then
			grep "< .* resolve-peers-reply status=0 procs=\[.*\.0\]" req.out
		fi
	fi &&
	if grep -q "> .* resolve-node" req.out; then
		grep "> .* resolve-node nspace=" req.out &&
		grep "< .* resolve-node-reply status=$STATUS" req.out &&
		if grep -q "resolve-node-reply status=0" req.out; then
			grep "< .* resolve-node-reply status=0 nodes=" req.out
		fi
	fi
'

test_expect_success TSHARK_LUA 'dissector decodes a v21 capture' '
	run_timeout 30 flux run -N1 -n1 -o pmix.wire=v21 -o pmix.store=hash \
		-o pmix.wirecapture=v21.pcapng ${BARRIER} &&
	tshark_pmix -r v21.pcapng >v21.out &&
	grep "> connect .*encoding=v21 " v21.out &&
	grep "job-info-reply nspace=.* keys$" v21.out &&
	grep "fence-reply status=0" v21.out &&
	finalize_reply_ok v21.out &&
	tshark_clean v21.pcapng
'

# The newest encoding the dissector knows, when the server offers it.
# The server URI variables in a task's environment say which it does.
if test_have_prereq TSHARK_LUA \
	&& run_timeout 30 flux run -N1 -n1 printenv PMIX_SERVER_URI41 \
		>/dev/null 2>&1; then
	test_set_prereq SERVER_V41
fi
test_expect_success TSHARK_LUA,SERVER_V41 'dissector decodes a v41 capture' '
	run_timeout 30 flux run -N1 -n1 -o pmix.wire=v41 -o pmix.store=hash \
		-o pmix.wirecapture=v41.pcapng ${BARRIER} &&
	tshark_pmix -r v41.pcapng >v41.out &&
	grep "> connect .*encoding=v41 " v41.out &&
	grep "job-info-reply nspace=.* keys$" v41.out &&
	grep "fence-reply status=0" v41.out &&
	finalize_reply_ok v41.out &&
	tshark_clean v41.pcapng
'

test_expect_success TSHARK_LUA 'dissector display filters select messages' '
	tshark -X lua_script:${DISSECTOR} -r hash.pcapng \
		-Y "pmix.cmd == 3" -T fields -e _ws.col.Info >fence.out &&
	test $(grep -c fence fence.out) -eq 1 &&
	tshark -X lua_script:${DISSECTOR} -r hash.pcapng \
		-Y "pmix.key == \"pmix.job.size\"" -T fields -e frame.number \
		>bykey.out &&
	test $(wc -l <bykey.out) -eq 1
'

# A record arriving split across reads of the channel must be
# reassembled, not dropped.  Splits are a scheduling race in normal
# use, so force them by shrinking the channel's read buffer below the
# size of any record: every record then arrives in pieces.  What
# proves the reassembly is that the capture still decodes whole,
# since a dropped or misframed record would leave the rest of the
# connection unreadable.  The shell says nothing about any of this -
# reassembly is ordinary stream handling, not an event.
test_expect_success TSHARK_LUA 'capture reassembles split records' '
	run_timeout 30 flux run -N1 -n1 -o pmix.store=hash \
		-o pmix.wirecapture=split.pcapng -o pmix.wirecapture-bufsize=16 \
		${BARRIER} 2>split.err &&
	test_must_fail grep "pmix:" split.err &&
	tshark_pmix -r split.pcapng >splitdis.out &&
	grep "> connect .*storage=hash" splitdis.out &&
	grep "job-info-reply nspace=.* keys$" splitdis.out &&
	grep "> tag=103 len=$LEN finalize$" splitdis.out &&
	finalize_reply_ok splitdis.out &&
	tshark_clean split.pcapng
'
# The dissector reassembles too: a message split across TCP segments
# is more than one packet in the capture, shown as continued.
test_expect_success TSHARK_LUA 'dissector reassembles messages split across packets' '
	tshark -X lua_script:${DISSECTOR} -r split.pcapng \
		-T fields -e _ws.col.Info | grep -c "(continued)" >cont.out &&
	test $(cat cont.out) -gt 0
'

test_done
