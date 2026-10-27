==================
flux-shell-pmix(7)
==================

SYNOPSIS
========

**flux** **run** *-opmi=pmix* [*-opmix.OPTION=VALUE* ...]

DESCRIPTION
===========

:program:`pmix` is a :core:man1:`flux-shell` plugin.  It runs a PMIx
server in each job shell, to launch programs built with Open MPI or
other PMIx clients.

The server is the OpenPMIx reference server, linked into the plugin.
The OpenPMIx version that flux-pmix was built with determines the wire
encodings, data stores, and authentication methods the server offers.
A client selects from those when it connects.

The plugin implements only what Open MPI needs to start.  Do not rely
on it for other PMIx features.

SHELL OPTIONS
=============

.. option:: pmi=pmix

  Enable the pmix plugin.  To enable other PMI implementations as
  well, separate them with commas, e.g. ``pmi=pmix,simple``.

.. option:: pmix.wire=LIST

  Offer only the listed wire encodings.  LIST is a comma separated
  subset of **v2**, **v21**, **v3**, **v4**, **v41**, **v51**, and
  **v61**.  The value is passed to OpenPMIx as
  :envvar:`PMIX_MCA_bfrops`, so the server loads only those
  encodings.  Each task sees only the server URIs for them, so a
  client can select only those.  The job fails at startup if the
  server has none of them.

.. option:: pmix.store=LIST

  Offer only the listed data stores.  LIST is a comma separated
  subset of the stores the server's OpenPMIx has, e.g. **hash**.  The
  value is passed to OpenPMIx as :envvar:`PMIX_MCA_gds`.  If **hash**
  is in LIST, it must be last, e.g. **shmem2,hash**.  The stores are
  **ds21** in 3.x and 4.x, **shmem2** in 5.x and 6.x, **shmem3** in
  7.x, and **hash** in all versions.

.. option:: pmix.auth=LIST

  Offer only the listed authentication methods.  LIST is a comma
  separated subset of the methods the server's OpenPMIx has, e.g.
  **native**, **munge**, or **none**.  The value is passed to OpenPMIx
  as :envvar:`PMIX_MCA_psec`.  A method the server does not have is
  ignored.

  The client chooses any one of the offered methods, and fails to
  connect if it has no method in common with the server.  What is
  offered by default depends on the OpenPMIx version and build
  options, but is usually **native** or **munge,native**.  This option
  is mainly for testing a client with a given method.

  The method authenticates: it establishes who the connecting process
  is.  The server then authorizes, admitting only the job's own user,
  and that does not vary with the method.  What varies is how well the
  claim is established.

  **munge**
    The client presents a credential signed by the local munge daemon,
    which names its user and group.  Available when OpenPMIx was built
    with munge support and the daemon is running.

  **native**
    OpenPMIx's built-in method, with no external dependency.  What it
    verifies about the client depends on the OpenPMIx version.

  **none**
    The client's word is taken.  This is a deliberate bypass, so
    OpenPMIx offers it only when it is listed here, and newer versions
    may not have it.  It is for when the other methods cannot work,
    as they may for a client in a container.  **none** does not change
    what the server listens on; the client must still reach it.

  The server is the user's own: it runs in the job shell with the
  user's credentials and no more.  What a weak method risks is that
  another user on the same node connects as the job's user and gains
  control of the job.  On a node the job has to itself, there is no
  such user.

.. option:: pmix.wirecapture=PATH

  Record every message between the tasks and the server in the file
  PATH, in the pcapng format that Wireshark reads.  The shell
  preloads a small library into each task that copies what the task
  sends and receives on its PMIx connection; the client itself is
  not changed and need not be rebuilt.  If PATH cannot be opened,
  the shell warns and the job runs without a capture.

  Each shell writes its own file, so when the job spans more than one
  node, PATH should be a mustache template that includes the node id
  (see MUSTACHE TEMPLATES in :core:man1:`flux-submit`).

  flux-pmix installs a dissector for :linux:man1:`wireshark` that
  decodes the capture.  :linux:man1:`tshark` and :linux:man1:`mergecap`
  are part of Wireshark.

.. option:: pmix.wirecapture-bufsize=BYTES

  Set the size of the buffer the shell reads each task's capture
  through, in bytes.  The default is 4 MiB, large enough that a
  record is nearly always read whole.  A smaller size forces records
  to arrive in pieces, to exercise reassembly in a test.

.. option:: pmix.log-dedup=off

  Write a tagged log message every time it is sent, instead of once
  per job (see LOGGING).  The default is **on**.

LOGGING
=======

:c:func:`PMIx_Log` writes a message to one or more channels.  The
server is part of the job, so each channel is a part of the job's
output:

PMIX_LOG_STDERR, PMIX_LOG_STDOUT
  The calling task's stderr or stdout.

PMIX_LOG_SYSLOG, PMIX_LOG_LOCAL_SYSLOG, PMIX_LOG_GLOBAL_SYSLOG
  The shell log, at the PMIX_LOG_SYSLOG_PRI priority (default
  LOG_ERR).  Nothing is written to the node's syslog.

PMIX_LOG_JOB_RECORD
  The shell log, at LOG_NOTICE.

The shell log is in the job's output eventlog.  A message on both a
syslog channel and the job record is written once, at the syslog
priority.  Other channels fail.  PMIX_LOG_ONCE writes the first
channel that is supported and ignores the rest.

A message tagged with PMIX_LOG_KEY and PMIX_LOG_VAL is written once
per job.  At exit, the shell logs one line per tag that was dropped,
for example::

  0.045s: flux-shell[0]: pmix: 1 more node sent help message help-mpi-btl-tcp.txt / invalid minimum port

Open MPI tags its help messages.  To see them from every rank, run
with ``--env=OMPI_MCA_opal_base_help_aggregate=0``.

EXAMPLE
=======

Capture a four task Open MPI job on two nodes.  Each node writes a
file, and ``mergecap`` combines them into one::

  $ flux run -N2 -n4 -o pmi=pmix \
      -o pmix.wirecapture='{{id}}-{{node.id}}.pcapng' ./a.out
  $ mergecap -w job.pcapng ƒB6c7KD-*.pcapng

List what rank 0 said to the server and what it got back.  ``>`` is a
request and ``<`` the reply, matched by tag, and the time is seconds
since the first packet::

  $ tshark -r job.pcapng -t r.3 -T fields -e _ws.col.Time -e _ws.col.Info \
      -Y 'pmix.msg && frame.interface_name contains "rank0-"'
  0.001  ƒB6c7KD.0: > tag=101 len=2 job-info
  0.005  ƒB6c7KD.0: < tag=101 len=186389 job-info-reply nspace="ƒB6c7KD" 3 keys
  0.013  ƒB6c7KD.0: > tag=106 len=316 commit local=4 keys remote=2 keys
  0.013  ƒB6c7KD.0: < tag=106 len=2 commit-reply status=0
  0.013  ƒB6c7KD.0: > tag=107 len=40 fence procs=[ƒB6c7KD.*] {pmix.collect=true}
  0.015  ƒB6c7KD.0: < tag=107 len=2 fence-reply status=0
  0.018  ƒB6c7KD.0: > tag=108 len=50 get proc=ƒB6c7KD.1 {pmix.immediate=true} key="btl.sm.5.0"
  0.018  ƒB6c7KD.0: < tag=108 len=230 get-reply status=0 1 procs
  0.032  ƒB6c7KD.0: > tag=112 len=31 get proc=ƒB6c7KD.2 key="btl.tcp.5.0"
  0.032  ƒB6c7KD.0: < tag=112 len=64 get-reply status=0 1 procs
  0.120  ƒB6c7KD.0: > tag=113 len=22 fence procs=[ƒB6c7KD.*]
  0.121  ƒB6c7KD.0: < tag=113 len=2 fence-reply status=0
  0.121  ƒB6c7KD.0: > tag=115 len=2 finalize
  0.161  ƒB6c7KD.0: < tag=115 len=2 finalize-reply status=0

Some lines are omitted.  This is MPI_Init through MPI_Finalize as the
server sees it: the task fetches the job layout, publishes its own
endpoints, fences to exchange them, looks up a shared memory endpoint
on its node and a TCP endpoint on the other, and a barrier later
leaves.  Dropping the ``-Y`` filter lists every rank, ``-O pmix``
expands each message's fields, and ``wireshark job.pcapng`` opens the
same capture interactively.  :doc:`/guide/tracing` has more.

RESOURCES
=========

Flux: http://flux-framework.org

OpenPMIx: https://openpmix.github.io

:doc:`/guide/tracing`

SEE ALSO
========

:core:man1:`flux-shell`, :core:man7:`flux-shell-options`,
:core:man1:`flux-submit`, :linux:man1:`wireshark`, :linux:man1:`tshark`,
:linux:man1:`mergecap`
