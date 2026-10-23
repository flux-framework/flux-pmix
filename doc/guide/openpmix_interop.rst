OpenPMIx Version Interoperability
=================================

flux-pmix runs a PMIx server in each job shell, and the MPI programs
that the job runs connect to it as PMIx clients.  The server and the
clients each have an OpenPMIx version, and the two are often
different.  This page describes what happens when they differ.

OpenPMIx publishes its own statement of cross-version support in
`Version Numbers and Binary Compatibility
<https://docs.openpmix.org/en/latest/versions.html>`_.  In short, a
server supports clients of OpenPMIx 2.1 and later, and a client
supports servers of 3.2 and later.  The same page notes that this
guarantees only that requests and responses can be exchanged, not
that both sides implement every request.  This page is about the
cases that work but work badly, and about what a server of a given
version can implement at all.

The OpenPMIx `security policy
<https://github.com/openpmix/openpmix/blob/master/SECURITY.md>`_
states which series receive fixes.  At the time of writing it
supports only the 7.0 series, once released, and declares every
earlier series end of life, with no fixes, including security fixes,
and none backported.  Every OpenPMIx that a Linux distribution
packages is in an earlier series.

.. note::
   A distribution that packages OpenPMIx supports its package under
   its own policy, whatever OpenPMIx says of that series.  The `Red
   Hat Enterprise Linux life cycle
   <https://access.redhat.com/support/policy/updates/errata>`_ is ten
   years, with security fixes for the whole of it.  Red Hat lists the
   `Application Streams with a shorter life
   <https://access.redhat.com/support/policy/updates/rhel-app-streams-life-cycle>`_;
   the ``pmix`` and ``openmpi`` packages are not among them.  For
   example, when `CVE-2023-41915
   <https://access.redhat.com/security/cve/CVE-2023-41915>`_, a race
   that let a user take ownership of files when the server runs as
   root, as it does under Slurm but not under Flux, was fixed
   upstream, Red Hat shipped the fix in its el8 2.2.5 and el9 3.2.3
   packages with security advisories, and without a version change.

Red Hat Enterprise Linux and its rebuilds package both Open MPI and
OpenPMIx in the AppStream repository, and build the Open MPI against
the OpenPMIx, so on a stock system the server and the packaged Open
MPI agree:

.. list-table::
   :header-rows: 1
   :widths: 10 14 14

   * - Release
     - Open MPI
     - OpenPMIx

   * - el8
     - 4.1.5
     - 2.2.5

   * - el9
     - 4.1.8
     - 3.2.3

   * - el10
     - 5.0.2
     - 4.2.8

The server version
------------------

The server is the reference server in the OpenPMIx library that the
shell plugin links against, so its version is that library's.  How
flux-pmix is deployed decides which library that is.  A site that runs
Flux as a system instance usually installs flux-pmix as a system
package, built against the OpenPMIx that the distribution packages,
and upgrades both with the distribution.  A Flux deployed some other
way, such as with Spack, gets the OpenPMIx chosen when its flux-pmix
was built, which may be newer than the distribution's, and a user
may deploy their own.

The server implements a PMIx feature through a callback that its
OpenPMIx declares, so a feature is available only if the server's
OpenPMIx has the callback, whatever the client's version.  flux-pmix
implements the callbacks that an MPI program needs to start, and can
add the others from the OpenPMIx version shown:

.. list-table::
   :header-rows: 1
   :widths: 36 10 12

   * - Server feature
     - OpenPMIx
     - flux-pmix

   * - fence, direct modex, abort
     - 3.2
     - yes

   * - spawn, allocation request, job control, process monitor,
       query, events, publish and lookup
     - 3.2
     - no

   * - tool connections
     - 3.2
     - no

   * - groups and process sets, which MPI Sessions in Open MPI 5.0
       and later are built on
     - 4.2
     - no

   * - session control, by which a scheduler pauses, resumes, or
       ends an allocation
     - 5.0
     - no

   * - resource blocks
     - 6.1
     - no

The client version
------------------

A client is any program that links an OpenPMIx client library and
connects to the server, and its version is that library's.  PMIx
also defines tools, such as debuggers, that connect to a server
without being launched by it; flux-pmix does not support tool
connections.  In practice the client is an MPI library, and usually
Open MPI.  MPICH can also be a PMIx client, but only when built with
``--with-pmix``; its default bootstrap is PMI, which Flux serves
without flux-pmix.  The rest of this section is about Open MPI.

Open MPI includes a copy of OpenPMIx and uses it unless ``configure``
finds an external one that meets its conditions: its headers and
library are installed where ``configure`` looks, its version is in
range, and the headers and library of the libevent (and, from 5.0,
hwloc) that it was built with are installed too, since ``configure``
finds and uses those the same way.  On a distribution that splits
packages, that means the ``-devel`` packages of all of them.  When no
``--with-pmix`` is given and any condition fails, ``configure``
silently ignores the external OpenPMIx and uses the copy.  Thus, an
Open MPI build that does not ask for the external OpenPMIx is likely
to get the copy.

Open MPI builds tend to proliferate outside of the system packages,
and each that uses the copy installs it as a private shared library
in its own prefix.  The table gives the last release of each Open MPI
series, the OpenPMIx it bundles, and the bounds on an external
OpenPMIx that ``configure`` enforces, which were not tested.

.. note::
   An application works only with the MPI it was built against, so a
   site may keep one Open MPI for each compiler and version in use.
   Users build their own as well.  MPI 5.0 adds a standard binary
   interface, which may reduce the number of builds in time.

.. list-table::
   :header-rows: 1
   :widths: 14 18 16 16

   * - Open MPI
     - Bundled OpenPMIx
     - Min. external
     - Max. external

   * - 3.1.6
     - 2.2.3
     - 2.x
     - none

   * - 4.0.7
     - 3.2.3
     - 3.0
     - none

   * - 4.1.8
     - 3.2.5
     - 3.0
     - none

   * - 5.0.11
     - 5.0.11
     - 4.2.4
     - 5.x

   * - 6.0.0rc2
     - 7.0.0rc1
     - 7.0.0
     - none

Components by version
---------------------

The wire encoding and the data store are each an OpenPMIx component,
and the set a version has decides what a pairing of versions can
select.  Each OpenPMIx version has these:

.. list-table::
   :header-rows: 1
   :widths: 10 22 18 50

   * - OpenPMIx
     - Encodings
     - Data stores
     - Notes

   * - 2.2
     - v21, v20, v12
     - ds21, ds12, hash
     - Not supported as the server

   * - 3.2
     - v3, v21, v20, v12
     - ds21, ds12, hash
     -

   * - 4.2
     - v41, v4, v3, v21, v20, v12
     - ds21, ds12, hash
     -

   * - 5.0
     - v41, v4, v3, v21, v20, v12
     - shmem2, hash
     -

   * - 6.0
     - v51, v41, v4, v3, v21, v20, v12
     - shmem2, hash
     -

   * - 6.1
     - v61, v51, v41, v4, v3, v21, v20, v12
     - shmem2, hash
     -

   * - 7.0
     - v61, v51, v41, v4, v3, v21
     - shmem3, hash
     - Release candidate at the time of writing

Optimal pairings
----------------

When a client connects, it selects the newest encoding that it and
the server both have.  Every version from 2.1 on has **v21**, so
there is always one, and the encoding is not a consideration in
pairing.
The server then selects the data store that it prefers from the
stores that it and the client both have.  Only **hash** is in every
version.  Thus, a client and a server that do not share a shared
memory store use **hash**.

**hash** is suboptimal.  It sends a copy of the job information to
each client at startup.  The job information has an entry for each
node and each task.  With OpenPMIx 5.0.7, it is about 120 bytes for
each node plus 110 bytes for each task.  For a job of 10,000 nodes
with 64 tasks on each node, each task receives about 70 MB, and each
server sends about 4.5 GB.  By contrast, a shared memory store writes
the job information to a segment one time on each node, and sends
each client only a description of the segment.

Thus, pair clients with a server that has the same shared memory
store: **ds21** in 3.x and 4.x, **shmem2** in 5.x and 6.x, or
**shmem3** in 7.x.

The tables that follow show the store that each server version
selects for each client version.

OpenPMIx 3.2 server:

.. list-table::
   :header-rows: 1
   :widths: 16 10 10

   * - Clients
     - Store
     - Optimal

   * - 2.2, 3.2, 4.2
     - ds21
     - X

   * - 5.0 and later
     - hash
     -

OpenPMIx 4.2 server:

.. list-table::
   :header-rows: 1
   :widths: 16 10 10

   * - Clients
     - Store
     - Optimal

   * - 2.2, 3.2
     - ds21
     - X

   * - 4.2
     - ds21
     - X

   * - 5.0 and later
     - hash
     -

OpenPMIx 5.0 server:

.. list-table::
   :header-rows: 1
   :widths: 16 10 10

   * - Clients
     - Store
     - Optimal

   * - 2.2, 3.2
     - hash
     -

   * - 4.2
     - hash
     -

   * - 5.0 and later
     - shmem2
     - X

OpenPMIx 6.1 server:

.. list-table::
   :header-rows: 1
   :widths: 16 10 10

   * - Clients
     - Store
     - Optimal

   * - 2.2, 3.2
     - hash
     -

   * - 4.2
     - hash
     -

   * - 5.0
     - shmem2
     - X

   * - 6.0
     - shmem2
     - X

   * - 6.1
     - shmem2
     - X

   * - 7.0
     - hash
     -

OpenPMIx 7.0 server:

.. list-table::
   :header-rows: 1
   :widths: 16 10 10

   * - Clients
     - Store
     - Optimal

   * - 2.2, 3.2
     - hash
     -

   * - 4.2
     - hash
     -

   * - 5.0
     - hash
     -

   * - 6.0
     - hash
     -

   * - 6.1
     - hash
     -

   * - 7.0
     - shmem3
     - X

Conclusions
-----------

1. For a system Flux deployment, prefer an enterprise distribution's
   OpenPMIx for the server, unless it lacks a feature the deployment
   needs.  The server library is installed once and used by every job
   on the system, and it is the side that acts on requests, so a flaw
   in it is reachable from every client.  It is the side where
   security fixes matter most.  Red Hat provides them for the life of
   an Enterprise Linux release, with advisories, and other
   distributions have policies of their own.  OpenPMIx upstream
   provides fixes only in the current series, and does not notify.

2. Expect the server, not the client, to limit features.  Spawn,
   allocation requests, and job control can be implemented on any
   supported server.  MPI Sessions, the MPI 4.0 feature that Open MPI
   5.0 and later provide, are built on PMIx groups and process sets,
   which need a 4.2 or later server; a site with an el9 server cannot
   get them by any choice of client.

3. Every pairing in the tables is expected to work, and the only
   known cost of a suboptimal one is startup time in proportion to
   job size.  For a system that runs large jobs, match clients to the
   server's shared memory store.  Where the site controls how Open
   MPI is built, build it with ``--with-pmix`` against the system
   OpenPMIx; then the client is the server's own OpenPMIx and shares
   its store.  Where it does not, the pairing tables show which
   clients share the server's store.
