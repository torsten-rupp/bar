FROM ubuntu:24.04
ENV container docker

# constants
ARG WINEPREFIX=/tmp/build/.wine

# variables
ARG uid=1000
ARG gid=1000

# disable interactive installion
ENV DEBIAN_FRONTEND noninteractive

# clear APT cache
RUN    apt clean \
    && rm -rf /var/lib/apt/lists/*

# update
RUN apt-get -y update

# install packages
RUN apt-get -y install --fix-missing \
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
  patch \
  pkg-config \
  postgresql \
  psmisc \
  rsync \
  socat \
  sqlite3 \
  subversion \
  sudo \
  tar \
  tcl \
  unzip \
  wget \
  xvfb \
  xz-utils \
  ;
RUN apt-get -y install --fix-missing \
  gcc \
  g++ \
  libblkid-dev \
  libc6 \
  libc6-dev \
  libinih-dev \
  openjdk-8-jdk \
  openjdk-8-jre \
  cmake \
  make \
  bison \
  flex \
  mingw-w64-i686-dev \
  gcc-mingw-w64-i686 \
  g++-mingw-w64-i686 \
  mingw-w64-x86-64-dev \
  gcc-mingw-w64-x86-64 \
  g++-mingw-w64-x86-64 \
  unoconv \
  txt2man \
  valgrind \
  ;

# install wine 32bit
RUN    dpkg --add-architecture i386 \
    && apt-get -y update \
    && apt-get -y install \
         wine32 \
  ;

# add user for build process
RUN    userdel `id -un $uid 2>/dev/null` 2>/dev/null || true \
    && groupadd -g $gid jenkins || true \
    && useradd -g $gid -u $uid jenkins -m

# initialize wine prefix
RUN install -d $WINEPREFIX

# install Inno Setup
RUN ls -la /home
RUN WINEPREFIX=$WINEPREFIX wineboot --init
#RUN wget \
#  -q \
#  -O /tmp/innosetup-5.6.1.exe \
#  https://files.jrsoftware.org/is/5/innosetup-5.6.1.exe
COPY innosetup-5.6.1.exe /tmp
#RUN pwd
#RUN ps auxw|grep wine
#RUN WINEARCH=win32 WINEPREFIX=$WINEPREFIX DISPLAY=:0.0 xvfb-run -e /dev/stdout --auto-servernum wine /tmp/innosetup-5.6.1.exe /VERYSILENT /SUPPRESSMSGBOXES
RUN    WINEARCH=win32 WINEPREFIX=$WINEPREFIX DISPLAY=:0.0 xvfb-run -e /dev/stdout wine /tmp/innosetup-5.6.1.exe /VERYSILENT /SUPPRESSMSGBOXES \
    && rm -rf /tmp/wine* \
    && rm /tmp/innosetup-5.6.1.exe \
    && WINEPREFIX=$WINEPREFIX wineboot --shutdown \
    && WINEPREFIX=$WINEPREFIX wineboot --end-session
#RUN rm -rf /tmp/wine-*

# install wine 64bit
RUN apt-get -y install --fix-missing \
  wine64 \
  ;

# get Windows OpenJDK
#OPENJDK_VERSION=20.0.2
RUN wget \
  --no-check-certificate \
  --quiet \
  --output-document /tmp/jdk-windows-x64_bin.zip \
  https://download.java.net/java/GA/jdk20.0.2/6e380f22cbe7469fa75fb448bd903d8e/9/GPL/openjdk-20.0.2_windows-x64_bin.zip
#RUN (cd /opt; unzip /root/jdk-windows-x64_bin.zip)
#RUN (cd "$WINEPREFIX/drive_c/Program Files"; unzip /tmp/jdk-windows-x64_bin.zip)
#RUN rm -f /tmp/jdk-windows-x64_bin.zip

# install launch4j
RUN wget \
  --no-check-certificate \
  --quiet \
  --output-document /tmp/launch4j-3.14-linux-x64.tgz \
  https://sourceforge.net/projects/launch4j/files/launch4j-3/3.14/launch4j-3.14-linux-x64.tgz/download
#RUN (cd "$WINEPREFIX/drive_c/Program Files"; unzip /tmp/launch4j-3.14-win32)
RUN    (cd /usr/local; tar xf /tmp/launch4j-3.14-linux-x64.tgz) \
    && rm -f /tmp/launch4j-3.14-win32

# create Windows tools archive
RUN (cd $WINEPREFIX/drive_c; \
     tar cjf /windows-tools.tar.bz2 \
       "Program Files/Inno Setup 5" \
    )
#       "Program Files/jdk-20.0.2" \
#       "Program Files/launch4j"; \

#RUN find /root -type d -print0 | xargs -0 chmod a+rwx
#RUN find /root -type f -print0 | xargs -0 chmod a+rw
#RUN install -d /home/build; cp -r /root/.wine /home/build; chown -R build:build /home/build/.wine

# clean-up wine
RUN rm -rf $WINEPREFIX

# add external third-party packages
COPY download-third-party-packages.sh /root
RUN    /root/download-third-party-packages.sh --no-decompress --destination-directory /media/extern \
    && rm /root/download-third-party-packages.sh

# mount /media/home
RUN mkdir /media/home && chown root /media/home
VOLUME [ "/media/home" ]

# mounts
RUN install -d /media/home && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
