Tracing the PMIx Protocol
=========================

flux-pmix can capture all the messages between the PMIx clients of a
job and its server.  Wireshark decodes the capture.  This page gives
the commands for the common cases.

How it works
------------

The ``pmix.wirecapture`` shell option preloads a small library into
each task.  The library copies each byte that the task reads from or
writes to its PMIx socket.  The shell writes the bytes to a pcapng
file.  pcapng is the capture format of Wireshark and tcpdump, defined
in `draft-ietf-opsawg-pcapng
<https://datatracker.ietf.org/doc/draft-ietf-opsawg-pcapng/>`_.  In
the file, each connection is an interface, and each read or write is
a packet.

The client is not changed, and it is not rebuilt.  The capture works
with any PMIx client that uses the socket.

The Wireshark dissector ``pmix.lua`` decodes the capture.  flux-pmix
installs it in the global Lua plugin directory of Wireshark, usually
``/usr/lib64/wireshark/plugins``.  Wireshark and ``tshark`` load it
from there at startup.  On a node that has the flux-pmix package,
no setup is necessary.

On a workstation that does not have the package, copy the file into
the personal Lua plugin directory, usually
``~/.local/lib/wireshark/plugins``.  Help > About Wireshark > Folders
shows the directory.  As an alternative, load the file for one run
with ``tshark -X lua_script:pmix.lua``.

Capture a job
-------------

Each shell writes its own file.  Thus, give a file name that is a
mustache template with the job id and the node id.  See MUSTACHE
TEMPLATES in :core:man1:`flux-submit`::

   $ flux run -N4 -n16 -o pmi=pmix \
       -o pmix.wirecapture='{{id}}-{{node.id}}.pcapng' ./a.out

This writes ``<jobid>-0.pcapng`` through ``<jobid>-3.pcapng`` in the
working directory of the job.  A one-node job can use a plain file
name.

Merge the files of a job
------------------------

``mergecap`` is part of Wireshark.  It merges the files in time
order and renumbers their interfaces.  The result is one capture for
the whole job::

   $ mergecap -w job.pcapng <jobid>-*.pcapng

The timestamps are from the realtime clock of each node.  The merge
is accurate only if the clocks of the nodes agree.

List the messages
-----------------

This shows one line for each message, with a relative timestamp.
The dissector labels each message with the namespace and rank of
its client.  It pairs each reply with its request::

   $ tshark -r job.pcapng -Y pmix.msg \
       -T fields -e frame.time_relative -e _ws.col.Info
   0.000628   ƒB6c7KD.1: > tag=101 len=2 job-info
   0.000649   ƒB6c7KD.1: < tag=101 len=1050 job-info-reply nspace="ƒB6c7KD" 16 keys
   0.000816   ƒB6c7KD.3: > tag=101 len=2 job-info
   0.000885   ƒB6c7KD.3: < tag=101 len=1050 job-info-reply nspace="ƒB6c7KD" 16 keys
   ...
   0.006583   ƒB6c7KD.3: > tag=107 len=40 fence procs=[ƒB6c7KD.*] {pmix.collect=true}
   0.007078   ƒB6c7KD.3: < tag=107 len=2 fence-reply status=0
   0.007548   ƒB6c7KD.3: > tag=108 len=32 get proc=ƒB6c7KD.0 key="pml.base.2.0"
   0.007551   ƒB6c7KD.3: < tag=108 len=85 get-reply status=0 1 procs

``>`` is a request from the client and ``<`` a reply from the
server.  The ``pmix.msg`` filter selects the packet that completes
each message.  A message that arrived in more than one read shows
``(continued)`` in the packets before that one; ``-Y pmix`` shows
those too.

Show the messages in full
-------------------------

This shows each message with its contents, in time order.  ``-O
pmix`` expands only the PMIx part of each packet, and the ``grep``
removes two lines that say nothing::

   $ tshark -r job.pcapng -O pmix -Y pmix.msg | grep -v '^EXPORTED_PDU\|^$'
   Frame 120: 76 bytes on wire (608 bits), 76 bytes captured (608 bits) on interface rank3-pid1515245-fd14, id 4
   PMIx > tag=107 len=40 fence procs=[ƒB6c7KD.*] {pmix.collect=true}
       Info (1)
           pmix.collect (BOOL): true
   Frame 126: 38 bytes on wire (304 bits), 38 bytes captured (304 bits) on interface rank3-pid1515245-fd14, id 4
   PMIx < tag=107 len=2 fence-reply status=0
   Frame 134: 68 bytes on wire (544 bits), 68 bytes captured (544 bits) on interface rank3-pid1515245-fd14, id 4
   PMIx > tag=108 len=32 get proc=ƒB6c7KD.0 key="pml.base.2.0"
   Frame 136: 121 bytes on wire (968 bits), 121 bytes captured (968 bits) on interface rank3-pid1515245-fd14, id 4
   PMIx < tag=108 len=85 get-reply status=0 1 procs
       Data for ƒB6c7KD.0 (2 keys)
           btl.tcp.5.0 (BYTE_OBJECT): <32 bytes>
           pml.base.2.0 (BYTE_OBJECT): <4 bytes>

The line for each packet names its interface, which is the rank,
and gives its time.

Follow one rank
---------------

Each connection is an interface.  The interface name has the rank,
the pid, and the descriptor.  Thus, the packets of a rank are the
packets on its interface::

   $ tshark -r job.pcapng -Y 'pmix.msg && frame.interface_name contains "rank3-"' \
       -T fields -e frame.time_relative -e _ws.col.Info
   0.000816   ƒB6c7KD.3: > tag=101 len=2 job-info
   0.000885   ƒB6c7KD.3: < tag=101 len=1050 job-info-reply nspace="ƒB6c7KD" 16 keys
   0.006376   ƒB6c7KD.3: > tag=106 len=275 commit local=3 keys remote=1 keys
   0.006379   ƒB6c7KD.3: < tag=106 len=2 commit-reply status=0
   0.006583   ƒB6c7KD.3: > tag=107 len=40 fence procs=[ƒB6c7KD.*] {pmix.collect=true}
   0.007078   ƒB6c7KD.3: < tag=107 len=2 fence-reply status=0

Show one message in full
------------------------

Use display filter fields to select the messages:

``pmix.cmd``
  The command of a request.

``pmix.reply_to``
  The command that a reply answers.

``pmix.key``, ``pmix.value``
  The entries of info arrays and of the job information.

The command codes are those of OpenPMIx: job-info is 0, fence is 3,
get is 4, and finalize is 5.  This shows the job information that a
client received::

   $ tshark -r job.pcapng -O pmix \
       -Y 'pmix.reply_to == 0 && frame.interface_name contains "rank0-"'
   Frame 12: 1086 bytes on wire (8688 bits), 1086 bytes captured (8688 bits) on interface rank0-pid1515244-fd14, id 0
   EXPORTED_PDU
   PMIx < tag=101 len=1050 job-info-reply nspace="ƒB6c7KD" 16 keys
       Job information (16 keys)
           pmix.jobid (STRING): "ƒB6c7KD"
           pmix.job.size (UINT32): 4
           pmix.num.nodes (UINT32): 2
           pmix.nlist (STRING): "node0,node1"
           ...
           pmix.node.arr (DATA_ARRAY): INFO[4]
               pmix.hname (STRING): "node0"
               pmix.lpeers (STRING): "0,1"
               pmix.lldr (PROC_RANK): 0
               pmix.local.size (UINT32): 2
           ...
           pmix.pblob (BYTE_OBJECT): rank 0, 5 keys
               pmix.nodeid (UINT32): 0
               pmix.lrank (UINT16): 0
               pmix.nrank (UINT16): 0
               pmix.appnum (UINT32): 0
               pmix.hname (STRING): "node0"
           ...

The keys are those of the PMIx standard; see :doc:`wire_protocol`
for how they are carried, and for what each request and reply
holds.

This shows each message that has a given key::

   $ tshark -r job.pcapng -O pmix -Y 'pmix.key == "pmix.hname"'

Time a phase
------------

The ``io,stat`` statistics count packets in each time interval.
Each display filter that you give is a column.  Thus, you can plot a
phase of the bootstrap against time.  This counts job-info requests,
their replies, fence requests, and fence replies in intervals of
10 ms::

   $ tshark -r job.pcapng -q \
       -z io,stat,0.01,'pmix.cmd==0','pmix.reply_to==0','pmix.cmd==3','pmix.reply_to==3'
   | Col 1: pmix.cmd==0                                                               |
   |     2: pmix.reply_to==0                                                          |
   |     3: pmix.cmd==3                                                               |
   |     4: pmix.reply_to==3                                                          |
   |----------------------------------------------------------------------------------|
   |              |1               |2               |3               |4               |
   | Interval     | Frames | Bytes | Frames | Bytes | Frames | Bytes | Frames | Bytes |
   |----------------------------------------------------------------------------------|
   | 0.00 <> 0.01 |      4 |   152 |      4 |  4344 |      6 |   456 |      4 |   152 |
   | 0.01 <> 0.02 |      0 |     0 |      0 |     0 |     10 |   760 |     12 |   456 |

Capture a particular configuration
----------------------------------

The connect request in the capture shows the encoding, the security
method, and the data store that the client and server selected.  The
``pmix.wire``, ``pmix.auth``, and ``pmix.store`` shell options of
:man7:`flux-shell-pmix` limit what the server offers.  Thus, you can
capture a client against a given configuration.

A client that does not match the server receives the job information
on the socket, not in a shared memory segment.  To capture that case,
select the hash store::

   $ flux run -N1 -n4 -o pmi=pmix -o pmix.store=hash \
       -o pmix.wirecapture=hash.pcapng ./a.out

Open in Wireshark
-----------------

Open the file in Wireshark::

   $ wireshark job.pcapng

The packet list shows one line for each message.  The details tree
shows each field of the selected packet.  Statistics > Conversations
and Statistics > I/O Graph work with the interfaces and the display
filter fields above.  Follow > TCP Stream is not available, because
the packets are not TCP.  To see one connection, filter on
``frame.interface_name``.
