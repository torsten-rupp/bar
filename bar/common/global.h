/***********************************************************************\
*
* Contents: global definitions
* Systems: Linux
*
\***********************************************************************/

#ifndef __GLOBAL__
#define __GLOBAL__

#if (defined DEBUG)
 #warning DEBUG option set - no LOCAL and no -O2 (optimizer) will be used!
#endif
#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#ifdef HAVE_STDBOOL_H
  #include <stdbool.h>
#endif
#include <limits.h>
#include <float.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#ifdef HAVE_LIBINTL_H
  #include <libintl.h>
#endif
#if defined(HAVE_PCRE)
  #include <pcreposix.h>
#elif defined(HAVE_REGEX_H)
  #include <regex.h>
#else
  #warning No regular expression library available!
#endif /* HAVE_PCRE || HAVE_REGEX_H */
#ifdef HAVE_EXECINFO_H
  #include <execinfo.h>
#endif
#include <errno.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <pthread.h>
#elif defined(PLATFORM_WINDOWS)
  #include <winsock2.h>
  #include <windows.h>
  #include <pthread.h>
  #include <intrin.h>
#endif /* PLATFORM_... */

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
// architecture
#if   defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
  #define ARCHTECTURE_X86
#elif defined(__arm__)
  #define ARCHTECTURE_ARM
#else
  #define ARCHTECTURE_UNKNOWN
#endif

#define DEBUG_LEVEL 8                          // debug level

// definition of boolean values
#if defined(__cplusplus) || defined(HAVE_STDBOOL_H)
  #ifndef FALSE
    #define FALSE false
  #endif
  #ifndef TRUE
    #define TRUE true
  #endif
#else
  #ifndef FALSE
    #define FALSE 0
  #endif
  #ifndef TRUE
    #define TRUE  1
  #endif
#endif
#define NO  FALSE
#define YES TRUE
#define OFF FALSE
#define ON  TRUE

// definition of some character names
#define BEL '\007'
#define ESC '\033'

// math constants
#ifndef PI
  #define PI 3.14159265358979323846
#endif

// bits/byte conversion
#define BYTES_TO_BITS(n) ((n)*8LL)
#define BITS_TO_BYTES(n) ((n)/8LL)

// time constants, time conversions
#define NS_PER_US     1000LL
#define NS_PER_MS     (1000LL*NS_PER_US)
#define NS_PER_S      (1000LL*NS_PER_MS)
#define NS_PER_SECOND NS_PER_S
#define NS_PER_MINUTE (1000LL*NS_PER_SECOND)
#define NS_PER_HOUR   (60LL*NS_PER_MINUTE)
#define NS_PER_DAY    (24LL*NS_PER_HOUR)

#define US_PER_MS     1000LL
#define US_PER_S      (1000LL*US_PER_MS)
#define US_PER_SECOND US_PER_S
#define US_PER_MINUTE (60LL*US_PER_SECOND)
#define US_PER_HOUR   (60LL*US_PER_MINUTE)
#define US_PER_DAY    (24LL*US_PER_HOUR)

#define MS_PER_S      1000LL
#define MS_PER_SECOND MS_PER_S
#define MS_PER_MINUTE (60LL*MS_PER_SECOND)
#define MS_PER_HOUR   (60LL*MS_PER_MINUTE)
#define MS_PER_DAY    (24LL*MS_PER_HOUR)

#define S_PER_MINUTE (60LL)
#define S_PER_HOUR   (60LL*S_PER_MINUTE)
#define S_PER_DAY    (24LL*S_PER_HOUR)

#define S_TO_MS(n) ((n)*1000L)
#define S_TO_US(n) ((n)*1000000LL)
#define MS_TO_US(n) ((n)*1000LL)

// minimal and maximal values for some scalar data types
#define MIN_CHAR           CHAR_MIN
#define MAX_CHAR           CHAR_MAX
#define MIN_SHORTINT       SHRT_MIN
#define MAX_SHORTINT       SHRT_MAX
#define MIN_INT            INT_MIN
#define MAX_INT            INT_MAX
#define MIN_LONG           LONG_MIN
#define MAX_LONG           LONG_MAX
#ifdef HAVE_LLONG_MIN
  #define MIN_LONG_LONG    LLONG_MIN
#else
  #define MIN_LONG_LONG    -9223372036854775808LL
#endif
#ifdef HAVE_LLONG_MAX
  #define MAX_LONG_LONG    LLONG_MAX
#else
  #define MAX_LONG_LONG    9223372036854775807LL
#endif
#define MIN_INT8           -128
#define MAX_INT8           127
#define MIN_INT16          -32768
#define MAX_INT16          32767
#define MIN_INT32          -2147483648
#define MAX_INT32          2147483647
#define MIN_INT64          (-9223372036854775807LL-1LL)
#define MAX_INT64          9223372036854775807LL

#define MIN_UCHAR          0
#define MAX_UCHAR          UCHAR_MAX
#define MIN_USHORT         0
#define MAX_USHORT         USHRT_MAX
#define MIN_UINT           0
#define MAX_UINT           UINT_MAX
#define MIN_ULONG          0
#define MAX_ULONG          ULONG_MAX
#define MIN_ULONG_LONG     0
#ifdef HAVE_ULLONG_MAX
  #define MAX_ULONG_LONG   ULLONG_MAX
#else
  #define MAX_ULONG_LONG   18446744073709551615LLU
#endif
#define MIN_UINT8          0
#define MAX_UINT8          255
#define MIN_UINT16         0
#define MAX_UINT16         65535
#define MIN_UINT32         0L
#define MAX_UINT32         4294967296LU
#define MIN_UINT64         0L
#define MAX_UINT64         18446744073709551615LLU

#define MIN_FLOAT          FLT_MIN
#define MAX_FLOAT          FLT_MAX
#define EPSILON_FLOAT      FLT_EPSILON
#define MIN_DOUBLE         DBL_MIN
#define MAX_DOUBLE         DBL_MAX
#define EPSILON_DOUBLE     DBL_EPSILON
#define MIN_LONGDOUBLE     LDBL_MIN
#define MAX_LONGDOUBLE     LDBL_MAX
#define EPSILON_LONGDOUBLE LDBL_EPSILON

// memory sizes
#define KB 1024
#define MB (1024L*KB)
#define GB (1024LL*MB)
#define TB (1024LL*GB)
#define PB (1024LL*TB)

// special constants
#define NO_WAIT      0L
#define WAIT_FOREVER -1L

// exit codes
#define EXITCODE_INTERNAL_ERROR 128

#ifndef NDEBUG

// dump info type
#define DUMP_INFO_TYPE_ALLOCATED (1 << 0)
#define DUMP_INFO_TYPE_HISTOGRAM (1 << 1)

#endif /* NDEBUG */

/**************************** Datatypes ********************************/
#if !defined(__cplusplus) && !defined(HAVE_STDBOOL_H) && !defined(_STDBOOL_H)
  typedef uint8_t bool;
#endif

typedef unsigned char       uchar;
typedef short int           shortint;
typedef unsigned short int  ushortint;
#ifndef HAVE_UINT
  typedef unsigned int        uint;
#endif
#ifndef HAVE_ULONG
  typedef unsigned long       ulong;
#endif
typedef unsigned long long  ulonglong;

// compare results
typedef enum
{
  CMP_LOWER=-1,
  CMP_EQUAL=0,
  CMP_GREATER=+1
} CmpResults;

// base datatypes
typedef uint8_t             byte;

typedef uint8_t             bool8;
typedef uint32_t            bool32;
typedef char                char8;
typedef unsigned char       uchar8;
typedef int8_t              int8;
typedef int16_t             int16;
typedef int32_t             int32;
typedef int64_t             int64;
typedef uint8_t             uint8;
typedef uint16_t            uint16;
typedef uint32_t            uint32;
typedef uint64_t            uint64;
typedef void                void32;

// mask+shift data
typedef struct
{
  uint16 mask;
  uint   shift;
} MaskShift16;

typedef struct
{
  uint32 mask;
  uint   shift;
} MaskShift32;

typedef struct
{
  uint64 mask;
  uint   shift;
} MaskShift64;

// execute once handle
typedef pthread_once_t ExecuteOnceHandle;

#ifndef NDEBUG

/***********************************************************************\
* Name   : ResourceDumpInfoFunction
* Purpose: resource dump info call-back function
* Input  : variableName  - variable name
*          resource      - resource
*          allocFileName - allocation file name
*          allocLineNb   - allocation line number
*          n             - string number [0..count-1]
*          count         - total string count
*          userData      - user data
* Output : -
* Return : TRUE for continue, FALSE for abort
* Notes  : -
\***********************************************************************/

typedef bool(*ResourceDumpInfoFunction)(const char *variableName,
                                        const void *resource,
                                        const char *allocFileName,
                                        ulong      allocLineNb,
                                        ulong      n,
                                        ulong      count,
                                        void       *userData
                                       );

// stack trace output types
typedef enum
{
  DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,
  DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_INFO,
  DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_FATAL
} DebugDumpStackTraceOutputTypes;

/***********************************************************************\
* Name   : DebugDumpStackTraceOutputFunction
* Purpose: debug dump strack trace output function
* Input  : text     - text
*          userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*DebugDumpStackTraceOutputFunction)(const char *text, void *userData);

#endif /* NDEBUG */

/**************************** Variables ********************************/

#ifndef NDEBUG
  extern pthread_mutex_t debugConsoleLock;    // lock console
  extern const char      *__testCodeName__;   // name of testcode to execute
#endif /* not NDEBUG */

/****************************** Macros *********************************/
#define __GLOBAL_CONCAT2(s1,s2) s1##s2
#define __GLOBAL_CONCAT(s1,s2) __GLOBAL_CONCAT2(s1,s2)

#define GLOBAL extern
#define LOCAL static

#undef INLINE
#ifdef NDEBUG
  #define INLINE static inline
  #define LOCAL_INLINE static inline
#else
  #define INLINE
  #define LOCAL_INLINE static
#endif /* NDEBUG */

#undef ATTRIBUTE_PACKED
#undef ATTRIBUTE_WARN_UNUSED_RESULT
#undef ATTRIBUTE_NO_INSTRUMENT_FUNCTION
#undef ATTRIBUTE_AUTO
#ifdef __GNUC__
  #define ATTRIBUTE_PACKED             __attribute__((__packed__))
  #define ATTRIBUTE_WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
  #ifndef DEBUG
    #define ATTRIBUTE_NO_INSTRUMENT_FUNCTION __attribute__((no_instrument_function))
  #else
    #define ATTRIBUTE_NO_INSTRUMENT_FUNCTION
  #endif
  #define ATTRIBUTE_AUTO(functionCode) __attribute((cleanup(functionCode)))
  #define ATTRIBUTE_DEPRECATED         __attribute__((deprecated))
#else
  #define ATTRIBUTE_PACKED
  #define ATTRIBUTE_WARN_UNUSED_RESULT
  #define ATTRIBUTE_NO_INSTRUMENT_FUNCTION
  #define ATTRIBUTE_AUTO(functionCode)
  #define ATTRIBUTE_DEPRECATED
#endif /* __GNUC__ */

// only for better reading
#define CALLBACK_(code,argument) code,argument
#define CALLBACK_LAMBDA_(functionReturnType,functionSignature,functionBody,functionUserData) \
  LAMBDA(functionReturnType,functionSignature,functionBody),functionUserData

// mask and shift value
#define MASKSHIFT(n,maskShift) (((n) & maskShift.mask) >> maskShift.shift)

// stringify
#define STRINGIFY(s) __STRINGIFY__(s)
#define __STRINGIFY__(s) #s

/***********************************************************************\
* Name   : EXECUTE_ONCE
* Purpose: execute block once
* Input  : functionReturnType - call-back function return type
*          functionSignature  - call-back function signature
*          functionBody       - call-back function body
* Output : -
* Return : -
* Notes  : example
*          EXECUTE_ONCE({
*                       ...
*                      });
\***********************************************************************/

#define __EXECUTE_ONCE(__n, functionBody) \
  ({ \
    static ExecuteOnceHandle __GLOBAL_CONCAT(__executeOnceHandle,__n) = PTHREAD_ONCE_INIT; \
    \
    auto void __closure__ (void); \
    void __closure__ (void) functionBody \
    pthread_once(&__GLOBAL_CONCAT(__executeOnceHandle,__n),__closure__); \
  })

#define EXECUTE_ONCE(functionBody) \
  __EXECUTE_ONCE(__COUNTER__,functionBody)

/***********************************************************************\
* Name   : LAMBDA
* Purpose: define a lambda-function (anonymouse function)
* Input  : functionReturnType - call-back function return type
*          functionSignature  - call-back function signature
*          functionBody       - call-back function body
* Output : -
* Return : -
* Notes  : example
*          List_init(list,
*                    LAMBDA(void,(...),{ ... }),NULL
*                    LAMBDA(void,(...),{ ... }),NULL
*                   );
\***********************************************************************/

#define LAMBDA(functionReturnType,functionSignature,functionBody) \
  ({ \
    auto functionReturnType __closure__ functionSignature; \
    functionReturnType __closure__ functionSignature functionBody \
    __closure__; \
  })

/***********************************************************************\
* Name   : CLOSURE
* Purpose: define a closure-function call
* Input  : functionReturnType - closure function return type
*          functionBody       - closure function body
* Output : -
* Return : function result
* Notes  : example
*          int a = CLOSURE(int,{ return 123; });
\***********************************************************************/

#define CLOSURE(functionReturnType,functionBody) \
  ({ \
    auto functionReturnType __closure__ (void); \
    functionReturnType __closure__ (void) functionBody \
    __closure__(); \
  })

/***********************************************************************\
* Name   : CALLBACK_INLINE
* Purpose: define an inline call-back function (anonymouse function)
* Input  : functionReturnType - call-back function signature
*          functionSignature  - call-back function signature
*          functionBody       - call-back function body
*          functionUserData   - call-back function user data
* Output : -
* Return : -
* Notes  : example
*          List_init(list,
*                    CALLBACK__INLINE(void,(...),{ ... },NULL)
*                    CALLBACK__INLINE(void,(...),{ ... },NULL)
*                   );
\***********************************************************************/

#define CALLBACK_INLINE(functionReturnType,functionSignature,functionBody,functionUserData) \
  LAMBDA(functionReturnType,functionSignature,functionBody),functionUserData

/***********************************************************************\
* Name   : printf/fprintf
* Purpose: printf/fprintf-work-around for Windows
* Input  : -
* Output : -
* Return : -
* Notes  : Windows does not support %ll format token, instead it tries
*          - as usually according to the MS principle: ignore any
*          standard
*          whenever possible - its own way (and of course fail...).
*          Thus use the MinGW implementation of printf/fprintf.
\***********************************************************************/

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
  /* Work-around for Windows:
  */
#if 1
  #ifndef printf
    #define printf __mingw_printf
  #endif
  #ifndef vprintf
    #define vprintf __mingw_vprintf
  #endif
  #ifndef fprintf
    #define fprintf __mingw_fprintf
  #endif
  #ifndef vfprintf
    #define vfprintf __mingw_vfprintf
  #endif
  #ifndef sprintf
    #define sprintf __mingw_sprintf
  #endif
  #ifndef vsprintf
    #define vsprintf __mingw_vsprintf
  #endif
  #ifndef snprintf
    #define snprintf __mingw_snprintf
  #endif
  #ifndef vsnprintf
    #define vsnprintf __mingw_vsnprintf
  #endif
#endif
#endif /* PLATFORM_... */

/***********************************************************************\
* Name   : UNUSED_VARIABLE
* Purpose: avoid compiler warning for unused variables/parameters
* Input  : variable - variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define UNUSED_VARIABLE(variable) (void)variable

/***********************************************************************\
* Name   : UNUSED_VARIABLE
* Purpose: avoid compiler warning for unused variables/parameters
* Input  : variable - variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

//#define UNUSED_FUNCTION(function) (void)&function;
#define UNUSED_FUNCTION(function)

/***********************************************************************\
* Name   : UNUSED_RESULT
* Purpose: avoid compiler warning for unused result
* Input  : result - value
* Output : -
* Return : -
* Notes  : http://git.savannah.gnu.org/cgit/gnulib.git/tree/lib/ignore-value.h
\***********************************************************************/

#if 3 < __GNUC__ + (4 <= __GNUC_MINOR__)
  #define UNUSED_RESULT(result) (__extension__({ __typeof__(result) __result = (result); (void)__result; }))
#else
  #define UNUSED_RESULT(result) ((void)(result))
#endif

/***********************************************************************\
* Name   : SIZE_OF_MEMBER
* Purpose: get size of struct member
* Input  : type   - struct type
*          member - member name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define SIZE_OF_MEMBER(type,member) (sizeof(((type*)NULL)->member))

/***********************************************************************\
* Name   : SIZE_OF_ARRAY
* Purpose: get size of array
* Input  : array - array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define SIZE_OF_ARRAY(array) (sizeof(array)/sizeof(array[0]))

/***********************************************************************\
* Name   : FOR_ARRAY
* Purpose: iterate over array and execute block
* Input  : array    - array
*          variable - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain indizes of array
*          usage:
*            FOR_ARRAY(array,variable)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define FOR_ARRAY(array,variable) \
  for ((variable) = 0; \
       (variable) < SIZE_OF_ARRAY(array); \
       (variable)++ \
      )

/***********************************************************************\
* Name   : ARRAY_FIRST
* Purpose: first element of array
* Input  : array - array
* Output : -
* Return : first element of array
* Notes  : -
\***********************************************************************/

#define ARRAY_FIRST(array) \
  array[0]

/***********************************************************************\
* Name   : ARRAY_LAST
* Purpose: last element of array
* Input  : array - array
* Output : -
* Return : last element of array
* Notes  : -
\***********************************************************************/

#define ARRAY_LAST(array) \
  array[SIZE_OF_ARRAY(array)-1]

/***********************************************************************\
* Name   : ARRAY_FIND
* Purpose: find index of value in array
* Input  : array     - array
*          size      - size of array (number of elements)
*          i         - iterator
*          condition - condition
* Output : -
* Return : index or size
* Notes  : -
\***********************************************************************/

#define ARRAY_FIND(array,size,i,condition) \
  ({ \
    auto uint __closure__ (void); \
    uint __closure__ (void) \
    { \
      uint i; \
      \
      i = 0; \
      while ((i) < (size) && !(condition)) \
      { \
        (i)++; \
      } \
      \
      return i; \
    }; \
    __closure__; \
  })()

/***********************************************************************\
* Name   : ARRAY_CONTAINS
* Purpose: check if value is in array
* Input  : array     - array
*          size      - size of array (number of elements)
*          i         - iterator
*          condition - condition
* Output : -
* Return : TURE iff in array
* Notes  : -
\***********************************************************************/

#define ARRAY_CONTAINS(array,size,i,condition) \
  ({ \
    auto bool __closure__ (void); \
    bool __closure__ (void) \
    { \
      uint i; \
      \
      i = 0; \
      while ((i) < (size) && !(condition)) \
      { \
        (i)++; \
      } \
      \
      return (i) < (size); \
    }; \
    __closure__; \
  })()

/***********************************************************************\
* Name   : FOR_ENUM
* Purpose: iterate over enum and execute block
* Input  : enumMin,enumMax - enum min./max.
*          variable        - iteration variable
* Output : -
* Return : -
* Notes  : value will contain enum values
*          usage:
*            unsigned int i;
*
*            FOR_ENUM(variable,value,enum1,enum2,...)
*            {
*              ... = value
*            }
\***********************************************************************/

#define __FOR_ENUM(__n, variable, value, ...) \
  typeof(value) __GLOBAL_CONCAT(__enum_values,__n) ## __[] = {__VA_ARGS__}; \
  for ((variable) = 0, value = __GLOBAL_CONCAT(__enum_values,__n) ## __[variable]; \
       (variable) < SIZE_OF_ARRAY(__GLOBAL_CONCAT(__enum_values,__n) ## __); \
       (variable)++, (value) = __GLOBAL_CONCAT(__enum_values,__n) ## __[variable] \
      )
#define FOR_ENUM(variable,value,...) \
  __FOR_ENUM(__COUNTER__,variable,value,__VA_ARGS__)

/***********************************************************************\
* Name   : ALIGN
* Purpose: align value to boundary
* Input  : n         - value
*          alignment - alignment
* Output : -
* Return : n >= n with n modulo alignment = 0
* Notes  : alignment must be 2^n!
\***********************************************************************/

#define ALIGN(n,alignment) (((alignment)>0) ? (((n)+(alignment)-1) & ~((alignment)-1)) : (n))

/***********************************************************************\
* Name   : IS_SET
* Purpose: check if bit is set
* Input  : value - value
*          mask  - bit mask
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

#define IS_SET(value,mask) (((value) & (mask)) != 0)

/***********************************************************************\
* Name   : SET_CLEAR, SET_VALUE, SET_ADD, SET_REM, IN_SET
* Purpose: set macros
* Input  : set     - set (integer)
*          element - element
* Output : -
* Return : TRUE if value is in set, FALSE otherwise
* Notes  : -
\***********************************************************************/

typedef uint32 Set;

#define SET_CLEAR(set) \
  do \
  { \
    (set) = 0; \
  } \
  while (0)

#define SET_VALUE(element) \
  (1U << (element))

#define SET_ADD(set,element) \
  do \
  { \
    (set) |= SET_VALUE(element); \
  } \
  while (0)

#define SET_REM(set,element) \
  do \
  { \
    (set) &= ~(SET_VALUE(element)); \
  } \
  while (0)

#define IN_SET(set,element) (((set) & SET_VALUE(element)) == SET_VALUE(element))

typedef byte* ValueSet;

#define ValueSet(x,n) byte x[3]
#define VALUE_SET \
  enum

#define VALUESET_CLEAR(set) \
  do \
  { \
    memClear(set,sizeof(set)); \
  } \
  while (0)

#define VALUESET_SET(set,bit) \
  do \
  { \
    ((byte*)(set))[bit/8] |= (1 << (bit%8)); \
  } \
  while (0)

#define VALUESET_REM(set,bit) \
  do \
  { \
    ((byte*)(set))[bit/8] &= ~(1 << (bit%8)); \
  } \
  while (0)

#define VALUESET_IS_SET(set,bit) \
  ((((byte*)(set))[bit/8] & (1 << (bit%8))) != 0)



/***********************************************************************\
* Name   : BITSET_SET, BITSET_CLEAR, BITSET_IS_SET
* Purpose: set macros
* Input  : set     - set (array)
*          element - element
* Output : -
* Return : TRUE if value is in bitset, FALSE otherwise
* Notes  : -
\***********************************************************************/

typedef byte* BitSet;

#define BITSET_SET(set,bit) \
  do \
  { \
    ((byte*)(set))[bit/8] |= (1 << (bit%8)); \
  } \
  while (0)

#define BITSET_CLEAR(set,bit) \
  do \
  { \
    ((byte*)(set))[bit/8] &= ~(1 << (bit%8)); \
  } \
  while (0)

#define BITSET_IS_SET(set,bit) \
  ((((byte*)(set))[bit/8] & (1 << (bit%8))) != 0)

/***********************************************************************\
* Name   : ATOMIC_INCREMENT, ATOMIC_DECREMENT
* Purpose: atomic increment/decrement value by 1
* Input  : n - value
* Output : -
* Return : old value
* Notes  : -
\***********************************************************************/

#define ATOMIC_INCREMENT(n) atomicIncrement(&(n), 1)
#define ATOMIC_DECREMENT(n) atomicIncrement(&(n),-1)

/***********************************************************************\
* Name   : IS_NAN, IS_INF
* Purpose: check is NaN, infinite
* Input  : d - number
* Output : -
* Return : TRUE iff NaN/infinite
* Notes  : -
\***********************************************************************/

#ifndef __cplusplus
  #define IS_NAN(d) (!((d)==(d)))                        // check if Not-A-Number (NaN)
  #define IS_INF(d) ((d<-MAX_DOUBLE) || (d>MAX_DOUBLE))  // check if infinit-number
#endif

/***********************************************************************\
* Name   : MIN, MAX
* Purpose: get min./max.
* Input  : x,y - numbers
* Output : -
* Return : min./max. of x,y
* Notes  : -
\***********************************************************************/

//#ifdef __GNUG__
//  #define MIN(x,y) ((x)<?(y))
//  #define MAX(x,y) ((x)>?(y))
//#else
  #define MIN(x,y) (((x)<(y)) ? (x) : (y))
  #define MAX(x,y) (((x)>(y)) ? (x) : (y))
//#endif

/***********************************************************************\
* Name   : IS_IN_RANGE
* Purpose: check if value is in range
* Input  : l,h - lower/upper bound
*          x   - number
* Output : -
* Return : TRUE if l <= x <= h
* Notes  : -
\***********************************************************************/

#define IS_IN_RANGE(l,x,h) (((l) <= (x)) && ((x) <= (h)))

/***********************************************************************\
* Name   : IN_RANGE
* Purpose: get value in range
* Input  : l,h - lower/upper bound
*          x   - number
* Output : -
* Return : x iff l<x<h,
*          l iff x <= l,
*          h iff x >= h
* Notes  : -
\***********************************************************************/

#ifdef __GNUG__
  #define IN_RANGE(l,x,h) (( ((x)<?(h) )>?(l))
#else
  #define IN_RANGE(l,x,h) ((x)<(l) ? (l) : ((x)>(h) ? (h) : (x)))
#endif

/***********************************************************************\
* Name   : FLOOR
* Purpose: round number down by factor
* Input  : x - number
*          n - factor (n=y^2)
* Output : -
* Return : x' rounded down with x' <= x && x' mod n == 0
* Notes  : -
\***********************************************************************/

#define FLOOR(x,n) ((x) & ~((n)-1))

/***********************************************************************\
* Name   : CEIL
* Purpose: round number up by factor
* Input  : x - number
*          n - factor (n=y^2)
* Output : -
* Return : x' rounded up with x' >= x && x' mod n == 0
* Notes  : -
\***********************************************************************/

#define CEIL(x,n) (((x)+(n)-1) & ~((n)-1))

/***********************************************************************\
* Name   : SQUARE
* Purpose: calculate square
* Input  : x - number
* Output : -
* Return : x*x
* Notes  : -
\***********************************************************************/

#define SQUARE(x) ((x)*(x))

/***********************************************************************\
* Name   : CHECK_RANGE
* Purpose: check if number is in range
* Input  : l,h - lower/upper bound
*          x   - number
* Output : -
* Return : TRUE iff l<x<h, FALSE otherwise
* Notes  : -
\***********************************************************************/

#define CHECK_RANGE(l,x,u) (( ((l)<=(x)) && ((x)<=(u)) ) || \
                            ( ((u)<=(x)) && ((x)<=(l)) )    \
                           )

/***********************************************************************\
* Name   : CHECK_ANGLE_RANGE
* Purpose: check if number/angle is in range
* Input  : l,h - lower/upper bound [rad]
*          a   - angle [rad]
* Output : -
* Return : TRUE iff l<a<h, FALSE otherwise
* Notes  : -
\***********************************************************************/

#define CHECK_ANGLE_RANGE(l,a,u) (((NormRad(l))<=(NormRad(u)))?CHECK_RANGE(NormRad(l),NormRad(a),NormRad(u)):(CHECK_RANGE(l,a,2*PI) || CHECK_RANGE(0,NormRad(a),NormRad(u))))
#ifndef __cplusplus
 #define IndexMod(l,i,u) ( l+(((i)<0)?( ((u)-(l)+1)-((-(i)%((u)-(l)+1)))%((u)-(l)+1) ):( ((i)>((u)-(l)+1))?( (i)%((u)-(l)+1) ):( (i) ) )) )
#endif

/***********************************************************************\
* Name   : RAD_TO_DEGREE, DEGREE_TO_RAD
* Purpose: convert rad to degree/degree to rad
* Input  : n - rad/degree
* Output : -
* Return : degree/rad value
* Notes  : -
\***********************************************************************/

#define RAD_TO_DEGREE(n) ((n)>=0?\
                          (((n)<=2*PI)?\
                           (n)*180.0/PI:\
                           ((n)-2*PI)*180.0/PI\
                          ):\
                          (((n)+2*PI)*180.0/PI)\
                         )

#define DEGREE_TO_RAD(n) ((n)>=0?\
                          (((n)<=360)?\
                           (n)*PI/180.0:\
                           ((n)-360)*PI/180.0\
                          ):\
                          (((n)+360)*PI/180.0)\
                         )

#ifndef __cplusplus
  #define RadToDegree(n) RAD_TO_DEGREE(n)
  #define DegreeToRad(n) DEGREE_TO_RAD(n)
#endif

/***********************************************************************\
* Name   : NORM_RAD360, NORM_RAD180, NORM_RAD90, NORM_RAD
* Purpose: normalize rad value
* Input  : n - value
* Output : -
* Return : normalized value
* Notes  : -
\***********************************************************************/

#define NORM_RAD360(n)    (fmod(n,2*PI))

#define NORM_RAD180(n)    (((n)>PI)\
                           ?((n)-2*PI)\
                           :(((n)<-PI)\
                             ?((n)+2*PI)\
                             :(n)\
                            )\
                          )

#define NORM_RAD90(n)     (((n)>3*PI/2)\
                           ?((n)-2*PI)\
                           :(((n)>PI/2)\
                             ?((n)-PI)\
                             :(((n)<-3*PI/2)\
                               ?((n)+2*PI)\
                               :(((n)<-PI/2)\
                                 ?((n)+PI)\
                                 :(n)\
                                )\
                              )\
                            )\
                          )
#define NORM_RAD(n)       (((n)<0)\
                           ?((n)+2*PI)\
                           :(((n)>(2*PI))\
                             ?((n)-2*PI)\
                             :(n)\
                            )\
                          )

/***********************************************************************\
* Name   : NORM_DEGREE360, NORM_DEGREE180, NORM_DEGREE90, NORM_DEGREE
* Purpose: normalize degree value
* Input  : n - value
* Output : -
* Return : normalized value
* Notes  : -
\***********************************************************************/

#define NORM_DEGREE360(n) (fmod(n,360))

#define NORM_DEGREE180(n) (((n)>180)\
                           ?((n)-360)\
                           :(((n)<-180)\
                             ?((n)+360):\
                             (n)\
                            )\
                          )
#define NORM_DEGREE90(n)  (((n)>270)\
                            ?((n)-180)\
                            :(((n)>180)\
                              ?((n)-90)\
                              :(((n)<-270)\
                                ?((n)+180)\
                                :(((n)<-180)\
                                  ?((n)+90)\
                                  :(n)\
                                 )\
                               )\
                             )\
                          )

#define NORM_DEGREE(n)    (((n)<0)\
                           ?((n)+360)\
                           :(((n)>360)\
                             ?((n)-360):\
                             (n)\
                            )\
                          )
/* used in code */
#ifndef __cplusplus
  #define SwapWORD(n) ( ((n & 0xFF00) >> 8) | \
                        ((n & 0x00FF) << 8)   \
                      )
  #define SwapLONG(n) ( ((n & 0xFF000000) >> 24) | \
                        ((n & 0x00FF0000) >>  8) | \
                        ((n & 0x0000FF00) <<  8) | \
                        ((n & 0x000000FF) << 24)   \
                      )
#endif

/***********************************************************************\
* Name   : BLOCK_DO, BLOCK_DOX
* Purpose: execute code with entry and exit code
* Input  : result    - result
*          entryCode - entry code
*          exitCode  - exit code
* Output : -
* Return : -
* Notes  : use gcc closure!
\***********************************************************************/

#define BLOCK_DO(entryCode,exitCode,block) \
  do \
  { \
    entryCode; \
    ({ \
      auto void __closure__(void); \
      \
      void __closure__(void)block; \
      __closure__; \
    })(); \
    exitCode; \
  } \
  while (0)

#define BLOCK_DOX(result,entryCode,exitCode,block) \
  do \
  { \
    entryCode; \
    result = ({ \
               auto typeof(result) __closure__(void); \
               \
               typeof(result) __closure__(void)block; __closure__; \
             })(); \
    exitCode; \
  } \
  while (0)

/***********************************************************************\
* Name   : CSTRING_CHAR_ITERATE
* Purpose: iterated over characters of string and execute block
* Input  : string           - string
*          iteratorVariable - iterator variable (type long)
*          variable         - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain all characters in string
*          usage:
*            CStringIterator iteratorVariable;
*            Codepoint       variable;
*            CSTRING_CHAR_ITERATE(string,iteratorVariable,variable)
*            {
*              ... = variable
*            }
\***********************************************************************/

#define CSTRING_CHAR_ITERATE(string,iteratorVariable,variable) \
  for (stringIteratorInit(&iteratorVariable,string), variable = stringIteratorGet(&iteratorVariable); \
       !stringIteratorEnd(&iteratorVariable); \
       stringIteratorNext(&iteratorVariable), variable = stringIteratorGet(&iteratorVariable) \
      )

/***********************************************************************\
* Name   : HALT, HALT_INSUFFICIENT_MEMORY, HALT_FATAL_ERROR,
*          HALT_INTERNAL_ERROR, HALT_INTERNAL_ERROR_AT,
*          HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED,
*          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE,
*          HALT_INTERNAL_ERROR_UNREACHABLE,
*          HALT_INTERNAL_ERROR_LOST_RESOURCE
* Purpose: halt macros
* Input  : errorLevel - error level
*          format     - format string
*          args       - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

// prefixes
#define HALT_PREFIX_FATAL_ERROR    "FATAL ERROR: "
#define HALT_PREFIX_INTERNAL_ERROR "INTERNAL ERROR: "

// 2 macros necessary, because of "string"-construction
#define __HALT_STRING1(s) __HALT_STRING2(s)
#define __HALT_STRING2(s) #s
#undef HALT
#ifdef NDEBUG
#define HALT(errorLevel, format, ...) \
  do \
  { \
    __halt(errorLevel,format, ## __VA_ARGS__); \
  } \
  while (0)

#define HALT_INSUFFICIENT_MEMORY(...) \
  do \
  { \
     __abort(HALT_PREFIX_FATAL_ERROR,"Insufficient memory", ## __VA_ARGS__); \
  } \
 while (0)

#define HALT_FATAL_ERROR(format, ...) \
  do \
  { \
     __abort(HALT_PREFIX_FATAL_ERROR,format, ## __VA_ARGS__); \
  } \
 while (0)

#define HALT_INTERNAL_ERROR(format, ...) \
  do \
  { \
     __abort(HALT_PREFIX_INTERNAL_ERROR, format, ## __VA_ARGS__); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE() \
  do \
  { \
     __abort(HALT_PREFIX_INTERNAL_ERROR, "Unhandled switch case"); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASEX(format, ...) \
  do \
  { \
     __abort(HALT_PREFIX_INTERNAL_ERROR, "Unhandled switch case " format, ## __VA_ARGS__); \
  } \
  while (0)
#else /* not NDEBUG */
#define HALT(errorLevel, format, ...) \
  do \
  { \
    __halt(__FILE__,__LINE__,errorLevel,format, ## __VA_ARGS__); \
  } \
  while (0)

#define HALT_INSUFFICIENT_MEMORY(...) \
  do \
  { \
     __abort(__FILE__,__LINE__,HALT_PREFIX_FATAL_ERROR,"Insufficient memory", ## __VA_ARGS__); \
  } \
 while (0)

#define HALT_FATAL_ERROR(format, ...) \
  do \
  { \
     __abort(__FILE__,__LINE__,HALT_PREFIX_FATAL_ERROR,format, ## __VA_ARGS__); \
  } \
 while (0)

#define HALT_INTERNAL_ERROR(format, ...) \
  do \
  { \
     __abort(__FILE__,__LINE__,HALT_PREFIX_INTERNAL_ERROR, format, ## __VA_ARGS__); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE() \
  do \
  { \
     __abort(__FILE__,__LINE__,HALT_PREFIX_INTERNAL_ERROR, "Unhandled switch case"); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASEX(format, ...) \
  do \
  { \
     __abort(__FILE__,__LINE__,HALT_PREFIX_INTERNAL_ERROR, "Unhandled switch case " format, ## __VA_ARGS__); \
  } \
  while (0)
#endif /* NDEBUG */
#define HALT_INTERNAL_ERROR_AT(file, line, format, ...) \
  do \
  { \
     __abortAt(file,line,HALT_PREFIX_INTERNAL_ERROR,format, ## __VA_ARGS__); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED() \
  do \
  { \
     HALT_INTERNAL_ERROR("Still not implemented"); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_UNREACHABLE() \
  do \
  { \
     HALT_INTERNAL_ERROR("Unreachable code"); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_LOST_RESOURCE() \
  do \
  { \
     HALT_INTERNAL_ERROR("Lost resource"); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_NOT_SUPPORTED() \
  do \
  { \
     HALT_INTERNAL_ERROR("Not supported"); \
  } \
  while (0)

/***********************************************************************\
* Name   : FAIL
* Purpose: fail macros
* Input  : errorLevel - error level
*          format     - format string
*          args       - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

/* 2 macros necessary, because of "string"-construction */
#define __FAIL_STRING1(s) __FAIL_STRING2(s)
#define __FAIL_STRING2(s) #s
#define FAIL(errorLevel, format, ...) \
  do \
  { \
   fprintf(stderr, format " - fail in file " __FILE__ ", line " __FAIL_STRING1(__LINE__) "\n" , ## __VA_ARGS__); \
   exit(errorLevel);\
  } \
  while (0)

/***********************************************************************\
* Name   : MEMSET, MEMCLEAR
* Purpose: set/clear memory macros
* Input  : p     - pointer
*          value - value
*          size  - size (in bytes)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define MEMSET(p,value,size) memset(p,value,size)

#define MEMCLEAR(p,size) memset(p,0,size)

/***********************************************************************\
* Name   : _
* Purpose: internationalization text macro
* Input  : text - text
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef HAVE_LIBINTL_H
  #define _(text) gettext(text)
#else
  #define _(text) text
#endif /* HAVE_LIBINTL_H */

/***********************************************************************\
* Name   : assertx
* Purpose: extended assert
* Input  : condition - condition
*          format    - format string
*          ...       - option arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  /* 2 macros necessary, because of "string"-construction */
  #define __ASSERTX_STRING1(s) __ASSERTX_STRING2(s)
  #define __ASSERTX_STRING2(s) #s
  #define assertx(condition, format, ...) \
    do \
    { \
      if (!(condition)) \
      { \
        fprintf(stderr, "%s:%d: %s: Assertion '%s' failed: ",__FILE__,__LINE__,__FUNCTION__,__ASSERTX_STRING1(condition)); \
        fprintf(stderr, format "\n" , ## __VA_ARGS__); \
        abort(); \
      } \
    } \
    while (0)
#else /* NDEBUG */
  #define assertx(condition, format, ...) \
    do \
    { \
    } \
    while (0)
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : __B
* Purpose: breakpoint
* Input  : -
* Output : -
* Return : -
* Notes  : for debugging only!
\***********************************************************************/

#define __B() do { } while (0)
#if defined(__x86_64__) || defined(__i386)
  #undef __B
  #define __B() \
    do \
    { \
      fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); asm("int3"); \
    } \
    while (0)
#endif
#if defined(__arm)
  #undef __B
  #define __B() \
    do \
    { \
      fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); asm("bkpt 0"); \
    } \
    while (0)
#endif

/***********************************************************************\
* Name   : DEBUG_MEMORY_FENCE, DEBUG_MEMORY_FENCE_INIT,
*          DEBUG_MEMORY_FENCE_CHECK
* Purpose: declare/init/check memory fences
* Input  : name - variable name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG

  #define DEBUG_MEMORY_FENCE(name) byte name[0]
  #define DEBUG_MEMORY_FENCE_INIT(name) \
    do \
    { \
      unsigned int __z; \
      \
      for (__z = 0; __z < sizeof(name); __z++) \
      { \
        name[__z] = 0xED; \
      } \
    } \
    while (0)
  #define DEBUG_MEMORY_FENCE_CHECK(name) \
    do \
    { \
      unsigned int __z; \
      for (__z = 0; __z < sizeof(name); __z++) \
      { \
        assert(name[__z] == 0xED); \
      } \
    } \
    while (0)

#else /* not NDEBUG */

  #define DEBUG_MEMORY_FENCE(name)
  #define DEBUG_MEMORY_FENCE_INIT(name) \
    do \
    { \
    } \
    while (0)
  #define DEBUG_MEMORY_FENCE_CHECK(name) \
    do \
    { \
    } \
    while (0)

#endif /* NDEBUG */

/***********************************************************************\
* Name   : DEBUG_TEST_CODE
* Purpose: execute test code
* Input  : -
* Output : -
* Return : -
* Notes  : test code is executed if:
*            - environment variable TESTCODE contains testcode name
*          or
*            - text file specified by environment varibale TESTCODE_LIST
*              contains testcode name and
*            - text file specified by environment varibale TESTCODE_SKIP
*              does not testcode contain name
*            - text file specified by environment varibale TESTCODE_DONE
*              does not testcode contain name
*          If environment variable TESTCODE_NAME is defined the name of
*          executed testcode is written to that text file.
*          If environment variable TESTCODE_DONE is defined the name of
*          executed testcode is added to that text file.
\***********************************************************************/

#ifndef NDEBUG
  #define DEBUG_TESTCODE() \
    if (debugIsTestCodeEnabled(__FILE__,__LINE__,__FUNCTION__,__COUNTER__))
// TODO: remove
  #define DEBUG_TESTCODE2(name,codeBody) \
    void (*__testcode__ ## __LINE__)(const char*) = ({ \
                                          auto void __closure__(const char *); \
                                          void __closure__(const char *__testCodeName__)codeBody __closure__; \
                                        }); \
    if (debugIsTestCodeEnabled(__FILE__,__LINE__,__FUNCTION__,__COUNTER__)) { __testcode__ ## __LINE__(name); }
#else /* not NDEBUG */
  #define DEBUG_TESTCODE(name) \
    if (FALSE)
#endif /* NDEBUG */

/***********************************************************************\
* Name   : DEBUG_TESTCODE_ERROR
* Purpose: get test code error code
* Input  : -
* Output : -
* Return : test code error code
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  #define DEBUG_TESTCODE_ERROR() \
    debugTestCodeError(__FILE__,__LINE__)
#else /* not NDEBUG */
  #define DEBUG_TESTCODE_ERROR() \
    ERROR_NONE
#endif /* NDEBUG */

/***********************************************************************\
* Name   : IS_DEBUG_TESTCODE
* Purpose: true if test code is executed
* Input  : name - test code name
* Output : -
* Return : TRUE iff test code is executed
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  #define IS_DEBUG_TESTCODE(name) \
    ((__testCodeName__ != NULL) && stringEquals(__testCodeName__,name))
#else /* not NDEBUG */
  #define IS_DEBUG_TESTCODE(name) \
    FALSE
#endif /* NDEBUG */

/***********************************************************************\
* Name   : DEBUG_LOCAL_RESOURCE
* Purpose: mark resource as local resource only
* Input  : resource  - resource
*          initValue - init value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG

  #define DEBUG_LOCAL_RESOURCE(resource,initValue) \
    do \
    { \
      debugLocalResource(__FILE__,__LINE__,resource); \
      resource = initValue; \
    } \
    while (0)

#else /* NDEBUG */

  #define DEBUG_LOCAL_RESOURCE(resource) \
    do \
    { \
    } \
    while (0)

#endif /* not NDEBUG */

/***********************************************************************\
* Name   : DEBUG_ADD_RESOURCE_TRACE, DEBUG_REMOVE_RESOURCE_TRACE,
*          DEBUG_ADD_RESOURCE_TRACEX, DEBUG_REMOVE_RESOURCE_TRACEX,
*          DEBUG_CHECK_RESOURCE_TRACE
* Purpose: add/remove debug trace allocated resource functions, check if
*          resource allocated
* Input  : fileName - file name
*          lineNb   - line number
*          resource - resource
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
  // 2 macros necessary, because of "string"-construction
  #define __DEBUG_RESOURCE_TRACE__STRING1(s) __DEBUG_RESOURCE_TRACE__STRING2(s)
  #define __DEBUG_RESOURCE_TRACE__STRING2(s) #s

  #define DEBUG_ADD_RESOURCE_TRACE(resource,type) \
    do \
    { \
      debugAddResourceTrace(__FILE__,__LINE__,__DEBUG_RESOURCE_TRACE__STRING1(type),__DEBUG_RESOURCE_TRACE__STRING1(resource),resource); \
    } \
    while (0)

  #define DEBUG_REMOVE_RESOURCE_TRACE(resource,type) \
    do \
    { \
      debugRemoveResourceTrace(__FILE__,__LINE__,__DEBUG_RESOURCE_TRACE__STRING1(type),__DEBUG_RESOURCE_TRACE__STRING1(resource),resource); \
    } \
    while (0)

  #define DEBUG_ADD_RESOURCE_TRACEX(fileName,lineNb,resource,type) \
    do \
    { \
      debugAddResourceTrace(fileName,lineNb,__DEBUG_RESOURCE_TRACE__STRING1(type),__DEBUG_RESOURCE_TRACE__STRING1(resource),resource); \
    } \
    while (0)

  #define DEBUG_REMOVE_RESOURCE_TRACEX(fileName,lineNb,resource,type) \
    do \
    { \
      debugRemoveResourceTrace(fileName,lineNb,__DEBUG_RESOURCE_TRACE__STRING1(type),__DEBUG_RESOURCE_TRACE__STRING1(resource),resource); \
    } \
    while (0)

  #define DEBUG_CHECK_RESOURCE_TRACE(resource) \
    do \
    { \
      debugCheckResourceTrace(__FILE__,__LINE__,__DEBUG_RESOURCE_TRACE__STRING1(resource),resource); \
    } \
    while (0)

  #define DEBUG_CHECK_RESOURCE_TRACEX(fileName,lineNb,resource) \
    do \
    { \
      debugCheckResourceTrace(fileName,lineNb,__DEBUG_RESOURCE_TRACE__STRING1(resource),resource); \
    } \
    while (0)

#else /* NDEBUG */

  #define DEBUG_ADD_RESOURCE_TRACE(resource,size) \
    do \
    { \
    } \
    while (0)

  #define DEBUG_REMOVE_RESOURCE_TRACE(resource,size) \
    do \
    { \
    } \
    while (0)

  #define DEBUG_ADD_RESOURCE_TRACEX(fileName,lineNb,resource,size) \
    do \
    { \
    } \
    while (0)

  #define DEBUG_REMOVE_RESOURCE_TRACEX(fileName,lineNb,resource,size) \
    do \
    { \
    } \
    while (0)

  #define DEBUG_CHECK_RESOURCE_TRACE(resource) \
    do \
    { \
    } \
    while (0)

  #define DEBUG_CHECK_RESOURCE_TRACEX(fileName,lineNb,resource) \
    do \
    { \
    } \
    while (0)

#endif /* not NDEBUG */

#ifdef HAVE_BACKTRACE
  #define BACKTRACE(stackTrace,stackTraceSize) \
    do \
    { \
      (stackTraceSize) = getStackTrace(stackTrace,SIZE_OF_ARRAY(stackTrace)); \
    } \
    while (0)
#else /* not HAVE_BACKTRACE */
  #define BACKTRACE(stackTrace,stackTraceSize) \
    do \
    { \
    } \
    while (0)
#endif /* HAVE_BACKTRACE */

/* begin macro iterator
   Link: http://jhnet.co.uk/articles/cpp_magic
*/
#define _ITERATOR_FIRST(a, ...) a
#define _ITERATOR_SECOND(a, b, ...) b

#define _ITERATOR_EMPTY()

// Note: O(n^2) memory usage!
#define _ITERATOR_EVAL(...)   _ITERATOR_EVAL10(__VA_ARGS__)
#define _ITERATOR_EVAL17(...) _ITERATOR_EVAL16(_ITERATOR_EVAL16(__VA_ARGS__))
#define _ITERATOR_EVAL16(...) _ITERATOR_EVAL15(_ITERATOR_EVAL15(__VA_ARGS__))
#define _ITERATOR_EVAL15(...) _ITERATOR_EVAL14(_ITERATOR_EVAL14(__VA_ARGS__))
#define _ITERATOR_EVAL14(...) _ITERATOR_EVAL13(_ITERATOR_EVAL13(__VA_ARGS__))
#define _ITERATOR_EVAL13(...) _ITERATOR_EVAL12(_ITERATOR_EVAL12(__VA_ARGS__))
#define _ITERATOR_EVAL12(...) _ITERATOR_EVAL11(_ITERATOR_EVAL11(__VA_ARGS__))
#define _ITERATOR_EVAL11(...) _ITERATOR_EVAL10(_ITERATOR_EVAL10(__VA_ARGS__))
#define _ITERATOR_EVAL10(...) _ITERATOR_EVAL09(_ITERATOR_EVAL09(__VA_ARGS__))
#define _ITERATOR_EVAL09(...) _ITERATOR_EVAL08(_ITERATOR_EVAL08(__VA_ARGS__))
#define _ITERATOR_EVAL08(...) _ITERATOR_EVAL07(_ITERATOR_EVAL07(__VA_ARGS__))
#define _ITERATOR_EVAL07(...) _ITERATOR_EVAL06(_ITERATOR_EVAL06(__VA_ARGS__))
#define _ITERATOR_EVAL06(...) _ITERATOR_EVAL05(_ITERATOR_EVAL05(__VA_ARGS__))
#define _ITERATOR_EVAL05(...) _ITERATOR_EVAL04(_ITERATOR_EVAL04(__VA_ARGS__))
#define _ITERATOR_EVAL04(...) _ITERATOR_EVAL03(_ITERATOR_EVAL03(__VA_ARGS__))
#define _ITERATOR_EVAL03(...) _ITERATOR_EVAL02(_ITERATOR_EVAL02(__VA_ARGS__))
#define _ITERATOR_EVAL02(...) _ITERATOR_EVAL01(_ITERATOR_EVAL01(__VA_ARGS__))
#define _ITERATOR_EVAL01(...) __VA_ARGS__

#define _ITERATOR_DEFER1(m) m _ITERATOR_EMPTY()
#define _ITERATOR_DEFER2(m) m _ITERATOR_EMPTY _ITERATOR_EMPTY()()
#define _ITERATOR_DEFER3(m) m _ITERATOR_EMPTY _ITERATOR_EMPTY _ITERATOR_EMPTY()()()
#define _ITERATOR_DEFER4(m) m _ITERATOR_EMPTY _ITERATOR_EMPTY _ITERATOR_EMPTY _ITERATOR_EMPTY()()()()

#define _ITERATOR_IS_PROBE(...) _ITERATOR_SECOND(__VA_ARGS__, 0)
#define _ITERATOR_PROBE() ~, 1

#define _ITERATOR_CAT(a,b) a ## b

#define _ITERATOR_NOT(x) _ITERATOR_IS_PROBE(_ITERATOR_CAT(_ITERATOR_NOT_, x))
#define _ITERATOR_NOT_0 _ITERATOR_PROBE()

#define _ITERATOR_BOOL(x) _ITERATOR_NOT(_ITERATOR_NOT(x))

#define _ITERATOR_IF_ELSE(condition)  __ITERATOR_IF_ELSE(_ITERATOR_BOOL(condition))
#define __ITERATOR_IF_ELSE(condition) _ITERATOR_CAT(_ITERATOR_IF_, condition)

#define _ITERATOR_IF_1(...) __VA_ARGS__ _ITERATOR_IF_1_ELSE
#define _ITERATOR_IF_0(...)             _ITERATOR_IF_0_ELSE

#define _ITERATOR_IF_1_ELSE(...)
#define _ITERATOR_IF_0_ELSE(...) __VA_ARGS__

#define _ITERATOR_HAS_ARGS(...) _ITERATOR_BOOL(_ITERATOR_FIRST(_ITERATOR_END_OF_ARGUMENTS_ __VA_ARGS__)())
#define _ITERATOR_END_OF_ARGUMENTS_() 0

#if 0
    // original without support of empty va-arg list
    #define _ITERATOR_MAP(prefix, first, ...) \
    prefix (first) \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
        _ITERATOR_DEFER2(__ITERATOR_MAP)()(prefix, __VA_ARGS__) \
    ) \
    ( \
        /* Do nothing, just terminate */ \
    )
    #define __ITERATOR_MAP() _ITERATOR_MAP

    #define _ITERATOR_MAP_COUNT(first, ...) \
    1+ \
    _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
    ( \
        _ITERATOR_DEFER2(__ITERATOR_MAP_COUNT)()(__VA_ARGS__) \
    ) \
    ( \
        /* nothing to do */ \
    )
    #define __ITERATOR_MAP_COUNT() _ITERATOR_MAP_COUNT
#endif /* 0 */

#define __ITERATOR_MAP(prefix, first, ...) \
  prefix (first) \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_DEFER2(___ITERATOR_MAP)()(prefix, __VA_ARGS__) \
  ) \
  ( \
    /* nothing to do */ \
  )
#define ___ITERATOR_MAP() __ITERATOR_MAP

#define _ITERATOR_MAP(prefix, ...) \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_EVAL(__ITERATOR_MAP(prefix, __VA_ARGS__)) \
  ) \
  ( \
    /* nothing to do */ \
  )

#define __ITERATOR_MAP_COUNT(first, ...) \
  1+ \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_DEFER2(___ITERATOR_MAP_COUNT)()(__VA_ARGS__) \
  ) \
  ( \
    /* nothing to do */ \
  )
#define ___ITERATOR_MAP_COUNT() __ITERATOR_MAP_COUNT

#define _ITERATOR_MAP_COUNT(...) \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_EVAL(__ITERATOR_MAP_COUNT(__VA_ARGS__)) \
  ) \
  ( \
    /* nothing to do */ \
  )

// get array [..][0] from array[..][3]
#define _ITERATOR_ARRAY0(v0,v1,v3,...) \
  v0, \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_DEFER2(__ITERATOR_ARRAY0)()(__VA_ARGS__) \
  ) \
  ( \
    /* nothing to do */ \
  )
#define __ITERATOR_ARRAY0() _ITERATOR_ARRAY0 \

// get array [..][1] from array[..][3]
#define _ITERATOR_ARRAY1(v0,v1,v2,...) \
  v1, \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_DEFER2(__ITERATOR_ARRAY1)()(__VA_ARGS__) \
  ) \
  ( \
    /* nothing to do */ \
  )
#define __ITERATOR_ARRAY1() _ITERATOR_ARRAY1 \

// get array [..][0,1] from array[..][3]
#define _ITERATOR_ARRAY2(v0,v1,v2,...) \
  v2, \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_DEFER2(__ITERATOR_ARRAY2)()(__VA_ARGS__) \
  ) \
  ( \
    /* nothing to do */ \
  )
#define __ITERATOR_ARRAY2() _ITERATOR_ARRAY2 \

// get array [..][0,1] from array[..][3]
#define _ITERATOR_ARRAY01(v0,v1,v3,...) \
  { v0,v1 }, \
  _ITERATOR_IF_ELSE(_ITERATOR_HAS_ARGS(__VA_ARGS__)) \
  ( \
    _ITERATOR_DEFER2(__ITERATOR_ARRAY01)()(__VA_ARGS__) \
  ) \
  ( \
    /* nothing to do */ \
  )
#define __ITERATOR_ARRAY01() _ITERATOR_ARRAY01 \

//#define fooMacro(x) FOO_ ## x
//#define foo(...) _ITERATOR_EVAL(_ITERATOR_MAP(fooMacro, __VA_ARGS__))
//#define fooCount(...) _ITERATOR_EVAL(_ITERATOR_MAP_COUNT(__VA_ARGS__)) 0
//foo(a,b,...) -> FOO_a, FOO_b, ...
/* end macro iterator */

#ifndef NDEBUG
  #define allocSecure(...) __allocSecure(__FILE__,__LINE__, ## __VA_ARGS__)
  #define freeSecure(...)  __freeSecure (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/**************************** Functions ********************************/

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NDEBUG
/***********************************************************************\
* Name   : __dprintf__
* Purpose: debug printf
* Input  : __fileName__ - file name
*          __lineNb__   - line number
*          format       - format string (like printf)
*          ...          - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void __dprintf__(const char *__fileName__,
                 ulong      __lineNb__,
                 const char *format,
                 ...
                );
#endif /* NDEBUG */

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : gcd
* Purpose: calculate greatest common divisor
* Input  : a,b - values
* Output : -
* Return : geatest common devisor or 0 if none
* Notes  : -
\***********************************************************************/

unsigned long gcd(unsigned long a, unsigned long b);

/***********************************************************************\
* Name   : lcm
* Purpose: calculate least common multiple
* Input  : a,b - value
* Output : -
* Return : least common multiple or 0 if a=b=0
* Notes  : -
\***********************************************************************/

unsigned long lcm(unsigned long a, unsigned long b);

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : getCycleCounter
* Purpose: get CPU cycle counter
* Input  : -
* Output : -
* Return : cycle counter
* Notes  : -
\***********************************************************************/

static inline uint64 getCycleCounter(void)
{
  #ifdef PLATFORM_LINUX
    #if defined(__x86_64__) || defined(__i386)
      unsigned int l,h;

      asm __volatile__ ("rdtsc" : "=a" (l), "=d" (h));

      return ((uint64)h << 32) | ((uint64)l << 0);
    #else
      return 0LL;
    #endif
  #elif PLATFORM_WINDOWS
    return __rdtsc();
  #else
    return 0LL;
  #endif /* PLATFORM_... */
}

/***********************************************************************\
* Name   : getStackTrace
* Purpose: get stack trace
* Input  : stackTrace        - stack trace variable
*          maxStackTraceSize - max. stack trace size
* Output : stackTrace - stack trace
* Return : stack trace size
* Notes  : -
\***********************************************************************/

static inline uint getStackTrace(void const * stackTrace[], uint maxStackTraceSize)
{
  uint stackTraceSize;
  #ifdef HAVE_BACKTRACE
    uint i;
  #endif

  #ifdef HAVE_BACKTRACE
    stackTraceSize = (uint)backtrace((void**)stackTrace,maxStackTraceSize);
    for (i = 0; i < stackTraceSize; i++)
    {
      stackTrace[i] = (void const **)((const byte*)stackTrace[i]-1);
    }
  #else
    UNUSED_VARIABLE(stackTrace);
    UNUSED_VARIABLE(maxStackTraceSize);

    stackTraceSize = 0;
  #endif

  return stackTraceSize;
}

/***********************************************************************\
* Name   : atomicIncrement
* Purpose: atomic increment value
* Input  : n - value
*          d - delta
* Output : -
* Return : old value
* Notes  : -
\***********************************************************************/

static inline uint atomicIncrement(uint *n, int d)
{
  assert(n != NULL);

  return __sync_fetch_and_add(n,d);
}

/***********************************************************************\
* Name   : atomicIncrement64
* Purpose: atomic increment value
* Input  : n - value
*          d - delta
* Output : -
* Return : old value
* Notes  : -
\***********************************************************************/

static inline uint64 atomicIncrement64(uint64 *n, int d)
{
  assert(n != NULL);

  return __sync_fetch_and_add(n,d);
}

/***********************************************************************\
* Name   : atomicCompareSwap
* Purpose: atomic increment value
* Input  : n                 - value
*          oldValue,newValue - old/new value
* Output : -
* Return : TURE iff swapped
* Notes  : -
\***********************************************************************/

static inline bool atomicCompareSwap32(uint *n, uint32 oldValue, uint32 newValue)
{
  assert(n != NULL);

  return __sync_bool_compare_and_swap(n,oldValue,newValue);
}

/***********************************************************************\
* Name   : atomicCompareSwap64
* Purpose: atomic increment value
* Input  : n                 - value
*          oldValue,newValue - old/new value
* Output : -
* Return : TURE iff swapped
* Notes  : -
\***********************************************************************/

static inline bool atomicCompareSwap64(uint *n, uint64 oldValue, uint64 newValue)
{
  assert(n != NULL);

  return __sync_bool_compare_and_swap(n,oldValue,newValue);
}

/***********************************************************************\
* Name   : swapBytes16
* Purpose: swap bytes of 16bit value
* Input  : n - word (a:b)
* Output : -
* Return : swapped word (b:a)
* Notes  : -
\***********************************************************************/

static inline uint16_t swapBytes16(uint16_t n)
{
  return   ((n & 0xFF00) >> 8)
         | ((n & 0x00FF) << 8);
}

/***********************************************************************\
* Name   : swapBytes32
* Purpose: swap bytes of 32bit value
* Input  : n - long (a:b:c:d)
* Output : -
* Return : swapped long (d:c:b:a)
* Notes  : -
\***********************************************************************/

static inline uint32_t swapBytes32(uint32_t n)
{
  return   ((n & 0xFF000000) >> 24)
         | ((n & 0x00FF0000) >>  8)
         | ((n & 0x0000FF00) <<  8)
         | ((n & 0x000000FF) << 24);
}

/***********************************************************************\
* Name   : memFill
* Purpose: fill memory
* Input  : p - memory address
*          n - size of memory [bytes]
*          d - fill value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

static inline void *memFill(void *p, size_t n, byte d)
{
  memset(p,d,n);

  return p;
}

/***********************************************************************\
* Name   : memClear
* Purpose: clear memory content (fill with 0)
* Input  : p - memory address
*          n - size of memory [bytes]
* Output : -
* Return : p
* Notes  : -
\***********************************************************************/

static inline void *memClear(void *p, size_t n)
{
  memFill(p,n,0);

  return p;
}

/***********************************************************************\
* Name   : memCopy
* Purpose: copy memory (may overlap)
* Input  : p0,p1 - destination/source memory address
*          n0,n1 - destination/source size [bytes]
* Output : -
* Return : p0
* Notes  : clear rest of memory in p0 if n0 > n1
\***********************************************************************/

static inline void *memCopy(void *p0, size_t n0, const void *p1, size_t n1)
{
  size_t n;

  assert(p0 != NULL);
  assert(p1 != NULL);

  n = MIN(n0,n1);
  if (   ((p0 > p1) && ((size_t)((byte*)p0-(byte*)p1) < n))
      || ((p1 > p0) && ((size_t)((byte*)p1-(byte*)p0) < n))
     )
  {
    // memory overlap
    memmove(p0,p1,n);
  }
  else
  {
    // memory do not overlap
    memcpy(p0,p1,n);
  }
  if (n0 > n1) memset((byte*)p0+n,0,n0-n);

  return p0;
}

/***********************************************************************\
* Name   : memCopyFast
* Purpose: copy memory (must not overlap)
* Input  : p0,p1 - destination/source memory address
*          n0,n1 - destination/source size [bytes]
* Output : -
* Return : p0
* Notes  : clear rest of memory in p0 if n0 > n1
\***********************************************************************/

static inline void *memCopyFast(void *p0, size_t n0, const void *p1, size_t n1)
{
  size_t n;

  assert(p0 != NULL);
  assert(p1 != NULL);

  n = MIN(n0,n1);
  // memory must not overlap
  assert(   ((p0 < p1) || ((size_t)((byte*)p0-(byte*)p1)) >= n)
         && ((p1 < p0) || ((size_t)((byte*)p1-(byte*)p0)) >= n)
        );
  memcpy(p0,p1,n);
  if (n0 > n1) memset((byte*)p0+n,0,n0-n);

  return p0;
}

/***********************************************************************\
* Name   : memEquals
* Purpose: check if memory content equals
* Input  : p0,p1 - memory addresses
*          n0,n1 - sizes [bytes]
* Output : -
* Return : TRUE iff memory content equals
* Notes  : -
\***********************************************************************/

static inline bool memEquals(const void *p0, size_t n0, const void *p1, size_t n1)
{
  return (n0 == n1) && (memcmp(p0,p1,n0) == 0);
}

/***********************************************************************\
* Name   : duplicate
* Purpose: duplicate memory block
* Input  : p0 - memory address variable
*          n0 - memory block size variable (can be NULL)
*          p1 - memory block
*          n1 - memory block size
* Output : p0 - allocated and copied memory
*          n9 - memory block size
* Return : p0
* Notes  : -
\***********************************************************************/

static inline void *duplicate(void **p0, size_t *n0, const void *p1, size_t n1)
{
  assert(p0 != NULL);

  (*p0) = malloc(n1);
  if ((*p0) != NULL)
  {
    memCopyFast(*p0,n1,p1,n1);
    if (n0 != NULL) (*n0) = n1;
  }

  return *p0;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : initSecure
* Purpose: init secure memory
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors initSecure(void);

/***********************************************************************\
* Name   : doneSecure
* Purpose: done secure memory
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void doneSecure(void);

/***********************************************************************\
* Name   : allocSecure
* Purpose: allocate secure memory
* Input  : size - size of memory block
* Output : -
* Return : secure memory or NULL iff insufficient memory
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void *allocSecure(size_t size);
#else /* not NDEBUG */
void *__allocSecure(const char *__fileName__,
                    ulong      __lineNb__,
                    size_t     size
                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : freeSecure
* Purpose: free secure memory
* Input  : p - secure memory
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void freeSecure(void *p);
#else /* not NDEBUG */
void __freeSecure(const char *__fileName__,
                  ulong      __lineNb__,
                  void       *p
                 );
#endif /* NDEBUG */

/*---------------------------------------------------------------------*/

#ifdef __cplusplus

/***********************************************************************\
* Name   : isNaN
* Purpose: check if not-a-number (NaN)
* Input  : n - numer
* Output : -
* Return : TRUE if n is NaN, FALSE otherwise
* Notes  : -
\***********************************************************************/

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wfloat-equal"
static inline bool isNaN(double n)
{
  return n != n;
}
#pragma GCC pop_options

/***********************************************************************\
* Name   : isInf
* Purpose: check if number is infinit
* Input  : n - number
* Output : -
* Return : TRUE if n is infinit, FALSE otherwise
* Notes  : -
\***********************************************************************/

static inline bool isInf(double n)
{
  return (n < -MAX_DOUBLE) || (n > MAX_DOUBLE);
}

#endif

#ifdef __cplusplus

/***********************************************************************\
* Name   : radToDegree
* Purpose: convert rad to degree
* Input  : n - angle in rad
* Output : -
* Return : angle in degree
* Notes  : -
\***********************************************************************/

static inline double radToDegree(double n)
{
//???  ASSERT_NaN(n);
//  n=fmod(n,2*PI);
//  if (n<0) n+=2*PI;
  return n*180/PI;
}

/***********************************************************************\
* Name   : degreeToRad
* Purpose: convert degree to rad
* Input  : n - angle in degree
* Output : -
* Return : angle in rad
* Notes  : -
\***********************************************************************/

static inline double degreeToRad(double n)
{
//???  ASSERT_NaN(n);
//  n=fmod(n,360);
//  if (n<0) n+=360;
  return n*PI/180;
}

#endif

#ifdef __cplusplus

/***********************************************************************\
* Name   : normRad
* Purpose: normalize angle in rad (0..2PI)
* Input  : n - angle in rad
* Output : -
* Return : normalized angle (0..2PI)
* Notes  : -
\***********************************************************************/

static inline double normRad(double n)
{
//???  ASSERT(!IsNaN(n));
  n = fmod(n,2 * PI);
  if (n < 0) n += 2*PI;
  return n;
}

/***********************************************************************\
* Name   : normDegree
* Purpose: normalize angle in degree (0..360)
* Input  : n - angle in degree
* Output : -
* Return : normalize angle (0..360)
* Notes  : -
\***********************************************************************/

static inline double normDegree(double n)
{
//???  ASSERT(!IsNaN(n));
  n = fmod(n,360);
  if (n < 0) n += 360;
  return n;
}

/***********************************************************************\
* Name   : normRad90
* Purpose: normalize angle in rad (-PI/2..PI/2)
* Input  : n - angle in rad
* Output : -
* Return : normalized angle (-PI/2..PI/2)
* Notes  : PI/2..3PI/2   = -PI/2..PI/2
*          3PI/2..2PI    = -PI/2..0
*          -PI/2..-3PI/2 = PI/2..-PI/2
*          -3PI/2..-2PI  = PI/2..0
\***********************************************************************/

static inline double normRad90(double n)
{
//???  ASSERT(!IsNaN(n));
  if (n >  3*PI/2) n -= 2*PI;
  if (n < -3*PI/2) n += 2*PI;
  if (n >    PI/2) n -= PI;
  if (n <   -PI/2) n += PI;
  return n;
}

/***********************************************************************\
* Name   : normRad180
* Purpose: normalize angle in rad (-PI..PI)
* Input  : n - angle in rad
* Output : -
* Return : normalized angle (-PI..PI)
* Notes  : -
\***********************************************************************/

static inline double normRad180(double n)
{
//???  ASSERT(!IsNaN(n));
  return (n > PI) ? (n-2*PI) : ((n < -PI) ? n+2*PI : n);
}

/***********************************************************************\
* Name   : normDegree90
* Purpose: normalize angle in rad (-90..90)
* Input  : n - angle in degree
* Output : -
* Return : normalized angle (-90..90)
* Notes  : 90..270    = -90..
*          270..360   = -90..0
*          -90..-270  = 90..-90
*          -270..-360 = 90..0
\***********************************************************************/

static inline double normDegree90(double n)
{
//???  ASSERT(!IsNaN(n));
  if (n >  270) n -= 360;
  if (n < -270) n += 360;
  if (n >   90) n -= 180;
  if (n <  -90) n += 180;
  return n;
}

/***********************************************************************\
* Name   : normDegree180
* Purpose: normalize angle in degree (-180..180)
* Input  : n - angle in degree
* Output : -
* Return : normalize angle (-180..180)
* Notes  : -
\***********************************************************************/

static inline double normDegree180(double n)
{
//???  ASSERT(!IsNaN(n));
  return (n > 180) ? (n-360) : ((n<-180) ? n+360 : n);
}

/***********************************************************************\
* Name   : normRad360
* Purpose: normalize angle in rad (-2PI...2PI)
* Input  : n - angle in rad
* Output : -
* Return : normalized angle (-2PI..2PI)
* Notes  : -
\***********************************************************************/

static inline double normRad360(double n)
{
//???  ASSERT(!IsNaN(n));
  return fmod(n,2*PI);
}

/***********************************************************************\
* Name   : normDegree360
* Purpose: normalize angle in degree (-360..360)
* Input  : n - angle in degree
* Output : -
* Return : normalize angle (-360..360)
* Notes  : -
\***********************************************************************/

static inline double normDegree360(double n)
{
//???  ASSERT(!IsNaN(n));
  return fmod(n,360);
}

#endif

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : initSimpleHash
* Purpose: initialize simple hash
* Input  : hash - hash variable
* Output : hash - initialzied hash
* Return : -
* Notes  : -
\***********************************************************************/

static inline void initSimpleHash(uint32 *hash)
{
  assert(hash != NULL);

  (*hash) = 0L;
}

/***********************************************************************\
* Name   : updateSimpleHash
* Purpose: update simple hash
* Input  : hash - hash variable
*          b    - value
* Output : hash - updated hash variable
* Return : -
* Notes  : -
\***********************************************************************/

static inline void updateSimpleHash(uint32 *hash, byte b)
{
  assert(hash != NULL);

  (*hash) += b;
  (*hash) += (*hash) << 10;
  (*hash) ^= (*hash) >> 6;
}

/***********************************************************************\
* Name   : doneSimpleHash
* Purpose: done simple hash
* Input  : -
* Output : -
* Return : hash value
* Notes  : -
\***********************************************************************/

static inline uint32 doneSimpleHash(uint32 hash)
{
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;

  return hash;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : __halt
* Purpose: halt program
* Input  : __fileName__ - file name
*          __lineNb__   - line number
*          exitcode     - exitcode
*          format       - format string (like printf)
*          ...          - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void __halt(int        exitcode,
            const char *format,
            ...
           ) __attribute__((noreturn))
             __attribute__((format(printf,2,3)));
#else /* not NDEBUG */
void __halt(const char *__fileName__,
            ulong      __lineNb__,
            int        exitcode,
            const char *format,
            ...
           ) __attribute__((noreturn))
             __attribute__((format(printf,4,5)));
#endif /* NDEBUG */

/***********************************************************************\
* Name   : __abort
* Purpose: abort program
* Input  : __fileName__ - file name
*          __lineNb__   - line number
*          prefix       - prefix text
*          format       - format string (like printf)
*          ...          - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void __abort(const char *prefix,
             const char *format,
             ...
            ) __attribute__((noreturn))
              __attribute__((format(printf,2,3)));

#else /* not NDEBUG */
void __abort(const char *__fileName__,
             ulong      __lineNb__,
             const char *prefix,
             const char *format,
             ...
            ) __attribute__((noreturn))
              __attribute__((format(printf,4,5)));
#endif /* NDEBUG */
void __abortAt(const char *fileName,
             ulong        lineNb,
             const char   *prefix,
             const char   *format,
             ...
            ) __attribute__((noreturn))
              __attribute__((format(printf,4,5)));

#ifndef NDEBUG

/***********************************************************************\
* Name   : debugIsTestCodeEnabled
* Purpose: check if test code is enabled
* Input  : __fileName__ - file name
*          __lineNb__   - line number
*          functionName - function name
*          counter      - counter
* Output : -
* Return : TRUE iff test code is enabled
* Notes  : -
\***********************************************************************/

bool debugIsTestCodeEnabled(const char *__fileName__,
                            ulong      __lineNb__,
                            const char *functionName,
                            uint       counter
                           );

/***********************************************************************\
* Name   : debugTestCodeError
* Purpose: get test code error code and stop at breakpoint
* Input  : -
* Output : -
* Return : error code
* Notes  : stop when environment variable TESTCODE_STOP is set
\***********************************************************************/

Errors debugTestCodeError(const char *__fileName__,
                          ulong      __lineNb__
                         );

/***********************************************************************\
* Name   : debugLocalResource
* Purpose: mark resource as local resource (must be freed before function
*          exit)
* Input  : __fileName__ - file name
*          __lineNb__   - line number
*          resource     - resource
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugLocalResource(const char *__fileName__,
                        ulong      __lineNb__,
                        const void *resource
                       ) ATTRIBUTE_NO_INSTRUMENT_FUNCTION;
#endif /* not NDEBUG */

#ifndef NDEBUG
/***********************************************************************\
* Name   : debugAddResourceTrace
* Purpose: add resource to debug trace list
* Input  : __fileName__ - file name
*          __lineNb__   - line number
*          variableName - variable name
*          resource     - resource
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugAddResourceTrace(const char *__fileName__,
                           ulong      __lineNb__,
                           const char *typeName,
                           const char *variableName,
                           const void *resource
                          );

/***********************************************************************\
* Name   : debugRemoveResourceTrace
* Purpose: remove resource from debug trace list
* Input  : __fileName__ - file name
*          __lineNb__   - line number
*          resource     - resource
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugRemoveResourceTrace(const char *__fileName__,
                              ulong      __lineNb__,
                              const char *typeName,
                              const char *variableName,
                              const void *resource
                             );

/***********************************************************************\
* Name   : debugCheckResourceTrace
* Purpose: check if resource is in debug trace list
* Input  : __fileName__ - file name
*          __lineNb__   - line number
*          variableName - variable name
*          resource     - resource
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugCheckResourceTrace(const char *__fileName__,
                             ulong      __lineNb__,
                             const char *variableName,
                             const void *resource
                            );

/***********************************************************************\
* Name   : debugResourceDone
* Purpose: done resource debug trace list
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugResourceDone(void);

/***********************************************************************\
* Name   : debugResourceDumpInfo, debugResourcePrintInfo
* Purpose: resource debug function: output allocated resources
* Input  : handle                   - output channel
*          resourceDumpInfoFunction - resource dump info call-back or
*                                     NULL
*          resourceDumpInfoUserData - resource dump info user data
*          resourceDumpInfoTypes    - resource dump info types; see
*                                     DUMP_INFO_TYPE_*
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugResourceDumpInfo(FILE                     *handle,
                           ResourceDumpInfoFunction resourceDumpInfoFunction,
                           void                     *resourceDumpInfoUserData,
                           uint                     resourceDumpInfoTypes
                          );
void debugResourcePrintInfo(ResourceDumpInfoFunction resourceDumpInfoFunction,
                            void                     *resourceDumpInfoUserData,
                            uint                     resourceDumpInfoTypes
                           );

/***********************************************************************\
* Name   : debugResourcePrintStatistics
* Purpose: print resource statistics
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugResourcePrintStatistics(void);

/***********************************************************************\
* Name   : debugResourcePrintHistogram
* Purpose: print resource histogram
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugResourcePrintHistogram(void);

/***********************************************************************\
* Name   : debugResourceCheck
* Purpose: do resource debug trace check
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugResourceCheck(void);
#endif /* not NDEBUG */

#ifndef NDEBUG
/***********************************************************************\
* Name   : debugDumpStackTraceAddOutput
* Purpose: add stack trace output handler function
* Input  : type     - output type; see DebugDumpStackTraceOutputTypes
*          function - output handler function
*          userData - user data for output handler function
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugDumpStackTraceAddOutput(DebugDumpStackTraceOutputTypes    type,
                                  DebugDumpStackTraceOutputFunction function,
                                  void                              *userData
                                 );

/***********************************************************************\
* Name   : debugDumpStackTraceOutput
* Purpose: stack trace output function
* Input  : handle - output stream
*          indent - indention of output
*          type   - output type; see DebugDumpStackTraceOutputTypes
*          format - format string (like printf)
*          ...    - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugDumpStackTraceOutput(FILE                           *handle,
                               uint                           indent,
                               DebugDumpStackTraceOutputTypes type,
                               const char                     *format,
                               ...
                              );

/***********************************************************************\
* Name   : debugDumpStackTrace
* Purpose: print function names of stack trace
* Input  : handle         - output stream
*          indent         - indention of output
*          type           - output type; see
*                           DebugDumpStackTraceOutputTypes
*          stackTrace     - stack trace
*          stackTraceSize - size of stack trace
*          skipFrameCount - number of stack frames to skip
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugDumpStackTrace(FILE                           *handle,
                         uint                           indent,
                         DebugDumpStackTraceOutputTypes type,
                         void const * const             stackTrace[],
                         uint                           stackTraceSize,
                         uint                           skipFrameCount
                        );

/***********************************************************************\
* Name   : debugDumpCurrentStackTrace
* Purpose: print function names of stack trace of current thread
* Input  : handle         - output stream
*          indent         - indention of output
*          type           - output type; see
*                           DebugDumpStackTraceOutputTypes
*          skipFrameCount - number of stack frames to skip
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugDumpCurrentStackTrace(FILE                           *handle,
                                uint                           indent,
                                DebugDumpStackTraceOutputTypes type,
                                uint                           skipFrameCount
                               );

/***********************************************************************\
* Name   : debugPrintStackTrace
* Purpose: print stack trace
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugPrintStackTrace(void);

/***********************************************************************\
* Name   : debugDumpMemory
* Purpose: dump memory content (hex dump)
* Input  : address      - start address
*          length       - length
*          printAddress - TRUE to print address, FALSE otherwise
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugDumpMemory(const void *address, uint length, bool printAddress);
#endif /* NDEBUG */

/* profiling with valgrind:

#include <valgrind/callgrind.h>

...
CALLGRIND_START_INSTRUMENTATION;
CALLGRIND_TOGGLE_COLLECT;
<code to analyze>
CALLGRIND_TOGGLE_COLLECT;
CALLGRIND_STOP_INSTRUMENTATION;

valgrind --tool=callgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes --collect-jumps=yes --collect-atstart=no --instr-atstart=no <executable> ...

*/

#ifdef __cplusplus
}
#endif

#endif /* __GLOBAL__ */

/* end of file */
