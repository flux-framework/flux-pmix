### flux-pmix

This repo adds basic pmix support to Flux.  It contains two main components
- a Flux shell plugin that adds pmix service to the flux shell for launching
  some versions of openmpi
- a Flux PMI client plugin that enables Flux to be launched by a foreign
  launcher that only supports pmix

The former is loaded by requesting it on the job submission command line, e.g.
```
$ flux run -o pmi=pmix myprogram
```

The latter may be selected on the broker command line, e.g.
```console
$ jsrun flux start -o,-Sbroker.boot-method=pmix
```

or by setting the `FLUX_PMI_CLIENT_METHODS` environment variable globally:
```
FLUX_PMI_CLIENT_METHODS="simple pmix single"
```

### installation

##### Building From Source

```console
./autogen.sh
./configure --localstatedir=/var/
make -j
make check
```

To configure flux-pmix with a side-installed version of flux-core, you will
need to make sure that side install's pkg-config files are exported. To do that,
something like this will work:

```console
export PKG_CONFIG_PATH=${flux_install}/lib/pkgconfig:$(pkg-config --variable pc_path pkg-config)
```

Run that before `make`.


### limitations

The pmix specs cover a broad range of topics.  Although the shell plugin is
based on the openpmix "reference implementation", it only populates callbacks
and attributes needed to bootstrap some versions of openmpi.  As such, the pmix
interface offered by this plugin should not be used for advanced use cases
such as: debugger/tool interfaces, process spawn, resource allocation,
job control, I/O forwarding, process groups, fabric support, storage support,
event notification, or mpi sessions.

Bootstrap scalability with this plugin is likely to be on par with Flux PMI-1,
since _fence_ is implemented with the same collective algorithm as PMI-1,
and the cost of _fence_ will dominate for the larger jobs.

Although Flux can now launch Flux with pmix by combining the two capabilities
provided by this project, this is not advisable as the Flux pmix server does
not pre-populate keys that Flux expects to find in the PMI KVS when launched
in a hierarchy, such as `flux.instance-level` and `flux.taskmap`.

While other MPIs such as mpich and derivatives have optional pmix bootstrap
support, using pmix to launch them under Flux is neither well tested nor
expected to confer any advantage over the PMI-1 default.

Since the pmix server is embedded in the flux-shell, it is completely
initialized and torn down for each job, and runs unprivileged as the job owner.

### license

SPDX-License-Identifier: LGPL-3.0

LLNL-CODE-764420
