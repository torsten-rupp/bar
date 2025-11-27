FROM ubuntu:24.04

# variables
ARG uid=1000
ARG gid=1000

# set marker in environment for running inside a docker
ENV CONTAINER=docker

# disable interactive installion
ENV DEBIAN_FRONTEND=noninteractive

# clear APT cache
RUN    apt clean \
    && rm -rf /var/lib/apt/lists/*

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
  faketime \
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
  postgresql-16 \
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
  libblkid-dev \
  libc6 \
  libc6-dev \
  libpq-dev \
  libinih-dev \
  libsystemd-dev \
  libcap-dev \
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

# add users for build+test process
RUN    userdel `id -un $uid 2>/dev/null` 2>/dev/null || true \
    && groupadd -g $gid jenkins || true \
    && useradd -g $gid -u $uid -m -p `openssl passwd jenkins` jenkins \
    && useradd -m -p `openssl passwd test` test

# enable sudo for jenkins+test
#RUN echo "ALL ALL = (ALL) NOPASSWD: ALL" > /etc/sudoers.d/all
COPY bar-sudoers /etc/sudoers.d/bar
RUN    usermod -aG sudo jenkins \
    && usermod -aG sudo test

# enable ftp: fix race-condition in vsftpd start script, enable write
RUN    sed '/^\s*start-stop-daemon.*/a sleep 3' -i /etc/init.d/vsftpd \
    && sed 's|#write_enable=YES|write_enable=YES|g' -i /etc/vsftpd.conf \
    && sed 's|#xferlog_file=/var/log/vsftpd.log|xferlog_file=/var/log/vsftpd.log|g' -i /etc/vsftpd.conf

# enable ssh: create keys, permit key login, max. number of concurrent connection attempts, enable logging
RUN    mkdir /home/test/.ssh \
    && ssh-keygen -b 2048 -t rsa -f /home/test/.ssh/id_rsa -q -N "" \
    && cat /home/test/.ssh/id_rsa.pub > /home/test/.ssh/authorized_keys \
    && chmod 755 /home/test \
    && chmod 700 /home/test/.ssh \
    && chown -R test:test /home/test/.ssh \
    && chmod 644 /home/test/.ssh/* \
    && sed '/#MaxStartups/a MaxStartups 256' -i /etc/ssh/sshd_config
COPY test-rsyslog-sshd.conf /etc/rsyslog.d/sshd.conf

# enable webDAV/webDAVs
RUN    a2enmod ssl \
    && a2enmod rewrite \
    && a2enmod dav \
    && a2enmod dav_fs \
    && a2ensite 000-default \
    && a2ensite default-ssl \
    && echo ServerName test >> /etc/apache2/apache2.conf
COPY apache2-test.conf /etc/apache2/sites-enabled/test.conf
RUN    sed 's|DocumentRoot /var/www/.*|DocumentRoot /var/www|g' -i /etc/apache2/sites-available/000-default.conf \
    && sed 's|DocumentRoot /var/www/.*|DocumentRoot /var/www|g' -i /etc/apache2/sites-available/default-ssl.conf \
    && sed 's|export APACHE_RUN_USER=.*|export APACHE_RUN_USER=test|g' -i /etc/apache2/envvars \
    && sed 's|export APACHE_RUN_GROUP=.*|export APACHE_RUN_GROUP=test|g' -i /etc/apache2/envvars \
    && chown -R test:test /var/www

# enable samba
RUN   ( \
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
      ) >> /etc/samba/smb.conf \
   && ( \
        echo "jenkins = jenkins"; \
        echo "test = test"; \
      ) > /etc/samba/smbusers \
   && usermod -aG sambashare jenkins \
   && usermod -aG sambashare test \
   && sh -c '(echo jenkins; echo jenkins)|smbpasswd -a jenkins' \
   && sh -c '(echo test; echo test)|smbpasswd -a test'

# add external third-party packages
COPY download-third-party-packages.sh /root
RUN    /root/download-third-party-packages.sh --no-decompress --destination-directory /media/extern \
    && rm -f /root/download-third-party-packages.sh

# default command
CMD "/bin/bash"
