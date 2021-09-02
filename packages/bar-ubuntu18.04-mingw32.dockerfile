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
RUN dpkg --add-architecture i386;
RUN apt-get -y update
RUN apt-get -y install \
  bc \
  bzip2 \
  curl \
  debhelper \
  devscripts \
  e2fsprogs \
  gettext \
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
  ;
RUN apt-get -y install \
  gcc \
  g++ \
  libc6 \
  libc6-dev \
  openjdk-8-jdk \
  openjdk-8-jre \
  make \
  mingw-w64-i686-dev \
  gcc-mingw-w64-i686 \
  g++-mingw-w64-i686 \
  mingw-w64-x86-64-dev \
  gcc-mingw-w64-x86-64 \
  g++-mingw-w64-x86-64 \
  unoconv \
  txt2man \
  valgrind \
  wine32 \
  ;

# mount /media/home
RUN mkdir /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

# mount /media/wine
RUN mkdir /media/wine  && chown root /media/wine
VOLUME [ "/media/wine" ]

CMD ["/usr/sbin/init"]
