#!/bin/sh

BASE_PATH=`dirname $0`

PROJECT_ROOT=$BASE_PATH

# get default type
case `uname -o` in
  *Linux*|*linux*)
    if test `uname -m` = "x86_64"; then
      type=linux_64
    else
      type=linux_32
    fi
    ;;
esac


# parse arugments
debugFlag=0
helpFlag=0
n=0
while test $# != 0; do
  case $1 in
    -h | --help)
      helpFlag=1
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
      case $1 in
        linux)
          if test `uname -m` = "x86_64"; then
            type=linux_64
          else
            type=linux_32
          fi
          ;;
        linux_32)
          type=linux
          ;;
        linux_64)
          type=linux
          ;;
        win32)
          if test `uname -m` = "x86_64"; then
            type=win32_64
          else
            type=win32_32
          fi
          ;;
        win32_32)
          type=win32_32
          ;;
        win32_64)
          type=win32_64
          ;;
        *)
          echo >&2 ERROR: unsupported type '$1'!
          exit 1
          ;;
      esac
      shift
      ;;
  esac
done
while test $# != 0; do
  case $1 in
    linux)
      if test `uname -m` = "x86_64"; then
        type=linux_64
      else
        type=linux_32
      fi
      ;;
    linux_32)
      type=linux
      ;;
    linux_64)
      type=linux
      ;;
    win32)
      if test `uname -m` = "x86_64"; then
        type=win32_64
      else
        type=win32_32
      fi
      ;;
    win32_32)
      type=win32_32
      ;;
    win32_64)
      type=win32_64
      ;;
    *)
      echo >&2 ERROR: unsupported type '$1'!
      exit 1
      ;;
  esac
  shift
done
if test $helpFlag -eq 1; then
  echo "Usage: $0 [options] linux|linux_32|linux_64|win32|win32_32|win32_64"
  echo ""
  echo "Options:  -h|--help  print help"
  exit 0
fi

# check arguments
if test -z "$type"; then
  echo >&2 ERROR: no type given!
fi

# build
$PROJECT_ROOT/download-third-party-packages.sh \
  --no-verbose \
  $ADDITIONAL_DOWNLOAD_FLAGS
case $type in
  linux_32)
    $PROJECT_ROOT/configure \
      ;
    ;;
  linux_64)
    $PROJECT_ROOT/configure \
      ;
    ;;
  win32_32)
    $PROJECT_ROOT/configure \
      --host=i686-w64-mingw32 \
      --build=x86_64-linux \
      --disable-link-static \
      --enable-link-dynamic \
      --disable-bfd \
      --disable-epm \
      ;
    ;;
  win32_64)
    $PROJECT_ROOT/configure \
      --host=x86_64-w64-mingw32 \
      --build=x86_64-linux \
      --disable-link-static \
      --enable-link-dynamic \
      --disable-bfd \
      --disable-epm \
      ;
    ;;
esac
make
make install DESTDIR=$PWD/tmp DIST=1 SYSTEM=Windows

exit 0
