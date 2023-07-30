FROM i386/centos:8
ENV container docker

# fix architecture in repository sources
RUN sed -i 's/\$basearch/i386/g' /etc/yum.conf
RUN sed -i 's/\$arch/i686/g' /etc/yum.repos.d/*
RUN sed -i 's/\$basearch/i386/g' /etc/yum.repos.d/*

# add user for build process
RUN groupadd -g 1000 build
RUN useradd -g 1000 -u 1000 build

# update
RUN yum -y update
RUN yum -y upgrade

# install packages
RUN yum -y install \
  bc \
  bzip2 \
  coreutils \
  curl \
  e2fsprogs \
  gettext \
  git \
  initscripts \
  joe \
  less \
  lua \
  m4 \
  mysql-client \
  openssl \
  patch \
  pkg-config \
  postgresql \
  psmisc \
  rpm-build \
  rsync \
  socat \
  sqlite \
  subversion \
  sudo \
  tar \
  tcl \
  unzip \
  wget \
  xz \
  ;
RUN yum -y install \
  gcc \
  gcc-c++ \
  java-1.6.0-openjdk-devel \
  jre \
  cmake \
  make \
  libsystemd-dev \
  bison \
  flex \
  rpm-build \
  valgrind \
  devtoolset-7-valgrind-devel \
  ;

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

# mounts
RUN install -d /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
