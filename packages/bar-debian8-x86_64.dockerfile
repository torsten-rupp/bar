FROM debian:8
ENV container docker

# add user for build process
RUN groupadd -g 1000 build
RUN useradd -g 1000 -u 1000 build

# disable interactive installion
ENV DEBIAN_FRONTEND noninteractive

# update
RUN apt-get -y update

# install packages (Note: ignore expired key with --force-yes)
RUN apt-get -y --force-yes install \
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
  lua5.1 \
  m4 \
  mysql-client \
  patch \
  pkg-config \
  postgresql \
  rsync \
  sqlite3 \
  subversion \
  sudo \
  tar \
  tcl \
  unzip \
  wget \
  xz-utils \
  ;
RUN apt-get -y --force-yes install \
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
  bison \
  flex \
  unoconv \
  txt2man \
  valgrind \
  ;

# add external third-party packages
COPY download-third-party-packages.sh /root
RUN /root/download-third-party-packages.sh --no-decompress --destination-directory /media/extern
RUN rm -f /root/download-third-party-packages.sh

# mounts
RUN install -d /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
