dnl --------------------------------------------------------------------
dnl
dnl $Source: /home/torsten/cvs/bar/configure.in,v $
dnl $Revision$
dnl $Author$
dnl Contents: BAR auto-configure script
dnl
dnl --------------------------------------------------------------------

dnl ----------------------- additional macros --------------------------


dnl --- print compile infofg
AC_DEFUN([AC_MSG_CREATE],
[
  _AS_ECHO_N([create $1... ])
])
AC_DEFUN([AC_MSG_COMPILE],
[
  _AS_ECHO_N([compile $1... ])
])

dnl --- output log file
AC_DEFUN([AC_MSG_OUTPUT_LOG],
[
  echo --- error log -------------------------------------------------------------------------
  cat $1 | while read s; do echo "  $s"; done
  echo ---------------------------------------------------------------------------------------
])

dnl --- set toolchain variables CFLAGS, CXXFLAGS, LDFALGS, LIBS
dnl usage: AC_SET_TOOLCHAIN_SETTINGS(<CC_FLAGS>,<CXX_FLAGS>,<CC_DEFINES>,<CC_INCLUDES>,<LD_FLAGS>,<LIBRARY_PATHS>,<STATIC_LIBRARIES>,<LIBRARIES>)
AC_DEFUN([AC_SET_TOOLCHAIN_SETTINGS],
[
  CFLAGS="$1 -v"
  CXXFLAGS="$2 -v"
  for z in $3 ""; do
    if test -n "$z"; then
      CFLAGS="$CFLAGS -D$z"
    fi
  done
  for z in $4 ""; do
    if test -n "$z"; then
      CFLAGS="$CFLAGS -I$z"
    fi
  done
  LDFLAGS="$5"
  for z in $6 ""; do
    if test -n "$z"; then
      LDFLAGS="$LDFLAGS -L$z"
    fi
  done
  LIBS=""
  for z in $7 ""; do
    if test -n "$z"; then
      LIBS="-l$z $LIBS"
    fi
  done
  for z in $8 ""; do
    if test -n "$z"; then
      LIBS="-l$z $LIBS"
    fi
  done
  for z in $7 ""; do
    if test -n "$z"; then
      LIBS="-l$z $LIBS"
    fi
  done
  for z in $8 ""; do
    if test -n "$z"; then
      LIBS="-l$z $LIBS"
    fi
  done
])

dnl --- check if C compiler support option
dnl usage: AC_COMPILER_OPTION(<option>,<variable>...)
AC_DEFUN([AC_COMPILER_OPTION],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK(
    [if $CC support option $1],
    ac_cv_c_option_`echo $1|tr -c -d a-zA-Z0-9_`,
    [
      cat << \EOF > test.c
int main(void)
{
  return 0;
}
EOF
      if AC_TRY_COMMAND($CC $1 test.c -o test.o); then
        ac_supported=yes
        for s in $2; do
          eval "if test -n \"\$$s\"; then $s=\"\$$s \"; fi"
          eval "$s=\"\$$s$1\""
        done
      else
        ac_supported=no
      fi
      AS_VAR_SET(ac_cv_c_option_`echo $1|tr -c -d a-zA-Z0-9_`,$ac_supported)

      rm -f test.c test.o
    ]
  )dnl
  AC_PROVIDE([$0])dnl
])

dnl --- check if linker support option
dnl usage: AC_LINKER_OPTION(<option>,<variable>...)
AC_DEFUN([AC_LINKER_OPTION],
[
  AC_CACHE_CHECK(
    [if $LD support option $1],
    ac_cv_linker_option_`echo $1|tr -c -d a-zA-Z0-9_`,
    [
      cat << \EOF > test.c
int main(void)
{
  return 0;
}
EOF
      $CC test.c -o test.o
      if AC_TRY_COMMAND($LD $1 -o test test.o); then
        ac_supported=yes
        for s in $2; do
          eval "if test -n \"\$$s\"; then $s=\"\$$s \"; fi"
          eval "$s=\"\$$s$1\""
        done
      else
        ac_supported=no
      fi
      AS_VAR_SET(ac_cv_linker_option_`echo $1|tr -c -d a-zA-Z0-9_`,$ac_supported)

      rm -f test.c test.o test
    ]
  )dnl
  AC_PROVIDE([$0])dnl
])

dnl --- check if constant is available
dnl usage: AC_CHECK_CONSTANT(<constant>,<if found>,<if not found>,<headers>)
AC_DEFUN([AC_CHECK_CONSTANT],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK(
    [if constant $1 is available],
    ac_cv_const_$1,
    [
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([`echo $4|sed 's/ /\n/g'|while read s; do if test -n "$s"; then echo $s|sed 's/\(.*\)/#include <\\1>/g'; fi; done`],
                                         [int i = (int)$1;
                                          return 0;
                                         ]
                                        )
                        ],
                        [AS_VAR_SET([ac_cv_const_$1],[yes]); $2],
                        [AS_VAR_SET([ac_cv_const_$1],[no]); $3]
                       )
    ]
  )dnl
  AC_PROVIDE([$0])dnl
])
dnl &(($1*)NULL).$2
dnl --- check if struct member is available
dnl usage: AC_CHECK_STRUCT_MEMBER(<type>,<member>,<if found>,<if not found>,<headers>)
AC_DEFUN([AC_CHECK_STRUCT_MEMBER],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK(
    [if $1.$2 is available],
    ac_cv_member_`echo $1_$2|tr -c -d a-zA-Z0-9_`,
    [
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([`echo $5|sed 's/ /\n/g'|while read s; do if test -n "$s"; then echo $s|sed 's/\(.*\)/#include <\\1>/g'; fi; done`],
                                         [int i = &(($1*)0)->$2;
                                          return 0;
                                         ]
                                        )
                        ],
                        [AS_VAR_SET([ac_cv_member_`echo $1_$2|tr -c -d a-zA-Z0-9_`],[yes]); $3],
                        [AS_VAR_SET([ac_cv_member_`echo $1_$2|tr -c -d a-zA-Z0-9_`],[no]); $4]
                       )
    ]
  )dnl
  AC_PROVIDE([$0])dnl
])

dnl --- check if function is available
dnl usage: AC_CHECK_FUNCTION(<function>,<if found>,<if not found>,<headers>)
AC_DEFUN([AC_CHECK_FUNCTION],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK(
    [for $1],
    ac_cv_func_$1,
    [
      ac_cv_func_$1="no"
      echo > conftest.log

      for ac_headers in $4 ""; do
        AC_LINK_IFELSE([AC_LANG_PROGRAM([`echo $ac_headers|sed 's/+/\n/g'|while read s; do if test -n "$s"; then echo $s|sed 's/\(.*\)/#include <\\1>/g'; fi; done`],
                                        [`if test -z "$ac_headers"; then echo "extern void $1();"; fi`
                                         #ifdef $1
                                         #else
                                           return (int)$1;
                                         #endif
                                        ]
                                       )
                       ],
                       [AS_VAR_SET([ac_cv_func_$1],[yes]); break]
                      )
      done
    ]
  )
  AS_IF([test "$ac_cv_func_$1" != no],[$2],:,[$3])
  AC_PROVIDE([$0])dnl
])

dnl --- check if function is available in library (Note: do not use AC_SEARCH_LIBS; it does not work with Windows functions)
dnl usage: AC_SEARCH_LIBRARIES(<symbol>,<libraries>,<if found>,<if not found>,<headers>)
AC_DEFUN([AC_SEARCH_LIBRARIES],
[
  AC_REQUIRE([AC_PROG_CC])
  AS_VAR_PUSHDEF([ac_search_libraries],[ac_cv_search_libraries_$1])dnl
  ac_result=""
  AC_CACHE_CHECK(
    [for library containing $1],
    ac_search_libraries,
    [
      dnl save C compiler settings
      ac_func_search_libraries_save_LIBS=$LIBS

      for ac_library in $2 ""; do
        if test -z "$ac_library"; then
          ac_result="none required"
        else
          ac_result=-l$ac_library
          LIBS="-l$ac_library $ac_func_search_libraries_save_LIBS"
        fi

        AC_LINK_IFELSE([AC_LANG_PROGRAM([`echo $5|sed 's/ /\n/g'|while read s; do if test -n "$s"; then echo $s|sed 's/\(.*\)/#include <\\1>/g'; fi; done`],
                                        [`if test -z "$5"; then echo "extern void $1();"; fi`
                                         return (int)$1;
                                        ]
                                       )
                          ],
                          [AS_VAR_SET([ac_search_libraries],[$ac_result]); break]
                         )
      done
      AS_VAR_SET_IF([ac_search_libraries],,[AS_VAR_SET([ac_search_libraries],[no])])

      dnl restore C compiler settings
      LIBS=$ac_func_search_libraries_save_LIBS
    ]
  )
  AS_VAR_COPY([ac_result],[ac_search_libraries])
  AS_IF([test "$ac_result" != no],[test "$ac_result" = "none required" || LIBS="$ac_result $LIBS" $3],[$4])
  AS_VAR_POPDEF([ac_search_libraries])dnl
])

dnl --- check if function is available in static library (Note: do not use AC_SEARCH_LIBS; it does not work with Windows functions)
dnl usage: AC_SEARCH_STATIC_LIBRARIES(<symbol>,<libraries>,<if found>,<if not found>,<headers>)
AC_DEFUN([AC_SEARCH_STATIC_LIBRARIES],
[
  AC_REQUIRE([AC_PROG_CC])
  AS_VAR_PUSHDEF([ac_search_static_libraries], [ac_cv_search_static_$1])dnl
  AC_CACHE_CHECK(
    [for static library containing $1],
    [ac_search_static_libraries],
    [
      dnl save C compiler settings
      ac_func_search_static_libraries_save_CFLAGS="$CFLAGS"
      ac_func_search_static_libraries_save_LIBS="$LIBS"

      dnl test static linkage
      CFLAGS="$CFLAGS"
      for ac_library in $2 ""; do
        if test -z "$ac_library"; then
          ac_result="none required"
        else
          ac_result=-l$ac_library
          LIBS="-Wl,-Bstatic -l$ac_library -Wl,-Bdynamic $ac_func_search_static_libraries_save_LIBS"
        fi

        AC_LINK_IFELSE([AC_LANG_PROGRAM([`echo $5|sed 's/ /\n/g'|while read s; do if test -n "$s"; then echo $s|sed 's/\(.*\)/#include <\\1>/g'; fi; done`],
                                        [`if test -z "$5"; then echo "extern void $1();"; fi`
                                         return (int)$1;
                                        ]
                                       )
                          ],
                          [AS_VAR_SET([ac_search_static_libraries],[$ac_result]); break]
                         )
      done
      AS_VAR_SET_IF([ac_search_static_libraries],,[AS_VAR_SET([ac_search_static_libraries],[no])])

      dnl restore C compiler settings
      LIBS=$ac_func_search_libraries_save_LIBS
      CFLAGS=$ac_func_search_static_libraries_save_CFLAGS
    ]
  )
  AS_VAR_COPY([ac_result],[ac_search_static_libraries])
  AS_IF([test "$ac_result" != no],[test "$ac_result" = "none required" || LIBS="$ac_result $LIBS" $3],[$4])
  AS_VAR_POPDEF([ac_search_static_libraries])dnl
])


dnl --- check if C compiler long long data type available
dnl usage: AC_C_LONG_LONG(<if found>,<if not found>)
# (original macro from http://ac-archive.sourceforge.net/C_Support/ac_c_long_long.html)
AC_DEFUN([AC_C_LONG_LONG],
[
  AC_CACHE_CHECK(
    [for long long int],
    ac_cv_c_long_long,
    [
      if test "$GCC" = yes; then
        ac_cv_c_long_long=yes
      else
        AC_TRY_COMPILE(,[long long int i;],
        ac_cv_c_long_long=yes,
        ac_cv_c_long_long=no)
      fi
    ]
  )
  AS_IF([test "$ac_cv_c_long_long" = "yes"],[$1],[$2])dnl
  AC_PROVIDE([$0])dnl
])

dnl --- extend C compiler defines for large files
dnl usage: AC_CHECK_LARGEFILE_DEFINES
AC_DEFUN([AC_CHECK_LARGEFILE_DEFINES],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK(
    [for needed large files defines],
    ac_cv_largefile_defines,
    [
if false; then
dnl For some reason also on a 64bit systems is seems to be necessary to set the defines below.
dnl Otherweise large files may not be written correctly!
      cat << \EOF > conftest.c
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
int main(void) { printf("%p",fopen64); return 0; }
EOF
      ${CC} ${CFLAGS} -Wall -Werror -c conftest.c -o conftest.o 1>/dev/null 2>/dev/null; rc=$?
      if test $rc -eq 0; then
        ac_cv_largefile_defines=""
      else
        ${CC} ${CFLAGS} -Wall -Werror -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -c conftest.c -o conftest.o 1>/dev/null 2>/dev/null; rc=$?
        if test $rc -eq 0; then
          ac_cv_largefile_defines="_FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE _LARGEFILE64_SOURCE"
        else
          ac_cv_largefile_defines=""
        fi
      fi
      rm -f conftest.c conftest.o
fi
      ac_cv_largefile_defines="_FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE _LARGEFILE64_SOURCE"

      CC_DEFINES="$CC_DEFINES $ac_cv_largefile_defines"
    ]
  )dnl
  AC_PROVIDE([$0])dnl
])

dnl --- check if variable attribute is supported
dnl usage: AC_CHECK_VARIABLE_ATTRIBUTE(<attribute>,<if found>,<if not found>)
AC_DEFUN([AC_CHECK_VARIABLE_ATTRIBUTE],
[
  AC_CACHE_CHECK(
    [if variable attribute $1 is supported],
    ac_cv_variable_attribute_$1,
    [
      ac_supported=no
      AC_TRY_COMPILE([$1 int x;],,ac_supported=yes,ac_supported=no)
      AS_IF([test "$ac_supported" = "yes"],[$2],:,[$3])
      AS_VAR_SET(ac_cv_variable_attribute_$1,$ac_supported)
    ]
  )dnl
  AC_PROVIDE([$0])dnl
])

dnl --- extend C compiler defines for pthreads
dnl usage: AC_CHECK_PTHREAD_DEFINES
AC_DEFUN([AC_CHECK_PTHREAD_DEFINES],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK(
    [for needed pthread defines],
    ac_cv_extended_pthread_defines,
    [
      cat << \EOF > conftest.c
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
int main(void) { printf("%p",pthread_mutexattr_settype); return 0; }
EOF
      ${CC} ${CFLAGS} -Wall -Werror -c conftest.c -o conftest.o 1>/dev/null 2>/dev/null; rc=$?
      if test $rc -eq 0; then
        ac_cv_extended_pthread_defines=""
      else
        ${CC} ${CFLAGS} -Wall -Werror -D_GNU_SOURCE -c conftest.c -o conftest.o 1>/dev/null 2>/dev/null; rc=$?
        if test $rc -eq 0; then
          ac_cv_extended_pthread_defines="_GNU_SOURCE"
        else
          ac_cv_extended_pthread_defines=""
        fi
      fi
      rm -f conftest.c conftest.o
      CC_DEFINES="$CC_DEFINES $ac_cv_extended_pthread_defines"
    ]
  )dnl
  AC_PROVIDE([$0])dnl
])

dnl --- check number of arguments of mkdir
dnl usage: AC_CHECK_MKDIR_ARGUMENTS
AC_DEFUN([AC_CHECK_MKDIR_ARGUMENTS],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK(
    [number of arguments of mkdir()],
    ac_cv_mkdir_arguments_count,
    [
      ac_cv_mkdir_arguments_count=""

      if test -z "$ac_cv_mkdir_arguments_count"; then
        cat << \EOF > conftest.c
#include <io.h>
int main(void) { mkdir("."); return 0; }
EOF
        ${CC} ${CFLAGS} -Wall -Werror -c conftest.c -o conftest.o 1>/dev/null 2>/dev/null; rc=$?
        if test $rc -eq 0; then
          ac_cv_mkdir_arguments_count=1
        fi
        rm -f conftest.c conftest.o
      fi
      if test -z "$ac_cv_mkdir_arguments_count"; then
        cat << \EOF > conftest.c
#include <sys/stat.h>
#include <sys/types.h>
int main(void) { mkdir(".",0); return 0; }
EOF
        ${CC} ${CFLAGS} -Wall -Werror -c conftest.c -o conftest.o 1>/dev/null 2>/dev/null; rc=$?
        if test $rc -eq 0; then
          ac_cv_mkdir_arguments_count=2
        fi
        rm -f conftest.c conftest.o
      fi
    ]
  )dnl

  AC_DEFINE_UNQUOTED(MKDIR_ARGUMENTS_COUNT,$ac_cv_mkdir_arguments_count,[number of mkdir() arguments])

  AC_PROVIDE([$0])dnl
])

dnl --- detect Java version
dnl usage: AC_JAVA_VERSION(<variable>)
AC_DEFUN([AC_JAVA_VERSION],
[
  AC_REQUIRE([AC_PROG_JAVAC])
  AC_REQUIRE([AC_PROG_JAVA])
  AC_CACHE_CHECK(
    [$JAVA version],
    ac_cv_prog_java_version,
    [
      cat << \EOF > Test.java
class Test
{
  public static void main(String[[]] args)
  {
    System.out.println(System.getProperty("java.version"));
  }
}
EOF
      if AC_TRY_COMMAND($JAVAC $JAVAC_FLAGS Test.java) && test -s Test.class; then
        ac_cv_prog_java_version=`$JAVA $JAVA_FLAGS Test`
      else
        ac_cv_prog_java_version=""
      fi
      AS_VAR_SET($1,$ac_cv_prog_java_version)

      rm -f Test.java Test.class
    ]
  )
  AC_PROVIDE([$0])dnl
])

dnl --- detect Java model (32/64 bit)
dnl usage: AC_JAVA_DATA_MODEL(<variable>)
AC_DEFUN([AC_JAVA_DATA_MODEL],
[
  AC_REQUIRE([AC_PROG_JAVAC])
  AC_REQUIRE([AC_PROG_JAVA])
  AC_CACHE_CHECK(
    [$JAVA data model],
    ac_cv_prog_java_data_model,
    [
      cat << \EOF > Test.java
class Test
{
  public static void main(String[[]] args)
  {
    System.out.println(System.getProperty("sun.arch.data.model"));
  }
}
EOF
      if AC_TRY_COMMAND($JAVAC $JAVAC_FLAGS Test.java) && test -s Test.class; then
        ac_cv_prog_java_data_model=`$JAVA $JAVA_FLAGS Test`
      else
        ac_cv_prog_java_data_model=32
      fi
      AS_VAR_SET($1,$ac_cv_prog_java_data_model)

      rm -f Test.java Test.class
    ]
  )
  AC_PROVIDE([$0])dnl
])

dnl --- detect EPM version
dnl usage: AC_PROG_EPM_VERSION
AC_DEFUN([AC_PROG_EPM_VERSION],
[
  AC_REQUIRE([AC_PROG_EPM])
  AC_MSG_CHECKING([$EPM version])
  AC_CACHE_VAL(
    ac_cv_prog_epm_version,
    [
      if   test -n "`$EPM --version 2>/dev/null | grep \"ESP Package Manager v4\"`"; then
        AC_MSG_RESULT([4.x])
        ac_cv_prog_epm_version="4.x"
        EPM_VERSION_4=1
        EPM_VERSION_3=
      elif test -n "`$EPM --version 2>/dev/null | grep \"ESP Package Manager v3\"`"; then
        AC_MSG_RESULT([3.x])
        ac_cv_prog_epm_version="3.x"
        EPM_VERSION_4=
        EPM_VERSION_3=1
      else
        AC_MSG_RESULT([unknown - assume version 4 or newer])
        ac_cv_prog_epm_version="unknown"
        EPM_VERSION_4=1
        EPM_VERSION_3=
      fi
    ]
  )
  AC_PROVIDE([$0])dnl
])

dnl --- check sudo command
dnl usage: AC_PROG_SUDO_COMMAND
AC_DEFUN([AC_PROG_SUDO_COMMAND],
[
  AC_MSG_CHECKING([execution of '$SUDO $1'])
  AS_VAR_PUSHDEF([ac_cv_prog_sudo_command],[ac_cv_prog_sudo_command_`echo $1|tr -c -d a-zA-Z0-9_`])dnl
  AC_CACHE_VAL(
    [ac_cv_prog_sudo_command],
    [
      SUDO_ASKPASS=false $SUDO -n $2 1</dev/null 2>/dev/null
      if   test $? -eq 0; then
        ac_result=yes
      else
        ac_result=no
      fi
      AC_MSG_RESULT($ac_result)
      AS_VAR_SET([ac_cv_prog_sudo_command],$ac_result)
    ]
  )
  AS_IF([test "$ac_result" != no],[$3],:,[$4])
  AS_VAR_POPDEF([ac_cv_prog_sudo_command])dnl
  AC_PROVIDE([$0])dnl
])

dnl ------------------------- basic settings ---------------------------

dnl --- initialize autoconf
AC_INIT(.)
AC_PREREQ(2.53)

dnl --- set source-directory (specify existing source file)
AC_CONFIG_SRCDIR(bar/bar.c)

dnl --- install-sh, config.sub and config.guess are in this directory.
AC_CONFIG_AUX_DIR(bin)

dnl --- check bost system
AC_CANONICAL_HOST

dnl ----------------------- substitute settings ------------------------

AC_SUBST(VERSION_MAJOR)                     dnl version major number
AC_SUBST(VERSION_MINOR)                     dnl version minor number
AC_SUBST(VERSION_REVISION)                  dnl version revision number (SVN number)

AC_SUBST(COPYRIGHT_DATE)                    dnl copyright date

AC_SUBST(PLATFORM)                          dnl platform name

AC_SUBST(FILE_SEPARATOR_CHAR)               dnl file separator path character (/ or \\)
AC_SUBST(FILE_SEPARATOR_STRING)             dnl file separator path character as string (/ or \\)

AC_SUBST(OBJEXT)                            dnl object extension
AC_SUBST(EXEEXT)                            dnl executable extension
AC_SUBST(SHELLEXT)                          dnl shell extension

AC_SUBST(ENABLE_DEBUG)                      dnl "yes" for debug version, "no" otherwise
AC_SUBST(ENABLE_BZ2)                        dnl "yes" for bzip2 support, "no" otherwise
AC_SUBST(ENABLE_LZMA)                       dnl "yes" for lzma support, "no" otherwise
AC_SUBST(ENABLE_LZO)                        dnl "yes" for lzo support, "no" otherwise
AC_SUBST(ENABLE_LZ4)                        dnl "yes" for lz4 support, "no" otherwise
AC_SUBST(ENABLE_ZSTD)                       dnl "yes" for zstd support, "no" otherwise
AC_SUBST(ENABLE_XDELTA3)                    dnl "yes" for xdelta3 support, "no" otherwise
AC_SUBST(ENABLE_GCRYPT)                     dnl "yes" for gcrypt support, "no" otherwise
AC_SUBST(ENABLE_FTP)                        dnl "yes" for ftp support, "no" otherwise
AC_SUBST(ENABLE_CURL)                       dnl "yes" for curl support, "no" otherwise
AC_SUBST(ENABLE_SSH)                        dnl "yes" for ssh support, "no" otherwise
AC_SUBST(ENABLE_TLS)                        dnl "yes" for tls support, "no" otherwise
AC_SUBST(ENABLE_ISO9660)                    dnl "yes" for iso9660 support, "no" otherwise
AC_SUBST(ENABLE_PCRE)                       dnl "yes" for PCRE support, "no" otherwise
AC_SUBST(ENABLE_GUI)                        dnl "yes" for GUI support, "no" otherwise
AC_SUBST(ENABLE_GUI_I18N)                   dnl "yes" for GUI i18n support, "no" otherwise
AC_SUBST(ENABLE_EPM)                        dnl "yes" for EPM support, "no" otherwise

AC_SUBST(HAVE_ICU)                          dnl "1" for ICU support, "" otherwise
AC_SUBST(HAVE_Z)                            dnl "1" for zlib support, "" otherwise
AC_SUBST(HAVE_BZ2)                          dnl "1" for bzip2 support, "" otherwise
AC_SUBST(HAVE_LZMA)                         dnl "1" for lzma support, "" otherwise
AC_SUBST(HAVE_LZO)                          dnl "1" for lzo support, "" otherwise
AC_SUBST(HAVE_LZ4)                          dnl "1" for lz4 support, "" otherwise
AC_SUBST(HAVE_ZSTD)                         dnl "1" for zstd support, "" otherwise
AC_SUBST(HAVE_XDELTA)                       dnl "1" for xdelta support, "" otherwise
AC_SUBST(HAVE_GCRYPT)                       dnl "1" for gcrypt support, "" otherwise
AC_SUBST(HAVE_FTP)                          dnl "1" for ftp support, "" otherwise
AC_SUBST(HAVE_CARES)                        dnl "1" for c-areas support, "" otherwise
AC_SUBST(HAVE_CURL)                         dnl "1" for curl support, "" otherwise
AC_SUBST(HAVE_MXML)                         dnl "1" for mxml support, "" otherwise
AC_SUBST(HAVE_SSL)                          dnl "1" for ssl support, "" otherwise
AC_SUBST(HAVE_SSH2)                         dnl "1" for ssh2 support, "" otherwise
AC_SUBST(HAVE_GNU_TLS)                      dnl "1" for GNU TLS support, "" otherwise
AC_SUBST(HAVE_ISO9660)                      dnl "1" for ISO9660 support, "" otherwise
AC_SUBST(HAVE_PCRE)                         dnl "1" for pcre support, "" otherwise
AC_SUBST(HAVE_PTHREADS_W32)                 dnl "1" for pthreads-w32 support, "" otherwise
AC_SUBST(HAVE_BREAKPAD)                     dnl "1" for BreakPad support, "" otherwise
AC_SUBST(HAVE_BFD)                          dnl "1" for BFD support, "" otherwise

AC_SUBST(HAVE_USLEEP)                       dnl "1" if usleep() is avaiulable, "" otherwise

AC_SUBST(INSTALL_DIR)                       dnl installation directory
AC_SUBST(INSTALL_BIN_DIR)                   dnl installation directory for binaries
AC_SUBST(CONFIG_DIR)                        dnl configuration directory
AC_SUBST(RUNTIME_DIR)                       dnl runtime directory
AC_SUBST(TLS_DIR)                           dnl TLS/SSL directory
AC_SUBST(MAN_DIR)                           dnl man page directory
AC_SUBST(LOCALE_DIR)                        dnl locale directory

AC_SUBST(CC)                                dnl C compiler
AC_SUBST(LD)                                dnl linker
AC_SUBST(STRIP)                             dnl strip
AC_SUBST(OBJDUMP)                           dnl objdump
AC_SUBST(OBJCOPY)                           dnl objcopy

AC_SUBST(CC_FLAGS)                          dnl C compiler flags
AC_SUBST(CC_WARN_FLAGS)                     dnl C compiler warning flags
AC_SUBST(CXX_FLAGS)                         dnl C++ compiler flags
AC_SUBST(CXX_WARN_FLAGS)                    dnl C++ compiler warning flags
AC_SUBST(CC_OPTIMIZE)                       dnl C compiler optimization flags
AC_SUBST(CC_OPTIMIZE_RELEASE)               dnl additional C compiler optimization flags for release version
AC_SUBST(CC_OPTIMIZE_DEBUG)                 dnl addtiional C compiler optimization flags for debug version
AC_SUBST(CC_DEFINES)                        dnl C compiler defines
AC_SUBST(CC_DEFINES_RELEASE)                dnl additional C compiler defines for debug version
AC_SUBST(CC_DEFINES_DEBUG)                  dnl addtioonal C compiler defines for debug version
AC_SUBST(CC_INCLUDES)                       dnl C compiler include paths

AC_SUBST(LD_FLAGS)                          dnl linker flags

AC_SUBST(LIBRARY_PATHS)                     dnl library paths
AC_SUBST(LIBRARIES)                         dnl libraries to link
AC_SUBST(STATIC_LIBRARIES)                  dnl libraries to link static

AC_SUBST(JAVA)                              dnl java command
AC_SUBST(JAVAC)                             dnl java compiler
AC_SUBST(JAR)                               dnl jar command

AC_SUBST(XGETTEXT)                          dnl xgettext command
AC_SUBST(MSGMERGE)                          dnl msgmerge command
AC_SUBST(MSGFMT)                            dnl msgfmt command

AC_SUBST(JAVA_DATA_MODEL)                   dnl java data model (32 or 64)
AC_SUBST(JAVA_FLAGS)                        dnl java flags
AC_SUBST(JAVAC_FLAGS)                       dnl java compiler flags
AC_SUBST(JAVA_PATH_SEPARATOR_CHAR)          dnl java path separator (: or ;)

AC_SUBST(SWT_DIR)                           dnl SWT directory
AC_SUBST(LAUNCH4J_DIR)                      dnl launch4j directory

AC_SUBST(DUMP_SYMS)                         dnl Breakpad dump symbols tool

AC_SUBST(CERTOOL)                           dnl GNU TLS certtool
AC_SUBST(OPENSSL)                           dnl OpenSSL key tool
AC_SUBST(KEYTOOL)                           dnl Java keytool

AC_SUBST(UNOCONV)                           dnl OO conversion tool
AC_SUBST(TXT2MAN)                           dnl text-to-man page tool

AC_SUBST(ATTR)                              dnl attr command
AC_SUBST(TIME)                              dnl time command
AC_SUBST(GETFATTR)                          dnl getfattr command
AC_SUBST(SETFATTR)                          dnl setfattr command

AC_SUBST(MKFS_EXT2)                         dnl mksfs.ext2 command
AC_SUBST(MKFS_EXT3)                         dnl mksfs.ext3 command
AC_SUBST(MKFS_EXT4)                         dnl mksfs.ext4 command
AC_SUBST(MKFS_MSDOS)                        dnl mksfs.msdos command
AC_SUBST(MKFS_REISERFS)                     dnl mksfs.reiserfs command
AC_SUBST(MKFS_REISER4)                      dnl mksfs.reiser4 command

AC_SUBST(XGETTEXT)                          dnl xgettext command
AC_SUBST(MSGMERGE)                          dnl msgmerge command
AC_SUBST(MSGFMT)                            dnl msgfmt command

AC_SUBST(EPM)                               dnl EPM tool
AC_SUBST(EPM_VERSION_3)                     dnl 1 if EPM version 3, otherwise empty
AC_SUBST(EPM_VERSION_4)                     dnl 1 if EPM version 4, otherwise empty

AC_SUBST(HAVE_SUDO_COMMAND_CHATTR_I)        dnl sudo command chattr +-i
AC_SUBST(HAVE_SUDO_COMMAND_CHATTR_A)        dnl sudo command chattr +-a

AC_SUBST(HEAPVIEW_DIR)                      dnl heapview directory

dnl ----------------------- initialize variables -----------------------

CC_FLAGS="$CFLAGS"
CC_WARN_FLAGS=""
CXX_FLAGS="$CFLAGS"
CXX_WARN_FLAGS=""
CC_OPTIMIZE=""
CC_OPTIMIZE_RELEASE=""
CC_OPTIMIZE_DEBUG=""
CC_DEFINES="_GNU_SOURCE"
CC_DEFINES_RELEASE=""
CC_DEFINES_DEBUG=""
CC_INCLUDES=""

LD_FLAGS="$LDFLAGS"
LIBRARY_PATHS=""
LIBRARIES=""
STATIC_LIBRARIES=""

ENABLE_DEBUG="no"
ENABLE_LINK_DYNAMIC="no"
ENABLE_BZ2="yes"
ENABLE_LZMA="yes"
ENABLE_LZO="yes"
ENABLE_LZ4="yes"
ENABLE_ZSTD="yes"
ENABLE_XDELTA3="yes"
ENABLE_GCRYPT="yes"
ENABLE_FTP="yes"
ENABLE_CURL="yes"
ENABLE_SSH="yes"
ENABLE_TLS="yes"
ENABLE_ISO9660="yes"
ENABLE_PCRE="yes"
ENABLE_PTHREADS_W32="yes"
ENABLE_BREAKPAD="yes"
ENABLE_BFD="yes"
ENABLE_GUI="yes"
ENABLE_GUI_I18N="yes"
ENABLE_EPM="yes"
ENABLE_PACKAGE_COMPILE="yes"

# temporary file
tmpFile=`mktemp configure-XXXXXX`

dnl ---------------------------- check system --------------------------

dnl AC_CANONICAL_SYSTEM
dnl $host
dnl $host_cpu
dnl $host_os
case "$host_os" in
  linux-*)
    PLATFORM=LINUX
    FILE_SEPARATOR_CHAR="/"
    JAVA_PATH_SEPARATOR=":"
    SHELLEXT=""
    nproc 1>/dev/null 2>&1
    if test $? -eq 0; then
      CPU_CORES=`nproc`
    else
      CPU_CORES=1
    fi
    ;;
  windows*)
    PLATFORM=WINDOWS
    FILE_SEPARATOR_CHAR="\\\\"
    JAVA_PATH_SEPARATOR=";"
    SHELLEXT=".cmd"
    CPU_CORES=1
    ;;
  mingw*)
    PLATFORM=WINDOWS
    FILE_SEPARATOR_CHAR="\\\\"
    JAVA_PATH_SEPARATOR=";"
    SHELLEXT=".cmd"
    CPU_CORES=1
    ;;
  sunos | solaris*)
    PLATFORM=SOLARIS
    FILE_SEPARATOR_CHAR="/"
    JAVA_PATH_SEPARATOR=":"
    SHELLEXT=""
    CPU_CORES=1
    ;;
  darwin*)
    PLATFORM=MAC_OSX
    FILE_SEPARATOR_CHAR="/"
    JAVA_PATH_SEPARATOR=":"
    SHELLEXT=""
    CPU_CORES=1
    ;;
  *)
    AC_MSG_ERROR([unknown host operation system '$host_os'. Please specify host as (cpu-vendor-os or cpu-os). Supported host operating systems: $host_supported_os])
    ;;
esac

dnl ---------------------------- directories ---------------------------

dnl --------------------- --with/--without options ---------------------

AC_ARG_WITH(
  config-dir,
  AC_HELP_STRING([--with-config-dir=<path>],[configuration directory (default: /etc/bar)]),
  [CONFIG_DIR=$withval],
  [CONFIG_DIR=/etc/bar]
)
AC_ARG_WITH(
  runtime-dir,
  AC_HELP_STRING([--with-runtime-dir=<path>],[runtime directory (default: /var/lib/bar)]),
  [RUNTIME_DIR=$withval],
  [RUNTIME_DIR=/var/lib/bar]
)
AC_ARG_WITH(
  tls-dir,
  AC_HELP_STRING([--with-tls-dir=<path>],[TLS/SLL configuration and certificate directory (default: /etc/ssl)]),
  [TLS_DIR=$withval],
  [TLS_DIR=/etc/ssl]
)
AC_ARG_WITH(
  swt-dir,
  AC_HELP_STRING([--with-swt-dir=<path>],[SWT directory]),
  [SWT_DIR=$withval]
)
AC_ARG_WITH(
  launch4j-dir,
  AC_HELP_STRING([--with-launch4j-dir=<path>],[launch4j directory]),
  [LAUNCH4J_DIR=$withval]
)

AC_ARG_WITH(
  heapview,
  AC_HELP_STRING([--with-heapview=<path>],[heapview directory]),
  [HEAPVIEW_DIR=$withval]
)

dnl ------------------- --enable/--disable options ---------------------

AC_ARG_ENABLE(
  debug,
  AC_HELP_STRING([--enable-debug],[enable debug version]),
  [ENABLE_DEBUG=$enableval]
)
AC_ARG_ENABLE(
  link-static,
  AC_HELP_STRING([--disable-link-static],[disable static linkage (same as --enable-link-dynamic)]),
  [if test "$enableval" = "no"; then ENABLE_LINK_DYNAMIC=yes; fi]
)
AC_ARG_ENABLE(
  link-dynamic,
  AC_HELP_STRING([--enable-link-dynamic],[enable dynamic linkage (same as --disable-link-static)]),
  [ENABLE_LINK_DYNAMIC=$enableval]
)
AC_ARG_ENABLE(
  bz2,
  AC_HELP_STRING([--disable-bz2],[disable bzip2 support]),
  [ENABLE_BZ2=$enableval]
)
AC_ARG_ENABLE(
  lzma,
  AC_HELP_STRING([--disable-lzma],[disable lzma/xz support]),
  [ENABLE_LZMA=$enableval]
)
AC_ARG_ENABLE(
  xz,
  AC_HELP_STRING([--disable-xz],[disable lzma/xz support]),
  [ENABLE_LZMA=$enableval]
)
AC_ARG_ENABLE(
  lzo,
  AC_HELP_STRING([--disable-lzo],[disable lzo support]),
  [ENABLE_LZO=$enableval]
)
AC_ARG_ENABLE(
  lz4,
  AC_HELP_STRING([--disable-lz4],[disable lz4 support]),
  [ENABLE_LZ4=$enableval]
)
AC_ARG_ENABLE(
  zstd,
  AC_HELP_STRING([--disable-zstd],[disable zstd support]),
  [ENABLE_ZSTD=$enableval]
)
AC_ARG_ENABLE(
  xdelta3,
  AC_HELP_STRING([--disable-xdelta3],[disable xdelta3 support]),
  [ENABLE_XDELTA3=$enableval]
)
AC_ARG_ENABLE(
  gcrypt,
  AC_HELP_STRING([--disable-gcrypt],[disable gcrypt support]),
  [ENABLE_GCRYPT=$enableval]
)
AC_ARG_ENABLE(
  ftp,
  AC_HELP_STRING([--disable-ftp],[disable FTP support]),
  [ENABLE_FTP=$enableval]
)
AC_ARG_ENABLE(
  curl,
  AC_HELP_STRING([--disable-curl],[disable curl support]),
  [ENABLE_CURL=$enableval]
)
AC_ARG_ENABLE(
  ssh,
  AC_HELP_STRING([--disable-ssh],[disable SSH support]),
  [ENABLE_SSH=$enableval]
)
AC_ARG_ENABLE(
  tls,
  AC_HELP_STRING([--disable-tls],[disable TLS support]),
  [ENABLE_TLS=$enableval]
)
AC_ARG_ENABLE(
  iso9660,
  AC_HELP_STRING([--disable-iso9660],[disable ISO 9660 support]),
  [ENABLE_ISO9660=$enableval]
)
AC_ARG_ENABLE(
  pcre,
  AC_HELP_STRING([--disable-pcre],[disable PCRE support]),
  [ENABLE_PCRE=$enableval]
)
AC_ARG_ENABLE(
  crashdump,
  AC_HELP_STRING([--disable-crashdump],[disable crash dump support]),
  [ENABLE_BREAKPAD=$enableval]
)
AC_ARG_ENABLE(
  bfd,
  AC_HELP_STRING([--disable-bfd],[disable BFD support]),
  [ENABLE_BFD=$enableval]
)
AC_ARG_ENABLE(
  gui,
  AC_HELP_STRING([--disable-gui],[disable GUI support (barcontrol)]),
  [ENABLE_GUI=$enableval]
)
AC_ARG_ENABLE(
  gui-i18n,
  AC_HELP_STRING([--disable-gui-i18n],[disable GUI localization support (barcontrol)]),
  [ENABLE_GUI_I18N=$enableval]
)
AC_ARG_ENABLE(
  epm,
  AC_HELP_STRING([--disable-epm],[disable EPM support]),
  [ENABLE_EPM=$enableval]
)
AC_ARG_ENABLE(
  package-compile,
  AC_HELP_STRING([--disable-package-compile],[disable compilation of external packages]),
  [ENABLE_PACKAGE_COMPILE=$enableval]
)

if test $ENABLE_TLS = "yes" -a ! $ENABLE_GCRYPT = "yes"; then
  AC_MSG_WARN([gcrypt not enabled, disabled TLS.])
  ENABLE_TLS=no
fi
if test $ENABLE_CURL = "yes" -a ! $ENABLE_GCRYPT = "yes"; then
  AC_MSG_WARN([gcrypt not enabled, disabled curl support.])
  ENABLE_CURL=no
fi
if test $ENABLE_CURL = "yes" -a ! $ENABLE_SSH = "yes"; then
  AC_MSG_WARN([ssh not enabled, disabled curl support.])
  ENABLE_CURL=no
fi
if test $ENABLE_CURL = "yes" -a ! $ENABLE_TLS = "yes"; then
  AC_MSG_WARN([TLS not enabled, disabled curl support.])
  ENABLE_CURL=no
fi

dnl ------------------------------ programs ----------------------------

if test -z "$CC"; then
  AC_PATH_PROGS(CC,gcc cc)
fi
if test -z "$CC"; then
  AC_MSG_ERROR([Cannot find C compiler in PATH. Please check your PATH environment variable.])
fi
if test -z "$CXX"; then
  AC_PATH_PROGS(CXX,g++ c++)
fi
if test -z "$CXX"; then
  AC_MSG_ERROR([Cannot find C++ compiler in PATH. Please check your PATH environment variable.])
fi
if test -z "$LD"; then
  AC_PATH_PROGS(LD,ld gcc)
fi
if test -z "$STRIP"; then
  AC_PATH_PROGS(STRIP,strip)
fi
if test -z "$OBJDUMP"; then
  AC_PATH_PROGS(OBJDUMP,objdump)
fi
if test -z "$OBJCOPY"; then
  AC_PATH_PROGS(OBJCOPY,objcopy)
fi

AC_PATH_PROGS(MAKE,make)
if test -z "$MAKE"; then
  AC_MSG_ERROR([Cannot find a 'make' in PATH. Please check your PATH environment variable.])
fi
if test -z "`$MAKE --version 2>/dev/null|grep GNU 2>/dev/null`"; then
  AC_MSG_ERROR([$MAKE is not a GNU make. Please install a GNU make.])
fi

AC_PATH_PROGS(PERL,perl)
if test -z "$PERL"; then
  AC_MSG_ERROR([Cannot find a 'perl' in PATH. Please check your PATH environment variable.])
fi

# required for building sqlite3.c
AC_PATH_PROGS(TCLSH,tclsh)
if test -z "$TCLSH"; then
  AC_MSG_ERROR([Cannot find a 'tclsh' in PATH. Please check your PATH environment variable.])
fi

AC_PROG_INSTALL
if test -z "$INSTALL"; then
  AC_MSG_ERROR([Cannot find a 'install' in PATH. Please check your PATH environment variable.])
fi

AC_PATH_PROGS(M4,m4)
if test -z "$M4"; then
  AC_MSG_ERROR([Cannot find a 'm4' in PATH. Please check your PATH environment variable.])
fi

AC_PATH_PROGS(XGETTEXT,xgettext)
if test -z "$XGETTEXT"; then
  AC_MSG_ERROR([Cannot find a 'xgettext' in PATH. Please check your PATH environment variable.])
fi
AC_PATH_PROGS(MSGMERGE,msgmerge)
if test -z "$MSGMERGE"; then
  AC_MSG_ERROR([Cannot find a 'msgmerge' in PATH. Please check your PATH environment variable.])
fi
AC_PATH_PROGS(MSGFMT,msgfmt)
if test -z "$MSGFMT"; then
  AC_MSG_ERROR([Cannot find a 'msgfmt' in PATH. Please check your PATH environment variable.])
fi

AC_PATH_PROGS(SVNVERSION,svnversion)

if test $ENABLE_EPM = "yes"; then
  if test -d epm -o -d "`readlink epm`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile epm

      cwd=`pwd`

      AC_MSG_COMPILE([epm])
      result=1
      if test $result -eq 1; then
        (cd epm; ./configure \
          --host=$build \
          --prefix=$cwd/packages \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd epm; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd epm; make install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    fi

    EPM=$cwd/packages/bin/epm
    AC_PROVIDE(AC_PROG_EPM)
  else
    AC_PATH_PROGS(EPM,epm)
  fi
fi
if test -n "$EPM"; then
  AC_PROG_EPM_VERSION
fi

AC_PATH_PROGS(MD5SUM,md5sum)

AC_COMPILER_OPTION(-Wall,                        CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_COMPILER_OPTION(-Wpointer-arith,              CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_COMPILER_OPTION(-Wunused-label,               CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_COMPILER_OPTION(-Wunused-parameter,           CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_COMPILER_OPTION(-Wunused-variable,            CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_COMPILER_OPTION(-Wmissing-prototypes,         CC_WARN_FLAGS)
AC_COMPILER_OPTION(-Wmissing-declarations,       CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_COMPILER_OPTION(-Wextra,                      CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_COMPILER_OPTION(-Wfloat-equal,                CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_COMPILER_OPTION(-Wdeclaration-after-statement,CC_WARN_FLAGS)
AC_COMPILER_OPTION(-Wbad-function-cast,          CC_WARN_FLAGS)
AC_COMPILER_OPTION(-Wsign-compare,               CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_COMPILER_OPTION(-Wstrict-prototypes,          CC_WARN_FLAGS)
AC_COMPILER_OPTION(-Wno-unknown-pragmas,         CC_WARN_FLAGS CXX_WARN_FLAGS)

CC_OPTIMIZE="$CC_OPTIMIZE"
CC_DEFINES="$CC_DEFINES"

CC_OPTIMIZE_RELEASE="$CC_OPTIMIZE_RELEASE -O2"
dnl Check: bug in gcc with -fschedule-insns2? If -O2 is used and -fno-schedule-insns2 is
dnl        not given, the program either crashes or parsing a string with %y seems to fail.
AC_COMPILER_OPTION(-fno-schedule-insns2,CC_OPTIMIZE_RELEASE)
CC_DEFINES_RELEASE="$CC_DEFINES_RELEASE NDEBUG"

CC_OPTIMIZE_DEBUG="$CC_OPTIMIZE_DEBUG -O0"
CC_DEFINES_DEBUG="$CC_DEFINES_DEBUG"

CC_INCLUDES="$CC_INCLUDES"
case "$host_os" in
  linux-*)
    ;;
  *)
    ;;
esac

LD_FLAGS=""
if test ! $ENABLE_LINK_DYNAMIC = "yes"; then
  LD_FLAGS="-static-libgcc"
fi
LIBRARY_PATHS="$LIBRARY_PATHS /usr/local/lib"
case "$host_os" in
  linux-*)
    if test "$ENABLE_DEBUG" = "yes"; then
      # required for printing stacktraces
      AC_COMPILER_OPTION(-rdynamic,CC_FLAGS CXX_FLAGS LD_FLAGS)
    fi
    ;;
  *)
    ;;
esac

if test "$ENABLE_GUI" = "yes"; then
  AC_PATH_PROGS(JAVA,java)
  AC_PROVIDE(AC_PROG_JAVA)
  if test -z "$JAVA"; then
    AC_MSG_WARN([No 'java' command found in path - cannot execute GUI])
    ENABLE_GUI="no"
  fi
  AC_PATH_PROGS(JAVAC,javac)
  AC_PROVIDE(AC_PROG_JAVAC)
  if test -z "$JAVAC"; then
    AC_MSG_WARN([No 'javac' command found in path - cannot compile GUI])
    ENABLE_GUI="no"
  fi
  AC_PATH_PROGS(JAR,jar)
  AC_PROVIDE(AC_PROG_JAR)
  if test -z "$JAVAC"; then
    AC_MSG_WARN([No 'jar' command found in path - cannot compile GUI])
    ENABLE_GUI="no"
  fi
  if test "$ENABLE_GUI_I18N" = "yes"; then
    AC_PATH_PROGS(XGETTEXT,xgettext)
    AC_PATH_PROGS(MSGMERGE,msgmerge)
    AC_PATH_PROGS(MSGFMT,msgfmt)
    if test -z "$XGETTEXT"; then
      AC_MSG_WARN([No 'xgettext' command found in path - cannot use localize application])
      ENABLE_GUI_I18N="no"
    fi
    if test -z "$MSGMERGE"; then
      AC_MSG_WARN([No 'msgmerge' command found in path - cannot use localize application])
      ENABLE_GUI_I18N="no"
    fi
    if test -z "$MSGFMT"; then
      AC_MSG_WARN([No 'msgfmt' command found in path - cannot use localize application])
      ENABLE_GUI_I18N="no"
    fi
  fi
  AC_PATH_PROGS(LAUNCH4J,launch4j)

  if test -n "$JAVA"; then
    AC_JAVA_VERSION(JAVA_VERSION)
    case "$JAVA_VERSION" in
      1.6*)
        ;;
      1.7*)
        ;;
      1.8*)
        ;;
      *)
        AC_MSG_WARN([Insufficient Java version (found $JAVA_VERSION, require 1.6, 1.7 or 1.8) - disabled compile GUI])
        ENABLE_GUI="no"
        ENABLE_GUI_I18N="no"
        ;;
    esac
  fi

  if test -n "$JAVAC" -a -n "$JAVA"; then
    AC_JAVA_DATA_MODEL(JAVA_DATA_MODEL)
  fi

  JAVA_FLAGS=""
  if test "$ENABLE_DEBUG" = "yes"; then
    JAVA_FLAGS="-enableassertions"
  fi
  if test "$ENABLE_DEBUG" = "yes"; then
    JAVAC_FLAGS="-g"
  fi
fi

AC_PATH_PROGS(CERTTOOL,certtool)
AC_PATH_PROGS(OPENSSL,openssl)
AC_PATH_PROGS(KEYTOOL,keytool)

AC_PATH_PROGS(UNOCONV,unoconv)
if test -z "$UNOCONV"; then
  AC_MSG_WARN([No 'unoconv' tool found. Cannot create PDF manual. Please check PATH or create PDF manual manually in OO.])
fi
AC_PATH_PROGS(TXT2MAN,txt2man)
if test -z "$TXT2MAN"; then
  AC_MSG_WARN([No 'txt2man' tool found. Cannot create man page. Please check PATH.])
fi

AC_PATH_PROGS(SUDO,sudo,,/usr/bin)
AC_PATH_PROGS(ATTR,attr,,/usr/bin)
AC_PATH_PROGS(TIME,time,,/usr/bin)
AC_PATH_PROGS(GETFATTR,getfattr,,/usr/bin)
AC_PATH_PROGS(SETFATTR,setfattr,,/usr/bin)

AC_PATH_PROGS(MKFS_EXT2,    mkfs.ext2,    ,/sbin)
AC_PATH_PROGS(MKFS_EXT3,    mkfs.ext3,    ,/sbin)
AC_PATH_PROGS(MKFS_EXT4,    mkfs.ext4,    ,/sbin)
AC_PATH_PROGS(MKFS_MSDOS,   mkfs.msdos,   ,/sbin)
AC_PATH_PROGS(MKFS_REISERFS,mkfs.reiserfs,,/sbin)
AC_PATH_PROGS(MKFS_REISER4, mkfs.reiser4, ,/sbin)

# sudo tests
AC_PROG_SUDO_COMMAND([chattr +i],[chattr +i $tmpFile],HAVE_SUDO_COMMAND_CHATTR_I=1,HAVE_SUDO_COMMAND_CHATTR_I=0)
AC_PROG_SUDO_COMMAND([chattr +a],[chattr +a $tmpFile],HAVE_SUDO_COMMAND_CHATTR_A=1,HAVE_SUDO_COMMAND_CHATTR_A=0)
SUDO_ASKPASS=false $SUDO -n chattr -i $tmpFile 1</dev/null 2>/dev/null
SUDO_ASKPASS=false $SUDO -n chattr -a $tmpFile 1</dev/null 2>/dev/null

dnl -------- mandatory headers/functions/definitions/libraries ---------

case $host_os in
  linux*)
    AC_SEARCH_LIBRARIES(round,m,[LIBRARIES="$LIBRARIES m"],AC_ERROR([m library not found]))
    AC_SEARCH_LIBRARIES(dlopen,dl,[LIBRARIES="$LIBRARIES dl"])
    AC_SEARCH_LIBRARIES(clock_gettime,rt,[LIBRARIES="$LIBRARIES rt"],AC_ERROR([rt library not found]))
    AC_SEARCH_LIBRARIES(pthread_create,pthread,[LIBRARIES="$LIBRARIES pthread"],AC_ERROR([pthread library not found]))
    ;;
  mingw*)
    AC_SEARCH_STATIC_LIBRARIES(pthread_create,pthreadGC2,[STATIC_LIBRARIES="$STATIC_LIBRARIES pthreadGC2"],,pthread.h)
    AC_SEARCH_STATIC_LIBRARIES(WSAStartup,wsock32,[STATIC_LIBRARIES="$STATIC_LIBRARIES wsock32"],,winsock2.h)
    ;;
  *)
    AC_ERROR([Unsupported operating system '$host_os'])
    ;;
esac

dnl ------------------------ optional packages -------------------------

# create packages directory
cwd=`pwd`
mkdir packages 2>/dev/null
if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
  (cd packages; rm -rf include; mkdir include 2>/dev/null)
  (cd packages; rm -rf lib; mkdir lib 2>/dev/null)
fi
(cd packages; rm -f lib64; ln -s lib lib64 2>/dev/null)

# add package include/library search directory
CC_INCLUDES="$CC_INCLUDES $cwd/packages/include"
LIBRARY_PATHS="$cwd/packages/lib $LIBRARY_PATHS"

# set variables for compile packages
AC_SET_TOOLCHAIN_SETTINGS($CC_FLAGS,
                          $CXX_FLAGS,
                          $CC_DEFINES,
                          $CC_INCLUDES,
                          $LD_FLAGS,
                          $LIBRARY_PATHS,
                          $STATIC_LIBRARIES,
                          $LIBRARIES)

# create sqlite3.c
if test -d sqlite -o -d "`readlink sqlite`"; then
  if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
    dnl create sqlite3.c almagation

    AC_MSG_CREATE([sqlite3.c])
    result=1
    if test $result -eq 1; then
      (cd sqlite; CFLAGS="-DSQLITE_ENABLE_UPDATE_DELETE_LIMIT" ./configure \
        --host=$host \
        --disable-tcl \
        --enable-fts4\
      ) 1>$tmpFile 2>&1
      if test $? -ne 0; then
        AC_MSG_RESULT([fail (configure)])
        AC_MSG_OUTPUT_LOG($tmpFile)
        result=0
      fi
    fi
    if test $result -eq 1; then
      (cd sqlite; make clean; make sqlite3.c) 1>$tmpFile 2>&1
      if test $? -ne 0; then
        AC_MSG_RESULT([fail (make)])
        AC_MSG_OUTPUT_LOG($tmpFile)
        result=0
      fi
    fi
    if test $result -eq 1; then
      (cd sqlite; \
       install -d $cwd/packages/sources; \
       install sqlite3.c $cwd/packages/sourcesp; \
       install sqlite3.h $cwd/packages/include; \
      ) 1>$tmpFile 2>&1
    fi
    if test $result -eq 1; then
      AC_MSG_RESULT([done])
    else
      AC_MSG_ERROR([Cannot create sqlite3.c])
    fi
  fi
fi

# compile/use icu
if test -d icu -o -d "`readlink icu`"; then
  if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
    dnl compile icu library

    cwd=`pwd`

    AC_MSG_COMPILE([icu])
    result=1
    if test $result -eq 1; then
      case $host_os in
        mingw*)
          # patch to disable usage of lib-command when RANLIB is set
          # patch created with:
          ;;
        *)
          ;;
      esac
    fi

    # build host
    if test $result -eq 1; then
      install -d icu/build/$build_cpu
      (cd icu/build/$build_cpu; CC= CXX= AR= RANLIB= ../../source/configure \
        --host=$build \
        --prefix=$cwd/packages \
        --disable-shared \
        --enable-static \
        --disable-tests \
        --disable-samples \
      ) 1>$tmpFile 2>&1
      if test $? -ne 0; then
        AC_MSG_RESULT([fail (configure)])
        AC_MSG_OUTPUT_LOG($tmpFile)
        result=0
      fi
    fi
    if test $result -eq 1; then
      (cd icu/build/$build_cpu; make clean; make -j$CPU_CORES `if test -n "$LIBTOOL"; then echo LIBTOOL=$LIBTOOL; fi`) 1>$tmpFile 2>&1
      if test $? -ne 0; then
        AC_MSG_RESULT([fail (make)])
        AC_MSG_OUTPUT_LOG($tmpFile)
        result=0
      fi
    fi

    if test "$build" != "$host"; then
      # build target
      install -d icu/build/$host_cpu
      if test $result -eq 1; then
        (cd icu/build/$host_cpu; ../../source/configure \
          --host=$host \
          --with-cross-build=$cwd/icu/build/$build_cpu \
          --prefix=$cwd/packages \
          --disable-shared \
          --enable-static \
          --disable-tests \
          --disable-samples \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd icu/build/$host_cpu; make clean; make -j$CPU_CORES `if test -n "$LIBTOOL"; then echo LIBTOOL=$LIBTOOL; fi`) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
    fi

    if test $result -eq 1; then
      (cd icu/build/$host_cpu; make install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
      if test $? -ne 0; then
        AC_MSG_RESULT([fail (install)])
        AC_MSG_OUTPUT_LOG($tmpFile)
        result=0
      fi
      case $host_os in
        mingw*)
          (cd packages/lib; ln -s libicuuc.lib libicuuc.a) 1>/dev/null 2>/dev/null
          (cd packages/lib; ln -s libicui18n.lib libicui18n.a) 1>/dev/null 2>/dev/null
          (cd packages/lib; ln -s libicudata.lib libicudata.a) 1>/dev/null 2>/dev/null
          ;;
        *)
          ;;
      esac
    fi
    if test $result -eq 1; then
      AC_MSG_RESULT([done])
    fi
  else
    result=1
  fi

  if test $result -eq 1; then
    HAVE_ICU=1
    STATIC_LIBRARIES="$STATIC_LIBRARIES icuuc icui18n icudata"
    AC_DEFINE(HAVE_ICU,1,[icu installed (local)])
  fi

  # get ICU version (build)
  AC_TRY_LINK([#include <stdio.h>
               #include "unicode/uvernum.h"
              ],
              [printf("%s\n",U_ICU_VERSION);],
              VERSION_ICU=`./conftest$ac_exeext`,
              VERSION_ICU=unknown
             )
else
  dnl search for installed icuuc, icui18n, icudata library
  if test $ENABLE_LINK_DYNAMIC = "yes"; then
    AC_SEARCH_LIBRARIES(u_strToUpper,icuuc,[HAVE_ICUUC=1;LIBRARIES="$LIBRARIES icuuc";AC_DEFINE(HAVE_ICUUC,1,[icuuc installed])])
  else
    if test -z "$HAVE_ICUUC"; then
      AC_SEARCH_STATIC_LIBRARIES(u_strToUpper,icuuc,[HAVE_ICUUC=1;STATIC_LIBRARIES="$STATIC_LIBRARIES icuuc";AC_DEFINE(HAVE_ICUUC,1,[static icuuc installed])])
    fi
    if test -z "$HAVE_ICUUC"; then
      AC_SEARCH_LIBRARIES(u_strToUpper,icuuc,[HAVE_ICUUC=1;LIBRARIES="$LIBRARIES icuuc";AC_DEFINE(HAVE_ICUUC,1,[icuuc installed])])
    fi
  fi
  if test $ENABLE_LINK_DYNAMIC = "yes"; then
    AC_SEARCH_LIBRARIES(uregex_open,icui18n,[HAVE_ICUI18N=1;LIBRARIES="$LIBRARIES icui18n";AC_DEFINE(HAVE_ICUI18N,1,[icui18n installed])])
  else
    if test -z "$HAVE_ICUI18N"; then
      AC_SEARCH_STATIC_LIBRARIES(uregex_open,icui18n,[HAVE_ICUI18N=1;STATIC_LIBRARIES="$STATIC_LIBRARIES icui18n";AC_DEFINE(HAVE_ICUI18N,1,[static icui18n installed])])
    fi
    if test -z "$HAVE_ICUI18N"; then
      AC_SEARCH_LIBRARIES(uregex_open,icui18n,[HAVE_ICUI18N=1;LIBRARIES="$LIBRARIES icui18n";AC_DEFINE(HAVE_ICUI18N,1,[icui18n installed])])
    fi
  fi
  if test $ENABLE_LINK_DYNAMIC = "yes"; then
    AC_SEARCH_LIBRARIES(icudt,icudata,[HAVE_ICUDATA=1;LIBRARIES="$LIBRARIES icudata";AC_DEFINE(HAVE_ICUI18N,1,[icudata installed])])
  else
    if test -z "$HAVE_ICUDATA"; then
      AC_SEARCH_STATIC_LIBRARIES(icudt,icudata,[HAVE_ICUDATA=1;STATIC_LIBRARIES="$STATIC_LIBRARIES icudata";AC_DEFINE(HAVE_ICUI18N,1,[static icudata installed])])
    fi
    if test -z "$HAVE_ICUDATA"; then
      AC_SEARCH_LIBRARIES(icudt,icudata,[HAVE_ICUDATA=1;LIBRARIES="$LIBRARIES icudata";AC_DEFINE(HAVE_ICUI18N,1,[icudata installed])])
    fi
  fi

  # get ICU version
  AC_TRY_LINK([#include <stdio.h>
               #include "unicode/uvernum.h"
              ],
              [printf("%s\n",U_ICU_VERSION);],
              VERSION_ICU=`./conftest$ac_exeext`,
              VERSION_ICU=unknown
             )
fi

# compile/use zlib
if test -d zlib -o -d "`readlink zlib`"; then
  if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
    dnl compile zlib library

    AC_MSG_COMPILE([zlib])
    result=1
    if test $result -eq 1; then
      (cd zlib; ./configure \
        --prefix=$cwd/packages \
        --static \
      ) 1>$tmpFile 2>&1
      if test $? -ne 0; then
        AC_MSG_RESULT([fail (configure)])
        AC_MSG_OUTPUT_LOG($tmpFile)
        result=0
      fi
    fi
    if test $result -eq 1; then
      (cd zlib; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
      if test $? -ne 0; then
        AC_MSG_RESULT([fail (make)])
        AC_MSG_OUTPUT_LOG($tmpFile)
        result=0
      fi
    fi
    if test $result -eq 1; then
      (cd zlib; make install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
      if test $? -ne 0; then
        AC_MSG_RESULT([fail (make)])
        AC_MSG_OUTPUT_LOG($tmpFile)
        result=0
      fi
    fi
    if test $result -eq 1; then
      AC_MSG_RESULT([done])
    fi
  else
    result=1
  fi

  if test $result -eq 1; then
    HAVE_Z=1
    STATIC_LIBRARIES="$STATIC_LIBRARIES z"
    AC_DEFINE(HAVE_Z,1,[zlib installed (local)])
  fi
else
  dnl search for installed zlib library
  if test $ENABLE_LINK_DYNAMIC = "yes"; then
    AC_SEARCH_LIBRARIES(zlibVersion,z,[HAVE_Z=1;LIBRARIES="z $LIBRARIES";AC_DEFINE(HAVE_Z,1,[z installed])])
  else
    if test -z "$HAVE_Z"; then
      AC_SEARCH_STATIC_LIBRARIES(zlibVersion,z,[HAVE_Z=1;STATIC_LIBRARIES="$STATIC_LIBRARIES z";AC_DEFINE(HAVE_Z,1,[z installed])])
    fi
    if test -z "$HAVE_Z"; then
      AC_SEARCH_LIBRARIES(zlibVersion,z,[HAVE_Z=1;LIBRARIES="z $LIBRARIES";AC_DEFINE(HAVE_Z,1,[z installed])])
    fi
  fi
fi
if test -z "$HAVE_Z"; then
  if test ! \( -d zlib -o -d "`readlink zlib`" \); then
    AC_MSG_WARN([zlib not compiled: zlib package missing])
  fi
fi

# compile/use bz2
if test $ENABLE_BZ2 = "yes"; then
  if test -d bzip2 -o -d "`readlink bzip2`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile bzip2 library

      AC_MSG_COMPILE([bzip2])
      result=1
      if test $result -eq 1; then
        if test "$build" == "$host"; then
          (cd bzip2; make clean; make -j$CPU_CORES libbz2.a) 1>$tmpFile 2>&1
          if test $? -ne 0; then
            AC_MSG_RESULT([fail (make)])
            AC_MSG_OUTPUT_LOG($tmpFile)
            result=0
          fi
        else
          (cd bzip2; make clean; make -j$CPU_CORES CC=$CC AR=$AR RANLIB=$RANLIB libbz2.a) 1>$tmpFile 2>&1
          if test $? -ne 0; then
            AC_MSG_RESULT([fail (make)])
            AC_MSG_OUTPUT_LOG($tmpFile)
            result=0
          fi
        fi
      fi
      if test $result -eq 1; then
        (cd bzip2; make install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      HAVE_BZ2=1
      STATIC_LIBRARIES="$STATIC_LIBRARIES bz2"
      AC_DEFINE(HAVE_BZ2,1,[bzip2 installed (local)])
    fi
  else
    dnl search for installed bzip2 library
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(BZ2_bzCompressInit,bz2,[HAVE_BZ2=1;LIBRARIES="bz2 $LIBRARIES";AC_DEFINE(HAVE_BZ2,1,[bzip2 installed])])
    else
      if test -z "$HAVE_BZ2"; then
        AC_SEARCH_STATIC_LIBRARIES(BZ2_bzCompressInit,bz2,[HAVE_BZ2=1;STATIC_LIBRARIES="$STATIC_LIBRARIES bz2";AC_DEFINE(HAVE_BZ2,1,[static bzip2 installed])])
      fi
      if test -z "$HAVE_BZ2"; then
        AC_SEARCH_LIBRARIES(BZ2_bzCompressInit,bz2,[HAVE_BZ2=1;LIBRARIES="bz2 $LIBRARIES";AC_DEFINE(HAVE_BZ2,1,[bzip2 installed])])
      fi
    fi
  fi

  if test -z "$HAVE_BZ2"; then
    if test ! \( -d bzip2 -o -d "`readlink bzip2`" \); then
      AC_MSG_WARN([bzip2 not compiled: bzip2 package missing])
    fi
  fi
fi

# compile/use lzma
if test $ENABLE_LZMA = "yes"; then
  if test -d xz -o -d "`readlink xz`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile lzma library

      AC_MSG_COMPILE([xz utils])
      result=1
      if test $result -eq 1; then
        case $host_os in
          mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            # patch created with:
            #   diff -u xz-5.0.4.org/m4/libtool.m4 xz-5.0.4/m4/libtool.m4 xz-5.0.4-libtool.patch
#            if test -d packages/xz-5.0.4; then
#              (cd packages/xz-5.0.4; patch --batch -N -p1 <$cwd/misc/xz-5.0.4-libtool.patch) 1>/dev/null 2>/dev/null
#            else
#              AC_MSG_WARN([xz-5.0.4 not available - skipped libtool patch!])
#            fi
            ;;
          *)
            ;;
        esac
      fi
      if test $result -eq 1; then
        (cd xz; ./configure \
          --host=$host \
          --prefix=$cwd/packages \
          --disable-shared \
          --enable-static \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd xz; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd xz; make install) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      HAVE_LZMA=1
      STATIC_LIBRARIES="$STATIC_LIBRARIES lzma"
      AC_DEFINE(HAVE_LZMA,1,[lzma installed (local)])
    fi
  else
    dnl search for installed lzma library
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(lzma_easy_encoder,lzma,[HAVE_LZMA=1;LIBRARIES="lzma $LIBRARIES";AC_DEFINE(HAVE_LZMA,1,[lzma installed])])
    else
      if test -z "$HAVE_LZMA"; then
        AC_SEARCH_STATIC_LIBRARIES(lzma_easy_encoder,lzma,[HAVE_LZMA=1;STATIC_LIBRARIES="$STATIC_LIBRARIES lzma";AC_DEFINE(HAVE_LZMA,1,[static lzma installed])])
      fi
      if test -z "$HAVE_LZMA"; then
        AC_SEARCH_LIBRARIES(lzma_easy_encoder,lzma,[HAVE_LZMA=1;LIBRARIES="lzma $LIBRARIES";AC_DEFINE(HAVE_LZMA,1,[lzma installed])])
      fi
    fi
  fi

  if test -z "$HAVE_LZMA"; then
    if test ! \( -d xz -o -d "`readlink xz`" \); then
      AC_MSG_WARN([lzma not compiled: xz package missing])
    fi
  fi
fi

# compile/use lzo
if test $ENABLE_LZO = "yes"; then
  if test -d lzo -o -d "`readlink lzo`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile lzo library

      AC_MSG_COMPILE([lzo])
      result=1
      if test $result -eq 1; then
        install -d lzo
        (cd lzo; ./configure \
          --host=$host \
          --prefix=$cwd/packages \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd lzo; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd lzo; make install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      HAVE_LZO=1
      STATIC_LIBRARIES="$STATIC_LIBRARIES lzo2"
      AC_DEFINE(HAVE_LZO,1,[lzo installed (local)])
    fi
  else
    dnl search for installed lzo library
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(lzo1x_1_compress,lzo,[HAVE_LZO=1;LIBRARIES="lzo2 $LIBRARIES";AC_DEFINE(HAVE_LZO,1,[lzo installed])])
    else
      if test -z "$HAVE_LZO"; then
        AC_SEARCH_STATIC_LIBRARIES(lzo1x_1_compress,lzo,[HAVE_LZO=1;STATIC_LIBRARIES="$STATIC_LIBRARIES lzo2";AC_DEFINE(HAVE_LZO,1,[static lzo installed])])
      fi
      if test -z "$HAVE_LZO"; then
        AC_SEARCH_LIBRARIES(lzo1x_1_compress,lzo,[HAVE_LZO=1;LIBRARIES="lzo2 $LIBRARIES";AC_DEFINE(HAVE_LZO,1,[lzo installed])])
      fi
    fi
  fi

  if test -z "$HAVE_LZO"; then
    if test ! \( -d lzo -o -d "`readlink lzo`" \); then
      AC_MSG_WARN([lzo not compiled: lzo package missing])
    fi
  fi
fi

# compile/use lz4
if test $ENABLE_LZ4 = "yes"; then
  if test -d lz4 -o -d "`readlink lz4`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile lz4 library

      AC_MSG_COMPILE([lz4])
      result=1
      if test $result -eq 1; then
        (cd lz4; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd lz4; make install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
        rm -f $cwd/packages/lib/liblz4*.so*
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      HAVE_LZ4=1
      STATIC_LIBRARIES="$STATIC_LIBRARIES lz4"
      AC_DEFINE(HAVE_LZ4,1,[lz4 installed (local)])
    fi
  else
    dnl search for installed lz4 library
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(LZ4_compress,lz4,[HAVE_LZ4=1;LIBRARIES="lz4 $LIBRARIES";AC_DEFINE(HAVE_LZ4,1,[lz4 installed])])
    else
      if test -z "$HAVE_LZ4"; then
        AC_SEARCH_STATIC_LIBRARIES(LZ4_compress,lz4,[HAVE_LZ4=1;STATIC_LIBRARIES="$STATIC_LIBRARIES lz4";AC_DEFINE(HAVE_LZ4,1,[static lz4 installed])])
      fi
      if test -z "$HAVE_LZ4"; then
        AC_SEARCH_LIBRARIES(LZ4_compress,lz4,[HAVE_LZ4=1;LIBRARIES="lz4 $LIBRARIES";AC_DEFINE(HAVE_LZ4,1,[lz4 installed])])
      fi
    fi
  fi

  if test -z "$HAVE_LZ4"; then
    if test ! \( -d lz4 -o -d "`readlink lz4`" \); then
      AC_MSG_WARN([lz4 not compiled: lz4 package missing])
    fi
  fi
fi

# compile/use zstd
if test $ENABLE_ZSTD = "yes"; then
  if test -d zstd -o -d "`readlink zstd`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile zstd library

      AC_MSG_COMPILE([zstd])
      result=1
      if test $result -eq 1; then
        (cd zstd; make clean; make -C lib -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd zstd; make -C lib install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
        rm -f $cwd/packages/lib/libzstd*.so*
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      HAVE_ZSTD=1
      CC_DEFINES="$CC_DEFINES ZSTD_STATIC_LINKING_ONLY"
      STATIC_LIBRARIES="$STATIC_LIBRARIES zstd"
      AC_DEFINE(HAVE_ZSTD,1,[zstd installed (local)])
    fi
  else
    dnl search for installed zstd library
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(ZSTD_compress,zstd,[HAVE_ZSTD=1;LIBRARIES="zstd $LIBRARIES";AC_DEFINE(HAVE_ZSTD,1,[zstd installed])])
    else
      if test -z "$HAVE_ZSTD"; then
        AC_SEARCH_STATIC_LIBRARIES(ZSTD_compress,zstd,[HAVE_ZSTD=1;CC_DEFINES="$CC_DEFINES ZSTD_STATIC_LINKING_ONLY";STATIC_LIBRARIES="$STATIC_LIBRARIES zstd";AC_DEFINE(HAVE_ZSTD,1,[static zstd installed])])
      fi
      if test -z "$HAVE_ZSTD"; then
        AC_SEARCH_LIBRARIES(ZSTD_compress,zstd,[HAVE_ZSTD=1;LIBRARIES="zstd $LIBRARIES";AC_DEFINE(HAVE_ZSTD,1,[zstd installed])])
      fi
    fi
  fi

  if test -z "$HAVE_ZSTD"; then
    if test ! \( -d zstd -o -d "`readlink zstd`" \); then
      AC_MSG_WARN([zstd not compiled: zstd package missing])
    fi
  fi
fi

# use xdelta3
if test $ENABLE_XDELTA3 = "yes"; then
  if test -d xdelta3 -o -d "`readlink xdelta3`"; then
    HAVE_XDELTA=1
    HAVE_XDELTA3=1
    CC_INCLUDES="$CC_INCLUDES $cwd/xdelta3"
    AC_DEFINE(HAVE_XDELTA,1,[xdelta installed])
    AC_DEFINE(HAVE_XDELTA3,1,[xdelta3 installed])
  fi
fi

# compile/use gcrypt
if test $ENABLE_GCRYPT = "yes"; then
  if test \( -d libgpg-error -o -d "`readlink libgpg-error`" \) -a \( -d libgcrypt -o -d "`readlink libgcrypt`" \); then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile libgcrypt library

      AC_MSG_COMPILE([libgpg-error])
      result1=1
      if test $result1 -eq 1; then
        (cd libgpg-error; ./configure \
          --host=$host \
          --prefix=$cwd/packages \
          --disable-shared \
          --enable-static \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd libgpg-error; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd libgpg-error; make install) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        AC_MSG_RESULT([done])
      else
        AC_MSG_ERROR([fail])
      fi

      AC_MSG_COMPILE([libgcrypt])
      result2=1
      if test $result2 -eq 1; then
        case $host_os in
          mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            # patch created with:
            #   diff -u libgcrypt-1.5.0.org/m4/libtool.m4 libgcrypt-1.5.0/m4/libtool.m4 >  libgcrypt-1.5.0-libtool.patch
            #   diff -u libgcrypt-1.5.0.org/aclocal.m4 libgcrypt-1.5.0/aclocal.m4       >> libgcrypt-1.5.0-libtool.patch
            #   diff -u libgcrypt-1.5.0.org/configure libgcrypt-1.5.0/configure         >> libgcrypt-1.5.0-libtool.patch
#            if test -d packages/libgcrypt-1.5.0; then
#              (cd packages/libgcrypt-1.5.0; patch --batch -N -p1 <$cwd/misc/libgcrypt-1.5.0-libtool.patch) 1>/dev/null 2>/dev/null
#            else
#              AC_MSG_WARN([libgcrypt-1.5.0 not available - skipped libtool patch!])
#            fi
            ;;
          *)
            ;;
        esac
      fi
      if test $result2 -eq 1; then
        case $host_cpu in
          x86*)
            (cd libgcrypt; LIBS="-L$cwd/packages/lib -lgpg-error" ./configure \
              --host=$host \
              --prefix=$cwd/packages \
              --with-libgpg-error-prefix=$cwd/packages \
              --enable-maintainer-mode \
              --disable-shared \
              --enable-static \
              `if test "$ENABLE_DEBUG" = "yes"; then echo --disable-optimization; fi` \
            ) 1>$tmpFile 2>&1
            ;;
          arm*)
#TODO
            (cd libgcrypt; LIBS="-L$cwd/packages/lib -lgpg-error" ./configure \
              --host=$host \
              --prefix=$cwd/packages \
              --with-libgpg-error-prefix=$cwd/packages \
              --enable-maintainer-mode \
--enable-ciphers="blowfish cast5 des twofish serpent camellia" \
              --disable-shared \
              --enable-static \
              `if test "$ENABLE_DEBUG" = "yes"; then echo --disable-optimization; fi` \
            ) 1>$tmpFile 2>&1
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd libgcrypt; make clean; make -j$CPU_CORES `if test -n "$LIBTOOL"; then echo LIBTOOL=$LIBTOOL; fi`) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd libgcrypt; make install) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
        case $host_os in
          mingw*)
            (cd packages/lib; ln -s libgcrypt.lib libgcrypt.a) 1>/dev/null 2>/dev/null
            ;;
          *)
            ;;
        esac
      fi
      if test $result2 -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result1=1
      result2=1
    fi

    if test $result1 -eq 1 -a $result2 -eq 1; then
      HAVE_GCRYPT=1
      HAVE_GPG_ERROR=1
      CC_DEFINES="$CC_DEFINES GCRYPT_NO_DEPRECATED"
      STATIC_LIBRARIES="$STATIC_LIBRARIES gcrypt gpg-error"
      AC_DEFINE(HAVE_GCRYPT,1,[gcrypt installed (local)])
    fi
  else
    dnl check if suitable gcrypt is installed
    AC_CHECK_TYPE(gcry_cipher_hd_t,
      [
        dnl search for installed gpg-error/gcrypt library
        LIBRARIES="$LIBRARIES dl"
        if test $ENABLE_LINK_DYNAMIC = "yes"; then
          AC_SEARCH_LIBRARIES(gpg_strerror,gpg-error,[HAVE_GPG_ERROR=1;LIBRARIES="$LIBRARIES gpg-error";AC_DEFINE(HAVE_GPG_ERROR,1,[GPG error installed])])
          AC_SEARCH_LIBRARIES(gcry_check_version,gcrypt,[HAVE_GCRYPT=1;LIBRARIES="$LIBRARIES gcrypt";AC_DEFINE(HAVE_GCRYPT,1,[gcrypt installed])])
        else
          if test -z "$HAVE_GPG_ERROR"; then
            AC_SEARCH_STATIC_LIBRARIES(gpg_strerror,gpg-error,[HAVE_GPG_ERROR=1;STATIC_LIBRARIES="$STATIC_LIBRARIES gpg-error";AC_DEFINE(HAVE_GPG_ERROR,1,[static GPG error installed])])
          fi
          if test -z "$HAVE_GPG_ERROR"; then
            AC_SEARCH_LIBRARIES(gpg_strerror,gpg-error,[HAVE_GPG_ERROR=1;LIBRARIES="$LIBRARIES gpg-error";AC_DEFINE(HAVE_GPG_ERROR,1,[GPG error installed])])
          fi
          if test -z "$HAVE_GCRYPT"; then
            AC_SEARCH_STATIC_LIBRARIES(gcry_check_version,gcrypt,[HAVE_GCRYPT=1;STATIC_LIBRARIES="$STATIC_LIBRARIES gcrypt";AC_DEFINE(HAVE_GCRYPT,1,[static gcrypt installed])])
          fi
          if test -z "$HAVE_GCRYPT"; then
            AC_SEARCH_LIBRARIES(gcry_check_version,gcrypt,[HAVE_GCRYPT=1;LIBRARIES="$LIBRARIES gcrypt";AC_DEFINE(HAVE_GCRYPT,1,[gcrypt installed])])
          fi
        fi
      ],
      [
        AC_MSG_WARN([no or wrong version of gcrypt. gcrypt version >=1.2.4 needed. Disabled gcrypt support.]);
        ENABLE_GCRYPT=no
      ],
      [#include <gcrypt.h>]
    )
  fi

  if test -z "$HAVE_GCRYPT"; then
    if test ! \( -d libgpg-error -o -d "`readlink libgpg-error`" \); then
      AC_MSG_WARN([libgcrypt not compiled: libgpg-error package missing])
    fi
    if test ! \( -d libgcrypt -o -d "`readlink libgcrypt`" \); then
      AC_MSG_WARN([libgcrypt not compiled: libgcrypt package missing])
    fi
  fi
fi

# compile/use gnutls
if test $ENABLE_TLS = "yes"; then
  if test \( -d gmp -o -d "`readlink gmp`" \) -a \( -d nettle -o -d "`readlink nettle`" \) -a \( -d gnutls -o -d "`readlink gnutls`" \); then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile gnutls library

      if test -z "$HAVE_GCRYPT"; then
        AC_MSG_WARN([gcrypt is missing. Compiling or linking gnutls may fail!])
      fi

      AC_MSG_COMPILE([gmp])
      result1=1
      if test $result1 -eq 1; then
        # Note: clean is obviously not enough, try distclean
        (cd gmp; make distclean) 1>/dev/null 2>&1
        if test -n "`uname -m|grep x86_64`"; then
          case $host_cpu in
            x86*)
              (cd gmp; CFLAGS= ./configure \
                --host=$host \
                --prefix=$cwd/packages \
                --disable-shared \
                --enable-static \
                --enable-pic \
                --enable-fat \
                ABI=64 \
              ) 1>$tmpFile 2>&1
              ;;
            arm*)
              (cd gmp; CFLAGS= ./configure \
                --host=$host \
                --prefix=$cwd/packages \
                --disable-shared \
                --enable-static \
                --enable-pic \
                --enable-fat \
              ) 1>$tmpFile 2>&1
              ;;
          esac
        else
          (cd gmp; CFLAGS= ./configure \
            --host=$host \
            --prefix=$cwd/packages \
            --disable-shared \
            --enable-static \
            --enable-pic \
            --enable-fat \
            ABI=32 \
          ) 1>$tmpFile 2>&1
        fi
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd gmp; make clean; make -j$CPU_CORES `if test -n "$LIBTOOL"; then echo LIBTOOL=$LIBTOOL; fi`) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd gmp; make install) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        AC_MSG_RESULT([done])
      fi

      AC_MSG_COMPILE([nettle])
      result2=1
      if test $result2 -eq 1; then
        (cd nettle; ./configure \
          --host=$host \
          --prefix=$cwd/packages \
          --with-include-path=$cwd/packages/include \
          --with-lib-path=$cwd/packages/lib \
          --disable-shared \
          --enable-static \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd nettle; make clean; make -j$CPU_CORES `if test -n "$LIBTOOL"; then echo LIBTOOL=$LIBTOOL; fi`) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd nettle; make install) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        AC_MSG_RESULT([done])
      fi

      AC_MSG_COMPILE([gnutls])
      result3=1
      if test $result3 -eq 1; then
        (cd gnutls; GMP_LIBS=-L$cwd/packages/lib ./configure \
          --host=$host \
          --prefix=$cwd/packages \
          --with-libnettle-prefix=$cwd/packages \
          --with-included-libtasn1 \
          --with-included-unistring \
          --disable-shared \
          --enable-static \
          --without-p11-kit \
          --without-tpm \
          --disable-tools \
          --disable-doc \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
      fi
      if test $result3 -eq 1; then
        (cd gnutls; make clean; make -j$CPU_CORES `if test -n "$LIBTOOL"; then echo LIBTOOL=$LIBTOOL; fi`) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
      fi
      if test $result3 -eq 1; then
        (cd gnutls; make install) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
      fi
      if test $result3 -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result1=1
      result2=1
      result3=1
    fi

    if test $result1 -eq 1 -a $result2 -eq 1 -a $result3 -eq 1; then
      HAVE_GNU_TLS=1
      STATIC_LIBRARIES="$STATIC_LIBRARIES gnutls gmp nettle hogweed"
      AC_DEFINE(HAVE_GNU_TLS,1,[gnutls installed (local)])
    fi
  else
    AC_CHECK_TYPE(gnutls_session_t,,AC_MSG_WARN([no or wrong version of gnutls. gnutls version >=2.0.1 needed. Disabled TLS support.]); ENABLE_TLS=no,[#include <gnutls/gnutls.h>])
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(gnutls_global_init,gnutls,[HAVE_GNU_TLS=1;LIBRARIES="$LIBRARIES gnutls";AC_DEFINE(HAVE_GNU_TLS,1,[GNU TLS installed])])
      AC_SEARCH_LIBRARIES(asn1_length_der,tasn1,[HAVE_TASN1=1;LIBRARIES="$LIBRARIES tasn1";AC_DEFINE(HAVE_TASN1,1,[TASN.1 installed])])
    else
      if test -z "$HAVE_GNU_TLS"; then
        AC_SEARCH_STATIC_LIBRARIES(gnutls_global_init,gnutls,[HAVE_GNU_TLS=1;STATIC_LIBRARIES="$STATIC_LIBRARIES gnutls";AC_DEFINE(HAVE_GNU_TLS,1,[static GNU TLS installed])])
      fi
      if test -z "$HAVE_GNU_TLS"; then
        AC_SEARCH_LIBRARIES(gnutls_global_init,gnutls,[HAVE_GNU_TLS=1;LIBRARIES="$LIBRARIES gnutls";AC_DEFINE(HAVE_GNU_TLS,1,[GNU TLS installed])])
      fi

      if test -z "$HAVE_TASN1"; then
        AC_SEARCH_STATIC_LIBRARIES(asn1_length_der,tasn1,[HAVE_TASN1=1;STATIC_LIBRARIES="$STATIC_LIBRARIES tasn1";AC_DEFINE(HAVE_TASN1,1,[static TASN.1 installed])])
      fi
      if test -z "$HAVE_TASN1"; then
        AC_SEARCH_LIBRARIES(asn1_length_der,tasn1,[HAVE_TASN1=1;LIBRARIES="$LIBRARIES tasn1";AC_DEFINE(HAVE_TASN1,1,[TASN.1 installed])])
      fi
    fi
  fi

  if test -z "$HAVE_GNU_TLS"; then
    if test ! \( -d gmp -o -d "`readlink gmp`" \); then
      AC_MSG_WARN([gnutls not compiled: gmp package missing])
    fi
    if test ! \( -d nettle -o -d "`readlink nettle`" \); then
      AC_MSG_WARN([gnutls not compiled: nettle package missing])
    fi
    if test ! \( -d gnutls -o -d "`readlink gnutls`" \); then
      AC_MSG_WARN([gnutls not compiled: gnutls package missing])
    fi
  fi
fi

# compile/use openssl
if test $ENABLE_SSH = "yes"; then
  if test -d openssl -o -d "`readlink openssl`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile openssl library

      AC_MSG_COMPILE([openssl])
      result=1
      if test $result -eq 1; then
        case "$host_os" in
          linux-*)
            if test -n "`uname -m|grep x86_64`"; then
              (cd openssl; ./Configure \
                --prefix=$cwd/packages \
                linux-x86_64 -Wa,--noexecstack \
              ) 1>$tmpFile 2>&1
              if test $? -ne 0; then
                AC_MSG_RESULT([fail (configure)])
                AC_MSG_OUTPUT_LOG($tmpFile)
                result=0
              fi
            else
              (cd openssl; ./Configure \
                --prefix=$cwd/packages \
                linux-generic32 -Wa,--noexecstack \
              ) 1>$tmpFile 2>&1
              if test $? -ne 0; then
                AC_MSG_RESULT([fail (configure)])
                AC_MSG_OUTPUT_LOG($tmpFile)
                result=0
              fi
            fi
            ;;
          mingw*)
            (cd openssl; ./Configure \
              --prefix=$cwd/packages \
              mingw \
            ) 1>$tmpFile 2>&1
            if test $? -ne 0; then
              AC_MSG_RESULT([fail (configure)])
              AC_MSG_OUTPUT_LOG($tmpFile)
              result=0
            fi
            ;;
          *)
            AC_MSG_ERROR([Support for '$host_os' still not implemented])
            ;;
        esac
      fi
      if test $result -eq 1; then
        # Note: OpenSSL does not support "make -j..."
        (cd openssl; make clean; make `if test -n "$LIBTOOL"; then echo LIBTOOL=$LIBTOOL; fi`) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd openssl; make all install_sw) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      HAVE_SSL=1
      STATIC_LIBRARIES="$STATIC_LIBRARIES crypto ssl"
      AC_DEFINE(HAVE_SSL,1,[ssl installed (local)])
    fi
  fi

  if test -d libssh2 -o -d "`readlink libssh2`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile ssh2 library

      AC_MSG_COMPILE([libssh2])
      result=1
      if test $result -eq 1; then
        case $host_os in
          mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            # patch created with:
            #   diff -u libssh2-1.4.2.org/m4/libtool.m4 libssh2-1.4.2/m4/libtool.m4 >  libssh2-1.4.2-libtool.patch
            #   diff -u libssh2-1.4.2.org/aclocal.m4 libssh2-1.4.2/aclocal.m4       >> libssh2-1.4.2-libtool.patch
            #   diff -u libssh2-1.4.2.org/configure libssh2-1.4.2/configure         >> libssh2-1.4.2-libtool.patch
#            if test -d packages/libssh2-1.4.2; then
#              (cd packages/libssh2-1.4.2; patch --batch -N -p1 <$cwd/misc/libssh2-1.4.2-libtool.patch) 1>/dev/null 2>/dev/null
#            else
#              AC_MSG_WARN([libssh2-1.4.2 not available - skipped libtool patch!])
#            fi
            ;;
          *)
            ;;
        esac
      fi
      if test $result -eq 1; then
        (cd libssh2; LIBS="-ldl" ./configure \
          --host=$host \
          --prefix=$cwd/packages \
          --with-libssl-prefix=$cwd/packages \
          --with-libgcrypt-prefix=$cwd/packages \
          --disable-shared --enable-static \
          --enable-debug \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd libssh2; make clean; make -C src `if test -n "$LIBTOOL"; then echo LIBTOOL=$LIBTOOL; fi`) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd libssh2; make install-exec install-data) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
        case $host_os in
          mingw*)
            (cd packages/lib; ln -s libssh2.lib libssh2.a) 1>/dev/null 2>/dev/null
            ;;
          *)
            ;;
        esac
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      HAVE_SSH2=1
      STATIC_LIBRARIES="$STATIC_LIBRARIES ssh2"
      AC_DEFINE(HAVE_SSH2,1,[ssh2 installed (local)])
    fi
  else
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(libssh2_session_init_ex,ssh2,[HAVE_SSH2=1;LIBRARIES="$LIBRARIES ssh2";AC_DEFINE(HAVE_SSH2,1,[SSH2 installed])])
    else
      if test -z "$HAVE_SSH2"; then
        AC_SEARCH_STATIC_LIBRARIES(libssh2_session_init_ex,ssh2,[HAVE_SSH2=1;STATIC_LIBRARIES="$STATIC_LIBRARIES ssh2";AC_DEFINE(HAVE_SSH2,1,[static SSH2 installed])])
      fi
      if test -z "$HAVE_SSH2"; then
        AC_SEARCH_LIBRARIES(libssh2_session_init_ex,ssh2,[HAVE_SSH2=1;LIBRARIES="$LIBRARIES ssh2";AC_DEFINE(HAVE_SSH2,1,[SSH2 installed])])
      fi
    fi
  fi

  if test -z "$HAVE_SSH2"; then
    if test ! \( -d libssh2 -o -d "`readlink libssh2`" \); then
      AC_MSG_WARN([libssh2 not compiled: libssh2 package missing])
    fi
  fi

  dnl link libcrypto, too, if available (may be needed by some libssh2 implementations)
  if test $ENABLE_LINK_DYNAMIC = "yes"; then
    AC_SEARCH_LIBRARIES(MD5_Init,crypto,[HAVE_CRYPTO=1;LIBRARIES="$LIBRARIES crypto";AC_DEFINE(HAVE_CRYPTO,1,[crypto installed])])
  else
    if test -z "$HAVE_CRYPTO"; then
      AC_SEARCH_STATIC_LIBRARIES(MD5_Init,crypto,[HAVE_CRYPTO=1;STATIC_LIBRARIES="$STATIC_LIBRARIES crypto";AC_DEFINE(HAVE_CRYPTO,1,[static crypto installed])])
    fi
    if test -z "$HAVE_CRYPTO"; then
      AC_SEARCH_LIBRARIES(MD5_Init,crypto,[HAVE_CRYPTO=1;LIBRARIES="$LIBRARIES crypto";AC_DEFINE(HAVE_CRYPTO,1,[crypto installed])])
    fi
  fi
fi

# compile/use curl
if test $ENABLE_CURL = "yes"; then
  if test \( -d curl -o -d "`readlink curl`" \) -a \( -d c-ares -o -d "`readlink c-ares`" \) -a \( -d mxml -o -d "`readlink mxml`" \); then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile c-ares, curl, mxml libraries

      if test -z "$HAVE_Z"; then
        AC_MSG_WARN([zlib is missing. Compiling or linking curl may fail!])
      fi
      if test -z "$HAVE_GNU_TLS"; then
        AC_MSG_WARN([gnutls is missing. Compiling or linking curl may fail!])
      fi
      if test -z "$HAVE_SSL"; then
        AC_MSG_WARN([libssl is missing. Compiling or linking curl may fail!])
      fi
      if test -z "$HAVE_SSH2"; then
        AC_MSG_WARN([libssh2 is missing. Compiling or linking curl may fail!])
      fi

      AC_MSG_COMPILE([c-ares])
      result1=1
      if test $result1 -eq 1; then
        case $host_os in
          mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            # patch created with:
            #   diff -u curl-7.28.1.org/m4/libtool.m4 curl-7.28.1/m4/libtool.m4 >  curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/aclocal.m4 curl-7.28.1/aclocal.m4       >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/Makefile.in curl-7.28.1/Makefile.in     >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/configure curl-7.28.1/configure         >> curl-7.28.1-libtool.patch
#              if test -d packages/curl-7.28.1; then
#                (cd packages/curl-7.28.1; patch --batch -N -p1 <$cwd/misc/curl-7.28.1-libtool.patch) 1>/dev/null 2>/dev/null
#              else
#                AC_MSG_WARN([curl-7.28.1 not available - skipped libtool patch!])
#              fi
            ;;
          *)
            ;;
        esac
      fi
      if test $result1 -eq 1; then
        install -d c-ares
        (cd c-ares; CFLAGS= ./configure \
          --host=$host \
          --prefix=$cwd/packages \
          --disable-shared \
          --enable-static \
          `if test "$ENABLE_DEBUG" = "yes"; then echo --enable-debug; fi` \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd c-ares; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd c-ares; make install) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
        case $host_os in
          mingw*)
            (cd packages/lib; ln -s libcares.lib libcares.a) 1>/dev/null 2>/dev/null
            ;;
          *)
            ;;
        esac
      fi
      if test $result1 -eq 1; then
        AC_MSG_RESULT([done])
      fi

      AC_MSG_COMPILE([curl])
      result2=1
      if test $result2 -eq 1; then
        case $host_os in
          mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            # patch created with:
            #   diff -u curl-7.28.1.org/m4/libtool.m4 curl-7.28.1/m4/libtool.m4 >  curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/aclocal.m4 curl-7.28.1/aclocal.m4       >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/Makefile.in curl-7.28.1/Makefile.in     >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/configure curl-7.28.1/configure         >> curl-7.28.1-libtool.patch
#            if test -d packages/curl-7.28.1; then
#              (cd packages/curl-7.28.1; patch --batch -N -p1 <$cwd/misc/curl-7.28.1-libtool.patch) 1>/dev/null 2>/dev/null
#            else
#              AC_MSG_WARN([curl-7.28.1 not available - skipped libtool patch!])
#            fi
            ;;
          *)
            ;;
        esac
      fi
      if test $result2 -eq 1; then
        (cd curl; LIBS="-ldl" ./configure \
          --host=$host \
          --prefix=$cwd/packages \
          --disable-shared \
          --enable-static \
          --disable-ldap \
          --disable-rtsp \
          --without-librtmp \
          --enable-ares=$cwd/packages \
          --with-zlib=$cwd/packages \
          --with-ssl=$cwd/packages \
          --with-gnutls=$cwd/packages \
          --with-libssh2=$cwd/packages \
          `if test "$ENABLE_DEBUG" = "yes"; then echo --enable-debug; fi` \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd curl; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd curl; make install) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
        case $host_os in
          mingw*)
            (cd packages/lib; ln -s libcurl.lib libcurl.a) 1>$tmpFile 2>&1
            ;;
          *)
            ;;
        esac
      fi
      if test $result2 -eq 1; then
        AC_MSG_RESULT([done])
      fi

      AC_MSG_COMPILE([mxml])
      result3=1
      if test $result3 -eq 1; then
        case $host_os in
          mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            # patch created with:
            #   diff -u curl-7.28.1.org/m4/libtool.m4 curl-7.28.1/m4/libtool.m4 >  curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/aclocal.m4 curl-7.28.1/aclocal.m4       >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/Makefile.in curl-7.28.1/Makefile.in     >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/configure curl-7.28.1/configure         >> curl-7.28.1-libtool.patch
#              if test -d packages/curl-7.28.1; then
#                (cd packages/curl-7.28.1; patch --batch -N -p1 <$cwd/misc/curl-7.28.1-libtool.patch) 1>/dev/null 2>/dev/null
#              else
#                AC_MSG_WARN([curl-7.28.1 not available - skipped libtool patch!])
#              fi
            ;;
          *)
            ;;
        esac
      fi
      if test $result3 -eq 1; then
        (cd mxml; ./configure \
          --host=$host \
          --prefix=$cwd/packages \
          --disable-shared \
          --enable-static \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
      fi
      if test $result3 -eq 1; then
        (cd mxml; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
      fi
      if test $result3 -eq 1; then
        (cd mxml; make install) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
        case $host_os in
          mingw*)
            (cd packages/lib; ln -s libmxml.lib libmxml.a) 1>/dev/null 2>/dev/null
            ;;
          *)
            ;;
        esac
      fi
      if test $result3 -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result1=1
      result2=1
      result3=1
    fi

    if test $result1 -eq 1 -a $result2 -eq 1 -a $result3 -eq 1; then
      HAVE_FTP=1
      HAVE_CARES=1
      HAVE_CURL=1
      HAVE_MXML=1
      CC_DEFINES="$CC_DEFINES CURL_STATICLIB"
      STATIC_LIBRARIES="$STATIC_LIBRARIES curl cares mxml"
      AC_DEFINE(HAVE_CARES,1,[c-ares installed (local)])
      AC_DEFINE(HAVE_CURL,1,[curl installed (local)])
      AC_DEFINE(HAVE_MXML,1,[mxml installed (local)])
    fi
  else
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(mxmlNewXML,mxml,[HAVE_MXML=1;LIBRARIES="$LIBRARIES mxml";AC_DEFINE(HAVE_MXML,1,[mxml installed])])
      AC_SEARCH_LIBRARIES(ares_library_init,cares,[HAVE_CARES=1;LIBRARIES="$LIBRARIES cares";AC_DEFINE(HAVE_CARES,1,[c-ares installed])])
      AC_SEARCH_LIBRARIES(curl_global_init,curl,[HAVE_CURL=1;LIBRARIES="$LIBRARIES curl";AC_DEFINE(HAVE_CURL,1,[curl installed])])
    else
      if test -z "$HAVE_MXML"; then
        AC_SEARCH_STATIC_LIBRARIES(mxmlNewXML,mxml,[HAVE_MXML=1;STATIC_LIBRARIES="$STATIC_LIBRARIES mxml";AC_DEFINE(HAVE_MXML,1,[static mxml installed])])
      fi
      if test -z "$HAVE_MXML"; then
        AC_SEARCH_LIBRARIES(mxmlNewXML,mxml,[HAVE_MXML=1;LIBRARIES="$LIBRARIES mxml";AC_DEFINE(HAVE_MXML,1,[mxml installed])])
      fi
      if test -z "$HAVE_CARES"; then
        AC_SEARCH_STATIC_LIBRARIES(ares_library_init,cares,[HAVE_CARES=1;STATIC_LIBRARIES="$STATIC_LIBRARIES cares";AC_DEFINE(HAVE_CURL,1,[static c-ares installed])])
      fi
      if test -z "$HAVE_CARES"; then
        AC_SEARCH_LIBRARIES(ares_library_init,cares,[HAVE_CARES=1;LIBRARIES="$LIBRARIES cares";AC_DEFINE(HAVE_CURL,1,[c-ares installed])])
      fi
      if test -z "$HAVE_CURL"; then
        AC_SEARCH_STATIC_LIBRARIES(curl_global_init,curl,[HAVE_CURL=1;STATIC_LIBRARIES="$STATIC_LIBRARIES curl";AC_DEFINE(HAVE_CURL,1,[static curl installed])])
      fi
      if test -z "$HAVE_CURL"; then
        AC_SEARCH_LIBRARIES(curl_global_init,curl,[HAVE_CURL=1;LIBRARIES="$LIBRARIES curl";AC_DEFINE(HAVE_CURL,1,[curl installed])])
      fi
    fi
  fi

  if test -z "$HAVE_CURL"; then
    if test ! \( -d curl -o -d "`readlink curl`" \); then
      AC_MSG_WARN([curl not compiled: curl package missing])
    fi
  fi
  if test -z "$HAVE_CARES"; then
    if test ! \( -d c-ares -o -d "`readlink c-ares`" \); then
      AC_MSG_WARN([libssh2 not compiled: c-ares package missing])
    fi
  fi

  if test -z "$HAVE_MXML"; then
    if test ! \( -d mxml -o -d "`readlink mxml`" \); then
      AC_MSG_WARN([libssh2 not compiled: mxml package missing])
    fi
  fi

  if test "$HAVE_CURL"; then
    dnl link libssl, too
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(SSL_new,ssl,[HAVE_SSL=1;LIBRARIES="$LIBRARIES ssl"],AC_ERROR([SSL library required for curl]))
    else
      AC_SEARCH_STATIC_LIBRARIES(SSL_new,ssl,[HAVE_SSL=1;STATIC_LIBRARIES="$STATIC_LIBRARIES ssl"],AC_ERROR([static SSL library required for curl]))
    fi
  fi
fi

# compile ftplib (Note: not used if curl is available)
if test $ENABLE_FTP = "yes" -a -z "$HAVE_CURL"; then
  if test -d ftplib -o -d "`readlink ftplib`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile ftp library

      AC_MSG_COMPILE([ftplib])
      if test $result -eq 1; then
        (cd ftplib/linux; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd ftplib/linux; cp ftplib.h $cwd/packages/include && cp libftp.a $cwd/packages/lib) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      HAVE_FTP=1
      STATIC_LIBRARIES="$STATIC_LIBRARIES ftp"
      AC_DEFINE(HAVE_FTP,1,[ftplib installed (local)])
    fi
  else
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(FtpInit,ftp,[HAVE_FTP=1;LIBRARIES="$LIBRARIES ftp";AC_DEFINE(HAVE_FTP,1,[FTP installed])])
    else
      if test -z "$HAVE_FTP"; then
        AC_SEARCH_STATIC_LIBRARIES(FtpInit,ftp,[HAVE_FTP=1;STATIC_LIBRARIES="$STATIC_LIBRARIES ftp";AC_DEFINE(HAVE_FTP,1,[static FTP installed])])
      fi
      if test -z "$HAVE_FTP"; then
        AC_SEARCH_LIBRARIES(FtpInit,ftp,[HAVE_FTP=1;LIBRARIES="$LIBRARIES ftp";AC_DEFINE(HAVE_FTP,1,[FTP installed])])
      fi
    fi
  fi

  if test -z "$HAVE_FTP"; then
    if test ! \( -d ftplib -o -d "`readlink ftplib`" \); then
      AC_MSG_WARN([ftplib not compiled: ftplib package missing])
    fi
  fi
fi

# compile/use cdio
if test $ENABLE_ISO9660 = "yes"; then
  if test \( -d libiconv -o -d "`readlink libiconv`" \) -a \( -d libcdio -o -d "`readlink libcdio`" \); then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile libiconv library

      AC_MSG_COMPILE([libiconv])
      result1=1
      if test $result1 -eq 1; then
        (cd libiconv; ./configure \
          --host=$host \
          --prefix=$cwd/packages \
          --disable-shared \
          --enable-static \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd libiconv; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd libiconv; make install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        AC_MSG_RESULT([done])
      fi

      dnl compile libcdio library

      AC_MSG_COMPILE([libcdio])
      result2=1
      if test $result2 -eq 1; then
        if test "$ENABLE_DEBUG" = "yes"; then
          (cd libcdio; CFLAGS="-g -O0" CXXFLAGS="-g -O0" ./configure \
            --host=$host \
            --prefix=$cwd/packages \
            --with-libiconv-prefix=$cwd/packages \
            --disable-cxx \
            --enable-static=iso9660 \
            --disable-shared \
            --enable-static \
          ) 1>$tmpFile 2>&1
        else
          (cd libcdio; ./configure \
            --host=$host \
            --prefix=$cwd/packages \
            --disable-cxx \
            --enable-static=iso9660 \
            --disable-shared \
          ) 1>$tmpFile 2>&1
        fi
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd libcdio; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd libcdio; make install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result1=1
      result2=1
    fi

    if test $result1 -eq 1 -a $result2 -eq 1; then
      HAVE_ISO9660=1
      STATIC_LIBRARIES="$STATIC_LIBRARIES iso9660 cdio iconv"
      AC_DEFINE(HAVE_ISO9660,1,[iso9660 installed (local)])
    fi
  else
    dnl search for installed iso9660 library
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(iso9660_open,iso9660,[HAVE_ISO9660=1;LIBRARIES="$LIBRARIES iso9660 cdio";AC_DEFINE(HAVE_ISO9660,1,[iso9660 installed])])
    else
      if test -z "$HAVE_ISO9660"; then
        AC_SEARCH_STATIC_LIBRARIES(iso9660_open,iso9660,[HAVE_ISO9660=1;STATIC_LIBRARIES="$STATIC_LIBRARIES iso9660 cdio";AC_DEFINE(HAVE_ISO9660,1,[static iso9660 installed])])
      fi
      if test -z "$HAVE_ISO9660"; then
        AC_SEARCH_LIBRARIES(iso9660_open,iso9660,[HAVE_ISO9660=1;LIBRARIES="$LIBRARIES iso9660 cdio";AC_DEFINE(HAVE_ISO9660,1,[iso9660 installed])])
      fi
    fi
  fi

  if test -z "$HAVE_ISO9660"; then
    if test ! \( -d libcdio -o -d "`readlink libcdio`" \); then
      AC_MSG_WARN([libcdio not compiled: libcdio package missing])
    fi
  fi
fi

# compile/use pcre
if test $ENABLE_PCRE = "yes"; then
  if test -d pcre -o -d "`readlink pcre`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile pcre library

      AC_MSG_COMPILE([pcre])
      result=1
      if test $result -eq 1; then
        case $host_os in
          mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            # patch created with:
            #   diff -u pcre-8.32.org/m4/libtool.m4 pcre-8.32/m4/libtool.m4 >  pcre-8.32-libtool.patch
            #   diff -u pcre-8.32.org/aclocal.m4 pcre-8.32/aclocal.m4       >> pcre-8.32-libtool.patch
            #   diff -u pcre-8.32.org/Makefile.in pcre-8.32/Makefile.in     >> pcre-8.32-libtool.patch
            #   diff -u pcre-8.32.org/configure pcre-8.32/configure         >> pcre-8.32-libtool.patch
#            if test -d packages/pcre-8.32; then
#              (cd packages/pcre-8.32; patch --batch -N -p1 <$cwd/misc/pcre-8.32-libtool.patch) 1>/dev/null 2>/dev/null
#            else
#              AC_MSG_WARN([pcre-8.32 not available - skipped libtool patch!])
#            fi
            ;;
          *)
            ;;
        esac
      fi
      if test $result -eq 1; then
        (cd pcre; ./configure \
          --host=$host \
          --prefix=$cwd/packages \
          --disable-shared \
          --enable-static \
          --enable-utf8 \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd pcre; make clean; make -j$CPU_CORES `if test -n "$LIBTOOL"; then echo LIBTOOL=$LIBTOOL; fi`) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd pcre; make install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
        case $host_os in
          mingw*)
            (cd packages/lib; ln -s libpcre.lib libpcre.a) 1>/dev/null 2>/dev/null
            (cd packages/lib; ln -s libpcreposix.lib libpcreposix.a) 1>/dev/null 2>/dev/null
            ;;
          *)
            ;;
        esac
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      HAVE_PCRE=1
      CC_DEFINES="$CC_DEFINES PCRE_STATIC"
      STATIC_LIBRARIES="$STATIC_LIBRARIES pcre pcreposix"
      AC_DEFINE(HAVE_PCRE,1,[pcre installed (local)])
    fi
  else
    dnl search for installed pcre library
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(pcre,pcre_compile,[HAVE_PCRE=1;LIBRARIES="$LIBRARIES pcre";AC_DEFINE(HAVE_PCRE,1,[pcre installed])])
    else
      if test -z "$HAVE_PCRE"; then
        AC_SEARCH_STATIC_LIBRARIES(pcre,pcre_compile,[HAVE_PCRE=1;CC_DEFINES="$CC_DEFINES PCRE_STATIC";STATIC_LIBRARIES="$STATIC_LIBRARIES pcre";AC_DEFINE(HAVE_PCRE,1,[static pcre installed])])
      fi
      if test -z "$HAVE_PCRE"; then
        AC_SEARCH_LIBRARIES(pcre,pcre_compile,[HAVE_PCRE=1;LIBRARIES="$LIBRARIES pcre";AC_DEFINE(HAVE_PCRE,1,[pcre installed])])
      fi
    fi
  fi

  if test -z "$HAVE_PCRE"; then
    if test ! \( -d pcre -o -d "`readlink pcre`" \); then
      AC_MSG_WARN([pcre not compiled: pcre package missing])
    fi
  fi
fi

case "$host_os" in
  windows*|mingw*)
    if test $ENABLE_PTHREADS_W32 = "yes"; then
      if test -d pthreads-w32 -o -d "`readlink pthreads-w32`"; then
        if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
          dnl compile pthreads-w32 library

          AC_MSG_COMPILE([pthreads-w32])
          result=1
          if test $result -eq 1; then
            if test "$ENABLE_DEBUG" = "yes"; then
              (cd pthreads-w32; make clean; make CC="$CC" AS="$AS" AR="$AR" RANLIB="$RANLIB" RC="$RC" GC-static-debug) 1>$tmpFile 2>&1
            else
              (cd pthreads-w32; make clean; make CC="$CC" AS="$AS" AR="$AR" RANLIB="$RANLIB" RC="$RC" GC-static) 1>$tmpFile 2>&1
            fi
            if test $? -ne 0; then
              AC_MSG_RESULT([fail (make)])
              AC_MSG_OUTPUT_LOG($tmpFile)
              result=0
            fi
          fi
          if test $result -eq 1; then
            (cd pthreads-w32; cp pthread.h sched.h semaphore.h $cwd/packages/include) 1>/dev/null
            (cd pthreads-w32; cp libpthreadGC2.a $cwd/packages/lib) 1>/dev/null
          fi
          if test $result -eq 1; then
            AC_MSG_RESULT([done])
          fi
        else
          result=1
        fi

        if test $result -eq 1; then
          HAVE_PTHREADS_W32=1
          CC_DEFINES="$CC_DEFINES PTW32_STATIC_LIB"
          STATIC_LIBRARIES="$STATIC_LIBRARIES pthreadGC2"
          AC_DEFINE(HAVE_PTHREADS_W32,1,[pthreads-w32 installed (local)])
        fi
      fi
    fi
    ;;
  *)
    ;;
esac

# compile/use breakpad
if test $ENABLE_BREAKPAD = "yes"; then
  if test -d breakpad -o -d "`readlink breakpad`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile breakpad library

      AC_MSG_COMPILE([breakpad])
      result=1
      if test $result -eq 1; then
        if test "$ENABLE_DEBUG" = "yes"; then
          (cd breakpad; CFLAGS="-g -O0" CXXFLAGS="-g -O0" ./configure \
            --host=$host \
            --prefix=$cwd/packages \
          ) 1>$tmpFile 2>&1
        else
          (cd breakpad; ./configure \
            --host=$host \
            --prefix=$cwd/packages \
          ) 1>$tmpFile 2>&1
        fi
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd breakpad; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd breakpad; make install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi

        # fix broken install of breakpad: does not install all header files?
#set -x
        install -d $cwd/packages/include/breakpad
        install -d $cwd/packages/include/breakpad/google_breakpad/processor
        install breakpad/src/google_breakpad/processor/*.h $cwd/packages/include/breakpad/google_breakpad/processor
        install -d $cwd/packages/include/breakpad/client/linux/handler
        install breakpad/src/client/linux/handler/*.h $cwd/packages/include/breakpad/client/linux
        install -d $cwd/packages/include/breakpad/client/linux/minidump_writer
        install breakpad/src/client/linux/minidump_writer/*.h $cwd/packages/include/breakpad/client/linux/minidump_writer
        install -d $cwd/packages/include/breakpad/client/linux/dump_writer_common
        install breakpad/src/client/linux/dump_writer_common/*.h $cwd/packages/include/breakpad/client/linux/dump_writer_common
        install breakpad/src/third_party/libdisasm/libdisasm.a $cwd/packages/lib
# 1>$tmpFile 2>&1
#find $cwd/packages/include/breakpad -type f -name '*.h'
#set +x
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      HAVE_BREAKPAD=1
      CC_INCLUDES="$CC_INCLUDES $cwd/packages/include/breakpad"
      STATIC_LIBRARIES="$STATIC_LIBRARIES breakpad_client"
      AC_DEFINE(HAVE_BREAKPAD,1,[breakpad installed (local)])

      AC_PATH_PROGS(DUMP_SYMS,dump_syms,,,$cwd/packages/bin)
    fi
  else
    dnl search for installed breakpad library
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(breakpad_client,ExceptionHandler,[HAVE_BREAKPAD=1;LIBRARIES="$LIBRARIES breakpad_client";AC_DEFINE(HAVE_BREAKPAD,1,[breakpad installed])])
    else
      if test -z "$HAVE_BREAKPAD"; then
        AC_SEARCH_STATIC_LIBRARIES(breakpad_client,ExceptionHandler,[HAVE_BREAKPAD=1;STATIC_LIBRARIES="$STATIC_LIBRARIES breakpad_client";AC_DEFINE(HAVE_BREAKPAD,1,[static breakpad installed])])
      fi
      if test -z "$HAVE_BREAKPAD"; then
        AC_SEARCH_LIBRARIES(breakpad_client,ExceptionHandler,[HAVE_BREAKPAD=1;LIBRARIES="$LIBRARIES breakpad_client";AC_DEFINE(HAVE_BREAKPAD,1,[breakpad installed])])
      fi
    fi

    AC_PATH_PROGS(DUMP_SYMS,dump_syms)
  fi

  if test -z "$HAVE_BREAKPAD"; then
    if test ! \( -d breakpad -o -d "`readlink breakpad`" \); then
      AC_MSG_WARN([breakpad not compiled: breakpad package missing])
    fi
  fi
fi

# compile/use binutils
if test $ENABLE_BFD = "yes"; then
  if test -d binutils -o -d "`readlink binutils`"; then
    if test "$ENABLE_PACKAGE_COMPILE" = "yes"; then
      dnl compile binutils BFD library

      AC_MSG_COMPILE([bfd])
      result=1
      if test $result -eq 1; then
        if test "$ENABLE_DEBUG" = "yes"; then
          (cd binutils; CFLAGS="-g -O0" CXXFLAGS="-g -O0" ./configure \
            --host=$host \
            --prefix=$cwd/packages \
          ) 1>$tmpFile 2>&1
        else
          (cd binutils; ./configure \
            --host=$host \
            --prefix=$cwd/packages \
            --disable-host-shared \
            --disable-ld \
          ) 1>$tmpFile 2>&1
        fi
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd binutils; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd binutils; make install PREFIX=$cwd/packages) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi

        # fix incomplete install: does not install all header files?
        install -d $cwd/packages/include
        install binutils/include/libiberty.h binutils/include/demangle.h $cwd/packages/include
        install -d $cwd/packages/lib
        install binutils/libiberty/libiberty.a $cwd/packages/lib
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      HAVE_BFD=1
      # PACKAGE required to make bfd.h happy
      CC_DEFINES="$CC_DEFINES PACKAGE"
      CC_INCLUDES="$CC_INCLUDES $cwd/packages/include"
      STATIC_LIBRARIES="$STATIC_LIBRARIES bfd iberty z"
      AC_DEFINE(HAVE_BFD,1,[libbfd installed (local)])

      AC_PATH_PROGS(DUMP_SYMS,dump_syms,,,$cwd/packages/bin)
    fi
  else
    dnl search for installed BFD library
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(bfd_init,bfd,[HAVE_BFD=1;LIBRARIES="$LIBRARIES bfd iberty z";AC_DEFINE(HAVE_BFD,1,[libbfd installed])])
    else
      if test -z "$HAVE_BFD"; then
        AC_SEARCH_STATIC_LIBRARIES(bfd_init,bfd,[HAVE_BFD=1;STATIC_LIBRARIES="$STATIC_LIBRARIES bfd iberty z";AC_DEFINE(HAVE_BFD,1,[static libbfd installed])])
      fi
      if test -z "$HAVE_BFD"; then
        AC_SEARCH_LIBRARIES(bfd_init,bfd,[HAVE_BFD=1;LIBRARIES="$LIBRARIES bfd iberty z";AC_DEFINE(HAVE_BFD,1,[libbfd installed])])
      fi
    fi
  fi

  if test -z "$HAVE_BFD"; then
    if test ! \( -d binutils -o -d "`readlink binutils`" \); then
      AC_MSG_WARN([libbfd not compiled: binutils package missing])
    fi
  fi
fi

# check mandatory packages for full version
AC_ARG_ENABLE(
  package-check,
  AC_HELP_STRING([--enable-package-check],[enable check packages]),
  [
   AS_IF(test $HAVE_ICU    ,,AC_MSG_ERROR([icu missing]))
   AS_IF(test $HAVE_Z      ,,AC_MSG_ERROR([zlib missing]))
   AS_IF(test $HAVE_BZ2    ,,AC_MSG_ERROR([bzip2 missing]))
   AS_IF(test $HAVE_LZMA   ,,AC_MSG_ERROR([lzma missing]))
   AS_IF(test $HAVE_LZO    ,,AC_MSG_ERROR([lzo missing]))
   AS_IF(test $HAVE_LZ4    ,,AC_MSG_ERROR([lz4 missing]))
   AS_IF(test $HAVE_ZSTD   ,,AC_MSG_ERROR([zstd missing]))
   AS_IF(test $HAVE_XDELTA ,,AC_MSG_ERROR([xdelta missing]))
   AS_IF(test $HAVE_GCRYPT ,,AC_MSG_ERROR([gcrypt missing]))
   AS_IF(test $HAVE_FTP    ,,AC_MSG_ERROR([ftp missing]))
   AS_IF(test $HAVE_CARES  ,,AC_MSG_ERROR([c-ares missing]))
   AS_IF(test $HAVE_CURL   ,,AC_MSG_ERROR([curl missing]))
   AS_IF(test $HAVE_MXML   ,,AC_MSG_ERROR([mxml missing]))
   AS_IF(test $HAVE_SSL    ,,AC_MSG_ERROR([ssl missing]))
   AS_IF(test $HAVE_SSH2   ,,AC_MSG_ERROR([ssh2 missing]))
   AS_IF(test $HAVE_GNU_TLS,,AC_MSG_ERROR([GNU TLS missing]))
   AS_IF(test $HAVE_ISO9660,,AC_MSG_ERROR([ISO9660 missing]))
   AS_IF(test $HAVE_PCRE   ,,AC_MSG_ERROR([PCRE missing]))
  ]
)

dnl -------------- headers/functions/definitions/libraries -------------

# set variables for tests
AC_SET_TOOLCHAIN_SETTINGS($CC_FLAGS,
                          $CXX_FLAGS,
                          $CC_DEFINES,
                          $CC_INCLUDES,
                          $LD_FLAGS,
                          $LIBRARY_PATHS,
                          $STATIC_LIBRARIES,
                          $LIBRARIES)

# headers
AC_CHECK_HEADERS(\
                 arpa/inet.h \
                 bfd.h \
                 demangle.h \
                 grp.h \
                 icu.h \
                 libintl.h \
                 link.h \
                 linux/fs.h \
                 linux/tcp.h \
                 netdb.h \
                 netinet/in.h \
                 pwd.h \
                 poll.h \
                 regex.h \
                 stdbool.h \
                 sys/inotify.h \
                 sys/ioctl.h \
                 sys/mount.h \
                 sys/resource.h \
                 sys/select.h \
                 sys/socket.h \
                 sys/statfs.h \
                 sys/statvfs.h \
                 sys/vfs.h \
                 sys/wait.h \
                 sys/xattr.h \
                 termios.h \
                 unicode/utypes.h \
                 unicode/uregex.h \
                 unicode/ustring.h \
                 unicode/ucol.h \
                 uuid/uuid.h
                )

# defines, constants, types, variables
AC_CHECK_TYPE(uint,AC_DEFINE(HAVE_UINT,1,[defined if uint is available]))
AC_CHECK_TYPE(ulong,AC_DEFINE(HAVE_ULONG,1,[defined if ulong is available]))
AC_CHECK_TYPE(long long int,AC_DEFINE(HAVE_LONG_LONG,1,[defined if ulong is available]))
dnl AC_C_LONG_LONG(AC_DEFINE(HAVE_LONG_LONG,1,[defined if long long data type available]),AC_MSG_ERROR(long long data type not available))
AC_TRY_COMPILE([#include <limits.h>],[long long int x=LLONG_MIN;],AC_DEFINE(HAVE_LLONG_MIN,1,[defined if LLONG_MIN is available]))
AC_TRY_COMPILE([#include <limits.h>],[long long int x=LLONG_MAX;],AC_DEFINE(HAVE_LLONG_MAX,1,[defined if LLONG_MAX is available]))
AC_TRY_COMPILE([#include <limits.h>],[unsigned long long int x=ULLONG_MAX;],AC_DEFINE(HAVE_ULLONG_MAX,1,[defined if ULLONG_MAX is available]))
AC_CHECK_VARIABLE_ATTRIBUTE(__thread,AC_DEFINE(HAVE___THREAD,1,[defined if __thread is available]))
dnl Note: AC_SYS_LARGEFILE() does not work with gcc? Use my own implementation
AC_CHECK_LARGEFILE_DEFINES
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_SIZEOF(unsigned long long)
AC_CHECK_SIZEOF(void *)
AC_CHECK_SIZEOF(size_t)

AC_SEARCH_STATIC_LIBRARIES(uuid_generate,uuid,[STATIC_LIBRARIES="$STATIC_LIBRARIES uuid"],,uuid/uuid.h)
case $host_os in
  linux*)
    AC_SEARCH_STATIC_LIBRARIES(idn_free,idn,[STATIC_LIBRARIES="$STATIC_LIBRARIES idn"],,idn-free.h)
    ;;
  mingw*)
    ;;
  *)
    AC_ERROR([Unsupported operating system '$host_os'])
    ;;
esac

dnl for debug/test only
dnl AC_CHECK_FUNCTION(gethostbyname,AC_DEFINE(HAVE_GETHOSTBYNAME,  1,[gethostbyname() available]),,winsock2.h)
dnl AC_CHECK_FUNCTION(gethostbyname,AC_DEFINE(HAVE_GETHOSTBYNAME,  1,[gethostbyname() available]),,winsock2.h)
dnl AC_CHECK_FUNCTION(gethostbynamex,AC_DEFINE(HAVE_GETHOSTBYNAME,  1,[gethostbyname() available]),,winsock2.h)
dnl AC_CHECK_FUNCTION(gethostbyname,  AC_DEFINE(HAVE_GETHOSTBYNAME,  1,[gethostbyname() available])  ,,winsock2.h)
dnl AC_CHECK_FUNCTION(gethostbyname_r,AC_DEFINE(HAVE_GETHOSTBYNAME_R,1,[gethostbyname_r() available]),,winsock2.h)
dnl AC_SEARCH_STATIC_LIBRARIES(zlibVersion,z,[HAVE_Z=1;STATIC_LIBRARIES="$STATIC_LIBRARIES z";AC_DEFINE(HAVE_Z,1,[z installed])])
dnl AC_MSG_ERROR([stop])

# set variables for tests
AC_SET_TOOLCHAIN_SETTINGS($CC_FLAGS,
                          $CXX_FLAGS,
                          $CC_DEFINES,
                          $CC_INCLUDES,
                          $LD_FLAGS,
                          $LIBRARY_PATHS,
                          $STATIC_LIBRARIES,
                          $LIBRARIES)

# check required defines
AC_CHECK_PTHREAD_DEFINES

# check constants
AC_CHECK_CONSTANT(S_IRUSR,        AC_DEFINE(HAVE_S_IRUSR,        1,[S_IRUSR available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(S_IWUSR,        AC_DEFINE(HAVE_S_IWUSR,        1,[S_IWUSR available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(S_IXUSR,        AC_DEFINE(HAVE_S_IXUSR,        1,[S_IXUSR available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(S_IRGRP,        AC_DEFINE(HAVE_S_IRGRP,        1,[S_IRGRP available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(S_IWGRP,        AC_DEFINE(HAVE_S_IWGRP,        1,[S_IWGRP available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(S_IXGRP,        AC_DEFINE(HAVE_S_IXGRP,        1,[S_IXGRP available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(S_IROTH,        AC_DEFINE(HAVE_S_IROTH,        1,[S_IROTH available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(S_IWOTH,        AC_DEFINE(HAVE_S_IWOTH,        1,[S_IWOTH available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(S_IXOTH,        AC_DEFINE(HAVE_S_IXOTH,        1,[S_IXOTH available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(S_ISUID,        AC_DEFINE(HAVE_S_ISUID,        1,[S_ISUID available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(S_ISGID,        AC_DEFINE(HAVE_S_ISGID,        1,[S_ISGID available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(S_ISVTX,        AC_DEFINE(HAVE_S_ISVTX,        1,[S_ISVTX available]),        ,sys/stat.h)
AC_CHECK_CONSTANT(FS_IOC_GETFLAGS,AC_DEFINE(HAVE_FS_IOC_GETFLAGS,1,[FS_IOC_GETFLAGS available]),,linux/fs.h)
AC_CHECK_CONSTANT(FS_COMPR_FL,    AC_DEFINE(HAVE_FS_COMPR_FL,    1,[FS_COMPR_FL available]),    ,linux/fs.h)
AC_CHECK_CONSTANT(FS_NOCOMP_FL,   AC_DEFINE(HAVE_FS_NOCOMP_FL,   1,[FS_NOCOMP_FL available]),   ,linux/fs.h)
AC_CHECK_CONSTANT(FS_IMMUTABLE_FL,AC_DEFINE(HAVE_FS_IMMUTABLE_FL,1,[FS_IMMUTABLE_FL available]),,linux/fs.h)
AC_CHECK_CONSTANT(FS_APPEND_FL,   AC_DEFINE(HAVE_FS_APPEND_FL,   1,[FS_APPEND_FL available]),   ,linux/fs.h)
AC_CHECK_CONSTANT(FS_NODUMP_FL,   AC_DEFINE(HAVE_FS_NODUMP_FL,   1,[FS_NODUMP_FL available]),   ,linux/fs.h)
AC_CHECK_CONSTANT(BLKSSZGET,      AC_DEFINE(HAVE_BLKSSZGET,      1,[BLKSSZGET available]),      ,sys/mount.h)
AC_CHECK_CONSTANT(BLKGETSIZE,     AC_DEFINE(HAVE_BLKGETSIZE,     1,[BLKGETSIZE available]),     ,sys/mount.h)
AC_CHECK_CONSTANT(O_DIRECTORY,    AC_DEFINE(HAVE_O_DIRECTORY,    1,[O_DIRECTORY available]),    ,sys/types.h sys/stat.h fcntl.h)
AC_CHECK_CONSTANT(O_NOATIME,      AC_DEFINE(HAVE_O_NOATIME,      1,[O_NOATIME available]),      ,sys/types.h sys/stat.h fcntl.h)

AC_CHECK_CONSTANT(SIGPIPE,AC_DEFINE(HAVE_SIGPIPE,1,[SIGPIPE available]),,signal.h)

AC_CHECK_CONSTANT(_SC_GETPW_R_SIZE_MAX,AC_DEFINE(HAVE__SC_GETPW_R_SIZE_MAX,1,[_SC_GETPW_R_SIZE_MAX available]),,unistd.h limits.h)
AC_CHECK_CONSTANT(_SC_NPROCESSORS_CONF,AC_DEFINE(HAVE__SC_NPROCESSORS_CONF,1,[_SC_NPROCESSORS_CONF available]),,unistd.h limits.h)

if test $HAVE_SSH2; then
  AC_CHECK_CONSTANT(CRYPTO_LOCK,,AC_ERROR([constant CRYPTO_LOCK is not available]),openssl/crypto.h)
fi

AC_CHECK_CONSTANT(IN_EXCL_UNLINK,AC_DEFINE(HAVE_IN_EXCL_UNLINK,1,[IN_EXCL_UNLINK available]),,sys/inotify.h)

# check types
AC_CHECK_TYPE(struct stat64,  AC_DEFINE(HAVE_STAT64,  1,[defined if struct stat64 is available]),  ,[#include <sys/stat.h>])
AC_CHECK_TYPE(struct __stat64,AC_DEFINE(HAVE___STAT64,1,[defined if struct __stat64 is available]),,[#include <sys/stat.h>])
AC_CHECK_TYPE(struct _stati64,AC_DEFINE(HAVE__STATI64,1,[defined if struct _stati64 is available]),,[#include <sys/stat.h>])

AC_CHECK_STRUCT_MEMBER(struct stat,st_atim.tv_nsec,AC_DEFINE(HAVE_STAT_ATIM_TV_NSEC,1,[defined if struct stat.st_atim.tv_nsec is available]),,[sys/types.h sys/stat.h fcntl.h])
AC_CHECK_STRUCT_MEMBER(struct stat,st_mtim.tv_nsec,AC_DEFINE(HAVE_STAT_MTIM_TV_NSEC,1,[defined if struct stat.st_mtim.tv_nsec is available]),,[sys/types.h sys/stat.h fcntl.h])
AC_CHECK_STRUCT_MEMBER(struct stat,st_ctim.tv_nsec,AC_DEFINE(HAVE_STAT_CTIM_TV_NSEC,1,[defined if struct stat.st_ctim.tv_nsec is available]),,[sys/types.h sys/stat.h fcntl.h])

# check functions
AC_CHECK_FUNCTION(pthread_attr_setname,AC_DEFINE(HAVE_PTHREAD_ATTR_SETNAME,1,[pthread_attr_setname() available]))
AC_CHECK_FUNCTION(pthread_setname_np,  AC_DEFINE(HAVE_PTHREAD_SETNAME_NP,  1,[pthread_setname_np() available]  ))

AC_CHECK_FUNCTION(gethostbyaddr,  AC_DEFINE(HAVE_GETHOSTBYADDR,  1,[gethostbyaddr() available])  ,,winsock2.h)
AC_CHECK_FUNCTION(gethostbyaddr_r,AC_DEFINE(HAVE_GETHOSTBYADDR_R,1,[gethostbyaddr_r() available]),,winsock2.h)
AC_CHECK_FUNCTION(gethostbyname,  AC_DEFINE(HAVE_GETHOSTBYNAME,  1,[gethostbyname() available])  ,,winsock2.h)
AC_CHECK_FUNCTION(gethostbyname_r,AC_DEFINE(HAVE_GETHOSTBYNAME_R,1,[gethostbyname_r() available]),,winsock2.h)

AC_CHECK_FUNCTION(getlogin,  AC_DEFINE(HAVE_GETLOGIN,  1,[getlogin() available]))
AC_CHECK_FUNCTION(getlogin_r,AC_DEFINE(HAVE_GETLOGIN_R,1,[getlogin_r() available]))

AC_CHECK_FUNCTION(chmod,        AC_DEFINE(HAVE_CHMOD,        1,[chmod() available]))
AC_CHECK_FUNCTION(chown,        AC_DEFINE(HAVE_CHOWN,        1,[chown() available]))
AC_CHECK_FUNCTION(lseek64,      AC_DEFINE(HAVE_LSEEK64,      1,[lseek64() available]))
AC_CHECK_FUNCTION(lseek,        AC_DEFINE(HAVE_LSEEK,        1,[lseek() available]))
AC_CHECK_FUNCTION(ftruncate64,  AC_DEFINE(HAVE_FTRUNCATE64,  1,[ftruncate64() available]))
AC_CHECK_FUNCTION(ftruncate,    AC_DEFINE(HAVE_FTRUNCATE,    1,[ftruncate() available]))
AC_CHECK_FUNCTION(fdatasync,    AC_DEFINE(HAVE_FDATASYNC,    1,[fdatasync() available]))
AC_CHECK_FUNCTION(fopen,        AC_DEFINE(HAVE_FOPEN,        1,[fopen() available]))
AC_CHECK_FUNCTION(fopen64,      AC_DEFINE(HAVE_FOPNE64,      1,[fopen64() available]))
AC_CHECK_FUNCTION(fseeko,       AC_DEFINE(HAVE_FSEEKO,       1,[fseeko() available]))
AC_CHECK_FUNCTION(_fseeki64,    AC_DEFINE(HAVE__FSEEKI64,    1,[_fseeki64() available]))
AC_CHECK_FUNCTION(fseeko64,     AC_DEFINE(HAVE_FSEEKO64,     1,[fseeko64() available]))
AC_CHECK_FUNCTION(ftello,       AC_DEFINE(HAVE_FTELLO,       1,[ftello() available]))
AC_CHECK_FUNCTION(_ftelli64,    AC_DEFINE(HAVE__FTELLI64,    1,[_ftelli64() available]))
AC_CHECK_FUNCTION(ftello64,     AC_DEFINE(HAVE_FTELLO64,     1,[ftello64() available]))
AC_CHECK_FUNCTION(getgrgid_r,   AC_DEFINE(HAVE_GETGRGID_R,   1,[getgrgid_r() available]))
AC_CHECK_FUNCTION(getgrnam_r,   AC_DEFINE(HAVE_GETGRNAM_R,   1,[getgrnam_r() available]))
AC_CHECK_FUNCTION(getpwnam_r,   AC_DEFINE(HAVE_GETPWNAM_R,   1,[getpwnam_r() available]))
AC_CHECK_FUNCTION(getpwuid_r,   AC_DEFINE(HAVE_GETPWUID_R,   1,[getpwuid_r() available]))
AC_CHECK_FUNCTION(ioctl,        AC_DEFINE(HAVE_IOCTL,        1,[ioctl() available]))
AC_CHECK_FUNCTION(lchmod,       AC_DEFINE(HAVE_LCHMOD,       1,[lchmod() available]))
AC_CHECK_FUNCTION(link,         AC_DEFINE(HAVE_LINK,         1,[link() available]))
AC_CHECK_FUNCTION(lstat64,      AC_DEFINE(HAVE_LSTAT64,      1,[lstat64() available]))
AC_CHECK_FUNCTION(lstat,        AC_DEFINE(HAVE_LSTAT,        1,[lstat() available]))
AC_CHECK_FUNCTION(major,        AC_DEFINE(HAVE_MAJOR,        1,[major() available]),,sys/types.h)
AC_CHECK_FUNCTION(makedev,      AC_DEFINE(HAVE_MAKEDEV,      1,[makedev() available]),,sys/types.h)
AC_CHECK_FUNCTION(minor,        AC_DEFINE(HAVE_MINOR,        1,[minor() available]),,sys/types.h)
AC_CHECK_FUNCTION(mknod,        AC_DEFINE(HAVE_MKNOD,        1,[mknod() available]))
AC_CHECK_FUNCTION(posix_fadvise,AC_DEFINE(HAVE_POSIX_FADVISE,1,[posix_fadvise() available]))
AC_CHECK_FUNCTION(readlink,     AC_DEFINE(HAVE_READLINK,     1,[readlink() available]))
AC_CHECK_FUNCTION(stat64,       AC_DEFINE(HAVE_STAT64,       1,[stat64() available]))
AC_CHECK_FUNCTION(stat,         AC_DEFINE(HAVE_STAT,         1,[stat() available]))
AC_CHECK_FUNCTION(_stati64,     AC_DEFINE(HAVE__STATI64,     1,[_stati64() available]))
AC_CHECK_FUNCTION(statfs,       AC_DEFINE(HAVE_STATFS,       1,[statfs() available]))
AC_CHECK_FUNCTION(statvfs,      AC_DEFINE(HAVE_STATVFS,      1,[statvfs() available]))
AC_CHECK_FUNCTION(symlink,      AC_DEFINE(HAVE_SYMLINK,      1,[symlink() available]))
AC_CHECK_FUNCTION(futimens,     AC_DEFINE(HAVE_FUTIMENSK,    1,[futimens() available]))
AC_CHECK_FUNCTION(fdopendir,    AC_DEFINE(HAVE_FDOPENDIR,    1,[fdopendir() available]),,sys/types.h dirent.h)
AC_CHECK_FUNCTION(mkdtemp,      AC_DEFINE(HAVE_MKDTEMP,      1,[mkdtemp() available]),,io.h)
AC_CHECK_FUNCTION(mkstemp,      AC_DEFINE(HAVE_MKSTEMP,      1,[mkstemp() available]),,io.h)
AC_CHECK_FUNCTION(mktemp,       AC_DEFINE(HAVE_MKTEMP,       1,[mktemp() available]) ,,io.h)
AC_CHECK_FUNCTION(llistxattr,   AC_DEFINE(HAVE_LLISTXATTR,   1,[llistxattr() available]) ,,sys/xattr.h)
AC_CHECK_FUNCTION(lgetxattr,    AC_DEFINE(HAVE_LGETXATTR,    1,[lgetxattr() available]) ,,sys/xattr.h)
AC_CHECK_FUNCTION(lsetxattr,    AC_DEFINE(HAVE_LSETXATTR,    1,[lsetxattr() available]) ,,sys/xattr.h)
AC_CHECK_MKDIR_ARGUMENTS

AC_CHECK_FUNCTION(u_init,       AC_DEFINE(HAVE_U_INIT,1,[u_init() available])  ,,icu.h)

AC_CHECK_FUNCTION(inotify_init1,    AC_DEFINE(HAVE_INOTIFY_INIT1,    1,[inotify_init1() available]))
AC_CHECK_FUNCTION(inotify_add_watch,AC_DEFINE(HAVE_INOTIFY_ADD_WATCH,1,[inotify_add_watch() available]))
AC_CHECK_FUNCTION(inotify_rm_watch, AC_DEFINE(HAVE_INOTIFY_RM_WATCH, 1,[inotify_rm_watch() available]))

AC_CHECK_FUNCTION(localtime_r,AC_DEFINE(HAVE_LOCALTIME_R,1,[localtime_r() available]))
AC_CHECK_FUNCTION(getdate_r,  AC_DEFINE(HAVE_GETDATE_R,  1,[getdate_r() available]))
AC_CHECK_FUNCTION(usleep,     [HAVE_USLEEP=1; AC_DEFINE(HAVE_USLEEP,1,[usleep() available])])
AC_CHECK_FUNCTION(nanosleep,  AC_DEFINE(HAVE_NANOSLEEP,  1,[nanosleep() available]))

AC_CHECK_FUNCTION(pipe,   AC_DEFINE(HAVE_PIPE,   1,[pipe() available]))
AC_CHECK_FUNCTION(fork,   AC_DEFINE(HAVE_FORK,   1,[fork() available]))
AC_CHECK_FUNCTION(waitpid,AC_DEFINE(HAVE_WAITPID,1,[waitpid() available]))

AC_CHECK_FUNCTION(zlibVersion,,AC_ERROR([mandatory function zlibVersion() is not available]),zlib.h)

AC_CHECK_FUNCTION(libssh2_userauth_publickey_frommemory,AC_DEFINE(HAVE_SSH2_USERAUTH_PUBLICKEY_FROMMEMORY,1,[SSH2 has libssh2_userauth_publickey_frommemory() function]))
AC_CHECK_FUNCTION(libssh2_channel_send_keepalive,       AC_DEFINE(HAVE_SSH2_CHANNEL_SEND_KEEPALIVE,       1,[SSH2 has libssh2_channel_send_keepalive()]))
AC_CHECK_FUNCTION(libssh2_keepalive_config,             AC_DEFINE(HAVE_SSH2_KEEPALIVE_CONFIG,             1,[SSH2 has libssh2_keepalive_config()]))
AC_CHECK_FUNCTION(libssh2_keepalive_send,               AC_DEFINE(HAVE_SSH2_KEEPALIVE_SEND,               1,[SSH2 has libssh2_keepalive_send()]))
AC_CHECK_FUNCTION(libssh2_scp_send64,                   AC_DEFINE(HAVE_SSH2_SCP_SEND64,                   1,[SSH2 has libssh2_scp_send64() function]))
AC_CHECK_FUNCTION(libssh2_sftp_seek64,                  AC_DEFINE(HAVE_SSH2_SFTP_SEEK64,                  1,[SSH2 has libssh2_sftp_seek64() function]))
AC_CHECK_FUNCTION(libssh2_sftp_seek2,                   AC_DEFINE(HAVE_SSH2_SFTP_SEEK2,                   1,[SSH2 has libssh2_sftp_seek2() function]))

AC_CHECK_FUNCTION(iso9660_open,              AC_DEFINE(HAVE_ISO9660_OPEN,              1,[iso9660_open() available]))
AC_CHECK_FUNCTION(iso9660_close,             AC_DEFINE(HAVE_ISO9660_CLOSE,             1,[iso9660_close() available]))
AC_CHECK_FUNCTION(iso9660_ifs_stat_translate,AC_DEFINE(HAVE_ISO9660_IFS_STAT_TRANSLATE,1,[iso9660_ifs_stat_translate() available]))
AC_CHECK_FUNCTION(iso9660_iso_seek_read,     AC_DEFINE(HAVE_ISO9660_ISO_SEEK_READ,     1,[iso9660_iso_seek_read() available]))

AC_CHECK_FUNCTION(uuid_generate,     AC_DEFINE(HAVE_UUID_GENERATE,     1,[uuid_generate() available]))
AC_CHECK_FUNCTION(uuid_unparse_lower,AC_DEFINE(HAVE_UUID_UNPARSE_LOWER,1,[uuid_unparse_lower() available]))

AC_CHECK_FUNCTION(sysconf,          AC_DEFINE(HAVE_SYSCONF,          1,[sysconf() available]))
AC_CHECK_FUNCTION(backtrace,        AC_DEFINE(HAVE_BACKTRACE,        1,[backtrace() available]))
AC_CHECK_FUNCTION(backtrace_symbols,AC_DEFINE(HAVE_BACKTRACE_SYMBOLS,1,[backtrace_symbols() available]))

AC_CHECK_FUNCTION(setlocale,     AC_DEFINE(HAVE_SETLOCALE,     1,[setlocale() available]))
AC_CHECK_FUNCTION(uselocale,     AC_DEFINE(HAVE_USELOCALE,     1,[uselocale() available]))
AC_CHECK_FUNCTION(newlocale,     AC_DEFINE(HAVE_NEWLOCALE,     1,[newlocale() available]))
AC_CHECK_FUNCTION(freelocale,    AC_DEFINE(HAVE_FREELOCALE,    1,[freelocale() available]))
AC_CHECK_FUNCTION(textdomain,    AC_DEFINE(HAVE_TEXTDOMAIN,    1,[textdomain() available]))
AC_CHECK_FUNCTION(bindtextdomain,AC_DEFINE(HAVE_BINDTEXTDOMAIN,1,[bindtextdomain() available]))
AC_CHECK_FUNCTION(gettext,       AC_DEFINE(HAVE_GETTEXT,       1,[gettext() available]))

AC_CHECK_FUNCTION(bfd_init,AC_DEFINE(HAVE_BFD_INIT,      1,[bfd_init() available]))

if test "$HAVE_SSH2"; then
  AC_CHECK_FUNCTION(CRYPTO_set_id_callback,     ,AC_ERROR([CRYPTO_set_id_callback() required for SSH2]),openssl/crypto.h)
  AC_CHECK_FUNCTION(CRYPTO_set_locking_callback,,AC_ERROR([CRYPTO_set_locking_callback() required for SSH2]),openssl/crypto.h)
  dnl AC_CHECK_FUNCTION(CRYPTO_THREADID_set_callback,AC_DEFINE(HAVE_CRYPTO_THREADID_SET_CALLBACK,1,[CRYPTO_THREADID_set_callback() required for SSH2]),,openssl/crypto.h)
fi

if test "$HAVE_CURL"; then
  AC_CHECK_FUNCTION(gpg_strerror,,AC_ERROR([gpg_strerror() required for curl]))
  AC_CHECK_FUNCTION(gcry_control,,AC_ERROR([gcry_control() required for curl]))
  AC_CHECK_FUNCTION(gnutls_global_init,,AC_ERROR([gnutls_global_init() required for curl]))
  AC_CHECK_FUNCTION(SSL_new,,AC_ERROR([SSL_new() required for curl]))
fi

dnl -------------------------------- misc ------------------------------

VERSION_MAJOR=`cat $srcdir/version|grep MAJOR|sed 's/.*=//g'`
VERSION_MINOR=`cat $srcdir/version|grep MINOR|sed 's/.*=//g'`
VERSION_REVISION=`$SVNVERSION . 2>/dev/null || cat $srcdir/version|grep RELEASE|sed 's/RELEASE=//g'`

COPYRIGHT_DATE="2007-`date +%Y`"

AC_DEFINE_UNQUOTED(FILE_SEPARATOR_CHAR,  '$FILE_SEPARATOR_CHAR',[file separator char])
AC_DEFINE_UNQUOTED(FILE_SEPARATOR_STRING,"$FILE_SEPARATOR_CHAR",[file separator string])

AC_DEFINE_UNQUOTED(CONFIG_DIR, "$CONFIG_DIR", [configuration directory])
AC_DEFINE_UNQUOTED(RUNTIME_DIR,"$RUNTIME_DIR",[runtime directory])
AC_DEFINE_UNQUOTED(TLS_DIR,    "$TLS_DIR",    [TLS directory])

# get version numbers
AC_TRY_LINK([#include <stdio.h>
             #include "sqlite3.h"
            ],
            [printf("%s\n",SQLITE_VERSION);],
            VERSION_SQLITE=`./conftest$ac_exeext`,
            VERSION_SQLITE=
           )
AC_TRY_LINK([#include <stdio.h>
             #include "zlib.h"
            ],
            [printf("%s\n",zlibVersion());],
            VERSION_Z=`./conftest$ac_exeext`,
            VERSION_Z=
           )
AC_TRY_LINK([#include <stdio.h>
             #include "bzlib.h"
            ],
            [printf("%s\n",BZ2_bzlibVersion());],
            VERSION_BZ2=`./conftest$ac_exeext`,
            VERSION_BZ2=
           )
AC_TRY_LINK([#include <stdio.h>
             #include "lzma.h"
            ],
            [printf("%d.%d.%d\n",LZMA_VERSION_MAJOR,LZMA_VERSION_MINOR,LZMA_VERSION_PATCH);],
            VERSION_LZMA=`./conftest$ac_exeext`,
            VERSION_LZMA=
           )
AC_TRY_LINK([#include <stdio.h>
             #include "lzo/lzoconf.h"
            ],
            [printf("%s\n",LZO_VERSION_STRING);],
            VERSION_LZO=`./conftest$ac_exeext`,
            VERSION_LZO=
           )
AC_TRY_LINK([#include <stdio.h>
             #include "lz4.h"],
             [printf("%d.%d.%d\n",LZ4_VERSION_MAJOR,LZ4_VERSION_MINOR,LZ4_VERSION_RELEASE);],
             VERSION_LZ4=`./conftest$ac_exeext`,
             VERSION_LZ4=
            )
AC_TRY_LINK([#include <stdio.h>
             #include "zstd.h"],
             [printf("%d.%d.%d\n",ZSTD_VERSION_MAJOR,ZSTD_VERSION_MINOR,ZSTD_VERSION_RELEASE);],
             VERSION_ZSTD=`./conftest$ac_exeext`,
             VERSION_ZSTD=
            )
AC_TRY_LINK([#include <stdio.h>
             #include "gcrypt.h"
            ],
            [printf("%s\n",GCRYPT_VERSION);],
            VERSION_GCRYPT=`./conftest$ac_exeext`,
            VERSION_GCRYPT=unknown
           )
AC_TRY_LINK([#include <stdio.h>
             #include "gmp.h"
            ],
            [printf("%s\n",gmp_version);],
            VERSION_GMP=`./conftest$ac_exeext`,
            VERSION_GMP=unknown
           )
AC_TRY_LINK([#include <stdio.h>
             #include "gnutls/gnutls.h"
            ],
            [printf("%s\n",GNUTLS_VERSION);],
            VERSION_GNUTLS=`./conftest$ac_exeext`,
            VERSION_GNUTLS=unknown
           )
AC_TRY_LINK([#include <stdio.h>
             #include "openssl/opensslv.h"
            ],
            [printf("%s\n",OPENSSL_VERSION_TEXT);],
            VERSION_OPENSSL=`./conftest$ac_exeext`,
            VERSION_OPENSSL=unknown
           )
AC_TRY_LINK([#include <stdio.h>
             #include "libssh2.h"],
            [printf("%d.%d.%d\n",LIBSSH2_VERSION_MAJOR,LIBSSH2_VERSION_MINOR,LIBSSH2_VERSION_PATCH);],
            VERSION_LIBSSH2=`./conftest$ac_exeext`,
            VERSION_LIBSSH2=unknown
           )
AC_TRY_LINK([#include <stdio.h>
             #include "curl/curl.h"],
            [printf("%s\n",LIBCURL_VERSION);],
            VERSION_CURL=`./conftest$ac_exeext`,
            VERSION_CURL=unknown
           )
AC_TRY_LINK([#include <stdio.h>
             #include "cdio/cdio.h"
            ],
            [printf("%s\n",CDIO_VERSION);],
            VERSION_CDIO=`./conftest$ac_exeext`,
            VERSION_CDIO=unknown
           )
AC_TRY_LINK([#include <stdio.h>
             #include "pcre.h"
            ],
            [printf("%s\n",pcre_version());],
            VERSION_PCRE=`./conftest$ac_exeext`,
            VERSION_PCRE=unknown
           )

dnl ------------ autoheader: top/bottom include for config.h -----------

AC_CONFIG_HEADERS(bar/config.h)

dnl no AC_DEFINE should occur after this line!

AH_TOP([
#ifndef __CONFIG__
#define __CONFIG__
])

AH_TOP([

#ifndef offsetof
  #define offsetof(type,member) ((size_t)&((type*)0)->member)
#endif /* offsetof */

])

AH_BOTTOM([#endif /* __CONFIG__ */])

dnl --------------------------------------------------------------------

if test "$prefix" = "NONE"; then
  prefix=/usr
fi
INSTALL_DIR=$prefix
if test "$bindir" = "\${exec_prefix}/bin"; then
  if test "$exec_prefix" = NONE; then
    INSTALL_BIN_DIR=$prefix/bin
  else
    INSTALL_BIN_DIR=$exec_prefix
  fi
else
  INSTALL_BIN_DIR=$bindir
fi
if test "$mandir" = "\${datarootdir}/man"; then
  MAN_DIR=$prefix/share/man
else
  MAN_DIR=${mandir}
fi
if test "$localedir" = "\${datarootdir}/locale"; then
  LOCALE_DIR=$prefix/share/locale
else
  LOCALE_DIR=${localedir}
fi

dnl patch PACKAGE entries in config.h
AC_CONFIG_COMMANDS(SET_PACKAGE_IN_CONFIG_H,
                   [sed -i 's/#define PACKAGE_/#define BAR_PACKAGE_/g' bar/config.h]
                  )
dnl make files
AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(bar/Makefile)
AC_CONFIG_FILES(bar/test/Makefile)
AC_CONFIG_FILES(bar/po/Makefile)
AC_CONFIG_FILES(barcontrol/Makefile)

dnl config files
AC_CONFIG_FILES(barcontrol/src/Config.java)

dnl epm file
AC_CONFIG_FILES(epm.list)

AC_CONFIG_COMMANDS(BUILD_NOTE,
                   [echo "";
                    echo "Configuration $VERSION_MAJOR.$VERSION_MINOR ($VERSION_REVISION):";
                    echo "  Host              : $host";
                    echo "  Dynamic linked    : `if test "$ENABLE_LINK_DYNAMIC" = "yes"; then echo yes; else echo no; fi`";
                    echo "  SQLite            : yes ($VERSION_SQLITE)";
                    echo "  zip compression   : `if test "$HAVE_Z"; then echo "yes ($VERSION_Z)"; else echo no; fi`";
                    echo "  bzip2 compression : `if test "$HAVE_BZ2"; then echo "yes ($VERSION_BZ2)"; else echo no; fi`";
                    echo "  lzma compression  : `if test "$HAVE_LZMA"; then echo "yes ($VERSION_LZMA)"; else echo no; fi`";
                    echo "  lzo compression   : `if test "$HAVE_LZO"; then echo "yes ($VERSION_LZO)"; else echo no; fi`";
                    echo "  lz4 compression   : `if test "$HAVE_LZ4"; then echo "yes ($VERSION_LZ4)"; else echo no; fi`";
                    echo "  zstd compression  : `if test "$HAVE_ZSTD"; then echo "yes ($VERSION_ZSTD)"; else echo no; fi`";
                    echo "  xdelta compression: `if test "$HAVE_XDELTA"; then echo yes; else echo no; fi`";
                    echo "  crypto support    : `if test "$HAVE_GCRYPT"; then echo "yes ($VERSION_GCRYPT)"; else echo no; fi`";
                    echo "  FTP support       : `if test "$HAVE_FTP"; then echo yes; else echo no; fi`";
                    echo "  curl support      : `if test "$HAVE_CURL" -a "$HAVE_MXML"; then echo "yes ($VERSION_CURL)"; else echo no; fi`";
                    echo "  SCP/SFTP support  : `if test "$HAVE_SSH2"; then echo "yes ($VERSION_LIBSSH2)"; else echo no; fi`";
                    echo "  TLS/SSL server    : `if test "$HAVE_GNU_TLS"; then echo "yes ($VERSION_OPENSSL, GMP $VERSION_GMP, GnuTLS $VERSION_GNUTLS)"; else echo no; fi`";
                    echo "  ISO 9660 support  : `if test "$HAVE_ISO9660"; then echo "yes ($VERSION_CDIO)"; else echo no; fi`";
                    echo "  PCRE support      : `if test "$HAVE_PCRE"; then echo "yes ($VERSION_PCRE)"; else echo no; fi`";
                    echo "  ICU support       : `if test "$HAVE_ICU"; then echo "yes ($VERSION_ICU)"; else echo no; fi`";
                    echo "  GUI support       : `if test "$ENABLE_GUI" = "yes"; then echo yes; else echo no; fi`";
                    echo "  GUI i18n support  : `if test "$ENABLE_GUI" = "yes" -a "$ENABLE_GUI_I18N" = "yes"; then echo yes; else echo no; fi`";
                    echo "  SWT directory     : $SWT_DIR"
                    echo ""
                    echo "  Debug version     : `if test "$ENABLE_DEBUG" = "yes"; then echo yes; else echo no; fi`";
                    echo "  Crash dump support: `if test "$HAVE_BREAKPAD"; then echo yes; else echo no; fi`";
                    echo "  Stacktrace support: `if test "$HAVE_BFD"; then echo yes; else echo no; fi`";
                    echo ""
                    echo "  Packages support  : `if test -n "$EPM"; then echo yes; else echo no; fi`"
                    echo ""
                    echo "  Install directory : $INSTALL_DIR"
                    echo "  Binary directory  : $INSTALL_BIN_DIR"
                    echo "  Config directory  : $CONFIG_DIR"
                    echo "  Runtime directory : $RUNTIME_DIR"
                    echo "  TLS directory     : $TLS_DIR"
                    echo ""
                    echo "  Heapview diretory : $HEAPVIEW_DIR"
                    echo ""
                    if test    ! "$HAVE_BZ2" \
                            -o ! "$HAVE_LZMA" \
                            -o ! "$HAVE_LZO" \
                            -o ! "$HAVE_LZ4" \
                            -o ! "$HAVE_ZSTD" \
                            -o ! "$HAVE_XDELTA" \
                            -o ! "$HAVE_GCRYPT" \
                            -o ! "$HAVE_FTP" \
                            -o ! "$HAVE_CARES" \
                            -o ! "$HAVE_CURL" \
                            -o ! "$HAVE_MXML" \
                            -o ! "$HAVE_SSL" \
                            -o ! "$HAVE_SSH2" \
                            -o ! "$HAVE_GNU_TLS" \
                            -o ! "$HAVE_ISO9660" \
                            -o ! "$HAVE_PCRE"; then
                      echo "Note: you can download missing additional packages with the script"
                      echo "      './download-third-party-packages.sh'. Please rerun configure"
                      echo "      after downloading."
                    fi
                   ],
                   VERSION_MAJOR="$VERSION_MAJOR"
                   VERSION_MINOR="$VERSION_MINOR"
                   VERSION_REVISION="$VERSION_REVISION"

                   host="$host"
                   ENABLE_LINK_DYNAMIC=$ENABLE_LINK_DYNAMIC
                   HAVE_Z=$HAVE_Z
                   HAVE_BZ2=$HAVE_BZ2
                   HAVE_LZMA=$HAVE_LZMA
                   HAVE_LZO=$HAVE_LZO
                   HAVE_LZ4=$HAVE_LZ4
                   HAVE_ZSTD=$HAVE_ZSTD
                   HAVE_XDELTA=$HAVE_XDELTA
                   HAVE_GCRYPT=$HAVE_GCRYPT
                   HAVE_FTP=$HAVE_FTP
                   HAVE_CURL=$HAVE_CURL
                   HAVE_MXML=$HAVE_MXML
                   HAVE_SSH2=$HAVE_SSH2
                   HAVE_GNU_TLS=$HAVE_GNU_TLS
                   HAVE_ISO9660=$HAVE_ISO9660
                   HAVE_PCRE=$HAVE_PCRE
                   HAVE_ICU=$HAVE_ICU

                   HAVE_BFD=$HAVE_BFD
                   HAVE_BREAKPAD=$HAVE_BREAKPAD
                   ENABLE_DEBUG=$ENABLE_DEBUG

                   EPM="$EPM"

                   ENABLE_GUI=$ENABLE_GUI
                   ENABLE_GUI_I18N=$ENABLE_GUI_I18N

                   SWT_DIR="$SWT_DIR"
                   INSTALL_DIR="$INSTALL_DIR"
                   INSTALL_BIN_DIR="$INSTALL_BIN_DIR"
                   CONFIG_DIR="$CONFIG_DIR"
                   RUNTIME_DIR="$RUNTIME_DIR"
                   TLS_DIR="$TLS_DIR"
                   HEAPVIEW_DIR="$HEAPVIEW_DIR"

                   VERSION_SQLITE="$VERSION_SQLITE"
                   VERSION_Z="$VERSION_Z"
                   VERSION_BZ2="$VERSION_BZ2"
                   VERSION_LZMA="$VERSION_LZMA"
                   VERSION_LZO="$VERSION_LZO"
                   VERSION_LZ4="$VERSION_LZ4"
                   VERSION_ZSTD="$VERSION_ZSTD"
                   VERSION_GCRYPT="$VERSION_GCRYPT"
                   VERSION_CURL="$VERSION_CURL"
                   VERSION_OPENSSL="$VERSION_OPENSSL"
                   VERSION_LIBSSH2="$VERSION_LIBSSH2"
                   VERSION_GMP="$VERSION_GMP"
                   VERSION_GNUTLS="$VERSION_GNUTLS"
                   VERSION_CDIO="$VERSION_CDIO"
                   VERSION_PCRE="$VERSION_PCRE"
                   VERSION_ICU="$VERSION_ICU"
                  )

AC_OUTPUT

dnl --------------------------------------------------------------------

rm -f $tmpFile

dnl --- end of file ---
