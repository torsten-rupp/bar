FROM centos:6
ENV container docker

# add user for build process
RUN groupadd -g 1000 build
RUN useradd -g 1000 -u 1000 build

# install packages
RUN yum -y install \
  bc \
  bzip2 \
  curl \
  e2fsprogs \
  gettext \
  initscripts \
  lua \
  m4 \
  openssl \
  patch \
  pkg-config \
  psmisc \
  rpm-build \
  sudo \
  subversion \
  sudo \
  tar \
  tcl \
  unzip \
  wget \
  xz \
  ;
# Note: no valgrind available
RUN yum -y install \
  gcc \
  gcc-c++ \
  java-1.6.0-openjdk-devel \
  jre \
  make \
  rpm-build \
  ;

# mount /media/home
RUN mkdir /media/home  && chown root /media/home
VOLUME [ "/media/home" ]

CMD ["/usr/sbin/init"]
