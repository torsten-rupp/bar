FROM i386/debian:8
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
  systemd \
  ;
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
  make \
  unoconv \
  txt2man \
  ;

# fix systemd
RUN (cd /lib/systemd/system/sysinit.target.wants/; \
     for i in *; do [ $i == systemd-tmpfiles-setup.service ] || rm -f $i; \
     done; \
    ); \
    rm -f /lib/systemd/system/multi-user.target.wants/*;\
    rm -f /etc/systemd/system/*.wants/*;\
    rm -f /lib/systemd/system/local-fs.target.wants/*; \
    rm -f /lib/systemd/system/sockets.target.wants/*udev*; \
    rm -f /lib/systemd/system/sockets.target.wants/*initctl*; \
    rm -f /lib/systemd/system/basic.target.wants/*;\
    rm -f /lib/systemd/system/anaconda.target.wants/*;
VOLUME [ "/sys/fs/cgroup" ]

# mount /media/home
RUN mkdir /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
