#!/bin/bash

#set -x

BASE_PATH=/media/home

TMP=/tmp/debian

# parse arugments
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
          debFileName="$1"
          n=5
          ;;
        5)
          debFileNameGUI="$1"
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
      debFileName="$1"
      n=5
      ;;
    5)
      debFileNameGUI="$1"
      n=6
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <distribution name> <version> <user:group> <package name> [<package name GUI>]"
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
if test -z "$debFileName"; then
  echo >&2 ERROR: no DEB filename given!
  exit 1
fi

# set error handler: execute bash shell
#trap /bin/bash ERR
#set -e

# create build directory
set -x
install -d $TMP
cd $TMP

# extract sources
tar xjf $BASE_PATH/$distributionFileName
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
LANG=en_US.utf8 ./packages/changelog.pl --type deb < $BASE_PATH/ChangeLog > debian/changelog

# build deb
#debuild \
#  -rfakeroot \
#  -e SOURCE_DIR=$BASE_PATH \
#  -e testsFlag=$testsFlag \
#  -us -uc
#ln -s packages/debian
debuild \
  -e SOURCE_DIR=$BASE_PATH \
  -e packageName=$packageName \
  -e distributionFileName=$distributionFileName \
  -e version=$version \
  -e debFileName=$debFileName \
  -e testsFlag=$testsFlag \
  -us -uc
#  -i -us -uc -b

# get result
cp -f $TMP/${packageName}_[0-9]*.deb $BASE_PATH/$debFileName
chown $userGroup $BASE_PATH/$debFileName
if test -n "$debFileNameGUI"; then
  cp -f $TMP/${packageName}-gui_[0-9]*.deb $BASE_PATH/$debFileNameGUI
  chown $userGroup $BASE_PATH/$debFileNameGUI
fi

# get MD5 hash
md5sum $BASE_PATH/$debFileName
if test -n "$debFileNameGUI"; then
  md5sum $BASE_PATH/$debFileNameGUI
fi

# debug
if test $debugFlag -eq 1; then
  /bin/bash
fi

# clean-up
rm -rf $TMP
