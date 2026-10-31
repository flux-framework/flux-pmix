FROM fluxrm/flux-core:el8

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
   && sudo usermod -G wheel $USER \
   && sudo usermod -G wheel fluxuser ; \
 fi

# ompi can't coexist with mpich
RUN cd /tmp \
 && mkdir mvapich2 \
 && cd mvapich2 \
 && wget -O - http://mvapich.cse.ohio-state.edu/download/mvapich/mv2/mvapich2-2.3.6.tar.gz | tar xvz --strip-components 1 \
 && ./configure --with-device=ch3:sock --disable-fortran --prefix=/usr \
 && sudo make uninstall \
 && cd .. \
 && rm -rf mvapich2

# prereqs for building openpmix and Open MPI, plus the distribution's
# pmix when no client ref is given and its Open MPI when no Open MPI
# ref is given.  A built library and a distribution one have the same
# soname, and Open MPI's binaries carry no rpath, so only one of each
# may be installed.  The distribution's Open MPI lands under
# /usr/lib64/openmpi, which ENV puts on the paths.
RUN sudo yum -y update \
 && sudo yum -y install \
    libevent-devel \
    zlib-devel \
    $(test -n "$OPENPMIX_CLIENT" || echo pmix-devel) \
    $(test -n "$OMPI" || echo openmpi-devel) \
 && sudo yum -y remove mpich mpich-devel \
 && sudo yum clean all
ENV PATH=/usr/lib64/openmpi/bin:$PATH \
    PKG_CONFIG_PATH=/usr/lib64/openmpi/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/share/pkgconfig

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
