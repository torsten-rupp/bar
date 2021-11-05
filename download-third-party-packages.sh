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

CURL="curl"
CURL_OPTIONS="-L --retry 5 --connect-timeout 60 --max-time 300"
ECHO="echo"
ECHO_NO_NEW_LINE="echo -n"
LN="ln"
MKDIR="mkdir"
RMF="rm -f"
RMRF="rm -rf"
SVN="svn"
TAR="tar"
WGET="wget"
WGET_OPTIONS="--tries=5 --timeout=300"
UNZIP="unzip"
XZ="xz"

BZIP2_VERSION=1.0.8
LZO_VERSION=2.10
LZ4_VERSION=r131
ZSTD_VERSION=1.4.8
XDELTA3_VERSION=3.0.11
MXML_VERSION=3.2
LIBGPG_ERROR_VERSION=1.41
LIBGCRYPT_VERSION=1.8.7
NETTLE_VERSION=3.7
GMP_VERSION=6.2.1
LIBIDN2_VERSION=2.3.0
GNU_TLS_SUB_DIRECTORY=v3.7
GNU_TLS_VERSION=3.7.0
LIBICONV_VERSION=1.16
OPENSSL_VERSION=1.1.1i
LIBSSH2_VERSION=1.9.0
C_ARES_VERSION=1.17.1
CURL_VERSION=7.74.0
PCRE_VERSION=8.45
#TODO: remove
SQLITE_YEAR=2019
SQLITE_YEAR=2020
#TODO: remove
SQLITE_VERSION=3270200
SQLITE_VERSION=3340000
# Note ICU: * 61.1 seems to be the latest version without C++11
#           * 58.2 seems to be the latest version which can be
#              compiled on older 32bit systems, e. g. CentOS 6
ICU_VERSION=58.3
MTX_VERSION=1.3.12
LIBCDIO_VERSION=2.1.0
BINUTILS_VERSION=2.35
BREAKPAD_REVISION=1430
EPM_VERSION=4.2

# --------------------------------- variables --------------------------------

# ---------------------------------- functions -------------------------------

fatalError()
{
  message=$1; shift

  echo >&2 FAIL!
  echo >&2 ERROR: $message

  exit 1
}


# ------------------------------------ main ----------------------------------

# parse arguments
destination=$PWD
localDirectory=
noDecompressFlag=0
verboseFlag=0
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
      destination="$2"
      shift
      shift
      ;;
    -l | --local-directory)
      localDirectory="$2"
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
    --verbose)
      verboseFlag=1
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
  $ECHO "Options: -d|--destination <path>      - destination directory"
  $ECHO "         -l|--local-directory <path>  - local directory to get packages from"
  $ECHO "         -n|--no-decompress           - do not decompress archives"
  $ECHO "         --verbose                    - verbose output"
  $ECHO "         --no-verbose                 - disable verbose output"
  $ECHO "         -c|--clean                   - delete all packages in destination directory"
  $ECHO "         --help                       - print this help"
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
  $ECHO " pthreads-w32"
  $ECHO " jre-windows"
  exit 0
fi

# check if required tools are available
type $CURL 1>/dev/null 2>/dev/null && $CURL --version 1>/dev/null 2>/dev/null
if test $? -gt 0; then
  $ECHO >&2 "ERROR: command 'curl' is not available"
  exit 1
fi
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

# get curl options
curlOptions=$CURL_OPTIONS
if test $verboseFlag -eq 0; then
  curlOptions="$curlOptions --silent"
fi

# get wget options
wgetOptions=$WGET_OPTIONS
if test $verboseFlag -eq 0; then
  wgetOptions="$wgetOptions --no-verbose"
fi

# create directory
install -d "$destination/extern"

#trap 'abort' 1
#trap 'abort' 0
#set -e

# run
cwd=`pwd`
if test $cleanFlag -eq 0; then
  # download

  if test $allFlag -eq 1 -o $zlibFlag -eq 1; then
    # zlib
    $ECHO_NO_NEW_LINE "Get zlib..."
    (
     cd "$destination/extern"
     fileName=`ls zlib-*.tar.gz 2>/dev/null|cat -`
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/zlib-*.tar.gz; then
         fileName=`ls $localDirectory/zlib-*.tar.gz 2>/dev/null|cat -`
       else
         fileName=`$CURL $CURL_OPTIONS --silent --output - 'http://www.zlib.net'|grep -E -e 'http://.*/zlib-.*\.tar\.gz'|head -1|sed 's|.*http://.*/\(.*\.tar\.gz\).*|\1|g'`
         url="http://www.zlib.net/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT `find extern -type d -name "zlib-*"` zlib)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $bzip2Flag -eq 1; then
    # bzip2
    $ECHO_NO_NEW_LINE "Get bzip2..."
    (
     cd "$destination/extern"
     fileName=bzip2-$BZIP2_VERSION.tar.gz
     url="https://sourceware.org/pub/bzip2/$fileName"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/bzip2-$BZIP2_VERSION.tar.gz; then
         fileName=$localDirectory/bzip2-$BZIP2_VERSION.tar.gz
       else
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/bzip2-$BZIP2_VERSION bzip2)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $lzmaFlag -eq 1; then
    # lzma
    $ECHO_NO_NEW_LINE "Get lzma..."
    (
     cd "$destination/extern"
     fileName=`ls xz-*.tar.gz 2>/dev/null|cat -`
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/xz-*.tar.gz; then
         fileName=`ls $localDirectory/xz-*.tar.gz 2>/dev/null|cat -`
       else
         fileName=`$CURL $CURL_OPTIONS --silent --output - 'http://tukaani.org/xz'|grep -E -e 'xz-.*\.tar\.gz'|head -1|sed 's|.*href="\(xz.*\.tar\.gz\)".*|\1|g'`
         url="http://tukaani.org/xz/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT `find extern -type d -name "xz-*"` xz)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $lzoFlag -eq 1; then
    # lzo
    $ECHO_NO_NEW_LINE "Get lzo..."
    (
     cd "$destination/extern"
     fileName=lzo-$LZO_VERSION.tar.gz
     url="http://www.oberhumer.com/opensource/lzo/download/$fileName"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/lzo-$LZO_VERSION.tar.gz; then
         fileName=$localDirectory/lzo-$LZO_VERSION.tar.gz
       else
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/lzo-$LZO_VERSION lzo)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $lz4Flag -eq 1; then
    # lz4
    $ECHO_NO_NEW_LINE "Get lz4..."
    (
     cd "$destination/extern"
     fileName=`ls lz4-*.tar.gz 2>/dev/null|cat -`
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/lz4-*.tar.gz; then
         fileName=`ls $localDirectory/lz4-*.tar.gz 2>/dev/null|cat -`
       else
#         url=`$CURL $curlOptions --silent --output - 'http://code.google.com/p/lz4'|grep -E -e 'lz4-.*\.tar\.gz'|head -1|sed 's|.*"\(http.*/lz4-.*\.tar\.gz\)".*|\1|g'`
#
#         fileName=`echo $URL|sed 's|.*/\(lz4-.*\.tar\.gz\).*|\1|g'`
#         $CURL $curlOptions -O "$url"
         fileName="lz4-$LZ4_VERSION.tar.gz"
         url="https://github.com/Cyan4973/lz4/archive/$LZ4_VERSION.tar.gz"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT `find extern -type d -name "lz4-*"` lz4)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $zstdFlag -eq 1; then
    # zstd
    $ECHO_NO_NEW_LINE "Get zstd..."
    (
     cd "$destination/extern"
     fileName=`ls zstd-*.zip 2>/dev/null|cat -`
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/zstd-*.zip; then
         fileName=`ls $localDirectory/zstd-*.zip 2>/dev/null|cat -`
       else
#         url=`$CURL $curlOptions --silent --output - 'http://code.google.com/p/zstd'|grep -E -e 'zstd-.*\.tar\.gz'|head -1|sed 's|.*"\(http.*/zstd-.*\.tar\.gz\)".*|\1|g'`
#
#         fileName=`echo $URL|sed 's|.*/\(zstd-.*\.tar\.gz\).*|\1|g'`
#         $CURL $curlOptions -O "$url"
         fileName="zstd-$ZSTD_VERSION.zip"
         url="https://github.com/facebook/zstd/archive/v$ZSTD_VERSION.zip"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT `find extern -type d -name "zstd-*"` zstd)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $xdelta3Flag -eq 1; then
    # xdelta3
    $ECHO_NO_NEW_LINE "Get xdelta3..."
    (
     cd "$destination/extern"
     fileName=xdelta3-$XDELTA3_VERSION.tar.gz
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/xdelta3-$XDELTA3_VERSION.tar.gz; then
         fileName=$localDirectory/xdelta3-$XDELTA3_VERSION.tar.gz
       else
         url="https://github.com/jmacd/xdelta-gpl/releases/download/v$XDELTA3_VERSION/$fileName"
         if test ! -f $fileName; then
           $CURL $curlOptions --output $fileName $url
           if test $? -ne 0; then
             fatalError "$url -> $fileName"
           fi
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT `find extern -maxdepth 1 -type d -name "xdelta3-*"` xdelta3)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    # gpg-error, gcrypt
    $ECHO_NO_NEW_LINE "Get gpg-error, gcrypt..."
    (
     cd "$destination/extern"
     fileName=libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2; then
         fileName=$localDirectory/libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2
       else
         url="https://www.gnupg.org/ftp/gcrypt/libgpg-error/$fileName"
         if test ! -f $fileName; then
           $CURL $curlOptions --output $fileName $url
           if test $? -ne 0; then
             fatalError "$url -> $fileName"
           fi
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
     fi

     fileName=libgcrypt-$LIBGCRYPT_VERSION.tar.bz2
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/libgcrypt-$LIBGCRYPT_VERSION.tar.bz2; then
         fileName=$localDirectory/libgcrypt-$LIBGCRYPT_VERSION.tar.bz2
       else
         url="https://www.gnupg.org/ftp/gcrypt/libgcrypt/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/libgpg-error-$LIBGPG_ERROR_VERSION libgpg-error)
      (cd "$destination"; $LN -sfT extern/libgcrypt-$LIBGCRYPT_VERSION libgcrypt)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $curlFlag -eq 1; then
    # c-ares
    $ECHO_NO_NEW_LINE "Get c-ares..."
    (
     cd "$destination/extern"
     fileName=c-ares-$C_ARES_VERSION.tar.gz
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/c-ares-$C_ARES_VERSION.tar.gz; then
         fileName=$localDirectory/c-ares-$C_ARES_VERSION.tar.gz
       else
         url="http://c-ares.haxx.se/download/$fileName"
         if test ! -f $fileName; then
           $CURL $curlOptions --output $fileName $url
           if test $? -ne 0; then
             fatalError "$url -> $fileName"
           fi
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/c-ares-$C_ARES_VERSION c-ares)
    fi
    $ECHO "ok"

    # curl
    $ECHO_NO_NEW_LINE "Get curl..."
    (
     cd "$destination/extern"
     fileName=curl-$CURL_VERSION.tar.bz2
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/curl-$CURL_VERSION.tar.bz2; then
         fileName=$localDirectory/curl-$CURL_VERSION.tar.bz2
       else
         url="http://curl.haxx.se/download/$fileName"
         if test ! -f $fileName; then
           $CURL $curlOptions --output $fileName $url
           if test $? -ne 0; then
             fatalError "$url -> $fileName"
           fi
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/curl-$CURL_VERSION curl)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $mxmlFlag -eq 1; then
    # mxml
    $ECHO_NO_NEW_LINE "Get mxml..."
    (
     cd "$destination/extern"
     fileName=mxml-$MXML_VERSION.zip
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/mxml-$MXML_VERSION.zip; then
         fileName=$localDirectory/mxml-$MXML_VERSION.zip
       else
         url="https://github.com/michaelrsweet/mxml/archive/v$MXML_VERSION.zip"
         if test ! -f $fileName; then
           $CURL $curlOptions --output $fileName $url
           if test $? -ne 0; then
             fatalError "$url -> $fileName"
           fi
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/mxml-$MXML_VERSION mxml)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $opensslFlag -eq 1; then
    # openssl 1.0.1g
    $ECHO_NO_NEW_LINE "Get openssl..."
    (
     cd "$destination/extern"
     fileName=openssl-$OPENSSL_VERSION.tar.gz
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/openssl-$OPENSSL_VERSION.tar.gz; then
         fileName=$localDirectory/openssl-$OPENSSL_VERSION.tar.gz
       else
         url="http://www.openssl.org/source/$fileName"
         if test ! -f $fileName; then
           $CURL $curlOptions --output $fileName $url
           if test $? -ne 0; then
             fatalError "$url -> $fileName"
           fi
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/openssl-$OPENSSL_VERSION openssl)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $libssh2Flag -eq 1; then
    # libssh2
    $ECHO_NO_NEW_LINE "Get libssh2..."
    (
     cd "$destination/extern"
     fileName=libssh2-$LIBSSH2_VERSION.tar.gz
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/libssh2-$LIBSSH2_VERSION.tar.gz; then
         fileName=$localDirectory/libssh2-$LIBSSH2_VERSION.tar.gz
       else
         url="http://www.libssh2.org/download/$fileName"
         if test ! -f $fileName; then
           $CURL $curlOptions --output $fileName $url
           if test $? -ne 0; then
             fatalError "$url -> $fileName"
           fi
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/libssh2-$LIBSSH2_VERSION libssh2)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    # nettle
    $ECHO_NO_NEW_LINE "Get nettle..."
    (
     cd "$destination/extern"
     fileName=nettle-$NETTLE_VERSION.tar.gz
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/nettle-$NETTLE_VERSION.tar.gz; then
         fileName=$localDirectory/nettle-$NETTLE_VERSION.tar.gz
       else
         url="https://ftp.gnu.org/gnu/nettle/$fileName"
         if test ! -f $fileName; then
           $CURL $curlOptions --output $fileName $url
           if test $? -ne 0; then
             fatalError "$url -> $fileName"
           fi
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/nettle-$NETTLE_VERSION nettle)
    fi
    $ECHO "ok"

    # gmp
    $ECHO_NO_NEW_LINE "Get gmp..."
    (
     cd "$destination/extern"
     fileName=gmp-$GMP_VERSION.tar.xz
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/gmp-$GMP_VERSION.tar.xz; then
         fileName=$localDirectory/gmp-$GMP_VERSION.tar.xz
       else
         url="https://gmplib.org/download/gmp/$fileName"
         if test ! -f $fileName; then
           $CURL $curlOptions --output $fileName $url
           if test $? -ne 0; then
             fatalError "$url -> $fileName"
           fi
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xJf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT `find extern -type d -name "gmp-*"` gmp)
    fi
    $ECHO "ok"

    # libidn2
    $ECHO_NO_NEW_LINE "Get libidn2..."
    (
     cd "$destination/extern"
     fileName=libidn2-$LIBIDN2_VERSION.tar.gz
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/libidn2-$LIBIDN2_VERSION.tar.gz; then
         fileName=$localDirectory/libidn2-$LIBIDN2_VERSION.tar.gz
       else
         url="https://ftp.gnu.org/gnu/libidn/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT `find extern -type d -name "libidn2-*"` libidn2)
    fi
    $ECHO "ok"

    # gnutls
    $ECHO_NO_NEW_LINE "Get gnutls..."
    (
     cd "$destination/extern"
     fileName=gnutls-$GNU_TLS_VERSION.tar.xz
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/gnutls-$GNU_TLS_VERSION.tar.xz; then
         fileName=$localDirectory/gnutls-$GNU_TLS_VERSION.tar.xz
       else
         url="https://www.gnupg.org/ftp/gcrypt/gnutls/$GNU_TLS_SUB_DIRECTORY/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $XZ -d -c $fileName | $TAR xf -
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/gnutls-$GNU_TLS_VERSION gnutls)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $libcdioFlag -eq 1; then
    # libiconv
    $ECHO_NO_NEW_LINE "Get libiconv..."
    (
     cd "$destination/extern"
     fileName=libiconv-$LIBICONV_VERSION.tar.gz
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/libiconv-$LIBICONV_VERSION.tar.gz; then
         fileName=$localDirectory/libiconv-$LIBICONV_VERSION.tar.gz
       else
         url="https://ftp.gnu.org/pub/gnu/libiconv/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/libiconv-$LIBICONV_VERSION libiconv)
    fi
    $ECHO "ok"

    # libcdio
    $ECHO_NO_NEW_LINE "Get libcdio..."
    (
     cd "$destination/extern"
     fileName=libcdio-$LIBCDIO_VERSION.tar.bz2
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/libcdio-$LIBCDIO_VERSION.tar.bz2; then
         fileName=$localDirectory/libcdio-$LIBCDIO_VERSION.tar.bz2
       else
         url="https://ftp.gnu.org/gnu/libcdio/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/libcdio-$LIBCDIO_VERSION libcdio)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $mtxFlag -eq 1; then
    # mtx
    $ECHO_NO_NEW_LINE "Get mtx..."
    (
     cd "$destination/extern"
     fileName=mtx-$MTX_VERSION.tar.gz
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/mtx-$MTX_VERSION.tar.gz; then
         fileName=$localDirectory/mtx-$MTX_VERSION.tar.gz
       else
         url="http://sourceforge.net/projects/mtx/files/mtx-stable/$MTX_VERSION/$fileName"
         if test ! -f $fileName; then
           $CURL $curlOptions --output $fileName $url
           if test $? -ne 0; then
             fatalError "$url -> $fileName"
           fi
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/mtx-$MTX_VERSION mtx)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $pcreFlag -eq 1; then
    # pcre
    $ECHO_NO_NEW_LINE "Get pcre..."
    (
     cd "$destination/extern"
     fileName=pcre-$PCRE_VERSION.tar.bz2
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/pcre-$PCRE_VERSION.zip; then
         fileName=$localDirectory/pcre-$PCRE_VERSION.zip
       else
         url="https://ftp.pcre.org/pub/pcre/$fileName"
         url="https://downloads.sourceforge.net/project/pcre/pcre/$PCRE_VERSION/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/pcre-$PCRE_VERSION pcre)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $sqliteFlag -eq 1; then
    # sqlite
    $ECHO_NO_NEW_LINE "Get sqlite..."
    (
     cd "$destination/extern"
     fileName=sqlite-src-$SQLITE_VERSION.zip
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/sqlite-src-$SQLITE_VERSION.zip; then
         fileName=$localDirectory/sqlite-src-$SQLITE_VERSION.zip
       else
         url="https://www.sqlite.org/$SQLITE_YEAR/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/sqlite-src-$SQLITE_VERSION sqlite)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $icuFlag -eq 1; then
    # icu
    $ECHO_NO_NEW_LINE "Get icu..."
    (
     cd "$destination/extern"
     fileName=icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz; then
         fileName=$localDirectory/icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz
       else
         url="https://github.com/unicode-org/icu/releases/download/release-`echo $ICU_VERSION|sed 's/\./-/g'`/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/icu icu)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $binutilsFlag -eq 1; then
    # binutils
    $ECHO_NO_NEW_LINE "Get binutils..."
    (
     cd "$destination/extern"
     fileName=binutils-$BINUTILS_VERSION.tar.bz2
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/binutils-$BINUTILS_VERSION.tar.bz2; then
         fileName=$localDirectory/binutils-$BINUTILS_VERSION.tar.bz2
       else
         url="http://ftp.gnu.org/gnu/binutils/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/binutils-$BINUTILS_VERSION binutils)
    fi
    $ECHO "ok"
  fi

  if test $allFlag -eq 1 -o $pthreadsW32Flag -eq 1; then
    # pthreads-w32 2.9.1
    $ECHO_NO_NEW_LINE "Get pthreads..."
    (
     cd "$destination/extern"
     fileName=pthreads-w32-2-9-1-release.tar.gz
     url="ftp://sourceware.org/pub/pthreads-win32/$fileName"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/pthreads-w32-2-9-1-release.tar.gz; then
         fileName=$localDirectory/pthreads-w32-2-9-1-release.tar.gz
       else
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/pthreads-w32-2-9-1-release pthreads-w32)
    fi
    $ECHO "ok"
  fi

  if test $breakpadFlag -eq 1; then
    # breakpad
    $ECHO_NO_NEW_LINE "Get breakpad..."
    (
     cd "$destination/extern"
     fileName=breakpad
     if test ! -d $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/breakpad; then
         fileName=$localDirectory/breakpad
       else
         $ECHO_NO_NEW_LINE "Checkout 'http://google-breakpad.googlecode.com/svn/trunk', revision $BREAKPAD_REVISION..."
         $SVN checkout 'http://google-breakpad.googlecode.com/svn/trunk' $fileName -r$BREAKPAD_REVISION >/dev/null
         $ECHO "done"
       fi
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/breakpad breakpad)
    fi
    $ECHO "ok"
  fi

  if test $epmFlag -eq 1; then
    # epm
    $ECHO_NO_NEW_LINE "Get epm..."
    (
     cd "$destination/extern"
     fileName=epm-$EPM_VERSION-source.tar.bz2
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/epm-$EPM_VERSION-source.tar.bz2; then
         fileName=$localDirectory/epm-$EPM_VERSION-source.tar.bz2
       else
         url="http://www.msweet.org/files/project2/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/epm-$EPM_VERSION epm)
    fi
    $ECHO "ok"
  fi

  if test $launch4jFlag -eq 1; then
    # launchj4
    $ECHO_NO_NEW_LINE "Get launchj4..."
    (
     cd "$destination/extern"
     fileName=launch4j-3.1.0-beta2-linux.tgz
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/launch4j-3.1.0-beta2-linux.tgz; then
         fileName=$localDirectory/launch4j-3.1.0-beta2-linux.tgz
       else
         url="http://downloads.sourceforge.net/project/launch4j/launch4j-3/3.1.0-beta2/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/launch4j launch4j)
    fi
    $ECHO "ok"
  fi

  if test $jreWindowsFlag -eq 1; then
    # Windows JRE from OpenJDK 6
    $ECHO_NO_NEW_LINE "Get OpenJDK..."
    (
     cd "$destination/extern"
     fileName=openjdk-1.6.0-unofficial-b30-windows-i586-image.zip
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/openjdk-1.6.0-unofficial-b30-windows-i586-image.zip; then
         fileName=$localDirectory/openjdk-1.6.0-unofficial-b30-windows-i586-image.zip
       else
         url="https://bitbucket.org/alexkasko/openjdk-unofficial-builds/downloads/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName 'openjdk-1.6.0-unofficial-b30-windows-i586-image/jre/*'
     fi

     fileName=openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip; then
         fileName=$localDirectory/openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip
       else
         url="https://bitbucket.org/alexkasko/openjdk-unofficial-builds/downloads/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
       fi
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName 'openjdk-1.6.0-unofficial-b30-windows-amd64-image/jre/*'
     fi
    )
    if test $? -ne 0; then
      exit $?
    fi
    if test $noDecompressFlag -eq 0; then
      (cd "$destination"; $LN -sfT extern/openjdk-1.6.0-unofficial-b30-windows-i586-image/jre jre_windows)
      (cd "$destination"; $LN -sfT extern/openjdk-1.6.0-unofficial-b30-windows-amd64-image/jre jre_windows_64)
    fi
    $ECHO "ok"
  fi
else
  # clean

  if test $allFlag -eq 1 -o $zlibFlag -eq 1; then
    # zlib
    (
      cd "$destination"
      $RMF extern/zlib-*.tar.gz
      $RMRF extern/zlib-*
    )
    $RMF zlib
  fi

  if test $allFlag -eq 1 -o $bzip2Flag -eq 1; then
    # bzip2
    (
      cd "$destination"
      $RMF extern/bzip2-*.tar.gz
      $RMRF extern/bzip2-*
    )
    $RMF bzip2
  fi

  if test $allFlag -eq 1 -o $lzmaFlag -eq 1; then
    # lzma
    (
      cd "$destination"
      $RMF `find extern -type f -name "xz-*.tar.gz" 2>/dev/null`
      $RMRF `find extern -type d -name "xz-*" 2>/dev/null`
    )
    $RMF xz
  fi

  if test $allFlag -eq 1 -o $lzoFlag -eq 1; then
    # lzo
    (
      cd "$destination"
      $RMF extern/lzo-*.tar.gz
      $RMRF extern/lzo-*
    )
    $RMF lzo
  fi

  if test $allFlag -eq 1 -o $lz4Flag -eq 1; then
    # lz4
    (
      cd "$destination"
      $RMF extern/lz4*.tar.gz
      $RMRF extern/lz4*
    )
    $RMF lz4
  fi

  if test $allFlag -eq 1 -o $zstdFlag -eq 1; then
    # zstd
    (
      cd "$destination"
      $RMF extern/zstd*.zip
      $RMRF extern/zstd*
    )
    $RMF zstd
  fi

  if test $allFlag -eq 1 -o $xdelta3Flag -eq 1; then
    # xdelta3
    (
      cd "$destination"
      $RMF `find extern -type f -name "xdelta3-*.tar.gz" 2>/dev/null`
      $RMRF `find extern -type d -name "xdelta3-*" 2>/dev/null`
    )
    $RMF xdelta3
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    # gcrypt
    (
      cd "$destination"
      $RMF extern/libgpg-error-*.tar.bz2 extern/libgcrypt-*.tar.bz2
      $RMRF extern/libgpg-error-* extern/libgcrypt-*
    )
    $RMF libgpg-error libgcrypt
  fi

  if test $allFlag -eq 1 -o $curlFlag -eq 1; then
    # curl
    (
      cd "$destination"
      $RMF extern/curl-*-.tar.bz2
      $RMRF extern/curl-*
    )
    $RMF curl

    # c-areas
    (
      cd "$destination"
      $RMF extern/c-ares-*-.tar.gz
      $RMRF extern/c-ares-*
    )
    $RMF c-ares
  fi

  if test $allFlag -eq 1 -o $mxmlFlag -eq 1; then
    # mxml
    (
      cd "$destination"
      $RMF extern/mxml-*-.tar.bz2
      $RMRF extern/mxml-*
    )
    $RMF mxml
  fi

  if test $allFlag -eq 1 -o $opensslFlag -eq 1; then
    # openssl
    (
      cd "$destination"
      $RMF extern/openssl*.tar.gz
      $RMRF extern/openssl*
    )
    $RMF openssl
  fi

  if test $allFlag -eq 1 -o $libssh2Flag -eq 1; then
    # libssh2
    (
      cd "$destination"
      $RMF extern/libssh2*.tar.gz
      $RMRF extern/libssh2*
    )
    $RMF libssh2
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    # gnutls
    (
      cd "$destination"
      $RMF extern/gnutls-*.tar.bz2
      $RMRF extern/gnutls-*
    )
    $RMF gnutls

    # libidn2
    (
      cd "$destination"
      $RMF extern/libidn2-*.tar.gz
      $RMRF extern/libidn2-*
    )
    $RMF gmp

    # gmp
    (
      cd "$destination"
      $RMF extern/gmp-*.tar.bz2
      $RMRF extern/gmp-*
    )
    $RMF gmp

    # nettle
    (
      cd "$destination"
      $RMF extern/nettle-*.tar.bz2
      $RMRF extern/nettle-*
    )
    $RMF nettle
  fi

  if test $allFlag -eq 1 -o $libcdioFlag -eq 1; then
    # libiconv
    (
      cd "$destination"
      $RMF extern/libiconv-*.tar.gz
      $RMRF extern/libiconv-*
    )
    $RMF libiconv

    # libcdio
    (
      cd "$destination"
      $RMF extern/libcdio-*.tar.gz
      $RMRF extern/libcdio-*
    )
    $RMF libcdio
  fi

  if test $allFlag -eq 1 -o $mtxFlag -eq 1; then
    # mtx
    (
      cd "$destination"
      $RMRF extern/mtx-*
    )
    $RMF mtx
  fi

  if test $allFlag -eq 1 -o $pcreFlag -eq 1; then
    # pcre
    (
      cd "$destination"
      $RMRF extern/pcre-*
    )
    $RMF pcre
  fi

  if test $allFlag -eq 1 -o $sqliteFlag -eq 1; then
    # sqlite
    (
      cd "$destination"
      $RMRF extern/sqlite-*
    )
    $RMF sqlite
  fi

  if test $allFlag -eq 1 -o $icuFlag -eq 1; then
    # icu
    (
      cd "$destination"
      $RMRF extern/icu4c-*
      $RMRF extern/icu
    )
    $RMF icu
  fi

  if test $allFlag -eq 1 -o $binutilsFlag -eq 1; then
    # binutils
    (
      cd "$destination"
      $RMRF extern/binutils-*
      $RMRF extern/binutils
    )
    $RMF binutils
  fi

  if test $allFlag -eq 1 -o $pthreadsW32Flag -eq 1; then
    # pthreadW32
    (
      cd "$destination"
      $RMRF extern/pthreads-w32-*
    )
    $RMF pthreads-w32
  fi

  if test $allFlag -eq 1 -o $breakpadFlag -eq 1; then
    # breakpad
    (
      cd "$destination"
      $RMRF extern/breakpad
    )
    $RMF breakpad
  fi

  if test $allFlag -eq 1 -o $epmFlag -eq 1; then
    # epm
    (
      cd "$destination"
      $RMF extern/epm-*.tar.bz2
      $RMRF extern/epm-*
    )
    $RMF epm
  fi

  if test $allFlag -eq 1 -o $launch4jFlag -eq 1; then
    # launch4j
    (
      cd "$destination"
      $RMF extern/launch4j-*.tgz
      $RMRF extern/launch4j
    )
    $RMF launch4j
  fi

  if test $jreWindowsFlag -eq 1; then
    # Windows JRE
    (
      cd "$destination"
      $RMF extern/openjdk-*.zip
      $RMRF extern/openjdk-*
    )
    $RMF jre_windows jre_windows_64
  fi
fi

exit 0
