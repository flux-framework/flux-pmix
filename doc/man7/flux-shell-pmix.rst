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

The options below restrict what the server offers, to test a client
against a particular configuration.

.. option:: pmix.wire=LIST

  Offer only the listed wire encodings.  LIST is a comma separated
  subset of **v2**, **v21**, **v3**, **v4**, **v41**, **v51**, and
  **v61**.  The value is passed to OpenPMIx as ``PMIX_MCA_bfrops``, so
  the server loads only those encodings.  Each task sees only the
  server URIs for them, so a client can select only those.  The job
  fails at startup if the server has none of them.

.. option:: pmix.store=LIST

  Offer only the listed data stores.  LIST is a comma separated
  subset of the stores the server's OpenPMIx has, e.g. **hash**.  The
  value is passed to OpenPMIx as ``PMIX_MCA_gds``.  If **hash** is in
  LIST, it must be last, e.g. **shmem2,hash**.  The stores are
  **ds21** in 3.x and 4.x, **shmem2** in 5.x and 6.x, **shmem3** in
  7.x, and **hash** in all versions.

.. option:: pmix.auth=LIST

  Offer only the listed authentication methods.  LIST is a comma
  separated subset of the methods the server's OpenPMIx has, e.g.
  **native**, **munge**, or **none**.  The value is passed to OpenPMIx
  as ``PMIX_MCA_psec``.  A method the server does not have is ignored.

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

RESOURCES
=========

Flux: http://flux-framework.org

OpenPMIx: https://openpmix.github.io

SEE ALSO
========

:core:man1:`flux-shell`, :core:man7:`flux-shell-options`
