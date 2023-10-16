FROM centos:7
ENV container docker

ARG uid=0
ARG gid=0

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
  autoconf \
  automake \
  gcc \
  gcc-c++ \
  java-1.6.0-openjdk-devel \
  jre \
  libtool \
  make \
  bison \
  flex \
  rpm-build \
  valgrind \
  devtoolset-7-valgrind-devel \
  ;

RUN yum -y install \
cmake \
;

# add cmake 3.2
#RUN wget https://cmake.org/files/v3.2/cmake-3.2.3.tar.gz
#RUN tar xf cmake-3.2.3.tar.gz
#RUN (cd cmake-3.2.3; ./bootstrap)
#RUN (cd cmake-3.2.3; make)
#RUN (cd cmake-3.2.3; make install)
#RUN rm -rf cmake-3.2.3

# add users
RUN groupadd -g 1000 build
RUN useradd -g 1000 -u 1000 build
RUN groupadd -g ${gid} jenkins
RUN useradd -m -u ${uid} -g ${gid} -p `openssl passwd jenkins` jenkins

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

# mounts
RUN install -d /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
