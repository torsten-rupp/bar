FROM ubuntu:18.04
ENV container docker

ARG UID=1000
ARG GID=1000

# add user for build process
RUN groupadd -g $GID build
RUN useradd -g $GID -u $UID build

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
  xvfb \
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

# install Inno Setup (user root+build)
RUN wineboot --update
RUN wget -q -O /tmp/innosetup-5.6.1.exe https://files.jrsoftware.org/is/5/innosetup-5.6.1.exe
RUN DISPLAY=:0.0 xvfb-run -n 0 -s "-screen 0 1024x768x16" wine /tmp/innosetup-5.6.1.exe /VERYSILENT /SUPPRESSMSGBOXES
RUN rm -f /tmp/innosetup-5.6.1.exe
RUN install -d /home/build; cp -r /root/.wine /home/build; chown -R build:build /home/build/.wine

# mount /media/home
RUN mkdir /media/home && chown root /media/home
VOLUME [ "/media/home" ]

# mounts
RUN install -d /media/home && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
