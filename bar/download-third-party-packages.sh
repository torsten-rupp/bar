#!/bin/sh

LN="ln"
RMF="rm -f"
RMRF="rm -rf"
TAR="tar"
WGET="wget"

tmpDirectory="tmp"

cwd=`pwd`

if test "$1" != "--clean"; then
  mkdir $tmpDirectory 2>/dev/null

  # bzip2
  (
   cd $tmpDirectory
   $WGET 'http://www.bzip.org/1.0.5/bzip2-1.0.5.tar.gz'
   $TAR xzf bzip2-1.0.5.tar.gz
  )
  $LN -f -s $tmpDirectory/bzip2-1.0.5 bzip2

  # lzma
  (
   cd $tmpDirectory
   $WGET 'http://tukaani.org/xz/xz-4.999.8beta.tar.gz'
   $TAR xzf xz-4.999.8beta.tar.gz
  )
  $LN -f -s $tmpDirectory/xz-4.999.8beta xz

  # gcrypt
  (
   cd $tmpDirectory
   $WGET 'ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-1.7.tar.bz2'
   $WGET 'ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-1.4.4.tar.bz2'
   $TAR xjf libgpg-error-1.7.tar.bz2
   $TAR xjf libgcrypt-1.4.4.tar.bz2
  )
  $LN -f -s $tmpDirectory/libgpg-error-1.7 libgpg-error
  $LN -f -s $tmpDirectory/libgcrypt-1.4.4 libgcrypt

  # ftplib
  (
   cd $tmpDirectory
   $WGET 'http://www.nbpfaus.net/~pfau/ftplib/ftplib-3.1-src.tar.gz'
   $WGET 'http://nbpfaus.net/~pfau/ftplib/ftplib-3.1-1.patch'
   $TAR xzf ftplib-3.1-src.tar.gz
   (cd ftplib-3.1; patch -p3 < ../ftplib-3.1-1.patch)
  )
  $LN -f -s $tmpDirectory/ftplib-3.1 ftplib

  # libssh2
  (
   cd $tmpDirectory
   $WGET 'http://prdownloads.sourceforge.net/libssh2/libssh2-1.1.tar.gz?download'
   $TAR xzf libssh2-1.1.tar.gz
  )
  $LN -f -s $tmpDirectory/libssh2-1.1 libssh2

  # gnutls
  (
   cd $tmpDirectory
   $WGET 'ftp://ftp.gnu.org/pub/gnu/gnutls/gnutls-2.8.1.tar.bz2'
   $TAR xzf gnutls-2.8.1.tar.bz2
  )
  $LN -f -s $tmpDirectory/gnutls-2.8.1 gnutls
else
  # bzip2
  $RMF $tmpDirectory/bzip2-1.0.5.tar.gz
  $RMRF $tmpDirectory/bzip2-1.0.5
  $RMF bzip2

  # lzma
  $RMF $tmpDirectory/xz-4.999.8beta.tar.gz
  $RMRF $tmpDirectory/xz-4.999.8beta
  $RMF xz

  # gcrypt
  $RMF $tmpDirectory/libgpg-error-1.7.tar.bz2 $tmpDirectory/libgcrypt-1.4.4.tar.bz2
  $RMRF $tmpDirectory/libgpg-error-1.7 $tmpDirectory/libgcrypt-1.4.4
  $RMF libgpg-error libgcrypt

  # ftplib
  $RMF $tmpDirectory/ftplib-3.1-src.tar.gz $tmpDirectory/ftplib-3.1-1.patch
  $RMRF $tmpDirectory/ftplib-3.1
  $RMF ftplib

  # libssh2
  $RMF $tmpDirectory/libssh2-1.1.tar.gz
  $RMRF $tmpDirectory/libssh2-1.1
  $RMF libssh2

  # gnutls
  $RMF $tmpDirectory/gnutls-2.8.1.tar.bz2
  $RMRF $tmpDirectory/gnutls-2.8.1
fi

exit 0
