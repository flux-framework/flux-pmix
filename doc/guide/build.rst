Building and Installing
=======================

Releases are on the `GitHub releases page
<https://github.com/flux-framework/flux-pmix/releases>`_.  flux-pmix
is at major version zero, so interfaces are not yet stable, and
breaking changes are announced in the release notes.

Dependencies
------------

On a distribution that splits packages, install the development
package of each library.

.. list-table::
   :header-rows: 1
   :widths: 24 18 40

   * - Package
     - Minimum version
     - Notes

   * - flux-core
     - 0.91.0
     - Install flux-pmix to the same prefix.  The fence exchange runs
       on the jobdict shell plugin that 0.91.0 introduced.

   * - OpenPMIx
     - 3.2.3
     - Linked into the server.  See :doc:`openpmix_interop`.

   * - hwloc
     - 2.0
     - Must be the one flux-core uses.

   * - jansson
     - 2.10
     -

   * - pkg-config, Python
     - 3.6 (Python)
     -

Optional: Open MPI 3.0 or MPICH 4.0 built with ``--with-pmi=pmix``,
to test launching MPI programs; Wireshark, to test the dissector;
Sphinx and aspell, for the documentation.

Building
--------

.. code-block:: console

  $ ./autogen.sh    # git clone only
  $ ./configure --prefix=PREFIX
  $ make -j
  $ make check
  $ make install

Install flux-pmix to the same prefix as flux-core, so that the plugins
land on flux-shell's and flux-broker's default search paths.
``configure`` looks for flux-core under the prefix first, then
wherever ``pkg-config`` finds it.

By default each library is the one ``pkg-config`` finds.  These
options choose another:

.. list-table::
   :header-rows: 1
   :widths: 34 46

   * - Option
     - Effect

   * - --with-pmix-server=PREFIX
     - The OpenPMIx for the server.

   * - --with-pmix-client=PREFIX
     - The OpenPMIx for the PMI client plugin and the test clients.
       When only the server is pinned, these stay on the default
       OpenPMIx, which stands in for the MPI libraries the server
       will meet.

   * - --with-mpi=PREFIX
     - The MPI for the MPI tests, which are skipped when none is
       found.  --without-mpi skips them.

   * - --with-wireshark-plugindir=DIR
     - Where the Wireshark dissector installs.  The default is under
       LIBDIR/wireshark/plugins, where Wireshark loads it without
       setup.

``make deb`` builds a Debian package in the ``debbuild`` directory
from a release tarball.
