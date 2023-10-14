#!/bin/bash

# constants
BUILD_DIR=$PWD

# parse arugments
sourcePath=$PWD
packageName=""
distributionFileName=""
version=""
userGroup=""
rpmFileName=""
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
          rpmFileName="$1"
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
      rpmFileName="$1"
      n=6
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <source path>:<distribution name> <version> <user:group> <package name>"
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
if test -z "$rpmFileName"; then
  echo >&2 ERROR: no RPM filename given!
  exit 1
fi

# get tools

# TODO: out-of-source build, use existing checkout

# set error handler: execute bash shell
#trap /bin/bash ERR
#set -e

# create temporary directory
tmpDir=`mktemp -d /tmp/rpm-XXXXXX`

# create .spec-file with changelog
sed '/^%changelog/q1' < $sourcePath/packages/backup-archiver.spec > $tmpDir/backup-archiver.spec
LANG=en_US.utf8 $sourcePath/packages/changelog.pl --type rpm < $sourcePath/ChangeLog >> $tmpDir/backup-archiver.spec

# build rpm package (Note: rpmbuild require access)
sudo chown $userGroup $distributionFileName
chmod a+r $distributionFileName
(
  set -e

  cd $tmpDir

# TODO: out-of-source build, build from source instead of extract distribution file
  # build rpm
  rpmbuild \
    -bb \
    --define "_sourcedir $sourcePath" \
    --define "_topdir `pwd`" \
    --define "packageName $packageName" \
    --define "distributionFileName $distributionFileName" \
    --define "version $version" \
    --define "rpmFileName $rpmFileName" \
    --define "testsFlag $testsFlag" \
    backup-archiver.spec

  # get result
  cp -f $tmpDir/RPMS/*/backup-archiver-[0-9]*.rpm $sourcePath/$rpmFileName
  chown $userGroup $sourcePath/$rpmFileName

  # get SHA256 hash
  sha256sum $sourcePath/$rpmFileName

  set +e
)
rc=$?

# debug
if test $debugFlag -eq 1; then
  (cd $tmpDir;
   /bin/bash
  )
fi

# clean-up
rm -rf $tmpDir

exit $rc
