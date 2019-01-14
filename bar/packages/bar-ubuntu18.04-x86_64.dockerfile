FROM ubuntu:18.04
ENV container docker

# add user for build process
RUN groupadd -g 1000 build
RUN useradd -g 1000 -u 1000 build

# disable interactive installion
ENV DEBIAN_FRONTEND noninteractive

#autogen
#libc6
#libc6-dev
#binutils
#libssl-dev
#openssl
#subversion

# install packages
RUN apt-get -y update
RUN apt-get -y install \
  bc \
  bzip2 \
  debhelper \
  devscripts \
  e2fsprogs \
  gettext \
  lua5.3 \
  m4 \
  patch \
  pkg-config \
  subversion \
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
  openjdk-8-jdk \
  openjdk-8-jre \
  make \
  ;

# mount /media/home
RUN mkdir /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
