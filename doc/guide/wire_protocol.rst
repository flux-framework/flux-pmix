The OpenPMIx Wire Protocol
==========================

The PMIx Standard [#std]_ defines a C interface.  It does not define
the protocol between a PMIx client library and its server.  That
protocol is private to OpenPMIx, the library that Open MPI and other
programs link, and OpenPMIx does not document it.  This page
describes it, for the reader of a capture (see :doc:`tracing`), and
as the reference for the dissector that decodes one.

The protocol is nonetheless shaped by the standard.  Every value on
the wire is one of the standard's data types, with the standard's
type code; the structures it carries, such as a process identifier
or an info, are the standard's structures, field by field; and the
constants in them, such as a status, a scope, or a reserved rank,
are the standard's constants.  What OpenPMIx adds is the byte layout
of each type, the framing of a message, and the request and reply
sequence of each command.  This page gives those, and names the
standard's definition where one applies.

Scope
-----

This page describes the protocol as OpenPMIx 2.1 through 6.1
implement it.  OpenPMIx declares every series before 7.0 to be end
of life, with no further changes, so these versions can be described
as fixed.  OpenPMIx 7.0 is not described: it is a release candidate
at the time of writing, and what it adds can still change.

The facts here are from the OpenPMIx source, chiefly the v2.2,
v3.2, v4.2, v5.0, and v6.1 release branches, and were checked
against captures from OpenPMIx 5.0.7.  The footnotes link to the
source at the 5.0.7 tag.  OpenPMIx names the parts of the protocol
after its own components; this page uses those names, which also
appear in the connect request on the wire.

References to the standard are to version 5.0, the latest release
at the time of writing and the version current for OpenPMIx 5.0,
against which the captures were checked.  A section number such as
(standard 3.2.9) is a section of that document.

The page covers simple clients: processes that the server's host
launched, with an identity registered in advance, which is every
process Flux launches.  OpenPMIx also serves tools, launchers, and
schedulers, which announce themselves with a different process type
in the connect request, send a different identity, and get a longer
connect response; `Process types`_ gives the types and the
differences, and the rest of the page does not cover them.  The
page describes every command a client can send, including those
flux-pmix does not implement, since a capture from a program that
uses one still has to be read.

Transport
---------

The client connects to the server over TCP, with the address it
finds in its environment.  The server writes the same address to
each of several variables, one for each encoding it offers::

   PMIX_SERVER_URI41=pmix-server.0;tcp4://127.0.0.1:57131
   PMIX_SERVER_URI4=pmix-server.0;tcp4://127.0.0.1:57131
   PMIX_SERVER_URI3=pmix-server.0;tcp4://127.0.0.1:57131
   PMIX_SERVER_URI21=pmix-server.0;tcp4://127.0.0.1:57131
   PMIX_SERVER_URI2=pmix-server.0;tcp4://127.0.0.1:57131

The value, in the ABNF of `RFC 5234
<https://www.rfc-editor.org/rfc/rfc5234>`_, is::

   uri       = server-id ";" address
   server-id = nspace "." rank
   address   = "tcp4://" ipv4 ":" port / "tcp6://" "[" ipv6 "]" ":" port

The client splits the server id at its last period, so the server's
namespace can hold periods.  The client does not negotiate
its encoding.  It looks for the variables of the encodings it has,
newest first, and uses the first one it finds. [#ptl]_  Each
variable names one encoding, and each client looks for the one
for its own release and then the older ones in turn:

.. envvar:: PMIX_SERVER_URI61

  The v61 encoding.  Looked for first by a 6.1 client.

.. envvar:: PMIX_SERVER_URI51

  The v51 encoding.  Looked for first by a 6.0 client, and second
  by 6.1.

.. envvar:: PMIX_SERVER_URI41

  The v41 encoding.  Looked for first by a 4.1, 4.2, or 5.0 client,
  and by every later one after the above.

.. envvar:: PMIX_SERVER_URI4

  The v4 encoding.  Looked for first by a 4.0 client, and by every
  later one after the above.

.. envvar:: PMIX_SERVER_URI3

  The v3 encoding.  Looked for first by a 3.x client, and by every
  later one after the above.

.. envvar:: PMIX_SERVER_URI21

  The v21 encoding.  Looked for first by a 2.1 or 2.2 client, and
  by every later one after the above.

.. envvar:: PMIX_SERVER_URI2

  The v20 encoding, which this page does not describe.  Looked for
  last by every client.

Thus, a server selects a client's encoding by which variables it
sets.

The URI variables are part of a larger set that the server puts in
each client's environment, and the client takes everything it
needs to connect from that set.  A server must set these:

.. envvar:: PMIX_NAMESPACE

  The client's namespace.  The client sends it in the connect
  request and rejects a job-info reply that names another.

.. envvar:: PMIX_RANK

  The client's rank, in decimal.

.. envvar:: PMIX_SERVER_URI61
            PMIX_SERVER_URI51
            PMIX_SERVER_URI41
            PMIX_SERVER_URI4
            PMIX_SERVER_URI3
            PMIX_SERVER_URI21
            PMIX_SERVER_URI2
  :no-index:

  The address, one variable per encoding offered, as above.

.. envvar:: PMIX_SECURITY_MODE

  The security methods the server accepts, comma-separated, such
  as ``munge,native``.  The client takes the first in the list
  that it has.

.. envvar:: PMIX_GDS_MODULE

  The data stores the server has, comma-separated, such as
  ``shmem2,hash``.  The client takes the one of these it prefers
  (see :doc:`openpmix_interop`).

.. envvar:: PMIX_BFROP_BUFFER_TYPE

  ``PMIX_BFROP_BUFFER_NON_DESC`` or ``PMIX_BFROP_BUFFER_FULLY_DESC``,
  the description mode (see `Pack`_).  The client uses the same
  mode as the server.

.. envvar:: PMIX_VERSION

  The server's OpenPMIx version, such as 5.0.7.  A client before
  3.1.5 does not read it.  Later clients record it and change two
  things for a server before 3.1.5: they ask for a whole rank's
  data rather than one key in get, and they look up a node by
  hostname rather than by the resolve commands.

.. envvar:: PMIX_HOSTNAME

  The server's hostname.  The client uses it as its own, so that
  both agree on the name of the node.

.. envvar:: PMIX_SERVER_TMPDIR
            PMIX_SYSTEM_TMPDIR

  Directories for rendezvous files.  A simple client does not use
  them to connect; tools do.

.. envvar:: PMIX_DSTORE_21_BASE_PATH

  With ds21 or ds12, the path of the shared memory segment (see
  `Data stores`_).

OpenPMIx also sets variables for the programming model it detects,
such as ``OMPI_COMM_WORLD_SIZE`` for Open MPI, which are not part
of the protocol: Open MPI reads them before :func:`PMIx_Init` and
would otherwise get the same values from the job information.

One process can connect more than once with the same identity.  A
child that the client forks, and that calls :func:`PMIx_Init`,
connects again, and the server treats each connection on its own.

Connect request
---------------

The first message on a connection is the connect request, [#ptl]_
and the server's answer to it is the `Connect response`_.  Neither
is in the encoding that the rest of the protocol uses: the client
has not yet said which encoding that is.  Nothing in them is a pack
(see `Pack`_), no field has a type code, and no integer is a
varint.  Each integer is a fixed-width field, and they are not all
in one byte order:

* The header of the request is the client's ``pmix_ptl_hdr_t``
  copied as is, so its fields are in **host byte order**.  It is
  the only header in the protocol that is.
* The integers of the request body, and of the response, are 4
  bytes in **network byte order**.
* The native credential, inside the body, is the client's uid and
  gid copied as is, so in **host byte order**.

The header is the same 16 bytes as every other message (see
`Message header`_), with the index set to -1, the tag to
``UINT32_MAX``, and the length to that of the body.

The body is a sequence of fields with no padding between them.
The layout has not changed since OpenPMIx 2.1: every client from
2.1 on sends the same fields in the same order.  The client knows
the server's version from :envvar:`PMIX_VERSION` (see `Transport`_)
but nothing in the request depends on it.  What varies is only
whether anything follows the last fixed field, the storage, and
that is described after the table.

In the table, *string* is a NUL-terminated string; *u32* is a
4-byte unsigned integer in network byte order; and *u8* is one
byte.  These are not the data types of the rest of this page.
Nothing is at a fixed offset after the first field, since the
strings and the credential vary in length; a reader walks the
fields in order.

.. list-table::
   :header-rows: 1
   :widths: 12 12 52

   * - Field
     - Type
     - Value

   * - security
     - string
     - The security method the client selected from
       :envvar:`PMIX_SECURITY_MODE`: munge, native, or none.

   * - credlen
     - u32
     - The length of the credential.

   * - cred
     - *credlen* bytes
     - The credential.  For native it is the client's effective uid
       and gid, each 4 bytes in host byte order.  For munge it is a
       munge credential.  For none it is empty.

   * - flag
     - u8
     - The process type (see `Process types`_).  A simple client
       sends 0.

   * - nspace
     - string
     - The client's namespace (standard 3.2.2, ``pmix_nspace_t``).
       This and the rank are the identity of a simple client; other
       process types send a different identity here.

   * - rank
     - u32
     - The client's rank (standard 3.2.3, ``pmix_rank_t``).

   * - version
     - string
     - The client library's version, such as 5.0.7, possibly with a
       suffix such as a1.

   * - encoding
     - string
     - The encoding the client selected: v21, v3, v4, v41, v51, or
       v61.

   * - buftype
     - u8
     - The description mode (see `Pack`_): 1 for undescribed, 2 for
       described.

   * - storage
     - string
     - The data store the client selected: ds12, ds21, hash, or
       shmem2.

   * - info
     - rest
     - Optional.  A pack of one SIZE *n*, then a pack of *n* INFO,
       in the client's encoding and description mode.  The INFO are
       in the table below.

The info is optional in the sense that the body may end after
the storage, and a server treats both the same way: it reads the
fields through the storage, and whatever remains of the body, if
anything, is the info.  There is no length on it; the server has
the body length from the header.  A server before 6.1 does not
look at the info from a simple client (flag 0) at all, so a client
can send it to any server.  The info is the only pack in the
request, and it is in the encoding the client named a few bytes
earlier.  What a client puts in it depends on its version and
whether it is a tool:

.. list-table::
   :header-rows: 1
   :widths: 14 20 10 32

   * - Sent by
     - Key
     - Type
     - Value

   * - 6.1 client
     - pmix.ppid
     - PID
     - The client's pid.

   * - 6.1 client
     - pmix.cmd.line
     - STRING
     - The client's command line.

   * - 2.2 tool
     - pmix.ppid
     - PID
     - The tool's pid.

   * - 3.1 tool
     - pmix.cmd.line
     - STRING
     - The tool's command line.

   * - 3.1 launcher
     - pmix.tool.launcher
     - BOOL
     - True.  Sent only by a tool that is a launcher.

   * - 5.0.9, 6.1 tool
     - pmix.ruid
     - UINT32
     - The tool's real uid.

   * - 5.0.9, 6.1 tool
     - pmix.euid
     - UINT32
     - The tool's effective uid.

   * - 5.0.9, 6.1 tool
     - pmix.rgid
     - UINT32
     - The tool's real gid.

   * - 5.0.9, 6.1 tool
     - pmix.egid
     - UINT32
     - The tool's effective gid.

A 2.0 client, which this page does not otherwise describe, ends the
body after the version, and the server takes the rest as v20,
undescribed, and ds12.

This is the 50-byte body of a connect request from an OpenPMIx
3.2.5 client with native security, from a capture, split at the
field boundaries.  The offset is from the start of the body::

   0000  6e 61 74 69 76 65 00     security "native"
   0007  00 00 00 08              credlen 8
   000b  d4 15 00 00              cred: uid 5588, host byte order
   000f  d4 15 00 00                    gid 5588
   0013  00                       flag 0, a simple client
   0014  c6 92 36 58 45 4b 73 56  nspace "ƒ6XEKsV"
   001c  00
   001d  00 00 00 00              rank 0
   0021  33 2e 32 2e 35 61 31 00  version "3.2.5a1"
   0029  76 33 00                 encoding "v3"
   002c  01                       buftype 1, undescribed
   002d  68 61 73 68 00           storage "hash"

The nspace is a Flux job id, whose first character is a two-byte
UTF-8 sequence.  A munge credential is a few hundred bytes of
text, so a request with munge security is longer, but the layout
is the same.

Process types
~~~~~~~~~~~~~

The flag in the connect request says what kind of process is
connecting.  The server registers a simple client's identity before
the client starts, so the client has only to name it.  The other
types are self-started, or started by a server that did not
register them, so they identify themselves by uid and gid, and
either bring an identity or ask the server for one.  The server
asks its host (for Flux, the shell plugin) to approve each one
through a callback; flux-pmix does not provide it, so a Flux server
refuses a type that needs an identity and accepts the others
without asking.

.. list-table::
   :header-rows: 1
   :widths: 6 20 10 34

   * - Flag
     - Process type
     - Since
     - Identity in the request

   * - 0
     - Simple client
     - 2.1
     - nspace and rank

   * - 1
     - Legacy tool
     - 2.1
     - uid and gid

   * - 2
     - Legacy launcher
     - 3.0
     - uid and gid

   * - 3
     - Tool that needs an identity
     - 2.2
     - uid and gid

   * - 4
     - Tool with its own identity
     - 2.2
     - uid and gid, then nspace and rank

   * - 5
     - Tool the server started
     - 2.2
     - uid and gid, then nspace and rank

   * - 6
     - Launcher that needs an identity
     - 3.1
     - uid and gid

   * - 7
     - Launcher with its own identity
     - 3.1
     - uid and gid, then nspace and rank

   * - 8
     - Launcher the server started
     - 3.1
     - uid and gid, then nspace and rank

   * - 9
     - Singleton
     - 4.1
     - uid and gid, then nspace and rank

   * - 10
     - Scheduler
     - 4.2.3
     - uid and gid, then nspace and rank

The uid and gid are each a u32, and the request is otherwise the
same as a simple client's: the identity replaces the nspace and
rank, and the version follows.  A launcher is a tool that starts
jobs, such as :program:`prun`; a singleton is a client that found
no server and connected to a system-level one; and a scheduler is
a tool that says it is the system's workload manager, so that the
server it connects to, a system-level one, can route allocation
and session requests to it.  None of these connect to a job's
server.  These types send the info from 2.2 on, with the keys the
info table gives.

Connect response
----------------

The server looks up the namespace and rank, checks the credential,
and records the client's encoding, description mode, and store.
With munge it also checks the uid and gid of the credential against
those registered for the rank.  It replies with no header, in the
same fixed-width network byte order fields as the request: a u32
status, and if the status is 0, a u32 *peer index*, a small integer
that identifies the connection to the server.  The status is one of
the standard's status constants (standard 3.1.1), but it is not a
packed STATUS.  On any other status the server closes the
connection.

The response to a process type other than a simple client (see
`Process types`_) is longer, and the server checks the credential
last rather than first.  The first u32 status is the host's answer.
After it, a type that asked for an identity (3 or 6) gets one: a
nspace as a 256-byte field, NUL-padded, and a u32 rank.  Then every
type but 0 gets the server's own identity in the same form, a nspace
field and a u32 rank, and then a second u32 status, the result of
the credential check.  The peer index is not sent.

Message header
--------------

After the handshake, every message is a 16-byte header and a
payload. [#header]_  The header fields are in network byte order,
whatever the encoding of the payload:

.. list-table::
   :header-rows: 1
   :widths: 10 10 50

   * - Offset
     - Type
     - Field

   * - 0
     - int32
     - Index.  The client sends its peer index.  The server sends
       its own index, which the client ignores.

   * - 4
     - uint32
     - Tag.

   * - 8
     - uint32
     - Length of the payload.

   * - 12
     - uint32
     - Padding, zero.  (The field is a ``size_t`` in the source, so
       the header is 12 bytes on a 32-bit platform.)

Tags 0, 1, and 2 are reserved: 0 carries event notifications from
the server (see `notify and events`_), 1 heartbeats, and 2 I/O
forwarding (see `I/O forwarding`_).  A heartbeat is a message with
an empty payload that a client sends when its host asked it to
through :func:`PMIx_Process_monitor`; MPI programs do not send
them.  A client allocates a tag for each request from 100 up,
starting at 101, and the server replies on the same tag.  More than
one request can be outstanding, and the server can reply in any
order.  Nothing on the wire closes a connection; each side notices
the other's end by the socket closing.

A payload is a sequence of packed values in the client's encoding
and description mode.  There is no length prefix on a sequence; a
receiver knows it has ended when the payload ends.

Encodings
---------

OpenPMIx has had six encodings, which it calls bfrops
modules. [#bfrops]_  Each server offers those it has, and a client
selects the newest it also has (see `Transport`_).

.. list-table::
   :header-rows: 1
   :widths: 10 46

   * - OpenPMIx
     - Encodings

   * - 2.1, 2.2
     - v21, v20, v12

   * - 3.x
     - v3, v21, v20, v12

   * - 4.0
     - v4, v3, v21, v20, v12

   * - 4.1, 4.2, 5.0
     - v41, v4, v3, v21, v20, v12

   * - 6.0
     - v51, v41, v4, v3, v21, v20, v12

   * - 6.1
     - v61, v51, v41, v4, v3, v21, v20, v12

There are only two wire formats.  **v21** and **v3** pack integers
at fixed width in network order.  **v4**, **v41**, **v51**, and
**v61** pack them as variable-length integers, and share one
implementation. [#base]_  Within each group, a later
encoding differs from an earlier one only in the data types it
defines (see `Data types`_).  v12 and v20 serve clients of 1.x and
2.0, which the servers of this page refuse, and are not described.

Pack
~~~~

Every value in a payload is written by a *pack* call, which writes
*n* values of one type *T*.  The connect request and response are
not payloads and have no packs (see `Connect request`_).  A pack
is:

1. In described mode only, the type code of INT32.
2. The count *n*, as an INT32.
3. In described mode only, the type code of *T*.
4. The *n* values, each in the layout of *T*.

A type code is a UINT16, the standard's ``pmix_data_type_t``
(standard 3.3).  A pack is what one call of the standard's
:func:`PMIx_Data_pack` (standard 11.3.1) writes; the standard
defines the call, with its count and type, but not what it writes.
The server sets the mode with :envvar:`PMIX_BFROP_BUFFER_TYPE` in
the client's environment, and the client announces it in the
connect request.  OpenPMIx servers use
undescribed mode.  In described mode, the type codes appear only at
this level, not inside the layouts of compound types.

A pack of *n* values is one count and *n* values.  It is not *n*
packs of one value.  The two are the same when *n* is 1, so a
decoder that confuses them decodes every one-element array and fails
on the first array of two.

A list inside a compound value, such as the keys of a QUERY or the
arguments of an APP, has no pack framing of its own: its count is a
field of the layout, and the elements follow it with no second
count.

Integers in v21 and v3
~~~~~~~~~~~~~~~~~~~~~~

An 8-bit integer is one byte.  A 16-, 32-, or 64-bit integer is 2,
4, or 8 bytes in network order.  SIZE, INT, UINT, and PID are
written at the sender's size for ``size_t``, ``int``, and ``pid_t``,
after a type code that gives that size: on 64-bit Linux, SIZE is the
code of UINT64 (15) and 8 bytes, and PID is the code of UINT32 (14)
and 4 bytes.

Integers in v4 through v61
~~~~~~~~~~~~~~~~~~~~~~~~~~

An 8-bit integer is one byte.  A 16-, 32-, or 64-bit integer is a
*varint*: the value in groups of 7 bits, least significant group
first, one group per byte, with bit 7 set on each byte that another
follows.  This is the unsigned LEB128 of DWARF and the varint of
Protocol Buffers, with one difference: at most 8 such bytes are
written, and if bits remain, a 9th byte holds bits 56 through 63 as
they are, with no continuation bit.

A signed integer is first sign-extended to 64 bits and mapped with
ZigZag, ``(v << 1) ^ (v >> 63)``, so that 0, -1, 1, -2, 2 become 0,
1, 2, 3, 4.  SIZE, INT, and UINT are varints with no type code.
PID keeps the type code of v3 before its varint.

Data types
~~~~~~~~~~

The types and their codes are the standard's ``pmix_data_type_t``
constants (standard 3.3), written here without the ``PMIX_``
prefix.  The standard defines each type's C structure, and OpenPMIx
packs that structure's fields in order, so the *Layout* column is
the standard's structure read as a sequence of types of this table.
Where the standard says a field is a pointer and a count, the layout
is the count and then the elements.  The standard leaves the codes
from 70 up to the implementation, and OpenPMIx assigns them; those
rows, and the layouts themselves, are OpenPMIx's.  Each row names
the structure's section of the standard where it has one.

.. list-table::
   :header-rows: 1
   :widths: 24 6 42

   * - Type
     - Code
     - Layout

   * - UNDEF
     - 0
     - Nothing.

   * - BOOL
     - 1
     - 1 byte, 0 or 1.

   * - BYTE, INT8, UINT8
     - 2, 7, 12
     - 1 byte.

   * - STRING
     - 3
     - INT32 length *L*, which counts the NUL, then *L* bytes.  A
       NULL string is length 0 and no bytes.

   * - SIZE
     - 4
     - See `Encodings`_.

   * - PID
     - 5
     - See `Encodings`_.

   * - INT, UINT
     - 6, 11
     - See `Encodings`_.

   * - INT16, UINT16
     - 8, 13
     - See `Encodings`_.

   * - INT32, UINT32
     - 9, 14
     - See `Encodings`_.

   * - INT64, UINT64
     - 10, 15
     - See `Encodings`_.

   * - FLOAT, DOUBLE
     - 16, 17
     - STRING, from ``printf("%f")``.

   * - TIMEVAL
     - 18
     - INT64 seconds, INT64 microseconds.

   * - TIME
     - 19
     - UINT64.

   * - STATUS
     - 20
     - INT32.  The values are the standard's status constants
       (standard 3.1.1), with the implementation's own below -1000.

   * - VALUE
     - 21
     - UINT16 type *t*, then one value of type *t*.  Nothing follows
       if *t* is UNDEF.  Standard 3.2.8, ``pmix_value_t``.

   * - PROC
     - 22
     - STRING namespace, UINT32 rank.  Standard 3.2.4,
       ``pmix_proc_t``.  The reserved ranks are the standard's:
       UINT32_MAX is ``PMIX_RANK_UNDEF`` and UINT32_MAX - 1 is
       ``PMIX_RANK_WILDCARD`` (standard 3.2.3).

   * - APP
     - 23
     - STRING cmd, INT argc, argc STRING, INT32 envc, envc STRING,
       STRING cwd, INT maxprocs, SIZE ninfo, ninfo INFO.  Standard
       12.2.5, ``pmix_app_t``.

   * - INFO
     - 24
     - STRING key, UINT32 flags, UINT16 type *t*, one value of type
       *t*.  Standard 3.2.9, ``pmix_info_t``; the flags are its
       ``pmix_info_directives_t`` (standard 3.2.10).

   * - PDATA
     - 25
     - PROC, STRING key, UINT16 type *t*, one value of type *t*.
       Standard 9.7.1, ``pmix_pdata_t``.

   * - BUFFER
     - 26
     - BYTE buffer type, SIZE length *L*, *L* bytes, which are a
       payload in the same encoding.

   * - BYTE_OBJECT
     - 27
     - SIZE length *L*, *L* bytes.  Standard 3.2.12,
       ``pmix_byte_object_t``.

   * - KVAL
     - 28
     - STRING key, VALUE.

   * - PERSIST, SCOPE, DATA_RANGE, COMMAND, PROC_STATE
     - 30, 32, 33, 34, 37
     - UINT8.  The values of SCOPE (standard 5.2.1.1), DATA_RANGE
       (standard 9.5.1), PERSIST (standard 9.5.2), and PROC_STATE
       (standard 3.2.5) are the standard's constants.  COMMAND is
       OpenPMIx's; see `Commands`_.

   * - INFO_DIRECTIVES
     - 35
     - UINT32.

   * - DATA_TYPE
     - 36
     - UINT16.

   * - PROC_INFO
     - 38
     - PROC, STRING hostname, STRING executable, PID, PROC_STATE.
       Standard 3.2.6, ``pmix_proc_info_t``.

   * - DATA_ARRAY
     - 39
     - UINT16 type *t*, SIZE *n*, then *n* values of type *t* if *n*
       is not 0 and *t* is not UNDEF.  Standard 3.2.13,
       ``pmix_data_array_t``.

   * - PROC_RANK
     - 40
     - UINT32.

   * - QUERY
     - 41
     - INT32 nkeys, nkeys STRING, SIZE nqual, nqual INFO.  Standard
       7.1.1, ``pmix_query_t``.

   * - ALLOC_DIRECTIVE
     - 43
     - UINT8.  Standard 13.1.4, ``pmix_alloc_directive_t``.

   * - IOF_CHANNEL
     - 45
     - UINT16, a bitmask.  From v3.

   * - ENVAR
     - 46
     - STRING name, STRING value, BYTE separator.  From v3.
       Standard 3.2.11, ``pmix_envar_t``.

   * - COORD
     - 47
     - UINT8 view, SIZE *n*, *n* UINT32.  From v4.

   * - REGATTR
     - 48
     - STRING name, STRING key, UINT16 type, INT32 *n*, *n* STRING
       descriptions.  From v4.

   * - REGEX
     - 49
     - A regex that begins with "pmix" is written as raw bytes to a
       NUL; any other is a STRING.  From v4.  The standard describes
       the prefix rule under :func:`PMIx_generate_regex` (standard
       17.2.1).

   * - JOB_STATE, LINK_STATE
     - 50, 51
     - UINT8.  From v4.

   * - PROC_CPUSET
     - 52
     - STRING, an hwloc list, or NULL if not bound.  From v4.

   * - GEOMETRY
     - 53
     - SIZE fabric, STRING uuid, STRING osname, SIZE *n*, *n* COORD.
       From v4.

   * - DEVICE_DIST
     - 54
     - STRING uuid, STRING osname, UINT64 type, UINT16 min, UINT16
       max.  From v4.

   * - ENDPOINT
     - 55
     - STRING uuid, STRING osname, SIZE *L*, *L* bytes.  From v4.

   * - TOPO
     - 56
     - STRING, the hwloc XML, then hwloc structs of hwloc's own
       layout.  From v4.

   * - DEVTYPE
     - 57
     - UINT64.  From v4.

   * - LOCTYPE
     - 58
     - UINT16.  From v4.

   * - COMPRESSED_BYTE_OBJECT
     - 59
     - As BYTE_OBJECT.  From v4.

   * - PROC_NSPACE
     - 60
     - STRING.  From v41.

   * - PROC_STATS, DISK_STATS, NET_STATS, NODE_STATS
     - 61–64
     - Monitoring statistics.  In v41 and v51 of 4.1 through 6.0;
       removed from both in 6.1.

   * - DATA_BUFFER
     - 65
     - As BYTE_OBJECT.  From v41.

   * - STOR_MEDIUM, STOR_ACCESS, STOR_PERSIST
     - 66–68
     - UINT64.  From v41.

   * - STOR_ACCESS_TYPE
     - 69
     - UINT16.  From v41.

   * - DEVICE
     - 70
     - STRING uuid, STRING osname, UINT64 type.  From v51.

   * - RESBLOCK_DIRECTIVE
     - 71
     - UINT8.  From v51.

   * - RESOURCE_UNIT
     - 72
     - UINT64 type, SIZE count.  From v51.

   * - NODE_PID
     - 73
     - STRING host, UINT32 node id, PID.  From v61.

   * - REGEX2
     - 74
     - STRING type, SIZE *L*, *L* bytes.  From v61.

A value of a type that an encoding does not have cannot be sent on
a connection that uses it: the client fails the operation with
``PMIX_ERR_UNKNOWN_DATA_TYPE`` (-16).  Thus, a 5.0 client connected with
v3 can send every type through ENVAR, and nothing after.

Layouts are not self-delimiting.  A decoder that does not know a
type cannot skip its value, and can decode no more of that payload.
It can still find the next message from the header.

The flags of an INFO are the standard's info directives (standard
3.2.10): 0x1 required, 0x2 end of array, 0x4 required and processed.
The standard reserves the top 16 bits for the implementation and
says nothing of 0x8, which OpenPMIx uses to mark a qualifier (see
`Qualified values`_).

Commands
--------

A request payload begins with a pack of one COMMAND.  The commands
are OpenPMIx's, one for each client call of the standard that
reaches the server; the standard has no notion of a command or its
code.  The codes [#cmd]_ have not changed from 2.1 through 6.1
except where a code was added:

.. list-table::
   :header-rows: 1
   :widths: 6 20 16 30

   * - Code
     - Command
     - From
     - Reply

   * - 0
     - job-info
     - 2.1
     - namespace and job information

   * - 1
     - abort
     - 2.1
     - status

   * - 2
     - commit
     - 2.1
     - status

   * - 3
     - fence
     - 2.1
     - status, when the fence completes

   * - 4
     - get
     - 2.1
     - status and data

   * - 5
     - finalize
     - 2.1
     - status

   * - 6, 7, 8
     - publish, lookup, unpublish
     - 2.1
     - status; lookup adds data

   * - 9
     - spawn
     - 2.1
     - status, namespace, job information

   * - 10, 11
     - connect, disconnect
     - 2.1
     - status, when the operation completes

   * - 12
     - notify
     - 2.1
     - status

   * - 13
     - register-events
     - 2.1
     - status

   * - 14
     - deregister-events
     - 2.1
     - none

   * - 15
     - query
     - 2.1
     - status and results

   * - 16
     - log
     - 2.1
     - status

   * - 17
     - allocate
     - 2.1
     - status and results

   * - 18
     - job-control
     - 2.1
     - status and results

   * - 19
     - monitor
     - 2.1
     - status

   * - 20, 21
     - get-credential, validate-credential
     - 3.0
     - status

   * - 22, 23, 29
     - iof-pull, iof-push, iof-deregister
     - 3.0
     - status

   * - 24–28
     - group-construct, group-join, group-invite, group-leave,
       group-destruct
     - 3.0
     - status; construct adds membership

   * - 30, 31
     - fabric-register, fabric-update
     - 4.0
     - status and results

   * - 32
     - compute-device-distances
     - 4.0
     - status and results

   * - 34
     - resource-block
     - 6.1
     - status and results

   * - 35
     - session-control
     - 6.0
     - status and results

   * - 36
     - request-sysinfo
     - 6.0
     - status and results

   * - 37, 38
     - resolve-peers, resolve-node
     - 5.0
     - status and results

   * - 39
     - gds-fallback
     - 6.1
     - status

Code 12 also carries event notifications from the server, on tag 0.

Every reply except job-info begins with a pack of one STATUS, whose
value is one of the standard's status constants (standard 3.1.1).
When the server cannot act on a request, including when it does not
implement the command, the reply is only the status, such as
``PMIX_ERR_NOT_SUPPORTED`` (-47).  Open MPI sends job-control at
startup; flux-pmix answers -47 and Open MPI continues.

The layouts below are from the client source [#client]_ of
OpenPMIx 6.1, with the differences of earlier versions noted.  Each is, in the main, the arguments of the
standard's client call packed in order, so the standard's
description of the call says what each field means; this page says
how it is carried.  They are written in a notation of this page, not
a formal grammar:

* Each line is one pack: a type, then the value or the name of the
  value.
* ``T[n]`` is one pack of *n* values of type *T*, where *n* is the
  value of an earlier line.
* ``[ ]`` around a line means the line is present only under the
  condition at its right.
* ``*`` after a line means the line repeats to the end of the
  payload.
* Text in parentheses is a note.

A failed request can be answered two ways.  When the server's host
has no handler for the command, the server replies with the status
alone.  When the host's handler runs and fails, the command's own
reply follows: where its layout has a count and a list after the
status, the count is sent, and is 0; other fields are present only
when the status is 0, unless the layout says otherwise.

The first group of commands is what an MPI program sends to
bootstrap.  The dissector decodes these.

job-info
~~~~~~~~

Sent in :func:`PMIx_Init` (standard 4.3), first after the
handshake::

   request:  COMMAND   0
   reply:    STRING    nspace
             KVAL      job information *

The nspace is the client's own, and the client rejects any other.
The KVAL sequence is described in `Job information`_.

commit
~~~~~~

Sent by :func:`PMIx_Commit` (standard 5.2.3) with the values the
client put since its last commit::

   request:  COMMAND   2
             [ SCOPE   1 (local)                          ]
             [ BUFFER  bucket: KVAL *                     ]
             [ SCOPE   2 (remote)                         ]
             [ BUFFER  bucket: KVAL *                     ]
   reply:    STATUS

The request has zero, one, or two scope-and-bucket pairs.  The
scopes are the standard's ``PMIX_LOCAL`` (1) and ``PMIX_REMOTE``
(2); a value put with ``PMIX_GLOBAL`` (3) is in both buckets.  A client sends a
commit with no buckets if it has nothing to commit.

fence
~~~~~

Sent by :func:`PMIx_Fence` (standard 8.1)::

   request:  COMMAND   3
             SIZE      nprocs
             PROC      procs[nprocs]
             SIZE      ninfo
             [ INFO    info[ninfo]       if ninfo > 0 ]
   reply:    STATUS

A client with no process list sends one PROC with its own namespace
and ``PMIX_RANK_WILDCARD``.  The info usually has ``pmix.collect``
(the standard's ``PMIX_COLLECT_DATA``, a bool), which asks that the
remote data of every participant be exchanged so that gets are then
served locally.

The exchange itself is not on this wire.  The server gives its host
the remote buckets of its local participants, the host carries them
to the other nodes by whatever means it has (for Flux, the shell's
exchange over the job's overlay), and each server stores what comes
back.  The format of what is carried is the server's own, and the
servers on every node must agree on it; the standard leaves it to
the host as an opaque blob (standard 17.2.5, ``fence_nb``).  A
client with **hash** then sees nothing of the exchange but the
status; with **shmem2**, the reply adds the segment descriptors of
any new segment the exchange filled (see `Data stores`_).

get
~~~

Sent by :func:`PMIx_Get` (standard 5.4) when the client does not
have the value::

   request:  COMMAND     4
             STRING      nspace
             PROC_RANK   rank
             SIZE        ninfo
             [ INFO      info[ninfo]     if ninfo > 0 ]
             STRING      key
   reply:    STATUS
             [ BYTE_OBJECT  proc-data *   if status == 0 ]

A 2.x client does not send the key.  Each proc-data holds, in the
client's encoding, a pack of one PROC and then a pack of one KVAL
for each value.  A rank of ``PMIX_RANK_UNDEF`` in that PROC means
the requested process; ``PMIX_RANK_WILDCARD`` means the namespace.

The server defers the reply until it has the data, unless the info
has ``pmix.immediate`` (``PMIX_IMMEDIATE``) set, in which case it
replies at once with ``PMIX_ERR_NOT_FOUND`` (-46).

finalize
~~~~~~~~

Sent by :func:`PMIx_Finalize` (standard 4.4)::

   request:  COMMAND   5
   reply:    STATUS

The client waits for the reply for a limited time, then closes.

abort
~~~~~

Sent by :func:`PMIx_Abort` (standard 12.1.1)::

   request:  COMMAND   1
             STATUS    exit status
             STRING    message
             SIZE      nprocs
             [ PROC    procs[nprocs]     if nprocs > 0 ]
   reply:    STATUS

An empty process list means the caller's whole namespace.

register-events and deregister-events
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Open MPI registers event handlers at startup, so every bootstrap
has these::

   request:  COMMAND   13
             SIZE      ncodes
             [ STATUS  codes[ncodes]     if ncodes > 0 ]
             SIZE      ninfo
             [ INFO    info[ninfo]       if ninfo > 0  ]
   reply:    STATUS

   request:  COMMAND   14
             STATUS    code *            (one pack per code)

There is no reply to deregister-events.  Its codes are one pack
each, not one pack of *n*.  The code ``INT32_MIN`` means the default
handler, which covers all codes.

notify and events
~~~~~~~~~~~~~~~~~

A client sends :func:`PMIx_Notify_event` (standard 10.1.8) as a
request.  The server
sends an event to a client as a message on tag 0, with the same
command code, and the client does not reply::

   request:  COMMAND     12
             STATUS      event code
             DATA_RANGE  range
             SIZE        ninfo
             [ INFO      info[ninfo]     if ninfo > 0 ]
   reply:    STATUS

   event:    COMMAND     12
             STATUS      event code
             PROC        source
             SIZE        ninfo
             [ INFO      info[ninfo]     if ninfo > 0 ]

job-control
~~~~~~~~~~~

Sent by :func:`PMIx_Job_control` (standard 13.2.1).  Open MPI 5.0
sends one at startup to register its shared memory segment for
cleanup::

   request:  COMMAND   18
             SIZE      ntargets
             [ PROC    targets[ntargets]    if ntargets > 0 ]
             SIZE      ndirs
             [ INFO    directives[ndirs]    if ndirs > 0    ]
   reply:    STATUS
             [ SIZE    ninfo                               ]
             [ INFO    results[ninfo]       if ninfo > 0   ]

The directives include ``pmix.jctrl.kill``, ``pmix.jctrl.sig``,
``pmix.jctrl.term``, and ``pmix.reg.cleanup``, which Open MPI uses.

The commands that follow are those of dynamic process and resource
management.  flux-pmix does not implement them at the time of
writing; they are here for when it does, and for the reader of a
capture from a program that uses them.

publish, lookup, and unpublish
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Sent by :func:`PMIx_Publish`, :func:`PMIx_Lookup`, and
:func:`PMIx_Unpublish` (standard 9.1, 9.6, and 9.9), which MPI uses for
:func:`MPI_Publish_name` and its relatives::

   request:  COMMAND   6 (publish)
             UINT32    uid
             SIZE      ninfo
             [ INFO    info[ninfo]       if ninfo > 0 ]
   reply:    STATUS

   request:  COMMAND   7 (lookup)
             UINT32    uid
             SIZE      nkeys
             STRING    key *             (one pack per key)
             SIZE      ninfo
             [ INFO    info[ninfo]       if ninfo > 0 ]
   reply:    STATUS
             [ SIZE    ndata                            ]
             [ PDATA   data[ndata]       if ndata > 0  ]

   request:  COMMAND   8 (unpublish)
             UINT32    uid
             SIZE      nkeys
             STRING    key *             (one pack per key)
             SIZE      ninfo
             [ INFO    info[ninfo]       if ninfo > 0 ]
   reply:    STATUS

The uid is the client's own effective uid.  In a publish, each info
is a key and the value to publish; in the others, the info holds
directives such as ``pmix.range`` and ``pmix.wait``.  The keys of
a lookup or unpublish are one pack each, not one pack of *n*.

spawn
~~~~~

Sent by :func:`PMIx_Spawn` (standard 12.2.1)::

   request:  COMMAND   9
             SIZE      ninfo
             [ INFO    job info[ninfo]   if ninfo > 0 ]
             SIZE      napps
             [ APP     apps[napps]       if napps > 0 ]
   reply:    STATUS
             STRING    nspace
             [ KVAL    job information *   if status == 0 ]

The server replies when the new job is launched, with
its namespace and its job information in the form of a job-info
reply.  The new processes connect as clients of that namespace, and
their job information should have ``pmix.spawned`` and
``pmix.parent``, which MPI uses to know it was spawned.

connect and disconnect
~~~~~~~~~~~~~~~~~~~~~~

Sent by :func:`PMIx_Connect` and :func:`PMIx_Disconnect` (standard
12.3.1 and 12.3.3), which MPI uses to join jobs that were launched or spawned
separately::

   request:  COMMAND   10 (connect) or 11 (disconnect)
             SIZE      nprocs
             PROC      procs[nprocs]
             SIZE      ninfo
             [ INFO    info[ninfo]       if ninfo > 0 ]
   reply:    STATUS
             [ BYTE_OBJECT  job data *   connect only ]

Both are collective: the server replies when every listed process
has made the same request.  On a connect, the reply has one job data
object for each other namespace in the set.  Its bytes are a pack of
one STRING nspace and then that namespace's job information as KVAL
packs, in the client's encoding.  After a connect, a client can get
values of processes in the connected namespaces.

process groups
~~~~~~~~~~~~~~

Sent by :func:`PMIx_Group_construct`, :func:`PMIx_Group_leave`, and
:func:`PMIx_Group_destruct` (standard 14.2.6, 14.2.14, and 14.2.8).  Open MPI 5.0 and later build MPI
Sessions on them (see below).  The group commands are in clients
from 4.0::

   request:  COMMAND   24 (construct)
             STRING    group id
             SIZE      nprocs
             PROC      procs[nprocs]
             SIZE      ninfo
             [ INFO    info[ninfo]         if ninfo > 0    ]
   reply:    STATUS
             [ SIZE    nmembers                             ]
             [ PROC    members[nmembers]   if nmembers > 0 ]
             [ BOOL    has context id      6.0 and later   ]
             [ SIZE    context id          if it has one   ]
             [ SIZE    ninfo               6.1 and later   ]
             [ INFO    group info[ninfo]   if ninfo > 0    ]

   request:  COMMAND   27 (leave)
             STRING    group id
             SIZE      ninfo
             [ INFO    info[ninfo]         if ninfo > 0 ]
   reply:    STATUS

   request:  COMMAND   28 (destruct)
             STRING    group id
             SIZE      nmembers
             PROC      members[nmembers]
             SIZE      ninfo
             [ INFO    info[ninfo]         if ninfo > 0 ]
   reply:    STATUS

Construct and destruct are collective over the listed processes.  A
4.x or 5.0 client reads the context id without a BOOL before it,
and the server sends the field whether or not one was requested,
since the client cannot otherwise tell.  The server assigns a
context id, unique across the system, when the request's info has
``pmix.grp.actxid`` set.  A 6.1 client also takes the group
information of each member, which it requested with
``pmix.grp.info``; an earlier client gets it with get, as qualified
values (see `Qualified values`_) of each member, qualified by
``pmix.grp.ctxid``.

:func:`PMIx_Group_invite` and :func:`PMIx_Group_join` are built on
events in the client and do not use commands 25 and 26.

Open MPI 5.0 implements :func:`MPI_Comm_create_from_group` and
:func:`MPI_Intercomm_create_from_groups` with these: each member
constructs a group named for the new communicator, with
``pmix.grp.actxid`` and a ``pmix.grp.info`` that holds
``pmix.grp.lclid``, its local communicator index; takes the context
id as the base of the new communicator's id, and fails if there is
none; and then destructs the group.  Later, a member that needs a
peer's local index gets ``pmix.grp.lclid`` from that peer, qualified
by ``pmix.grp.ctxid``.  Thus, a construct and a destruct are on the
path of each such communicator.  :func:`MPI_Session_init` also queries
the server for process sets (see `query`_).

query
~~~~~

Sent by :func:`PMIx_Query_info` (standard 7.1.2) for what the client
cannot answer from its own data::

   request:  COMMAND   15
             SIZE      nqueries
             QUERY     queries[nqueries]
   reply:    STATUS
             [ SIZE    ninfo                            ]
             [ INFO    results[ninfo]     if ninfo > 0 ]

Open MPI 5.0 queries ``pmix.qry.psetnum`` and
``pmix.qry.psets``, qualified by ``pmix.qry.rfsh`` so that the
request reaches the server, and parses ``pmix.qry.psets`` as a
comma-separated STRING, though the standard says a DATA_ARRAY.

allocate
~~~~~~~~

Sent by :func:`PMIx_Allocation_request` (standard 13.1.1)::

   request:  COMMAND           17
             ALLOC_DIRECTIVE   directive
             SIZE              ninfo
             [ INFO            info[ninfo]       if ninfo > 0 ]
   reply:    STATUS
             [ SIZE            ninfo                          ]
             [ INFO            results[ninfo]    if ninfo > 0 ]

The directive is the standard's ``pmix_alloc_directive_t``: 1 new,
2 extend, 3 release, 4 reacquire, or 5 cancel, which OpenPMIx adds.  The info describes
the request, such as ``pmix.alloc.nnodes`` and ``pmix.alloc.time``;
the results describe the grant, such as ``pmix.alloc.id``.  The
server can reply before the grant and report the outcome later as
an event.

log
~~~

Sent by :func:`PMIx_Log` (standard 13.4.1)::

   request:  COMMAND   16
             TIME      timestamp          server 3.0 or later
             SIZE      ndata
             [ INFO    data[ndata]        if ndata > 0 ]
             SIZE      ndirs
             [ INFO    directives[ndirs]  if ndirs > 0 ]
   reply:    STATUS

monitor
~~~~~~~

Sent by :func:`PMIx_Process_monitor` (standard 13.3.1)::

   request:  COMMAND   19
             INFO      monitor
             STATUS    error code
             SIZE      ndirs
             [ INFO    directives[ndirs]  if ndirs > 0 ]
   reply:    STATUS
             [ SIZE    ninfo                           ]
             [ INFO    results[ninfo]     if ninfo > 0 ]

credentials
~~~~~~~~~~~

Sent by :func:`PMIx_Get_credential` and
:func:`PMIx_Validate_credential` (standard 16.1.1 and 16.2.1),
from 3.0::

   request:  COMMAND       20 (get)
             SIZE          ninfo
             [ INFO        info[ninfo]        if ninfo > 0 ]
   reply:    STATUS
             [ BYTE_OBJECT credential                       ]
             [ SIZE        ninfo                            ]
             [ INFO        info[ninfo]        if ninfo > 0 ]

   request:  COMMAND       21 (validate)
             BYTE_OBJECT   credential
             SIZE          ndirs
             [ INFO        directives[ndirs]  if ndirs > 0 ]
   reply:    STATUS
             [ SIZE        ninfo                            ]
             [ INFO        info[ninfo]        if ninfo > 0 ]

I/O forwarding
~~~~~~~~~~~~~~

Sent by :func:`PMIx_IOF_pull`, :func:`PMIx_IOF_push`, and
:func:`PMIx_IOF_deregister` (standard 18.5.7 through 18.5.9), from
3.0.  The forwarded data itself
goes on tag 2::

   request:  COMMAND       22 (pull)
             SIZE          nprocs
             [ PROC        procs[nprocs]      if nprocs > 0 ]
             SIZE          ndirs
             [ INFO        directives[ndirs]  if ndirs > 0  ]
             IOF_CHANNEL   channels
             SIZE          local id
   reply:    STATUS
             [ SIZE        remote id                        ]

   request:  COMMAND       23 (push)
             SIZE          nprocs
             [ PROC        targets[nprocs]    if nprocs > 0 ]
             SIZE          ndirs
             [ INFO        directives[ndirs]  if ndirs > 0  ]
             [ BYTE_OBJECT data               if any        ]
   reply:    STATUS

   request:  COMMAND       29 (deregister)
             SIZE          ndirs
             [ INFO        directives[ndirs]  if ndirs > 0  ]
             SIZE          remote id
   reply:    STATUS

   on tag 2: PROC          source
             IOF_CHANNEL   channel
             SIZE          remote id
             SIZE          ninfo
             [ INFO        info[ninfo]        if ninfo > 0  ]
             BYTE_OBJECT   data

fabric and devices
~~~~~~~~~~~~~~~~~~

Sent by :func:`PMIx_Fabric_register`, :func:`PMIx_Fabric_update`,
and :func:`PMIx_Compute_distances` (standard 15.4.1, 15.4.3, and
12.4.5), from 4.0::

   request:  COMMAND       30 (fabric-register)
             SIZE          ndirs
             [ INFO        directives[ndirs]  if ndirs > 0 ]
   reply:    STATUS
             [ SIZE        ninfo                           ]
             [ INFO        fabric info[ninfo] if ninfo > 0 ]

   request:  COMMAND       31 (fabric-update)
             SIZE          fabric index
   reply:    as fabric-register

   request:  COMMAND       32 (compute-device-distances)
             TOPO          topology
             PROC_CPUSET   cpuset
             SIZE          ninfo
             [ INFO        info[ninfo]        if ninfo > 0 ]
   reply:    STATUS
             [ SIZE        ndist                           ]
             [ DEVICE_DIST distances[ndist]   if ndist > 0 ]

resolve
~~~~~~~

Sent by :func:`PMIx_Resolve_peers` and :func:`PMIx_Resolve_nodes`
(standard 7.2), from 5.0::

   request:  COMMAND   37 (resolve-peers)
             STRING    node name
             STRING    nspace
   reply:    STATUS
             [ SIZE    nprocs                           ]
             [ PROC    procs[nprocs]      if nprocs > 0 ]

   request:  COMMAND   38 (resolve-node)
             STRING    nspace
   reply:    STATUS
             [ STRING  node list                        ]

session-control
~~~~~~~~~~~~~~~

Sent by :func:`PMIx_Session_control`, from 6.0, by which a scheduler
or a job pauses, resumes, or ends a session::

   request:  COMMAND   35
             UINT32    session id
             SIZE      ndirs
             [ INFO    directives[ndirs]  if ndirs > 0 ]
   reply:    STATUS
             [ SIZE    ninfo                           ]
             [ INFO    results[ninfo]     if ninfo > 0 ]

resource-block
~~~~~~~~~~~~~~

Sent by :func:`PMIx_Resource_block`, from 6.1.  Its types are in v51
and later, so a 6.1 client connected with v41 cannot send it::

   request:  COMMAND             34
             RESBLOCK_DIRECTIVE  directive
             STRING              block name
             SIZE                nunits
             [ RESOURCE_UNIT     units[nunits]      if nunits > 0 ]
             SIZE                ninfo
             [ INFO              info[ninfo]        if ninfo > 0  ]
   reply:    STATUS
             [ SIZE              ninfo                            ]
             [ INFO              results[ninfo]     if ninfo > 0  ]

gds-fallback
~~~~~~~~~~~~

Sent by a 6.1 client when its data store fails to attach to the
server's, to ask for another::

   request:  COMMAND   39
             STRING    module name
   reply:    STATUS

Code 36, request-sysinfo, is defined in 6.0 but no client sends it.

Job information
---------------

The job-info reply is the client's main source of information about
the job.  The keys and their meanings are the standard's reserved
keys (standard 6), which it sorts into *data realms*: session, job,
application, node, and process (standard 6.1).  The standard says
which keys of each realm a host must provide and which it may
(standard 17.2.3, :func:`PMIx_server_register_nspace`), and
OpenPMIx derives more from what the host gives it, so what a given
server sends is not fixed by the protocol.  What the protocol fixes
is how the realms are carried, and that depends on the data store
the client selected (see `Data stores`_).  With **hash**, the reply
holds the job information itself, as a sequence of KVAL.  A key
that is not one of the containers below is job-realm information,
which the client files under the wildcard rank:

.. list-table::
   :header-rows: 1
   :widths: 18 12 40

   * - Key
     - Type
     - Holds

   * - pmix.ssn.arr
     - DATA_ARRAY of INFO
     - Session-realm information.  The standard requires
       pmix.session.id in the array, and assigns no value to a job
       in no session; an OpenPMIx 5.0 or later server fills in
       ``UINT32_MAX`` when the host registers none, and its store
       treats that value as the default session and any other as a
       real one.

   * - pmix.app.arr
     - DATA_ARRAY of INFO
     - Application-realm information.  The standard requires
       pmix.appnum in the array.

   * - pmix.node.arr
     - DATA_ARRAY of INFO
     - Node-realm information.  The standard requires pmix.hname or
       pmix.nodeid in the array.

   * - pmix.pblob
     - BYTE_OBJECT
     - Process-realm information for one rank: a pack of one
       PROC_RANK, then a pack of one KVAL for each value, in the
       client's encoding.

The first three are the standard's own attributes,
``PMIX_SESSION_INFO_ARRAY``, ``PMIX_APP_INFO_ARRAY``, and
``PMIX_NODE_INFO_ARRAY`` (standard 17.2.3.1), with its rules for
what identifies the array.  ``pmix.pblob`` is OpenPMIx's: the
standard carries process-realm information in ``pmix.pdata``
(``PMIX_PROC_INFO_ARRAY``), an array of INFO with ``pmix.rank`` in
it, and ``pmix.pblob`` is a packed form of the same that OpenPMIx
sends in its place.  The standard also lets the arrays nest, a
job array holding node arrays for example; OpenPMIx flattens them
before sending.

Clients before 3.1.5 take node information in an older form: each
node's array under a key that is the node's hostname, and the keys
of the client's own node repeated at job level.  A 3.2 or later
server sends that form to such a client.

Two things a host registers do not reach the client as keys.  The
node and process maps (``pmix.nmap``, ``pmix.pmap``) are consumed
by the server, which expands them into the per-node and per-rank
entries above, and a get for either returns NOT_FOUND.  And in
clients before 5.0, a realm's identifier (``pmix.session.id``,
``pmix.nodeid`` for a node) is kept as the identity of the realm
object the client builds, not as a key in its list, so a get for
it by name finds nothing even though the reply carried it; 5.0
answers each from the object.  The ds21 store, which copies the
reply's entries into its segment unparsed, can return such a key
on a 4.x client where the hash store cannot.

Qualified values
~~~~~~~~~~~~~~~~

A value that applies only under some condition, such as within one
process group, is a KVAL with key ``pmix.qual.val`` and a
DATA_ARRAY of INFO as its value.  The first INFO is the key and
value; each other INFO is a qualifier, with flag 0x8 set.  A client
that wants such a value puts the qualifiers, with that flag, in the
info of its get request.  This is an OpenPMIx convention.  The
standard uses qualifiers as directives to a call, such as the info
of :func:`PMIx_Get` (standard 5.4), and does not define this key,
the flag, or any way to store a value under qualifiers.

Data stores
-----------

The data store [#gds]_ is how a client holds the job information
and the values it gets, and the client selects one from those the
server lists in :envvar:`PMIX_GDS_MODULE` (see
:doc:`openpmix_interop`).  The choice changes what the job-info
reply carries, and with a shared memory store, where the job
information actually is.

hash
~~~~

The client keeps everything in its own memory, and the job-info
reply carries the job information itself, as `Job information`_
describes.  Every version has hash, and it is the store a client
and server fall back to when they share no other.

ds12 and ds21
~~~~~~~~~~~~~

The server writes the job information to a shared memory segment
before the client starts, and the client finds the segment by
:envvar:`PMIX_DSTORE_21_BASE_PATH` (or
:envvar:`PMIX_DSTORE_12_BASE_PATH`) in its environment.  The
job-info reply is only the nspace.

The segment is a set of files under that path, each a flat
sequence of records addressed by offset: an initial segment
indexes namespaces, a meta segment indexes ranks by offset and
count, and a data segment holds each rank's values as a size, a
key, and a packed VALUE in the client's encoding, the same bytes
a job-info reply would carry.  Readers and the writer coordinate
through process-shared mutexes in a lock segment.  Nothing in it
depends on where it is mapped.

ds12 is in 2.1 through 4.x and ds21 in 2.2 through 4.x; 5.0
dropped both.

shmem2
~~~~~~

The server writes the job information to a segment when the first
client connects.  The reply holds three keys, each a BYTE_OBJECT:
two PMIX_GDS_SHMEM2_SEG_BLOB, which describe the segment, and
PMIX_GDS_SHMEM2_KIDX, which is a copy of the server's attribute
dictionary, about 185 KB.  Each SEG_BLOB is a pack of KVAL: the
nspace, a segment id, the segment's file path, its size, and
PMIX_GDS_SHMEM2_SEG_HADR, the address at which the server mapped
it, as a hex string.  The KIDX is a pack of KVAL that lists each
key the server knows with its index and type, so that the client
resolves keys to the same indices the server used.

The segment holds a copy of the server's job tracker, built as the
same structures: the server walks the job information in its own
hash store and constructs the lists and hash tables again inside
the segment, with a bump allocator that hands out segment
addresses, so the pointers between them are segment addresses.
The pointers are why the client maps the segment at the server's
address; relocated, they would point nowhere.  The layout is that
of the OpenPMIx build that wrote it, so a client reads its job
information through the data structures of a matching OpenPMIx,
not through the encoding, and a capture of its job-info reply
shows only the descriptor.  The server's own store is untouched;
nothing in the segment is shared with it.  A fence whose exchange fills a new
segment adds that segment's descriptor to the fence reply.

shmem2 is in 5.0 through 6.1.

shmem3
~~~~~~

OpenPMIx 7.0 replaces shmem2 with shmem3, which keeps the same
design and adds to the descriptor a generation number, so that a
modex segment can hold only what changed since the one before, and
a session id for session segments.  It is not described further
here; 7.0 is a release candidate at the time of writing.

The server's side
-----------------

Taken together, this is what a server does for a simple client,
and what a capture of an MPI bootstrap shows.  The client's
environment (see `Transport`_) tells it where to connect and what
to speak.  The server accepts the TCP connection, reads the
connect request, checks the identity against what its host
registered and the credential by the method named, and sends the
connect response.  It then answers requests on their tags:

* **job-info**, with the job information as KVAL (hash) or a
  segment descriptor (shmem2 or ds21), and nothing else works
  until this is answered;
* **commit**, by storing the buckets by scope under the rank;
* **fence**, by waiting for every local participant, having the
  host exchange the remote buckets across nodes when collect is
  set, storing what the host brings back, and replying to each
  participant;
* **get**, from what it has stored, or deferred until a fence or
  commit supplies it, or by asking the server of the rank's node
  through the host;
* **register-events**, **job-control**, **notify**, and
  **deregister-events**, with a status, since Open MPI sends them
  at startup and tolerates ``PMIX_ERR_NOT_SUPPORTED``;
* **abort** and **finalize**, through the host.

The server also sends events on tag 0 when the host asks it to,
and must be prepared for a client to connect more than once and
for requests to be outstanding at once.  The rest of the commands
are for dynamic process and resource management, and an MPI
program runs with ``PMIX_ERR_NOT_SUPPORTED`` for all of them.

Beyond bootstrap
~~~~~~~~~~~~~~~~

Three features go past what bootstrap needs, and each adds to
what the server keeps and what it asks of its host.  OpenPMIx
gives the host one callback for each (``group``, ``spawn``,
``allocate``, and ``query`` in ``pmix_server_module_t``, standard
17.2.5); what follows is what sits between the wire and those
callbacks.

**Process groups** (commands 24, 27, 28) are a collective over
processes that may span namespaces, so the server cannot complete
one alone.  For a construct it collects the request from each
local member, merges their ``pmix.grp.info``, and passes the
group id, the full process list, and the merged info to the host
once.  The host carries the request to every other node that has
a member, agrees on the result, and calls back with the final
membership (``pmix.grp.mbrs``), a context id
(``pmix.grp.ctxid``) if one was asked for, which must be unique
across every job the host runs, and the merged group info of all
members.  The server stores that info, keyed by the context id,
so that later gets qualified by ``pmix.grp.ctxid`` find it, and
replies to each local member.  Thus a group construct is a fence
over an arbitrary set of processes with a small payload and an
id allocation, and a destruct is a fence that frees the id.  The
exchange the host does for a fence (see `fence`_) serves, if it
can run over a subset of nodes and carry a payload.

**Process sets** need no command of their own.  A pset is a name
for a set of processes.  The host defines one by name and member
list (:func:`PMIx_server_define_process_set`, standard 17.2.8),
usually one per job plus any the user names, and the server
keeps the list and announces each definition and deletion to its
clients as an event, ``PMIX_PROCESS_SET_DEFINE`` (-55) or
``PMIX_PROCESS_SET_DELETE`` (-56), with ``pmix.pset.nm`` and
``pmix.pset.mems`` in the event info.  A client learns psets
three ways.  The host may put ``pmix.pset.nm``
(``PMIX_PSET_NAME``) in the application-realm job information,
as PRRTE does for an app launched with a pset name, and a client
then gets it from its own copy.  A `get`_ of ``pmix.pset.nms``
(``PMIX_PSET_NAMES``) for a process goes to the server, which
answers from its list with a comma-separated string of the psets
the process is in.  A `query`_ for ``pmix.qry.psetnum``,
``pmix.qry.psets``, or ``pmix.qry.pmems`` goes to the host,
which answers with the count, the names, or the members of a
named pset.  Open MPI Sessions queries the names at
:func:`MPI_Session_init` and gets ``pmix.pset.nm`` of each
process to build a group from a pset.

**Spawn and allocation** are host operations with a thin wire
layer.  For `spawn`_, the server passes the apps and job info to
the host, and the host launches the job, registers the new
namespace with the server (standard 17.2.3) as it would any job,
and calls back with the namespace; the server then replies with
the namespace and the new job's information, which it has by
virtue of the registration.  The new processes connect as
ordinary clients, and if the parent and children are to
communicate, `connect and disconnect`_ follow, which are a fence
across the two namespaces whose reply carries each other's job
information.  For `allocate`_, the server passes the directive
and info to the host and replies with what the host returns;
nothing else is on the wire, and the client expects the host to
honor ``pmix.alloc.reqid`` so that it can match a later event to
the request.  Growing or shrinking a running job is thus an
allocate (extend or release), a change to the job's psets with
the events above, and for growth a spawn or a connect to bring
the new processes in.  The standard also defines events for a
process ending, ``PMIX_EVENT_JOB_END`` (-145) and
``PMIX_ERR_PROC_TERM_WO_SYNC`` (-200), that a shrinking job's
survivors expect through `notify and events`_.

Version summary
---------------

.. list-table::
   :header-rows: 1
   :widths: 12 10 50

   * - Client
     - Encoding
     - Protocol differences

   * - 2.1, 2.2
     - v21
     - Ends the connect body after the version.  Does not send the
       key in get.

   * - 3.0, 3.1
     - v3
     - Sends the connect info array.  Sends the key in get.  Takes
       the older node information form before 3.1.5.

   * - 3.2
     - v3
     - As 3.1.5.

   * - 4.0
     - v4
     - Adds uid and gid to the connect info.  Adds types 47–59.

   * - 4.1, 4.2
     - v41
     - Adds types 60–69.

   * - 5.0
     - v41
     - As 4.2.  Adds commands 37 and 38.

   * - 6.0
     - v51
     - Adds types 70–72 and commands 35 and 36.

   * - 6.1
     - v61
     - Removes types 61–64.  Adds types 73 and 74 and commands 34
       and 39.

.. rubric:: References

.. [#std] PMIx Standard, version 5.0, PMIx Administrative Steering
   Committee, 2023.
   https://github.com/pmix/pmix-standard/releases/download/v5.0/pmix-standard-v5.0.pdf

.. [#ptl] The transport layer, including the connect handshake:
   https://github.com/openpmix/openpmix/tree/v5.0.7/src/mca/ptl

.. [#header] The message header, ``pmix_ptl_hdr_t``:
   https://github.com/openpmix/openpmix/blob/v5.0.7/src/mca/ptl/ptl_types.h

.. [#bfrops] The encodings, one module per directory:
   https://github.com/openpmix/openpmix/tree/v5.0.7/src/mca/bfrops

.. [#base] The pack and unpack routines the v4 and later encodings
   share:
   https://github.com/openpmix/openpmix/tree/v5.0.7/src/mca/bfrops/base

.. [#cmd] The command codes, ``pmix_cmd_t``:
   https://github.com/openpmix/openpmix/blob/v5.0.7/src/include/pmix_globals.h

.. [#client] The client side of each command:
   https://github.com/openpmix/openpmix/tree/v5.0.7/src/client and
   https://github.com/openpmix/openpmix/tree/v5.0.7/src/common

.. [#gds] The data stores, one module per directory:
   https://github.com/openpmix/openpmix/tree/v5.0.7/src/mca/gds
