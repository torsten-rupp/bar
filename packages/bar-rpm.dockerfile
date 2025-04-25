FROM almalinux:8
ENV container docker

# variables
ARG uid=1000
ARG gid=1000

# update
RUN yum -y update
RUN yum -y upgrade

# install packages
RUN yum -y install \
  initscripts \
  openssl \
  jre \
  ;

RUN yum -y install --allowerasing \
  bc \
  coreutils \
  less \
  lua \
  psmisc \
  rsync \
  socat \
  sudo \
  ;

RUN yum -y install \
  autoconf \
  automake \
  bison \
  bzip2 \
  curl \
  gcc-toolset-9-valgrind-devel \
  e2fsprogs \
  flex \
  gcc \
  gcc-c++ \
  gettext \
  git \
  java-1.8.0-openjdk-devel \
  libtool \
  m4 \
  make \
  mariadb \
  patch \
  pkg-config \
  postgresql \
  rpm-build \
  rpm-build \
  sqlite \
  tar \
  tcl \
  unzip \
  valgrind \
  wget \
  xz \
  ;

RUN yum -y install \
  cmake \
  ;

# add cmake 3.2
#RUN    wget https://cmake.org/files/v3.2/cmake-3.2.3.tar.gz \
#    && tar xf cmake-3.2.3.tar.gz \
#    && (cd cmake-3.2.3; ./bootstrap) \
#    && (cd cmake-3.2.3; make) \
#    && (cd cmake-3.2.3; make install) \
#    && rm -rf cmake-3.2.3

# add user for build process
RUN    userdel `id -un $uid 2>/dev/null` 2>/dev/null || true \
    && groupadd -g $gid jenkins || true \
    && useradd -g $gid -u $uid jenkins -m

# enable sudo for all
RUN echo "ALL ALL = (ALL) NOPASSWD: ALL" > /etc/sudoers.d/all

# fix systemd
RUN (cd /lib/systemd/system/sysinit.target.wants/; \
     for i in *; do [ $i == systemd-tmpfiles-setup.service ] || rm -f $i; \
     done; \
    ); \
    rm -f /lib/systemd/system/multi-user.target.wants/*;\
    rm -f /etc/systemd/system/*.wants/*;\
    rm -f /lib/systemd/system/local-fs.target.wants/*; \
    rm -f /lib/systemd/system/sockets.target.wants/*udev*; \
    rm -f /lib/systemd/system/sockets.target.wants/*initctl*; \
    rm -f /lib/systemd/system/basic.target.wants/*;\
    rm -f /lib/systemd/system/anaconda.target.wants/*;
VOLUME [ "/sys/fs/cgroup" ]

# add external third-party packages
COPY download-third-party-packages.sh /root
RUN    /root/download-third-party-packages.sh --no-decompress --destination-directory /media/extern \
    && rm /root/download-third-party-packages.sh

# mounts
RUN install -d /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
