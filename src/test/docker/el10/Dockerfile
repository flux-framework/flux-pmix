FROM fluxrm/flux-core:el10

ARG USER=fluxuser
ARG UID=1000
ARG OMPI_BRANCH=v5.0.x
ARG OPENPMIX_BRANCH=v4.2.9

# Set NATIVE=1 to use the distribution's openmpi and pmix packages
# instead of building the branches above.  That tests the versions a
# stock el10 system has, which Open MPI links against the system pmix.
ARG NATIVE=

RUN \
 if test "$USER" != "fluxuser"; then  \
      sudo groupadd -g $UID $USER \
   && sudo useradd -g $USER -u $UID -d /home/$USER -m $USER \
   && sudo sh -c "printf \"$USER ALL= NOPASSWD: ALL\\n\" >> /etc/sudoers" \
   && sudo usermod -G wheel $USER \
   && sudo usermod -G wheel fluxuser ; \
 fi

# install ompi prereqs, and tshark for the capture tests
RUN sudo yum -y update \
 && sudo yum -y install \
    libevent-devel \
    zlib-devel \
    wireshark-cli \
 && sudo yum -y remove mpich mpich-devel \
 && sudo yum clean all

# native: install the distribution packages.  They put Open MPI under
# /usr/lib64/openmpi, which the ENV below puts on the paths.
RUN if test -n "$NATIVE"; then \
      sudo yum -y install pmix-devel openmpi-devel \
   && sudo yum clean all; \
    fi
ENV PATH=/usr/lib64/openmpi/bin:$PATH \
    PKG_CONFIG_PATH=/usr/lib64/openmpi/lib/pkgconfig:/usr/lib64/pkgconfig:/usr/share/pkgconfig

 # build/install openpmix
RUN if test -z "$NATIVE"; then \
    cd /tmp \
 && git clone -b ${OPENPMIX_BRANCH} \
      --recursive --depth=1 https://github.com/openpmix/openpmix \
 && cd openpmix \
 && git branch \
 && ./autogen.pl \
 && ./configure --prefix=/usr \
      --enable-debug --disable-sphinx \
 && make -j $(nproc) \
 && sudo rm -rf /usr/lib64/pmix \
 && sudo make install \
 && cd .. \
 && rm -rf openpmix; \
    fi

# build/install ompi
RUN if test -z "$NATIVE"; then \
    cd /tmp \
 && git clone -b ${OMPI_BRANCH} \
      --recursive --depth=1 https://github.com/open-mpi/ompi \
 && cd ompi \
 && git branch \
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
