FROM ubuntu:18.04
ENV container docker

# add user for build process
RUN groupadd -g 1000 build
RUN useradd -g 1000 -u 1000 build

# disable interactive installion
ENV DEBIAN_FRONTEND noninteractive

# install packages
RUN dpkg --add-architecture i386;
RUN apt-get -y update;
RUN apt-get -y install \
  bc \
  bzip2 \
  coreutils \
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
RUN apt-get -y install \
  libmysqlclient-dev \
  libpq-dev \
  ;

# install Inno Setup
RUN apt-get -y install \
  xvfb \
  ;
RUN wineboot --update;
RUN wget -q -O /tmp/innosetup-5.6.1.exe https://files.jrsoftware.org/is/5/innosetup-5.6.1.exe
RUN DISPLAY=:0.0 xvfb-run -n 0 -s "-screen 0 1024x768x16" wine /tmp/innosetup-5.6.1.exe /VERYSILENT /SUPPRESSMSGBOXES

# mount /media/home
RUN mkdir /media/home && chown root /media/home
VOLUME [ "/media/home" ]

# mounts
RUN install -d /media/home && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
