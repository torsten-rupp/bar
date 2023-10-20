FROM debian:10
ENV container docker

ARG uid=0
ARG gid=0

# disable interactive installion
ENV DEBIAN_FRONTEND noninteractive

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
  autoconf \
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
  libc6 \
  libc6-dev \
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

# add users
RUN groupadd -g 1000 build
RUN useradd -g 1000 -u 1000 build
RUN groupadd -g ${gid} jenkins
RUN useradd -m -u ${uid} -g ${gid} -p `openssl passwd jenkins` jenkins

# enable sudo for all
RUN echo "ALL ALL = (ALL) NOPASSWD: ALL" > /etc/sudoers.d/all

# add external third-party packages
COPY download-third-party-packages.sh /root
RUN /root/download-third-party-packages.sh --no-decompress --destination-directory /media/extern
RUN rm -f /root/download-third-party-packages.sh

# mounts
RUN install -d /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
