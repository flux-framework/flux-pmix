Feature Checklist
=================

The PMIx Standard defines a broad set of interfaces, but the flux-pmix
shell plugin implements a subset.  This document details what features of
the Standard 5.0 [#std]_ are implemented.  When this depends on
flux-pmix having been built with a particular openpmix server version,
this is noted.
:doc:`openpmix_interop` describes what happens when the openpmix server
and client versions differ.

The sections follow the chapters of the Standard.

Every server version flux-pmix builds against, 3.2.3 and later,
provides each implemented feature, and every client since 2.1 can use
it, except as noted.  A feature marked **no** is one the server
answers with ``PMIX_ERR_NOT_SUPPORTED``.  Where the note gives an
OpenPMIx version, that is the server version from which flux-pmix
could add the feature.

Initialization and Finalization
-------------------------------

Chapter 4 of the Standard, Client Initialization and Finalization.  A
client connects to the server in its job shell when it initializes.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_Init`, :func:`PMIx_Finalize`
     - yes
     - The server admits only the job owner.  See AUTHENTICATION in
       :man7:`flux-shell-pmix`.

Data Access and Sharing
-----------------------

Chapter 5 of the Standard, Data Access and Sharing.  A client stores
data with :func:`PMIx_Put` and :func:`PMIx_Commit`, and a fence
carries it to the other ranks.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_Put`, :func:`PMIx_Commit`, :func:`PMIx_Get`
     - yes
     -

   * - :func:`PMIx_Get` of a key not exchanged in a fence
     - no
     - Answered with ``PMIX_ERR_NOT_IMPLEMENTED``, so the get fails
       rather than hangs.

The Standard calls the second case a direct modex: a get of a key that
a rank on another node has put but not yet exchanged through a fence.
Open MPI and MPICH exchange everything in a fence at startup and do
not depend on it.

Reserved Keys
-------------

Chapter 6 of the Standard, Reserved Keys.  The server registers these
keys when the job starts, and a client reads them with
:func:`PMIx_Get` without a request to the server.  The realm is the
one the Standard assigns to the key.

.. list-table::
   :header-rows: 1
   :widths: 12 40 8 20

   * - Realm
     - Key
     - Status
     - Notes

   * - Session
     - :const:`PMIX_UNIV_SIZE`
     - yes
     - The job size.

   * - Session
     - :const:`PMIX_SESSION_ID`
     - yes
     - Needs a 5.0 client.

   * - Job
     - :const:`PMIX_JOBID`
     - yes
     - The Flux job id.

   * - Job
     - :const:`PMIX_JOB_SIZE`, :const:`PMIX_JOB_NUM_APPS`, :const:`PMIX_NUM_NODES`,
       :const:`PMIX_NODE_LIST`
     - yes
     -

   * - Job
     - :const:`PMIX_TMPDIR`, :const:`PMIX_TDIR_RMCLEAN`
     - yes
     -

   * - Job
     - :const:`PMIX_ANL_MAP`
     - yes
     - The PMI-1 process map.

   * - Application
     - :const:`PMIX_APPNUM`, :const:`PMIX_WDIR`
     - yes
     - :const:`PMIX_WDIR` is not found by a 3.2 client on a 4.x server.

   * - Node
     - :const:`PMIX_HOSTNAME`, :const:`PMIX_NODEID`, :const:`PMIX_LOCAL_SIZE`,
       :const:`PMIX_LOCAL_PEERS`, :const:`PMIX_LOCALLDR`
     - yes
     - For every node.

   * - Node
     - :const:`PMIX_LOCAL_PROCS`, :const:`PMIX_NODE_OVERSUBSCRIBED`
     - yes
     - Needs a 4.0 client.  For every node.

   * - Process
     - :const:`PMIX_LOCAL_RANK`, :const:`PMIX_NODE_RANK`, :const:`PMIX_NODEID`,
       :const:`PMIX_APPNUM`, :const:`PMIX_GLOBAL_RANK`, :const:`PMIX_APP_RANK`,
       :const:`PMIX_SPAWNED`, :const:`PMIX_REINCARNATION`, :const:`PMIX_HOSTNAME`
     - yes
     - For every rank.  :const:`PMIX_NODEID` of a remote rank needs a
       5.0 client.

   * - Node
     - :const:`PMIX_NODE_SIZE`
     - no
     - Counts processes of every job on the node.

   * - Process
     - :const:`PMIX_CPUSET`, :const:`PMIX_LOCALITY_STRING`,
       :const:`PMIX_PACKAGE_RANK`
     - no
     - Flux binds tasks after the server starts.

   * - Process
     - :const:`PMIX_PROCDIR`
     - no
     -

A key registered for every node or rank can be asked about a remote
node or rank.

Query Operations
----------------

Chapter 7 of the Standard, Query Operations.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_Query_info`
     - no
     - Open MPI sends this at startup and tolerates the error.

Synchronization
---------------

Chapter 8 of the Standard, Synchronization.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_Fence`
     - yes
     - Whole job only.

A fence over the ranks of one node completes in OpenPMIx without
reaching the host.  A fence over any other subset of ranks fails with
``PMIX_ERR_NOT_SUPPORTED``.

Publish/Lookup Operations
-------------------------

Chapter 9 of the Standard, Publish/Lookup Operations.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_Publish`, :func:`PMIx_Lookup`, :func:`PMIx_Unpublish`
     - no
     -

Event Notification
------------------

Chapter 10 of the Standard, Event Notification.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_Register_event_handler`,
       :func:`PMIx_Deregister_event_handler`
     - yes
     -

   * - :func:`PMIx_Notify_event`
     - yes
     - Delivered within a node only.

The server logs each event it receives as a shell warning.  An event
a client notifies reaches the clients on its node that registered for
it.  Nothing carries it to other nodes.

Process Management
------------------

Chapter 12 of the Standard, Process Management.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_Abort`
     - yes
     - Raises a fatal job exception.

   * - :func:`PMIx_Spawn`
     - no
     - :func:`MPI_Comm_spawn` does not work.

   * - :func:`PMIx_Connect`, :func:`PMIx_Disconnect`
     - no
     -

   * - :func:`PMIx_Load_topology`
     - yes
     - Returns the topology the server shares.  See TOPOLOGY in
       :man7:`flux-shell-pmix`.

The status passed to :func:`PMIx_Abort` is in the exception message
but does not become the job's exit code.

Job Management and Reporting
----------------------------

Chapter 13 of the Standard, Job Management and Reporting.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_Allocation_request`
     - no
     -

   * - :func:`PMIx_Job_control`
     - no
     - Open MPI sends this at startup and tolerates the error.

   * - :func:`PMIx_Process_monitor`
     - no
     -

   * - :func:`PMIx_Log`
     - yes
     - See LOGGING in :man7:`flux-shell-pmix`.

Process Sets and Groups
-----------------------

Chapter 14 of the Standard, Process Sets and Groups.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :const:`PMIX_PSET_NAMES`, :const:`PMIX_PSET_MEMBERS`
     - no
     - Needs a 4.2 server.

   * - :func:`PMIx_Group_construct`, :func:`PMIx_Group_destruct`,
       :func:`PMIx_Group_invite`, :func:`PMIx_Group_join`,
       :func:`PMIx_Group_leave`
     - no
     - Needs a 4.2 server.  MPI Sessions in Open MPI 5.0 and later
       are built on groups.

Fabric Support
--------------

Chapter 15 of the Standard, Fabric Support Definitions.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_Fabric_register`, :func:`PMIx_Fabric_update`,
       :func:`PMIx_Fabric_deregister`
     - no
     - Needs a 4.2 server.

Security
--------

Chapter 16 of the Standard, Security.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_Get_credential`, :func:`PMIx_Validate_credential`
     - no
     -

Credentials are for tools that connect to a system-level server and
need more than the uid check the server library makes, so they go
with tool connections, which are unsupported.

Tools and Debuggers
-------------------

Chapter 18 of the Standard, Tools and Debuggers.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_tool_init`, :func:`PMIx_tool_attach_to_server`
     - no
     - Debuggers and other tools cannot attach.

   * - :func:`PMIx_IOF_pull`, :func:`PMIx_IOF_push`
     - no
     -

Beyond Standard 5.0
-------------------

OpenPMIx offers these to a server before or without the Standard
defining them.

.. list-table::
   :header-rows: 1
   :widths: 30 8 42

   * - Feature
     - Status
     - Notes

   * - :func:`PMIx_Session_control`
     - no
     - Needs a 5.0 server.

   * - :func:`PMIx_Resource_block`
     - no
     - Needs a 6.1 server.

.. [#std] PMIx Standard, version 5.0, PMIx Administrative Steering
   Committee, May 2023:
   https://github.com/pmix/pmix-standard/releases/download/v5.0/pmix-standard-v5.0.pdf
