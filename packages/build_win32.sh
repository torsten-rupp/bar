#!/bin/bash
#!/bin/bash

# constants
BUILD_DIR=$PWD
ADDITIONAL_DOWNLOAD_FLAGS=

# parse arugments
sourcePath=$PWD
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
          sourcePath="$1"
          n=1
          ;;
        1)
          packageName="$1"
          n=2
          ;;
        2)
          distributionFileName=`readlink -f "$1"`
          n=3
          ;;
        3)
          version="$1"
          n=4
          ;;
        4)
          userGroup="$1"
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
      sourcePath="$1"
      n=1
      ;;
    1)
      packageName="$1"
      n=2
      ;;
    2)
      distributionFileName=`readlink -f "$1"`
      n=3
      ;;
    3)
      version="$1"
      n=4
      ;;
    4)
      userGroup="$1"
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
  echo "Usage: $0 [options] <source path> <distribution name> <version> <user:group> <setup name>"
  echo ""
  echo "Options:  -t|--test   execute tests"
  echo "          -d|--debug  enable debug"
  echo "          -h|--help   print help"
  exit 0
fi

# enable traciing
if test $debugFlag -eq 1; then
  set -x
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
if test -z "$setupName"; then
  echo >&2 ERROR: no setup name given!
  exit 1
fi

# get tools
wine=`which wine`
if test -z "$wine"; then
  echo >&2 ERROR: wine not found!
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

# create temporary directory
tmpDir=`mktemp -d /tmp/win32-XXXXXX`

(
  cd $tmpDir

# TODO: out-of-source build, build from source instead of extrac distribution file
  # get sources
  if test $fromSourceFlag -eq 1; then
    projectRoot=$SOURCE_PATH
  else
    # extract sources
    tar xjf $distributionFileName
    cd $packageName-$version
    projectRoot=$PWD
  fi

  # build Win32
  ./download-third-party-packages.sh \
    --local-directory /media/extern \
    --no-verbose \
    $ADDITIONAL_DOWNLOAD_FLAGS
  $projectRoot/configure \
    --host=i686-w64-mingw32 \
    --build=x86_64-linux \
    --disable-link-static \
    --enable-link-dynamic \
    --disable-bfd \
--disable-epm \
    ;
  make
  make install DESTDIR=$PWD/tmp DIST=1 SYSTEM=Windows

  # build setup program
  install packages/backup-archiver.iss backup-archiver.iss
  $wine "$iscc" \
    /O$sourcePath \
    /F$setupName \
    backup-archiver.iss

  # get result
  chown $userGroup $sourcePath/${setupName}.exe

  # get MD5 hash
  md5sum $sourcePath/${setupName}.exe

  # debug
  if test $debugFlag -eq 1; then
    /bin/bash
  fi
)

# clean-up
rm -rf $tmpDir

exit 0
