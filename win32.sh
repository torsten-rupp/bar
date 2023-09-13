TOOLCHAIN_PREFIX=i686-w64-mingw32
#TOOLCHAIN_PREFIX=i586-mingw32msvc

install -d win

if test "$1" = "-d"; then
  shift

  (cd win;
   rm -rf packages; install -d packages
   ../download-third-party-packages.sh
  )
  if test $? -ne 0; then
    exit $?
  fi
fi

# LDFLAGS="-L/usr/i686-w64-mingw32/lib" \

#  --toolchain-prefix=$TOOLCHAIN_PREFIX-gcc \

(cd win;
 xCC=$TOOLCHAIN_PREFIX-gcc \
 xCXX=$TOOLCHAIN_PREFIX-g++ \
 xAS=$TOOLCHAIN_PREFIX-as \
 xLD=$TOOLCHAIN_PREFIX-g++ \
 xAR=$TOOLCHAIN_PREFIX-ar \
 xRANLIB=$TOOLCHAIN_PREFIX-ranlib \
 xRC=$TOOLCHAIN_PREFIX-windres \
 xNM=$TOOLCHAIN_PREFIX-nm \
 BUILD_CC=gcc \
 xLIBTOOL=libtool \
 ../configure \
 --disable-static \
 --enable-dynamic \
   --host=i686-w64-mingw32 \
   --build=x86_64-linux \
   --disable-iso9660 \
   --disable-bfd \
--disable-lzma \
--disable-smbclient \
   "$@";
)
if test $? -ne 0; then
  exit $?
fi

(cd win;
 make
)
if test $? -ne 0; then
  exit $?
fi

exit 0
