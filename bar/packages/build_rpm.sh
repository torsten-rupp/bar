#!/bin/bash

#set -x

BASE_PATH=/media/home

# parse arugments
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
          rpmFileName="$1"
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
      rpmFileName="$1"
      n=5
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
if test -z "$rpmFileName"; then
  echo >&2 ERROR: no RPM filename given!
  exit 1
fi

# set error handler: execute bash shell
#trap /bin/bash ERR
#set -e

# create .spec-file with changelog
sed -i '/^%changelog/q1' packages/backup-archiver.spec;
LANG=en_US.utf8 ./packages/changelog.pl --type rpm < ChangeLog >> packages/backup-archiver.spec

# build rpm
sed -i '/^%changelog/q1' packages/backup-archiver.spec;
LANG=en_US.utf8 ./packages/changelog.pl --type rpm < ChangeLog >> packages/backup-archiver.spec
rpmbuild \
  -bb \
  --define "_sourcedir $BASE_PATH" \
  --define "packageName $packageName" \
  --define "distributionFileName $distributionFileName" \
  --define "version $version" \
  --define "rpmFileName $rpmFileName" \
  --define "testsFlag $testsFlag" \
  $BASE_PATH/packages/backup-archiver.spec

# get result
cp -f /root/rpmbuild/RPMS/*/backup-archiver-[0-9]*.rpm $BASE_PATH/$rpmFileName
chown $userGroup $BASE_PATH/$rpmFileName

# get MD5 hash
md5sum $BASE_PATH/$rpmFileName

# debug
if test $debugFlag -eq 1; then
  /bin/bash
fi

#TODO: remove
/bin/bash
