# flux-pmix

flux-pmix adds [PMIx](https://openpmix.github.io) support to the
[Flux](https://flux-framework.org) resource manager framework.  It has two
components:

- a `flux-shell` plugin that runs a PMIx server in each job shell, so Flux
  can launch programs built with Open MPI, MPICH, or another PMIx client:
  ```console
  $ flux run -o pmi=pmix myprogram
  ```
- a Flux PMI client plugin that lets Flux bootstrap under a launcher that
  offers only PMIx:
  ```console
  $ jsrun flux start -o,-Sbroker.boot-method=pmix
  ```

The [documentation](https://flux-framework.readthedocs.io/projects/flux-pmix)
covers building and installing, the PMIx features implemented, the shell
options, OpenPMIx version interoperability, and tracing the PMIx protocol.

## Build

```console
$ ./autogen.sh
$ ./configure
$ make -j
$ make check
$ make install
```

Install flux-pmix to the same prefix as flux-core so the plugins are found
on flux's default search paths.  See the build guide for the configure
options that select the OpenPMIx and MPI libraries.

## License

SPDX-License-Identifier: LGPL-3.0

LLNL-CODE-764420
