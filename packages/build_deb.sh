#!/bin/bash

# constants
BUILD_DIR=$PWD

# parse arugments
sourcePath=$PWD
packageName=""
distributionFileName=""
version=""
userGroup=""
debFileName=""
debFileNameGUI=""
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
          sourcePath=`readlink -f "$1"`
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
          debFileName="$1"
          n=6
          ;;
        6)
          debFileNameGUI="$1"
          n=7
          ;;
      esac
      shift
      ;;
  esac
done
while test $# != 0; do
  case $n in
    0)
      sourcePath=`readlink -f "$1"`
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
      debFileName="$1"
      n=6
      ;;
    6)
      debFileNameGUI="$1"
      n=7
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <source path> <distribution name> <version> <user:group> <package name> [<package name GUI>]"
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
if test -z "$debFileName"; then
  echo >&2 ERROR: no DEB filename given!
  exit 1
fi

# get tools

# TODO: out-of-source build, use existing checkout

# set error handler: execute bash shell
#trap /bin/bash ERR
#set -e

# create temporary directory
tmpDir=`mktemp -d /tmp/deb-XXXXXX`

# build deb package
(
  cd $tmpDir

  # extract sources
  tar xjf $distributionFileName
  cd $packageName-$version

  # create debian files with changelog
  install -d debian
  install -t debian \
          packages/debian/compat \
          packages/debian/control \
          packages/debian/copyright \
          packages/debian/preinst \
          packages/debian/postinst \
          packages/debian/prerm \
          packages/debian/postrm \
          packages/debian/rules
  install -d debian/source
  install -t debian/source \
          packages/debian/source/format

  # create changelog
  LANG=en_US.utf8 ./packages/changelog.pl --type deb < ChangeLog > debian/changelog

# TODO: out-of-source build, build from source instead of extrac distribution file
  # build deb
  #debuild \
  #  -rfakeroot \
  #  -e SOURCE_DIR=$sourcePath \
  #  -e testsFlag=$testsFlag \
  #  -us -uc
  #ln -s packages/debian
  debuild \
    -e SOURCE_DIR=$sourcePath \
    -e packageName=$packageName \
    -e distributionFileName=$distributionFileName \
    -e version=$version \
    -e debFileName=$debFileName \
    -e testsFlag=$testsFlag \
    -us -uc
  #  -i -us -uc -b

  # get result
  cp -f $tmpDir/${packageName}_[0-9]*.deb $sourcePath/$debFileName
  chown $userGroup $sourcePath/$debFileName
  if test -n "$debFileNameGUI"; then
    cp -f $tmpDir/${packageName}-gui_[0-9]*.deb $sourcePath/$debFileNameGUI
    chown $userGroup $sourcePath/$debFileNameGUI
  fi

  # get MD5 hash
  md5sum $sourcePath/$debFileName
  if test -n "$debFileNameGUI"; then
    md5sum $sourcePath/$debFileNameGUI
  fi

  # debug
  if test $debugFlag -eq 1; then
    /bin/bash
  fi
)

# clean-up
rm -rf $tmpDir

exit 0
