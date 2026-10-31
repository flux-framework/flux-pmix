FROM fluxrm/flux-core:jammy

ARG USER=fluxuser
ARG UID=1000

# Each of these names a git ref to build, or is empty to use the
# distribution's package instead.  An openpmix ref containing * names
# the newest tag that matches it.  OPENPMIX_CLIENT and OMPI install in
# the system prefix, where flux-pmix's test clients, its upmi plugin,
# and Open MPI find them as they would on any system.  OPENPMIX_SERVER
# installs under /opt/openpmix-server, and the matrix row passes
# --with-pmix-server pointing there.  Neither is built with debug,
# which would make buffers fully described.
ARG OPENPMIX_SERVER=
ARG OPENPMIX_CLIENT=
ARG OMPI=

# A built client openpmix replaces the distribution's, but the
# distribution's Open MPI depends on the distribution's pmix, so a
# client ref needs an Open MPI ref too (or none at all).
RUN if test -n "$OPENPMIX_CLIENT" -a -z "$OMPI"; then \
      echo "OPENPMIX_CLIENT needs OMPI: distribution Open MPI uses distribution pmix" >&2; \
      exit 1; \
    fi

RUN \
 if test "$USER" != "fluxuser"; then  \
      sudo groupadd -g $UID $USER \
   && sudo useradd -g $USER -u $UID -d /home/$USER -m $USER \
   && sudo sh -c "printf \"$USER ALL= NOPASSWD: ALL\\n\" >> /etc/sudoers" \
   && sudo adduser $USER sudo ; \
 fi

# The base image carries an openpmix and a prrte from 2021 that nothing
# uses (flux-framework/flux-core#7870 drops them); clear them so that a
# client ref built below is the only openpmix in the system prefix.
RUN sudo rm -rf /usr/lib/libpmix.* /usr/lib/pmix /usr/lib/pkgconfig/pmix.pc \
      /usr/include/pmix*.h /usr/include/pmix /usr/share/pmix \
      /usr/etc/pmix-mca-params.conf /usr/bin/pmix_info /usr/bin/pmixcc \
      /usr/bin/pattrs /usr/bin/pevent /usr/bin/plookup /usr/bin/pps \
      /usr/bin/pquery \
      /usr/lib/libprrte.* /usr/lib/prte /usr/include/prte* /usr/share/prte \
      /usr/bin/prte /usr/bin/prterun /usr/bin/prun /usr/bin/pterm \
      /usr/bin/prte_info \
 && sudo ldconfig

# prereqs for building openpmix and Open MPI, plus the distribution's
# pmix when no client ref is given and its Open MPI when no Open MPI
# ref is given.  A built library and a distribution one have the same
# soname, and Open MPI's binaries carry no rpath, so only one of each
# may be installed.
RUN sudo apt purge -yy mpich libmpich-dev \
 && sudo apt-get update \
 && sudo apt-get -qq install -y --no-install-recommends \
    libevent-dev \
    flex \
    openssh-client \
    $(test -n "$OPENPMIX_CLIENT" || echo libpmix-dev) \
    $(test -n "$OMPI" || echo libopenmpi-dev openmpi-bin) \
 && sudo apt clean \
 && sudo rm -rf /var/lib/apt/lists/*

# build the client openpmix into the system prefix and the server one
# into /opt/openpmix-server, when a ref is given.  A base image may
# have left a distribution pmix's components under the prefix.
RUN for side in client server; do \
      eval ref=\$OPENPMIX_$(echo $side | tr a-z A-Z); \
      test -n "$ref" || continue; \
      if test $side = client; then prefix=/usr; \
      else prefix=/opt/openpmix-server; fi; \
      sudo rm -rf $prefix/lib64/pmix $prefix/lib/pmix; \
      case $ref in *\**) ref=$(git ls-remote --tags --refs \
        https://github.com/openpmix/openpmix "$ref" \
        | sed 's,.*/,,; s/rc/~rc/' | sort -V | tail -1 | sed 's/~//'); \
        echo "openpmix $side: newest tag is $ref";; esac; \
      cd /tmp \
   && git clone -b $ref --recursive --depth=1 \
        https://github.com/openpmix/openpmix \
   && cd openpmix \
   && ./autogen.pl \
   && ./configure --prefix=$prefix \
        --disable-sphinx \
   && make -j $(nproc) \
   && sudo make install \
   && cd .. \
   && rm -rf openpmix; \
    done

# build Open MPI when a ref is given
RUN if test -n "$OMPI"; then \
      cd /tmp \
   && git clone -b $OMPI --recursive --depth=1 \
        https://github.com/open-mpi/ompi \
   && cd ompi \
   && ./autogen.pl \
   && ./configure --prefix=/usr \
        --disable-man-pages --enable-debug --enable-mem-debug \
        --with-pmix=external --with-libevent --disable-sphinx \
   && make -j $(nproc) \
   && sudo make install \
   && cd .. \
   && rm -rf ompi; \
    fi

USER $USER
WORKDIR /home/$USER
