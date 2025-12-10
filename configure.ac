dnl --------------------------------------------------------------------
dnl
dnl Contents: BAR auto-configure script
dnl
dnl --------------------------------------------------------------------

dnl ----------------------- additional macros --------------------------

dnl --- print compile info
AC_DEFUN([AC_MSG_CREATE],
[
  _AS_ECHO_N([create $1... ])
])
AC_DEFUN([AC_MSG_COMPILE],
[
  _AS_ECHO_N([compile $1... ])
])
;
dnl --- output log file
AC_DEFUN([AC_MSG_OUTPUT_LOG],
[
  AS_ECHO(["--- error log -------------------------------------------------------------------------"])
  cat $1 | while read s; do AS_ECHO(["  $s"]); done
  AS_ECHO(["---------------------------------------------------------------------------------------"])
])

dnl --- set toolchain variables CFLAGS, CXXFLAGS, LDFLAGS, LIBS
dnl usage: AC_SET_TOOLCHAIN_SETTINGS(<CC_FLAGS>,<CXX_FLAGS>,<CC_DEFINES>,<CC_INCLUDES>,<LD_FLAGS>,<LIBRARY_PATHS>,<STATIC_LIBRARIES>,<LIBRARIES>)
AC_DEFUN([AC_SET_TOOLCHAIN_SETTINGS],
[
  CPPFLAGS="$1"
  CFLAGS="$2 -v"
  CXXFLAGS="$3 -v"
  for z in $4 ""; do
    if test -n "$z"; then
      CPPFLAGS="$CPPFLAGS -D$z"
      CFLAGS="$CFLAGS -D$z"
    fi
  done
  for z in $5 ""; do
    if test -n "$z"; then
      CPPFLAGS="$CPPFLAGS -I$z"
      CFLAGS="$CFLAGS -I$z"
    fi
  done
  LDFLAGS="$6"
  for z in $7 ""; do
    if test -n "$z"; then
      LDFLAGS="$LDFLAGS -L$z"
    fi
  done
  LIBS=""
  for z in $8 ""; do
    if test -n "$z"; then
      LIBS="$LIBS -l$z"
    fi
  done
  for z in $9 ""; do
    if test -n "$z"; then
      LIBS="$LIBS -l$z"
    fi
  done
  for z in $8 ""; do
    if test -n "$z"; then
      LIBS="$LIBS -l$z"
    fi
  done
  for z in $9 ""; do
    if test -n "$z"; then
      LIBS="$LIBS -l$z"
    fi
  done

  # some host tests need dynamic libraries
  export LD_LIBRARY_PATH=`pwd`/extern/lib
])

dnl --- check if C compiler support option
dnl usage: AC_CC_OPTION(<option>,<variable>...)
AC_DEFUN([AC_CC_OPTION],
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

dnl --- check if C++ compiler support option
dnl usage: AC_CXX_OPTION(<option>,<variable>...)
AC_DEFUN([AC_CXX_OPTION],
[
  AC_REQUIRE([AC_PROG_CXX])
  AC_CACHE_CHECK(
    [if $CXX support option $1],
    ac_cv_cxx_option_`echo $1|tr -c -d a-zA-Z0-9_`,
    [
      cat << \EOF > test.cpp
int main(void)
{
  return 0;
}
EOF
      if AC_TRY_COMMAND($CXX $1 test.cpp -o test.o); then
        ac_supported=yes
        for s in $2; do
          eval "if test -n \"\$$s\"; then $s=\"\$$s \"; fi"
          eval "$s=\"\$$s$1\""
        done
      else
        ac_supported=no
      fi
      AS_VAR_SET(ac_cv_cxx_option_`echo $1|tr -c -d a-zA-Z0-9_`,$ac_supported)

      rm -f test.cpp test.o
    ]
  )dnl
  AC_PROVIDE([$0])dnl
])

dnl --- check if linker support option
dnl usage: AC_LINKER_OPTION(<option>,<variable>...)
AC_DEFUN([AC_LINKER_OPTION],
[
  AC_REQUIRE([AC_PROG_CPP])
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

dnl --- check for C++ header
dnl usage: AC_CHECK_CXX_HEADER(<header>)
dnl
dnl Note: it seems
dnl
dnl AC_LANG_PUSH(C++)
dnl AC_CHECK_HEADER(...)
dnl AC_LANG_POP()
dnl
dnl find the header, but does not create a AC_SUBST()?
AC_DEFUN([AC_CHECK_CXX_HEADER],
[
  AC_REQUIRE([AC_PROG_CXX])
  AC_LANG_PUSH(C++)
  AC_CHECK_HEADER($1,[AC_DEFINE(AS_TR_CPP(HAVE_$1),1,[defined if header <$1> is available])])
  AC_LANG_POP()
  AC_PROVIDE([$0])dnl
])

dnl --- get constant (define)
dnl usage: AC_CONSTANT_VALUE(<text>,<variable>,<includes>,<source>,<include search path>,[<trim characters>])
AC_DEFUN([AC_CONSTANT_VALUE],
[
  AC_REQUIRE([AC_PROG_CPP])
  AS_VAR_PUSHDEF([ac_cv_constant_value],[ac_cv_constant_value_$2])dnl
  if test -n "$1"; then
    AC_CACHE_CHECK(
      [$1],
      ac_cv_constant_value,
      [
        AC_LANG_CONFTEST([AC_LANG_PROGRAM([[$3]],[[__CONFTEST__ $4]])])
        if AC_TRY_COMMAND($CPP $CPPFLAGS `if test -n "$5"; then echo -I$5; fi` -P -E conftest.c -o conftest); then
          ac_cv_constant_value=`grep __CONFTEST__ conftest|sed 's/__CONFTEST__\s*//g'`
        else
          ac_cv_constant_value=
        fi
        if test -n "AS_ESCAPE([[$6]])"; then
          ac_cv_constant_value=`echo $ac_cv_constant_value|tr -d "AS_ESCAPE([[$6]])"`
        fi
        AS_VAR_SET($2,$ac_cv_constant_value)

        rm -f conftest.c conftest
      ]
    )dnl
  else
    AC_CACHE_VAL(
      ac_cv_constant_value,
      [
        AC_LANG_CONFTEST([AC_LANG_PROGRAM([[$3]],[[__CONFTEST__ $4]])])
        if AC_TRY_COMMAND($CPP $CPPFLAGS `if test -n "$5"; then echo -I$5; fi` -P -E conftest.c -o conftest); then
          ac_cv_constant_value=`grep __CONFTEST__ conftest|sed 's/__CONFTEST__\s*//g'`
        else
          ac_cv_constant_value=
        fi
        if test -n "AS_ESCAPE([[$6]])"; then
          ac_cv_constant_value=`echo $ac_cv_constant_value|tr -d "AS_ESCAPE([[$6]])"`
        fi
        AS_VAR_SET($2,$ac_cv_constant_value)

        rm -f conftest.c conftest
      ]
    )dnl
  fi
  AS_VAR_POPDEF([ac_cv_constant_value])dnl
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
        AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <stdint.h>
                                         `echo $ac_headers|sed 's/+/\n/g'|while read s; do if test -n "$s"; then echo $s|sed 's/\(.*\)/#include <\\1>/g'; fi; done`
                                        ],
                                        [`if test -z "$ac_headers"; then echo "extern void $1();"; fi`
                                         #ifdef $1
                                         #else
                                           return (intptr_t)$1;
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
      ac_libraries=""
      for ac_library in $2; do
        if test -n "$ac_libraries"; then
          ac_libraries="$ac_libraries -l$ac_library"
        else
          ac_libraries="-l$ac_library"
        fi
      done
      LIBS="-Wl,-Bstatic $ac_libraries -Wl,-Bdynamic $ac_func_search_static_libraries_save_LIBS"
      if test -z "$ac_libraries"; then
        ac_result="none required"
      else
        ac_result=$ac_libraries
      fi
      AC_LINK_IFELSE([AC_LANG_PROGRAM([`echo $5|sed 's/ /\n/g'|while read s; do if test -n "$s"; then echo $s|sed 's/\(.*\)/#include <\\1>/g'; fi; done`],
                                      [`if test -z "$5"; then echo "extern void $1();"; fi`
                                       return (int)$1;
                                      ]
                                     )
                     ],
                     [AS_VAR_SET([ac_search_static_libraries],[$ac_result]); break]
                    )
      AS_VAR_SET_IF([ac_search_static_libraries],,[AS_VAR_SET([ac_search_static_libraries],[no])])

      dnl restore C compiler settings
      LIBS=$ac_func_search_static_libraries_save_LIBS
      CFLAGS=$ac_func_search_static_libraries_save_CFLAGS
    ]
  )
  AS_VAR_COPY([ac_result],[ac_search_static_libraries])
  AS_IF([test "$ac_result" != no],[test "$ac_result" = "none required" || LIBS="$ac_result $LIBS" $3],[$4])
  AS_VAR_POPDEF([ac_search_static_libraries])dnl
])

dnl --- check if C compiler support static_assert()
dnl usage: AC_CHECK_STATIC_ASSERT(<if found>,<if not found>,<headers>)
AC_DEFUN([AC_CHECK_STATIC_ASSERT],
[
  AC_CACHE_CHECK(
    [if static_assert() is supported],
    ac_cv_static_assert,
    [
      AC_LINK_IFELSE([AC_LANG_PROGRAM([`echo $3|sed 's/ /\n/g'|while read s; do if test -n "$s"; then echo $s|sed 's/\(.*\)/#include <\\1>/g'; fi; done`],
                                      [`if test -z "$3"; then echo "#include <assert.h>"; fi`
                                       static_assert(1 == 1,"test");
                                      ]
                                     )
                     ],
                     [AS_VAR_SET([ac_cv_static_assert],[yes])],
                     [AS_VAR_SET([ac_cv_static_assert],[no])]
                    )
    ]
  )
  AS_IF([test "$ac_cv_static_assert" != "no"],[AC_DEFINE(HAVE_STATIC_ASSERT,1,[defined if static_assert() is available])])
  AC_PROVIDE([$0])dnl
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
      ac_cv_mkdir_arguments_count="unknown"

      if test "$ac_cv_mkdir_arguments_count" = "unknown"; then
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
      if test "$ac_cv_mkdir_arguments_count" = "unknown"; then
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
  )
  AS_IF([test "$ac_cv_mkdir_arguments_count" != "unknown"],[$1],:,[$2])
  AC_DEFINE_UNQUOTED(MKDIR_ARGUMENTS_COUNT,$ac_cv_mkdir_arguments_count,[number of mkdir() arguments])

  AC_PROVIDE([$0])dnl
])

dnl --- check number of arguments of bfd_section_vma
dnl usage: AC_CHECK_BFD_SECTION_VMA_ARGUMENTS
AC_DEFUN([AC_CHECK_BFD_SECTION_VMA_ARGUMENTS],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK(
    [number of arguments of bfd_section_vma()],
    ac_cv_bfd_section_vma_arguments_count,
    [
      ac_cv_bfd_section_vma_arguments_count="unknown"

      if test "$ac_cv_bfd_section_vma_arguments_count" = "unknown"; then
        cat << \EOF > conftest.c
#include <stdlib.h>
#include <bfd.h>
int main(void) { asection *section = NULL; bfd_section_vma(section); return 0; }
EOF
        ${CC} ${CFLAGS} -Wall -Werror -c conftest.c -o conftest.o 1>/dev/null 2>/dev/null; rc=$?
        if test $rc -eq 0; then
          ac_cv_bfd_section_vma_arguments_count=1
        fi
        rm -f conftest.c conftest.o
      fi
      if test "$ac_cv_bfd_section_vma_arguments_count" = "unknown"; then
        cat << \EOF > conftest.c
#include <stdlib.h>
#include <bfd.h>
int main(void) { bfd *abfd = NULL; asection *section = NULL; bfd_section_vma(abfd,section); (void)abfd; return 0; }
EOF
        ${CC} ${CFLAGS} -Wall -Werror -c conftest.c -o conftest.o 1>/dev/null 2>/dev/null; rc=$?
        if test $rc -eq 0; then
          ac_cv_bfd_section_vma_arguments_count=2
        fi
        rm -f conftest.c conftest.o
      fi
    ]
  )
  AS_IF([test "$ac_cv_bfd_section_vma_arguments_count" != "unknown"],[$1],:,[$2])
  AC_DEFINE_UNQUOTED(BFD_SECTION_VMA_ARGUMENTS_COUNT,$ac_cv_bfd_section_vma_arguments_count,[number of bfd_section_vma() arguments])

  AC_PROVIDE([$0])dnl
])

dnl --- check number of arguments of bfd_section_size
dnl usage: AC_CHECK_BFD_SECTION_SIZE_ARGUMENTS
AC_DEFUN([AC_CHECK_BFD_SECTION_SIZE_ARGUMENTS],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_CACHE_CHECK(
    [number of arguments of bfd_section_size()],
    ac_cv_bfd_section_size_arguments_count,
    [
      ac_cv_bfd_section_size_arguments_count="unknown"

      if test "$ac_cv_bfd_section_size_arguments_count" = "unknown"; then
        cat << \EOF > conftest.c
#include <stdlib.h>
#include <bfd.h>
int main(void) { asection *section = NULL; bfd_section_size(section); return 0; }
EOF
        ${CC} ${CFLAGS} -Wall -Werror -c conftest.c -o conftest.o 1>/dev/null 2>/dev/null; rc=$?
        if test $rc -eq 0; then
          ac_cv_bfd_section_size_arguments_count=1
        fi
        rm -f conftest.c conftest.o
      fi
      if test "$ac_cv_bfd_section_size_arguments_count" = "unknown"; then
        cat << \EOF > conftest.c
#include <stdlib.h>
#include <bfd.h>
int main(void) { bfd *abfd = NULL; asection *section = NULL; bfd_section_size(abfd,section); (void)abfd; return 0; }
EOF
        ${CC} ${CFLAGS} -Wall -Werror -c conftest.c -o conftest.o 1>/dev/null 2>/dev/null; rc=$?
        if test $rc -eq 0; then
          ac_cv_bfd_section_size_arguments_count=2
        fi
        rm -f conftest.c conftest.o
      fi
    ]
  )
  AS_IF([test "$ac_cv_bfd_section_size_arguments_count" != "unknown"],[$1],:,[$2])
  AC_DEFINE_UNQUOTED(BFD_SECTION_SIZE_ARGUMENTS_COUNT,$ac_cv_bfd_section_size_arguments_count,[number of bfd_section_size() arguments])

  AC_PROVIDE([$0])dnl
])

dnl --- detect Java compiler version
dnl usage: AC_JAVAC_VERSION(<variable>)
AC_DEFUN([AC_JAVAC_VERSION],
[
  AC_REQUIRE([AC_PROG_JAVAC])
  AC_CACHE_CHECK(
    [$JAVAC version],
    ac_cv_prog_java_version,
    [
      ac_cv_prog_java_version=`$JAVAC -version 2>&1|sed 's/^.* \([[^_]]\+\).*/\\1/g'`
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

dnl --- check sudo command
dnl usage: AC_PROG_SUDO_COMMAND(<text>,<command>,<if found>,<if not found>)
AC_DEFUN([AC_PROG_SUDO_COMMAND],
[
  if test -n "$SUDO"; then
    AC_MSG_CHECKING([for execution of '$SUDO $1'])
    AS_VAR_PUSHDEF([ac_cv_prog_sudo_command],[ac_cv_prog_sudo_command_`echo $1|tr -c -d a-zA-Z0-9_`])dnl
    AC_CACHE_VAL(
      [ac_cv_prog_sudo_command],
      [
        # escape command
        ac_pattern="$2"
        ac_pattern=`echo "$ac_pattern"|sed 's/\(\[[\)/\\\\\1/g'`
        ac_pattern=`echo "$ac_pattern"|sed 's/\(\]]\)/\\\\\1/g'`
        ac_pattern=`echo "$ac_pattern"|sed 's/=/\\\\\\\\=/g'`
        ac_pattern=`echo "$ac_pattern"|sed 's/ \\+/\\\\s+/g'`

        # test sudo
        SUDO_ASKPASS=false $SUDO -n -ll 2>/dev/null | grep -E "$ac_pattern" 1>/dev/null 2>/dev/null
        if   test $? -eq 0; then
          ac_result=yes
        else
          ac_result=no
        fi
        AC_MSG_RESULT($ac_result)
        AS_VAR_SET([ac_cv_prog_sudo_command],$ac_result)
      ]
    )
  else
    ac_result=no
  fi
  AS_IF([test "$ac_result" != "no"],[$3],[$4])
  AS_VAR_POPDEF([ac_cv_prog_sudo_command])dnl
  AC_PROVIDE([$0])dnl
])

dnl --- detect wine C: drive path
dnl usage: AC_WINE_C_DRIVE(<variable>)
AC_DEFUN([AC_WINE_C_DRIVE],
[
  AC_CACHE_CHECK(
    [wine C: drive],
    ac_cv_wine_c_drive,
    [
      ac_cv_wine_c_drive=`winepath C: 2>/dev/null`
      ac_cv_wine_c_drive=`dirname $ac_cv_wine_c_drive 2>/dev/null`
      ac_cv_wine_c_drive=`realpath $ac_cv_wine_c_drive/c: 2>/dev/null`
      AS_VAR_SET($1,$ac_cv_wine_c_drive)
    ]
  )
  AC_PROVIDE([$0])dnl
])

dnl --- detect ISCC.exe
dnl usage: AC_PROG_ISCC(<variable>)
AC_DEFUN([AC_PROG_ISCC],
[
  AC_REQUIRE([AC_PROG_WINE])
  AC_REQUIRE([AC_WINE_C_DRIVE])
  AC_CACHE_CHECK(
    [ISCC],
    ac_cv_prog_iscc,
    [
      $WINE c\:/Program\ Files\ \(x86\)/Inno\ Setup\ 5/ISCC.exe /\? 1>/dev/null 2>/dev/null
      if test $? -lt 2; then
        ac_cv_prog_iscc=`winepath c\:/Program\ Files\ \(x86\)/Inno\ Setup\ 5/ISCC.exe 2>/dev/null`
      fi
      $WINE c\:/Program\ Files/Inno\ Setup\ 5/ISCC.exe \/\? 1>/dev/null 2>/dev/null
      if test $? -lt 2; then
        ac_cv_prog_iscc=`winepath c\:/Program\ Files/Inno\ Setup\ 5/ISCC.exe 2>/dev/null`
      fi
      AS_VAR_SET($1,$ac_cv_prog_iscc)
    ]
  )
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

AC_SUBST(VERSION_MAJOR)                      dnl version major number
AC_SUBST(VERSION_MINOR)                      dnl version minor number
AC_SUBST(VERSION_PATCH)                      dnl version patch level
AC_SUBST(VERSION_REVISION)                   dnl version revision number (SVN number)

AC_SUBST(COPYRIGHT_DATE)                     dnl copyright date

AC_SUBST(PLATFORM)                           dnl platform name
AC_SUBST(CPU)                                dnl cpu name

AC_SUBST(FILE_SEPARATOR_CHAR)                dnl file separator path character (/ or \\)
AC_SUBST(FILE_SEPARATOR_STRING)              dnl file separator path character as string (/ or \\)

AC_SUBST(OBJEXT)                             dnl object extension
AC_SUBST(EXEEXT)                             dnl executable extension
AC_SUBST(SHELLEXT)                           dnl shell extension

AC_SUBST(ENABLE_DEBUG)                       dnl "yes" for debug version, "no" otherwise
AC_SUBST(ENABLE_DEBUG_THREAD_CRASH_HANDLERS) dnl "yes" for debug version with crash handlers, "no" otherwise
AC_SUBST(ENABLE_BZ2)                         dnl "yes" for bzip2 support, "no" otherwise
AC_SUBST(ENABLE_LZMA)                        dnl "yes" for lzma support, "no" otherwise
AC_SUBST(ENABLE_LZO)                         dnl "yes" for lzo support, "no" otherwise
AC_SUBST(ENABLE_LZ4)                         dnl "yes" for lz4 support, "no" otherwise
AC_SUBST(ENABLE_ZSTD)                        dnl "yes" for zstd support, "no" otherwise
AC_SUBST(ENABLE_XDELTA3)                     dnl "yes" for xdelta3 support, "no" otherwise
AC_SUBST(ENABLE_GCRYPT)                      dnl "yes" for gcrypt support, "no" otherwise
AC_SUBST(ENABLE_CURL)                        dnl "yes" for curl support, "no" otherwise
AC_SUBST(ENABLE_SSH)                         dnl "yes" for ssh support, "no" otherwise
AC_SUBST(ENABLE_TLS)                         dnl "yes" for tls support, "no" otherwise
AC_SUBST(ENABLE_ISO9660)                     dnl "yes" for iso9660 support, "no" otherwise
AC_SUBST(ENABLE_SMBCLIENT)                   dnl "yes" for SMB client support, "no" otherwise
AC_SUBST(ENABLE_PCRE)                        dnl "yes" for PCRE support, "no" otherwise
AC_SUBST(ENABLE_GUI)                         dnl "yes" for GUI support, "no" otherwise
AC_SUBST(ENABLE_GUI_I18N)                    dnl "yes" for GUI i18n support, "no" otherwise

AC_SUBST(HAVE_ICU)                           dnl "1" for ICU support, "" otherwise
AC_SUBST(HAVE_Z)                             dnl "1" for zlib support, "" otherwise
AC_SUBST(HAVE_BZ2)                           dnl "1" for bzip2 support, "" otherwise
AC_SUBST(HAVE_LZMA)                          dnl "1" for lzma support, "" otherwise
AC_SUBST(HAVE_LZO)                           dnl "1" for lzo support, "" otherwise
AC_SUBST(HAVE_LZ4)                           dnl "1" for lz4 support, "" otherwise
AC_SUBST(HAVE_ZSTD)                          dnl "1" for zstd support, "" otherwise
AC_SUBST(HAVE_XDELTA3)                       dnl "1" for xdelta3 support, "" otherwise
AC_SUBST(HAVE_IDN2)                          dnl "1" for idn2 support, "" otherwise
AC_SUBST(HAVE_GCRYPT)                        dnl "1" for gcrypt support, "" otherwise
AC_SUBST(HAVE_FTP)                           dnl "1" for ftp support, "" otherwise
AC_SUBST(HAVE_CARES)                         dnl "1" for c-ares support, "" otherwise
AC_SUBST(HAVE_CURL)                          dnl "1" for curl support, "" otherwise
AC_SUBST(HAVE_MXML)                          dnl "1" for mxml support, "" otherwise
AC_SUBST(HAVE_SSL)                           dnl "1" for ssl support, "" otherwise
AC_SUBST(HAVE_SSH2)                          dnl "1" for ssh2 support, "" otherwise
AC_SUBST(HAVE_GNU_TLS)                       dnl "1" for GNU TLS support, "" otherwise
AC_SUBST(HAVE_ISO9660)                       dnl "1" for ISO9660 support, "" otherwise
AC_SUBST(HAVE_SMB2)                          dnl "1" for SMB2 support, "" otherwise
AC_SUBST(HAVE_PCRE)                          dnl "1" for pcre support, "" otherwise
AC_SUBST(HAVE_MARIADB)                       dnl "1" for MariaDB support, "" otherwise
AC_SUBST(HAVE_POSTGRESQL)                    dnl "1" for PostgreSQL support, "" otherwise
AC_SUBST(HAVE_PAR2)                          dnl "1" for PAR2 support, "" otherwise
AC_SUBST(HAVE_ISOFS)                         dnl "1" for isofs support, "" otherwise
AC_SUBST(HAVE_UUID)                          dnl "1" for uuid support, "" otherwise
AC_SUBST(HAVE_RCU)                           dnl "1" for rcu support, "" otherwise
AC_SUBST(HAVE_XFS)                           dnl "1" for xfs support, "" otherwise
AC_SUBST(HAVE_BURN)                          dnl "1" for burn support, "" otherwise
AC_SUBST(HAVE_MOUNT)                         dnl "1" for mount support, "" otherwise
AC_SUBST(HAVE_PTHREADS_W32)                  dnl "1" for pthreads-w32 support, "" otherwise
AC_SUBST(HAVE_BFD)                           dnl "1" for BFD support, "" otherwise

AC_SUBST(HAVE_STATIC_ASSERT)                 dnl "1" if static_assert() is supported, "" otherwise
#AC_SUBST(HAVE_USLEEP)                        dnl "1" if usleep() is avaiulable, "" otherwise

AC_SUBST(INSTALL_DIR)                        dnl installation directory
AC_SUBST(INSTALL_BIN_DIR)                    dnl installation directory for binaries
AC_SUBST(CONFIG_DIR)                         dnl configuration directory
AC_SUBST(RUNTIME_DIR)                        dnl runtime directory
AC_SUBST(TLS_DIR)                            dnl TLS/SSL directory
AC_SUBST(MAN_DIR)                            dnl man page directory
AC_SUBST(LOCALE_DIR)                         dnl locale directory

AC_SUBST(CPP)                                dnl C preprocessor
AC_SUBST(CC)                                 dnl C compiler
AC_SUBST(LD)                                 dnl linker
AC_SUBST(STRIP)                              dnl strip
AC_SUBST(OBJDUMP)                            dnl objdump
AC_SUBST(OBJCOPY)                            dnl objcopy

AC_SUBST(CC_FLAGS)                           dnl C compiler flags
AC_SUBST(CC_FLAGS_RELEASE)                   dnl additional C compiler flags for release version
AC_SUBST(CC_FLAGS_DEBUG)                     dnl additional C compiler flags for debug version
AC_SUBST(CC_FLAGS_GCOV)                      dnl additional C compiler flags for gcov version
AC_SUBST(CC_FLAGS_GPROF)                     dnl additional C compiler flags for gprof version
AC_SUBST(CC_FLAGS_VALGRIND)                  dnl additional C compiler flags for valgrind version
AC_SUBST(CC_WARN_FLAGS)                      dnl C compiler warning flags
AC_SUBST(CXX_FLAGS)                          dnl C++ compiler flags
AC_SUBST(CXX_WARN_FLAGS)                     dnl C++ compiler warning flags
AC_SUBST(CC_FLAGS_OPTIMIZE)                  dnl C compiler optimization flags
AC_SUBST(CC_FLAGS_OPTIMIZE_RELEASE)          dnl additional C compiler optimization flags for release version
AC_SUBST(CC_FLAGS_OPTIMIZE_DEBUG)            dnl addtiional C compiler optimization flags for debug version
AC_SUBST(CC_FLAGS_OPTIMIZE_VALGRIND)         dnl addtiional C compiler optimization flags for valgrind version
AC_SUBST(CC_DEFINES)                         dnl C compiler defines
AC_SUBST(CC_DEFINES_RELEASE)                 dnl additional C compiler defines for debug version
AC_SUBST(CC_DEFINES_DEBUG)                   dnl additional C compiler defines for debug version
AC_SUBST(CC_DEFINES_VALGRIND)                dnl additional C compiler defines for valgrind version
AC_SUBST(CC_INCLUDES)                        dnl C compiler include paths

AC_SUBST(LD_FLAGS)                           dnl linker flags
AC_SUBST(LD_FLAGS_RELEASE)                   dnl additional linker flags for release version
AC_SUBST(LD_FLAGS_DEBUG)                     dnl additional linker flags for debug version
AC_SUBST(LD_FLAGS_GCOV)                      dnl additional linker flags for gcov version
AC_SUBST(LD_FLAGS_VALGRIND)                  dnl additional linker flags for valgrind version

AC_SUBST(LIBRARY_PATHS)                      dnl library paths
AC_SUBST(LIBRARIES)                          dnl libraries to link
AC_SUBST(STATIC_LIBRARIES)                   dnl libraries to link static

AC_SUBST(JAVA)                               dnl java command
AC_SUBST(JAVAC)                              dnl java compiler
AC_SUBST(JAR)                                dnl jar command

AC_SUBST(XGETTEXT)                           dnl xgettext command
AC_SUBST(MSGMERGE)                           dnl msgmerge command
AC_SUBST(MSGFMT)                             dnl msgfmt command

AC_SUBST(JAVA_DATA_MODEL)                    dnl java data model (32 or 64)
AC_SUBST(JAVA_FLAGS)                         dnl java flags
AC_SUBST(JAVAC_FLAGS)                        dnl java compiler flags
AC_SUBST(JAVA_PATH_SEPARATOR_CHAR)           dnl java path separator (: or ;)

AC_SUBST(SWT_DIR)                            dnl SWT directory
AC_SUBST(LAUNCH4JC)                          dnl launch4jc
# TODO: remove
AC_SUBST(LAUNCH4J_DIR)                       dnl launch4j directory
AC_SUBST(JRE_WINDOWS_DIR)                    dnl JRE Windows directory

AC_SUBST(DUMP_SYMS)                          dnl Breakpad dump symbols tool

AC_SUBST(LCOV)                               dnl lcov command
AC_SUBST(GENHTML)                            dnl genhtml command

AC_SUBST(CERTOOL)                            dnl GNU TLS certtool
AC_SUBST(OPENSSL)                            dnl OpenSSL key tool
AC_SUBST(KEYTOOL)                            dnl Java keytool

AC_SUBST(UNOCONV)                            dnl OO conversion tool
AC_SUBST(TXT2MAN)                            dnl text-to-man page tool
AC_SUBST(CONVERT)                            dnl convert tool

AC_SUBST(ATTR)                               dnl attr command
AC_SUBST(TIME)                               dnl time command
AC_SUBST(GETFATTR)                           dnl getfattr command
AC_SUBST(SETFATTR)                           dnl setfattr command

AC_SUBST(MKFS_EXT2)                          dnl mksfs.ext2 command
AC_SUBST(MKFS_EXT3)                          dnl mksfs.ext3 command
AC_SUBST(MKFS_EXT4)                          dnl mksfs.ext4 command
AC_SUBST(MKFS_MSDOS)                         dnl mksfs.msdos command
AC_SUBST(MKFS_EXFAT)                         dnl mksfs.exfat command
AC_SUBST(MKFS_XFS)                           dnl mksfs.xfs command

AC_SUBST(SOCAT)                              dnl socat command

AC_SUBST(XGETTEXT)                           dnl xgettext command
AC_SUBST(MSGMERGE)                           dnl msgmerge command
AC_SUBST(MSGFMT)                             dnl msgfmt command

AC_SUBST(MYSQL)                              dnl mysql command
AC_SUBST(PSQL)                               dnl psql command
AC_SUBST(CREATEDB)                           dnl createdb command

AC_SUBST(DOCKER)                             dnl docker
AC_SUBST(WINE)                               dnl wine
AC_SUBST(WINE_C_DRIVE)                       dnl wine c:-drive path
AC_SUBST(ISCC)                               dnl Inno Setup

AC_SUBST(HAVE_SUDO_COMMANDS,1)               dnl sudo commands
AC_SUBST(HAVE_SUDO_COMMAND_MKNOD_C,1)        dnl sudo command mkdnod * c *
AC_SUBST(HAVE_SUDO_COMMAND_MKNOD_B,1)        dnl sudo command mkdnod * b *
AC_SUBST(HAVE_SUDO_COMMAND_CHATTR,1)         dnl sudo command chattr +-[ia] *
AC_SUBST(HAVE_SUDO_COMMAND_MOUNT_UMOUNT,1)   dnl sudo command mount /dev/loop[01] test/dev[01], umount test/dev[01]
AC_SUBST(HAVE_SUDO_COMMAND_LOSETUP,1)        dnl sudo command losetup -d /dev/loop[01], losetup /dev/loop[01] test/dev[01].dat, losetup /dev/loop[01] test/dev0[ab].dat
AC_SUBST(HAVE_SUDO_COMMAND_CHMOD,1)          dnl sudo command chmod o+rw /dev/loop[01]
AC_SUBST(HAVE_SUDO_COMMAND_DD,1)             dnl sudo command dd if=/dev/loop[[01]] of=-
AC_SUBST(HAVE_SUDO_COMMAND_MKFS,1)           dnl sudo command mkfs -t * /dev/loop[01]
AC_SUBST(HAVE_SUDO_COMMAND_CP,1)             dnl sudo command cp -r -p data/* test/dev[01]
AC_SUBST(HAVE_SUDO_COMMAND_DIFF,1)           dnl sudo command diff -r test/dev0 test/dev1

AC_SUBST(HAVE_SUDO_COMMAND_CHATTR_I,1)       dnl sudo command chattr +-i
AC_SUBST(HAVE_SUDO_COMMAND_CHATTR_A,1)       dnl sudo command chattr +-a

AC_SUBST(HAVE_USER_TEST,1)                   dnl user test exists

AC_SUBST(HEAPVIEW_DIR)                       dnl heapview directory

dnl ----------------------- initialize variables -----------------------

CPP_FLAGS="$CPPFLAGS"
CC_FLAGS="$CFLAGS"
CC_WARN_FLAGS=""
CXX_FLAGS="$CXXFLAGS"
CXX_WARN_FLAGS=""
CC_FLAGS_OPTIMIZE=""
CC_FLAGS_OPTIMIZE_RELEASE=""
CC_FLAGS_OPTIMIZE_DEBUG=""
CC_FLAGS_OPTIMIZE_VALGRIND=""
CC_DEFINES="_GNU_SOURCE"
CC_DEFINES_RELEASE=""
CC_DEFINES_DEBUG=""
CC_DEFINES_VALGRIND=""
CC_INCLUDES=""

LD_FLAGS="$LDFLAGS"
LIBRARY_PATHS=""
LIBRARIES=""
STATIC_LIBRARIES=""

ENABLE_DEBUG="no"
ENABLE_DEBUG_THREAD_CRASH_HANDLERS="yes"
ENABLE_LINK_DYNAMIC="no"
ENABLE_BZ2="yes"
ENABLE_LZMA="yes"
ENABLE_LZO="yes"
ENABLE_LZ4="yes"
ENABLE_ZSTD="yes"
ENABLE_XDELTA3="yes"
ENABLE_GCRYPT="yes"
ENABLE_CURL="yes"
ENABLE_SSH="yes"
ENABLE_TLS="yes"
ENABLE_ISO9660="yes"
ENABLE_SMBCLIENT="yes"
ENABLE_PCRE="yes"
ENABLE_MARIADB="yes"
ENABLE_POSTGRESQL="yes"
ENABLE_PTHREADS_W32="yes"
ENABLE_PAR2="yes"
ENABLE_ISOFS="yes"
ENABLE_XFS="yes"
ENABLE_BURN="yes"
ENABLE_MOUNT="yes"
ENABLE_BFD="yes"
ENABLE_GUI="yes"
ENABLE_GUI_I18N="yes"
ENABLE_EXTERN_COMPILE="yes"

# temporary file
tmpFile=`mktemp configure-XXXXXX`

dnl ---------------------------- check system --------------------------

# get absolute source directory path (Note: do not use realpath; may be not available on some platforms)
SOURCE_DIR=`(cd $srcdir; pwd)`

# get platform specific settings
dnl AC_CANONICAL_SYSTEM
dnl $host
dnl $host_cpu
dnl $host_os
dnl $build
dnl $build_cpu
dnl $build_os
dnl AC_WARN([host: $host cpu=$host_cpu os=$host_os])
dnl AC_WARN([build: $build cpu=$build_cpu os=$build_os])
case $host_os in
  linux-*)
    PLATFORM=LINUX
    SYSTEM_NAME=Linux
    CPU=$host_cpu
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
  sunos | solaris*)
    PLATFORM=SOLARIS
    SYSTEM_NAME=Solaris
    CPU=$host_cpu
    FILE_SEPARATOR_CHAR="/"
    JAVA_PATH_SEPARATOR=":"
    SHELLEXT=""
    CPU_CORES=1
    ;;
  darwin*)
    PLATFORM=MAC_OSX
    SYSTEM_NAME=MacOSX
    CPU=$host_cpu
    FILE_SEPARATOR_CHAR="/"
    JAVA_PATH_SEPARATOR=":"
    SHELLEXT=""
    CPU_CORES=1
    ;;
  windows*|mingw*)
    PLATFORM=WINDOWS
    SYSTEM_NAME=Windows
    CPU=$host_cpu
    FILE_SEPARATOR_CHAR="\\\\"
    JAVA_PATH_SEPARATOR=";"
    SHELLEXT=".cmd"
    nproc 1>/dev/null 2>&1
    if test $? -eq 0; then
      CPU_CORES=`nproc`
    else
      CPU_CORES=1
    fi
    ;;
  *)
    AC_MSG_ERROR([unknown host operation system '$host_os'. Please specify host as (cpu-vendor-os or cpu-os). Supported host operating systems: $host_supported_os])
    ;;
esac

dnl ---------------------------- directories ---------------------------

dnl --------------------- --with/--without options ---------------------

#TODO: remove
AC_ARG_WITH(
  toolchain-prefix,
  AC_HELP_STRING([--with-toolchain-prefix=<prefix>],[toolchain prefix]),
  [TOOLCHAIN_PREFIX=$withval]
)
AC_ARG_WITH(
  config-dir,
  AC_HELP_STRING([--with-config-dir=<path>],[configuration directory (default: /etc)]),
  [CONFIG_DIR=$withval],
  [CONFIG_DIR=/etc]
)
AC_ARG_WITH(
  runtime-dir,
  AC_HELP_STRING([--with-runtime-dir=<path>],[runtime directory (default: /var/lib)]),
  [RUNTIME_DIR=$withval],
  [RUNTIME_DIR=/var/lib]
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
  AC_HELP_STRING([--enable-debug],[enable debug functions]),
  [ENABLE_DEBUG=$enableval]
)
AC_ARG_ENABLE(
  debug-thread-crash-handlers,
  AC_HELP_STRING([--disable-debug-thread-crash-handlers],[enable debug thread crash handlers]),
  [if test "$enableval" = "no"; then ENABLE_DEBUG_THREAD_CRASH_HANDLERS=no; fi]
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
  all,
  AC_HELP_STRING([--disable-all],[disable debug functions]),
  [ENABLE_BZ2=$enableval
   ENABLE_LZMA=$enableval
   ENABLE_LZO=$enableval
   ENABLE_LZ4=$enableval
   ENABLE_ZSTD=$enableval
   ENABLE_XDELTA3=$enableval
   ENABLE_GCRYPT=$enableval
   ENABLE_CURL=$enableval
   ENABLE_SSH=$enableval
   ENABLE_TLS=$enableval
   ENABLE_ISO9660=$enableval
   ENABLE_SMBCLIENT=$enableval
   ENABLE_MARIADB=$enableval
   ENABLE_POSTGRESQL=$enableval
   ENABLE_PCRE=$enableval
   ENABLE_PAR2=$enableval
   ENABLE_ISOFS=$enableval
   ENABLE_XFS=$enableval
   ENABLE_BURN=$enableval
   ENABLE_MOUNT=$enableval
   ENABLE_GUI=$enableval
   ENABLE_GUI_I18N=$enableval
  ]
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
  smbclient,
  AC_HELP_STRING([--disable-smbclient],[disable SMB client support]),
  [ENABLE_SMBCLIENT=$enableval]
)
AC_ARG_ENABLE(
  mariadb,
  AC_HELP_STRING([--disable-mariadb],[disable MariaDB support]),
  [ENABLE_MARIADB=$enableval]
)
AC_ARG_ENABLE(
  postgresql,
  AC_HELP_STRING([--disable-postgresql],[disable PostgreSQL support]),
  [ENABLE_POSTGRESQL=$enableval]
)
AC_ARG_ENABLE(
  pcre,
  AC_HELP_STRING([--disable-pcre],[disable PCRE support]),
  [ENABLE_PCRE=$enableval]
)
AC_ARG_ENABLE(
  par2,
  AC_HELP_STRING([--disable-par2],[disable PAR2 support]),
  [ENABLE_PAR2=$enableval]
)
AC_ARG_ENABLE(
  isofs,
  AC_HELP_STRING([--disable-isofs],[disable ISO file system support]),
  [ENABLE_ISOFS=$enableval]
)
AC_ARG_ENABLE(
  xfs,
  AC_HELP_STRING([--disable-xfs],[disable XFS file system support]),
  [ENABLE_XFS=$enableval]
)
AC_ARG_ENABLE(
  burn,
  AC_HELP_STRING([--disable-burn],[disable burn support]),
  [ENABLE_BURN=$enableval]
)
AC_ARG_ENABLE(
  mount,
  AC_HELP_STRING([--disable-mount],[disable mount support]),
  [ENABLE_MOUNT=$enableval]
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
  extern-compile,
  AC_HELP_STRING([--disable-extern-compile],[disable compilation of external software]),
  [ENABLE_EXTERN_COMPILE=$enableval]
)

# check dependencies of enabled packages
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

if test -z "$BUILD_CC"; then
  AC_PATH_PROGS(BUILD_CC,$build-gcc $build-cc gcc cc)
fi
if test -z "$BUILD_LD"; then
  AC_PATH_PROGS(BUILD_LD,$build-ld $build-ld ld gcc)
fi

if test -z "$CPP"; then
  AC_PATH_PROGS(CPP,$host-cpp cpp)
fi
if test -z "$CPP"; then
  AC_MSG_ERROR([Cannot find C preprocessor in PATH. Please check your PATH environment variable.])
fi
if test -z "$CC"; then
  AC_PATH_PROGS(CC,$host-gcc $host-cc gcc cc)
fi
if test -z "$CC"; then
  AC_MSG_ERROR([Cannot find C compiler in PATH. Please check your PATH environment variable.])
fi
if test -z "$CXX"; then
  AC_PATH_PROGS(CXX,$host-g++ $host-c++ g++ c++)
fi
if test -z "$CXX"; then
  AC_MSG_ERROR([Cannot find C++ compiler in PATH. Please check your PATH environment variable.])
fi
if test -z "$LD"; then
  AC_PATH_PROGS(LD,$host-ld $host-gcc ld gcc)
fi
if test -z "$AR"; then
  AC_PATH_PROGS(AR,$host-ar ar)
fi
if test -z "$STRIP"; then
  AC_PATH_PROGS(STRIP,$host-strip strip)
fi
if test -z "$OBJDUMP"; then
  AC_PATH_PROGS(OBJDUMP,$host-objdump objdump)
fi
if test -z "$OBJCOPY"; then
  AC_PATH_PROGS(OBJCOPY,$host-objcopy objcopy)
fi
if test -z "$RANLIB"; then
  AC_PATH_PROGS(RANLIB,$host-ranlib ranlib)
fi
if test -z "$WINDRES"; then
  AC_PATH_PROGS(WINDRES,$host-windres windres)
fi

AC_PATH_PROGS(CMAKE,cmake)
if test -z "$CMAKE"; then
  AC_MSG_ERROR([Cannot find a 'cmake' in PATH. Please check your PATH environment variable.])
fi
AC_PATH_PROGS(MAKE,make)
if test -z "$MAKE"; then
  AC_MSG_ERROR([Cannot find a 'make' in PATH. Please check your PATH environment variable.])
fi
if test -z "`$MAKE --version 2>/dev/null|grep GNU 2>/dev/null`"; then
  AC_MSG_ERROR([$MAKE is not a GNU make. Please install a GNU make.])
fi
AC_PATH_PROGS(PATCH,patch)
if test -z "$PATCH"; then
  AC_MSG_ERROR([Cannot find a 'patch' in PATH. Please check your PATH environment variable.])
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

AC_PATH_PROGS(M4,m4)
if test -z "$M4"; then
  AC_MSG_ERROR([Cannot find a 'm4' in PATH. Please check your PATH environment variable.])
fi

AC_PATH_PROGS(PKGCONFIG,pkg-config)
if test -z "$PKGCONFIG"; then
  AC_MSG_ERROR([Cannot find a 'pkg-config' in PATH. Please check your PATH environment variable.])
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

AC_PROG_INSTALL
if test -z "$INSTALL"; then
  AC_MSG_ERROR([Cannot find a 'install' in PATH. Please check your PATH environment variable.])
fi

AC_PATH_PROGS(SQLITE3,sqlite3)
if test -z "$SQLITE3"; then
  AC_MSG_ERROR([Cannot find a 'sqlite3' in PATH. Please check your PATH environment variable.])
fi
if test $ENABLE_MARIADB = "yes"; then
  AC_PATH_PROGS(MYSQL,mysql)
  if test -z "$MYSQL"; then
    AC_MSG_WARN([No 'mysql' command found in path - tests with MariaDB cannot be executed.])
  fi
fi
if test $ENABLE_POSTGRESQL = "yes"; then
  AC_PATH_PROGS(PSQL,psql)
  if test -z "$PSQL"; then
    AC_MSG_WARN([No 'psql' command found in path - tests with PostgreSQL cannot be executed.])
  fi
  AC_PATH_PROGS(CREATEDB,createdb)
  if test -z "$CREATEDB"; then
    AC_MSG_WARN([No 'createdb' command found in path - tests with PostgreSQL cannot be executed.])
  fi
fi

AC_PATH_PROGS(SVNVERSION,svnversion)
AC_PATH_PROGS(GIT,git)

AC_PATH_PROGS(MD5SUM,md5sum)

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

  if test -n "$JAVA"; then
    AC_JAVAC_VERSION(JAVA_VERSION)
    case "$JAVA_VERSION" in
      1.6*)
        ;;
      1.7*)
        ;;
      1.8*)
        ;;
      11.0*)
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

  AC_PATH_PROGS(LAUNCH4JC,launch4jc,,$LAUNCH4J_DIR $PWD/launch4j)
  if test -d jre_windows; then
    JRE_WINDOWS_DIR=$PWD/jre_windows
  fi
fi

AC_PATH_PROGS(CERTTOOL,certtool)
AC_PATH_PROGS(OPENSSL,openssl)
AC_PATH_PROGS(KEYTOOL,keytool)

AC_PATH_PROGS(LCOV,lcov)
AC_PATH_PROGS(GENHTML,genhtml)

AC_PATH_PROGS(UNOCONV,unoconv)
if test -z "$UNOCONV"; then
  AC_MSG_WARN([No 'unoconv' tool found. Cannot create PDF manual. Please check PATH or create PDF manual manually in OO.])
fi
AC_PATH_PROGS(CONVERT,convert)
if test -z "$CONVERT"; then
  AC_MSG_WARN([No 'convert' tool found. Cannot create distribution and PDF manual. Please check PATH.])
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

AC_PATH_PROGS(MKFS_EXT2, mkfs.ext2, ,/sbin)
AC_PATH_PROGS(MKFS_EXT3, mkfs.ext3, ,/sbin)
AC_PATH_PROGS(MKFS_EXT4, mkfs.ext4, ,/sbin)
AC_PATH_PROGS(MKFS_MSDOS,mkfs.msdos,,/sbin)
AC_PATH_PROGS(MKFS_EXFAT,mkfs.exfat,,/sbin)
AC_PATH_PROGS(MKFS_XFS,  mkfs.xfs,  ,/sbin)

AC_PATH_PROGS(SOCAT,socat)

AC_PATH_PROGS(DOCKER,docker)
# required for tests
AC_PATH_PROGS(KILLALL,killall)
if test -z "$KILLALL"; then
  AC_MSG_ERROR([Cannot find a 'killall' in PATH. Please check your PATH environment variable.])
fi
AC_PATH_PROGS(WINE,wine)
AC_PATH_PROGS(WINEPATH,winepath)
AC_PROVIDE(AC_PROG_WINE)
AC_PROVIDE(AC_PROG_WINEPATH)
if test -n "$WINE" -a -n "$WINEPATH"; then
  AC_WINE_C_DRIVE(WINE_C_DRIVE)
  AC_PROG_ISCC(ISCC)
  if test -z "$ISCC"; then
    AC_MSG_WARN([No 'ISCC.exe' tool found. Cannot create Win32 setup program.])
  fi
else
  AC_MSG_WARN([No 'wine' tool found. Cannot create Win32 setup program.])
fi

# compiler/link flags/paths

# Note: do not use AC_PROG_CC_STDC, because it adds the option to the CC command
AC_CC_OPTION(-std=gnu99,                      CC_FLAGS)

# Note: disable address randomization for debug/analysis builds
AC_CC_OPTION(-fno-pie,                        CC_FLAGS_DEBUG CC_FLAGS_GCOV CC_FLAGS_GPROF CC_FLAGS_VALGRIND CXX_FLAGS_DEBUG)

AC_CC_OPTION(-Wall,                           CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_CC_OPTION(-Wpointer-arith,                 CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_CC_OPTION(-Wunused-label,                  CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_CC_OPTION(-Wunused-parameter,              CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_CC_OPTION(-Wunused-variable,               CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_CC_OPTION(-Wmissing-prototypes,            CC_WARN_FLAGS)
AC_CC_OPTION(-Wmissing-declarations,          CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_CC_OPTION(-Wstrict-aliasing,               CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_CC_OPTION(-Wextra,                         CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_CC_OPTION(-Wfloat-equal,                   CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_CC_OPTION(-Wbad-function-cast,             CC_WARN_FLAGS)
AC_CC_OPTION(-Wsign-compare,                  CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_CC_OPTION(-Wstrict-prototypes,             CC_WARN_FLAGS)
AC_CC_OPTION(-Wno-declaration-after-statement,CC_WARN_FLAGS)
AC_CC_OPTION(-Wno-unknown-pragmas,            CC_WARN_FLAGS CXX_WARN_FLAGS)
AC_CC_OPTION(-Wvla,                           CC_WARN_FLAGS CXX_WARN_FLAGS)
case $host_os in
  linux-*)
    ;;
  sunos | solaris*)
    ;;
  darwin*)
    ;;
  windows*|mingw*)
    # Note: __mingw_printf, __mingw_fprintf are not recognized as format functions by mingw gcc version 7.3?
    AC_CC_OPTION(-Wno-format,CC_WARN_FLAGS CXX_WARN_FLAGS)
    ;;
esac

AC_CXX_OPTION(-no-pie,LD_FLAGS_DEBUG LD_FLAGS_GCOV LD_FLAGS_VALGRIND)

CC_FLAGS_OPTIMIZE="$CC_FLAGS_OPTIMIZE"
CC_FLAGS_OPTIMIZE_RELEASE="$CC_FLAGS_OPTIMIZE_RELEASE -O2"
dnl Check: bug in gcc with -fschedule-insns2? If -O2 is used and -fno-schedule-insns2 is
dnl        not given, the program either crashes or parsing a string with %y seems to fail.
AC_CC_OPTION(-fno-schedule-insns2,CC_FLAGS_OPTIMIZE_RELEASE)
CC_FLAGS_OPTIMIZE_DEBUG="$CC_FLAGS_OPTIMIZE_DEBUG -O0"
CC_FLAGS_OPTIMIZE_VALGRIND="$CC_FLAGS_OPTIMIZE_VALGRIND -O2"

CC_DEFINES="$CC_DEFINES"
CC_DEFINES_RELEASE="$CC_DEFINES_RELEASE NDEBUG"
CC_DEFINES_DEBUG="$CC_DEFINES_DEBUG"
CC_DEFINES_VALGRIND="$CC_DEFINES_VALGRIND"

case $host_os in
  linux-*)
    ;;
  sunos | solaris*)
    ;;
  darwin*)
    ;;
  windows*|mingw*)
    CC_DEFINES="$CC_DEFINES _WIN32_WINNT=0x0600 _POSIX_C_SOURCE"
    ;;
esac

CC_INCLUDES="$CC_INCLUDES"
case $host_os in
  linux-*)
    ;;
  sunos | solaris*)
    ;;
  darwin*)
    ;;
  windows*|mingw*)
    ;;
esac

LD_FLAGS=""
if test ! $ENABLE_LINK_DYNAMIC = "yes"; then
  LD_FLAGS="-static-libgcc"
fi
LIBRARY_PATHS="$LIBRARY_PATHS /usr/local/lib"
case $host_os in
  linux-*)
    if test "$ENABLE_DEBUG" = "yes"; then
      # required for printing stacktraces
      AC_CC_OPTION(-rdynamic,CC_FLAGS CXX_FLAGS LD_FLAGS)
    fi
    ;;
  sunos | solaris*)
    if test "$ENABLE_DEBUG" = "yes"; then
      # required for printing stacktraces
      AC_CC_OPTION(-rdynamic,CC_FLAGS CXX_FLAGS LD_FLAGS)
    fi
    ;;
  darwin*)
    if test "$ENABLE_DEBUG" = "yes"; then
      # required for printing stacktraces
      AC_CC_OPTION(-rdynamic,CC_FLAGS CXX_FLAGS LD_FLAGS)
    fi
    ;;
  windows*|mingw*)
    ;;
esac

# sudo tests (Note: keep "," immediately after "]"!)
AC_PROG_SUDO_COMMAND([mknod \* c \*],                                       [mknod \* c \*],                                       ,[HAVE_SUDO_COMMAND_MKNOD_C=0;      HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([mknod \* b \*],                                       [mknod \* b \*],                                       ,[HAVE_SUDO_COMMAND_MKNOD_B=0;      HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([chattr \* data/\*],                                   [chattr \* data/\*],                                   ,[HAVE_SUDO_COMMAND_CHATTR=0;       HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([chattr \* configure-\*],                              [chattr \* configure-\*],                              ,[HAVE_SUDO_COMMAND_CHATTR=0;       HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([mount /dev/loop[[01]] intermediate/dev[[01]]],        [mount /dev/loop[[01]] intermediate/dev[[01]]],        ,[HAVE_SUDO_COMMAND_MOUNT_UMOUNT=0; HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([umount intermediate/dev[[01]]],                       [umount intermediate/dev[[01]]],                       ,[HAVE_SUDO_COMMAND_MOUNT_UMOUNT=0; HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([losetup -d /dev/loop[[01]]],                          [losetup -d /dev/loop[[01]]],                          ,[HAVE_SUDO_COMMAND_LOSETUP=0;      HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([losetup /dev/loop[[01]] intermediate/dev[[01]].dat],  [losetup /dev/loop[[01]] intermediate/dev[[01]]\.dat], ,[HAVE_SUDO_COMMAND_LOSETUP=0;      HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([losetup /dev/loop[[01]] intermediate/dev0[[ab]]\.dat],[losetup /dev/loop[[01]] intermediate/dev0[[ab]]\.dat],,[HAVE_SUDO_COMMAND_LOSETUP=0;      HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([chmod o\+rw /dev/loop[[01]]],                         [chmod o\+rw /dev/loop[[01]]],                         ,[HAVE_SUDO_COMMAND_CHMOD=0;        HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([cp -r -p data/\* intermediate/dev[[01]]],             [cp -r -p data/\* intermediate/dev[[01]]],             ,[HAVE_SUDO_COMMAND_CP=0;           HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([diff -r intermediate/dev0 intermediate/dev1],         [diff -r intermediate/dev0 intermediate/dev1],         ,[HAVE_SUDO_COMMAND_DIFF=0;         HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([chattr -i \* data/\*],                                [chattr \* data/\*],                                   ,[HAVE_SUDO_COMMAND_CHATTR_I=0;     HAVE_SUDO_COMMANDS=0])
AC_PROG_SUDO_COMMAND([chattr -a \* data/\*],                                [chattr \* data/\*],                                   ,[HAVE_SUDO_COMMAND_CHATTR_A=0;     HAVE_SUDO_COMMANDS=0])

# test user
AC_CACHE_CHECK(
  [if user 'test' exists],
  ac_cv_user_test,
  [
    if AC_TRY_COMMAND(id test 1>/dev/null); then
      ac_result=yes
    else
      ac_result=no
    fi
    AS_IF([test "$ac_result" != no],[HAVE_USER_TEST=1],:,[HAVE_USER_TEST=0])
    AS_VAR_SET(ac_cv_user_test,$ac_result)
  ]
)dnl

dnl -------- mandatory headers/functions/definitions/libraries ---------

case $host_os in
  linux-*)
    AC_SEARCH_LIBRARIES(round,m,[LIBRARIES="$LIBRARIES m"],AC_ERROR([m library not found]))
    AC_SEARCH_LIBRARIES(dlopen,dl,[LIBRARIES="$LIBRARIES dl"])
    AC_SEARCH_LIBRARIES(clock_gettime,rt,[LIBRARIES="$LIBRARIES rt"],AC_ERROR([rt library not found]))
    AC_SEARCH_LIBRARIES(pthread_create,pthread,[LIBRARIES="$LIBRARIES pthread"],AC_ERROR([pthread library not found]))
    AC_SEARCH_LIBRARIES(ns_initparse,resolv,[LIBRARIES="$LIBRARIES resolv"])
    ;;
  sunos | solaris*)
    AC_SEARCH_LIBRARIES(round,m,[LIBRARIES="$LIBRARIES m"],AC_ERROR([m library not found]))
    AC_SEARCH_LIBRARIES(dlopen,dl,[LIBRARIES="$LIBRARIES dl"])
    AC_SEARCH_LIBRARIES(clock_gettime,rt,[LIBRARIES="$LIBRARIES rt"],AC_ERROR([rt library not found]))
    AC_SEARCH_LIBRARIES(pthread_create,pthread,[LIBRARIES="$LIBRARIES pthread"],AC_ERROR([pthread library not found]))
    AC_SEARCH_LIBRARIES(ns_initparse,resolv,[LIBRARIES="$LIBRARIES resolv"])
    ;;
  darwin*)
    AC_SEARCH_LIBRARIES(round,m,[LIBRARIES="$LIBRARIES m"],AC_ERROR([m library not found]))
    AC_SEARCH_LIBRARIES(dlopen,dl,[LIBRARIES="$LIBRARIES dl"])
    AC_SEARCH_LIBRARIES(clock_gettime,rt,[LIBRARIES="$LIBRARIES rt"],AC_ERROR([rt library not found]))
    AC_SEARCH_LIBRARIES(pthread_create,pthread,[LIBRARIES="$LIBRARIES pthread"],AC_ERROR([pthread library not found]))
    AC_SEARCH_LIBRARIES(ns_initparse,resolv,[LIBRARIES="$LIBRARIES resolv"])
    ;;
  windows*|mingw*)
    AC_SEARCH_STATIC_LIBRARIES(pthread_create,pthreadGC2,[STATIC_LIBRARIES="$STATIC_LIBRARIES pthreadGC2"],,pthread.h)
    AC_SEARCH_STATIC_LIBRARIES(WSAStartup,wsock32,[STATIC_LIBRARIES="$STATIC_LIBRARIES wsock32"],,winsock2.h)
    AC_SEARCH_STATIC_LIBRARIES(getaddrinfo,ws2_32,[STATIC_LIBRARIES="$STATIC_LIBRARIES ws2_32"],,ws2tcpip.h)
    AC_SEARCH_STATIC_LIBRARIES(UuidCreate,rpcrt4,[STATIC_LIBRARIES="$STATIC_LIBRARIES rpcrt4"],,windows.h rpcdce.h)
    AC_SEARCH_STATIC_LIBRARIES(SHGetFolderPath,shell32,[STATIC_LIBRARIES="$STATIC_LIBRARIES shell32"],,shlobj.h)
    AC_SEARCH_STATIC_LIBRARIES(CoTaskMemFree,ole32,[STATIC_LIBRARIES="$STATIC_LIBRARIES ole32"],,combaseapi.h)
    AC_SEARCH_STATIC_LIBRARIES(FOLDERID_LocalAppData,uuid,[STATIC_LIBRARIES="$STATIC_LIBRARIES uuid"])
    AC_SEARCH_STATIC_LIBRARIES(PathRemoveFileSpec,shlwapi,[STATIC_LIBRARIES="$STATIC_LIBRARIES shlwapi"],,shlwapi.h)
    AC_SEARCH_STATIC_LIBRARIES(GetUnicastIpAddressTable,iphlpapi ssp,[STATIC_LIBRARIES="$STATIC_LIBRARIES iphlpapi ssp"],,winsock2.h netioapi.h)
    AC_SEARCH_STATIC_LIBRARIES(NCryptOpenKey,ncrypt,[STATIC_LIBRARIES="$STATIC_LIBRARIES ncrypt"],,windows.h ncrypt.h)
    AC_SEARCH_LIBRARIES(ns_initparse,resolv,[LIBRARIES="$LIBRARIES resolv"])
    ;;
esac

dnl ------------------- optional external software ---------------------

# create extern directory
cwd=`pwd`
mkdir extern 2>/dev/null
if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
  (cd extern; rm -rf include; mkdir include 2>/dev/null)
  (cd extern; rm -rf lib; mkdir lib 2>/dev/null)
fi
(cd extern; rm -f lib64; ln -s lib lib64 2>/dev/null)

# add extern include/library+local include search directory
CC_INCLUDES="$CC_INCLUDES $cwd/extern/include $cwd/bar"
LIBRARY_PATHS="$cwd/extern/lib $cwd/extern/lib/mariadb $LIBRARY_PATHS"

# set toolchain settings for compile extern (Note: do not move ) at the end!)
AC_SET_TOOLCHAIN_SETTINGS($CPP_FLAGS,
                          $CC_FLAGS,
                          $CXX_FLAGS,
                          $CC_DEFINES,
                          $CC_INCLUDES,
                          $LD_FLAGS,
                          $LIBRARY_PATHS,
                          $STATIC_LIBRARIES,
                          $LIBRARIES)

# compile/use icu
if test -d icu -o -d "`readlink icu`"; then
  if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
    cwd=`pwd`

    AC_MSG_COMPILE([icu])
    result=1
    if test $result -eq 1; then
      case $host_os in
        linux-*)
          ;;
        sunos | solaris*)
          ;;
        darwin*)
          ;;
        windows*|mingw*)
          # patch to disable usage of lib-command when RANLIB is set
          ;;
      esac
      if test $? -ne 0; then
        AC_MSG_RESULT([fail (patch)])
        AC_MSG_OUTPUT_LOG($tmpFile)
        result=0
      fi
    fi
    if test $result -eq 1; then
      install -d icu/build/$build
      (cd icu/build/$build; CFLAGS="-fPIC" CXXFLAGS="-fPIC" LDFLAGS="" ../../source/configure \
        --build=$build \
        --prefix=$cwd/extern \
        --disable-shared \
        --enable-static \
        --disable-tests \
        --disable-samples \
        --with-package-data=static \
      ) 1>$tmpFile 2>&1
      if test $? -ne 0; then
        AC_MSG_RESULT([fail (configure host)])
        AC_MSG_OUTPUT_LOG($tmpFile)
        result=0
      fi
    fi
    if test $result -eq 1; then
      (cd icu/build/$build; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
      if test $? -ne 0; then
        AC_MSG_RESULT([fail (make host)])
        AC_MSG_OUTPUT_LOG($tmpFile)
        result=0
      fi
    fi
    if test "$host" != "$build"; then
      install -d icu/build/$host
      if test $result -eq 1; then
        (cd icu/build/$host; CFLAGS="-fPIC" CXXFLAGS="-fPIC" LDFLAGS="" ../../source/configure \
          --host=$host \
          --build=$build \
          --with-cross-build=$cwd/icu/build/$build \
          --prefix=$cwd/extern \
          --disable-shared \
          --enable-static \
          --disable-tests \
          --disable-samples \
          --with-package-data=static \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure target)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd icu/build/$host; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make target)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
    fi
    if test $result -eq 1; then
      (cd icu/build/$host; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
      if test $? -ne 0; then
        result=0
      fi
      case $host_os in
        linux-*)
          ;;
        sunos | solaris*)
          ;;
        darwin*)
          ;;
        windows*|mingw*)
          # Note: bug in library names of older ICU versions?
          #       https://unicode-org.atlassian.net/browse/ICU-13138
          (cd extern/lib; ln -s libsicuuc.a libicuuc.a) 1>/dev/null 2>/dev/null
          (cd extern/lib; ln -s libsicuin.a libicui18n.a) 1>/dev/null 2>/dev/null
          (cd extern/lib; ln -s sicudt.a libicudata.a) 1>/dev/null 2>/dev/null
          (cd extern/lib; ln -s libsicuio.a libicuio.a) 1>/dev/null 2>/dev/null
          (cd extern/lib; ln -s libsicutu.a libicutu.a) 1>/dev/null 2>/dev/null
          (cd extern/lib; ln -s libsiculx.a libiculx.a) 1>/dev/null 2>/dev/null
          ;;
      esac
      if test $result -ne 1; then
        AC_MSG_RESULT([fail (install)])
        AC_MSG_OUTPUT_LOG($tmpFile)
      fi
    fi
    if test $result -eq 1; then
      AC_MSG_RESULT([done])
    fi
  else
    result=1
  fi

  if test $result -eq 1; then
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      LIBRARIES="$LIBRARIES icuuc icui18n icudata"
    else
      STATIC_LIBRARIES="$STATIC_LIBRARIES icuuc icui18n icudata"
    fi
    HAVE_ICU=1
    AC_DEFINE(HAVE_ICU,1,[icu installed (local)])
  fi
else
  dnl search for installed icu library

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
fi

# compile/use zlib
if test -d zlib -o -d "`readlink zlib`"; then
  if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
    cwd=`pwd`

    AC_MSG_COMPILE([zlib])
    result=1
    if test $result -eq 1; then
      (cd zlib; CC="$CC" CPPFLAGS="" CFLAGS="`if test "$ENABLE_DEBUG" = "yes"; then echo -- -O0 -g; fi`" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="$LIBS" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
        --prefix=$cwd/extern \
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
      (cd zlib; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
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
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      LIBRARIES="$LIBRARIES z"
    else
      STATIC_LIBRARIES="$STATIC_LIBRARIES z"
    fi
    HAVE_Z=1
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
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

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
          (cd bzip2; make clean; make -j$CPU_CORES CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="$LIBS" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" libbz2.a) 1>$tmpFile 2>&1
          if test $? -ne 0; then
            AC_MSG_RESULT([fail (make)])
            AC_MSG_OUTPUT_LOG($tmpFile)
            result=0
          fi
        fi
      fi
      if test $result -eq 1; then
        # Note: do not call 'install', it does build additional tools
        (cd bzip2; \
         install -d $cwd/extern/include; \
         install -m 644 bzlib.h $cwd/extern/include; \
         install -d $cwd/extern/lib; \
         install -m 644 libbz2.a $cwd/extern/lib; \
        ) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES bz2"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES bz2"
      fi
      HAVE_BZ2=1
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

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_BZ2"; then
    if test ! \( -d bzip2 -o -d "`readlink bzip2`" \); then
      AC_MSG_WARN([bzip2 not compiled: bzip2 package missing])
    fi
  fi
fi

# compile/use lzma
if test $ENABLE_LZMA = "yes"; then
  if test -d xz -o -d "`readlink xz`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([xz utils])
      result=1
      if test $result -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            #   diff -u xz-5.0.4.org/m4/libtool.m4 xz-5.0.4/m4/libtool.m4 xz-5.0.4-libtool.patch
#TODO: windows cross-build
#            if test -d extern/xz-5.0.4; then
#              (cd extern/xz-5.0.4; $PATCH --batch -N -p1 < $SOURCE_DIR/misc/xz-5.0.4-libtool.patch) 1>$tmpFile 2>&1
#            else
#              AC_MSG_WARN([xz-5.0.4 not available - skipped libtool patch!])
#            fi
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd xz; ./configure \
          --host=$host \
          --build=$build \
          --prefix=$cwd/extern \
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES lzma"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES lzma"
      fi
      HAVE_LZMA=1
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

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_LZMA"; then
    if test ! \( -d xz -o -d "`readlink xz`" \); then
      AC_MSG_WARN([lzma not compiled: xz package missing])
    fi
  fi
fi

# compile/use lzo
if test $ENABLE_LZO = "yes"; then
  if test -d lzo -o -d "`readlink lzo`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([lzo])
      result=1
      if test $result -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            # patch to fix missing --tag=CC for in libtool call
            #   diff -u extern/lzo-2.10.org/Makefile.in extern/lzo-2.10/Makefile.in  > misc/lzo-2.10-libtool.patch
#            (cd lzo; $PATCH --batch -N -p1 < $SOURCE_DIR/misc/lzo-2.10-libtool.patch) 1>$tmpFile 2>&1
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        install -d lzo
        (cd lzo; ./configure \
          --host=$host \
          --build=$build \
          --prefix=$cwd/extern \
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
        (cd lzo; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES lzo2"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES lzo2"
      fi
      HAVE_LZO=1
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

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_LZO"; then
    if test ! \( -d lzo -o -d "`readlink lzo`" \); then
      AC_MSG_WARN([lzo not compiled: lzo package missing])
    fi
  fi
fi

# compile/use lz4
if test $ENABLE_LZ4 = "yes"; then
  if test -d lz4 -o -d "`readlink lz4`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([lz4])
      result=1
      if test $result -eq 1; then
        (cd lz4; make clean; make -j$CPU_CORES CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="$LIBS" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" lib) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        # Note: do not call 'install', it does build additional tools
        # (cd lz4; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
        (cd lz4; \
         install -d $cwd/extern/include; \
         install -m 644 lib/lz4.h lib/lz4frame.h lib/lz4hc.h lib/xxhash.h $cwd/extern/include; \
         install -d $cwd/extern/lib; \
         install -m 644 lib/liblz4.a $cwd/extern/lib; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
        rm -f $cwd/extern/lib/liblz4*.so*
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES lz4"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES lz4"
      fi
      HAVE_LZ4=1
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

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_LZ4"; then
    if test ! \( -d lz4 -o -d "`readlink lz4`" \); then
      AC_MSG_WARN([lz4 not compiled: lz4 package missing])
    fi
  fi
fi

# compile/use zstd
if test $ENABLE_ZSTD = "yes"; then
  if test -d zstd -o -d "`readlink zstd`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([zstd])
      result=1
      if test $result -eq 1; then
        if test "$build" == "$host"; then
          (cd zstd; make clean; make -j$CPU_CORES -C lib) 1>$tmpFile 2>&1
          if test $? -ne 0; then
            AC_MSG_RESULT([fail (make)])
            AC_MSG_OUTPUT_LOG($tmpFile)
            result=0
          fi
        else
          # Note: do not set LDFLAGS
          (cd zstd; make clean; make -j$CPU_CORES -C lib CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LIBS="$LIBS" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES") 1>$tmpFile 2>&1
          if test $? -ne 0; then
            AC_MSG_RESULT([fail (make)])
            AC_MSG_OUTPUT_LOG($tmpFile)
            result=0
          fi
        fi
      fi
      if test $result -eq 1; then
        (cd zstd; make -C lib install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
        rm -f $cwd/extern/lib/libzstd*.so*
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      CC_DEFINES="$CC_DEFINES ZSTD_STATIC_LINKING_ONLY"
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES zstd"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES zstd"
      fi
      HAVE_ZSTD=1
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

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_ZSTD"; then
    if test ! \( -d zstd -o -d "`readlink zstd`" \); then
      AC_MSG_WARN([zstd not compiled: zstd package missing])
    fi
  fi
fi

# use xdelta3
if test $ENABLE_XDELTA3 = "yes"; then
  if test -d xdelta3 -o -d "`readlink xdelta3`"; then
    dnl compile xdelta sources

    cwd=`pwd`

    # Note: no cross-compile; copy source only
    AC_MSG_CREATE([xdelta3])
    result=1
    if test $result -ne 1; then
      AC_MSG_RESULT([fail])
    fi
    if test $result -eq 1; then
      (cd xdelta3; \
       install -d $cwd/extern/sources; \
       install -d $cwd/extern/include; \
       install -m 644 xdelta3.c $cwd/extern/sources; \
       install -m 644 -t $cwd/extern/include \
         xdelta3.h \
         xdelta3-internal.h \
         xdelta3-list.h \
         xdelta3-hash.h \
         xdelta3-cfgs.h \
         xdelta3-decode.h; \
      ) 1>$tmpFile 2>&1
      (cd $cwd/extern/include; ln -sf ../sources/xdelta3.c)
      AC_MSG_RESULT([done])
    fi

#    CC_INCLUDES="$CC_INCLUDES $cwd/xdelta3"
    HAVE_XDELTA3=1
    AC_DEFINE(HAVE_XDELTA3,1,[xdelta3 installed])
  fi
fi

# compile/use gcrypt
if test $ENABLE_GCRYPT = "yes"; then
  if test \( -d libgpg-error -o -d "`readlink libgpg-error`" \) -a \( -d libgcrypt -o -d "`readlink libgcrypt`" \); then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([libgpg-error])
      result1=1
      if test $result1 -eq 1; then
        (cd libgpg-error; ./configure \
          --host=$host \
          --build=$build \
          --prefix=$cwd/extern \
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
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            #   diff -u libgcrypt-1.5.0.org/m4/libtool.m4 libgcrypt-1.5.0/m4/libtool.m4 >  libgcrypt-1.5.0-libtool.patch
            #   diff -u libgcrypt-1.5.0.org/aclocal.m4 libgcrypt-1.5.0/aclocal.m4       >> libgcrypt-1.5.0-libtool.patch
            #   diff -u libgcrypt-1.5.0.org/configure libgcrypt-1.5.0/configure         >> libgcrypt-1.5.0-libtool.patch
#TODO: windows cross-build
#            if test -d extern/libgcrypt-1.5.0; then
#              (cd extern/libgcrypt-1.5.0; $PATCH --batch -N -p1 < $SOURCE_DIR/misc/libgcrypt-1.5.0-libtool.patch) 1>$tmpFile 2>&1
#            else
#              AC_MSG_WARN([libgcrypt-1.5.0 not available - skipped libtool patch!])
#            fi
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        case $host_cpu in
          *86*)
            (cd libgcrypt; LIBS="-L$cwd/extern/lib -lgpg-error" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
              --with-libgpg-error-prefix=$cwd/extern \
              --enable-maintainer-mode \
              --disable-shared \
              --enable-static \
              `if test "$ENABLE_DEBUG" = "yes"; then echo --disable-optimization; fi` \
            ) 1>$tmpFile 2>&1
            ;;
          arm*)
#TODO: cipher
            (cd libgcrypt; LIBS="-L$cwd/extern/lib -lgpg-error" ./configure \
              --host=$build \
              --prefix=$cwd/extern \
              --with-libgpg-error-prefix=$cwd/extern \
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
        (cd libgcrypt; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
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
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            (cd extern/lib; ln -s libgcrypt.lib libgcrypt.a) 1>/dev/null 2>/dev/null
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
      CC_DEFINES="$CC_DEFINES GCRYPT_NO_DEPRECATED"
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES gcrypt gpg-error"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES gcrypt gpg-error"
      fi
      HAVE_GCRYPT=1
      HAVE_GPG_ERROR=1
      AC_DEFINE(HAVE_GCRYPT,1,[gcrypt installed (local)])
    fi
  else
    dnl search for installed libgpg-error, libgcrypt libraries

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

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_GCRYPT"; then
    if test ! \( -d libgpg-error -o -d "`readlink libgpg-error`" \); then
      AC_MSG_WARN([libgcrypt not compiled: libgpg-error package missing])
    fi
    if test ! \( -d libgcrypt -o -d "`readlink libgcrypt`" \); then
      AC_MSG_WARN([libgcrypt not compiled: libgcrypt package missing])
    fi
  fi
fi

# compile/use openssl
if test $ENABLE_SSH = "yes"; then
  if test -d openssl -o -d "`readlink openssl`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([openssl])
      result=1
      if test $result -eq 1; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            if test "$host_cpu" = "x86_64"; then
              (cd openssl; ./Configure \
                --prefix=$cwd/extern \
                no-shared \
                no-tests \
                linux-x86_64 \
                -Wa,--noexecstack \
              ) 1>$tmpFile 2>&1
              if test $? -ne 0; then
                AC_MSG_RESULT([fail (configure)])
                AC_MSG_OUTPUT_LOG($tmpFile)
                result=0
              fi
            else
              (cd openssl; ./Configure \
                --prefix=$cwd/extern \
                linux-generic32 \
                no-shared \
                no-tests \
                -Wa,--noexecstack \
              ) 1>$tmpFile 2>&1
              if test $? -ne 0; then
                AC_MSG_RESULT([fail (configure)])
                AC_MSG_OUTPUT_LOG($tmpFile)
                result=0
              fi
            fi
            ;;
          windows*|mingw*)
            if test "$host_cpu" = "x86_64"; then
              (cd openssl; CFLAGS="" LDFLAGS="" ./Configure \
                --prefix=$cwd/extern \
                --cross-compile-prefix=x86_64-w64-mingw32- \
                no-shared \
                no-tests \
                mingw64 \
              ) 1>$tmpFile 2>&1
            else
              (cd openssl; CFLAGS="" LDFLAGS="" ./Configure \
                --prefix=$cwd/extern \
                --cross-compile-prefix=x86_64-w64-mingw32- \
                no-shared \
                no-tests \
                mingw \
              ) 1>$tmpFile 2>&1
            fi
            if test $? -ne 0; then
              AC_MSG_RESULT([fail (configure)])
              AC_MSG_OUTPUT_LOG($tmpFile)
              result=0
            fi
            ;;
        esac
      fi
      if test $result -eq 1; then
        (cd openssl; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            LIBRARIES="$LIBRARIES ssl crypto"
            ;;
          windows*|mingw*)
            LIBRARIES="$LIBRARIES ssl crypto crypt32"
            ;;
        esac
      else
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            STATIC_LIBRARIES="$STATIC_LIBRARIES ssl crypto"
            ;;
          windows*|mingw*)
            STATIC_LIBRARIES="$STATIC_LIBRARIES ssl crypto crypt32"
            ;;
        esac
      fi
      HAVE_SSL=1
      AC_DEFINE(HAVE_SSL,1,[ssl installed (local)])
    fi
  fi

  if test -d libssh2 -o -d "`readlink libssh2`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      AC_MSG_COMPILE([libssh2])
      result=1
      if test $result -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            #   diff -u libssh2-1.4.2.org/m4/libtool.m4 libssh2-1.4.2/m4/libtool.m4 >  libssh2-1.4.2-libtool.patch
            #   diff -u libssh2-1.4.2.org/aclocal.m4 libssh2-1.4.2/aclocal.m4       >> libssh2-1.4.2-libtool.patch
            #   diff -u libssh2-1.4.2.org/configure libssh2-1.4.2/configure         >> libssh2-1.4.2-libtool.patch
#TODO: windows cross-build
#            if test -d extern/libssh2-1.4.2; then
#              (cd extern/libssh2-1.4.2; $PATCH --batch -N -p1 < $SOURCE_DIR/misc/libssh2-1.4.2-libtool.patch) 1>$tmpFile 2>&1
#            else
#              AC_MSG_WARN([libssh2-1.4.2 not available - skipped libtool patch!])
#            fi
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd libssh2; PKG_CONFIG_PATH=$cwd/extern/lib/pkgconfig ./configure \
          --host=$host \
          --build=$build \
          --prefix=$cwd/extern \
          --with-libssl-prefix=$cwd/extern \
          --with-libgcrypt-prefix=$cwd/extern \
          --disable-shared --enable-static \
          --disable-examples-build \
          --enable-debug \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd libssh2; make clean; make -j$CPU_CORES -C src) 1>$tmpFile 2>&1
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
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            (cd extern/lib; ln -s libssh2.lib libssh2.a) 1>/dev/null 2>/dev/null
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES ssh2"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES ssh2"
      fi
      HAVE_SSH2=1
      AC_DEFINE(HAVE_SSH2,1,[ssh2 installed (local)])
    fi
  else
    dnl search for installed ssh2 libraries

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

  dnl link platform specific libraries
  case $host_os in
    linux-* | sunos | solaris* | darwin*)
      dnl link crypto, too, if available (may be needed by some libssh2 implementations)
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
      ;;
    windows*|mingw*)
      dnl link crypto+crypt32, too, if available (may be needed by some libssh2 implementations)
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        AC_SEARCH_LIBRARIES(CertGetCertificateContextProperty,crypt32,[LIBRARIES="$LIBRARIES crypt32"],,windows.h wincrypt.h)
        AC_SEARCH_LIBRARIES(MD5_Init,crypto,[HAVE_CRYPTO=1;LIBRARIES="$LIBRARIES crypto";AC_DEFINE(HAVE_CRYPTO,1,[crypto installed])])
      else
        AC_SEARCH_STATIC_LIBRARIES(CertGetCertificateContextProperty,crypt32,[STATIC_LIBRARIES="$STATIC_LIBRARIES crypt32"],,windows.h wincrypt.h)
        if test -z "$HAVE_CRYPTO"; then
          AC_SEARCH_STATIC_LIBRARIES(MD5_Init,crypto,[HAVE_CRYPTO=1;STATIC_LIBRARIES="$STATIC_LIBRARIES crypto";AC_DEFINE(HAVE_CRYPTO,1,[static crypto installed])])
        fi
        if test -z "$HAVE_CRYPTO"; then
          AC_SEARCH_LIBRARIES(MD5_Init,crypto,[HAVE_CRYPTO=1;LIBRARIES="$LIBRARIES crypto";AC_DEFINE(HAVE_CRYPTO,1,[crypto installed])])
        fi
      fi
      ;;
  esac

  if test -z "$HAVE_SSL"; then
    if test ! \( -d openssl -o -d "`readlink openssl`" \); then
      AC_MSG_WARN([openssl not compiled: openssl package missing])
    fi
  fi
  if test -z "$HAVE_SSH2"; then
    if test ! \( -d libssh2 -o -d "`readlink libssh2`" \); then
      AC_MSG_WARN([libssh2 not compiled: libssh2 package missing])
    fi
  fi
fi

# compile/use libidn2
if test \( $ENABLE_TLS = "yes" \) -o \( $ENABLE_CURL = "yes" \); then
  if test \( -d libidn2 -o -d "`readlink libidn2`" \) ; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([libidn2])
      result1=1
      if test $result -eq 1; then
        (cd libidn2; ./configure \
          --host=$host \
          --build=$build \
          --prefix=$cwd/extern \
          --disable-shared \
          --enable-static \
          --disable-doc \
          --with-included-libunistring \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd libidn2; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd libidn2; make install) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES idn2"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES idn2"
      fi
      HAVE_IDN2=1
      AC_DEFINE(HAVE_IDN2,1,[libidn2 installed (local)])
    fi
  else
    dnl search for installed libidn2+libunistring libraries

    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(idn2_check_version,idn2,[HAVE_IDN2=1;LIBRARIES="$LIBRARIES idn2";AC_DEFINE(HAVE_IDN2,1,[libidn2 installed])])
    else
      if test -z "$HAVE_IDN2"; then
        AC_SEARCH_STATIC_LIBRARIES(idn2_check_version,idn2,[HAVE_IDN2=1;STATIC_LIBRARIES="$STATIC_LIBRARIES idn2";AC_DEFINE(HAVE_IDN2,1,[static libidn2 installed])])
      fi
    fi
  fi

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac
fi

# compile/use gnutls
if test $ENABLE_TLS = "yes"; then
  if test \( -d gmp -o -d "`readlink gmp`" \) -a \( -d nettle -o -d "`readlink nettle`" \) -a \( -d gnutls -o -d "`readlink gnutls`" \) -a "$HAVE_IDN2"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      if test -z "$HAVE_GCRYPT"; then
        AC_MSG_WARN([gcrypt is missing. Compiling or linking gnutls may fail!])
      fi

      AC_MSG_COMPILE([gmp])
      result1=1
      if test $result1 -eq 1; then
        # Note: clean is obviously not enough, try distclean
        (cd gmp; make distclean) 1>/dev/null 2>&1
        if test "$host_cpu" = "x86_64"; then
          case $host_os in
            linux-*)
              case $host_cpu in
                *86*)
                  (cd gmp; CFLAGS= ./configure \
                    --host=$host \
                    --build=$build \
                    --prefix=$cwd/extern \
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
                    --build=$build \
                    --prefix=$cwd/extern \
                    --disable-shared \
                    --enable-static \
                    --enable-pic \
                    --enable-fat \
                  ) 1>$tmpFile 2>&1
                  ;;
              esac
              ;;
            sunos | solaris*)
              ;;
            darwin*)
              ;;
            windows*|mingw*)
              (cd gmp; CFLAGS= ./configure \
                --host=$host \
                --build=$build \
                --prefix=$cwd/extern \
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
            --prefix=$cwd/extern \
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
        (cd gmp; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
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
          --build=$build \
          --prefix=$cwd/extern \
          --with-include-path=$cwd/extern/include \
          --with-lib-path=$cwd/extern/lib \
          --disable-shared \
          --enable-static \
          --enable-fat \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd nettle; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
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
        (cd gnutls; LDFLAGS=-L$cwd/extern/lib PKG_CONFIG_PATH=$cwd/extern/lib/pkgconfig ./configure \
          --host=$host \
          --build=$build \
          --prefix=$cwd/extern \
          --with-libnettle-prefix=$cwd/extern \
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
        (cd gnutls; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES gnutls nettle hogweed gmp"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES gnutls nettle hogweed gmp"
      fi
      HAVE_GNU_TLS=1
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

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

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

# compile/use curl
if test $ENABLE_CURL = "yes"; then
#  if test \( -d c-ares -o -d "`readlink c-ares`" \) -a \( -d curl -o -d "`readlink curl`" \) -a \( -d mxml -o -d "`readlink mxml`" \) -a "$HAVE_GPG_ERROR" -a "$HAVE_IDN2"; then
  if test \( -d c-ares -o -d "`readlink c-ares`" \) -a \( -d curl -o -d "`readlink curl`" \) -a \( -d mxml -o -d "`readlink mxml`" \); then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      dnl compile c-ares, curl, mxml libraries

      cwd=`pwd`

      if test -z "$HAVE_Z"; then
        AC_MSG_WARN([zlib is missing. Compiling or linking curl may fail!])
      fi
      if test -z "$HAVE_ZSTD"; then
        AC_MSG_WARN([zstd is missing. Compiling or linking curl may fail!])
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
      if test -z "$HAVE_IDN2"; then
        AC_MSG_WARN([libidn2 is missing. Compiling or linking curl may fail!])
      fi

      AC_MSG_COMPILE([c-ares])
      result1=1
      if test $result1 -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            #   diff -u curl-7.28.1.org/m4/libtool.m4 curl-7.28.1/m4/libtool.m4 >  curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/aclocal.m4 curl-7.28.1/aclocal.m4       >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/Makefile.in curl-7.28.1/Makefile.in     >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/configure curl-7.28.1/configure         >> curl-7.28.1-libtool.patch
#TODO: windows cross-build
#            if test -d extern/curl-7.28.1; then
#              (cd extern/curl-7.28.1; $PATCH --batch -N -p1 < $SOURCE_DIR/misc/curl-7.28.1-libtool.patch) 1>$tmpFile 2>&1
#            else
#              AC_MSG_WARN([curl-7.28.1 not available - skipped libtool patch!])
#            fi
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        # Note: https://github.com/c-ares/c-ares/blob/cares-1_14_0/README.msvc#L133-L138
        install -d c-ares
        (cd c-ares; CPPFLAGS="-DCARES_STATICLIB" CFLAGS="" LDFLAGS="" ./configure \
          --host=$host \
          --build=$build \
          --prefix=$cwd/extern \
          --disable-shared \
          --enable-static \
          --disable-tests \
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
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            (cd extern/lib; ln -s libcares.lib libcares.a) 1>/dev/null 2>/dev/null
            ;;
        esac
      fi
      if test $result1 -eq 1; then
        AC_MSG_RESULT([done])
      fi

      AC_MSG_COMPILE([mxml])
      result2=1
      if test $result2 -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            #   diff -u curl-7.28.1.org/m4/libtool.m4 curl-7.28.1/m4/libtool.m4 >  curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/aclocal.m4 curl-7.28.1/aclocal.m4       >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/Makefile.in curl-7.28.1/Makefile.in     >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/configure curl-7.28.1/configure         >> curl-7.28.1-libtool.patch
#TODO: windows cross-build
#            if test -d extern/curl-7.28.1; then
#              (cd extern/curl-7.28.1; $PATCH --batch -N -p1 < $SOURCE_DIR/misc/curl-7.28.1-libtool.patch) 1>$tmpFile 2>&1
#            else
#              AC_MSG_WARN([curl-7.28.1 not available - skipped libtool patch!])
#            fi
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd mxml; ./configure \
          --host=$host \
          --build=$build \
          --prefix=$cwd/extern \
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
        (cd mxml; make clean; make -j$CPU_CORES libmxml.a) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        # Note: there is no target to install the library and header files only; do it manually
        (cd mxml; \
         make install-libmxml.a; \
         install -m 644 mxml.h $cwd/extern/include; \
         install -m 644 mxml.pc $cwd/extern/lib/pkgconfig; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            (cd extern/lib; ln -s libmxml.lib libmxml.a) 1>/dev/null 2>/dev/null
            ;;
        esac
      fi
      if test $result2 -eq 1; then
        AC_MSG_RESULT([done])
      fi

      AC_MSG_COMPILE([curl])
      result3=1
      if test $result3 -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            #   diff -u curl-7.28.1.org/m4/libtool.m4 curl-7.28.1/m4/libtool.m4 >  curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/aclocal.m4 curl-7.28.1/aclocal.m4       >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/Makefile.in curl-7.28.1/Makefile.in     >> curl-7.28.1-libtool.patch
            #   diff -u curl-7.28.1.org/configure curl-7.28.1/configure         >> curl-7.28.1-libtool.patch
#TODO: windows cross-build
#            if test -d extern/curl-7.28.1; then
#              (cd extern/curl-7.28.1; $PATCH --batch -N -p1 < $SOURCE_DIR/misc/curl-7.28.1-libtool.patch) 1>$tmpFile 2>&1
#            else
#              AC_MSG_WARN([curl-7.28.1 not available - skipped libtool patch!])
#            fi
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
      fi
      if test $result3 -eq 1; then
        if test "$host" == "$build"; then
          # compile build
# TODO: temporary disabled brotli compression, psl
          (cd curl; PKG_CONFIG_PATH=$cwd/extern/lib/pkgconfig ./configure \
            --host=$host \
            --build=$build \
            --prefix=$cwd/extern \
            --disable-shared \
            --enable-static \
            --disable-ldap \
            --disable-rtsp \
            --without-librtmp \
            --enable-ares=$cwd/extern \
            --with-zlib=$cwd/extern \
            `if test $HAVE_ZSTD; then echo --with-zstd=$cwd/extern; else echo --without-zstd; fi` \
            `if test $HAVE_GNU_TLS; then echo --with-gnutls=$cwd/extern; fi` \
            `if test $HAVE_SSL; then echo --with-openssl=$cwd/extern --disable-openssl-auto-load-config; else echo --without-ssl; fi` \
            `if test $HAVE_SSH2; then echo --with-libssh2=$cwd/extern; fi` \
--without-brotli \
--without-libpsl \
            `if test "$ENABLE_DEBUG" = "yes"; then echo --enable-debug; fi` \
          ) 1>$tmpFile 2>&1
          if test $? -ne 0; then
            AC_MSG_RESULT([fail (configure)])
            AC_MSG_OUTPUT_LOG($tmpFile)
            result3=0
          fi
        else
          # cross-compile host
          (cd curl; PKG_CONFIG_PATH=$cwd/extern/lib/pkgconfig CPPFLAGS="-DCARES_STATICLIB" LDFLAGS="" LIBS="-lcrypt32 -liphlpapi -lssp" ./configure \
            --host=$host \
            --build=$build \
            --prefix=$cwd/extern \
            --disable-shared \
            --enable-static \
            --disable-ldap \
            --disable-rtsp \
            --without-librtmp \
            --enable-ares=$cwd/extern \
            --with-zlib=$cwd/extern \
            `if test $HAVE_ZSTD; then echo --with-zstd=$cwd/extern; else echo --without-zstd; fi` \
            `if test $HAVE_GNU_TLS; then echo --with-gnutls=$cwd/extern; fi` \
            `if test $HAVE_SSL; then echo --with-openssl=$cwd/extern --disable-openssl-auto-load-config; else echo --without-ssl; fi` \
            `if test $HAVE_SSH2; then echo --with-libssh2=$cwd/extern; fi` \
            `if test "$ENABLE_DEBUG" = "yes"; then echo --enable-debug; fi` \
          ) 1>$tmpFile 2>&1
          if test $? -ne 0; then
            AC_MSG_RESULT([fail (configure)])
            AC_MSG_OUTPUT_LOG($tmpFile)
            result3=0
          fi
        fi
      fi
      if test $result3 -eq 1; then
        (cd curl; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
      fi
      if test $result3 -eq 1; then
        (cd curl; make install) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            (cd extern/lib; ln -s libcurl.lib libcurl.a) 1>$tmpFile 2>&1
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
      CC_DEFINES="$CC_DEFINES CARES_STATICLIB CURL_STATICLIB"
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES curl cares mxml"
        if test "$host" != "$build"; then
          LIBRARIES="$LIBRARIES crypt32"
        fi
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES curl cares mxml"
        if test "$host" != "$build"; then
          STATIC_LIBRARIES="$STATIC_LIBRARIES crypt32"
        fi
      fi
      HAVE_FTP=1
      HAVE_CARES=1
      HAVE_MXML=1
      HAVE_CURL=1
      AC_DEFINE(HAVE_CARES,1,[c-ares installed (local)])
      AC_DEFINE(HAVE_MXML,1,[mxml installed (local)])
      AC_DEFINE(HAVE_CURL,1,[curl installed (local)])
    fi
  else
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(curl_global_init,curl,[HAVE_CURL=1;LIBRARIES="$LIBRARIES curl";AC_DEFINE(HAVE_CURL,1,[curl installed])])
      AC_SEARCH_LIBRARIES(ares_library_init,cares,[HAVE_CARES=1;LIBRARIES="$LIBRARIES cares";AC_DEFINE(HAVE_CARES,1,[c-ares installed])])
      AC_SEARCH_LIBRARIES(mxmlNewXML,mxml,[HAVE_MXML=1;LIBRARIES="$LIBRARIES mxml";AC_DEFINE(HAVE_MXML,1,[mxml installed])])
    else
      if test -z "$HAVE_CURL"; then
        AC_SEARCH_STATIC_LIBRARIES(curl_global_init,curl,[HAVE_CURL=1;STATIC_LIBRARIES="$STATIC_LIBRARIES curl";AC_DEFINE(HAVE_CURL,1,[static curl installed])])
      fi
      if test -z "$HAVE_CURL"; then
        AC_SEARCH_LIBRARIES(curl_global_init,curl,[HAVE_CURL=1;LIBRARIES="$LIBRARIES curl";AC_DEFINE(HAVE_CURL,1,[curl installed])])
      fi
      if test -z "$HAVE_CARES"; then
        AC_SEARCH_STATIC_LIBRARIES(ares_library_init,cares,[HAVE_CARES=1;STATIC_LIBRARIES="$STATIC_LIBRARIES cares";AC_DEFINE(HAVE_CURL,1,[static c-ares installed])])
      fi
      if test -z "$HAVE_CARES"; then
        AC_SEARCH_LIBRARIES(ares_library_init,cares,[HAVE_CARES=1;LIBRARIES="$LIBRARIES cares";AC_DEFINE(HAVE_CURL,1,[c-ares installed])])
      fi
      if test -z "$HAVE_MXML"; then
        AC_SEARCH_STATIC_LIBRARIES(mxmlNewXML,mxml,[HAVE_MXML=1;STATIC_LIBRARIES="$STATIC_LIBRARIES mxml";AC_DEFINE(HAVE_MXML,1,[static mxml installed])])
      fi
      if test -z "$HAVE_MXML"; then
        AC_SEARCH_LIBRARIES(mxmlNewXML,mxml,[HAVE_MXML=1;LIBRARIES="$LIBRARIES mxml";AC_DEFINE(HAVE_MXML,1,[mxml installed])])
      fi
    fi
  fi

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_CURL"; then
    if test ! \( -d curl -o -d "`readlink curl`" \); then
      AC_MSG_WARN([curl not compiled: curl package missing])
    fi
  fi
  if test -z "$HAVE_CARES"; then
    if test ! \( -d c-ares -o -d "`readlink c-ares`" \); then
      AC_MSG_WARN([curl not compiled: c-ares package missing])
    fi
  fi
  if test -z "$HAVE_MXML"; then
    if test ! \( -d mxml -o -d "`readlink mxml`" \); then
      AC_MSG_WARN([curl not compiled: mxml package missing])
    fi
  fi
  if test -z "$HAVE_IDN2"; then
    if test ! \( -d libidn2 -o -d "`readlink libidn2`" \); then
      AC_MSG_WARN([curl not compiled: libidn2 package missing])
    fi
  fi

  if test "$HAVE_CURL" -a "$HAVE_CARES" -a "$HAVE_MXML" -a "$HAVE_IDN2"; then
    dnl link libssl, too
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(SSL_new,ssl,[HAVE_SSL=1;LIBRARIES="$LIBRARIES ssl"],AC_ERROR([SSL library required for curl]))
    else
      AC_SEARCH_STATIC_LIBRARIES(SSL_new,ssl,[HAVE_SSL=1;STATIC_LIBRARIES="$STATIC_LIBRARIES ssl"],AC_ERROR([static SSL library required for curl]))
    fi
  fi
fi

# create sqlite3.c
if test -d sqlite -o -d "`readlink sqlite`"; then
  if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
    dnl create sqlite3.c almagation

    cwd=`pwd`

    AC_MSG_CREATE([sqlite3.c])
    result=1
    if test $result -eq 1; then
      # Note: no cross-compile; create source only
      (cd sqlite; CFLAGS="-DSQLITE_ENABLE_UPDATE_DELETE_LIMIT" LDFLAGS="" ./configure \
        --build=$build \
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
       install -d $cwd/extern/sources; \
       install -d $cwd/extern/include; \
       install -m 644 sqlite3.c $cwd/extern/sources; \
       install -m 644 sqlite3.h $cwd/extern/include; \
      ) 1>$tmpFile 2>&1
    fi
    if test $result -eq 1; then
      AC_MSG_RESULT([done])
    else
      AC_MSG_ERROR([Cannot create sqlite3.c])
    fi
  fi
fi

# compile MariaDB client
if test $ENABLE_MARIADB = "yes"; then
  if test \( -d zlib -o -d "`readlink zlib`" \) -a \( -d mariadb-connector-c -o -d "`readlink mariadb-connector-c`" \); then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([MariaDB client])
      result=1
      if test $result -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            # patch to fix compile with GNUTLS, fix signature mysql_load_plugin()
            #   diff -u -r mariadb-connector-c-3.1.13-src.org mariadb-connector-c-3.1.13-src > misc/mariadb-connector-c-3.1.13.patch
#TODO            (cd mariadb-connector-c; $PATCH --batch -N -p1 < $SOURCE_DIR/misc/mariadb-connector-c-3.1.13.patch) 1>$tmpFile 2>&1
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            (cd mariadb-connector-c; PKG_CONFIG_PATH=$cwd/extern/lib/pkgconfig CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="$LIBS" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" $CMAKE . \
              -DCMAKE_SYSTEM_NAME=$SYSTEM_NAME \
              -DCMAKE_INSTALL_PREFIX=$cwd/extern \
              -DWITH_EXTERNAL_ZLIB=ON \
              -DZLIB_INCLUDE_DIR=$cwd/extern/include \
              -DZLIB_LIBRARY=$cwd/extern/lib/libz.a \
              -DWITHSSL=`if test "$HAVE_SSL" = "yes"; then echo GNUTLS; else echo OFF; fi` \
              -DWITH_CURL=OFF \
              -DWITH_UNIT_TESTS=OFF \
              `if test "$ENABLE_DEBUG" = "yes"; then echo -DCMAKE_BUILD_TYPE=Debug; fi` \
            ) 1>$tmpFile 2>&1
            ;;
          windows*|mingw*)
            (cd mariadb-connector-c; PKG_CONFIG_PATH=$cwd/extern/lib/pkgconfig CC="$CC" CPPFLAGS="-D_WIN32_WINNT=0x0600 -D_POSIX_C_SOURCE" CFLAGS="-D_WIN32_WINNT=0x0600 -D_POSIX_C_SOURCE" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="$LIBS" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" $CMAKE . \
              -DCMAKE_SYSTEM_NAME=$SYSTEM_NAME \
              -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
              -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
              -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
              -DCMAKE_INSTALL_PREFIX=$cwd/extern \
              -DWITH_EXTERNAL_ZLIB=ON \
              -DZLIB_INCLUDE_DIR=$cwd/extern/include \
              -DZLIB_LIBRARY=$cwd/extern/lib/libz.a \
              -DWITH_SSL=`if test "$HAVE_SSL" = "yes"; then echo GNUTLS; else echo OFF; fi` \
              -DWITH_CURL=OFF \
              -DWITH_UNIT_TESTS=OFF \
              `if test "$ENABLE_DEBUG" = "yes"; then echo -DCMAKE_BUILD_TYPE=Debug; fi` \
            ) 1>$tmpFile 2>&1
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd mariadb-connector-c; \
         make clean; \
         make -C libmariadb mariadbclient -j$CPU_CORES; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd mariadb-connector-c; \
         make -C include install PREFIX=$cwd/extern; \
         install -t $cwd/extern/lib libmariadb/libmariadbclient.a; \
        ) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES mariadbclient"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES mariadbclient"
      fi
      HAVE_MARIADB=1
      AC_DEFINE(HAVE_MARIADB,1,[mariadb-connector-c installed (local)])
    fi
  else
    dnl search for installed mariadbclient library

    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(mysql_init,mariadb,[HAVE_MARIADB=1;LIBRARIES="mariadbclient $LIBRARIES";AC_DEFINE(HAVE_MARIADB,1,[mariadbclient installed])])
    else
      if test -z "$HAVE_MARIADB"; then
        AC_SEARCH_STATIC_LIBRARIES(mysql_init,mariadb,[HAVE_MARIADB=1;STATIC_LIBRARIES="$STATIC_LIBRARIES mariadbclient";AC_DEFINE(HAVE_MARIADB,1,[mariadbclient installed])])
      fi
      if test -z "$HAVE_MARIADB"; then
        AC_SEARCH_LIBRARIES(mysql_init,mariadb,[HAVE_MARIADB=1;LIBRARIES="mariadbclient $LIBRARIES";AC_DEFINE(HAVE_MARIADB,1,[mariadbclient installed])])
      fi
    fi
  fi

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_MARIADB"; then
    if test ! \( -d zlib -o -d "`readlink zlib`" \); then
      AC_MSG_WARN([MariaDB client not compiled: zlib package missing])
    fi
    if test ! \( -d mariadb-connector-c -o -d "`readlink mariadb-connector-c`" \); then
      AC_MSG_WARN([MariaDB client not compiled: MariaDB client package missing])
    fi
  fi
fi

# compile PostgreSQL client
if test $ENABLE_POSTGRESQL = "yes"; then
  if test -d postgresql -o -d "`readlink postgresql`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([PostgreSQL connector])
      result=1
      if test $result -eq 1; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            (cd postgresql; PKG_CONFIG_PATH=$cwd/extern/lib/pkgconfig CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
              --with-includes=$cwd/extern/include \
              --with-libraries=$cwd/extern/lib \
              `if test "$HAVE_SSL"; then echo --with-openssl; else echo --without-openssl; fi` \
              --without-readline \
              `if test "$ENABLE_DEBUG" = "yes"; then echo --enable-debug; fi` \
            ) 1>$tmpFile 2>&1
            ;;
          windows*|mingw*)
            (cd postgresql; PKG_CONFIG_PATH=$cwd/extern/lib/pkgconfig CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread -lcrypt32 -lwsock32 -lws2_32" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
              --with-includes=$cwd/extern/include \
              --with-libraries=$cwd/extern/lib \
              `if test "$HAVE_SSL"; then echo --with-openssl; else echo --without-openssl; fi` \
              --without-readline \
              `if test "$ENABLE_DEBUG" = "yes"; then echo --enable-debug; fi` \
            ) 1>$tmpFile 2>&1
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd postgresql; \
         make -C src/common           MAKELEVEL=0 -j$CPU_CORES; \
         make -C src/port             MAKELEVEL=0 -j$CPU_CORES; \
         make -C src/interfaces/libpq MAKELEVEL=0 -j$CPU_CORES; \
         make -C src/interfaces/ecpg  MAKELEVEL=0 -j$CPU_CORES; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd postgresql; \
         make -C src/include          MAKELEVEL=0 install PREFIX=$cwd/extern; \
         make -C src/common           MAKELEVEL=0 install PREFIX=$cwd/extern; \
         make -C src/port             MAKELEVEL=0 install PREFIX=$cwd/extern; \
         make -C src/interfaces/libpq MAKELEVEL=0 install PREFIX=$cwd/extern; \
         make -C src/interfaces/ecpg  MAKELEVEL=0 install PREFIX=$cwd/extern; \
        ) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES pq pgport pgcommon pgcommon_shlib pgtypes"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES pq pgport pgcommon pgcommon_shlib pgtypes"
      fi
      HAVE_POSTGRESQL=1
      AC_DEFINE(HAVE_POSTGRESQL,1,[pq/pgport/pgcommon/pgtypes installed (local)])
    fi
  else
    dnl search for installed pq library

    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(pq_init,pq,[HAVE_POSTGRESQL=1;LIBRARIES="pq $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pq installed])])
      AC_SEARCH_LIBRARIES(pg_printf,pgport,[HAVE_POSTGRESQL=1;LIBRARIES="pgport $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pgport installed])])
      AC_SEARCH_LIBRARIES(scram_HMAC_init,pgcommon,[HAVE_POSTGRESQL=1;LIBRARIES="pgcommon $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pgcommon installed])])
      AC_SEARCH_LIBRARIES(pg_encoding_to_char,pgcommon_shlib,[HAVE_POSTGRESQL=1;LIBRARIES="pgcommon_shlib $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pgcommon_shlib installed])])
      AC_SEARCH_LIBRARIES(PGTYPESnumeric_to_long,pgtypes,[HAVE_POSTGRESQL=1;LIBRARIES="pgtypes $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pqtypes installed])])
    else
      if test -z "$HAVE_POSTGRESQL"; then
        AC_SEARCH_STATIC_LIBRARIES(pq_init,pq,[HAVE_POSTGRESQL=1;STATIC_LIBRARIES="$STATIC_LIBRARIES pq";AC_DEFINE(HAVE_POSTGRESQL,1,[pq installed])])
        AC_SEARCH_STATIC_LIBRARIES(pg_printf,pgport,[HAVE_POSTGRESQL=1;LIBRARIES="pgport $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pgport installed])])
        AC_SEARCH_STATIC_LIBRARIES(scram_HMAC_init,pgcommon,[HAVE_POSTGRESQL=1;LIBRARIES="pgcommon $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pgcommon installed])])
        AC_SEARCH_STATIC_LIBRARIES(pg_encoding_to_char,pgcommon_shlib,[HAVE_POSTGRESQL=1;LIBRARIES="pgcommon_shlib $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pgcommon_shlib installed])])
        AC_SEARCH_STATIC_LIBRARIES(PGTYPESnumeric_to_long,pgtypes,[HAVE_POSTGRESQL=1;STATIC_LIBRARIES="$STATIC_LIBRARIES pgtypes";AC_DEFINE(HAVE_POSTGRESQL,1,[pq installed])])
      fi
      if test -z "$HAVE_POSTGRESQL"; then
        AC_SEARCH_LIBRARIES(pq_init,pq,[HAVE_POSTGRESQL=1;LIBRARIES="pq $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pq installed])])
        AC_SEARCH_LIBRARIES(pg_printf,pgport,[HAVE_POSTGRESQL=1;LIBRARIES="pgport $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pgport installed])])
        AC_SEARCH_LIBRARIES(scram_HMAC_init,pgcommon,[HAVE_POSTGRESQL=1;LIBRARIES="pgcommon $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pgcommon installed])])
        AC_SEARCH_LIBRARIES(pg_encoding_to_char,pgcommon_shlib,[HAVE_POSTGRESQL=1;LIBRARIES="pgcommon_shlib $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pgcommon_shlib installed])])
        AC_SEARCH_LIBRARIES(PGTYPESnumeric_to_long,pgtypes,[HAVE_POSTGRESQL=1;LIBRARIES="pgtypes $LIBRARIES";AC_DEFINE(HAVE_POSTGRESQL,1,[pq installed])])
      fi
    fi
  fi

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_POSTGRESQL"; then
    if test ! \( -d postgresql -o -d "`readlink postgresql`" \); then
      AC_MSG_WARN([PostgreSQL not compiled: PostgreSQL package missing])
    fi
  fi
fi

# compile/use cdio
if test $ENABLE_ISO9660 = "yes"; then
  if test \( -d libiconv -o -d "`readlink libiconv`" \) -a \( -d libcdio -o -d "`readlink libcdio`" \); then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([libiconv])
      result1=1
      if test $result1 -eq 1; then
        (cd libiconv; CFLAGS="-fPIC" LDFLAGS="" ./configure \
          --host=$host \
          --build=$build \
          --prefix=$cwd/extern \
          --disable-shared \
          --enable-static \
          --with-pic \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG(libiconv/config.log)
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd libiconv; make clean; make) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG(libiconv/config.log)
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd libiconv; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG(libiconv/config.log)
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        AC_MSG_RESULT([done])
      fi

      AC_MSG_COMPILE([libcdio])
      result2=1
      if test $result2 -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            ;;
        esac
        if test $result2 -ne 1; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
        fi
      fi
      if test $result2 -eq 1; then
        if test "$ENABLE_DEBUG" = "yes"; then
          (cd libcdio; CFLAGS="-g -O0" CXXFLAGS="-g -O0" LDFLAGS="" ./configure \
            --host=$host \
            --build=$build \
            --prefix=$cwd/extern \
            --with-libiconv-prefix=$cwd/extern \
            --disable-cxx \
            --enable-static=iso9660 \
            --disable-shared \
            --without-cd-drive \
            --without-cd-info \
            --without-cdda-player \
            --without-iso-info \
            --without-iso-read \
          ) 1>$tmpFile 2>&1
        else
          (cd libcdio; ./configure \
            --host=$host \
            --prefix=$cwd/extern \
            --disable-cxx \
            --enable-static=iso9660 \
            --disable-shared \
            --without-cd-drive \
            --without-cd-info \
            --without-cdda-player \
            --without-iso-info \
            --without-iso-read \
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
        (cd libcdio; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES iso9660 cdio iconv"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES iso9660 cdio iconv"
      fi
      HAVE_ISO9660=1
      AC_DEFINE(HAVE_ISO9660,1,[iso9660 installed (local)])
    fi
  else
    dnl search for installed libiconv, cdio libraries

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

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_ISO9660"; then
    if test ! \( -d libcdio -o -d "`readlink libcdio`" \); then
      AC_MSG_WARN([libcdio not compiled: libcdio package missing])
    fi
  fi
fi

# compile/use libsmb2
if test $ENABLE_SMBCLIENT = "yes"; then
  if test -d libsmb2 -o -d "`readlink libsmb2`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([libkrb5])
      result1=1
      if test $result1 -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            ;;
        esac
        if test $result1 -ne 1; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
        fi
      fi
      case $host_os in
        linux-*| sunos | solaris* | darwin*)
          if test $result1 -eq 1; then
            (cd krb5/src; CFLAGS="-fPIC" LDFLAGS="" ./configure \
              --prefix=$cwd/extern \
              --host=$host \
              --build=$build \
              --enable-static \
              --disable-shared \
            ) 1>$tmpFile 2>&1
            if test $? -ne 0; then
              AC_MSG_RESULT([fail (configure)])
              AC_MSG_OUTPUT_LOG(krb5/src/config.log)
              AC_MSG_OUTPUT_LOG($tmpFile)
              result1=0
            fi
          fi
          if test $result1 -eq 1; then
            # Note: build only lib: tests do not compile with static libraries
            (   cd krb5/src \
             && make clean \
             && make -j$CPU_CORES -C util \
             && make -j$CPU_CORES -C include \
             && make -j$CPU_CORES -C lib \
            ) 1>$tmpFile 2>&1
            if test $? -ne 0; then
              AC_MSG_RESULT([fail (make)])
              AC_MSG_OUTPUT_LOG(krb5/src/config.log)
              AC_MSG_OUTPUT_LOG($tmpFile)
              result1=0
            fi
          fi
          if test $result1 -eq 1; then
            # Note: work-around partial build: create required install directories
            $INSTALL -d $cwd/extern/include/krb5
            $INSTALL -d $cwd/extern/include/gssapi
            $INSTALL -d $cwd/extern/include/gssrpc
            $INSTALL -d $cwd/extern/include/kadm5
            $INSTALL -d $cwd/extern/sbin
            $INSTALL -d $cwd/extern/bin/compute_et
            $INSTALL -d $cwd/extern/share/man/man1
            (cd krb5/src;  make -C util install PREFIX=$cwd/extern && make -C include install PREFIX=$cwd/extern && make -C lib install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
            if test $? -ne 0; then
              AC_MSG_RESULT([fail (install)])
              AC_MSG_OUTPUT_LOG(krb5/src/config.log)
              AC_MSG_OUTPUT_LOG($tmpFile)
              result1=0
            fi
          fi
          if test $result1 -eq 1; then
            AC_MSG_RESULT([done])
          fi
          ;;
        windows*|mingw*)
          # not supported on Windows
# TODO
          AC_MSG_RESULT([skipped])
          ;;
      esac

      AC_MSG_COMPILE([libsmb2])
      result2=1
      if test $result2 -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            ;;
        esac
        if test $result2 -ne 1; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
        fi
      fi
      if test $result2 -eq 1; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            # Note: use configure, because with cmake the external Kerberos libraries are not used
            if test "$ENABLE_DEBUG" = "yes"; then
              (cd libsmb2; sh -x ./bootstrap && CFLAGS="-g -O0 -I$cwd/extern/include" CXXFLAGS="-g -O0" LDFLAGS="" ./configure \
                --prefix=$cwd/extern \
                --host=$host \
                --build=$build \
                --enable-static \
                --disable-shared \
                --disable-werror \
              ) 1>$tmpFile 2>&1
            else
              (cd libsmb2; sh -x ./bootstrap && CFLAGS="-I$cwd/extern/include" CXXFLAGS="" ./configure \
                --prefix=$cwd/extern \
                --host=$host \
                --build=$build \
                --enable-static \
                --disable-shared \
                --disable-werror \
              ) 1>$tmpFile 2>&1
            fi
            if test $? -ne 0; then
              AC_MSG_RESULT([fail (configure)])
              AC_MSG_OUTPUT_LOG($tmpFile)
              result2=0
            fi
            ;;
          windows*|mingw*)
            if test "$ENABLE_DEBUG" = "yes"; then
              (cd libsmb2; cmake \
                -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_SYSTEM_NAME=Windows \
                -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
                -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
                -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
                -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
                -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
                -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
                -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0600" \
                -DCMAKE_INSTALL_PREFIX=$cwd/extern \
                \
                -DOPENSSL_INCLUDE_DIR=$cwd/extern/include \
                -DBUILD_SHARED_LIBS=OFF
              ) 1>$tmpFile 2>&1
            else
              (cd libsmb2; cmake \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_SYSTEM_NAME=Windows \
                -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
                -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
                -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 \
                -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
                -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
                -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
                -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0600" \
                -DCMAKE_INSTALL_PREFIX=$cwd/extern \
                \
                -DOPENSSL_INCLUDE_DIR=$cwd/extern/include \
                -DBUILD_SHARED_LIBS=OFF
              ) 1>$tmpFile 2>&1
            fi
            if test $? -ne 0; then
              AC_MSG_RESULT([fail (configure)])
              AC_MSG_OUTPUT_LOG($tmpFile)
              result2=0
            fi
            ;;
        esac
      fi
      if test $result2 -eq 1; then
        (cd libsmb2; \
         make clean; \
         make -C include -j$CPU_CORES; \
         make -C lib -j$CPU_CORES; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd libsmb2; \
         make -C include install PREFIX=$cwd/extern; \
         make -C lib install PREFIX=$cwd/extern; \
        ) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            LIBRARIES="$LIBRARIES smb2 krb5support k5crypto com_err gssapi_krb5 krb5"
            ;;
          windows*|mingw*)
            LIBRARIES="$LIBRARIES smb2"
            ;;
        esac
      else
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            STATIC_LIBRARIES="$STATIC_LIBRARIES smb2 krb5support k5crypto com_err gssapi_krb5 krb5"
            ;;
          windows*|mingw*)
            STATIC_LIBRARIES="$STATIC_LIBRARIES smb2"
            ;;
        esac
      fi
      HAVE_SMB2=1
      AC_DEFINE(HAVE_SMB2,1,[SMB2 client installed (local)])
    fi
  else
    dnl search for installed krb5, smb2 libraries

    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(smb2_init_context,smb2,[HAVE_SMB2=1;LIBRARIES="$LIBRARIES smb2 krb5support k5crypto com_err gssapi_krb5 krb5";AC_DEFINE(HAVE_SMB2,1,[SMB2 client installed])])
    else
      if test -z "$HAVE_SMB2"; then
        AC_SEARCH_STATIC_LIBRARIES(smb2_init_context,smb2,[HAVE_SMB2=1;STATIC_LIBRARIES="$STATIC_LIBRARIES smb2 krb5support k5crypto com_err gssapi_krb5 krb5";AC_DEFINE(HAVE_SMB2,1,[static SMB2 client installed])])
      fi
      if test -z "$HAVE_SMB2"; then
        AC_SEARCH_LIBRARIES(smb2_init_context,smb2,[HAVE_SMB2=1;LIBRARIES="$LIBRARIES smb2 krb5support k5crypto com_err gssapi_krb5 krb5";AC_DEFINE(HAVE_SMB2,1,[SMB2 client installed])])
      fi
    fi
  fi

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_SMB2"; then
    if test ! \( -d krb5 -o -d "`readlink krb5`" \); then
      AC_MSG_WARN([libsmb2 not compiled: krb5 package missing])
    fi
    if test ! \( -d libsmb2 -o -d "`readlink libsmb2`" \); then
      AC_MSG_WARN([libsmb2 not compiled: libsmb2 package missing])
    fi
  fi
fi

# compile/use pcre
if test $ENABLE_PCRE = "yes"; then
  if test -d pcre -o -d "`readlink pcre`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([pcre])
      result=1
      if test $result -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            # patch to disable usage of lib-command when RANLIB is set
            #   diff -u pcre-8.32.org/m4/libtool.m4 pcre-8.32/m4/libtool.m4 >  pcre-8.32-libtool.patch
            #   diff -u pcre-8.32.org/aclocal.m4 pcre-8.32/aclocal.m4       >> pcre-8.32-libtool.patch
            #   diff -u pcre-8.32.org/Makefile.in pcre-8.32/Makefile.in     >> pcre-8.32-libtool.patch
            #   diff -u pcre-8.32.org/configure pcre-8.32/configure         >> pcre-8.32-libtool.patch
#TODO: windows cross-build
#            if test -d extern/pcre-8.32; then
#              (cd extern/pcre-8.32; $PATCH --batch -N -p1 < $SOURCE_DIR/misc/pcre-8.32-libtool.patch) 1>$tmpFile 2>&1
#            else
#              AC_MSG_WARN([pcre-8.32 not available - skipped libtool patch!])
#            fi
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd pcre; ./configure \
          --host=$host \
          --build=$build \
          --prefix=$cwd/extern \
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
        (cd pcre; make clean; make -j$CPU_CORES) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd pcre; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            (cd extern/lib; ln -s libpcre.lib libpcre.a) 1>/dev/null 2>/dev/null
            (cd extern/lib; ln -s libpcreposix.lib libpcreposix.a) 1>/dev/null 2>/dev/null
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
      CC_DEFINES="$CC_DEFINES PCRE_STATIC"
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES pcre pcreposix"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES pcre pcreposix"
      fi
      HAVE_PCRE=1
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

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_PCRE"; then
    if test ! \( -d pcre -o -d "`readlink pcre`" \); then
      AC_MSG_WARN([pcre not compiled: pcre package missing])
    fi
  fi
fi

# compile/use PAR2 library
if test $ENABLE_PAR2 = "yes"; then
  if test -d par2cmdline -o -d "`readlink par2cmdline`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([PAR2])
      result=1
      if test $result -eq 1; then
        (cd par2cmdline; ./automake.sh) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (automake)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            (cd par2cmdline; CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
              --disable-openmp \
            ) 1>$tmpFile 2>&1
            ;;
          windows*|mingw*)
            (cd par2cmdline; CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread -lcrypt32 -lwsock32 -lws2_32" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
              --disable-openmp \
            ) 1>$tmpFile 2>&1
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd par2cmdline; \
         make -j$CPU_CORES; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd par2cmdline; \
         cp src/libpar2.h $cwd/extern/include; \
         cp libpar2.a $cwd/extern/lib; \
        ) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES par2"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES par2"
      fi
      HAVE_PAR2=1
      AC_DEFINE(HAVE_PAR2,1,[PAR2 installed (local)])
    fi
  else
    dnl search for installed PAR2 library

    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(par2create,par2,[HAVE_PAR2=1;LIBRARIES="par2 $LIBRARIES";AC_DEFINE(HAVE_PAR2,1,[PAR2 installed])])
    else
      if test -z "$HAVE_PAR2"; then
        AC_SEARCH_STATIC_LIBRARIES(par2create,par2,[HAVE_PAR2=1;STATIC_LIBRARIES="$STATIC_LIBRARIES par2";AC_DEFINE(HAVE_PAR2,1,[PAR2 installed])])
      fi
      if test -z "$HAVE_PAR2"; then
        AC_SEARCH_LIBRARIES(par2create,par2,[HAVE_PAR2=1;LIBRARIES="par2 $LIBRARIES";AC_DEFINE(HAVE_PAR2,1,[PAR2 installed])])
      fi
    fi
  fi

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_PAR2"; then
    if test ! \( -d par2cmdline -o -d "`readlink par2cmdline`" \); then
      AC_MSG_WARN([PAR2 not compiled: PAR2 package missing])
    fi
  fi
fi

# compile/use isofs library
if test $ENABLE_ISOFS = "yes"; then
  if test -d libisofs -o -d "`readlink libisofs`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([libisofs])
      result=1
      if test $result -eq 1; then
        (cd libisofs; ./bootstrap) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (automake)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            (cd libisofs; CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
            ) 1>$tmpFile 2>&1
            ;;
          windows*|mingw*)
            (cd libisofs; CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread -lcrypt32 -lwsock32 -lws2_32" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
            ) 1>$tmpFile 2>&1
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd libisofs; \
         make -j$CPU_CORES; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd libisofs; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES isofs"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES isofs"
      fi
      HAVE_ISOFS=1
      AC_DEFINE(HAVE_ISOFS,1,[libisofs installed (local)])
    fi
  else
    dnl search for installed isofs library

    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(iso_image_new,isofs,[HAVE_ISOFS=1;LIBRARIES="isofs $LIBRARIES";AC_DEFINE(HAVE_ISOFS,1,[libisofs installed])])
    else
      if test -z "$HAVE_ISOFS"; then
        AC_SEARCH_STATIC_LIBRARIES(iso_image_new,isofs,[HAVE_ISOFS=1;STATIC_LIBRARIES="$STATIC_LIBRARIES isofs";AC_DEFINE(HAVE_ISOFS,1,[libisofs installed])])
      fi
      if test -z "$HAVE_ISOFS"; then
        AC_SEARCH_LIBRARIES(iso_image_new,isofs,[HAVE_ISOFS=1;LIBRARIES="isofs $LIBRARIES";AC_DEFINE(HAVE_ISOFS,1,[libisofs installed])])
      fi
    fi
  fi

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_ISOFS"; then
    if test ! \( -d libisofs -o -d "`readlink libisofs`" \); then
      AC_MSG_WARN([libisofs not compiled: libisofs package missing])
    fi
  fi
fi

# compile/use xfsprogrs (libuuid liburcu libxfs)
if test $ENABLE_XFS = "yes"; then
  if test \( -d util-linux -o -d "`readlink util-linux`" \) -a \( -d userspace-rcu -o -d "`readlink userspace-rcu`" \) -a \( -d xfsprogs -o -d "`readlink xfsprogs`" \); then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      dnl compile libuuid, libblkid from util-linux
      AC_MSG_COMPILE([libuuid])
      result1=1
      if test $result1 -eq 1; then
        (cd util-linux; ./autogen.sh) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (autogen)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            (cd util-linux; CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
              --disable-all-programs --enable-libuuid --enable-libblkid \
            ) 1>$tmpFile 2>&1
            ;;
          windows*|mingw*)
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd util-linux; \
         make -j$CPU_CORES; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        (cd util-linux; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result1=0
        fi
      fi
      if test $result1 -eq 1; then
        AC_MSG_RESULT([done])
      fi

      AC_MSG_COMPILE([userspace-rcu])
      result2=1
      if test $result2 -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            (cd userspace-rcu; CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
            ) 1>$tmpFile 2>&1
            ;;
          windows*|mingw*)
            (cd userspace-rcu; CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread -lcrypt32 -lwsock32 -lws2_32" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
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
        (cd userspace-rcu; \
         make -j$CPU_CORES; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        (cd userspace-rcu; \
         make install PREFIX=$cwd/extern; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result2=0
        fi
      fi
      if test $result2 -eq 1; then
        AC_MSG_RESULT([done])
      fi

      AC_MSG_COMPILE([xfsprogs])
      result3=1
      if test $result3 -eq 1; then
        case $host_os in
          linux-*)
            ;;
          sunos | solaris*)
            ;;
          darwin*)
            ;;
          windows*|mingw*)
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
      fi
      if test $result3 -eq 1; then
        (cd xfsprogs; make configure) 1>$tmpFile 2>&1
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            (cd xfsprogs; CC="$CC" CPPFLAGS="" CFLAGS="-I$cwd/extern/include" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
            ) 1>$tmpFile 2>&1
            ;;
          windows*|mingw*)
            (cd xfsprogs; CC="$CC" CPPFLAGS="" CFLAGS="-I$cwd/extern/include" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread -lcrypt32 -lwsock32 -lws2_32" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
            ) 1>$tmpFile 2>&1
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
      fi

      if test $result3 -eq 1; then
        (   cd xfsprogs \
         && make headers \
         && make -j$CPU_CORES -C include install-headers \
         && make -j$CPU_CORES -C libxfs install-headers \
         && make -j$CPU_CORES -C include \
         && make -j$CPU_CORES -C libfrog \
         && make -j$CPU_CORES -C libxfs \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result3=0
        fi
      fi
      if test $result3 -eq 1; then
        (   cd xfsprogs \
         && make -j$CPU_CORES -C libfrog install-dev \
         && make -j$CPU_CORES -C libxfs install-dev \
        ) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES xfs frog urcu uuid blkid"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES xfs frog urcu uuid blkid"
      fi
      HAVE_UUID=1
      HAVE_RCU=1
      HAVE_XFS=1
      AC_DEFINE(HAVE_XFS,1,[xfs installed (local)])
    fi
  else
    dnl search for installed libxfs, libfrog, liburcu libraries

# TODO: frog, rcu
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(libxfs_init,xfs,[HAVE_XFSPROGS=1;LIBRARIES="xfs frog urcu uuid blkid $LIBRARIES";AC_DEFINE(HAVE_XFS,1,[libxfs installed])])
    else
      if test -z "$HAVE_XFSPROGS"; then
        AC_SEARCH_STATIC_LIBRARIES(libxfs_init,xfs,[HAVE_XFSPROGS=1;STATIC_LIBRARIES="$STATIC_LIBRARIES xfs frog urcu uuid blkid";AC_DEFINE(HAVE_XFS,1,[libxfs installed])])
      fi
      if test -z "$HAVE_XFSPROGS"; then
        AC_SEARCH_LIBRARIES(libxfs_init,xfs,[HAVE_XFSPROGS=1;LIBRARIES="xfs frog urcu uuid blkid $LIBRARIES";AC_DEFINE(HAVE_XFS,1,[libxfs installed])])
      fi
    fi
  fi

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_XFS"; then
    if test ! \( -d util-linux -o -d "`readlink util-linux`" \); then
      AC_MSG_WARN([util-linux not compiled: util-linux package missing])
    fi
    if test ! \( -d userspace-rcu -o -d "`readlink userspace-rcu`" \); then
      AC_MSG_WARN([userspace-rcu not compiled: userspace-rcu package missing])
    fi
    if test ! \( -d xfsprogs -o -d "`readlink xfsprogs`" \); then
      AC_MSG_WARN([xfsprogs not compiled: xfsprogs package missing])
    fi
  fi
fi

# compile/use burn library
if test $ENABLE_BURN = "yes"; then
  if test -d libburn -o -d "`readlink libburn`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([libburn])
      result=1
      if test $result -eq 1; then
        (cd libburn; ./bootstrap) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (automake)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            (cd libburn; CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
            ) 1>$tmpFile 2>&1
            ;;
          windows*|mingw*)
            (cd libburn; CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread -lcrypt32 -lwsock32 -lws2_32" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
            ) 1>$tmpFile 2>&1
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd libburn; \
         make -j$CPU_CORES; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd libburn; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES burn"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES burn"
      fi
      HAVE_BURN=1
      AC_DEFINE(HAVE_BURN,1,[libburn installed (local)])
    fi
  else
    dnl search for installed burn library

    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(burn_disc_write,burn,[HAVE_BURN=1;LIBRARIES="burn $LIBRARIES";AC_DEFINE(HAVE_BURN,1,[libburn installed])])
    else
      if test -z "$HAVE_BURN"; then
        AC_SEARCH_STATIC_LIBRARIES(burn_disc_write,burn,[HAVE_BURN=1;STATIC_LIBRARIES="$STATIC_LIBRARIES burn";AC_DEFINE(HAVE_BURN,1,[libburn installed])])
      fi
      if test -z "$HAVE_BURN"; then
        AC_SEARCH_LIBRARIES(burn_disc_write,burn,[HAVE_BURN=1;LIBRARIES="burn $LIBRARIES";AC_DEFINE(HAVE_BURN,1,[libburn installed])])
      fi
    fi
  fi

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_BURN"; then
    if test ! \( -d libburn -o -d "`readlink libburn`" \); then
      AC_MSG_WARN([libburn not compiled: libburn package missing])
    fi
  fi
fi

# compile/use libmount
if test $ENABLE_MOUNT = "yes"; then
  if test -d util-linux -o -d "`readlink util-linux`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      dnl compile libmount from util-linux
      AC_MSG_COMPILE([libmount])
      result=1
      if test $result -eq 1; then
        (cd util-linux; ./autogen.sh) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (autogen)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        case $host_os in
          linux-* | sunos | solaris* | darwin*)
            (cd util-linux; CC="$CC" CPPFLAGS="" CFLAGS="" CXXFLAGS="" AS="$AS" LD="$LD" LDFLAGS="" LIBS="-lpthread" AR="$AR" RANLIB="$RANLIB" WINDRES="$WINDRES" ./configure \
              --host=$host \
              --build=$build \
              --prefix=$cwd/extern \
              --disable-all-programs --enable-libmount --enable-libblkid \
            ) 1>$tmpFile 2>&1
            ;;
          windows*|mingw*)
            ;;
        esac
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (configure)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd util-linux; \
         make -j$CPU_CORES; \
        ) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (make)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
      fi
      if test $result -eq 1; then
        (cd util-linux; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
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
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES mount blkid"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES mount blkid"
      fi
      HAVE_MOUNT=1
      AC_DEFINE(HAVE_MOUNT,1,[libmount installed (local)])
    fi
  else
    dnl search for installed libmount

    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(mnt_new_table_from_file,mount,[HAVE_MOUNT=1;LIBRARIES="mount blkid $LIBRARIES";AC_DEFINE(HAVE_MOUNT,1,[libisofs installed])])
    else
      if test -z "$HAVE_MOUNT"; then
        AC_SEARCH_STATIC_LIBRARIES(mnt_new_table_from_file,mount,[HAVE_MOUNT=1;STATIC_LIBRARIES="$STATIC_LIBRARIES mount blkid";AC_DEFINE(HAVE_MOUNT,1,[libisofs installed])])
      fi
      if test -z "$HAVE_MOUNT"; then
        AC_SEARCH_LIBRARIES(mnt_new_table_from_file,mount,[HAVE_MOUNT=1;LIBRARIES="mount blkid $LIBRARIES";AC_DEFINE(HAVE_MOUNT,1,[libisofs installed])])
      fi
    fi
  fi

  dnl link platform specific libraries
  case $host_os in
    linux-*)
      ;;
    sunos | solaris*)
      ;;
    darwin*)
      ;;
    windows*|mingw*)
      ;;
  esac

  if test -z "$HAVE_MOUNT"; then
    if test ! \( -d util-linux -o -d "`readlink util-linux`" \); then
      AC_MSG_WARN([libmount not compiled: util-linux package missing])
    fi
  fi
fi

# compile pthreads-w32
case $host_os in
  linux-*)
    ;;
  sunos | solaris*)
    ;;
  darwin*)
    ;;
  windows*|mingw*)
    if test $ENABLE_PTHREADS_W32 = "yes"; then
      if test -d pthreads-w32 -o -d "`readlink pthreads-w32`"; then
        if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
          cwd=`pwd`

          AC_MSG_COMPILE([pthreads-w32])
          result=1
          if test $result -eq 1; then
            # patch to fix warnings:
            #   diff -u pthreads-w32-2-9-1-release.org pthreads-w32-2-9-1-release > pthreads-w32-2-9-1-release.patch
#TODO            (cd pthreads-w32; $PATCH --batch -N -p1 < $SOURCE_DIR/misc/pthreads-w32-2-9-1-release.patch) 1>$tmpFile 2>&1
            if test $? -ne 0; then
              AC_MSG_RESULT([fail (patch)])
              AC_MSG_OUTPUT_LOG($tmpFile)
              result=0
            fi
          fi
          if test $result -eq 1; then
            if test "$ENABLE_DEBUG" = "yes"; then
              (cd pthreads-w32; make clean; make CROSS="$host-" GC-static-debug) 1>$tmpFile 2>&1
            else
              (cd pthreads-w32; make clean; make CROSS="$host-" GC-static) 1>$tmpFile 2>&1
            fi
            if test $? -ne 0; then
              AC_MSG_RESULT([fail (make)])
              AC_MSG_OUTPUT_LOG($tmpFile)
              result=0
            fi
          fi
          if test $result -eq 1; then
            (cd pthreads-w32; install -m 644 pthread.h sched.h semaphore.h $cwd/extern/include) 1>/dev/null
            (cd pthreads-w32; install -m 644 libpthreadGC2.a $cwd/extern/lib) 1>/dev/null
          fi
          if test $result -eq 1; then
            AC_MSG_RESULT([done])
          fi
        else
          result=1
        fi

        if test $result -eq 1; then
          CC_DEFINES="$CC_DEFINES PTW32_STATIC_LIB"
          if test $ENABLE_LINK_DYNAMIC = "yes"; then
            LIBRARIES="$LIBRARIES pthreadGC2"
          else
            STATIC_LIBRARIES="$STATIC_LIBRARIES pthreadGC2"
          fi
          HAVE_PTHREADS_W32=1
          AC_DEFINE(HAVE_PTHREADS_W32,1,[pthreads-w32 installed (local)])
        fi
      fi
    fi
    ;;
esac

# compile/use binutils
if test $ENABLE_BFD = "yes"; then
  if test -d binutils -o -d "`readlink binutils`"; then
    if test "$ENABLE_EXTERN_COMPILE" = "yes"; then
      cwd=`pwd`

      AC_MSG_COMPILE([bfd])
      result=1
      if test $result -eq 1; then
        # patch to fix memory leak
        #   diff -u binutils-2.32.org/bfd/dwarf2.c binutils-2.32/bfd/dwarf2.c > ../misc/binutils-2.32-dwarf2-leak.patch
#TODO        (cd extern/binutils-2.32; $PATCH --batch -N -p1 < $SOURCE_DIR/misc/binutils-2.32-dwarf2-leak.patch) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (patch)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi
        # Note: - fix distlean
        #       - use cpu+os to avoid "unknown" in name which cause
        #         using of wrong tool names by binutils build!
        (cd binutils; make distclean; find . -type f -name config.cache | xargs rm) 1>/dev/null 2>&1
#            --host=$build_cpu-$build_os

        if test "$ENABLE_DEBUG" = "yes"; then
          (cd binutils; CC="" CFLAGS="-g -O0" CXXFLAGS="-g -O0" LD="" LDFLAGS="" ./configure \
            --host=$host \
            --build=$build \
            --prefix=$cwd/extern \
            --disable-host-shared \
            --disable-ld \
          ) 1>$tmpFile 2>&1
        else
          (cd binutils; CC="" CFLAGS="" CXXFLAGS="" LD="" LDFLAGS="" ./configure \
            --host=$host \
            --build=$build \
            --prefix=$cwd/extern \
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
        (cd binutils; make install PREFIX=$cwd/extern) 1>$tmpFile 2>&1
        if test $? -ne 0; then
          AC_MSG_RESULT([fail (install)])
          AC_MSG_OUTPUT_LOG($tmpFile)
          result=0
        fi

        # fix incomplete install: does not install all header files?
        install -d $cwd/extern/include
        install -m 644 binutils/include/libiberty.h binutils/include/demangle.h $cwd/extern/include
        install -d $cwd/extern/lib
        install -m 644 binutils/libiberty/libiberty.a $cwd/extern/lib
      fi
      if test $result -eq 1; then
        AC_MSG_RESULT([done])
      fi
    else
      result=1
    fi

    if test $result -eq 1; then
      # PACKAGE required to make bfd.h happy
      CC_DEFINES="$CC_DEFINES PACKAGE"
      CC_INCLUDES="$CC_INCLUDES $cwd/extern/include"
      if test $ENABLE_LINK_DYNAMIC = "yes"; then
        LIBRARIES="$LIBRARIES bfd sframe iberty z"
      else
        STATIC_LIBRARIES="$STATIC_LIBRARIES bfd sframe iberty z"
      fi
      HAVE_BFD=1
      AC_DEFINE(HAVE_BFD,1,[libbfd installed (local)])

      AC_PATH_PROGS(DUMP_SYMS,dump_syms,,,$cwd/extern/bin)
    fi
  else
    dnl search for installed bfd, sframe, iberty libraries

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

# check mandatory extern software for full version
AC_ARG_ENABLE(
  extern-check,
  AC_HELP_STRING([--enable-extern-check],[enable check extern software]),
  [
   AS_IF(test $HAVE_ICU       ,,AC_MSG_ERROR([icu missing]))
   AS_IF(test $HAVE_BZ2       ,,AC_MSG_ERROR([bzip2 missing]))
   AS_IF(test $HAVE_LZMA      ,,AC_MSG_ERROR([lzma missing]))
   AS_IF(test $HAVE_LZO       ,,AC_MSG_ERROR([lzo missing]))
   AS_IF(test $HAVE_LZ4       ,,AC_MSG_ERROR([lz4 missing]))
   AS_IF(test $HAVE_ZSTD      ,,AC_MSG_ERROR([zstd missing]))
   AS_IF(test $HAVE_XDELTA3   ,,AC_MSG_ERROR([xdelta3 missing]))
   AS_IF(test $HAVE_GCRYPT    ,,AC_MSG_ERROR([gcrypt missing]))
   AS_IF(test $HAVE_FTP       ,,AC_MSG_ERROR([ftp missing]))
   AS_IF(test $HAVE_CARES     ,,AC_MSG_ERROR([c-ares missing]))
   AS_IF(test $HAVE_CURL      ,,AC_MSG_ERROR([curl missing]))
   AS_IF(test $HAVE_MXML      ,,AC_MSG_ERROR([mxml missing]))
   AS_IF(test $HAVE_SSL       ,,AC_MSG_ERROR([ssl missing]))
   AS_IF(test $HAVE_SSH2      ,,AC_MSG_ERROR([ssh2 missing]))
   AS_IF(test $HAVE_GNU_TLS   ,,AC_MSG_ERROR([GNU TLS missing]))
   AS_IF(test $HAVE_MARIADB   ,,AC_MSG_ERROR([MariaDB missing]))
   AS_IF(test $HAVE_POSTGRESQL,,AC_MSG_ERROR([PostgreSQL missing]))
   AS_IF(test $HAVE_ISO9660   ,,AC_MSG_ERROR([ISO9660 missing]))
   AS_IF(test $HAVE_SMB2      ,,AC_MSG_ERROR([smb2 missing]))
   AS_IF(test $HAVE_PCRE      ,,AC_MSG_ERROR([PCRE missing]))
   AS_IF(test $HAVE_PAR2      ,,AC_MSG_ERROR([PAR2 missing]))
   AS_IF(test $HAVE_ISOFS     ,,AC_MSG_ERROR([isofs missing]))
   AS_IF(test $HAVE_XFS       ,,AC_MSG_ERROR([xfs missing]))
   AS_IF(test $HAVE_BURN      ,,AC_MSG_ERROR([burn missing]))
   AS_IF(test $HAVE_MOUNT     ,,AC_MSG_ERROR([mount missing]))
  ]
)

dnl -------------- headers/functions/definitions/libraries -------------

# set toolchain settings for tests (Note: do not move ) at the end!)
AC_SET_TOOLCHAIN_SETTINGS($CPP_FLAGS,
                          $CC_FLAGS,
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
                 blkid/blkid.h \
                 demangle.h \
                 dlfcn.h \
                 elf.h \
                 execinfo.h \
                 grp.h \
                 icu.h \
                 libiberty/demangle.h \
                 libintl.h \
                 libmount/libmount.h \
                 libpq-fe.h \
                 link.h \
                 linux/fs.h \
                 linux/tcp.h \
                 mntent.h \
                 mariadb/mysql.h \
                 mariadb/errmsg.h \
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
                 sys/sysmacros.h \
                 sys/utsname.h \
                 sys/vfs.h \
                 sys/wait.h \
                 sys/xattr.h \
                 systemd/sd-daemon.h \
                 systemd/sd-id128.h \
                 termios.h \
                 unicode/utypes.h \
                 unicode/uregex.h \
                 unicode/ustring.h \
                 unicode/ucol.h \
                 uuid/uuid.h \
                 valgrind/callgrind.h
                )
AC_CHECK_CXX_HEADER(libpar2.h)
AC_CHECK_HEADER(sqlite3.h,,AC_ERROR([mandatory sqlite3.h is not available]))

# defines, constants, types, variables
AC_CHECK_TYPE(uint,AC_DEFINE(HAVE_UINT,1,[defined if uint is available]))
AC_CHECK_TYPE(ulong,AC_DEFINE(HAVE_ULONG,1,[defined if ulong is available]))
AC_CHECK_TYPE(long long int,AC_DEFINE(HAVE_LONG_LONG,1,[defined if ulong is available]))
dnl AC_C_LONG_LONG(AC_DEFINE(HAVE_LONG_LONG,1,[defined if long long data type available]),AC_MSG_ERROR([long long data type not available]))
AC_TRY_COMPILE([#include <limits.h>],[long long int x=LLONG_MIN;],AC_DEFINE(HAVE_LLONG_MIN,1,[defined if LLONG_MIN is available]))
AC_TRY_COMPILE([#include <limits.h>],[long long int x=LLONG_MAX;],AC_DEFINE(HAVE_LLONG_MAX,1,[defined if LLONG_MAX is available]))
AC_TRY_COMPILE([#include <limits.h>],[unsigned long long int x=ULLONG_MAX;],AC_DEFINE(HAVE_ULLONG_MAX,1,[defined if ULLONG_MAX is available]))
AC_CHECK_VARIABLE_ATTRIBUTE(__thread,AC_DEFINE(HAVE___THREAD,1,[defined if __thread is available]))
dnl Note: AC_SYS_LARGEFILE() does not work with gcc? Use my own implementation
AC_CHECK_LARGEFILE_DEFINES()
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_SIZEOF(unsigned long long)
AC_CHECK_SIZEOF(void *)
AC_CHECK_SIZEOF(size_t)
AC_CHECK_STATIC_ASSERT()

if test $ENABLE_LINK_DYNAMIC = "yes"; then
  AC_SEARCH_LIBRARIES(uuid_generate,uuid,[LIBRARIES="$LIBRARIES uuid"],,uuid/uuid.h)
else
  AC_SEARCH_STATIC_LIBRARIES(uuid_generate,uuid,[STATIC_LIBRARIES="$STATIC_LIBRARIES uuid"],[],uuid/uuid.h)
fi
case $host_os in
  linux-*)
    if test $ENABLE_LINK_DYNAMIC = "yes"; then
      AC_SEARCH_LIBRARIES(idn_free,idn2,[LIBRARIES="$LIBRARIES idn2"],,idn-free.h)
    else
      AC_SEARCH_STATIC_LIBRARIES(idn_free,idn2,[STATIC_LIBRARIES="$STATIC_LIBRARIES idn2"],,idn-free.h)
    fi
    ;;
  sunos | solaris*)
    ;;
  darwin*)
    ;;
  windows*|mingw*)
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

# set toolchain settings for tests (Note: do not move ) at the end!)
AC_SET_TOOLCHAIN_SETTINGS($CPP_FLAGS,
                          $CC_FLAGS,
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
AC_CHECK_CONSTANT(SIGBUS,         AC_DEFINE(HAVE_SIGBUS,         1,[SIGBUS available]),         ,signal.h)
AC_CHECK_CONSTANT(SIGQUIT,        AC_DEFINE(HAVE_SIGQUIT,        1,[SIGQUIT available]),        ,signal.h)
AC_CHECK_CONSTANT(SIGALRM,        AC_DEFINE(HAVE_SIGALRM,        1,[SIGALRM available]),        ,signal.h)
AC_CHECK_CONSTANT(SIGUSR1,        AC_DEFINE(HAVE_SIGUSR1,        1,[SIGUSR1 available]),        ,signal.h)
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
AC_CHECK_CONSTANT(O_LARGEFILE,    AC_DEFINE(HAVE_O_LARGEFILE,    1,[O_LARGEFILE available]),    ,sys/types.h sys/stat.h fcntl.h)
AC_CHECK_CONSTANT(O_DIRECTORY,    AC_DEFINE(HAVE_O_DIRECTORY,    1,[O_DIRECTORY available]),    ,sys/types.h sys/stat.h fcntl.h)
AC_CHECK_CONSTANT(O_NOATIME,      AC_DEFINE(HAVE_O_NOATIME,      1,[O_NOATIME available]),      ,sys/types.h sys/stat.h fcntl.h)
AC_CHECK_CONSTANT(O_NOCTTY,       AC_DEFINE(HAVE_O_NOCTTY,       1,[O_NOCTTY available]),       ,sys/types.h sys/stat.h fcntl.h)
AC_CHECK_CONSTANT(O_NONBLOCK,     AC_DEFINE(HAVE_O_NONBLOCK,     1,[O_NONBLOCK available]),     ,sys/types.h sys/stat.h fcntl.h)
AC_CHECK_CONSTANT(MSG_NOSIGNAL,   AC_DEFINE(HAVE_MSG_NOSIGNAL,   1,[MSG_NOSIGNAL available]),   ,sys/types.h sys/socket.h)
AC_CHECK_CONSTANT(TCP_NODELAY,    AC_DEFINE(HAVE_TCP_NODELAY,    1,[TCP_NODELAY available]),    ,netinet/tcp.h)

AC_CHECK_CONSTANT(SIGPIPE,AC_DEFINE(HAVE_SIGPIPE,1,[SIGPIPE available]),,signal.h)

AC_CHECK_CONSTANT(_SC_GETPW_R_SIZE_MAX,AC_DEFINE(HAVE__SC_GETPW_R_SIZE_MAX,1,[_SC_GETPW_R_SIZE_MAX available]),,unistd.h limits.h)
AC_CHECK_CONSTANT(_SC_NPROCESSORS_CONF,AC_DEFINE(HAVE__SC_NPROCESSORS_CONF,1,[_SC_NPROCESSORS_CONF available]),,unistd.h limits.h)

if test $HAVE_SSH2; then
  AC_CHECK_CONSTANT(CRYPTO_LOCK,,AC_ERROR([constant CRYPTO_LOCK is not available]),openssl/crypto.h)
fi

AC_CHECK_CONSTANT(IN_EXCL_UNLINK,AC_DEFINE(HAVE_IN_EXCL_UNLINK,1,[IN_EXCL_UNLINK available]),,sys/inotify.h)

AC_CHECK_CONSTANT(CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,AC_DEFINE(HAVE_CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,1,[CURLINFO_CONTENT_LENGTH_DOWNLOAD_T available]),,curl/curl.h)

AC_CHECK_CONSTANT(AFS_SUPER_MAGIC, AC_DEFINE(HAVE_AFS_SUPER_MAGIC, 1,[AFS_SUPER_MAGIC available]), ,linux/magic.h)
AC_CHECK_CONSTANT(CODA_SUPER_MAGIC,AC_DEFINE(HAVE_CODA_SUPER_MAGIC,1,[CODA_SUPER_MAGIC available]),,linux/magic.h)
AC_CHECK_CONSTANT(NFS_SUPER_MAGIC, AC_DEFINE(HAVE_NFS_SUPER_MAGIC, 1,[NFS_SUPER_MAGIC available]), ,linux/magic.h)
AC_CHECK_CONSTANT(SMB_SUPER_MAGIC, AC_DEFINE(HAVE_SMB_SUPER_MAGIC, 1,[SMB_SUPER_MAGIC available]), ,linux/magic.h)

# check types
AC_CHECK_TYPE(struct stat64,  AC_DEFINE(HAVE_STRUCT_STAT64,   1,[defined if struct stat64 is available]),  ,[#include <sys/stat.h>])
AC_CHECK_TYPE(struct __stat64,AC_DEFINE(HAVE_STRUCT___STAT64, 1,[defined if struct __stat64 is available]),,[#include <sys/stat.h>])
AC_CHECK_TYPE(struct _stati64,AC_DEFINE(HAVE_STRUCT__STATI64, 1,[defined if struct _stati64 is available]),,[#include <sys/stat.h>])

AC_CHECK_TYPE(sigset_t, AC_DEFINE(HAVE_SIGSET_T, 1,[defined if sigset_t is available]), ,[#include <signal.h>]  )
AC_CHECK_TYPE(siginfo_t,AC_DEFINE(HAVE_SIGINFO_T,1,[defined if siginfo_t is available]),,[#include <signal.h>]  )

AC_CHECK_STRUCT_MEMBER(struct stat,st_atim.tv_nsec,AC_DEFINE(HAVE_STAT_ATIM_TV_NSEC,1,[defined if struct stat.st_atim.tv_nsec is available]),,[sys/types.h sys/stat.h fcntl.h])
AC_CHECK_STRUCT_MEMBER(struct stat,st_mtim.tv_nsec,AC_DEFINE(HAVE_STAT_MTIM_TV_NSEC,1,[defined if struct stat.st_mtim.tv_nsec is available]),,[sys/types.h sys/stat.h fcntl.h])
AC_CHECK_STRUCT_MEMBER(struct stat,st_ctim.tv_nsec,AC_DEFINE(HAVE_STAT_CTIM_TV_NSEC,1,[defined if struct stat.st_ctim.tv_nsec is available]),,[sys/types.h sys/stat.h fcntl.h])

# check functions
AC_CHECK_FUNCTION(pthread_attr_setname,AC_DEFINE(HAVE_PTHREAD_ATTR_SETNAME,1,[pthread_attr_setname() available]))
AC_CHECK_FUNCTION(pthread_setname_np,  AC_DEFINE(HAVE_PTHREAD_SETNAME_NP,  1,[pthread_setname_np() available]  ))
AC_CHECK_FUNCTION(pthread_setschedprio,AC_DEFINE(HAVE_PTHREAD_SETSCHEDPRIO,1,[pthread_setschedprio() available]))

AC_CHECK_FUNCTION(gethostbyaddr,  AC_DEFINE(HAVE_GETHOSTBYADDR,  1,[gethostbyaddr() available])  ,,winsock2.h)
AC_CHECK_FUNCTION(gethostbyaddr_r,AC_DEFINE(HAVE_GETHOSTBYADDR_R,1,[gethostbyaddr_r() available]),,winsock2.h)
AC_CHECK_FUNCTION(gethostbyname,  AC_DEFINE(HAVE_GETHOSTBYNAME,  1,[gethostbyname() available])  ,,winsock2.h)
AC_CHECK_FUNCTION(gethostbyname_r,AC_DEFINE(HAVE_GETHOSTBYNAME_R,1,[gethostbyname_r() available]),,winsock2.h)

AC_CHECK_FUNCTION(getlogin,  AC_DEFINE(HAVE_GETLOGIN,  1,[getlogin() available]))
AC_CHECK_FUNCTION(getlogin_r,AC_DEFINE(HAVE_GETLOGIN_R,1,[getlogin_r() available]))

AC_CHECK_FUNCTION(chmod,               AC_DEFINE(HAVE_CHMOD,               1,[chmod() available]))
AC_CHECK_FUNCTION(chown,               AC_DEFINE(HAVE_CHOWN,               1,[chown() available]))
AC_CHECK_FUNCTION(lseek64,             AC_DEFINE(HAVE_LSEEK64,             1,[lseek64() available]))
AC_CHECK_FUNCTION(lseek,               AC_DEFINE(HAVE_LSEEK,               1,[lseek() available]))
AC_CHECK_FUNCTION(ftruncate64,         AC_DEFINE(HAVE_FTRUNCATE64,         1,[ftruncate64() available]))
AC_CHECK_FUNCTION(ftruncate,           AC_DEFINE(HAVE_FTRUNCATE,           1,[ftruncate() available]))
AC_CHECK_FUNCTION(fdatasync,           AC_DEFINE(HAVE_FDATASYNC,           1,[fdatasync() available]))
AC_CHECK_FUNCTION(fopen,               AC_DEFINE(HAVE_FOPEN,               1,[fopen() available]))
AC_CHECK_FUNCTION(fopen64,             AC_DEFINE(HAVE_FOPNE64,             1,[fopen64() available]))
AC_CHECK_FUNCTION(fseeko,              AC_DEFINE(HAVE_FSEEKO,              1,[fseeko() available]))
AC_CHECK_FUNCTION(_fseeki64,           AC_DEFINE(HAVE__FSEEKI64,           1,[_fseeki64() available]))
AC_CHECK_FUNCTION(fseeko64,            AC_DEFINE(HAVE_FSEEKO64,            1,[fseeko64() available]))
AC_CHECK_FUNCTION(ftello,              AC_DEFINE(HAVE_FTELLO,              1,[ftello() available]))
AC_CHECK_FUNCTION(_ftelli64,           AC_DEFINE(HAVE__FTELLI64,           1,[_ftelli64() available]))
AC_CHECK_FUNCTION(ftello64,            AC_DEFINE(HAVE_FTELLO64,            1,[ftello64() available]))
AC_CHECK_FUNCTION(getgrgid_r,          AC_DEFINE(HAVE_GETGRGID_R,          1,[getgrgid_r() available]))
AC_CHECK_FUNCTION(getgrnam_r,          AC_DEFINE(HAVE_GETGRNAM_R,          1,[getgrnam_r() available]))
AC_CHECK_FUNCTION(getpwnam_r,          AC_DEFINE(HAVE_GETPWNAM_R,          1,[getpwnam_r() available]))
AC_CHECK_FUNCTION(getpwuid_r,          AC_DEFINE(HAVE_GETPWUID_R,          1,[getpwuid_r() available]))
AC_CHECK_FUNCTION(get_current_dir_name,AC_DEFINE(HAVE_GET_CURRENT_DIR_NAME,1,[get_current_dir_name() available]))
AC_CHECK_FUNCTION(ioctl,               AC_DEFINE(HAVE_IOCTL,               1,[ioctl() available]))
AC_CHECK_FUNCTION(lchmod,              AC_DEFINE(HAVE_LCHMOD,              1,[lchmod() available]))
AC_CHECK_FUNCTION(link,                AC_DEFINE(HAVE_LINK,                1,[link() available]))
AC_CHECK_FUNCTION(lstat64,             AC_DEFINE(HAVE_LSTAT64,             1,[lstat64() available]))
AC_CHECK_FUNCTION(lstat,               AC_DEFINE(HAVE_LSTAT,               1,[lstat() available]))
AC_CHECK_FUNCTION(major,               AC_DEFINE(HAVE_MAJOR,               1,[major() available]),,sys/sysmacros.h)
AC_CHECK_FUNCTION(makedev,             AC_DEFINE(HAVE_MAKEDEV,             1,[makedev() available]),,sys/sysmacros.h)
AC_CHECK_FUNCTION(minor,               AC_DEFINE(HAVE_MINOR,               1,[minor() available]),,sys/sysmacros.h)
AC_CHECK_FUNCTION(mknod,               AC_DEFINE(HAVE_MKNOD,               1,[mknod() available]))
AC_CHECK_FUNCTION(posix_fadvise,       AC_DEFINE(HAVE_POSIX_FADVISE,       1,[posix_fadvise() available]))
AC_CHECK_FUNCTION(readlink,            AC_DEFINE(HAVE_READLINK,            1,[readlink() available]))
AC_CHECK_FUNCTION(sigaction,           AC_DEFINE(HAVE_SIGACTION,           1,[sigaction() available]))
AC_CHECK_FUNCTION(stat64,              AC_DEFINE(HAVE_STAT64,              1,[stat64() available]))
AC_CHECK_FUNCTION(stat,                AC_DEFINE(HAVE_STAT,                1,[stat() available]))
AC_CHECK_FUNCTION(_stati64,            AC_DEFINE(HAVE__STATI64,            1,[_stati64() available]))
AC_CHECK_FUNCTION(statfs,              AC_DEFINE(HAVE_STATFS,              1,[statfs() available]))
AC_CHECK_FUNCTION(statvfs,             AC_DEFINE(HAVE_STATVFS,             1,[statvfs() available]))
AC_CHECK_FUNCTION(symlink,             AC_DEFINE(HAVE_SYMLINK,             1,[symlink() available]))
AC_CHECK_FUNCTION(futimens,            AC_DEFINE(HAVE_FUTIMENSK,           1,[futimens() available]))
AC_CHECK_FUNCTION(fdopendir,           AC_DEFINE(HAVE_FDOPENDIR,           1,[fdopendir() available]),,sys/types.h dirent.h)
AC_CHECK_FUNCTION(mkdtemp,             AC_DEFINE(HAVE_MKDTEMP,             1,[mkdtemp() available]),,io.h)
AC_CHECK_FUNCTION(mkstemp,             AC_DEFINE(HAVE_MKSTEMP,             1,[mkstemp() available]),,io.h)
AC_CHECK_FUNCTION(mktemp,              AC_DEFINE(HAVE_MKTEMP,              1,[mktemp() available]) ,,io.h)
AC_CHECK_FUNCTION(llistxattr,          AC_DEFINE(HAVE_LLISTXATTR,          1,[llistxattr() available]) ,,sys/xattr.h)
AC_CHECK_FUNCTION(lgetxattr,           AC_DEFINE(HAVE_LGETXATTR,           1,[lgetxattr() available]) ,,sys/xattr.h)
AC_CHECK_FUNCTION(lsetxattr,           AC_DEFINE(HAVE_LSETXATTR,           1,[lsetxattr() available]) ,,sys/xattr.h)
AC_CHECK_FUNCTION(select,              AC_DEFINE(HAVE_SELECT,              1,[select() available]) ,,sys/select.h winsock2.h)
AC_CHECK_FUNCTION(pselect,             AC_DEFINE(HAVE_PSELECT,             1,[pselect() available]) ,,sys/select.h winsock2.h)
AC_CHECK_FUNCTION(signal,              AC_DEFINE(HAVE_SIGNAL,              1,[signal() available]) ,,signal.h)
AC_CHECK_FUNCTION(kill,                AC_DEFINE(HAVE_KILL,                1,[kill() available]) ,,signal.h)
AC_CHECK_MKDIR_ARGUMENTS(,AC_MSG_ERROR([cannot detect number of arguments of mkdir()]))

AC_CHECK_FUNCTION(u_init,AC_DEFINE(HAVE_U_INIT,1,[u_init() available]),,icu.h)

AC_CHECK_FUNCTION(inotify_init1,    AC_DEFINE(HAVE_INOTIFY_INIT1,    1,[inotify_init1() available]))
AC_CHECK_FUNCTION(inotify_add_watch,AC_DEFINE(HAVE_INOTIFY_ADD_WATCH,1,[inotify_add_watch() available]))
AC_CHECK_FUNCTION(inotify_rm_watch, AC_DEFINE(HAVE_INOTIFY_RM_WATCH, 1,[inotify_rm_watch() available]))

AC_CHECK_FUNCTION(gmtime_r,   AC_DEFINE(HAVE_GMTIME_R,   1,[gmtime_r() available]))
AC_CHECK_FUNCTION(localtime_r,AC_DEFINE(HAVE_LOCALTIME_R,1,[localtime_r() available]))
AC_CHECK_FUNCTION(getdate_r,  AC_DEFINE(HAVE_GETDATE_R,  1,[getdate_r() available]))
#AC_CHECK_FUNCTION(usleep,     [HAVE_USLEEP=1; AC_DEFINE(HAVE_USLEEP,1,[usleep() available])])
AC_CHECK_FUNCTION(usleep,     AC_DEFINE(HAVE_USLEEP,     1,[usleep() available])])
AC_CHECK_FUNCTION(nanosleep,  AC_DEFINE(HAVE_NANOSLEEP,  1,[nanosleep() available]))
AC_CHECK_FUNCTION(timegm,     AC_DEFINE(HAVE_TIMEGM,     1,[timegm() available]))
AC_CHECK_FUNCTION(strptime,   AC_DEFINE(HAVE_STRPTIME,   1,[strptime() available]))

AC_CHECK_FUNCTION(pipe,   AC_DEFINE(HAVE_PIPE,   1,[pipe() available]))
AC_CHECK_FUNCTION(fork,   AC_DEFINE(HAVE_FORK,   1,[fork() available]))
AC_CHECK_FUNCTION(waitpid,AC_DEFINE(HAVE_WAITPID,1,[waitpid() available]))

AC_CHECK_FUNCTION(zlibVersion,,AC_ERROR([mandatory function zlibVersion() is not available]),zlib.h)

AC_CHECK_FUNCTION(ZSTD_CCtx_reset,AC_DEFINE(HAVE_ZSTD_CCTX_RESET,1,[SSH2 has libssh2_keepalive_config()]))

AC_CHECK_FUNCTION(libssh2_userauth_publickey_frommemory,AC_DEFINE(HAVE_SSH2_USERAUTH_PUBLICKEY_FROMMEMORY,1,[SSH2 has libssh2_userauth_publickey_frommemory() function]))
AC_CHECK_FUNCTION(libssh2_channel_send_keepalive,       AC_DEFINE(HAVE_SSH2_CHANNEL_SEND_KEEPALIVE,       1,[SSH2 has libssh2_channel_send_keepalive()]))
AC_CHECK_FUNCTION(libssh2_keepalive_config,             AC_DEFINE(HAVE_SSH2_KEEPALIVE_CONFIG,             1,[SSH2 has libssh2_keepalive_config()]))
AC_CHECK_FUNCTION(libssh2_keepalive_send,               AC_DEFINE(HAVE_SSH2_KEEPALIVE_SEND,               1,[SSH2 has libssh2_keepalive_send()]))
AC_CHECK_FUNCTION(libssh2_scp_send64,                   AC_DEFINE(HAVE_SSH2_SCP_SEND64,                   1,[SSH2 has libssh2_scp_send64() function]))
AC_CHECK_FUNCTION(libssh2_sftp_seek64,                  AC_DEFINE(HAVE_SSH2_SFTP_SEEK64,                  1,[SSH2 has libssh2_sftp_seek64() function]))
AC_CHECK_FUNCTION(libssh2_sftp_seek2,                   AC_DEFINE(HAVE_SSH2_SFTP_SEEK2,                   1,[SSH2 has libssh2_sftp_seek2() function]))

AC_CHECK_FUNCTION(SHA256_Init,                          AC_DEFINE(HAVE_SHA256_INIT,                       1,[OpenSSL has SHA256_Init() function]))
AC_CHECK_FUNCTION(SHA256_Final,                         AC_DEFINE(HAVE_SHA256_FINAL,                      1,[OpenSSL has SHA256_Final() function]))
AC_CHECK_FUNCTION(SHA256_Update,                        AC_DEFINE(HAVE_SHA256_UPDATE,                     1,[OpenSSL has SHA256_Update() function]))

AC_CHECK_FUNCTION(gcry_md_open,                         AC_DEFINE(HAVE_GCRY_MD_OPEN,                      1,[libgcrypt has gcry_md_open() function]))
AC_CHECK_FUNCTION(gcry_md_close,                        AC_DEFINE(HAVE_GCRY_MD_CLOSE,                     1,[libgcrypt has gcry_md_close() function]))
AC_CHECK_FUNCTION(gcry_md_write,                        AC_DEFINE(HAVE_GCRY_MD_WRITE,                     1,[libgcrypt has gcry_md_write() function]))

AC_CHECK_FUNCTION(iso9660_open,              AC_DEFINE(HAVE_ISO9660_OPEN,              1,[iso9660_open() available]))
AC_CHECK_FUNCTION(iso9660_close,             AC_DEFINE(HAVE_ISO9660_CLOSE,             1,[iso9660_close() available]))
AC_CHECK_FUNCTION(iso9660_ifs_stat_translate,AC_DEFINE(HAVE_ISO9660_IFS_STAT_TRANSLATE,1,[iso9660_ifs_stat_translate() available]))
AC_CHECK_FUNCTION(iso9660_iso_seek_read,     AC_DEFINE(HAVE_ISO9660_ISO_SEEK_READ,     1,[iso9660_iso_seek_read() available]))

AC_CHECK_FUNCTION(smb2_init_context,      AC_DEFINE(HAVE_SMB2_INIT_CONTEXT,1,[smb2_init_context() available]))
AC_CHECK_FUNCTION(smb2_done_context,      AC_DEFINE(HAVE_SMB2_DONE_CONTEXT,1,[smb2_done_context() available]))
AC_CHECK_FUNCTION(smb2_parse_url,         AC_DEFINE(HAVE_SMB2_PARSE_URL,   1,[smb2_parse_url() available]))
AC_CHECK_FUNCTION(smb2_opendir,           AC_DEFINE(HAVE_OPENDIR,          1,[smb2_opendir() available]))

AC_CHECK_FUNCTION(uuid_generate,     AC_DEFINE(HAVE_UUID_GENERATE,     1,[uuid_generate() available]))
AC_CHECK_FUNCTION(uuid_parse,        AC_DEFINE(HAVE_UUID_PARSE,        1,[uuid_parse() available]))
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

# Note: these functions around bound by daynmic lazy loading to avoid dependency to libsystemd.so
#AC_CHECK_FUNCTION(sd_notify,                        AC_DEFINE(HAVE_SD_NOTIFY,                        1,[sd_notify() available]),,systemd/sd-daemon.h)
#AC_CHECK_FUNCTION(sd_notifyf,                       AC_DEFINE(HAVE_SD_NOTIFYF,                       1,[sd_notifyf() available]),,systemd/sd-daemon.h)
#AC_CHECK_FUNCTION(sd_notify_barrier,                AC_DEFINE(HAVE_SD_NOTIFY_BARRIER,                1,[sd_notify_barrier() available]),,systemd/sd-daemon.h)
#AC_CHECK_FUNCTION(sd_id128_get_machine,             AC_DEFINE(HAVE_SD_ID128_GET_MACHINE,             1,[sd_id128_get_machine() available]),,systemd/sd-id128.h)
#AC_CHECK_FUNCTION(sd_id128_get_machine_app_specific,AC_DEFINE(HAVE_SD_ID128_GET_MACHINE_APP_SPECIFIC,1,[sd_id128_get_machine_app_specific() available]),,systemd/sd-id128.h)

AC_CHECK_FUNCTION(mnt_new_table_from_file,AC_DEFINE(HAVE_MNT_NEW_TABLE_FROM_FILE,1,[mnt_new_table_from_file() available]),,libmount.h)
AC_CHECK_FUNCTION(mnt_context_mount,AC_DEFINE(HAVE_MNT_CONTEXT_MOUNT,1,[mnt_context_mount() available]),,libmount.h)

AC_CHECK_FUNCTION(blkid_new_probe,         AC_DEFINE(HAVE_BLKID_NEW,               1,[blkid_new_probe() available]),,blkid/blkid.h)
AC_CHECK_FUNCTION(blkid_do_probe,          AC_DEFINE(HAVE_BLKID_DO_PROBE,          1,[blkid_do_probe() available]),,blkid/blkid.h)
AC_CHECK_FUNCTION(blkid_probe_lookup_value,AC_DEFINE(HAVE_BLKID_PROBE_LOOKUP_VALUE,1,[blkid_probe_lookup_value() available]),,blkid/blkid.h)

AC_CHECK_FUNCTION(bfd_init,AC_DEFINE(HAVE_BFD_INIT,1,[bfd_init() available]),,bfd.h)
AC_CHECK_FUNCTION(bfd_get_section_flags,AC_DEFINE(HAVE_BFD_GET_SECTION_FLAGS,1,[bfd_get_section_flags() available]),,bfd.h)
AC_CHECK_FUNCTION(bfd_section_flags,AC_DEFINE(HAVE_BFD_SECTION_FLAGS,1,[bfd_section_flags() available]),,bfd.h)
if test $HAVE_BFD; then
  AC_CHECK_BFD_SECTION_VMA_ARGUMENTS(,AC_MSG_ERROR([cannot detect number of arguments of bfd_section_vma()]))
  AC_CHECK_BFD_SECTION_SIZE_ARGUMENTS(,AC_MSG_ERROR([cannot detect number of arguments of bfd_section_size()]))
fi

case $host_os in
  linux-*)
    ;;
  sunos | solaris*)
    ;;
  darwin*)
    ;;
  windows*|mingw*)
    AC_CHECK_FUNCTION(WSAPoll,AC_DEFINE(HAVE_WSAPOLL,1,[WSAPoll() available]),,winsock2.h)
    ;;
esac

dnl -------------------------------- misc ------------------------------

VERSION_MAJOR=`cat $SOURCE_DIR/version|grep MAJOR|sed 's/.*=//g'`
VERSION_MINOR=`cat $SOURCE_DIR/version|grep MINOR|sed 's/.*=//g'`
VERSION_PATCH=`cat $SOURCE_DIR/version|grep PATCH|sed 's/.*=//g'`
VERSION_REVISION=`$SVNVERSION . 2>/dev/null || cat $SOURCE_DIR/version|grep RELEASE|sed 's/RELEASE=//g'`

COPYRIGHT_DATE="2007-`date +%Y`"

AC_DEFINE_UNQUOTED(FILE_SEPARATOR_CHAR,  '$FILE_SEPARATOR_CHAR',[file separator char])
AC_DEFINE_UNQUOTED(FILE_SEPARATOR_STRING,"$FILE_SEPARATOR_CHAR",[file separator string])

AC_DEFINE_UNQUOTED(CONFIG_DIR, "$CONFIG_DIR", [configuration directory])
AC_DEFINE_UNQUOTED(RUNTIME_DIR,"$RUNTIME_DIR",[runtime directory])
AC_DEFINE_UNQUOTED(TLS_DIR,    "$TLS_DIR",    [TLS directory])

AS_IF([test "$ENABLE_DEBUG"                       = yes],[AC_DEFINE_UNQUOTED(ENABLE_DEBUG,                      "1",[enabled debug functions])]            )
AS_IF([test "$ENABLE_DEBUG_THREAD_CRASH_HANDLERS" = yes],[AC_DEFINE_UNQUOTED(ENABLE_DEBUG_THREAD_CRASH_HANDLERS,"1",[enabled debug thread crash handlers])])

# get and check version numbers (Note: keep closing ) at the end!)
AC_CONSTANT_VALUE([SQLite version],
                  VERSION_SQLITE,
                  [#include "sqlite3.h"
                  ],
                  [SQLITE_VERSION],
                  [],
                  [ "]
                 )
AC_CONSTANT_VALUE([],
                  VERSION_SQLITE_NUMBER,
                  [#include "sqlite3.h"
                  ],
                  [SQLITE_VERSION_NUMBER]
                  []
                 )
if test $VERSION_SQLITE_NUMBER -lt 3038000; then
  AC_ERROR([required SQLite 3.38 (3038000) or newer - found $VERSION_SQLITE])
fi
AC_DEFINE_UNQUOTED(VERSION_SQLITE,"$VERSION_SQLITE",[sqlite version])

AC_CONSTANT_VALUE([ICU version],
                  VERSION_ICU,
                  [#include "unicode/uvernum.h"
                  ],
                  [U_ICU_VERSION],
                  [],
                  [ "]
                 )
AC_DEFINE_UNQUOTED(VERSION_ICU,"$VERSION_ICU",[icu version])

AC_CONSTANT_VALUE([zlib version],
                  VERSION_Z,
                  [#include "zlib.h"
                  ],
                  [ZLIB_VERSION],
                  [],
                  [ "]
                 )
AC_DEFINE_UNQUOTED(VERSION_Z,"$VERSION_Z",[zlib version])

if test "$HAVE_BZ2"; then
  # Note: bz2 have a define for version on in a private header file
  AC_CONSTANT_VALUE([bz2 version],
                    VERSION_BZ2,
                    [#include "bzlib_private.h"
                    ],
                    [BZ_VERSION],
                    [bzip2],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_BZ2,"$VERSION_BZ2",[bz2 version])
else
  AC_DEFINE_UNQUOTED(VERSION_BZ2,"",[bz2 version])
fi

if test "$HAVE_LZMA"; then
  AC_CONSTANT_VALUE([lzma version],
                    VERSION_LZMA,
                    [#include "lzma.h"
                    ],
                    [LZMA_VERSION_MAJOR.LZMA_VERSION_MINOR.LZMA_VERSION_PATCH],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_LZMA,"$VERSION_LZMA",[lzma version])
else
  AC_DEFINE_UNQUOTED(VERSION_LZMA,"",[lzma version])
fi

if test "$HAVE_LZO"; then
  AC_CONSTANT_VALUE([lzo version],
                    VERSION_LZO,
                    [#include "lzo/lzoconf.h"
                    ],
                    [LZO_VERSION_STRING],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_LZO,"$VERSION_LZO",[lzo version])
else
  AC_DEFINE_UNQUOTED(VERSION_LZO,"",[lzo version])
fi

if test "$HAVE_LZ4"; then
  AC_CONSTANT_VALUE([lz4 version],
                    VERSION_LZ4,
                    [#include "lz4.h"
                    ],
                    [LZ4_VERSION_MAJOR.LZ4_VERSION_MINOR.LZ4_VERSION_RELEASE],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_LZ4,"$VERSION_LZ4",[lz4 version])
else
  AC_DEFINE_UNQUOTED(VERSION_LZ4,"",[lz4 version])
fi

if test "$HAVE_ZSTD"; then
  AC_CONSTANT_VALUE([zstd version],
                    VERSION_ZSTD,
                    [#include "zstd.h"
                    ],
                    [ZSTD_VERSION_MAJOR.ZSTD_VERSION_MINOR.ZSTD_VERSION_RELEASE],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_ZSTD,"$VERSION_ZSTD",[zstd version])
else
  AC_DEFINE_UNQUOTED(VERSION_ZSTD,"",[zstd version])
fi

if test "$HAVE_XDELTA3"; then
  # Note: xdelta3 does not have a define for version
# TODO: version
  VERSION_XDELTA3="3.0.11"
else
  VERSION_XDELTA3=""
fi
AC_DEFINE_UNQUOTED(VERSION_XDELTA3,"$VERSION_XDELTA3",[xdelta3 version])

if test "$HAVE_GCRYPT"; then
  AC_CONSTANT_VALUE([gcrypt version],
                    VERSION_GCRYPT,
                    [#include "gcrypt.h"
                    ],
                    [GCRYPT_VERSION],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_GCRYPT,"$VERSION_GCRYPT",[gcrypt version])
  AC_CONSTANT_VALUE([GMP version],
                    VERSION_GMP,
                    [#include "gmp.h"
                    ],
                    [__GNU_MP_VERSION.__GNU_MP_VERSION_MINOR.__GNU_MP_VERSION_PATCHLEVEL],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_GMP,"$VERSION_GMP",[gmp version])
  AC_CONSTANT_VALUE([GNU TLS version],
                    VERSION_GNUTLS,
                    [#include "gnutls/gnutls.h"
                    ],
                    [GNUTLS_VERSION],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_GNUTLS,"$VERSION_GNUTLS",[gnuTLS version])
  AC_CONSTANT_VALUE([openssl version],
                    VERSION_OPENSSL,
                    [#include "openssl/opensslv.h"
                    ],
                    [OPENSSL_VERSION_TEXT],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_OPENSSL,"$VERSION_OPENSSL",[openssl version])
else
  AC_DEFINE_UNQUOTED(VERSION_GCRYPT, "",[VERSION_GCRYPT version])
  AC_DEFINE_UNQUOTED(VERSION_GMP,    "",[gmp version])
  AC_DEFINE_UNQUOTED(VERSION_GNUTLS, "",[gnuTLS version])
  AC_DEFINE_UNQUOTED(VERSION_OPENSSL,"",[openssl version])
fi

if test "$HAVE_SSH2"; then
  AC_CONSTANT_VALUE([ssh2 version],
                    VERSION_SSH2,
                    [#include "libssh2.h"
                    ],
                    [LIBSSH2_VERSION],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_SSH2,"$VERSION_SSH2",[libssh version])
else
  AC_DEFINE_UNQUOTED(VERSION_SSH2,"",[libssh version])
fi

if test "$HAVE_CURL"; then
  AC_CONSTANT_VALUE([curl version],
                    VERSION_CURL,
                    [#include "curl/curl.h"
                    ],
                    [LIBCURL_VERSION],
                    [],
                    [ "]
                   )
  if test -n "$VERSION_CURL" -a "$VERSION_CURL" \< "7.77"; then
    AC_ERROR([required libcurl 7.77 (0x077700) or newer - found '$VERSION_CURL'])
  fi
  AC_DEFINE_UNQUOTED(VERSION_CURL,"$VERSION_CURL",[curl version])
else
  AC_DEFINE_UNQUOTED(VERSION_CURL,"",[curl version])
fi

if test "$HAVE_ISO9660"; then
  AC_CONSTANT_VALUE([cdio version],
                    VERSION_CDIO,
                    [#include "cdio/cdio.h"
                    ],
                    [CDIO_VERSION],
                    [],
                    [ "]
                   )
  AC_CONSTANT_VALUE([],
                    VERSION_CDIO_NUMBER,
                    [#include "cdio/cdio.h"
                    ],
                    [LIBCDIO_VERSION_NUM],
                    [],
                    [ "]
                   )

  if test $VERSION_CDIO_NUMBER -lt 10100; then
    AC_ERROR([required libcdio 1.10 (10100) or newer - found $VERSION_CDIO])
  fi
  AC_DEFINE_UNQUOTED(VERSION_CDIO,"$VERSION_CDIO",[cdio version])
else
  AC_DEFINE_UNQUOTED(VERSION_CDIO,"",[cdio version])
fi

if test "$HAVE_SMB2"; then
  ac_cv_value=`cd libsmb2; PKG_CONFIG_PATH=. pkg-config --modversion libsmb2`
  AS_VAR_SET(VERSION_SMB2,$ac_cv_value)
  AC_DEFINE_UNQUOTED(VERSION_SMB2,"$VERSION_SMB2",[SMB2 version])
else
  AC_DEFINE_UNQUOTED(VERSION_SMB2,"",[SMB2 version])
fi

if test "$HAVE_MARIADB"; then
  AC_CONSTANT_VALUE([MariaDB version],
                    VERSION_MARIADB,
                    [#include "mariadb/mariadb_version.h"
                    ],
                    [MARIADB_CLIENT_VERSION_STR],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_MARIADB,"$VERSION_MARIADB",[MariaDB version])
else
  AC_DEFINE_UNQUOTED(VERSION_MARIADB,"",[MariaDB version])
fi

if test "$HAVE_POSTGRESQL"; then
  AC_CONSTANT_VALUE([PostgreSQL version],
                    VERSION_POSTGRESQL,
                    [#include "pg_config.h"
                    ],
                    [PG_VERSION_NUM],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_POSTGRESQL,"$VERSION_POSTGRESQL",[PostgreSQL version])
else
  AC_DEFINE_UNQUOTED(VERSION_POSTGRESQL,"",[PostgreSQL version])
fi

if test "$HAVE_PCRE"; then
  AC_CONSTANT_VALUE([pcre version],
                    VERSION_PCRE,
                    [#include "pcre.h"
                    ],
                    [PCRE_MAJOR.PCRE_MINOR],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_PCRE,"$VERSION_PCRE",[pcre version])
else
  AC_DEFINE_UNQUOTED(VERSION_PCRE,"",[pcre version])
fi

if test "$HAVE_PAR2"; then
  AC_CONSTANT_VALUE([PAR2 version],
                    VERSION_PAR2,
                    [#include "par2cmdline/config.h"
                    ],
                    [VERSION],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_PAR2,"$VERSION_PAR2",[PAR2 version])
else
  AC_DEFINE_UNQUOTED(VERSION_PAR2,"",[PAR2 version])
fi

if test "$HAVE_ISOFS"; then
  AC_CONSTANT_VALUE([isofs version],
                    VERSION_ISOFS,
                    [#include "libisofs/libisofs.h"
                    ],
                    [iso_lib_header_version_major.iso_lib_header_version_minor.iso_lib_header_version_micro],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_ISOFS,"$VERSION_ISOFS",[isofs file system version])
else
  AC_DEFINE_UNQUOTED(VERSION_ISOFS,"",[isofs file system version])
fi


if test "$HAVE_RCU"; then
  AC_CONSTANT_VALUE([rcu version],
                    VERSION_RCU,
                    [#include "urcu/urcu_version.h"
                    ],
                    [LIBURCU_VERSION],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_RCU,"$VERSION_RCU",[rcu version])
else
  AC_DEFINE_UNQUOTED(VERSION_RCU,"",[rcu version])
fi

if test "$HAVE_XFS"; then
  AC_CONSTANT_VALUE([xfs version],
                    VERSION_XFS,
                    [#include "xfs/libxfs_version.h"
                    ],
                    [LIBXFS_VERSION],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_XFS,"$VERSION_XFS",xfs version])
else
  AC_DEFINE_UNQUOTED(VERSION_XFS,"",[xfs version])
fi

if test "$HAVE_BURN"; then
  AC_CONSTANT_VALUE([burn version],
                    VERSION_BURN,
                    [#include "libburn/libburn.h"
                    ],
                    [burn_header_version_major.burn_header_version_minor.burn_header_version_micro],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_BURN,"$VERSION_BURN",[burn version])
else
  AC_DEFINE_UNQUOTED(VERSION_BURN,"",[burn version])
fi

if test "$HAVE_MOUNT"; then
  AC_CONSTANT_VALUE([mount version],
                    VERSION_MOUNT,
                    [#include "libmount/libmount.h"
                    ],
                    [LIBMOUNT_VERSION],
                    [],
                    [ "]
                   )
  AC_DEFINE_UNQUOTED(VERSION_MOUNT,"$VERSION_MOUNT",[mount version])
else
  AC_DEFINE_UNQUOTED(VERSION_MOUNT,"",[mount version])
fi

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
AC_CONFIG_FILES(bar/tests/Makefile)
AC_CONFIG_FILES(bar/po/Makefile)
AC_CONFIG_FILES(barcontrol/Makefile)
AC_CONFIG_FILES(doc/Makefile)

dnl config files
AC_CONFIG_FILES(barcontrol/src/Config.java)

AC_CONFIG_COMMANDS(BUILD_NOTE,
                   [echo "";
                    echo "Configuration $VERSION_MAJOR`if test -n "$VERSION_MINOR"; then echo .$VERSION_MINOR; fi`$VERSION_PATCH ($VERSION_REVISION):";
                    echo "  Host                 : $host";
                    echo "  Dynamic linked       : `if test "$ENABLE_LINK_DYNAMIC" = "yes"; then echo yes; else echo no; fi`";
                    echo "  ICU support          : `if test "$HAVE_ICU"; then echo "yes ($VERSION_ICU)"; else echo no; fi`";
                    echo "  zip compression      : `if test "$HAVE_Z"; then echo "yes ($VERSION_Z)"; else echo no; fi`";
                    echo "  bzip2 compression    : `if test "$HAVE_BZ2"; then echo "yes ($VERSION_BZ2)"; else echo no; fi`";
                    echo "  lzma compression     : `if test "$HAVE_LZMA"; then echo "yes ($VERSION_LZMA)"; else echo no; fi`";
                    echo "  lzo compression      : `if test "$HAVE_LZO"; then echo "yes ($VERSION_LZO)"; else echo no; fi`";
                    echo "  lz4 compression      : `if test "$HAVE_LZ4"; then echo "yes ($VERSION_LZ4)"; else echo no; fi`";
                    echo "  zstd compression     : `if test "$HAVE_ZSTD"; then echo "yes ($VERSION_ZSTD)"; else echo no; fi`";
                    echo "  xdelta3 compression  : `if test "$HAVE_XDELTA3"; then echo "yes ($VERSION_XDELTA3)"; else echo no; fi`";
                    echo "  crypto support       : `if test "$HAVE_GCRYPT"; then echo "yes ($VERSION_GCRYPT)"; else echo no; fi`";
                    echo "  FTP support          : `if test "$HAVE_FTP"; then echo yes; else echo no; fi`";
                    echo "  curl support         : `if test "$HAVE_CURL" -a "$HAVE_MXML"; then echo "yes ($VERSION_CURL)"; else echo no; fi`";
                    echo "  SCP/SFTP support     : `if test "$HAVE_GCRYPT" -a "$HAVE_SSH2"; then echo "yes ($VERSION_SSH2)"; else echo no; fi`";
                    echo "  TLS/SSL              : `if test "$HAVE_GNU_TLS"; then echo "yes (OpenSSL $VERSION_OPENSSL, GMP $VERSION_GMP, GnuTLS $VERSION_GNUTLS)"; else echo no; fi`";
                    echo "  ISO 9660 support     : `if test "$HAVE_ISO9660"; then echo "yes ($VERSION_CDIO)"; else echo no; fi`";
                    echo "  SMB support          : `if test "$HAVE_SMB2"; then echo "yes ($VERSION_SMB2)"; else echo no; fi`";
                    echo "  PCRE support         : `if test "$HAVE_PCRE"; then echo "yes ($VERSION_PCRE)"; else echo no; fi`";
                    echo "  SQLite               : yes ($VERSION_SQLITE)";
                    echo "  MariaDB support      : `if test "$HAVE_MARIADB"; then echo "yes ($VERSION_MARIADB)"; else echo no; fi`";
                    echo "  PostgreSQL support   : `if test "$HAVE_POSTGRESQL"; then echo "yes ($VERSION_POSTGRESQL)"; else echo no; fi`";
                    echo "  PAR2 support         : `if test "$HAVE_PAR2"; then echo "yes ($VERSION_PAR2)"; else echo no; fi`";
                    echo "  isofs support        : `if test "$HAVE_ISOFS"; then echo "yes ($VERSION_ISOFS)"; else echo no; fi`";
                    echo "  xfs support          : `if test "$HAVE_XFS"; then echo "yes ($VERSION_XFS, RCU $VERSION_RCU)"; else echo no; fi`";
                    echo "  burn support         : `if test "$HAVE_BURN"; then echo "yes ($VERSION_BURN)"; else echo no; fi`";
                    echo "  mount support        : `if test "$HAVE_MOUNT"; then echo "yes ($VERSION_MOUNT)"; else echo no; fi`";
                    echo "  GUI support          : `if test "$ENABLE_GUI" = "yes"; then echo yes; else echo no; fi`";
                    echo "  GUI i18n support     : `if test "$ENABLE_GUI" = "yes" -a "$ENABLE_GUI_I18N" = "yes"; then echo yes; else echo no; fi`";
                    echo "  SWT                  : `if test -n \"$SWT_DIR\"; then echo $SWT_DIR; else echo \"(integrated)\"; fi`"
                    echo ""
                    echo "  Debug version        : `if test "$ENABLE_DEBUG" = "yes"; then echo yes; else echo no; fi`";
                    echo "  Thread crash handlers: `if test "$ENABLE_DEBUG_THREAD_CRASH_HANDLERS" = "yes"; then echo yes; else echo no; fi`";
                    echo "  Stacktrace support   : `if test "$HAVE_BFD"; then echo yes; else echo no; fi`";
                    echo ""
                    echo "  Install directory    : $INSTALL_DIR"
                    echo "  Binary directory     : $INSTALL_BIN_DIR"
                    echo "  Config directory     : $CONFIG_DIR"
                    echo "  Runtime directory    : $RUNTIME_DIR"
                    echo "  TLS directory        : $TLS_DIR"
                    echo ""
                    echo "  Heapview directory   : $HEAPVIEW_DIR"
                    echo ""
                    if test    ! "$HAVE_Z" \
                            -o ! "$HAVE_BZ2" \
                            -o ! "$HAVE_LZMA" \
                            -o ! "$HAVE_LZO" \
                            -o ! "$HAVE_LZ4" \
                            -o ! "$HAVE_ZSTD" \
                            -o ! "$HAVE_XDELTA3" \
                            -o ! "$HAVE_GCRYPT" \
                            -o ! "$HAVE_FTP" \
                            -o ! "$HAVE_CARES" \
                            -o ! "$HAVE_CURL" \
                            -o ! "$HAVE_MXML" \
                            -o ! "$HAVE_SSL" \
                            -o ! "$HAVE_SSH2" \
                            -o ! "$HAVE_GNU_TLS" \
                            -o ! "$HAVE_MARIDDB" \
                            -o ! "$HAVE_POSTGRESQL" \
                            -o ! "$HAVE_ISO9660" \
                            -o ! "$HAVE_SMB2" \
                            -o ! "$HAVE_PCRE" \
                            -o ! "$HAVE_PAR2" \
                            -o ! "$HAVE_ISOFS" \
                            -o ! "$HAVE_XFSPROGS" \
                            -o ! "$HAVE_BURN" \
                            -o ! "$HAVE_MOUNT" ; then
                      echo "Note: you can download missing additional software with the script"
                      echo "      './download-third-party-packages.sh'. Please rerun configure"
                      echo "      after downloading."
                    fi
                   ],
                   VERSION_MAJOR="$VERSION_MAJOR"
                   VERSION_MINOR="$VERSION_MINOR"
                   VERSION_PATCH="$VERSION_PATCH"
                   VERSION_REVISION="$VERSION_REVISION"

                   host="$host"

                   ENABLE_LINK_DYNAMIC=$ENABLE_LINK_DYNAMIC
                   HAVE_ICU=$HAVE_ICU
                   HAVE_Z=$HAVE_Z
                   HAVE_BZ2=$HAVE_BZ2
                   HAVE_LZMA=$HAVE_LZMA
                   HAVE_LZO=$HAVE_LZO
                   HAVE_LZ4=$HAVE_LZ4
                   HAVE_ZSTD=$HAVE_ZSTD
                   HAVE_XDELTA3=$HAVE_XDELTA3
                   HAVE_GCRYPT=$HAVE_GCRYPT
                   HAVE_FTP=$HAVE_FTP
                   HAVE_CURL=$HAVE_CURL
                   HAVE_MXML=$HAVE_MXML
                   HAVE_SSL=$HAVE_SSL
                   HAVE_SSH2=$HAVE_SSH2
                   HAVE_GNU_TLS=$HAVE_GNU_TLS
                   HAVE_MARIADB=$HAVE_MARIADB
                   HAVE_POSTGRESQL=$HAVE_POSTGRESQL
                   HAVE_ISO9660=$HAVE_ISO9660
                   HAVE_SMB2=$HAVE_SMB2
                   HAVE_PCRE=$HAVE_PCRE
                   HAVE_PAR2=$HAVE_PAR2
                   HAVE_ISOFS=$HAVE_ISOFS
                   HAVE_RCU=$HAVE_RCU
                   HAVE_XFS=$HAVE_XFS
                   HAVE_BURN=$HAVE_BURN
                   HAVE_MOUNT=$HAVE_MOUNT
                   HAVE_BFD=$HAVE_BFD

                   ENABLE_DEBUG=$ENABLE_DEBUG
                   ENABLE_DEBUG_THREAD_CRASH_HANDLERS=$ENABLE_DEBUG_THREAD_CRASH_HANDLERS

                   DOCKER="$DOCKER"
                   ISCC="$ISCC"

                   ENABLE_GUI=$ENABLE_GUI
                   ENABLE_GUI_I18N=$ENABLE_GUI_I18N

                   SWT_DIR="$SWT_DIR"
                   INSTALL_DIR="$INSTALL_DIR"
                   INSTALL_BIN_DIR="$INSTALL_BIN_DIR"
                   CONFIG_DIR="$CONFIG_DIR"
                   RUNTIME_DIR="$RUNTIME_DIR"
                   TLS_DIR="$TLS_DIR"
                   HEAPVIEW_DIR="$HEAPVIEW_DIR"

                   VERSION_ICU="$VERSION_ICU"
                   VERSION_Z="$VERSION_Z"
                   VERSION_BZ2="$VERSION_BZ2"
                   VERSION_LZMA="$VERSION_LZMA"
                   VERSION_LZO="$VERSION_LZO"
                   VERSION_LZ4="$VERSION_LZ4"
                   VERSION_ZSTD="$VERSION_ZSTD"
                   VERSION_XDELTA3="$VERSION_XDELTA3"
                   VERSION_GCRYPT="$VERSION_GCRYPT"
                   VERSION_CURL="$VERSION_CURL"
                   VERSION_OPENSSL="$VERSION_OPENSSL"
                   VERSION_SSH2="$VERSION_SSH2"
                   VERSION_GMP="$VERSION_GMP"
                   VERSION_GNUTLS="$VERSION_GNUTLS"
                   VERSION_MARIADB="$VERSION_MARIADB"
                   VERSION_POSTGRESQL="$VERSION_POSTGRESQL"
                   VERSION_CDIO="$VERSION_CDIO"
                   VERSION_SMB2="$VERSION_SMB2"
                   VERSION_PCRE="$VERSION_PCRE"
                   VERSION_SQLITE="$VERSION_SQLITE"
                   VERSION_PAR2="$VERSION_PAR2"
                   VERSION_ISOFS="$VERSION_ISOFS"
                   VERSION_RCU="$VERSION_RCU"
                   VERSION_XFS="$VERSION_XFS"
                   VERSION_BURN="$VERSION_BURN"
                   VERSION_MOUNT="$VERSION_MOUNT"
                  )

AC_OUTPUT

dnl --------------------------------------------------------------------

rm -f $tmpFile

dnl --- end of file ---
