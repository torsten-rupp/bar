# require Debian >= 13
FROM debian:13
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

RUN apt-get -y install --no-install-recommends --fix-missing \
  initscripts \
  openssl \
  default-jre \
  ;

RUN apt-get -y install --no-install-recommends --fix-missing \
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
RUN apt-get -y install --no-install-recommends --fix-missing \
  autoconf \
  automake \
  autotools-dev \
  bison \
  build-essential \
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
  libcap-dev \
  libinih-dev \
  libsystemd-dev \
  m4 \
  make \
  mariadb-client \
  patch \
  pkg-config \
  postgresql \
  psmisc \
  sqlite3 \
  subversion \
  tar \
  tcl \
  txt2man \
  unzip \
  valgrind \
  wget \
  xz-utils \
  ;

# install packages for dynamic linkage
RUN apt-get -y install --no-install-recommends --fix-missing \
  libiberty-dev \
  libicu-dev \
  zlib1g-dev \
  libz-dev \
  liblzo2-dev \
  liblz4-dev \
  liblzma-dev \
  libbz2-dev \
  libzstd-dev \
  libssl-dev \
  libtls-dev \
  nettle-dev \
  libgmp-dev \
  libcurl4-openssl-dev \
  libc-ares-dev \
  libidn2-dev \
  libgcrypt20-dev \
  libssh2-1-dev \
  libmount-dev \
  libburn-dev \
  libiso9660-dev \
  libisofs-dev \
  libcdio-dev \
  libsmbclient-dev \
  libsmb2-dev \
  libkrb5-dev \
  libsqlite3-dev \
  libmariadb-dev \
  libpq-dev \
  uuid-dev \
  libblkid-dev \
  liburcu-dev \
  libfrog-dev \
  binutils-dev \
  ;

RUN apt-get install -y --no-install-recommends --fix-missing \
  libreoffice \
  python3-pip \
  python3-uno \
  ;

# install autoconf 2.7x (only available from Debian > 11)
#RUN    cd /tmp \
#    && wget https://ftpmirror.gnu.org/gnu/autoconf/autoconf-2.72.tar.xz \
#         --no-check-certificate \
#         --quiet \
#         --output-document autoconf-2.72.tar.xz \
#    && tar xf autoconf-2.72.tar.xz \
#    && (cd autoconf-2.72; ./configure) \
#    && (cd autoconf-2.72; make) \
#    && (cd autoconf-2.72; make install) \
#    && rm -rf autoconf-2.72 autoconf-2.72.tar.xz
#ENV PATH=/usr/local/bin:$PATH

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
