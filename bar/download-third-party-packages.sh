#!/bin/sh

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/download-third-party-packages.sh,v $
# $Revision: 1.18 $
# $Author: torsten $
# Contents: download third-party packages
# Systems: Unix
#
# ----------------------------------------------------------------------------

# --------------------------------- constants --------------------------------

ECHO="echo"
LN="ln"
MKDIR="mkdir"
RMF="rm -f"
RMRF="rm -rf"
TAR="tar"
WGET="wget"

# --------------------------------- variables --------------------------------

# ---------------------------------- functions -------------------------------

# ------------------------------------ main ----------------------------------

# parse arguments
allFlag=1
zlibFlag=0
bzip2Flag=0
lzmaFlag=0
xdeltaFlag=0
gcryptFlag=0
ftplibFlag=0
opensslFlag=0
libssh2Flag=0
gnutlsFlag=0
libcdioFlag=0
epmFlag=0
destination=""
noDecompressFlag=0

helpFlag=0
cleanFlag=0
while test $# != 0; do
  case $1 in
    -h | --help)
      helpFlag=1
      shift
      ;;
    -d | --destination)
      destination=$2
      shift
      shift
      ;;
    -n | --no-decompress)
      noDecompressFlag=1
      shift
      ;;
    -c | --clean)
      cleanFlag=1
      shift
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
        xdelta)
          allFlag=0
          xdeltaFlag=1
          ;;
        gcrypt)
          allFlag=0
          gcryptFlag=1
          ;;
        ftplib)
          allFlag=0
          ftplibFlag=1
          ;;
        openssl)
          allFlag=0
          opensslFlag=1
          ;;
        ssh2|libssh2)
          allFlag=0
          libssh2Flag=1
          ;;
        gnutls)
          allFlag=0
          gnutlsFlag=1
          ;;
        cdio|libcdio)
          allFlag=0
          libcdioFlag=1
          ;;
        epm)
          allFlag=0
          epmFlag=1
          ;;
        *)
          $ECHO >&2 "ERROR: unknown package '$1'"
          exit 1
          ;;
      esac
      shift
      ;;
  esac
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
    xdelta)
      allFlag=0
      xdeltaFlag=1
      ;;
    gcrypt)
      allFlag=0
      gcryptFlag=1
      ;;
    ftplib)
      allFlag=0
      ftplibFlag=1
      ;;
    openssl)
      allFlag=0
      opensslFlag=1
      ;;
    ssh2|libssh2)
      allFlag=0
      libssh2Flag=1
      ;;
    gnutls)
      allFlag=0
      gnutlsFlag=1
      ;;
    cdio|libcdio)
      allFlag=0
      libcdioFlag=1
      ;;
    epm)
      allFlag=0
      epmFlag=1
      ;;
    *)
      $ECHO >&2 "ERROR: unknown package '$1'"
      exit 1
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  $ECHO "download-third-party-packages.sh [-d|--destination=<path>] [-n|--no-decompress] [-c|--clean] [--help] [all] [zlib] [bzip2] [lzma] [xdelta] [gcrypt] [ftplib] [openssl] [libssh2] [gnutls] [libcdio] [epm]"
  $ECHO ""
  $ECHO "Download additional third party packages."
  exit 0
fi

# check if wget is available
type wget 1>/dev/null 2>/dev/null && wget --version 1>/dev/null 2>/dev/null
if test $? -ne 0; then
  $ECHO >&2 "ERROR: command wget is not available"
  exit 1
fi

# run
tmpDirectory="packages"
cwd=`pwd`
if test $cleanFlag -eq 0; then
  # download
  $MKDIR $tmpDirectory 2>/dev/null

  if test $allFlag -eq 1 -o $zlibFlag -eq 1; then
    # zlib
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     fileName=`ls zlib-*.tar.gz 2>/dev/null`
     if test ! -f "$fileName"; then
       fileName=`wget --quiet -O - 'http://www.zlib.net'|grep -E -e 'http://zlib.net/zlib-.*\.tar\.gz'|head -1|sed 's|.*http://zlib.net/\(.*\.tar\.gz\)".*|\1|g'`
       $WGET "http://www.zlib.net/$fileName"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s `find $tmpDirectory -type d -name "zlib-*"` zlib
    fi
  fi

  if test $allFlag -eq 1 -o $bzip2Flag -eq 1; then
    # bzip2 1.0.5
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f bzip2-1.0.5.tar.gz; then
       $WGET 'http://www.bzip.org/1.0.5/bzip2-1.0.5.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf bzip2-1.0.5.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/bzip2-1.0.5 bzip2
    fi
  fi

  if test $allFlag -eq 1 -o $lzmaFlag -eq 1; then
    # lzma
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     fileName=`ls xz-*.tar.gz 2>/dev/null`
     if test ! -f "$fileName"; then
       fileName=`wget --quiet -O - 'http://tukaani.org/xz'|grep -E -e 'xz-.*\.tar\.gz'|head -1|sed 's|.*href="\(xz.*\.tar\.gz\)".*|\1|g'`
       $WGET "http://tukaani.org/xz/$fileName"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s `find $tmpDirectory -type d -name "xz-*"` xz
    fi
  fi

  if test $allFlag -eq 1 -o $xdeltaFlag -eq 1; then
    # xdelta
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f xdelta3.0.0.tar.gz; then
       $WGET 'http://xdelta.googlecode.com/files/xdelta3.0.0.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf xdelta3.0.0.tar.gz
       (cd xdelta3.0.0; patch -p1 < ../../misc/xdelta3.0.patch)
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s `find $tmpDirectory -type d -name "xdelta3*"` xdelta3
    fi
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    # gpg-error 1.7, gcrypt 1.4.4
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f libgpg-error-1.7.tar.bz2; then
       $WGET 'ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-1.7.tar.bz2'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf libgpg-error-1.7.tar.bz2
     fi
     if test ! -f libgcrypt-1.4.4.tar.bz2; then
       $WGET 'ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-1.4.4.tar.bz2'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf libgcrypt-1.4.4.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/libgpg-error-1.7 libgpg-error
      $LN -f -s $tmpDirectory/libgcrypt-1.4.4 libgcrypt
    fi
  fi

  if test $allFlag -eq 1 -o $ftplibFlag -eq 1; then
    # ftplib 3.1
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f ftplib-3.1-src.tar.gz; then
       $WGET 'http://www.nbpfaus.net/~pfau/ftplib/ftplib-3.1-src.tar.gz'
     fi
     if test ! -f ftplib-3.1-1.patch; then
       $WGET 'http://nbpfaus.net/~pfau/ftplib/ftplib-3.1-1.patch'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf ftplib-3.1-src.tar.gz
       (cd ftplib-3.1; patch -p3 < ../ftplib-3.1-1.patch) 1>/dev/null 2>/dev/null
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/ftplib-3.1 ftplib
    fi
  fi

  if test $allFlag -eq 1 -o $opensslFlag -eq 1; then
    # openssl 1.0.1c
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f openssl-1.0.1c.tar.gz; then
       $WGET 'http://www.openssl.org/source/openssl-1.0.1c.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf openssl-1.0.1c.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/openssl-1.0.1c openssl
    fi
  fi

  if test $allFlag -eq 1 -o $libssh2Flag -eq 1; then
    # libssh2 1.4.2
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f libssh2-1.4.2.tar.gz; then
       $WGET 'http://www.libssh2.org/download/libssh2-1.4.2.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf libssh2-1.4.2.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/libssh2-1.4.2 libssh2
    fi
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    # gnutls 2.10.2
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f gnutls-2.10.2.tar.bz2; then
       $WGET 'ftp://ftp.gnu.org/pub/gnu/gnutls/gnutls-2.10.2.tar.bz2'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf gnutls-2.10.2.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/gnutls-2.10.2 gnutls
    fi
  fi

  if test $allFlag -eq 1 -o $libcdioFlag -eq 1; then
    # libcdio 0.82
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi

     if test ! -f libcdio-0.82.tar.gz; then
       $WGET 'ftp://ftp.gnu.org/gnu/libcdio/libcdio-0.82.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf libcdio-0.82.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/libcdio-0.82 libcdio
    fi
  fi

  if test $allFlag -eq 1 -o $epmFlag -eq 1; then
    # epm 4.1
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f epm-4.1-source.tar.bz2; then
       $WGET 'http://ftp.easysw.com/pub/epm/4.1/epm-4.1-source.tar.bz2'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf epm-4.1-source.tar.bz2
       (cd epm-4.1; patch -p1 < ../../misc/epm-4.1-rpm.patch)
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/epm-4.1 epm
    fi
  fi
else
  # clean

  if test $allFlag -eq 1 -o $zlibFlag -eq 1; then
    # zlib
    $RMF $tmpDirectory/zlib-*.tar.gz
    $RMRF $tmpDirectory/zlib-*
    $RMF zlib
  fi

  if test $allFlag -eq 1 -o $bzip2Flag -eq 1; then
    # bzip2
    $RMF $tmpDirectory/bzip2-*.tar.gz
    $RMRF $tmpDirectory/bzip2-*
    $RMF bzip2
  fi

  if test $allFlag -eq 1 -o $lzmaFlag -eq 1; then
    # lzma
    $RMF `find $tmpDirectory -type f -name "xz-*.tar.gz" 2>/dev/null`
    $RMRF `find $tmpDirectory -type d -name "xz-*" 2>/dev/null`
    $RMF xz
  fi

  if test $allFlag -eq 1 -o $xdeltaFlag -eq 1; then
    # xdelta
    $RMF `find $tmpDirectory -type f -name "xdelta3*.tar.gz" 2>/dev/null`
    $RMRF `find $tmpDirectory -type d -name "xdelta3*" 2>/dev/null`
    $RMF xdelta
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    # gcrypt
    $RMF $tmpDirectory/libgpg-error-*.tar.bz2 $tmpDirectory/libgcrypt-*.tar.bz2
    $RMRF $tmpDirectory/libgpg-error-* $tmpDirectory/libgcrypt-*
    $RMF libgpg-error libgcrypt
  fi

  if test $allFlag -eq 1 -o $ftplibFlag -eq 1; then
    # ftplib
    $RMF $tmpDirectory/ftplib-*-src.tar.gz $tmpDirectory/ftplib-*.patch
    $RMRF $tmpDirectory/ftplib-*
    $RMF ftplib
  fi

  if test $allFlag -eq 1 -o $opensslFlag -eq 1; then
    # openssl
    $RMF $tmpDirectory/openssl*.tar.gz
    $RMRF $tmpDirectory/openssl*
    $RMF openssl
  fi

  if test $allFlag -eq 1 -o $libssh2Flag -eq 1; then
    # libssh2
    $RMF $tmpDirectory/libssh2*.tar.gz
    $RMRF $tmpDirectory/libssh2*
    $RMF libssh2
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    # gnutls
    $RMF $tmpDirectory/gnutls-*.tar.bz2
    $RMRF $tmpDirectory/gnutls-*
    $RMF gnutls
  fi

  if test $allFlag -eq 1 -o $libcdioFlag -eq 1; then
    # gnutls
    $RMF $tmpDirectory/libcdio-*.tar.gz
    $RMRF $tmpDirectory/libcdio-*
    $RMF libcdio
  fi

  if test $allFlag -eq 1 -o $epmFlag -eq 1; then
    # epm
    $RMF $tmpDirectory/epm-*.tar.bz2
    $RMRF $tmpDirectory/epm-*
    $RMF epm
  fi
fi

exit 0
