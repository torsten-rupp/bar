#!/bin/bash

set -x

BASE_PATH=/media/home

ADDITIONAL_DOWNLOAD_FLAGS=

# parse arugments
packageName=""
distributionFileName=""
version=""
userGroup=""
testsFlag=0
debugFlag=0
helpFlag=0
n=0
while test $# != 0; do
  case $1 in
    -h | --help)
      helpFlag=1
      shift
      ;;
    -t | --tests)
      testsFlag=1
      shift
      ;;
    -d | --debug)
      debugFlag=1
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
      case $n in
        0)
          packageName="$1"
          n=1
          ;;
        1)
          distributionFileName="$1"
          n=2
          ;;
        2)
          version="$1"
          n=3
          ;;
        3)
          userGroup="$1"
          n=4
          ;;
        4)
          setupName="$1"
          n=5
          ;;
      esac
      shift
      ;;
  esac
done
while test $# != 0; do
  case $n in
    0)
      packageName="$1"
      n=1
      ;;
    1)
      distributionFileName="$1"
      n=1
      ;;
    2)
      version="$1"
      n=3
      ;;
    3)
      userGroup="$1"
      n=4
      ;;
    5)
      setupName="$1"
      n=6
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <distribution name> <version> <user:group> <package name>"
  echo ""
  echo "Options:  -t|--test  execute tests"
  echo "          -h|--help  print help"
  exit 0
fi

# check arguments
if test -z "$packageName"; then
  echo >&2 ERROR: no package name given!
  exit 1
fi
if test -z "$distributionFileName"; then
  echo >&2 ERROR: no distribution filename given!
  exit 1
fi
if test -z "$version"; then
  echo >&2 ERROR: no version given!
  exit 1
fi
if test -z "$userGroup"; then
  echo >&2 ERROR: no user:group id given!
  exit 1
fi
if test -z "$packageName"; then
  echo >&2 ERROR: no package name given!
  exit 1
fi

# set error handler: execute bash shell
trap /bin/bash ERR
set -e

# build Win32
cd /tmp
tar xjf $BASE_PATH/$distributionFileName
cd $packageName-$version
./download-third-party-packages.sh \
  --clean
./download-third-party-packages.sh \
  --no-verbose \
  $ADDITIONAL_DOWNLOAD_FLAGS
./configure \
  --host=i686-w64-mingw32 \
  --build=x86_64-linux \
  --disable-link-static \
  --enable-link-dynamic \
  --disable-iso9660 \
  --disable-bfd \
--disable-epm \
  ;
make
make install DESTDIR=$PWD/tmp DIST=1 SYSTEM=Windows

wine-stable '/media/wine/drive_c/Program Files/Inno Setup 5/ISCC.exe' \
  /O$BASE_PATH \
  /F$setupName \
  bar.iss

# get result
#cp -f /tmp/bar-setup-[0-9]*.exe $BASE_PATH/$setupName
#chown $userGroup $BASE_PATH/$setupName

md5sum $BASE_PATH/$setupName

# debug
if test $debugFlag -eq 1; then
  /bin/bash
fi
/bin/bash
