FROM ubuntu:14.04
ENV container docker

# add user for build process
RUN groupadd -g 1000 build
RUN useradd -g 1000 -u 1000 build

# disable interactive installion
ENV DEBIAN_FRONTEND noninteractive

# install packages
RUN apt-get -y update
RUN apt-get -y install \
  bc \
  bzip2 \
  coreutils \
  curl \
  debhelper \
  devscripts \
  e2fsprogs \
  gettext \
  imagemagick \
  joe \
  less \
  lua5.1 \
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
  ;
RUN apt-get -y install \
  autotools-dev \
  dh-autoreconf \
  gcc \
  g++ \
  libc6 \
  libc6-dev \
  default-jdk \
  default-jre \
  cmake \
  make \
  unoconv \
  txt2man \
  valgrind \
  ;
RUN apt-get -y install \
  libmysqlclient-dev \
  libpq-dev \
  ;

# mounts
RUN install -d /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
