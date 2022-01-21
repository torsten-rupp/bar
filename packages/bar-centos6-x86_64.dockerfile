FROM centos:6
ENV container docker

# use different repository
# see: https://www.getpagespeed.com/server-setup/how-to-fix-yum-after-centos-6-went-eol
RUN curl https://www.getpagespeed.com/files/centos6-eol.repo --output /etc/yum.repos.d/CentOS-Base.repo;

RUN yum -y install centos-release-scl;
RUN curl https://www.getpagespeed.com/files/centos6-scl-eol.repo --output /etc/yum.repos.d/CentOS-SCLo-scl.repo;
RUN curl https://www.getpagespeed.com/files/centos6-scl-rh-eol.repo --output /etc/yum.repos.d/CentOS-SCLo-scl-rh.repo;

# update
RUN yum -y update
RUN yum -y upgrade
#RUN yum list all | grep devtoolset;

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
  git \
  initscripts \
  lua \
  m4 \
  mariadb-client \
  openssl \
  patch \
  pkg-config \
  postgresql \
  psmisc \
  rpm-build \
  rsync \
  sudo \
  subversion \
  sudo \
  tar \
  tcl \
  unzip \
  wget \
  xz \
  ;
# Note: no valgrind available
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
  ;

# required for gcc 7
RUN yum -y install \
  devtoolset-7 \
  ;

# mounts
RUN install -d /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
