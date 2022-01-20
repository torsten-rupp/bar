FROM centos:7
ENV container docker

# add user for build process
RUN groupadd -g 1000 build
RUN useradd -g 1000 -u 1000 build

# install packages
RUN yum -y install \
  bc \
  bzip2 \
  coreutils \
  curl \
  e2fsprogs \
  gettext \
  initscripts \
  joe \
  less \
  lua \
  m4 \
  openssl \
  patch \
  pkg-config \
  psmisc \
  rpm-build \
  rsync \
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
  bison \
  flex \
  rpm-build \
  valgrind \
  ;
RUN yum -y install \
  postgresql-devel \
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
