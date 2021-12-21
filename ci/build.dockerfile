FROM ubuntu:18.04
ENV container docker

# disable interactive installion
ENV DEBIAN_FRONTEND noninteractive

# install packages
RUN apt-get -y update
RUN apt-get -y install \
  bc \
  bzip2 \
  curl \
  debhelper \
  devscripts \
  e2fsprogs \
  gettext \
  imagemagick \
  joe \
  less \
  lua5.3 \
  m4 \
  patch \
  pkg-config \
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
  libmysqlclient-dev \
  libpq-dev \
  openjdk-8-jdk \
  openjdk-8-jre \
  cmake \
  make \
  unoconv \
  txt2man \
  valgrind \
  lcov \
  binutils \
  ;

RUN mkdir /.cache;
