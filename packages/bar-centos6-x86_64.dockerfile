FROM centos:6
ENV container docker

ARG uid=0
ARG gid=0

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
  mysql \
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
  sudo \
  tar \
  tcl \
  unzip \
  wget \
  xz \
  ;
# Note: no valgrind available
RUN yum -y install \
  autoconf \
  automake \
  gcc \
  gcc-c++ \
  java-1.6.0-openjdk-devel \
  jre \
  libtool \
  make \
  flex \
  rpm-build \
  valgrind \
  devtoolset-7-valgrind-devel \
  ;

# required for gcc 7
RUN yum -y install \
  devtoolset-7 \
  ;

# add cmake >= 3.2
RUN wget --no-check-certificate --quiet https://cmake.org/files/v3.2/cmake-3.2.3.tar.gz
RUN tar xf cmake-3.2.3.tar.gz
RUN (cd cmake-3.2.3; ./bootstrap)
RUN (cd cmake-3.2.3; make -j`nproc --ignore 1`)
RUN (cd cmake-3.2.3; make install)
RUN rm -rf cmake-3.2.3

# add bison >= 3.0.4
RUN wget --no-check-certificate --quiet https://ftp.gnu.org/gnu/bison/bison-3.0.5.tar.xz
RUN tar xf bison-3.0.5.tar.xz
RUN (cd bison-3.0.5; ./configure)
RUN (cd bison-3.0.5; make -j`nproc --ignore 1`)
RUN (cd bison-3.0.5; make install)
RUN rm -rf bison-3.0.5

# add users
RUN groupadd -g ${gid} jenkins
RUN useradd -m -u ${uid} -g ${gid} -p `openssl passwd jenkins` jenkins

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
