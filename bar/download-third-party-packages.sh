#!/bin/sh

ECHO="echo"
LN="ln"
MKDIR="mkdir"
RMF="rm -f"
RMRF="rm -rf"
TAR="tar"
WGET="wget"

# parse arguments
allFlag=1
zlibFlag=0
bzip2Flag=0
lzmaFlag=0
gcryptFlag=0
ftplibFlag=0
libssh2Flag=0
gnutlsFlag=0

helpFlag=0
cleanFlag=0
while test $# != 0; do
  case $1 in
    -h | --help)
      helpFlag=1
      ;;
    --clean)
      cleanFlag=1
      ;;
    --)
      shift
      break
      ;;
    -*)
      $ECHO >&2 "ERROR: unknown option '$1'"
      exit 1
      ;;
    *)
      case $1 in
        all)
          allFlag=1
         ;;
        zlib)
         allFlag=0
         zlibFlag=1
         ;;
        bzip2)
         allFlag=0
         bzip2Flag=1
         ;;
        lzma)
         allFlag=0
         lzmaFlag=1
         ;;
        gcrypt)
         allFlag=0
         gcryptFlag=1
         ;;
        ftplib)
         allFlag=0
         ftplibFlag=1
         ;;
        libssh2)
         allFlag=0
         libssh2Flag=1
         ;;
        gnutls)
         allFlag=0
         gnutlsFlag=1
         ;;
       *)
         $ECHO >&2 "ERROR: unknown package '$1'"
         exit 1
         ;;
      esac
      ;;
  esac
  shift
done
while test $# != 0; do
   case $1 in
     all)
       allFlag=1
      ;;
     zlib)
      allFlag=0
      zlibFlag=1
      ;;
     bzip2)
      allFlag=0
      bzip2Flag=1
      ;;
     lzma)
      allFlag=0
      lzmaFlag=1
      ;;
     gcrypt)
      allFlag=0
      gcryptFlag=1
      ;;
     ftplib)
      allFlag=0
      ftplibFlag=1
      ;;
     libssh2)
      allFlag=0
      libssh2Flag=1
      ;;
     gnutls)
      allFlag=0
      gnutlsFlag=1
      ;;
    *)
      $ECHO >&2 "ERROR: unknown package '$1'"
      exit 1
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  $ECHO "download-third-party-packages.sh [--clean] [--help] [all] [zlib] [bzip2] [lzma] [gcrypt] [ftplib] [libssh2] [gnutls]"
  $ECHO ""
  $ECHO "Download additional third party packages."
  exit 0
fi

# run
tmpDirectory="packages"
cwd=`pwd`
if test $cleanFlag -eq 0; then
  $MKDIR $tmpDirectory 2>/dev/null

  if test $allFlag -eq 1 -o $zlibFlag -eq 1; then
    # zlib 1.2.3
    (
     cd $tmpDirectory
     if test ! -f zlib-1.2.3.tar.gz; then
       $WGET 'http://www.zlib.net/zlib-1.2.3.tar.gz'
     fi
     $TAR xzf zlib-1.2.3.tar.gz
    )
    $LN -f -s $tmpDirectory/zlib-1.2.3 zlib
  fi

  if test $allFlag -eq 1 -o $bzip2Flag -eq 1; then
    # bzip2 1.0.5
    (
     cd $tmpDirectory
     if test ! -f bzip2-1.0.5.tar.gz; then
       $WGET 'http://www.bzip.org/1.0.5/bzip2-1.0.5.tar.gz'
     fi
     $TAR xzf bzip2-1.0.5.tar.gz
    )
    $LN -f -s $tmpDirectory/bzip2-1.0.5 bzip2
  fi

  if test $allFlag -eq 1 -o $lzmaFlag -eq 1; then
    # lzma
    (
     cd $tmpDirectory
     fileName=`ls xz-*.tar.gz 2>/dev/null`
     if test ! -f "$fileName"; then
       fileName=`wget --quiet -O - 'http://tukaani.org/xz'|grep -E -e 'xz-.*\.tar\.gz'|sed 's|.*href="\(xz.*\.tar\.gz\)".*|\1|g'`     
       $WGET "http://tukaani.org/xz/$fileName"
     fi
     $TAR xzf $fileName
    )
    $LN -f -s `find $tmpDirectory -type d -name "xz-*"` xz
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    # gpg-error 1.7, gcrypt 1.4.4
    (
     cd $tmpDirectory
     if test ! -f libgpg-error-1.7.tar.bz2; then
       $WGET 'ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-1.7.tar.bz2'
     fi
     $TAR xjf libgpg-error-1.7.tar.bz2
     if test ! -f libgcrypt-1.4.4.tar.bz2; then
       $WGET 'ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-1.4.4.tar.bz2'
     fi
     $TAR xjf libgcrypt-1.4.4.tar.bz2
    )
    $LN -f -s $tmpDirectory/libgpg-error-1.7 libgpg-error
    $LN -f -s $tmpDirectory/libgcrypt-1.4.4 libgcrypt
  fi

  if test $allFlag -eq 1 -o $ftplibFlag -eq 1; then
    # ftplib 3.1
    (
     cd $tmpDirectory
     if test ! -f ftplib-3.1-src.tar.gz; then
       $WGET 'http://www.nbpfaus.net/~pfau/ftplib/ftplib-3.1-src.tar.gz'
     fi
     if test ! -f ftplib-3.1-1.patch; then
       $WGET 'http://nbpfaus.net/~pfau/ftplib/ftplib-3.1-1.patch'
     fi
     $TAR xzf ftplib-3.1-src.tar.gz
     (cd ftplib-3.1; patch -p3 < ../ftplib-3.1-1.patch)
    )
    $LN -f -s $tmpDirectory/ftplib-3.1 ftplib
  fi

  if test $allFlag -eq 1 -o $libssh2Flag -eq 1; then
    # libssh2 1.2.4
    (
     cd $tmpDirectory
     if test ! -f libssh2-1.2.4.tar.gz; then
       $WGET 'http://www.libssh2.org/download/libssh2-1.2.4.tar.gz'
     fi
     $TAR xzf libssh2-1.2.4.tar.gz
    )
    $LN -f -s $tmpDirectory/libssh2-1.2.4 libssh2
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    # gnutls 2.8.5
    (
     cd $tmpDirectory
     if test ! -f gnutls-2.8.5.tar.bz2; then
       $WGET 'ftp://ftp.gnu.org/pub/gnu/gnutls/gnutls-2.8.5.tar.bz2'
     fi
     $TAR xjf gnutls-2.8.5.tar.bz2
    )
    $LN -f -s $tmpDirectory/gnutls-2.8.5 gnutls
  fi
else
  # bzip2
  $RMF $tmpDirectory/bzip2-*.tar.gz
  $RMRF $tmpDirectory/bzip2-*
  $RMF bzip2

  # lzma
  $RMF `find $tmpDirectory -type f -name "xz-*.tar.gz" 2>/dev/null`
  $RMRF `find $tmpDirectory -type d -name "xz-*" 2>/dev/null`
  $RMF xz

  # gcrypt
  $RMF $tmpDirectory/libgpg-error-*.tar.bz2 $tmpDirectory/libgcrypt-*.tar.bz2
  $RMRF $tmpDirectory/libgpg-error-* $tmpDirectory/libgcrypt-*
  $RMF libgpg-error libgcrypt

  # ftplib
  $RMF $tmpDirectory/ftplib-*-src.tar.gz $tmpDirectory/ftplib-*.patch
  $RMRF $tmpDirectory/ftplib-*
  $RMF ftplib

  # libssh2
  $RMF $tmpDirectory/libssh2*.tar.gz
  $RMRF $tmpDirectory/libssh2*
  $RMF libssh2

  # gnutls
  $RMF $tmpDirectory/gnutls-*.tar.bz2
  $RMRF $tmpDirectory/gnutls-*
  $RMF gnutls
fi

exit 0
