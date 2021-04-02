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
fromSourceFlag=0
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
    -s | --from-source)
      fromSourceFlag=1
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
          wine="$1"
          n=5
          ;;
        5)
          setupName="$1"
          n=6
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
    4)
      wine="$1"
      n=5
      ;;
    5)
      setupName="$1"
      n=6
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <distribution name> <version> <user:group> <iscc> <setup name>"
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
if test -z "$wine"; then
  echo >&2 ERROR: no wine name given!
  exit 1
fi
if test -z "$setupName"; then
  echo >&2 ERROR: no setup name given!
  exit 1
fi

# get ISCC
iscc=""
$wine '/media/wine/Program Files (x86)/Inno Setup 5/ISCC.exe' '/?' 1>/dev/null 2>/dev/null
if test $? -lt 2; then
  iscc='/media/wine/Program Files (x86)/Inno Setup 5/ISCC.exe'
fi
$wine '/media/wine/Program Files/Inno Setup 5/ISCC.exe' '/?' 1>/dev/null 2>/dev/null
if test $? -lt 2; then
  iscc='/media/wine/Program Files/Inno Setup 5/ISCC.exe'
fi
if test -z "$iscc"; then
  echo >&2 ERROR: ISCC.exe not found!
  exit 1
fi

# set error handler: execute bash shell
trap /bin/bash ERR
set -e

# get sources
cd /tmp
if test $fromSourceFlag -eq 1; then
  PROJECT_ROOT=$BASE_PATH
else
  # extract sources
  tar xjf $BASE_PATH/$distributionFileName
  cd $packageName-$version
  PROJECT_ROOT=$PWD
fi

# build Win32
$PROJECT_ROOT/download-third-party-packages.sh \
  --clean
$PROJECT_ROOT/download-third-party-packages.sh \
  --no-verbose \
  $ADDITIONAL_DOWNLOAD_FLAGS
$PROJECT_ROOT/configure \
  --host=i686-w64-mingw32 \
  --build=x86_64-linux \
  --disable-link-static \
  --enable-link-dynamic \
  --disable-bfd \
--disable-epm \
  ;
make
make install DESTDIR=$PWD/tmp DIST=1 SYSTEM=Windows

if test $fromSourceFlag -ne 1; then
set -x       .
install packages/backup-archiver.iss backup-archiver.iss
$wine "$iscc" \
  /O$BASE_PATH \
  /F$setupName \
  backup-archiver.iss

# get result
chown $userGroup $BASE_PATH/${setupName}.exe

# get MD5 hash
md5sum $BASE_PATH/${setupName}.exe
fi

# debug
if test $debugFlag -eq 1; then
  /bin/bash
fi

#TODO: remove
/bin/bash
