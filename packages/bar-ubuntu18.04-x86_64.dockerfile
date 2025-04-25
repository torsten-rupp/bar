FROM ubuntu:18.04
ENV container docker

# variables
ARG uid=1000
ARG gid=1000

# add user for build process
RUN userdel `id -un $uid 2>/dev/null` 2>/dev/null || true
RUN groupadd -g $gid build || true
RUN useradd -g $gid -u $uid build -m

# disable interactive installion
ENV DEBIAN_FRONTEND noninteractive

#autogen
#libc6
#libc6-dev
#binutils
#libssl-dev
#openssl
#subversion

# update
RUN apt-get -y update

# install packages
RUN apt-get -y install \
  bc \
  bzip2 \
  coreutils \
  curl \
  debhelper \
  devscripts \
  e2fsprogs \
  gettext \
  git \
  joe \
  less \
  lua5.3 \
  m4 \
  mysql-client \
  patch \
  pkg-config \
  postgresql \
  rsync \
  socat \
  sqlite3 \
  subversion \
  sudo \
  tar \
  tcl \
  unzip \
  wget \
  xz-utils \
  ;
RUN apt-get -y install \
  gcc \
  g++ \
  libc6 \
  libc6-dev \
  libsystemd-dev \
  openjdk-8-jdk \
  openjdk-8-jre \
  cmake \
  make \
  bison \
  flex \
  unoconv \
  txt2man \
  valgrind \
  ;

# mounts
RUN install -d /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
