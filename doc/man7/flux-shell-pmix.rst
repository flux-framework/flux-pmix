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

RESOURCES
=========

Flux: http://flux-framework.org

OpenPMIx: https://openpmix.github.io

SEE ALSO
========

:core:man1:`flux-shell`, :core:man7:`flux-shell-options`
