### flux-pmix

This repo contains a flux shell plugin that is required for bootstrapping
openmpi v5+ MPI executables.

The plugin is loaded by requesting it at job submission time with
the `-ompi=openmpi@5` option.

Typically this project would be configured with the same `--prefix` as
flux-core.  If that is not practical, for example in a spack environment,
then flux can be told to look elsewhere by setting `FLUX_SHELL_RC_PATH`, e.g.

```
FLUX_SHELL_RC_PATH=$prefix/etc/flux/shell/lua.d:$FLUX_SHELL_RC_PATH
```
where `$prefix` is the installation prefix of this project.

### limitations

Clients connect to pmix servers using TCP over IPv4 localhost.  The pmix
"native" security mechanism is used, which has not been vetted by the Flux
team.  If in doubt, do not use this plugin when nodes are shared by multiple
users.

The pmix spec covers a broad range of topics.  Although this plugin is based
on the openpmix "reference implementation", it only populates callbacks and
attributes needed to bootstrap some versions of openmpi.  As such, the pmix
interface offered by this plugin should not be used for advanced use cases
such as: debugger/tool interfaces, process spawn, resource allocation,
job control, I/O forwarding, process groups, fabric support, storage support,
event notification, or mpi sessions.

Bootstrap scalability with this plugin is likely to be on par with PMI-1,
since _fence_ is implemented with the same collective algorithm as PMI-1,
and the cost of _fence_ will dominate for the larger jobs.  Therefore, we
currently recommend the well tested PMI-1 default for older openmpi versions.

Although Flux has optional (configure-time) support for being bootstrapped
by pmix, this plugin will not be used when Flux launches Flux, since the
Flux broker preferentially chooses PMI-1 over pmix if both are offered, and
the Flux shell's PMI-1 service is always offered.  In addition, attributes
required for nesting Flux like `PMI_process_mapping` and `flux.instance-level`
are not set by this plugin.

While other MPIs such as mpich and derivatives have optional pmix bootstrap
support, using pmix to launch them under Flux is neither tested nor expected
to confer any advantage over the PMI-1 default.

### license

SPDX-License-Identifier: LGPL-3.0

LLNL-CODE-764420
