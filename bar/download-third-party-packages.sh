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
UNZIP="unzip"
XZ="xz"

BZIP2_VERSION=1.0.6
LZO_VERSION=2.09
LZ4_VERSION=r131
ZSTD_VERSION=1.3.2
MXML_VERSION=2.10
LIBGPG_ERROR_VERSION=1.25
LIBGCRYPT_VERSION=1.8.0
NETTLE_VERSION=3.4
GMP_VERSION=6.1.2
GNU_TLS_SUB_DIRECTORY=v3.5
GNU_TLS_VERSION=3.5.19
LIBSSH2_VERSION=1.8.0
PCRE_VERSION=8.40
#SQLITE_YEAR=2016
SQLITE_YEAR=2017
#SQLITE_VERSION=3140100
SQLITE_VERSION=3210000
ICU_VERSION=62.1
MTX_VERSION=1.3.12
BINUTILS_VERSION=2.25
BREAKPAD_REVISION=1430
EPM_VERSION=4.2
XDELTA3_VERSION=3.1.0

# --------------------------------- variables --------------------------------

# ---------------------------------- functions -------------------------------

# ------------------------------------ main ----------------------------------

# parse arguments
destination="."
noDecompressFlag=0

allFlag=1
zlibFlag=0
bzip2Flag=0
lzmaFlag=0
lzoFlag=0
lz4Flag=0
zstdFlag=0
xdelta3Flag=0
gcryptFlag=0
ftplibFlag=0
curlFlag=0
mxmlFlag=0
opensslFlag=0
libssh2Flag=0
gnutlsFlag=0
libcdioFlag=0
pcreFlag=0
sqliteFlag=0
icuFlag=0
mtxFlag=0
binutilsFlag=0
pthreadsW32Flag=0
breakpadFlag=0
epmFlag=0
launch4jFlag=0
jreWindowsFlag=0

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
        lzo)
          allFlag=0
          lzoFlag=1
          ;;
        lz4)
          allFlag=0
          lz4Flag=1
          ;;
        zstd)
          allFlag=0
          zstdFlag=1
          ;;
        xdelta3)
          allFlag=0
          xdelta3Flag=1
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
        mtx)
          allFlag=0
          mtxFlag=1
          ;;
        pcre)
          allFlag=0
          pcreFlag=1
          ;;
        sqlite)
          allFlag=0
          sqliteFlag=1
          ;;
        icu)
          allFlag=0
          icuFlag=1
          ;;
        binutils|bfd)
          allFlag=0
          binutilsFlag=1
          ;;
        pthreads-w32|pthreads-W32|pthreadsw32|pthreadsW32)
          allFlag=0
          pthreadsW32Flag=1
          ;;
        breakpad|minidump)
          allFlag=0
          breakpadFlag=1
          ;;
        epm)
          allFlag=0
          epmFlag=1
          ;;
        launch4j)
          allFlag=0
          launch4jFlag=1
          ;;
        jre-windows)
          allFlag=0
          jreWindowsFlag=1
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
    lzo)
      allFlag=0
      lzoFlag=1
      ;;
    lz4)
      allFlag=0
      lz4Flag=1
      ;;
    zstd)
      allFlag=0
      zstdFlag=1
      ;;
    xdelta3)
      allFlag=0
      xdelta3Flag=1
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
    mtx)
      allFlag=0
      mtxFlag=1
      ;;
    pcre)
      allFlag=0
      pcreFlag=1
      ;;
    sqlite)
      allFlag=0
      sqliteFlag=1
      ;;
    icu)
      allFlag=0
      icuFlag=1
      ;;
    binutils|bfd)
      allFlag=0
      binutilsFlag=1
      ;;
    pthreads-w32|pthreads-W32|pthreadsw32|pthreadsW32)
      allFlag=0
      pthreadsW32Flag=1
      ;;
    breakpad|minidump)
      allFlag=0
      breakpadFlag=1
      ;;
    epm)
      allFlag=0
      epmFlag=1
      ;;
    launch4j)
      allFlag=0
      launch4jFlag=1
      ;;
    jre-windows)
      allFlag=0
      jreWindowsFlag=1
      ;;
    *)
      $ECHO >&2 "ERROR: unknown package '$1'"
      exit 1
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  $ECHO "Download additional third party packages."
  $ECHO ""
  $ECHO "Usage: download-third-party-packages.sh [-d|--destination=<path>] [-n|--no-decompress] [-c|--clean] [--help] [all] [<package>] ..."
  $ECHO ""
  $ECHO "Packages (included in 'all'):"
  $ECHO ""
  $ECHO " zlib"
  $ECHO " bzip2"
  $ECHO " lzma"
  $ECHO " lzo"
  $ECHO " lz4"
  $ECHO " zstd"
  $ECHO " xdelta3"
  $ECHO " gcrypt"
  $ECHO " curl"
  $ECHO " mxml"
  $ECHO " openssl"
  $ECHO " libssh2"
  $ECHO " gnutls"
  $ECHO " libcdio"
  $ECHO " pcre"
  $ECHO " sqlite"
  $ECHO " icu"
  $ECHO " binutils"
  $ECHO ""
  $ECHO "Additional optional packages:"
  $ECHO ""
  $ECHO " breakpad"
  $ECHO " epm"
  $ECHO " launch4j"
  $ECHO " jre-windows"
  exit 0
fi

# check if required tools are available
type $WGET 1>/dev/null 2>/dev/null && $WGET --version 1>/dev/null 2>/dev/null
if test $? -gt 0; then
  $ECHO >&2 "ERROR: command 'wget' is not available"
  exit 1
fi
type $SVN 1>/dev/null 2>/dev/null && $SVN --version 1>/dev/null 2>/dev/null
if test $? -gt 0; then
  $ECHO >&2 "ERROR: command 'svn' is not available"
  exit 1
fi
type $PATCH 1>/dev/null 2>/dev/null && $PATCH --version 1>/dev/null 2>/dev/null
if test $? -gt 0; then
  $ECHO >&2 "ERROR: command 'patch' is not available"
  exit 1
fi
type $UNZIP 1>/dev/null 2>/dev/null && $UNZIP --version 1>/dev/null 2>/dev/null
if test $? -gt 10; then
  $ECHO >&2 "ERROR: command 'unzip' is not available"
  exit 1
fi
type $XZ 1>/dev/null 2>/dev/null && $XZ --version 1>/dev/null 2>/dev/null
if test $? -gt 0; then
  $ECHO >&2 "ERROR: command 'xz' is not available"
  exit 1
fi

# create directory
packageDirectory="$destination/packages"
install -d "$packageDirectory"

# run
cwd=`pwd`
if test $cleanFlag -eq 0; then
  # download

  if test $allFlag -eq 1 -o $zlibFlag -eq 1; then
    # zlib
    (
     cd $destination/packages
     fileName=`ls zlib-*.tar.gz 2>/dev/null`
     if test ! -f "$fileName"; then
       fileName=`$WGET $WGET_OPTIONS --quiet -O - 'http://www.zlib.net'|grep -E -e 'http://.*/zlib-.*\.tar\.gz'|head -1|sed 's|.*http://.*/\(.*\.tar\.gz\).*|\1|g'`
       $WGET $WGET_OPTIONS "http://www.zlib.net/$fileName"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find packages -type d -name "zlib-*"` zlib)
    fi
  fi

  if test $allFlag -eq 1 -o $bzip2Flag -eq 1; then
    # bzip2
    (
     cd $destination/packages
     if test ! -f bzip2-$BZIP2_VERSION.tar.gz; then
       $WGET $WGET_OPTIONS "http://www.bzip.org/$BZIP2_VERSION/bzip2-$BZIP2_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf bzip2-$BZIP2_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/bzip2-$BZIP2_VERSION bzip2)
    fi
  fi

  if test $allFlag -eq 1 -o $lzmaFlag -eq 1; then
    # lzma
    (
     cd $destination/packages
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
      (cd $destination; $LN -sfT `find packages -type d -name "xz-*"` xz)
    fi
  fi

  if test $allFlag -eq 1 -o $lzoFlag -eq 1; then
    # lzo
    (
     cd $destination/packages
     if test ! -f lzo-$LZO_VERSION.tar.gz; then
       $WGET $WGET_OPTIONS "http://www.oberhumer.com/opensource/lzo/download/lzo-$LZO_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf lzo-$LZO_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/lzo-$LZO_VERSION lzo)
    fi
  fi

  if test $allFlag -eq 1 -o $lz4Flag -eq 1; then
    # lz4
    (
     cd $destination/packages
     fileName=`ls lz4-*.tar.gz 2>/dev/null`
     if test ! -f "$fileName"; then
#       url=`$WGET $WGET_OPTIONS --quiet -O - 'http://code.google.com/p/lz4'|grep -E -e 'lz4-.*\.tar\.gz'|head -1|sed 's|.*"\(http.*/lz4-.*\.tar\.gz\)".*|\1|g'`
#
#       fileName=`echo $URL|sed 's|.*/\(lz4-.*\.tar\.gz\).*|\1|g'`
#       $WGET $WGET_OPTIONS "$url"
       fileName="lz4-$LZ4_VERSION.tar.gz"
       $WGET $WGET_OPTIONS "https://github.com/Cyan4973/lz4/archive/$LZ4_VERSION.tar.gz" -O "$fileName"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find packages -type d -name "lz4-*"` lz4)
    fi
  fi
  
  if test $allFlag -eq 1 -o $zstdFlag -eq 1; then
    # zstd
    (
     cd $destination/packages
     fileName=`ls zstd-*.zip 2>/dev/null`
     if test ! -f "$fileName"; then
#       url=`$WGET $WGET_OPTIONS --quiet -O - 'http://code.google.com/p/zstd'|grep -E -e 'zstd-.*\.tar\.gz'|head -1|sed 's|.*"\(http.*/zstd-.*\.tar\.gz\)".*|\1|g'`
#
#       fileName=`echo $URL|sed 's|.*/\(zstd-.*\.tar\.gz\).*|\1|g'`
#       $WGET $WGET_OPTIONS "$url"
       fileName="zstd-$ZSTD_VERSION.zip"
       $WGET $WGET_OPTIONS "https://github.com/facebook/zstd/archive/v$ZSTD_VERSION.zip" -O "$fileName"
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find packages -type d -name "zstd-*"` zstd)
    fi
  fi

  if test $allFlag -eq 1 -o $xdelta3Flag -eq 1; then
    # xdelta3
    (
     cd $destination/packages
     if test ! -f xdelta3-$XDELTA3_VERSION.tar.gz; then
       $WGET $WGET_OPTIONS "https://github.com/jmacd/xdelta-gpl/releases/download/v3.1.0/xdelta3-$XDELTA3_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf xdelta3-$XDELTA3_VERSION.tar.gz

       # patch to fix warnings:
       #   diff -u xdelta3.0.0.org/xdelta3.c        xdelta3.0.0/xdelta3.c        >  xdelta3.0.patch
       #   diff -u xdelta3.0.0.org/xdelta3-decode.h xdelta3.0.0/xdelta3-decode.h >> xdelta3.0.patch
       #   diff -u xdelta3.0.0.org/xdelta3-hash.h   xdelta3.0.0/xdelta3-hash.h   >> xdelta3.0.patch
       (cd xdelta3-$XDELTA3_VERSION; $PATCH --batch -N -p1 < ../../misc/xdelta3-3.1.0.patch) 1>/dev/null 2>/dev/null
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find packages -maxdepth 1 -type d -name "xdelta3-*"` xdelta3)
    fi
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    # gpg-error, gcrypt
    (
     cd $destination/packages
     if test ! -f libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2; then
       $WGET $WGET_OPTIONS "ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2
     fi
     if test ! -f libgcrypt-$LIBGCRYPT_VERSION.tar.bz2; then
       $WGET $WGET_OPTIONS "ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-$LIBGCRYPT_VERSION.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf libgcrypt-$LIBGCRYPT_VERSION.tar.bz2

       # patch to disable wrong deprecated warnings:
       #   diff -u libgcrypt-1.5.0.org/src/gcrypt.h libgcrypt-1.5.0/src/gcrypt.h > libgcrypt-warning.patch
       (cd libgcrypt-$LIBGCRYPT_VERSION; $PATCH --batch -N -p1 < ../../misc/libgcrypt-warning.patch) 1>/dev/null 2>/dev/null
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/libgpg-error-$LIBGPG_ERROR_VERSION libgpg-error)
      (cd $destination; $LN -sfT packages/libgcrypt-$LIBGCRYPT_VERSION libgcrypt)
    fi
  fi

  # obsolete
  if test $ftplibFlag -eq 1; then
    # ftplib 3.1
    (
     cd $destination/packages
     if test ! -f ftplib-4.0.tar.gz; then
       $WGET $WGET_OPTIONS "http://nbpfaus.net/~pfau/ftplib/ftplib-4.0.tar.gz"
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
      (cd $destination; $LN -sfT packages/ftplib-4.0 ftplib)
    fi
  fi

  if test $allFlag -eq 1 -o $curlFlag -eq 1; then
    # c-areas 1.10
    (
     cd $destination/packages
     if test ! -f c-ares-1.10.0.tar.gz; then
       $WGET $WGET_OPTIONS "http://c-ares.haxx.se/download/c-ares-1.10.0.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf c-ares-1.10.0.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/c-ares-1.10.0 c-ares)
    fi

    # curl 7.28.1
    (
     cd $destination/packages
     if test ! -f curl-7.28.1.tar.bz2; then
       $WGET $WGET_OPTIONS "http://curl.haxx.se/download/curl-7.28.1.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf curl-7.28.1.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/curl-7.28.1 curl)
    fi
  fi

  if test $allFlag -eq 1 -o $mxmlFlag -eq 1; then
    # mxml
    (
     cd $destination/packages
     if test ! -f mxml-$MXML_VERSION.tar.gz; then
       $WGET $WGET_OPTIONS "https://github.com/michaelrsweet/mxml/releases/download/release-$MXML_VERSION/mxml-$MXML_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf mxml-$MXML_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/mxml-$MXML_VERSION mxml)
    fi
  fi

  if test $allFlag -eq 1 -o $opensslFlag -eq 1; then
    # openssl 1.0.1g
    (
     cd $destination/packages
     if test ! -f openssl-1.0.1g.tar.gz; then
       $WGET $WGET_OPTIONS "http://www.openssl.org/source/openssl-1.0.1g.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf openssl-1.0.1g.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/openssl-1.0.1g openssl)
    fi
  fi

  if test $allFlag -eq 1 -o $libssh2Flag -eq 1; then
    # libssh2
    (
     cd $destination/packages
     if test ! -f libssh2-$LIBSSH2_VERSION.tar.gz; then
       $WGET $WGET_OPTIONS "http://www.libssh2.org/download/libssh2-$LIBSSH2_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf libssh2-$LIBSSH2_VERSION.tar.gz

       # patch to support keep alive for libssh 2.1.1 (ignore errors):
       #   diff -u libssh2-1.1.org/include/libssh2.h libssh2-1.1/include/libssh2.h >  libssh2-1.1-keepalive.patch
       #   diff -u libssh2-1.1.org/src/channel.c     libssh2-1.1/src/channel.c     >> libssh2-1.1-keepalive.patch
       (cd packages; patch --batch -N -p0 < ../misc/libssh2-1.1-keepalive.patch) 1>/dev/null 2>/dev/null
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/libssh2-$LIBSSH2_VERSION libssh2)
    fi
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    # nettle
    (
     cd $destination/packages
     if test ! -f nettle-$NETTLE_VERSION.tar.gz; then
       $WGET $WGET_OPTIONS "https://ftp.gnu.org/gnu/nettle/nettle-$NETTLE_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf nettle-$NETTLE_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/nettle-$NETTLE_VERSION nettle)
    fi

    # gmp
    (
     cd $destination/packages
     if test ! -f gmp-$GMP_VERSION.tar.xz; then
       $WGET $WGET_OPTIONS "https://gmplib.org/download/gmp/gmp-$GMP_VERSION.tar.xz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xJf gmp-$GMP_VERSION.tar.xz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find packages -type d -name "gmp-*"` gmp)
    fi

    # gnutls
    (
     cd $destination/packages
     if test ! -f gnutls-$GNU_TLS_VERSION.tar.xz; then
       $WGET $WGET_OPTIONS "ftp://ftp.gnutls.org/gcrypt/gnutls/$GNU_TLS_SUB_DIRECTORY/gnutls-$GNU_TLS_VERSION.tar.xz"
     fi
     if test $noDecompressFlag -eq 0; then
       $XZ -d -c gnutls-$GNU_TLS_VERSION.tar.xz | $TAR xf -
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/gnutls-$GNU_TLS_VERSION gnutls)
    fi
  fi

  if test $allFlag -eq 1 -o $libcdioFlag -eq 1; then
    # libiconv 1.15
    (
     cd $destination/packages
     if test ! -f libiconv-1.15.tar.gz; then
       $WGET $WGET_OPTIONS "https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.15.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf libiconv-1.15.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/libiconv-1.15 libiconv)
    fi

    # libcdio 0.92
    (
     cd $destination/packages
     if test ! -f libcdio-0.92.tar.gz; then
       $WGET $WGET_OPTIONS "ftp://ftp.gnu.org/gnu/libcdio/libcdio-0.92.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf libcdio-0.92.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/libcdio-0.92 libcdio)
    fi
  fi

  if test $allFlag -eq 1 -o $mtxFlag -eq 1; then
    # mtx
    (
     cd $destination/packages
     if test ! -f mtx-$MTX_VERSION.tar.gz; then
       $WGET $WGET_OPTIONS "http://sourceforge.net/projects/mtx/files/mtx-stable/$MTX_VERSION/mtx-$MTX_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf mtx-$MTX_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/mtx-$MTX_VERSION mtx)
    fi
  fi

  if test $allFlag -eq 1 -o $pcreFlag -eq 1; then
    # pcre
    (
     cd $destination/packages
     if test ! -f pcre-$PCRE_VERSION.tar.bz2; then
       $WGET $WGET_OPTIONS "ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre-$PCRE_VERSION.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf pcre-$PCRE_VERSION.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/pcre-$PCRE_VERSION pcre)
    fi
  fi

  if test $allFlag -eq 1 -o $sqliteFlag -eq 1; then
    # sqlite
    (
     cd $destination/packages
     if test ! -f sqlite-src-$SQLITE_VERSION.zip; then
       $WGET $WGET_OPTIONS "https://www.sqlite.org/$SQLITE_YEAR/sqlite-src-$SQLITE_VERSION.zip"
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q sqlite-src-$SQLITE_VERSION.zip
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/sqlite-src-$SQLITE_VERSION sqlite)
    fi
  fi

  if test $allFlag -eq 1 -o $icuFlag -eq 1; then
    # icu
    (
     cd $destination/packages
     if test ! -f icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz; then
       $WGET $WGET_OPTIONS "http://download.icu-project.org/files/icu4c/$ICU_VERSION/icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/icu icu)
    fi
  fi

  if test $allFlag -eq 1 -o $binutilsFlag -eq 1; then
    # binutils
    (
     cd $destination/packages
     if test ! -f binutils-$BINUTILS_VERSION.tar.bz2; then
       $WGET $WGET_OPTIONS "http://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf binutils-$BINUTILS_VERSION.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/binutils-$BINUTILS_VERSION binutils)
    fi
  fi

  if test $allFlag -eq 1 -o $pthreadsW32Flag -eq 1; then
    # pthreads-w32 2.9.1
    (
     cd $destination/packages
     if test ! -f pthreads-w32-2-9-1-release.tar.gz; then
       $WGET $WGET_OPTIONS "ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf pthreads-w32-2-9-1-release.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/pthreads-w32-2-9-1-release pthreads-w32)
    fi
  fi

  if test $breakpadFlag -eq 1; then
    # breakpad
    (
     cd $destination/packages
     if test ! -d breakpad; then
       $ECHO_NO_NEW_LINE "Checkout 'http://google-breakpad.googlecode.com/svn/trunk', revision $BREAKPAD_REVISION..."
       $SVN checkout 'http://google-breakpad.googlecode.com/svn/trunk' breakpad -r$BREAKPAD_REVISION >/dev/null
       $ECHO "done"
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/breakpad breakpad)
    fi
  fi

  if test $epmFlag -eq 1; then
    # epm
    (
     cd $destination/packages
     if test ! -f epm-$EPM_VERSION-source.tar.bz2; then
       $WGET $WGET_OPTIONS "http://www.msweet.org/files/project2/epm-$EPM_VERSION-source.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf epm-$EPM_VERSION-source.tar.bz2

       # patch to support creating RPM packages on different machines:
       #   diff -u epm-4.1.org/rpm.c epm-4.1/rpm.c > epm-4.1-rpm.patch
       (cd epm-$EPM_VERSION; $PATCH --batch -N -p1 < ../../misc/epm-4.1-rpm.patch) 1>/dev/null 2>/dev/null
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/epm-$EPM_VERSION epm)
    fi
  fi

  if test $launch4jFlag -eq 1; then
    # launchj4
    (
     cd $destination/packages
     if test ! -f launch4j-3.1.0-beta2-linux.tgz; then
       $WGET $WGET_OPTIONS "http://downloads.sourceforge.net/project/launch4j/launch4j-3/3.1.0-beta2/launch4j-3.1.0-beta2-linux.tgz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf launch4j-3.1.0-beta2-linux.tgz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/launch4j launch4j)
    fi
  fi

  if test $jreWindowsFlag -eq 1; then
    # Windows JRE from OpenJDK 6
    (
     cd $destination/packages
     if test ! -f openjdk-1.6.0-unofficial-b30-windows-i586-image.zip; then
       $WGET $WGET_OPTIONS "https://bitbucket.org/alexkasko/openjdk-unofficial-builds/downloads/openjdk-1.6.0-unofficial-b30-windows-i586-image.zip"
     fi
     if test ! -f openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip; then
       $WGET $WGET_OPTIONS "https://bitbucket.org/alexkasko/openjdk-unofficial-builds/downloads/openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip"
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q openjdk-1.6.0-unofficial-b30-windows-i586-image.zip 'openjdk-1.6.0-unofficial-b30-windows-i586-image/jre/*'
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip 'openjdk-1.6.0-unofficial-b30-windows-amd64-image/jre/*'
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT packages/openjdk-1.6.0-unofficial-b30-windows-i586-image/jre jre_windows)
      (cd $destination; $LN -sfT packages/openjdk-1.6.0-unofficial-b30-windows-amd64-image/jre jre_windows_64)
    fi
  fi
else
  # clean

  if test $allFlag -eq 1 -o $zlibFlag -eq 1; then
    # zlib
    (
      cd $destination
      $RMF packages/zlib-*.tar.gz
      $RMRF packages/zlib-*
    )
    $RMF zlib
  fi

  if test $allFlag -eq 1 -o $bzip2Flag -eq 1; then
    # bzip2
    (
      cd $destination
      $RMF packages/bzip2-*.tar.gz
      $RMRF packages/bzip2-*
    )
    $RMF bzip2
  fi

  if test $allFlag -eq 1 -o $lzmaFlag -eq 1; then
    # lzma
    (
      cd $destination
      $RMF `find packages -type f -name "xz-*.tar.gz" 2>/dev/null`
      $RMRF `find packages -type d -name "xz-*" 2>/dev/null`
    )
    $RMF xz
  fi

  if test $allFlag -eq 1 -o $lzoFlag -eq 1; then
    # lzo
    (
      cd $destination
      $RMF packages/lzo-*.tar.gz
      $RMRF packages/lzo-*
    )
    $RMF lzo
  fi

  if test $allFlag -eq 1 -o $lz4Flag -eq 1; then
    # lz4
    (
      cd $destination
      $RMF packages/lz4*.tar.gz
      $RMRF packages/lz4*
    )
    $RMF lz4
  fi

  if test $allFlag -eq 1 -o $zstdFlag -eq 1; then
    # zstd
    (
      cd $destination
      $RMF packages/zstd*.zip
      $RMRF packages/zstd*
    )
    $RMF zstd
  fi

  if test $allFlag -eq 1 -o $xdelta3Flag -eq 1; then
    # xdelta3
    (
      cd $destination
      $RMF `find packages -type f -name "xdelta3-*.tar.gz" 2>/dev/null`
      $RMRF `find packages -type d -name "xdelta3-*" 2>/dev/null`
    )
    $RMF xdelta3
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    # gcrypt
    (
      cd $destination
      $RMF packages/libgpg-error-*.tar.bz2 packages/libgcrypt-*.tar.bz2
      $RMRF packages/libgpg-error-* packages/libgcrypt-*
    )
    $RMF libgpg-error libgcrypt
  fi

  if test $allFlag -eq 1 -o $ftplibFlag -eq 1; then
    # ftplib
    (
      cd $destination
      $RMF packages/ftplib-*-src.tar.gz packages/ftplib-*.patch
      $RMRF packages/ftplib-*
    )
    $RMF ftplib
  fi

  if test $allFlag -eq 1 -o $curlFlag -eq 1; then
    # curl
    (
      cd $destination
      $RMF packages/curl-*-.tar.bz2
      $RMRF packages/curl-*
    )
    $RMF curl

    # c-areas
    (
      cd $destination
      $RMF packages/c-ares-*-.tar.gz
      $RMRF packages/c-ares-*
    )
    $RMF c-ares
  fi

  if test $allFlag -eq 1 -o $mxmlFlag -eq 1; then
    # mxml
    (
      cd $destination
      $RMF packages/mxml-*-.tar.bz2
      $RMRF packages/mxml-*
    )
    $RMF mxml
  fi

  if test $allFlag -eq 1 -o $opensslFlag -eq 1; then
    # openssl
    (
      cd $destination
      $RMF packages/openssl*.tar.gz
      $RMRF packages/openssl*
    )
    $RMF openssl
  fi

  if test $allFlag -eq 1 -o $libssh2Flag -eq 1; then
    # libssh2
    (
      cd $destination
      $RMF packages/libssh2*.tar.gz
      $RMRF packages/libssh2*
    )
    $RMF libssh2
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    # gnutls
    (
      cd $destination
      $RMF packages/gnutls-*.tar.bz2
      $RMRF packages/gnutls-*
    )
    $RMF gnutls

    # gmp
    (
      cd $destination
      $RMF packages/gmp-*.tar.bz2
      $RMRF packages/gmp-*
    )
    $RMF gmp

    # nettle
    (
      cd $destination
      $RMF packages/nettle-*.tar.bz2
      $RMRF packages/nettle-*
    )
    $RMF nettle
  fi

  if test $allFlag -eq 1 -o $libcdioFlag -eq 1; then
    # libiconv
    (
      cd $destination
      $RMF packages/libiconv-*.tar.gz
      $RMRF packages/libiconv-*
    )
    $RMF libiconv

    # libcdio
    (
      cd $destination
      $RMF packages/libcdio-*.tar.gz
      $RMRF packages/libcdio-*
    )
    $RMF libcdio
  fi

  if test $allFlag -eq 1 -o $mtxFlag -eq 1; then
    # mtx
    (
      cd $destination
      $RMRF $tmpDirectory/mtx-*
    )
    $RMF mtx
  fi

  if test $allFlag -eq 1 -o $pcreFlag -eq 1; then
    # pcre
    (
      cd $destination
      $RMRF packages/pcre-*
    )
    $RMF pcre
  fi

  if test $allFlag -eq 1 -o $sqliteFlag -eq 1; then
    # sqlite
    (
      cd $destination
      $RMRF packages/sqlite-*
    )
    $RMF sqlite
  fi

  if test $allFlag -eq 1 -o $icuFlag -eq 1; then
    # icu
    (
      cd $destination
      $RMRF $tmpDirectory/icu4c-*
      $RMRF $tmpDirectory/icu
    )
    $RMF icu
  fi

  if test $allFlag -eq 1 -o $binutilsFlag -eq 1; then
    # binutils
    (
      cd $destination
      $RMRF packages/binutils
    )
    $RMF binutils
  fi

  if test $allFlag -eq 1 -o $pthreadsW32Flag -eq 1; then
    # pthreadW32
    (
      cd $destination
      $RMRF packages/pthreads-w32-*
    )
    $RMF pthreads-w32
  fi

  if test $allFlag -eq 1 -o $breakpadFlag -eq 1; then
    # breakpad
    (
      cd $destination
      $RMRF packages/breakpad
    )
    $RMF breakpad
  fi

  if test $allFlag -eq 1 -o $epmFlag -eq 1; then
    # epm
    (
      cd $destination
      $RMF packages/epm-*.tar.bz2
      $RMRF packages/epm-*
    )
    $RMF epm
  fi

  if test $allFlag -eq 1 -o $launch4jFlag -eq 1; then
    # launch4j
    (
      cd $destination
      $RMF packages/launch4j-*.tgz
      $RMRF packages/launch4j
    )
    $RMF launch4j
  fi

  if test $jreWindowsFlag -eq 1; then
    # Windows JRE
    (
      cd $destination
      $RMF packages/openjdk-*.zip
      $RMRF packages/openjdk-*
    )
    $RMF jre_windows jre_windows_64
  fi
fi

exit 0
