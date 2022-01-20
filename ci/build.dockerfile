FROM ubuntu:18.04
ENV container docker

# disable interactive installion
ENV DEBIAN_FRONTEND noninteractive

# update
RUN apt-get -y update

# install packages
RUN apt-get -y install \
  bc \
  bzip2 \
  curl \
  debhelper \
  devscripts \
  e2fsprogs \
  gettext \
  git \
  imagemagick \
  joe \
  less \
  lua5.3 \
  m4 \
  mariadb-client mariadb-client-core-10.1 \
  patch \
  pkg-config \
  postgresql \
  rsync \
  subversion \
  sudo \
  tar \
  tcl \
  unzip \
  wget \
  xz-utils \
  zip \
  ;
RUN apt-get -y install \
  gcc \
  g++ \
  libc6 \
  libc6-dev \
  libpq-dev \
  openjdk-8-jdk \
  openjdk-8-jre \
  cmake \
  make \
  bison \
  flex \
  unoconv \
  txt2man \
  valgrind \
  lcov \
  binutils \
  ;

RUN mkdir /.cache;
