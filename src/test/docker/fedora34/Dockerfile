FROM fluxrm/flux-core:fedora34

ARG USER=fluxuser
ARG UID=1000
ARG OMPI_BRANCH=v5.0.x

RUN \
 if test "$USER" != "fluxuser"; then  \
      sudo groupadd -g $UID $USER \
   && sudo useradd -g $USER -u $UID -d /home/$USER -m $USER \
   && sudo sh -c "printf \"$USER ALL= NOPASSWD: ALL\\n\" >> /etc/sudoers" \
   && sudo usermod -G wheel $USER \
   && sudo usermod -G wheel fluxuser ; \
 fi

# install ompi prereqs
RUN sudo yum -y update \
 && sudo yum -y install \
    flex \
    libevent-devel \
    zlib-devel \
 && sudo yum -y remove mpich mpich-devel \
 && sudo yum clean all

# build/install ompi
RUN cd /tmp \
 && git clone -b ${OMPI_BRANCH} \
      --recursive --depth=1 https://github.com/open-mpi/ompi \
 && cd ompi \
 && git branch \
 && ./autogen.pl \
 && ./configure --prefix=/usr \
      --disable-man-pages --enable-debug --enable-mem-debug \
 && make -j $(nproc) \
 && sudo make install \
 && cd .. \
 && rm -rf ompi

USER $USER
WORKDIR /home/$USER

