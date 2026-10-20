flux-pmix
=========

The flux-pmix project adds PMIx support to the Flux resource manager
framework, so that Flux can launch MPI programs that bootstrap with PMIx,
and can itself be launched by a resource manager that offers only PMIx.
It contains two components:

Shell plugin
  A :core:man1:`flux-shell` plugin that runs a PMIx server in each job
  shell, for programs built with Open MPI or another PMIx client.  It is
  enabled per job on the submission command line::

    $ flux run -o pmi=pmix myprogram

PMI client plugin
  A Flux PMI client plugin that lets Flux bootstrap under a launcher that
  provides only PMIx, such as ``jsrun``.  It is selected on the broker
  command line::

    $ jsrun flux start -o,-Sbroker.boot-method=pmix

.. toctree::
   :maxdepth: 2
   :caption: Manual Pages

   man7/index

Related Information
===================

.. list-table::
   :header-rows: 1

   * - Description
     - Links

   * - Main Flux Documentation Pages
     - `Docs <https://flux-framework.readthedocs.io/en/latest/index.html>`_

   * - flux-core, the base Flux framework
     - `Docs <https://flux-framework.readthedocs.io/projects/flux-core>`_

   * - OpenPMIx, the PMIx reference implementation
     - `Docs <https://docs.openpmix.org>`_

   * - flux-pmix source and issues
     - `GitHub <https://github.com/flux-framework/flux-pmix>`_
