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
CURL_OPTIONS="-L --retry 5 --connect-timeout 60 --max-time 300 --insecure"
CP="cp"
ECHO="echo"
ECHO_NO_NEW_LINE="echo -n"
GIT="git"
INSTALL="install"
LN="ln"
MKDIR="mkdir"
PATCH="patch"
RMF="rm -f"
RMRF="rm -rf"
TAR="tar"
UNZIP="unzip"
XZ="xz"

ZLIB_VERSION=1.3
BZIP2_VERSION=1.0.8
XZ_VERSION=5.2.5
LZO_VERSION=2.10
LZ4_VERSION=r131
ZSTD_VERSION=1.5.2
XDELTA3_VERSION=3.0.11
MXML_VERSION=3.3
LIBGPG_ERROR_VERSION=1.45
LIBGCRYPT_VERSION=1.10.1
NETTLE_VERSION=3.9.1
GMP_VERSION=6.2.1
LIBIDN2_VERSION=2.3.4
GNU_TLS_SUB_DIRECTORY=v3.6
GNU_TLS_VERSION=3.6.16
LIBICONV_VERSION=1.16
OPENSSL_VERSION=1.1.1n
LIBSSH2_VERSION=1.10.0
C_ARES_VERSION=1.20.1
CURL_VERSION=7.77.0
PCRE_VERSION=8.45
SQLITE_YEAR=2022
SQLITE_VERSION=3380200
#SQLITE_VERSION=3390400
MARIADB_CLIENT_VERSION=3.1.13
POSTGRESQL_VERSION=9.6.24
# Note ICU: * 61.1 seems to be the latest version without C++11
#           * 58.2 seems to be the latest version which can be
#              compiled on older 32bit systems, e. g. CentOS 6
ICU_VERSION=58.3
MTX_VERSION=1.3.12
LIBCDIO_VERSION=2.1.0
KRB5_VERSION=1.21
KRB5_VERSION_MINOR=2
LIBSMB2_VERSION=4.0.0
BINUTILS_VERSION=2.41
PTHREAD_W32_VERSION=2-9-1

# --------------------------------- variables --------------------------------

# ---------------------------------- functions -------------------------------

fatalError()
{
  message=$1; shift

  echo >&2 FAIL!
  echo >&2 ERROR: $message

  exit 4
}

# ------------------------------------ main ----------------------------------

# parse arguments
workingDirectory=$PWD
patchDirectory=$workingDirectory/misc
destinationDirectory=$PWD/extern
localDirectory=
noDecompressFlag=0
verboseFlag=0
cleanFlag=0
insecureFlag=0
helpFlag=0

allFlag=1
zlibFlag=0
bzip2Flag=0
xzFlag=0
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
libsmb2Flag=0
pcreFlag=0
sqlite3Flag=0
mariaDBFlag=0
postgreSQLFlag=0
icuFlag=0
mtxFlag=0
binutilsFlag=0
pthreadsW32Flag=0
breakpadFlag=0
launch4jFlag=0
jreWindowsFlag=0

while test $# != 0; do
  case $1 in
    -h | --help)
      helpFlag=1
      shift
      ;;
    -w | --working-directory)
      workingDirectory=`readlink -f "$2"`
      shift
      shift
      ;;
    -p | --patch-directory)
      patchDirectory=`readlink -f "$2"`
      shift
      shift
      ;;
    -d | --destination-directory)
      destinationDirectory="$2"
      shift
      shift
      ;;
    -l | --local-directory)
      localDirectory=`readlink -f "$2"`
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
    --insecure)
      insecureFlag=1
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
        xz | lzma)
          allFlag=0
          xzFlag=1
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
        smb2|libsmb2|smbclient|libsmbclient)
          allFlag=0
          libsmb2Flag=1
          ;;
        pcre)
          allFlag=0
          pcreFlag=1
          ;;
        sqlite|sqlite3)
          allFlag=0
          sqlite3Flag=1
          ;;
        mariadb)
          allFlag=0
          mariaDBFlag=1
          ;;
        postgresql)
          allFlag=0
          postgreSQLFlag=1
          ;;
        mtx)
          allFlag=0
          mtxFlag=1
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
    xz | lzma)
      allFlag=0
      xzFlag=1
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
    smb2|libsmb2|smbclient|libsmbclient)
      allFlag=0
      libsmb2Flag=1
      ;;
    pcre)
      allFlag=0
      pcreFlag=1
      ;;
    sqlite|sqlite3)
      allFlag=0
      sqlite3Flag=1
      ;;
    mariadb)
      allFlag=0
      mariaDBFlag=1
      ;;
    postgresql)
      allFlag=0
      postgreSQLFlag=1
      ;;
    mtx)
      allFlag=0
      mtxFlag=1
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
  $ECHO "         -p|--patch-directory <path>  - directory with patches"
  $ECHO "         -l|--local-directory <path>  - local directory to get packages from"
  $ECHO "         -n|--no-decompress           - do not decompress archives"
  $ECHO "         --insecure                   - disable SSL certificate checks"
  $ECHO "         --verbose                    - verbose output"
  $ECHO "         --no-verbose                 - disable verbose output"
  $ECHO "         -c|--clean                   - delete all packages in destination directory"
  $ECHO "         --help                       - print this help"
  $ECHO ""
  $ECHO "Packages (included in 'all'):"
  $ECHO ""
  $ECHO " zlib"
  $ECHO " bzip2"
  $ECHO " xz"
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
  $ECHO " libsmbclient"
  $ECHO " pcre"
  $ECHO " sqlite3"
  $ECHO " mariadb"
  $ECHO " postgresql"
  $ECHO " mtx"
  $ECHO " icu"
  $ECHO " binutils"
  $ECHO ""
  $ECHO "Additional optional packages:"
  $ECHO ""
  $ECHO " breakpad"
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
type $GIT 1>/dev/null 2>/dev/null && $GIT --version 1>/dev/null 2>/dev/null
if test $? -gt 0; then
  $ECHO >&2 "ERROR: command 'git' is not available"
  exit 1
fi
type $PATCH 1>/dev/null 2>/dev/null
if test $? -gt 10; then
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

# get curl options
curlOptions=$CURL_OPTIONS
if test $insecureFlag -eq 1; then
  curlOptions="$curlOptions --insecure"
fi
if test $verboseFlag -eq 0; then
  curlOptions="$curlOptions --silent"
fi

#trap 'abort' 1
#trap 'abort' 0
#set -e

# run
cwd=`pwd`
if test $cleanFlag -eq 0; then
  # download

  if test $allFlag -eq 1 -o $zlibFlag -eq 1; then
    # zlib
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get zlib ($ZLIB_VERSION)..."
     fileName="zlib-$ZLIB_VERSION.tar.gz"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/zlib-$ZLIB_VERSION.tar.gz; then
         $LN -s $localDirectory/zlib-$ZLIB_VERSION.tar.gz $fileName
         result=1
       else
         url="http://www.zlib.net/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "$url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT `find $destinationDirectory -maxdepth 1 -type d -name "zlib-*"` zlib)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi

       # patch to fix missing valgrind issue in zlib 1.3
       #   diff -u zlib-1.3.org/deflate.c zlib-1.3/deflate.c > ../misc/zlib-1.3-valgrind-issue.patch
       # Note: ignore exit code 1: patch may already be applied
       (cd $workingDirectory/zlib; $PATCH --batch -N -p1 < $patchDirectory/zlib-1.3-valgrind-issue.patch) 1>/dev/null
       if test $? -gt 1; then
         fatalError "patch"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $bzip2Flag -eq 1; then
    # bzip2
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get bzip2 ($BZIP2_VERSION)..."
     fileName="bzip2-$BZIP2_VERSION.tar.gz"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/bzip2-$BZIP2_VERSION.tar.gz; then
         $LN -s $localDirectory/bzip2-$BZIP2_VERSION.tar.gz $fileName
         result=1
       else
         url="https://sourceware.org/pub/bzip2/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/bzip2-$BZIP2_VERSION bzip2)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $xzFlag -eq 1; then
    # xz
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get xz ($XZ_VERSION)..."
     fileName="xz-$XZ_VERSION.tar.gz"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/xz-$XZ_VERSION.tar.gz; then
         $LN -s $localDirectory/xz-$XZ_VERSION.tar.gz $fileName
         result=1
       else
         url="https://tukaani.org/xz/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/xz-$XZ_VERSION xz)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $lzoFlag -eq 1; then
    # lzo
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get lzo ($LZO_VERSION)..."
     fileName="lzo-$LZO_VERSION.tar.gz"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/lzo-$LZO_VERSION.tar.gz; then
         $LN -s $localDirectory/lzo-$LZO_VERSION.tar.gz $fileName
         result=1
       else
         url="http://www.oberhumer.com/opensource/lzo/download/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/lzo-$LZO_VERSION lzo)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi

       # patch to fix missing --tag=CC for in libtool call
       #   diff -u extern/lzo-2.10.org/Makefile.in extern/lzo-2.10/Makefile.in  > misc/lzo-2.10-libtool.patch
       # Note: ignore exit code 1: patch may already be applied
       (cd $workingDirectory/lzo; $PATCH --batch -N -p1 < $patchDirectory/lzo-2.10-libtool.patch) 1>/dev/null
       if test $? -gt 1; then
         fatalError "patch"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $lz4Flag -eq 1; then
    # lz4
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get lz4 ($LZ4_VERSION)..."
     fileName="lz4-$LZ4_VERSION.tar.gz"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/lz4-$LZ4_VERSION.tar.gz; then
         $LN -s $localDirectory/lz4-$LZ4_VERSION.tar.gz $fileName
         result=1
       else
         url="https://github.com/Cyan4973/lz4/archive/$LZ4_VERSION.tar.gz"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT `find $destinationDirectory -maxdepth 1 -type d -name "lz4-*"` lz4)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $zstdFlag -eq 1; then
    # zstd
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get zstd ($ZSTD_VERSION)..."
     fileName="zstd-$ZSTD_VERSION.zip"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/zstd-$ZSTD_VERSION.zip; then
         $LN -s $localDirectory/zstd-$ZSTD_VERSION.zip $fileName
         result=1
       else
         url="https://github.com/facebook/zstd/archive/v$ZSTD_VERSION.zip"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT `find $destinationDirectory -maxdepth 1 -type d -name "zstd-*"` zstd)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $xdelta3Flag -eq 1; then
    # xdelta3
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get xdelta3 ($XDELTA3_VERSION)..."
     fileName="xdelta3-$XDELTA3_VERSION.tar.gz"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/xdelta3-$XDELTA3_VERSION.tar.gz; then
         $LN -s $localDirectory/xdelta3-$XDELTA3_VERSION.tar.gz $fileName
         result=1
       else
         url="https://github.com/jmacd/xdelta-gpl/releases/download/v$XDELTA3_VERSION/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT `find $destinationDirectory -maxdepth 1 -type d -name "xdelta3-*"` xdelta3)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi

       # patch to fix warnings:
       #   diff -Naur xdelta3-3.0.11.org xdelta3-3.0.11 > xdelta3-3.0.11.patch
       # Note: ignore exit code 1: patch may already be applied
       (cd $workingDirectory/xdelta3; $PATCH --batch -N -p1 < $patchDirectory/xdelta3-3.0.11.patch) 1>/dev/null
       if test $? -gt 1; then
         fatalError "patch"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get gpg-error ($LIBGPG_ERROR_VERSION)..."
     fileName="libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2; then
         $LN -s $localDirectory/libgpg-error-$LIBGPG_ERROR_VERSION.tar.bz2 $fileName
         result=1
       else
         url="https://www.gnupg.org/ftp/gcrypt/libgpg-error/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/libgpg-error-$LIBGPG_ERROR_VERSION libgpg-error)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac

    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get gcrypt ($LIBGCRYPT_VERSION)..."
     fileName="libgcrypt-$LIBGCRYPT_VERSION.tar.bz2"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/libgcrypt-$LIBGCRYPT_VERSION.tar.bz2; then
         $LN -s $localDirectory/libgcrypt-$LIBGCRYPT_VERSION.tar.bz2 $fileName
         result=1
       else
         url="https://www.gnupg.org/ftp/gcrypt/libgcrypt/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/libgcrypt-$LIBGCRYPT_VERSION libgcrypt)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $curlFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get c-ares ($C_ARES_VERSION)..."
     fileName="c-ares-$C_ARES_VERSION.tar.gz"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/c-ares-$C_ARES_VERSION.tar.gz; then
         $LN -s $localDirectory/c-ares-$C_ARES_VERSION.tar.gz $fileName
         result=1
       else
         url="http://c-ares.haxx.se/download/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/c-ares-$C_ARES_VERSION c-ares)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    if test $noDecompressFlag -eq 0; then
      (cd "$workingDirectory"; $LN -sfT extern/curl-$CURL_VERSION curl)
    fi
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $mxmlFlag -eq 1; then
    # mxml
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get mxml ($MXML_VERSION)..."
     fileName="mxml-$MXML_VERSION.zip"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/mxml-$MXML_VERSION.zip; then
         $LN -s $localDirectory/mxml-$MXML_VERSION.zip $fileName
         result=1
       else
         url="https://github.com/michaelrsweet/mxml/archive/v$MXML_VERSION.zip"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi
     fi

     exit $result
    )
    result=$?
    if test $noDecompressFlag -eq 0; then
      (cd "$workingDirectory"; $LN -sfT extern/mxml-$MXML_VERSION mxml)
    fi
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $opensslFlag -eq 1; then
    # openssl
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get openssl ($OPENSSL_VERSION)..."
     fileName="openssl-$OPENSSL_VERSION.tar.gz"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/openssl-$OPENSSL_VERSION.tar.gz; then
         $LN -s $localDirectory/openssl-$OPENSSL_VERSION.tar.gz $fileName
         result=1
       else
         url="http://www.openssl.org/source/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi
     fi

     exit $result
    )
    result=$?
    if test $noDecompressFlag -eq 0; then
      (cd "$workingDirectory"; $LN -sfT extern/openssl-$OPENSSL_VERSION openssl)
    fi
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $libssh2Flag -eq 1; then
    # libssh2
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get libssh2 ($LIBSSH2_VERSION)..."
     fileName="libssh2-$LIBSSH2_VERSION.tar.gz"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/libssh2-$LIBSSH2_VERSION.tar.gz; then
         $LN -s $localDirectory/libssh2-$LIBSSH2_VERSION.tar.gz $fileName
         result=1
       else
         url="http://www.libssh2.org/download/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/libssh2-$LIBSSH2_VERSION libssh2)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi

       # patch for memory leak for libssh2 1.8.2 (ignore errors):
       #   diff -u libssh2-1.8.2.org/src libssh2-1.8.2/src > libssh2-1.8.2-memory-leak.patch
       # Note: ignore exit code 1: patch may already be applied
#         (cd $workingDirectory/libssh2; $PATCH --batch -N -p1 < $patchDirectory/libssh2-1.8.2-memory-leak.patch) 1>/dev/null
#         if test $? -gt 1; then
#           fatalError "patch"
#         fi
     fi

     exit $result
    )
    result=$?
    if test $noDecompressFlag -eq 0; then
      (cd "$workingDirectory"; $LN -sfT extern/libssh2-$LIBSSH2_VERSION libssh2)
    fi
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    # nettle
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get nettle ($NETTLE_VERSION)..."
     fileName="nettle-$NETTLE_VERSION.tar.gz"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/nettle-$NETTLE_VERSION.tar.gz; then
         $LN -s $localDirectory/nettle-$NETTLE_VERSION.tar.gz $fileName
         result=1
       else
         url="https://ftp.gnu.org/gnu/nettle/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi
     fi

     exit $result
    )
    result=$?
    if test $noDecompressFlag -eq 0; then
      (cd "$workingDirectory"; $LN -sfT extern/nettle-$NETTLE_VERSION nettle)
    fi
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac

    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get libidn2 ($LIBIDN2_VERSION)..."
     fileName="libidn2-$LIBIDN2_VERSION.tar.gz"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/libidn2-$LIBIDN2_VERSION.tar.gz; then
         $LN -s $localDirectory/libidn2-$LIBIDN2_VERSION.tar.gz $fileName
         result=1
       else
         url="https://ftp.gnu.org/gnu/libidn/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT `find $destinationDirectory -maxdepth 1 -type d -name "libidn2-*"` libidn2)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac

    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get curl ($CURL_VERSION)..."
     fileName="curl-$CURL_VERSION.tar.bz2"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/curl-$CURL_VERSION.tar.bz2; then
         $LN -s $localDirectory/curl-$CURL_VERSION.tar.bz2 $fileName
         result=1
       else
         url="http://curl.haxx.se/download/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/curl-$CURL_VERSION curl)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get nettle ($NETTLE_VERSION)..."
     fileName="nettle-$NETTLE_VERSION.tar.gz"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/nettle-$NETTLE_VERSION.tar.gz; then
         $LN -s $localDirectory/nettle-$NETTLE_VERSION.tar.gz $fileName
         result=1
       else
         url="https://ftp.gnu.org/gnu/nettle/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/nettle-$NETTLE_VERSION nettle)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac

    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get gmp ($GMP_VERSION)..."
     fileName="gmp-$GMP_VERSION.tar.xz"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/gmp-$GMP_VERSION.tar.xz; then
         $LN -s $localDirectory/gmp-$GMP_VERSION.tar.xz $fileName
         result=1
       else
         url="https://gmplib.org/download/gmp/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xJf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT `find $destinationDirectory -maxdepth 1 -type d -name "gmp-*"` gmp)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac

    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get libidn2 ($LIBIDN2_VERSION)..."
     fileName="libidn2-$LIBIDN2_VERSION.tar.gz"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/libidn2-$LIBIDN2_VERSION.tar.gz; then
         $LN -s $localDirectory/libidn2-$LIBIDN2_VERSION.tar.gz $fileName
         result=1
       else
         url="https://ftp.gnu.org/gnu/libidn/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT `find $destinationDirectory -maxdepth 1 -type d -name "libidn2-*"` libidn2)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac

    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get gnutls ($GNU_TLS_VERSION)..."
     fileName="gnutls-$GNU_TLS_VERSION.tar.xz"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/gnutls-$GNU_TLS_VERSION.tar.xz; then
         $LN -s $localDirectory/gnutls-$GNU_TLS_VERSION.tar.xz $fileName
         result=1
       else
         url="https://www.gnupg.org/ftp/gcrypt/gnutls/$GNU_TLS_SUB_DIRECTORY/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $XZ -d -c $fileName | $TAR xf -
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/gnutls-$GNU_TLS_VERSION gnutls)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $libcdioFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get libiconv ($LIBICONV_VERSION)..."
     fileName="libiconv-$LIBICONV_VERSION.tar.gz"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/libiconv-$LIBICONV_VERSION.tar.gz; then
         $LN -s $localDirectory/libiconv-$LIBICONV_VERSION.tar.gz $fileName
         result=1
       else
         url="https://ftp.gnu.org/pub/gnu/libiconv/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/libiconv-$LIBICONV_VERSION libiconv)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac

    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get libcdio ($LIBCDIO_VERSION)..."
     fileName="libcdio-$LIBCDIO_VERSION.tar.bz2"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/libcdio-$LIBCDIO_VERSION.tar.bz2; then
         $LN -s $localDirectory/libcdio-$LIBCDIO_VERSION.tar.bz2 $fileName
         result=1
       else
         url="https://ftp.gnu.org/gnu/libcdio/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/libcdio-$LIBCDIO_VERSION libcdio)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $libsmb2Flag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get Kerberos 5 ($KRB5_VERSION.$KRB5_VERSION_MINOR)..."
     fileName="krb5-$KRB5_VERSION.$KRB5_VERSION_MINOR.tar.gz"
     if test ! -f "$fileName"; then
       if test -n "$localDirectory" -a -f $localDirectory/$fileName; then
         $LN -s $localDirectory/$fileName $fileName
         result=1
       else
         url="https://kerberos.org/dist/krb5/$KRB5_VERSION/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT `find $destinationDirectory -maxdepth 1 -type d -name "krb5-*"` krb5)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac

    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get libsmb2 ($LIBSMB2_VERSION)..."
     directoryName="libsmb2-$LIBSMB2_VERSION"
     if test ! -d $directoryName; then
       if test -n "$localDirectory" -a -d $localDirectory/libsmb2-$LIBSMB2_VERSION; then
         # Note: make a copy to get usable file permissions (source may be owned by root)
         $CP -r $localDirectory/libsmb2-$LIBSMB2_VERSION $directoryName
         result=1
       else
         url="https://github.com/sahlberg/libsmb2"
         $GIT clone $url $directoryName 1>/dev/null 2>/dev/null
         if test $? -ne 0; then
           fatalError "checkout $url -> $directoryName"
         fi
         (cd $directoryName; \
          $GIT checkout v$LIBSMB2_VERSION 1>/dev/null 2>/dev/null; \
          install -d m4;
         )
         if test $? -ne 0; then
           fatalError "checkout tag v$LIBSMB2_VERSION"
         fi
         result=2
       fi
     else
       result=3
     fi

     if test $noDecompressFlag -eq 0; then
       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/libsmb2-$LIBSMB2_VERSION libsmb2)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi

       # patch to fix defintions for MinGW:
       #   diff -Naur libsmb2-4.0.0.org libsmb2-4.0.0 > libsmb2-4.0.0-mingw-definitions.patch
       # Note: ignore exit code 1: patch may already be applied
       (cd $workingDirectory/libsmb2; $PATCH --batch -N -p1 < $patchDirectory/libsmb2-4.0.0-mingw-definitions.patch) 1>/dev/null
       if test $? -gt 1; then
         fatalError "patch"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac

    if test $noDecompressFlag -eq 0; then
      (cd "$workingDirectory"; $LN -sfT extern/libsmb2-$LIBSMB2_VERSION libsmb2)
    fi
  fi

  if test $allFlag -eq 1 -o $pcreFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get pcre ($PCRE_VERSION)..."
     fileName="pcre-$PCRE_VERSION.tar.bz2"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/pcre-$PCRE_VERSION.tar.bz2; then
         $LN -s $localDirectory/pcre-$PCRE_VERSION.tar.bz2 $fileName
         result=1
       else
         url="https://downloads.sourceforge.net/project/pcre/pcre/$PCRE_VERSION/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/pcre-$PCRE_VERSION pcre)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $sqlite3Flag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get sqlite ($SQLITE_VERSION)..."
     fileName="sqlite-src-$SQLITE_VERSION.zip"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/sqlite-src-$SQLITE_VERSION.zip; then
         $LN -s $localDirectory/sqlite-src-$SQLITE_VERSION.zip $fileName
         result=1
       else
         url="https://www.sqlite.org/$SQLITE_YEAR/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/sqlite-src-$SQLITE_VERSION sqlite)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $mariaDBFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get MariaDB ($MARIADB_CLIENT_VERSION)..."
     fileName="mariadb-connector-c-$MARIADB_CLIENT_VERSION-src.tar.gz"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/mariadb-connector-c-$MARIADB_CLIENT_VERSION-src.tar.gz; then
         $LN -s $localDirectory/mariadb-connector-c-$MARIADB_CLIENT_VERSION-src.tar.gz $fileName
         result=1
       else
         url="https://archive.mariadb.org/connector-c-$MARIADB_CLIENT_VERSION/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/mariadb-connector-c-$MARIADB_CLIENT_VERSION-src mariadb-connector-c)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi

       # patch to fix compile with GNUTLS, fix signature mysql_load_plugin()
       #   diff -u -r mariadb-connector-c-3.1.13-src.org mariadb-connector-c-3.1.13-src > misc/mariadb-connector-c-3.1.13.patch
       # Note: ignore exit code 1: patch may already be applied
       (cd $workingDirectory/mariadb-connector-c; $PATCH --batch -N -p1 < $patchDirectory/mariadb-connector-c-3.1.13.patch) 1>/dev/null
       if test $? -gt 1; then
         fatalError "patch"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $postgreSQLFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get PostgreSQL ($POSTGRESQL_VERSION)..."
     fileName="postgresql-$POSTGRESQL_VERSION.tar.bz2"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/postgresql-$POSTGRESQL_VERSION.tar.bz2; then
         $LN -s $localDirectory/postgresql-$POSTGRESQL_VERSION.tar.bz2 $fileName
         result=1
       else
         url="https://ftp.postgresql.org/pub/source/v$POSTGRESQL_VERSION/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/postgresql-$POSTGRESQL_VERSION postgresql)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $mtxFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get mtx ($MTX_VERSION)..."
     fileName="mtx-$MTX_VERSION.tar.gz"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/mtx-$MTX_VERSION.tar.gz; then
         $LN -s $localDirectory/mtx-$MTX_VERSION.tar.gz $fileName
         result=1
       else
         url="http://sourceforge.net/projects/mtx/files/mtx-stable/$MTX_VERSION/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/mtx-$MTX_VERSION mtx)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $icuFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get icu ($ICU_VERSION)..."
     fileName="icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz; then
         $LN -s $localDirectory/icu4c-`echo $ICU_VERSION|sed 's/\./_/g'`-src.tgz $fileName
         result=1
       else
         url="https://github.com/unicode-org/icu/releases/download/release-`echo $ICU_VERSION|sed 's/\./-/g'`/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/icu icu)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi

       # patch to disable xlocale: does not exists anymore? not required?
       #   diff -u icu.org/source/i18n/digitlst.cpp icu/source/i18n/digitlst.cpp > icu-xlocale.patch
       # Note: ignore exit code 1: patch may already be applied
       (cd $workingDirectory/icu; $PATCH --batch -N -p1 < $patchDirectory/icu-xlocale.patch) 1>/dev/null
       if test $? -gt 1; then
         fatalError "patch"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $binutilsFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get binutils ($BINUTILS_VERSION)..."
     fileName="binutils-$BINUTILS_VERSION.tar.bz2"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/binutils-$BINUTILS_VERSION.tar.bz2; then
         $LN -s $localDirectory/binutils-$BINUTILS_VERSION.tar.bz2 $fileName
         result=1
       else
         url="http://ftp.gnu.org/gnu/binutils/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xjf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/binutils-$BINUTILS_VERSION binutils)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $allFlag -eq 1 -o $pthreadsW32Flag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get pthreads w32 ($PTHREAD_W32_VERSION)..."
     fileName="pthreads-w32-$PTHREAD_W32_VERSION-release.tar.gz"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/pthreads-w32-$PTHREAD_W32_VERSION-release.tar.gz; then
         $LN -s $localDirectory/pthreads-w32-$PTHREAD_W32_VERSION-release.tar.gz $fileName
         result=1
       else
         url="ftp://sourceware.org/pub/pthreads-win32/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/pthreads-w32-2-9-1-release pthreads-w32)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi

       # patch to fix missing include:
       #   diff -u pthreads-w32-2-9-1-release.org pthreads-w32-2-9-1-release > pthreads-w32-2-9-1-release.patch
       # Note: ignore exit code 1: patch may already be applied
       (cd $workingDirectory/pthreads-w32; $PATCH --batch -N -p1 < $patchDirectory/pthreads-w32-2-9-1-release.patch) 1>/dev/null
       if test $? -gt 1; then
         fatalError "patch"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $launch4jFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get launchj4..."
     fileName="launch4j-3.1.0-beta2-linux.tgz"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/launch4j-3.1.0-beta2-linux.tgz; then
         $LN -s $localDirectory/launch4j-3.1.0-beta2-linux.tgz $fileName
         result=1
       else
         url="http://downloads.sourceforge.net/project/launch4j/launch4j-3/3.1.0-beta2/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $TAR xzf $fileName
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/launch4j launch4j)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi

  if test $jreWindowsFlag -eq 1; then
    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get OpenJDK Windows..."
     fileName="openjdk-1.6.0-unofficial-b30-windows-i586-image.zip"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/openjdk-1.6.0-unofficial-b30-windows-i586-image.zip; then
         $LN -s $localDirectory/openjdk-1.6.0-unofficial-b30-windows-i586-image.zip $fileName
         result=1
       else
         url="https://bitbucket.org/alexkasko/openjdk-unofficial-builds/downloads/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=1
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName 'openjdk-1.6.0-unofficial-b30-windows-i586-image/jre/*'
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/openjdk-1.6.0-unofficial-b30-windows-i586-image/jre jre_windows)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac

    (
     install -d "$destinationDirectory"
     cd "$destinationDirectory"

     $ECHO_NO_NEW_LINE "Get OpenJDK Windows 64bit..."
     fileName="openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip"
     if test ! -f $fileName; then
       if test -n "$localDirectory" -a -f $localDirectory/openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip; then
         $LN -s $localDirectory/openjdk-1.6.0-unofficial-b30-windows-amd64-image.zip
         result=1
       else
         url="https://bitbucket.org/alexkasko/openjdk-unofficial-builds/downloads/$fileName"
         $CURL $curlOptions --output $fileName $url
         if test $? -ne 0; then
           fatalError "download $url -> $fileName"
         fi
         result=2
       fi
     else
       result=3
     fi
     if test $noDecompressFlag -eq 0; then
       $UNZIP -o -q $fileName 'openjdk-1.6.0-unofficial-b30-windows-amd64-image/jre/*'
       if test $? -ne 0; then
         fatalError "decompress"
       fi

       (cd "$workingDirectory"; $LN -sfT $destinationDirectory/openjdk-1.6.0-unofficial-b30-windows-amd64-image/jre jre_windows_64)
       if test $? -ne 0; then
         fatalError "symbolic link"
       fi
     fi

     exit $result
    )
    result=$?
    case $result in
      1) $ECHO "ok (local)"; ;;
      2) $ECHO "ok"; ;;
      3) $ECHO "ok (cached)"; ;;
      *) exit $result; ;;
    esac
  fi
else
  # clean

  if test $allFlag -eq 1 -o $zlibFlag -eq 1; then
    # zlib
    (
      cd "$destinationDirectory"
      $RMF zlib-*.tar.gz
      $RMRF zlib-*
    ) 2>/dev/null
    $RMF $workingDirectory/zlib
  fi

  if test $allFlag -eq 1 -o $bzip2Flag -eq 1; then
    # bzip2
    (
      cd "$destinationDirectory"
      $RMF bzip2-*.tar.gz
      $RMRF bzip2-*
    ) 2>/dev/null
    $RMF $workingDirectory/bzip2
  fi

  if test $allFlag -eq 1 -o $xzFlag -eq 1; then
    # xz
    (
      cd "$destinationDirectory"
      $RMF `find . -maxdepth 1 -type f -name "xz-*.tar.gz" 2>/dev/null`
      $RMRF `find . -maxdepth 1 -type d -name "xz-*" 2>/dev/null`
    ) 2>/dev/null
    $RMF $workingDirectory/xz
  fi

  if test $allFlag -eq 1 -o $lzoFlag -eq 1; then
    # lzo
    (
      cd "$destinationDirectory"
      $RMF lzo-*.tar.gz
      $RMRF lzo-*
    ) 2>/dev/null
    $RMF $workingDirectory/lzo
  fi

  if test $allFlag -eq 1 -o $lz4Flag -eq 1; then
    # lz4
    (
      cd "$destinationDirectory"
      $RMF lz4*.tar.gz
      $RMRF lz4*
    ) 2>/dev/null
    $RMF $workingDirectory/lz4
  fi

  if test $allFlag -eq 1 -o $zstdFlag -eq 1; then
    # zstd
    (
      cd "$destinationDirectory"
      $RMF zstd*.zip
      $RMRF zstd*
    ) 2>/dev/null
    $RMF $workingDirectory/zstd
  fi

  if test $allFlag -eq 1 -o $xdelta3Flag -eq 1; then
    # xdelta3
    (
      cd "$destinationDirectory"
      $RMF `find . -maxdepth 1 -type f -name "xdelta3-*.tar.gz" 2>/dev/null`
      $RMRF `find . -maxdepth 1 -type d -name "xdelta3-*" 2>/dev/null`
    ) 2>/dev/null
    $RMF $workingDirectory/xdelta3
  fi

  if test $allFlag -eq 1 -o $gcryptFlag -eq 1; then
    # gcrypt
    (
      cd "$destinationDirectory"
      $RMF libgpg-error-*.tar.bz2 libgcrypt-*.tar.bz2
      $RMRF libgpg-error-* libgcrypt-*
    ) 2>/dev/null
    $RMF $workingDirectory/libgpg-error libgcrypt
  fi

  if test $allFlag -eq 1 -o $curlFlag -eq 1; then
    # curl
    (
      cd "$destinationDirectory"
      $RMF curl-*-.tar.bz2
      $RMRF curl-*
    ) 2>/dev/null
    $RMF $workingDirectory/curl

    # libidn2
    (
      cd "$destinationDirectory"
      $RMF libidn2-*.tar.gz
      $RMRF libidn2-*
    ) 2>/dev/null
    $RMF $workingDirectory/libidn2

    # c-areas
    (
      cd "$destinationDirectory"
      $RMF c-ares-*-.tar.gz
      $RMRF c-ares-*
    ) 2>/dev/null
    $RMF $workingDirectory/c-ares
  fi

  if test $allFlag -eq 1 -o $mxmlFlag -eq 1; then
    # mxml
    (
      cd "$destinationDirectory"
      $RMF mxml-*-.tar.bz2
      $RMRF mxml-*
    ) 2>/dev/null
    $RMF $workingDirectory/mxml
  fi

  if test $allFlag -eq 1 -o $opensslFlag -eq 1; then
    # openssl
    (
      cd "$destinationDirectory"
      $RMF openssl*.tar.gz
      $RMRF openssl*
    ) 2>/dev/null
    $RMF $workingDirectory/openssl
  fi

  if test $allFlag -eq 1 -o $libssh2Flag -eq 1; then
    # libssh2
    (
      cd "$destinationDirectory"
      $RMF libssh2*.tar.gz
      $RMRF libssh2*
    ) 2>/dev/null
    $RMF $workingDirectory/libssh2
  fi

  if test $allFlag -eq 1 -o $gnutlsFlag -eq 1; then
    # gnutls
    (
      cd "$destinationDirectory"
      $RMF gnutls-*.tar.bz2
      $RMRF gnutls-*
    ) 2>/dev/null
    $RMF $workingDirectory/gnutls

    # libidn2
    (
      cd "$destinationDirectory"
      $RMF libidn2-*.tar.gz
      $RMRF libidn2-*
    ) 2>/dev/null
    $RMF $workingDirectory/libidn2

    # gmp
    (
      cd "$destinationDirectory"
      $RMF gmp-*.tar.bz2
      $RMRF gmp-*
    ) 2>/dev/null
    $RMF $workingDirectory/gmp

    # nettle
    (
      cd "$destinationDirectory"
      $RMF nettle-*.tar.bz2
      $RMRF nettle-*
    ) 2>/dev/null
    $RMF $workingDirectory/nettle
  fi

  if test $allFlag -eq 1 -o $libcdioFlag -eq 1; then
    # libiconv
    (
      cd "$destinationDirectory"
      $RMF libiconv-*.tar.gz
      $RMRF libiconv-*
    ) 2>/dev/null
    $RMF $workingDirectory/libiconv

    # libcdio
    (
      cd "$destinationDirectory"
      $RMF libcdio-*.tar.gz
      $RMRF libcdio-*
    ) 2>/dev/null
    $RMF $workingDirectory/libcdio
  fi

  if test $allFlag -eq 1 -o $libsmb2Flag -eq 1; then
    # krb5
    (
      cd "$destinationDirectory"
      $RMRF krb5-*
    ) 2>/dev/null
    $RMF $workingDirectory/krb5

    # libsmb2
    (
      cd "$destinationDirectory"
      $RMRF libsmb2-*
    ) 2>/dev/null
    $RMF $workingDirectory/libsmb2
  fi

  if test $allFlag -eq 1 -o $pcreFlag -eq 1; then
    # pcre
    (
      cd "$destinationDirectory"
      $RMRF pcre-*
    ) 2>/dev/null
    $RMF $workingDirectory/pcre
  fi

  if test $allFlag -eq 1 -o $sqlite3Flag -eq 1; then
    # sqlite
    (
      cd "$destinationDirectory"
      $RMRF sqlite-*
    ) 2>/dev/null
    $RMF $workingDirectory/sqlite
  fi

  if test $allFlag -eq 1 -o $mariaDBFlag -eq 1; then
    # MariaDB
    (
      cd "$destinationDirectory"
      $RMRF mariadb-connector-c-*
    ) 2>/dev/null
    $RMF $workingDirectory/mariadb-connector-c
  fi

  if test $allFlag -eq 1 -o $postgreSQLFlag -eq 1; then
    # PostgreSQL
    (
      cd "$destinationDirectory"
      $RMRF postgresql-*
    ) 2>/dev/null
    $RMF $workingDirectory/postgresql
  fi

  if test $allFlag -eq 1 -o $mtxFlag -eq 1; then
    # mtx
    (
      cd "$destinationDirectory"
      $RMRF mtx-*
    ) 2>/dev/null
    $RMF $workingDirectory/mtx
  fi

  if test $allFlag -eq 1 -o $icuFlag -eq 1; then
    # icu
    (
      cd "$destinationDirectory"
      $RMF icu4c-*
      $RMRF icu
    ) 2>/dev/null
    $RMF $workingDirectory/icu
  fi

  if test $allFlag -eq 1 -o $binutilsFlag -eq 1; then
    # binutils
    (
      cd "$destinationDirectory"
      $RMRF binutils-*
      $RMRF binutils
    ) 2>/dev/null
    $RMF $workingDirectory/binutils
  fi

  if test $allFlag -eq 1 -o $pthreadsW32Flag -eq 1; then
    # pthreadW32
    (
      cd "$destinationDirectory"
      $RMRF pthreads-w32-*
    ) 2>/dev/null
    $RMF $workingDirectory/pthreads-w32
  fi

  if test $allFlag -eq 1 -o $launch4jFlag -eq 1; then
    # launch4j
    (
      cd "$destinationDirectory"
      $RMF launch4j-*.tgz
      $RMRF launch4j
    ) 2>/dev/null
    $RMF $workingDirectory/launch4j
  fi

  if test $jreWindowsFlag -eq 1; then
    # Windows JRE
    (
      cd "$destinationDirectory"
      $RMF openjdk-*.zip
      $RMRF openjdk-*
    ) 2>/dev/null
    $RMF $workingDirectory/jre_windows jre_windows_64
  fi
fi

exit 0
