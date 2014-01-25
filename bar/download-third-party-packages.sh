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
ECHO_NO_NEW_LINE="echo -n"
LN="ln"
MKDIR="mkdir"
PATCH="patch"
RMF="rm -f"
RMRF="rm -rf"
SVN="svn"
TAR="tar"
WGET="wget"
WGET_OPTIONS="--timeout=30 --tries=3"

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
curlFlag=0
mxmlFlag=0
opensslFlag=0
libssh2Flag=0
gnutlsFlag=0
libcdioFlag=0
pcreFlag=0
pthreadsW32Flag=0
breakpadFlag=0
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
        gcrypt|libgcrypt)
          allFlag=0
          gcryptFlag=1
          ;;
        ftplib)
          allFlag=0
          ftplibFlag=1
          ;;
        curl)
          allFlag=0
          curlFlag=1
          mxmlFlag=1
          ;;
        mxml)
          allFlag=0
          mxmlFlag=1
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
        breakpad|minidump)
          allFlag=0
          breakpadFlag=1
          ;;
        pcre)
          allFlag=0
          pcreFlag=1
          ;;
        pthreads-w32|pthreads-W32|pthreadsw32|pthreadsW32)
          allFlag=0
          pthreadsW32Flag=1
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
    gcrypt|libgcrypt)
      allFlag=0
      gcryptFlag=1
      ;;
    ftplib)
      allFlag=0
      ftplibFlag=1
      ;;
    curl)
      allFlag=0
      curlFlag=1
      mxmlFlag=1
      ;;
    mxml)
      allFlag=0
      mxmlFlag=1
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
    breakpad|minidump)
      allFlag=0
      breakpadFlag=1
      ;;
    pcre)
      allFlag=0
      pcreFlag=1
      ;;
    pthreads-w32|pthreads-W32|pthreadsw32|pthreadsW32)
      allFlag=0
      pthreadsW32Flag=1
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
  $ECHO "download-third-party-packages.sh [-d|--destination=<path>] [-n|--no-decompress] [-c|--clean] [--help] [all] [zlib] [bzip2] [lzma] [xdelta] [gcrypt] [curl] [mxml] [openssl] [libssh2] [gnutls] [libcdio] [breakpad] [pcre] [epm]"
  $ECHO ""
  $ECHO "Download additional third party packages."
  exit 0
fi

# check if wget, patch are available
type $WGET 1>/dev/null 2>/dev/null && $WGET --version 1>/dev/null 2>/dev/null
if test $? -ne 0; then
  $ECHO >&2 "ERROR: command 'wget' is not available"
  exit 1
fi
type $SVN 1>/dev/null 2>/dev/null && $SVN --version 1>/dev/null 2>/dev/null
if test $? -ne 0; then
  $ECHO >&2 "ERROR: command 'svn' is not available"
  exit 1
fi
type $PATCH 1>/dev/null 2>/dev/null && $PATCH --version 1>/dev/null 2>/dev/null
if test $? -ne 0; then
  $ECHO >&2 "ERROR: command 'patch' is not available"
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
       fileName=`$WGET $WGET_OPTIONS --quiet -O - 'http://www.zlib.net'|grep -E -e 'http://zlib.net/zlib-.*\.tar\.gz'|head -1|sed 's|.*http://zlib.net/\(.*\.tar\.gz\)".*|\1|g'`
       $WGET $WGET_OPTIONS "http://www.zlib.net/$fileName"
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
       $WGET $WGET_OPTIONS 'http://www.bzip.org/1.0.5/bzip2-1.0.5.tar.gz'
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
       fileName=`$WGET $WGET_OPTIONS --quiet -O - 'http://tukaani.org/xz'|grep -E -e 'xz-.*\.tar\.gz'|head -1|sed 's|.*href="\(xz.*\.tar\.gz\)".*|\1|g'`
       $WGET $WGET_OPTIONS "http://tukaani.org/xz/$fileName"
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
    # xdelta 3.0.0
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f xdelta3.0.0.tar.gz; then
       $WGET $WGET_OPTIONS 'http://xdelta.googlecode.com/files/xdelta3.0.0.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf xdelta3.0.0.tar.gz

       # patch to fix warnings:
       #   diff -u xdelta3.0.0.org/xdelta3.c        xdelta3.0.0/xdelta3.c        >  xdelta3.0.patch
       #   diff -u xdelta3.0.0.org/xdelta3-decode.h xdelta3.0.0/xdelta3-decode.h >> xdelta3.0.patch
       #   diff -u xdelta3.0.0.org/xdelta3-hash.h   xdelta3.0.0/xdelta3-hash.h   >> xdelta3.0.patch
       (cd xdelta3.0.0; $PATCH --batch -N -p1 < ../../misc/xdelta3.0.patch) 1>/dev/null 2>/dev/null
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s `find $tmpDirectory -type d -name "xdelta3*"` xdelta3
    fi
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    # gpg-error 1.10, gcrypt 1.5.0
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f libgpg-error-1.10.tar.bz2; then
       $WGET $WGET_OPTIONS 'ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-1.10.tar.bz2'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf libgpg-error-1.10.tar.bz2
     fi
     if test ! -f libgcrypt-1.5.0.tar.bz2; then
       $WGET $WGET_OPTIONS 'ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-1.5.0.tar.bz2'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf libgcrypt-1.5.0.tar.bz2

       # patch to disable wrong deprecated warnings:
       #   diff -u libgcrypt-1.5.0.org/src/gcrypt.h libgcrypt-1.5.0/src/gcrypt.h > libgcrypt-warning.patch
       (cd libgcrypt-1.5.0; $PATCH --batch -N -p1 < ../../misc/libgcrypt-warning.patch) 1>/dev/null 2>/dev/null
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/libgpg-error-1.10 libgpg-error
      $LN -f -s $tmpDirectory/libgcrypt-1.5.0 libgcrypt
    fi
  fi

  # obsolete
  if test $ftplibFlag -eq 1; then
    # ftplib 3.1
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f ftplib-4.0.tar.gz; then
       $WGET $WGET_OPTIONS 'http://nbpfaus.net/~pfau/ftplib/ftplib-4.0.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf ftplib-4.0.tar.gz

       # patch to disable output via perror():
       #   diff -u ftplib-3.1.org/linux/Makefile ftplib-3.1/linux/Makefile > ftplib-3.1-without-perror.patch
       (cd ftplib-4.0; $PATCH --batch -N -p1 < ../../misc/ftplib-3.1-without-perror.patch   ) 1>/dev/null 2>/dev/null
       # patch to fix bug in FTPAccess:
       #   diff -u ftplib-3.1.org/inux/ftplib.c ftplib-3.1/inux/ftplib.c > ftplib-3.1-ftpaccess.patch
       (cd ftplib-4.0; $PATCH --batch -N -p1 < ../../misc/ftplib-3.1-ftpaccess.patch        ) 1>/dev/null 2>/dev/null
       # patch to support timeout in receive:
       #   diff -u ftplib-3.1.org/linux/ftplib.c ftplib-3.1/linux/ftplib.c > ftplib-3.1-receive-timeout.patch
       (cd ftplib-4.0; $PATCH --batch -N -p1 < ../../misc/ftplib-3.1-receive-timeout.patch  ) 1>/dev/null 2>/dev/null
       # patch to fix not closed file in FtpXfer:
       #   diff -u ftplib-3.1.org/linux/ftplib.c ftplib-3.1/linux/ftplib.c > ftplib-3.1-ftpdir-file-close.patch
       (cd ftplib-4.0; $PATCH --batch -N -p1 < ../../misc/ftplib-3.1-ftpdir-file-close.patch) 1>/dev/null 2>/dev/null
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/ftplib-4.0 ftplib
    fi
  fi

  if test $allFlag -eq 1 -o $curlFlag -eq 1; then
    # c-areas 1.10
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f c-ares-1.10.0.tar.gz; then
       $WGET $WGET_OPTIONS 'http://c-ares.haxx.se/download/c-ares-1.10.0.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf c-ares-1.10.0.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/c-ares-1.10.0 c-ares
    fi

    # curl 7.28.1
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f curl-7.28.1.tar.bz2; then
       $WGET $WGET_OPTIONS 'http://curl.haxx.se/download/curl-7.28.1.tar.bz2'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf curl-7.28.1.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/curl-7.28.1 curl
    fi
  fi

  if test $allFlag -eq 1 -o $mxmlFlag -eq 1; then
    # mxml 2.7
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f mxml-2.7.tar.gz; then
       $WGET $WGET_OPTIONS 'http://www.msweet.org/files/project3/mxml-2.7.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf mxml-2.7.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/mxml-2.7 mxml
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
       $WGET $WGET_OPTIONS 'http://www.openssl.org/source/openssl-1.0.1c.tar.gz'
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
       $WGET $WGET_OPTIONS 'http://www.libssh2.org/download/libssh2-1.4.2.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf libssh2-1.4.2.tar.gz

       # patch to support keep alive for libssh 2.1.1 (ignore errors):
       #   diff -u libssh2-1.1.org/include/libssh2.h libssh2-1.1/include/libssh2.h >  libssh2-1.1-keepalive.patch
       #   diff -u libssh2-1.1.org/src/channel.c     libssh2-1.1/src/channel.c     >> libssh2-1.1-keepalive.patch
       (cd packages; patch --batch -N -p0 < ../misc/libssh2-1.1-keepalive.patch) 1>/dev/null 2>/dev/null
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/libssh2-1.4.2 libssh2
    fi
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then

#28C67298
#gpg --recv-keys 28C67298
#gpg --list-keys 28C67298; echo $?
    # nettle 2.6
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f nettle-2.6.tar.gz; then
       $WGET $WGET_OPTIONS 'ftp://ftp.lysator.liu.se/pub/security/lsh/nettle-2.6.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf nettle-2.6.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/nettle-2.6 nettle
    fi

#https://gmplib.org/download/gmp/gmp-5.1.3.tar.bz2.sig
    # gmp 5.1.3
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f gmp-5.1.3.tar.bz2; then
       $WGET $WGET_OPTIONS 'https://gmplib.org/download/gmp/gmp-5.1.3.tar.bz2'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf gmp-5.1.3.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/gmp-5.1.3 gmp
    fi

    # gnutls 3.1.18
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi

     if test ! -f gnutls-3.1.18.tar.xz; then
       $WGET $WGET_OPTIONS 'ftp://ftp.gnutls.org/gcrypt/gnutls/v3.1/gnutls-3.1.18.tar.xz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xJf gnutls-3.1.18.tar.xz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/gnutls-3.1.18 gnutls
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
       $WGET $WGET_OPTIONS 'ftp://ftp.gnu.org/gnu/libcdio/libcdio-0.82.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf libcdio-0.82.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/libcdio-0.82 libcdio
    fi
  fi

  if test $allFlag -eq 1 -o $breakpadFlag -eq 1; then
    # breakpad
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi

     if test ! -d breakpad; then
       $ECHO_NO_NEW_LINE "Checkout 'http://google-breakpad.googlecode.com/svn/trunk'..."
       $SVN checkout 'http://google-breakpad.googlecode.com/svn/trunk' breakpad >/dev/null
       $ECHO "done"
     fi
    )
    $LN -f -s $tmpDirectory/breakpad breakpad
  fi

  if test $allFlag -eq 1 -o $pcreFlag -eq 1; then
    # pcre 8.32
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi

     if test ! -f pcre-8.32.tar.bz2; then
       $WGET $WGET_OPTIONS 'ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre-8.32.tar.bz2'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf pcre-8.32.tar.bz2
     fi
    )
    $LN -f -s $tmpDirectory/pcre-8.32 pcre
  fi

  if test $allFlag -eq 1 -o $pthreadsW32Flag -eq 1; then
    # pthreads-w32 2.9.1
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi

     if test ! -f pthreads-w32-2-9-1-release.tar.gz; then
       $WGET $WGET_OPTIONS 'ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.tar.gz'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf pthreads-w32-2-9-1-release.tar.gz
     fi
    )
    $LN -f -s $tmpDirectory/pthreads-w32-2-9-1-release pthreads-w32
  fi

  if test $allFlag -eq 1 -o $epmFlag -eq 1; then
    # epm 4.1
    (
     if test -n "$destination"; then
       cd $destination
     else
       cd $tmpDirectory
     fi
     if test ! -f epm-4.2-source.tar.bz2; then
       $WGET $WGET_OPTIONS 'http://www.msweet.org/files/project2/epm-4.2-source.tar.bz2'
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf epm-4.2-source.tar.bz2

       # patch to support creating RPM packages on different machines:
       #   diff -u epm-4.1.org/rpm.c epm-4.1/rpm.c > epm-4.1-rpm.patch
       (cd epm-4.2; $PATCH --batch -N -p1 < ../../misc/epm-4.1-rpm.patch) 1>/dev/null 2>/dev/null
     fi
    )
    if test $noDecompressFlag -eq 0; then
      $LN -f -s $tmpDirectory/epm-4.2 epm
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

  if test $allFlag -eq 1 -o $curlFlag -eq 1; then
    # curl
    $RMF $tmpDirectory/curl-*-.tar.bz2
    $RMRF $tmpDirectory/curl-*
    $RMF curl

    # c-areas
    $RMF $tmpDirectory/c-ares-*-.tar.gz
    $RMRF $tmpDirectory/c-ares-*
    $RMF c-ares
  fi

  if test $allFlag -eq 1 -o $mxmlFlag -eq 1; then
    # mxml
    $RMF $tmpDirectory/mxml-*-.tar.bz2
    $RMRF $tmpDirectory/mxml-*
    $RMF mxml
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

    # gmp
    $RMF $tmpDirectory/gmp-*.tar.bz2
    $RMRF $tmpDirectory/gmp-*
    $RMF gmp

    # nettle
    $RMF $tmpDirectory/nettle-*.tar.bz2
    $RMRF $tmpDirectory/nettle-*
    $RMF nettle
  fi

  if test $allFlag -eq 1 -o $libcdioFlag -eq 1; then
    # libcdio
    $RMF $tmpDirectory/libcdio-*.tar.gz
    $RMRF $tmpDirectory/libcdio-*
    $RMF libcdio
  fi

  if test $allFlag -eq 1 -o $breakpadFlag -eq 1; then
    # breakpad
    $RMRF $tmpDirectory/breakpad
    $RMF breakpad
  fi

  if test $allFlag -eq 1 -o $pcreFlag -eq 1; then
    # breakpad
    $RMRF $tmpDirectory/pcre-*
    $RMF pcre
  fi

  if test $allFlag -eq 1 -o $pthreadsW32Flag -eq 1; then
    # breakpad
    $RMRF $tmpDirectory/pthreads-w32-*
    $RMF pcre
  fi

  if test $allFlag -eq 1 -o $epmFlag -eq 1; then
    # epm
    $RMF $tmpDirectory/epm-*.tar.bz2
    $RMRF $tmpDirectory/epm-*
    $RMF epm
  fi
fi

exit 0
