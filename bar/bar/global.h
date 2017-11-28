/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: global definitions
* Systems: Linux
*
\***********************************************************************/

#ifndef __GLOBAL__
#define __GLOBAL__

#if (defined DEBUG)
 #warning DEBUG option set - no LOCAL and no -O2 (optimizer) will be used!
#endif

/****************************** Includes *******************************/
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
#ifdef HAVE_BACKTRACE
  #include <execinfo.h>
#endif
#include <assert.h>

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
#define NS_PER_SECOND (1000LL*NS_PER_MS)
#define NS_PER_MINUTE (1000LL*NS_PER_SECOND)
#define NS_PER_HOUR   (60LL*NS_PER_MINUTE)
#define NS_PER_DAY    (24LL*NS_PER_HOUR)

#define US_PER_MS     1000LL
#define US_PER_SECOND (1000LL*US_PER_MS)
#define US_PER_MINUTE (60LL*US_PER_SECOND)
#define US_PER_HOUR   (60LL*US_PER_MINUTE)
#define US_PER_DAY    (24LL*US_PER_HOUR)

#define MS_PER_SECOND 1000LL
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
#define MIN_INT64          9223372036854775808LL
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
#define MB (1024*1024)
#define GB (1024L*1024L*1024L)

// special constants
#define NO_WAIT      0L
#define WAIT_FOREVER -1L

// exit codes
#define EXITCODE_INTERNAL_ERROR 128

/**************************** Datatypes ********************************/
#ifndef HAVE_STDBOOL_H
  #ifndef __cplusplus
    typedef uint8_t bool;
  #endif
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

typedef unsigned char       bool8;
typedef unsigned int        bool32;
typedef char                char8;
typedef unsigned char       uchar8;
typedef char                int8;
typedef short int           int16;
typedef int                 int32;
typedef long long int       int64;
typedef unsigned char       uint8;
typedef unsigned short int  uint16;
typedef unsigned int        uint32;
typedef unsigned long long  uint64;
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

/**************************** Variables ********************************/

#ifndef NDEBUG
  extern pthread_mutex_t debugConsoleLock;    // lock console
  extern const char      *__testCodeName__;   // name of testcode to execute
#endif /* not NDEBUG */

/****************************** Macros *********************************/
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
#else
  #define ATTRIBUTE_PACKED
  #define ATTRIBUTE_WARN_UNUSED_RESULT
  #define ATTRIBUTE_NO_INSTRUMENT_FUNCTION
  #define ATTRIBUTE_AUTO(functionCode)
#endif /* __GNUC__ */

// only for better reading
#define CALLBACK(code,argument) code,argument
#define CALLBACK_NULL NULL,NULL

// mask and shift value
#define MASKSHIFT(n,maskShift) (((n) & maskShift.mask) >> maskShift.shift)

// debugging
#if defined(__x86_64__) || defined(__i386)
  #define __BP() do { asm(" int3"); } while (0)
#else
  #define __BP() do { } while (0)
#endif

/***********************************************************************\
* Name   : LAMBDA
* Purpose: define a lambda-function (anonymouse function)
* Input  : functionReturnType - call-back function return type
*          functionSignature  - call-back function signature
*          functionBody       - call-back function body
* Output : -
* Return : -
* Notes  : example
*          List_removeAndFree(list,
*                             node,
*                             LAMBDA(void,(...),{ ... })
*                            );
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
*          List_removeAndFree(list,
*                             node,
*                             CALLBACK_INLINE(void,(...),{ ... },NULL)
*                            );
\***********************************************************************/

#define CALLBACK_INLINE(functionReturnType,functionSignature,functionBody,functionUserData) \
  ({ \
    auto functionReturnType __closure__ functionSignature; \
    functionReturnType __closure__ functionSignature functionBody \
    __closure__; \
  }), \
  functionUserData

/***********************************************************************\
* Name   : printf/fprintf
* Purpose: printf/fprintf-work-around for Windows
* Input  : -
* Output : -
* Return : -
* Notes  : Windows does not support %ll format token, instead it tries
*          - as usual according to the MS principle: ignore any standard
*          whenever possible - its own way (and of course fail...).
*          Thus use the MinGW implementation of printf/fprintf.
\***********************************************************************/

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
  /* Work-around for Windows:
  */
  #ifndef printf
    #define printf __mingw_printf
  #endif
  #ifndef fprintf
    #define fprintf __mingw_fprintf
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
* Purpose: iterated over array and execute block
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
* Name   : ALIGN
* Purpose: align value to boundary
* Input  : n         - address
*          alignment - alignment
* Output : -
* Return : -
* Notes  : -
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

/***********************************************************************\
* Name   : BITSET_SET, BITSET_CLEAR, BITSET_IS_SET, SET_REM, IN_SET
* Purpose: set macros
* Input  : set     - set (array)
*          element - element
* Output : -
* Return : TRUE if value is in bitset, FALSE otherwise
* Notes  : -
\***********************************************************************/

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
* Name   : MIN, MAX, IN_RANGE
* Purpose: get min./max.
* Input  : x,y - numbers
* Output : -
* Return : min./max. of x,y
* Notes  : -
\***********************************************************************/

#ifdef __GNUG__
  #define MIN(x,y) ((x)<?(y))
  #define MAX(x,y) ((x)>?(y))
#else
  #define MIN(x,y) (((x)<(y)) ? (x) : (y))
  #define MAX(x,y) (((x)>(y)) ? (x) : (y))
#endif

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
#define HALT(errorLevel, format, args...) \
  do \
  { \
    __halt(errorLevel,format, ## args); \
  } \
  while (0)

#define HALT_INSUFFICIENT_MEMORY() \
  do \
  { \
     __abort(HALT_PREFIX_FATAL_ERROR,"Insufficient memory"); \
  } \
 while (0)

#define HALT_FATAL_ERROR(format, args...) \
  do \
  { \
     __abort(HALT_PREFIX_FATAL_ERROR,format, ## args); \
  } \
 while (0)

#define HALT_INTERNAL_ERROR(format, args...) \
  do \
  { \
     __abort(HALT_PREFIX_INTERNAL_ERROR, format, ## args); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE() \
  do \
  { \
     __abort(HALT_PREFIX_INTERNAL_ERROR, "Unhandled switch case"); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASEX(format, args...) \
  do \
  { \
     __abort(HALT_PREFIX_INTERNAL_ERROR, "Unhandled switch case " format, ## args); \
  } \
  while (0)
#else /* not NDEBUG */
#define HALT(errorLevel, format, args...) \
  do \
  { \
    __halt(__FILE__,__LINE__,errorLevel,format, ## args); \
  } \
  while (0)

#define HALT_INSUFFICIENT_MEMORY(args...) \
  do \
  { \
     __abort(__FILE__,__LINE__,HALT_PREFIX_FATAL_ERROR,"Insufficient memory", ## args); \
  } \
 while (0)

#define HALT_FATAL_ERROR(format, args...) \
  do \
  { \
     __abort(__FILE__,__LINE__,HALT_PREFIX_FATAL_ERROR,format, ## args); \
  } \
 while (0)

#define HALT_INTERNAL_ERROR(format, args...) \
  do \
  { \
     __abort(__FILE__,__LINE__,HALT_PREFIX_INTERNAL_ERROR, format, ## args); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE() \
  do \
  { \
     __abort(__FILE__,__LINE__,HALT_PREFIX_INTERNAL_ERROR, "Unhandled switch case"); \
  } \
  while (0)
#define HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASEX(format, args...) \
  do \
  { \
     __abort(__FILE__,__LINE__,HALT_PREFIX_INTERNAL_ERROR, "Unhandled switch case " format, ## args); \
  } \
  while (0)
#endif /* NDEBUG */
#define HALT_INTERNAL_ERROR_AT(file, line, format, args...) \
  do \
  { \
     __abortAt(file,line,HALT_PREFIX_INTERNAL_ERROR,format, ## args); \
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
#define FAIL(errorLevel, format, args...) \
  do \
  { \
   fprintf(stderr, format " - fail in file " __FILE__ ", line " __FAIL_STRING1(__LINE__) "\n" , ## args); \
   exit(errorLevel);\
  } \
  while (0)

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
*          DEBUG_IS_RESOURCE_TRACE
*          DEBUG_CHECK_RESOURCE_TRACE
* Purpose: add/remove debug trace allocated resource functions,
*          check if resource allocated
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

  #define DEBUG_ADD_RESOURCE_TRACE(resource,size) \
    do \
    { \
      debugAddResourceTrace(__FILE__,__LINE__,__DEBUG_RESOURCE_TRACE__STRING1(resource),resource,size); \
    } \
    while (0)

  #define DEBUG_REMOVE_RESOURCE_TRACE(resource,size) \
    do \
    { \
      debugRemoveResourceTrace(__FILE__,__LINE__,resource,size); \
    } \
    while (0)

  #define DEBUG_ADD_RESOURCE_TRACEX(fileName,lineNb,resource,size) \
    do \
    { \
      debugAddResourceTrace(fileName,lineNb,__DEBUG_RESOURCE_TRACE__STRING1(resource),resource,size); \
    } \
    while (0)

  #define DEBUG_REMOVE_RESOURCE_TRACEX(fileName,lineNb,resource,size) \
    do \
    { \
      debugRemoveResourceTrace(fileName,lineNb,resource,size); \
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
* Name   : atomicIncrement
* Purpose: atomic increment value
* Input  : n - value
*          d - delta
* Output : -
* Return : new value
* Notes  : -
\***********************************************************************/

static inline uint atomicIncrement(uint *n, int d)
{
  assert(n != NULL);

  return __sync_add_and_fetch(n,d);
}

/***********************************************************************\
* Name   : swapWORD
* Purpose: swap low/high byte of word (2 bytes)
* Input  : n - word (a:b)
* Output : -
* Return : swapped word (b:a)
* Notes  : -
\***********************************************************************/

static inline ushort swapWORD(ushort n)
{
  return   ((n & 0xFF00) >> 8)
         | ((n & 0x00FF) << 8);
}

/***********************************************************************\
* Name   : swapLONG
* Purpose: swap bytes of long (4 bytes)
* Input  : n - long (a:b:c:d)
* Output : -
* Return : swapped long (d:c:b:a)
* Notes  : -
\***********************************************************************/

static inline ulong swapLONG(ulong n)
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

void *allocSecure(size_t size);

/***********************************************************************\
* Name   : freeSecure
* Purpose: free secure memory
* Input  : p - secure memory
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void freeSecure(void *p);

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
* Name   : stringClear
* Purpose: clear string
* Input  : s - string
* Output : -
* Return : string
* Notes  : string is always NUL-terminated
\***********************************************************************/

static inline char *stringClear(char *s)
{
  if (s != NULL)
  {
    (*s) = '\0';
  }

  return s;
}

/***********************************************************************\
* Name   : stringEquals
* Purpose: compare strings for equal
* Input  : s1, s2 - strings
* Output : -
* Return : TRUE iff equals
* Notes  : -
\***********************************************************************/

static inline bool stringEquals(const char *s1, const char *s2)
{
  return strcmp(s1,s2) == 0;
}

/***********************************************************************\
* Name   : stringEqualsIgnoreCase
* Purpose: compare strings for equal and ignore case
* Input  : s1, s2 - strings
* Output : -
* Return : TRUE iff equals
* Notes  : -
\***********************************************************************/

static inline bool stringEqualsIgnoreCase(const char *s1, const char *s2)
{
  return strcasecmp(s1,s2) == 0;
}

/***********************************************************************\
* Name   : stringStartsWith
* Purpose: check if string starts with prefix
* Input  : s      - string
*          prefix - prefix
* Output : -
* Return : TRUE iff s start with prefix
* Notes  : -
\***********************************************************************/

static inline bool stringStartsWith(const char *s, const char *prefix)
{
  return strncmp(s,prefix,strlen(prefix)) == 0;
}

/***********************************************************************\
* Name   : stringStartsWithIgnoreCase
* Purpose: check if string starts with prefix
* Input  : s      - string
*          prefix - prefix
* Output : -
* Return : TRUE iff s start with prefix
* Notes  : -
\***********************************************************************/

static inline bool stringStartsWithIgnoreCase(const char *s, const char *prefix)
{
  return strncasecmp(s,prefix,strlen(prefix)) == 0;
}

/***********************************************************************\
* Name   : stringIsEmpty
* Purpose: check if string is NULL or empty
* Input  : s - string
* Output : -
* Return : TRUE iff empty
* Notes  : -
\***********************************************************************/

static inline bool stringIsEmpty(const char *s)
{
  return (s == NULL) || (s[0] == '\0');
}

/***********************************************************************\
* Name   : stringSet
* Purpose: set string
* Input  : destination - destination string
*          source      - source string
*          n           - size of destination string
* Output : -
* Return : destination string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringSet(char *destination, const char *source, size_t n)
{
  assert(n > 0);

  if (destination != NULL)
  {
    if (source != NULL)
    {
      strncpy(destination,source,n-1); destination[n-1] = '\0';
    }
    else
    {
      destination[0] = '\0';
    }
  }

  return destination;
}

/***********************************************************************\
* Name   : stringAppend
* Purpose: append string
* Input  : destination - destination string
*          source      - source string
*          n           - size of destination string
* Output : -
* Return : destination string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringAppend(char *destination, const char *source, size_t n)
{
  size_t m;

  assert(n > 0);

  if (destination != NULL)
  {
    m = strlen(destination);
    if ((source != NULL) && (n > (m+1)))
    {
      strncat(destination,source,n-(m+1));
    }
  }

  return destination;
}

/***********************************************************************\
* Name   : stringLength
* Purpose: get string length
* Input  : s - string
* Output : -
* Return : string length or 0
* Notes  : -
\***********************************************************************/

static inline size_t stringLength(const char *s)
{
  return (s != NULL) ? strlen(s) : 0;
}

/***********************************************************************\
* Name   : stringFind
* Purpose: find string/character in string
* Input  : s                   - string
*          findString,findChar - string/character to find
* Output : -
* Return : index or -1
* Notes  : -
\***********************************************************************/

static inline long stringFind(const char *s, const char *findString)
{
  const char *t;

  t = strstr(s,findString);
  return (t != NULL) ? (long)(t-s) : -1L;
}

static inline long stringFindChar(const char *s, char findChar)
{
  const char *t;

  t = strchr(s,findChar);
  return (t != NULL) ? (long)(t-s) : -1L;
}

/***********************************************************************\
* Name   : stringSub
* Purpose: get sub-string
* Input  : destination - destination string
*          n           - size of destination string
*          source      - source string
*          index       - sub-string start index
*          length      - sub-string length or -1
* Output : -
* Return : destination string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringSub(char *destination, size_t n, const char *source, size_t index, ssize_t length)
{
  ssize_t m;

  assert(n > 0);

  if (destination != NULL)
  {
    if (source != NULL)
    {
      m = (length >= 0) ? MIN((ssize_t)n-1,length) : MIN((ssize_t)n-1,(ssize_t)strlen(source)-(ssize_t)index);
      if (m < 0) m = 0;
      strncpy(destination,source+index,m); destination[m] = '\0';
    }
  }

  return destination;
}

/***********************************************************************\
* Name   : stringTrimBegin
* Purpose: trim spaces at beginning of string
* Input  : string - string
* Output : -
* Return : trimmed string
* Notes  : -
\***********************************************************************/

static inline const char* stringTrimBegin(const char *string)
{
  while (isspace(*string))
  {
    string++;
  }

  return string;
}

/***********************************************************************\
* Name   : stringTrimEnd
* Purpose: trim spaces at end of string
* Input  : string - string
* Output : -
* Return : trimmed string
* Notes  : -
\***********************************************************************/

static inline char* stringTrimEnd(char *string)
{
  char *s;

  s = string+strlen(string)-1;
  while ((s >= string) && isspace(*s))
  {
    s--;
  }
  if (s >= string) s[0] = '\0';

  return string;
}

/***********************************************************************\
* Name   : stringTrim
* Purpose: trim spaces at beginning and end of string
* Input  : string - string
* Output : -
* Return : trimmed string
* Notes  : -
\***********************************************************************/

static inline char* stringTrim(char *string)
{
  char *s;

  while (isspace(*string))
  {
    string++;
  }

  s = string+strlen(string)-1;
  while ((s >= string) && isspace(*s))
  {
    s--;
  }
  if (s >= string) s[0] = '\0';

  return string;
}

/***********************************************************************\
* Name   : stringFormat
* Purpose: format string
* Input  : string - string
*          n      - size of string
*          format - format string
*          ...    - optional arguments
* Output : -
* Return : destination string
* Notes  : string is always NULL or NUL-terminated
\***********************************************************************/

static inline char* stringFormat(char *string, size_t n, const char *format, ...)
{
  va_list arguments;

  assert(string != NULL);
  assert(n > 0);
  assert(format != NULL);

  va_start(arguments,format);
  vsnprintf(string,n,format,arguments);
  va_end(arguments);

  return string;
}

/***********************************************************************\
* Name   : stringToInt
* Purpose: convert string to int-value
* Input  : string - string
*          i      - value variable
* Output : i - value
* Return : TRUE iff no error
* Notes  :
\***********************************************************************/

static inline bool stringToInt(const char *string, int *i)
{
  long long int n;
  char          *s;

  assert(string != NULL);
  assert(i != NULL);

  n = strtoll(string,&s,0);
  if ((*s) == '\0')
  {
    (*i) = (int)n;
    return TRUE;
  }
  else
  {
    (*i) = 0;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringToInt
* Purpose: convert string to uint-value
* Input  : string - string
*          i      - value variable
* Output : i - value
* Return : TRUE iff no error
* Notes  :
\***********************************************************************/

static inline bool stringToUInt(const char *string, uint *i)
{
  long long int n;
  char          *s;

  assert(string != NULL);
  assert(i != NULL);

  n = strtoll(string,&s,0);
  if ((*s) == '\0')
  {
    (*i) = (uint)n;
    return TRUE;
  }
  else
  {
    (*i) = 0;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringToInt
* Purpose: convert string to int64-value
* Input  : string - string
*          l      - value variable
* Output : l - value
* Return : TRUE iff no error
* Notes  :
\***********************************************************************/

static inline bool stringToInt64(const char *string, int64 *l)
{
  long long int n;
  char          *s;

  assert(string != NULL);
  assert(l != NULL);

  n = strtoll(string,&s,0);
  if ((*s) == '\0')
  {
    (*l) = (int64)n;
    return TRUE;
  }
  else
  {
    (*l) = 0LL;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringToInt
* Purpose: convert string to uint64-value
* Input  : string - string
*          l      - value variable
* Output : l - value
* Return : TRUE iff no error
* Notes  :
\***********************************************************************/

static inline bool stringToUInt64(const char *string, uint64 *l)
{
  long long int n;
  char          *s;

  assert(string != NULL);
  assert(l != NULL);

  n = strtoll(string,&s,0);
  if ((*s) == '\0')
  {
    (*l) = (uint64)n;
    return TRUE;
  }
  else
  {
    (*l) = 0LL;
    return FALSE;
  }
}

/***********************************************************************\
* Name   : stringToDouble
* Purpose: convert string to double value
* Input  : string - string
*          d      - value variable
* Output : d - value
* Return : TRUE iff no error
* Notes  :
\***********************************************************************/

static inline bool stringToDouble(const char *string, double *d)
{
  double n;
  char   *s;

  assert(string != NULL);
  assert(d != NULL);

  n = strtod(string,&s);
  if ((*s) == '\0')
  {
    (*d) = n;
    return TRUE;
  }
  else
  {
    (*d) = 0.0;
    return FALSE;
  }
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
* Purpose: mark resource as local resource (must be freed before
*          function exit)
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
                           const char *variableName,
                           const void *resource,
                           uint       size
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
                              const void *resource,
                              uint       size
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
* Name   : debugResourceDumpInfo
* Purpose: dump resource debug trace list to file
* Input  : handle - file handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugResourceDumpInfo(FILE *handle);

/***********************************************************************\
* Name   : debugResourcePrintInfo
* Purpose: print resource debug trace list
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugResourcePrintInfo(void);

/***********************************************************************\
* Name   : debugResourcePrintStatistics
* Purpose: done resource debug trace statistics
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugResourcePrintStatistics(void);

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

/***********************************************************************\
* Name   : debugDumpStackTrace
* Purpose: print function names of stack trace
* Input  : handle         - output stream
*          indent         - indention of output
*          stackTrace     - stack trace
*          stackTraceSize - size of stack trace
*          skipFrameCount - number of stack frames to skip
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugDumpStackTrace(FILE       *handle,
                         uint       indent,
                         void const *stackTrace[],
                         uint       stackTraceSize,
                         uint       skipFrameCount
                        );

/***********************************************************************\
* Name   : debugDumpStackTrace, debugDumpCurrentStackTrace
* Purpose: print function names of stack trace of current thread
* Input  : handle         - output stream
*          indent         - indention of output
*          skipFrameCount - number of stack frames to skip
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void debugDumpCurrentStackTrace(FILE *handle,
                                uint indent,
                                uint skipFrameCount
                               );

#ifndef NDEBUG
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

#ifdef __cplusplus
}
#endif

#endif /* __GLOBAL__ */

/* end of file */
