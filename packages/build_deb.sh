#!/bin/bash

set -x
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
toolsFlag=0
guiFlag=0
linkDynamicFlag=0
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
    --tools)
      toolsFlag=1
      shift
      ;;
    --gui)
      guiFlag=1
      shift
      ;;
    --link-dynamic)
      linkDynamicFlag=1
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

# print help
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] <source path> <distribution name> <version> <user:group> <package name> <package name GUI>"
  echo ""
  echo "Options:  --tools         build command line tools package"
  echo "          --gui           build GUI package"
  echo "          --link-dynamic  build dynamic linked binaries"
  echo "          -t|--test       execute tests"
  echo "          -d|--debug      enable debug"
  echo "          -h|--help       print help"
  exit 0
fi

# enable tracing
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
if test $toolsFlag -eq 1 -a -z "$debFileName"; then
  echo >&2 ERROR: no tools DEB filename given!
  exit 1
fi
if test $guiFlag -eq 1 -a -z "$debFileNameGUI"; then
  echo >&2 ERROR: no GUI DEB filename given!
  exit 1
fi

# find tools

# TODO: out-of-source build, use existing checkout

# set error handler: execute bash shell
#trap /bin/bash ERR
#set -e

# create temporary directory
# Note: inside docker use a fixed temporary directory,
#       because e. g. libgcrypt does not accept directories
#       with a name contain '-O[1-9]'
if test -f /.dockerenv; then
  tmpDir=/tmp/deb
  install -d $tmpDir
else
  tmpDir=`mktemp -d /tmp/deb-XXXXXX`
fi

# build deb package
(
  set -e

  cd $tmpDir

  # extract sources
  tar xjf $distributionFileName
  cd $packageName-$version

  # create debian files
  install -d debian
  install -t debian \
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
  debBuildProfiles=""
  if test \( $toolsFlag -eq 0 -a $guiFlag -eq 0 \) -o $toolsFlag -eq 1; then
    debBuildProfiles="$debBuildProfiles tools"
  fi
  if test \( $toolsFlag -eq 0 -a $guiFlag -eq 0 \) -o $guiFlag -eq 1; then
    debBuildProfiles="$debBuildProfiles gui"
  fi
  if test $linkDynamicFlag -eq 1; then
    # Note:
    #  - libxdelta3, libxfs, libpar2 not available as dynamic libraries
    #  - available libmxml is too old
    #  - libpcre not available
    additionalDownloadFlags="xdelta3 xfs par2 mxml pcre"
    additionalConfigureFlags="--enable-link-dynamic"
  else
    additionalDownloadFlags=""
    additionalConfigureFlags=""
  fi
  DEB_BUILD_PROFILES="$debBuildProfiles" \
  debuild \
    -e SOURCE_DIR=$sourcePath \
    -e ADDITIONAL_DOWNLOAD_FLAGS="$additionalDownloadFlags" \
    -e ADDITIONAL_CONFIGURE_FLAGS="$additionalConfigureFlags" \
    -us -uc
  #  -i -us -uc -b

#    -e packageName=$packageName \
#    -e distributionFileName=$distributionFileName \
#    -e version=$version \
#    -e debFileName=$debFileName \
#    -e testsFlag=$testsFlag \

  # get results
  if test $toolsFlag -eq 1; then
    cp -f $tmpDir/${packageName}_[0-9]*.deb $sourcePath/$debFileName
    chown $userGroup $sourcePath/$debFileName
  fi
  if test $guiFlag -eq 1; then
    cp -f $tmpDir/${packageName}-gui_[0-9]*.deb $sourcePath/$debFileNameGUI
    chown $userGroup $sourcePath/$debFileNameGUI
  fi

  # get SHA256 hash
  if test $toolsFlag -eq 1; then
    sha256sum $sourcePath/$debFileName
  fi
  if test $guiFlag -eq 1; then
    sha256sum $sourcePath/$debFileNameGUI
  fi

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
