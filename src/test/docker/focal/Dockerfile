FROM fluxrm/flux-core:focal

ARG USER=fluxuser
ARG UID=1000
ARG OMPI_BRANCH=v5.0.x
ARG OPENPMIX_BRANCH=v4.1.1rc2

RUN \
 if test "$USER" != "fluxuser"; then  \
      sudo groupadd -g $UID $USER \
   && sudo useradd -g $USER -u $UID -d /home/$USER -m $USER \
   && sudo sh -c "printf \"$USER ALL= NOPASSWD: ALL\\n\" >> /etc/sudoers" \
   && sudo adduser $USER sudo ; \
 fi

# ompi can't coexist with mpich
RUN sudo apt remove -yy mpich libmpich-dev \
 && sudo apt clean

# install ompi prereqs
RUN sudo apt-get update \
 && sudo apt-get -qq install -y --no-install-recommends \
    libevent-dev \
    flex \
    openssh-client \
 && sudo apt clean \
 && sudo rm -rf /var/lib/apt/lists/*

# build/install openpmix
RUN cd /tmp \
 && git clone -b ${OPENPMIX_BRANCH} \
      --recursive --depth=1 https://github.com/openpmix/openpmix \
 && cd openpmix \
 && git branch \
 && ./autogen.pl \
 && ./configure --prefix=/usr \
      --enable-debug \
 && make -j $(nproc) \
 && sudo make install \
 && cd .. \
 && rm -rf openpmix

# build/install ompi
RUN cd /tmp \
 && git clone -b ${OMPI_BRANCH} \
      --recursive --depth=1 https://github.com/open-mpi/ompi \
 && cd ompi \
 && git branch \
 && ./autogen.pl \
 && ./configure --prefix=/usr \
      --disable-man-pages --enable-debug --enable-mem-debug \
      --with-pmix=external --with-libevent \
 && make -j $(nproc) \
 && sudo make install \
 && cd .. \
 && rm -rf ompi

USER $USER
WORKDIR /home/$USER

