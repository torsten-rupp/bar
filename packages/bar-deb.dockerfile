FROM debian:11
ENV container docker

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

RUN apt-get -y install \
  initscripts \
  openssl \
  default-jre \
  ;

RUN apt-get -y install \
  bc \
  coreutils \
  joe \
  less \
  lua5.1 \
  rsync \
  socat \
  sudo \
  ;

# install packages (Note: ignore expired key with --force-yes)
RUN apt-get -y install \
  autotools-dev \
  bison \
  bzip2 \
  cmake \
  curl \
  debhelper \
  default-jdk \
  devscripts \
  dh-autoreconf \
  e2fsprogs \
  flex \
  g++ \
  gcc \
  gettext \
  git \
  libblkid-dev \
  libc6 \
  libc6-dev \
  libinih-dev \
  m4 \
  make \
  mariadb-client \
  patch \
  pkg-config \
  postgresql \
  sqlite3 \
  subversion \
  tar \
  tcl \
  txt2man \
  unoconv \
  unzip \
  valgrind \
  wget \
  xz-utils \
  ;

# install autoconf 2.7x (only available from Debian > 11)
RUN    cd /tmp \
    && wget https://ftpmirror.gnu.org/gnu/autoconf/autoconf-2.72.tar.xz \
         --no-check-certificate \
         --quiet \
         --output-document autoconf-2.72.tar.xz \
    && tar xf autoconf-2.72.tar.xz \
    && (cd autoconf-2.72; ./configure) \
    && (cd autoconf-2.72; make) \
    && (cd autoconf-2.72; make install) \
    && rm -rf autoconf-2.72 autoconf-2.72.tar.xz
ENV PATH=/usr/local/bin:$PATH

# add user for build process
RUN    userdel `id -un $uid 2>/dev/null` 2>/dev/null || true \
    && groupadd -g $gid jenkins || true \
    && useradd -g $gid -u $uid jenkins -m

# enable sudo for all
RUN echo "ALL ALL = (ALL) NOPASSWD: ALL" > /etc/sudoers.d/all

# add external third-party packages
COPY download-third-party-packages.sh /root
RUN    /root/download-third-party-packages.sh --no-decompress --destination-directory /media/extern \
    && rm -f /root/download-third-party-packages.sh

# mounts
RUN    install -d /media/home \
    && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
