FROM fluxrm/flux-core:el8

ARG USER=fluxuser
ARG UID=1000
ARG OMPI_BRANCH=v5.0.x
ARG OPENPMIX_BRANCH=v4.2.2

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

# install ompi prereqs
RUN sudo yum -y update \
 && sudo yum -y install \
    libevent-devel \
    zlib-devel \
 && sudo yum clean all

 # build/install openpmix
RUN cd /tmp \
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
      --with-pmix=external --with-libevent --disable-sphinx \
 && make -j $(nproc) \
 && sudo make install \
 && cd .. \
 && rm -rf ompi

USER $USER
WORKDIR /home/$USER

