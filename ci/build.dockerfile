FROM ubuntu:22.04

ARG uid=0
ARG gid=0

# set marker in environment for running inside a docker
ENV CONTAINER=docker

# disable interactive installion
ENV DEBIAN_FRONTEND=noninteractive

# update
RUN apt-get -y update --fix-missing

# install packages
RUN apt-get -y --fix-missing install \
  attr \
  bc \
  bzip2 \
  curl \
  debhelper \
  devscripts \
  dosfstools \
  e2fsprogs \
  gettext \
  git \
  imagemagick \
  joe \
  less \
  lua5.3 \
  m4 \
  mariadb-client \
  ncurses-bin \
  patch \
  pkg-config \
  postgresql-14 \
  psmisc \
  reiserfsprogs \
  rsync \
  rsyslog \
  samba \
  socat \
  software-properties-common \
  sqlite3 \
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
RUN apt-get -y --fix-missing install \
  gcc \
  g++ \
  libc6 \
  libc6-dev \
  libpq-dev \
  libsystemd-dev \
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
RUN apt-get update --fix-missing
RUN apt-get -y --fix-missing install \
  apache2 \
  openssh-server \
  vsftpd \
  dvdisaster \
  genisoimage \
  growisofs \
  ;

RUN mkdir /.cache;

# add users
RUN groupadd -g ${gid} jenkins
RUN useradd -m -u ${uid} -g ${gid} -p `openssl passwd jenkins` jenkins
RUN useradd -m -p `openssl passwd test` test

# enable sudo for jenkins+test
#RUN echo "ALL ALL = (ALL) NOPASSWD: ALL" > /etc/sudoers.d/all
COPY bar-sudoers /etc/sudoers.d/bar
RUN usermod -aG sudo jenkins
RUN usermod -aG sudo test

# enable ftp: fix race-condition in vsftpd start script, enable write
RUN sed '/^\s*start-stop-daemon.*/a sleep 3' -i /etc/init.d/vsftpd
RUN sed 's|#write_enable=YES|write_enable=YES|g' -i /etc/vsftpd.conf
RUN sed 's|#xferlog_file=/var/log/vsftpd.log|xferlog_file=/var/log/vsftpd.log|g' -i /etc/vsftpd.conf

# enable ssh: create keys, permit key login, max. number of concurrent connection attempts, enable logging
RUN mkdir /home/test/.ssh
RUN ssh-keygen -b 2048 -t rsa -f /home/test/.ssh/id_rsa -q -N ""
RUN cat /home/test/.ssh/id_rsa.pub > /home/test/.ssh/authorized_keys
RUN chmod 755 /home/test
RUN chmod 700 /home/test/.ssh
RUN chown -R test:test /home/test/.ssh
RUN chmod 644 /home/test/.ssh/*
RUN sed '/#MaxStartups/a MaxStartups 256' -i /etc/ssh/sshd_config
COPY test-rsyslog-sshd.conf /etc/rsyslog.d/sshd.conf

# enable webDAV/webDAVs
RUN a2enmod ssl
RUN a2enmod rewrite
RUN a2enmod dav
RUN a2enmod dav_fs
RUN a2ensite 000-default
RUN a2ensite default-ssl
RUN echo ServerName test >> /etc/apache2/apache2.conf
COPY apache2-test.conf /etc/apache2/sites-enabled/test.conf
RUN sed 's|DocumentRoot /var/www/.*|DocumentRoot /var/www|g' -i /etc/apache2/sites-available/000-default.conf
RUN sed 's|DocumentRoot /var/www/.*|DocumentRoot /var/www|g' -i /etc/apache2/sites-available/default-ssl.conf
RUN sed 's|export APACHE_RUN_USER=.*|export APACHE_RUN_USER=test|g' -i /etc/apache2/envvars
RUN sed 's|export APACHE_RUN_GROUP=.*|export APACHE_RUN_GROUP=test|g' -i /etc/apache2/envvars
RUN chown -R test:test /var/www

# enable samba
RUN \
( \
  echo "jenkins = jenkins"; \
  echo "test = test"; \
  echo "[global]"; \
  echo "  passdb backend = smbpasswd"; \
  echo ""; \
  echo "[test]"; \
  echo "  path = /home/test"; \
  echo "  writeable = yes"; \
  echo "  browseable = yes"; \
  echo "  valid users = test"; \
  echo ""; \
  echo "[jenkins]"; \
  echo "  path = /home/jenkins"; \
  echo "  writeable = yes"; \
  echo "  browseable = yes"; \
  echo "  valid users = jenkins"; \
) >> /etc/samba/smb.conf
RUN \
( \
  echo "jenkins = jenkins"; \
  echo "test = test"; \
) > /etc/samba/smbusers
RUN usermod -aG sambashare jenkins
RUN usermod -aG sambashare test
RUN sh -c '(echo jenkins; echo jenkins)|smbpasswd -a jenkins'
RUN sh -c '(echo test; echo test)|smbpasswd -a test'

# add external third-party packages
COPY download-third-party-packages.sh /root
RUN /root/download-third-party-packages.sh --no-decompress --destination-directory /media/extern
RUN rm -f /root/download-third-party-packages.sh
