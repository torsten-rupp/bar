FROM ubuntu:18.04

# set marker in environment for running inside a docker
ENV CONTAINER=docker

# disable interactive installion
ENV DEBIAN_FRONTEND=noninteractive

# update
RUN apt-get -y update

# install packages
RUN apt-get -y install \
  attr \
  bc \
  bzip2 \
  curl \
  debhelper \
  devscripts \
  e2fsprogs \
  gettext \
  git \
  imagemagick \
  joe \
  less \
  lua5.3 \
  m4 \
  mariadb-client \
  netcat \
  ncurses-bin \
  patch \
  pkg-config \
  postgresql \
  psmisc \
  rsync \
  subversion \
  sudo \
  tar \
  tcl \
  tmux \
  unzip \
  wget \
  xz-utils \
  zip \
  ;

# install packages for building
RUN apt-get -y install \
  gcc \
  g++ \
  libc6 \
  libc6-dev \
  libpq-dev \
  openjdk-8-jdk \
  openjdk-8-jre \
  cmake \
  make \
  bison \
  flex \
  unoconv \
  txt2man \
  valgrind \
  lcov \
  binutils \
  uuid-dev \
  ;

# install packages for tests
RUN apt-get -y install \
  apache2 \
  openssh-server \
  vsftpd \
  ;

RUN mkdir /.cache;

# add test user
RUN useradd -m test -p `openssl passwd -crypt test`;

# enable sudo for test user
RUN echo "ALL ALL = (ALL) NOPASSWD: ALL" > /etc/sudoers.d/test
COPY test-sudoers /etc/suduers.d/test
RUN usermod -aG sudo test

# enable ftp: fix race-condition in vsftpd start script, enable write
RUN sed '/^\s*start-stop-daemon.*/a sleep 3' -i /etc/init.d/vsftpd
RUN sed 's/#write_enable=YES/write_enable=YES/g' -i /etc/vsftpd.conf
RUN sed 's/#xferlog_enable=YES/xferlog_enable=YES/g' -i /etc/vsftpd.conf

# enable ssh: create keys, permit key login, max. number of concurrent connection attempts
RUN mkdir /home/test/.ssh
RUN ssh-keygen -b 2048 -t rsa -f /home/test/.ssh/id_rsa -q -N ""
RUN cat /home/test/.ssh/id_rsa.pub > /home/test/.ssh/authorized_keys
RUN chmod 755 /home/test
RUN chmod 700 /home/test/.ssh
RUN chown -R test:test /home/test/.ssh
RUN chmod 644 /home/test/.ssh/*
RUN sed '/#MaxStartups/a MaxStartups 256' -i /etc/ssh/sshd_config

# enable webDAV/webDAVs
RUN a2enmod ssl
RUN a2enmod rewrite
RUN a2enmod dav
RUN a2enmod dav_fs
RUN a2ensite 000-default
RUN a2ensite default-ssl
RUN echo ServerName test >> /etc/apache2/apache2.conf
COPY test-apache2.conf /etc/apache2/sites-enabled/test.conf
RUN sed 's/export APACHE_RUN_USER=.*/export APACHE_RUN_USER=test/g' -i /etc/apache2/envvars
RUN sed 's/export APACHE_RUN_GROUP=.*/export APACHE_RUN_GROUP=test/g' -i /etc/apache2/envvars
RUN (cd /var/www; rm -rf html; ln -s /home/test html)
