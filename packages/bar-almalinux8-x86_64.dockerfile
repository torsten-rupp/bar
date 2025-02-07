FROM almalinux:8
ENV container docker

# update
RUN yum -y update
RUN yum -y upgrade

# install packages
RUN yum -y install \
  initscripts \
  openssl \
  jre \
  ;

RUN yum -y install \
  bc \
  less \
  lua \
  openssl \
  psmisc \
  rsync \
  socat \
  ;

RUN yum -y install \
  autoconf \
  automake \
  bison \
  bzip2 \
  cmake \
  curl \
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
  sqlite \
  sudo \
  tar \
  tcl \
  unzip \
  valgrind \
  valgrind-devel \
  wget \
  xz \
  ;

# add user for build process
RUN groupadd -g 1000 build
RUN useradd -g 1000 -u 1000 build

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

# enable sudo for all
RUN echo "ALL ALL = (ALL) NOPASSWD: ALL" > /etc/sudoers.d/all

# add external third-party packages
COPY download-third-party-packages.sh /root
RUN /root/download-third-party-packages.sh --no-decompress --destination-directory /media/extern
RUN rm -f /root/download-third-party-packages.sh

# mounts
RUN install -d /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
