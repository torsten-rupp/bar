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
XDELTA3_VERSION=3.0.11
MXML_VERSION=2.12
LIBGPG_ERROR_VERSION=1.25
LIBGCRYPT_VERSION=1.8.4
NETTLE_VERSION=3.4
GMP_VERSION=6.1.2
LIBIDN2_VERSION=2.0.5
GNU_TLS_SUB_DIRECTORY=v3.5
GNU_TLS_VERSION=3.5.19
LIBICONV_VERSION=1.15
OPENSSL_VERSION=1.1.1
LIBSSH2_VERSION=1.8.0
C_ARES_VERSION=1.15.0
CURL_VERSION=7.62.0
PCRE_VERSION=8.40
SQLITE_YEAR=2019
SQLITE_VERSION=3270200
# Note ICU: * 61.1 seems to be the latest version without C++11
#           * 58.2 seems to be the latest version which can be
#              compiled on older 32bit systems, e. g. CentOS 6
ICU_VERSION=58.2
MTX_VERSION=1.3.12
LIBCDIO_VERSION=2.0.0
BINUTILS_VERSION=2.31.1
BREAKPAD_REVISION=1430
EPM_VERSION=4.2

# --------------------------------- variables --------------------------------

# ---------------------------------- functions -------------------------------

# ------------------------------------ main ----------------------------------

# parse arguments
destination=$PWD
noDecompressFlag=0
verboseFlag=1
cleanFlag=0
helpFlag=0

allFlag=1
zlibFlag=0
bzip2Flag=0
lzmaFlag=0
lzoFlag=0
lz4Flag=0
zstdFlag=0
xdelta3Flag=0
gcryptFlag=0
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
    --no-verbose)
      verboseFlag=0
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
  $ECHO "Usage: download-third-party-packages.sh [<options>] all|<package> ..."
  $ECHO ""
  $ECHO "Options: -d|--destination=<path>"
  $ECHO "         -n|--no-decompress"
  $ECHO "         --no-verbose"
  $ECHO "         -c|--clean"
  $ECHO "         --help"
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

# get wget options
wgetOptions=$WGET_OPTIONS
if test $verboseFlag -eq 0; then
  wgetOptions="$wgetOptions --no-verbose"
fi

# create directory
install -d "$destination/extern"

# run
cwd=`pwd`
if test $cleanFlag -eq 0; then
  # download

  if test $allFlag -eq 1 -o $zlibFlag -eq 1; then
    # zlib
    (
     cd $destination/extern
     fileName=`ls zlib-*.tar.gz 2>/dev/null`
     if test ! -f "$fileName"; then
       fileName=`$WGET $WGET_OPTIONS --quiet -O - 'http://www.zlib.net'|grep -E -e 'http://.*/zlib-.*\.tar\.gz'|head -1|sed 's|.*http://.*/\(.*\.tar\.gz\).*|\1|g'`
       $WGET $wgetOptions "http://www.zlib.net/$fileName"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find extern -type d -name "zlib-*"` zlib)
    fi
  fi

  if test $allFlag -eq 1 -o $bzip2Flag -eq 1; then
    # bzip2
    (
     cd $destination/extern
     if test ! -f bzip2-$BZIP2_VERSION.tar.gz; then
       $WGET $wgetOptions "https://downloads.sourceforge.net/project/bzip2/bzip2-$BZIP2_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf bzip2-$BZIP2_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/bzip2-$BZIP2_VERSION bzip2)
    fi
  fi

  if test $allFlag -eq 1 -o $lzmaFlag -eq 1; then
    # lzma
    (
     cd $destination/extern
     fileName=`ls xz-*.tar.gz 2>/dev/null`
     if test ! -f "$fileName"; then
       fileName=`$WGET $WGET_OPTIONS --quiet -O - 'http://tukaani.org/xz'|grep -E -e 'xz-.*\.tar\.gz'|head -1|sed 's|.*href="\(xz.*\.tar\.gz\)".*|\1|g'`
       $WGET $wgetOptions "http://tukaani.org/xz/$fileName"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find extern -type d -name "xz-*"` xz)
    fi
  fi

  if test $allFlag -eq 1 -o $lzoFlag -eq 1; then
    # lzo
    (
     cd $destination/extern
     if test ! -f lzo-$LZO_VERSION.tar.gz; then
       $WGET $wgetOptions "http://www.oberhumer.com/opensource/lzo/download/lzo-$LZO_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf lzo-$LZO_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/lzo-$LZO_VERSION lzo)
    fi
  fi

  if test $allFlag -eq 1 -o $lz4Flag -eq 1; then
    # lz4
    (
     cd $destination/extern
     fileName=`ls lz4-*.tar.gz 2>/dev/null`
     if test ! -f "$fileName"; then
#       url=`$WGET $WGET_OPTIONS --quiet -O - 'http://code.google.com/p/lz4'|grep -E -e 'lz4-.*\.tar\.gz'|head -1|sed 's|.*"\(http.*/lz4-.*\.tar\.gz\)".*|\1|g'`
#
#       fileName=`echo $URL|sed 's|.*/\(lz4-.*\.tar\.gz\).*|\1|g'`
#       $WGET $wgetOptions "$url"
       fileName="lz4-$LZ4_VERSION.tar.gz"
       $WGET $wgetOptions "https://github.com/Cyan4973/lz4/archive/$LZ4_VERSION.tar.gz" -O "$fileName"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find extern -type d -name "lz4-*"` lz4)
    fi
  fi

  if test $allFlag -eq 1 -o $zstdFlag -eq 1; then
    # zstd
    (
     cd $destination/extern
     fileName=`ls zstd-*.zip 2>/dev/null`
     if test ! -f "$fileName"; then
#       url=`$WGET $WGET_OPTIONS --quiet -O - 'http://code.google.com/p/zstd'|grep -E -e 'zstd-.*\.tar\.gz'|head -1|sed 's|.*"\(http.*/zstd-.*\.tar\.gz\)".*|\1|g'`
#
#       fileName=`echo $URL|sed 's|.*/\(zstd-.*\.tar\.gz\).*|\1|g'`
#       $WGET $wgetOptions "$url"
       fileName="zstd-$ZSTD_VERSION.zip"
       $WGET $wgetOptions "https://github.com/facebook/zstd/archive/v$ZSTD_VERSION.zip" -O "$fileName"
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find extern -type d -name "zstd-*"` zstd)
    fi
  fi

  if test $allFlag -eq 1 -o $xdelta3Flag -eq 1; then
    # xdelta3
    (
     cd $destination/extern
     if test ! -f xdelta3-$XDELTA3_VERSION.tar.gz; then
       $WGET $wgetOptions "https://github.com/jmacd/xdelta-gpl/releases/download/v$XDELTA3_VERSION/xdelta3-$XDELTA3_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf xdelta3-$XDELTA3_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find extern -maxdepth 1 -type d -name "xdelta3-*"` xdelta3)
    fi
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    # gpg-error, gcrypt
    (
     cd $destination/extern
     if test ! -f libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2; then
       $WGET $wgetOptions "ftp://ftp.gnupg.org/gcrypt/libgpg-error/libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2
     fi
     if test ! -f libgcrypt-$LIBGCRYPT_VERSION.tar.bz2; then
       $WGET $wgetOptions "ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-$LIBGCRYPT_VERSION.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf libgcrypt-$LIBGCRYPT_VERSION.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/libgpg-error-$LIBGPG_ERROR_VERSION libgpg-error)
      (cd $destination; $LN -sfT extern/libgcrypt-$LIBGCRYPT_VERSION libgcrypt)
    fi
  fi

  if test $allFlag -eq 1 -o $curlFlag -eq 1; then
    # c-areas
    (
     cd $destination/extern
     if test ! -f c-ares-$C_ARES_VERSION.tar.gz; then
       $WGET $wgetOptions "http://c-ares.haxx.se/download/c-ares-$C_ARES_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf c-ares-$C_ARES_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/c-ares-$C_ARES_VERSION c-ares)
    fi

    # curl
    (
     cd $destination/extern
     if test ! -f curl-$CURL_VERSION.tar.bz2; then
       $WGET $wgetOptions "http://curl.haxx.se/download/curl-$CURL_VERSION.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf curl-$CURL_VERSION.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/curl-$CURL_VERSION curl)
    fi
  fi

  if test $allFlag -eq 1 -o $mxmlFlag -eq 1; then
    # mxml
    (
     cd $destination/extern
     if test ! -f mxml-$MXML_VERSION.tar.gz; then
       $WGET $wgetOptions "https://github.com/michaelrsweet/mxml/releases/download/v$MXML_VERSION/mxml-$MXML_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf mxml-$MXML_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/mxml-$MXML_VERSION mxml)
    fi
  fi

  if test $allFlag -eq 1 -o $opensslFlag -eq 1; then
    # openssl 1.0.1g
    (
     cd $destination/extern
     if test ! -f openssl-$OPENSSL_VERSION.tar.gz; then
       $WGET $wgetOptions "http://www.openssl.org/source/openssl-$OPENSSL_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf openssl-$OPENSSL_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/openssl-$OPENSSL_VERSION openssl)
    fi
  fi

  if test $allFlag -eq 1 -o $libssh2Flag -eq 1; then
    # libssh2
    (
     cd $destination/extern
     if test ! -f libssh2-$LIBSSH2_VERSION.tar.gz; then
       $WGET $wgetOptions "http://www.libssh2.org/download/libssh2-$LIBSSH2_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf libssh2-$LIBSSH2_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/libssh2-$LIBSSH2_VERSION libssh2)
    fi
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    # nettle
    (
     cd $destination/extern
     if test ! -f nettle-$NETTLE_VERSION.tar.gz; then
       $WGET $wgetOptions "https://ftp.gnu.org/gnu/nettle/nettle-$NETTLE_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf nettle-$NETTLE_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/nettle-$NETTLE_VERSION nettle)
    fi

    # gmp
    (
     cd $destination/extern
     if test ! -f gmp-$GMP_VERSION.tar.xz; then
       $WGET $wgetOptions "https://gmplib.org/download/gmp/gmp-$GMP_VERSION.tar.xz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xJf gmp-$GMP_VERSION.tar.xz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find extern -type d -name "gmp-*"` gmp)
    fi

    # libidn2
    (
     cd $destination/extern
     if test ! -f libidn2-$LIBIDN2_VERSION.tar.gz; then
       $WGET $wgetOptions "https://ftp.gnu.org/gnu/libidn/libidn2-$LIBIDN2_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf libidn2-$LIBIDN2_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT `find extern -type d -name "libidn2-*"` libidn2)
    fi

    # gnutls
    (
     cd $destination/extern
     if test ! -f gnutls-$GNU_TLS_VERSION.tar.xz; then
       $WGET $wgetOptions "ftp://ftp.gnutls.org/gcrypt/gnutls/$GNU_TLS_SUB_DIRECTORY/gnutls-$GNU_TLS_VERSION.tar.xz"
     fi
     if test $noDecompressFlag -eq 0; then
       $XZ -d -c gnutls-$GNU_TLS_VERSION.tar.xz | $TAR xf -
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/gnutls-$GNU_TLS_VERSION gnutls)
    fi
  fi

  if test $allFlag -eq 1 -o $libcdioFlag -eq 1; then
    # libiconv
    (
     cd $destination/extern
     if test ! -f libiconv-$LIBICONV_VERSION.tar.gz; then
       $WGET $wgetOptions "https://ftp.gnu.org/pub/gnu/libiconv/libiconv-$LIBICONV_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf libiconv-$LIBICONV_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/libiconv-$LIBICONV_VERSION libiconv)
    fi

    # libcdio 0.92
    (
     cd $destination/extern
     if test ! -f libcdio-$LIBCDIO_VERSION.tar.gz; then
       $WGET $wgetOptions "ftp://ftp.gnu.org/gnu/libcdio/libcdio-$LIBCDIO_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf libcdio-$LIBCDIO_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/libcdio-$LIBCDIO_VERSION libcdio)
    fi
  fi

  if test $allFlag -eq 1 -o $mtxFlag -eq 1; then
    # mtx
    (
     cd $destination/extern
     if test ! -f mtx-$MTX_VERSION.tar.gz; then
       $WGET $wgetOptions "http://sourceforge.net/projects/mtx/files/mtx-stable/$MTX_VERSION/mtx-$MTX_VERSION.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf mtx-$MTX_VERSION.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/mtx-$MTX_VERSION mtx)
    fi
  fi

  if test $allFlag -eq 1 -o $pcreFlag -eq 1; then
    # pcre
    (
     cd $destination/extern
     if test ! -f pcre-$PCRE_VERSION.tar.bz2; then
       $WGET $wgetOptions "ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/pcre-$PCRE_VERSION.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf pcre-$PCRE_VERSION.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/pcre-$PCRE_VERSION pcre)
    fi
  fi

  if test $allFlag -eq 1 -o $sqliteFlag -eq 1; then
    # sqlite
    (
     cd $destination/extern
     if test ! -f sqlite-src-$SQLITE_VERSION.zip; then
       $WGET $wgetOptions "https://www.sqlite.org/$SQLITE_YEAR/sqlite-src-$SQLITE_VERSION.zip"
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q sqlite-src-$SQLITE_VERSION.zip
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/sqlite-src-$SQLITE_VERSION sqlite)
    fi
  fi

  if test $allFlag -eq 1 -o $icuFlag -eq 1; then
    # icu
    (
     cd $destination/extern
     if test ! -f icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz; then
       $WGET $wgetOptions "http://download.icu-project.org/files/icu4c/$ICU_VERSION/icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/icu icu)
    fi
  fi

  if test $allFlag -eq 1 -o $binutilsFlag -eq 1; then
    # binutils
    (
     cd $destination/extern
     if test ! -f binutils-$BINUTILS_VERSION.tar.bz2; then
       $WGET $wgetOptions "http://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf binutils-$BINUTILS_VERSION.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/binutils-$BINUTILS_VERSION binutils)
    fi
  fi

  if test $allFlag -eq 1 -o $pthreadsW32Flag -eq 1; then
    # pthreads-w32 2.9.1
    (
     cd $destination/extern
     if test ! -f pthreads-w32-2-9-1-release.tar.gz; then
       $WGET $wgetOptions "ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.tar.gz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf pthreads-w32-2-9-1-release.tar.gz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/pthreads-w32-2-9-1-release pthreads-w32)
    fi
  fi

  if test $breakpadFlag -eq 1; then
    # breakpad
    (
     cd $destination/extern
     if test ! -d breakpad; then
       $ECHO_NO_NEW_LINE "Checkout 'http://google-breakpad.googlecode.com/svn/trunk', revision $BREAKPAD_REVISION..."
       $SVN checkout 'http://google-breakpad.googlecode.com/svn/trunk' breakpad -r$BREAKPAD_REVISION >/dev/null
       $ECHO "done"
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/breakpad breakpad)
    fi
  fi

  if test $epmFlag -eq 1; then
    # epm
    (
     cd $destination/extern
     if test ! -f epm-$EPM_VERSION-source.tar.bz2; then
       $WGET $wgetOptions "http://www.msweet.org/files/project2/epm-$EPM_VERSION-source.tar.bz2"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf epm-$EPM_VERSION-source.tar.bz2
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/epm-$EPM_VERSION epm)
    fi
  fi

  if test $launch4jFlag -eq 1; then
    # launchj4
    (
     cd $destination/extern
     if test ! -f launch4j-3.1.0-beta2-linux.tgz; then
       $WGET $wgetOptions "http://downloads.sourceforge.net/project/launch4j/launch4j-3/3.1.0-beta2/launch4j-3.1.0-beta2-linux.tgz"
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf launch4j-3.1.0-beta2-linux.tgz
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/launch4j launch4j)
    fi
  fi

  if test $jreWindowsFlag -eq 1; then
    # Windows JRE from OpenJDK 6
    (
     cd $destination/extern
     if test ! -f openjdk-1.6.0-unofficial-b30-windows-i586-image.zip; then
       $WGET $wgetOptions "https://bitbucket.org/alexkasko/openjdk-unofficial-builds/downloads/openjdk-1.6.0-unofficial-b30-windows-i586-image.zip"
     fi
     if test ! -f openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip; then
       $WGET $wgetOptions "https://bitbucket.org/alexkasko/openjdk-unofficial-builds/downloads/openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip"
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q openjdk-1.6.0-unofficial-b30-windows-i586-image.zip 'openjdk-1.6.0-unofficial-b30-windows-i586-image/jre/*'
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip 'openjdk-1.6.0-unofficial-b30-windows-amd64-image/jre/*'
     fi
    )
    if test $noDecompressFlag -eq 0; then
      (cd $destination; $LN -sfT extern/openjdk-1.6.0-unofficial-b30-windows-i586-image/jre jre_windows)
      (cd $destination; $LN -sfT extern/openjdk-1.6.0-unofficial-b30-windows-amd64-image/jre jre_windows_64)
    fi
  fi
else
  # clean

  if test $allFlag -eq 1 -o $zlibFlag -eq 1; then
    # zlib
    (
      cd $destination
      $RMF extern/zlib-*.tar.gz
      $RMRF extern/zlib-*
    )
    $RMF zlib
  fi

  if test $allFlag -eq 1 -o $bzip2Flag -eq 1; then
    # bzip2
    (
      cd $destination
      $RMF extern/bzip2-*.tar.gz
      $RMRF extern/bzip2-*
    )
    $RMF bzip2
  fi

  if test $allFlag -eq 1 -o $lzmaFlag -eq 1; then
    # lzma
    (
      cd $destination
      $RMF `find extern -type f -name "xz-*.tar.gz" 2>/dev/null`
      $RMRF `find extern -type d -name "xz-*" 2>/dev/null`
    )
    $RMF xz
  fi

  if test $allFlag -eq 1 -o $lzoFlag -eq 1; then
    # lzo
    (
      cd $destination
      $RMF extern/lzo-*.tar.gz
      $RMRF extern/lzo-*
    )
    $RMF lzo
  fi

  if test $allFlag -eq 1 -o $lz4Flag -eq 1; then
    # lz4
    (
      cd $destination
      $RMF extern/lz4*.tar.gz
      $RMRF extern/lz4*
    )
    $RMF lz4
  fi

  if test $allFlag -eq 1 -o $zstdFlag -eq 1; then
    # zstd
    (
      cd $destination
      $RMF extern/zstd*.zip
      $RMRF extern/zstd*
    )
    $RMF zstd
  fi

  if test $allFlag -eq 1 -o $xdelta3Flag -eq 1; then
    # xdelta3
    (
      cd $destination
      $RMF `find extern -type f -name "xdelta3-*.tar.gz" 2>/dev/null`
      $RMRF `find extern -type d -name "xdelta3-*" 2>/dev/null`
    )
    $RMF xdelta3
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    # gcrypt
    (
      cd $destination
      $RMF extern/libgpg-error-*.tar.bz2 extern/libgcrypt-*.tar.bz2
      $RMRF extern/libgpg-error-* extern/libgcrypt-*
    )
    $RMF libgpg-error libgcrypt
  fi

  if test $allFlag -eq 1 -o $curlFlag -eq 1; then
    # curl
    (
      cd $destination
      $RMF extern/curl-*-.tar.bz2
      $RMRF extern/curl-*
    )
    $RMF curl

    # c-areas
    (
      cd $destination
      $RMF extern/c-ares-*-.tar.gz
      $RMRF extern/c-ares-*
    )
    $RMF c-ares
  fi

  if test $allFlag -eq 1 -o $mxmlFlag -eq 1; then
    # mxml
    (
      cd $destination
      $RMF extern/mxml-*-.tar.bz2
      $RMRF extern/mxml-*
    )
    $RMF mxml
  fi

  if test $allFlag -eq 1 -o $opensslFlag -eq 1; then
    # openssl
    (
      cd $destination
      $RMF extern/openssl*.tar.gz
      $RMRF extern/openssl*
    )
    $RMF openssl
  fi

  if test $allFlag -eq 1 -o $libssh2Flag -eq 1; then
    # libssh2
    (
      cd $destination
      $RMF extern/libssh2*.tar.gz
      $RMRF extern/libssh2*
    )
    $RMF libssh2
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    # gnutls
    (
      cd $destination
      $RMF extern/gnutls-*.tar.bz2
      $RMRF extern/gnutls-*
    )
    $RMF gnutls

    # libidn2
    (
      cd $destination
      $RMF extern/libidn2-*.tar.gz
      $RMRF extern/libidn2-*
    )
    $RMF gmp

    # gmp
    (
      cd $destination
      $RMF extern/gmp-*.tar.bz2
      $RMRF extern/gmp-*
    )
    $RMF gmp

    # nettle
    (
      cd $destination
      $RMF extern/nettle-*.tar.bz2
      $RMRF extern/nettle-*
    )
    $RMF nettle
  fi

  if test $allFlag -eq 1 -o $libcdioFlag -eq 1; then
    # libiconv
    (
      cd $destination
      $RMF extern/libiconv-*.tar.gz
      $RMRF extern/libiconv-*
    )
    $RMF libiconv

    # libcdio
    (
      cd $destination
      $RMF extern/libcdio-*.tar.gz
      $RMRF extern/libcdio-*
    )
    $RMF libcdio
  fi

  if test $allFlag -eq 1 -o $mtxFlag -eq 1; then
    # mtx
    (
      cd $destination
      $RMRF extern/mtx-*
    )
    $RMF mtx
  fi

  if test $allFlag -eq 1 -o $pcreFlag -eq 1; then
    # pcre
    (
      cd $destination
      $RMRF extern/pcre-*
    )
    $RMF pcre
  fi

  if test $allFlag -eq 1 -o $sqliteFlag -eq 1; then
    # sqlite
    (
      cd $destination
      $RMRF extern/sqlite-*
    )
    $RMF sqlite
  fi

  if test $allFlag -eq 1 -o $icuFlag -eq 1; then
    # icu
    (
      cd $destination
      $RMRF extern/icu4c-*
      $RMRF extern/icu
    )
    $RMF icu
  fi

  if test $allFlag -eq 1 -o $binutilsFlag -eq 1; then
    # binutils
    (
      cd $destination
      $RMRF extern/binutils-*
      $RMRF extern/binutils
    )
    $RMF binutils
  fi

  if test $allFlag -eq 1 -o $pthreadsW32Flag -eq 1; then
    # pthreadW32
    (
      cd $destination
      $RMRF extern/pthreads-w32-*
    )
    $RMF pthreads-w32
  fi

  if test $allFlag -eq 1 -o $breakpadFlag -eq 1; then
    # breakpad
    (
      cd $destination
      $RMRF extern/breakpad
    )
    $RMF breakpad
  fi

  if test $allFlag -eq 1 -o $epmFlag -eq 1; then
    # epm
    (
      cd $destination
      $RMF extern/epm-*.tar.bz2
      $RMRF extern/epm-*
    )
    $RMF epm
  fi

  if test $allFlag -eq 1 -o $launch4jFlag -eq 1; then
    # launch4j
    (
      cd $destination
      $RMF extern/launch4j-*.tgz
      $RMRF extern/launch4j
    )
    $RMF launch4j
  fi

  if test $jreWindowsFlag -eq 1; then
    # Windows JRE
    (
      cd $destination
      $RMF extern/openjdk-*.zip
      $RMRF extern/openjdk-*
    )
    $RMF jre_windows jre_windows_64
  fi
fi

exit 0
