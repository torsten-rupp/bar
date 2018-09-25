/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: dynamic string functions
* Systems: all
*
\***********************************************************************/

#define __STRINGS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(HAVE_PCRE)
  #include <pcreposix.h>
#elif defined(HAVE_REGEX_H)
  #include <regex.h>
#else
  #warning No regular expression library available!
#endif /* HAVE_PCRE || HAVE_REGEX_H */
#include <errno.h>
#ifdef HAVE_BACKTRACE
  #include <execinfo.h>
#endif
#include <assert.h>

#include "common/global.h"
#ifndef NDEBUG
  #include <pthread.h>
  #include "lists.h"
#endif /* not NDEBUG */

#include "strings.h"

/****************** Conditional compilation switches *******************/
#define HALT_ON_INSUFFICIENT_MEMORY   // halt on insufficient memory
#define _TRACE_STRING_ALLOCATIONS      // trace all allocated strings
#define _FILL_MEMORY                   // fill memory

#ifndef NDEBUG
  #define DEBUG_LIST_HASH_SIZE   65537
  // max. string check: print warning if to many strings allocated/strings become to long
  #ifdef TRACE_STRING_ALLOCATIONS
    #define MAX_STRINGS_CHECK
    #define WARN_MAX_STRINGS       2000
    #define WARN_MAX_STRINGS_DELTA 500
    #define WARN_MAX_STRING_LENGTH (1024*1024)
  #endif /* TRACE_STRING_ALLOCATIONS */
#endif /* not NDEBUG */

/***************************** Constants *******************************/
const char STRING_ESCAPE_CHARACTERS_MAP_FROM[STRING_ESCAPE_CHARACTER_MAP_LENGTH] =
{'\0','\007','\b','\t','\n','\v','\f','\r','\033'};
const char STRING_ESCAPE_CHARACTERS_MAP_TO[STRING_ESCAPE_CHARACTER_MAP_LENGTH] =
{'0', 'a',   'b', 't', 'n', 'v', 'f', 'r', 'e'   };

struct __String __STRING_EMPTY =
{
  0,
  0,
  STRING_TYPE_CONST,
  "",
  #ifndef NDEBUG
    STRING_CHECKSUM(0,0,"")
  #endif /* not NDEBUG */
};
const struct __String* STRING_EMPTY = &__STRING_EMPTY;

#define STRING_START_LENGTH 64   // string start length
#define STRING_DELTA_LENGTH 32   // string delta increasing/decreasing

LOCAL const char *DEFAULT_TRUE_STRINGS[] =
{
  "1",
  "true",
  "yes",
  "on",
};
LOCAL const char *DEFAULT_FALSE_STRINGS[] =
{
  "0",
  "false",
  "no",
  "off",
};

#define DEBUG_FILL_BYTE     0xFE
#define DEBUG_MAX_FREE_LIST 4000

/***************************** Datatypes *******************************/
typedef enum
{
  FORMAT_LENGTH_TYPE_INTEGER,
  FORMAT_LENGTH_TYPE_LONG,
  FORMAT_LENGTH_TYPE_LONGLONG,
  FORMAT_LENGTH_TYPE_DOUBLE,
  FORMAT_LENGTH_TYPE_QUAD,
  FORMAT_LENGTH_TYPE_POINTER,
} FormatLengthTypes;

typedef struct
{
  char              token[16];
  unsigned int      length;
  bool              alternateFlag;
  bool              zeroPaddingFlag;
  bool              leftAdjustedFlag;
  bool              blankFlag;
  bool              signFlag;
  unsigned int      width;
  unsigned int      precision;
  FormatLengthTypes lengthType;
  char              quoteChar;
  char              conversionChar;
} FormatToken;

#ifndef NDEBUG
  typedef struct DebugStringNode
  {
    LIST_NODE_HEADER(struct DebugStringNode);

    const char      *allocFileName;
    ulong           allocLineNb;
    #ifdef HAVE_BACKTRACE
      const void *stackTrace[16];
      int        stackTraceSize;
    #endif /* HAVE_BACKTRACE */

    const char      *deleteFileName;
    ulong           deleteLineNb;
    #ifdef HAVE_BACKTRACE
      const void *deleteStackTrace[16];
      int        deleteStackTraceSize;
    #endif /* HAVE_BACKTRACE */

    struct __String *string;
  } DebugStringNode;

  typedef struct
  {
    LIST_HEADER(DebugStringNode);
    ulong memorySize;

    struct
    {
      DebugStringNode *first;
      ulong           count;
    } hash[DEBUG_LIST_HASH_SIZE];
  } DebugStringList;
#endif /* not NDEBUG */

/***************************** Variables *******************************/
#ifndef NDEBUG
  LOCAL pthread_once_t      debugStringInitFlag = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutexattr_t debugStringLockAttributes;
  LOCAL pthread_mutex_t     debugStringLock;
  #ifdef TRACE_STRING_ALLOCATIONS
    LOCAL DebugStringList     debugStringAllocList;
    LOCAL DebugStringList     debugStringFreeList;
  #endif /* TRACE_STRING_ALLOCATIONS */
  #ifdef MAX_STRINGS_CHECK
    LOCAL ulong debugMaxStringNextWarningCount;
  #endif /* MAX_STRINGS_CHECK */
#endif /* not NDEBUG */

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define STRING_IS_ASSIGNABLE(string) \
    (((string)->type == STRING_TYPE_DYNAMIC) || ((string)->type == STRING_TYPE_STATIC))
  #define STRING_IS_DYNAMIC(string) \
    ((string)->type == STRING_TYPE_DYNAMIC)

  #define STRING_CHECK_ASSIGNABLE(string) \
    do \
    { \
      if (string != NULL) \
      { \
        assert(STRING_IS_ASSIGNABLE(string)); \
      } \
    } \
    while (0)
  #define STRING_CHECK_DYNAMIC(string) \
    do \
    { \
      if (string != NULL) \
      { \
        assert(STRING_IS_DYNAMIC(string)); \
      } \
    } \
    while (0)

  #ifdef MAX_STRINGS_CHECK
    #define STRING_UPDATE_VALID(string) \
      do \
      { \
        if (string != NULL) \
        { \
          (string)->checkSum = STRING_CHECKSUM((string)->length,(string)->maxLength,(string)->data); \
          if ((string)->length > WARN_MAX_STRING_LENGTH) \
          { \
            fprintf(stderr,"DEBUG WARNING: extremly long string %p: %lu!\n",string,(string)->length); \
            usleep((((string)->length-WARN_MAX_STRING_LENGTH)/100L)*10*1000); \
          } \
        } \
      } \
      while (0)
  #else /* not MAX_STRINGS_CHECK */
    #define STRING_UPDATE_VALID(string) \
      do \
      { \
        if (string != NULL) \
        { \
          (string)->checkSum = STRING_CHECKSUM((string)->length,(string)->maxLength,(string)->data); \
        } \
      } \
      while (0)
  #endif /* MAX_STRINGS_CHECK */
#else /* NDEBUG */
  #define STRING_CHECK_ASSIGNABLE(string) \
    do \
    { \
    } \
    while (0)

  #define STRING_CHECK_DYNAMIC(string) \
    do \
    { \
    } \
    while (0)

  #define STRING_UPDATE_VALID(string) \
    do \
    { \
    } \
    while (0)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef NDEBUG

/***********************************************************************\
* Name   : debugStringInit
* Purpose: initialize debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugStringInit(void)
{
  pthread_mutexattr_init(&debugStringLockAttributes);
  pthread_mutexattr_settype(&debugStringLockAttributes,PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&debugStringLock,&debugStringLockAttributes);
  #ifdef TRACE_STRING_ALLOCATIONS
    List_init(&debugStringAllocList);
    debugStringAllocList.memorySize = 0L;
    memset(debugStringAllocList.hash,0,sizeof(debugStringAllocList.hash));
    List_init(&debugStringFreeList);
    debugStringFreeList.memorySize = 0L;
    memset(debugStringFreeList.hash,0,sizeof(debugStringFreeList.hash));
  #endif /* TRACE_STRING_ALLOCATIONS */
  #ifdef MAX_STRINGS_CHECK
    debugMaxStringNextWarningCount = WARN_MAX_STRINGS;
  #endif /* MAX_STRINGS_CHECK */
}

#ifdef TRACE_STRING_ALLOCATIONS

/***********************************************************************\
* Name   : debugStringHashIndex
* Purpose: get string hash index
* Input  : string - string
* Output : -
* Return : hash index
* Notes  : -
\***********************************************************************/

LOCAL_INLINE uint debugStringHashIndex(ConstString string)
{
  assert(string != NULL);

  return ((uintptr_t)string >> 2) % DEBUG_LIST_HASH_SIZE;
}

/***********************************************************************\
* Name   : debugFindString
* Purpose: find string in list
* Input  : debugStringList - string list
*          string          - string
* Output : -
* Return : string node or NULL if not found
* Notes  : -
\***********************************************************************/

LOCAL DebugStringNode *debugFindString(const DebugStringList *debugStringList, ConstString string)
{
  uint            index;
  DebugStringNode *debugStringNode;
  ulong           n;

  assert(debugStringList != NULL);
  assert(string != NULL);

  index = debugStringHashIndex(string);

  debugStringNode = debugStringList->hash[index].first;
  n               = debugStringList->hash[index].count;
  while ((debugStringNode != NULL) && (n > 0) && (debugStringNode->string != string))
  {
    debugStringNode = debugStringNode->next;
    n--;
  }

  return (n > 0) ? debugStringNode : NULL;
}

/***********************************************************************\
* Name   : debugAddString
* Purpose: add string node to list
* Input  : debugStringList - string list
*          debugStringNode - string node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugAddString(DebugStringList *debugStringList, DebugStringNode *debugStringNode)
{
  uint index;

  assert(debugStringList != NULL);
  assert(debugStringNode != NULL);

  index = debugStringHashIndex(debugStringNode->string);

  if (debugStringList->hash[index].first != NULL)
  {
    List_insert(debugStringList,debugStringNode,debugStringList->hash[index].first);
  }
  else
  {
    List_append(debugStringList,debugStringNode);
  }
  debugStringList->hash[index].first = debugStringNode;
  debugStringList->hash[index].count++;
}

/***********************************************************************\
* Name   : debugRemoveString
* Purpose: remove string node from list
* Input  : debugStringList - string list
*          debugStringNode - string node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void debugRemoveString(DebugStringList *debugStringList, DebugStringNode *debugStringNode)
{
  uint index;

  assert(debugStringList != NULL);
  assert(debugStringNode != NULL);

  index = debugStringHashIndex(debugStringNode->string);
  assert(debugStringList->hash[index].count > 0);

  List_remove(debugStringList,debugStringNode);
  debugStringList->hash[index].count--;
  if      (debugStringList->hash[index].count == 0)
  {
    debugStringList->hash[index].first = NULL;
  }
  else if (debugStringList->hash[index].first == debugStringNode)
  {
    debugStringList->hash[index].first = debugStringNode->next;
  }
}

#endif /* TRACE_STRING_ALLOCATIONS */

#endif /* not NDEBUG */

/***********************************************************************\
* Name   : printErrorConstString
* Purpose: print error for modified constant string
* Input  : string - string
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printErrorConstString(const struct __String *string)
{
  #ifndef NDEBUG
    #ifdef TRACE_STRING_ALLOCATIONS
      DebugStringNode *debugStringNode;
    #endif /* TRACE_STRING_ALLOCATIONS */

    pthread_once(&debugStringInitFlag,debugStringInit);

    pthread_mutex_lock(&debugStringLock);
    {
      #ifdef TRACE_STRING_ALLOCATIONS
        debugStringNode = debugFindString(&debugStringAllocList,string);
        if (debugStringNode != NULL)
        {
          fprintf(stderr,
                  "FATAL ERROR: cannot modify constant string '%s' which was allocated at %s, %lu!\n",
                  string->data,
                  debugStringNode->allocFileName,
                  debugStringNode->allocLineNb
                 );
        }
        else
        {
          fprintf(stderr,"DEBUG WARNING: string '%s' not found in debug list\n",string->data);
        }
      #else /* TRACE_STRING_ALLOCATIONS */
        fprintf(stderr,"FATAL ERROR: cannot modify constant string '%s'\n",string->data);
      #endif /* TRACE_STRING_ALLOCATIONS */
      #ifdef HAVE_BACKTRACE
        debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
      #endif /* HAVE_BACKTRACE */
    }
    pthread_mutex_unlock(&debugStringLock);
  #else /* NDEBUG */
    fprintf(stderr,"FATAL ERROR: cannot modify constant string '%s'\n",string->data);
  #endif /* not NDEBUG */
  HALT_INTERNAL_ERROR("modify const string");
}

/***********************************************************************\
* Name   : allocString
* Purpose: allocate a new string
* Input  : -
* Output : -
* Return : allocated string or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

LOCAL_INLINE struct __String* allocString(void)
{
  struct __String *string;

  string = (struct __String*)malloc(sizeof(struct __String));
  if (string == NULL)
  {
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return NULL;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }
  string->data = (char*)malloc(STRING_START_LENGTH);
  if (string->data == NULL)
  {
    free(string);
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return NULL;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }

  string->length    = 0L;
  string->maxLength = STRING_START_LENGTH;
  string->type      = STRING_TYPE_DYNAMIC;
  string->data[0]   = NUL;
  #ifndef NDEBUG
    #ifdef FILL_MEMORY
      memset(&string->data[1],DEBUG_FILL_BYTE,STRING_START_LENGTH-1);
    #endif /* FILL_MEMORY */
  #endif /* not NDEBUG */

  STRING_UPDATE_VALID(string);

  return string;
}

/***********************************************************************\
* Name   : allocTmpString
* Purpose: allocate a temporary string
* Input  : -
* Output : -
* Return : allocated string or NULL on insufficient memory
* Notes  : temporary strings are not included in list of allocated
*          strings
\***********************************************************************/

#ifdef NDEBUG
LOCAL_INLINE struct __String* allocTmpString(void)
#else /* not NDEBUG */
LOCAL_INLINE struct __String* allocTmpString(const char *__fileName__, ulong __lineNb__)
#endif /* NDEBUG */
{
  String tmpString;
  #ifndef NDEBUG
    #ifdef TRACE_STRING_ALLOCATIONS
      DebugStringNode *debugStringNode;
    #endif /* TRACE_STRING_ALLOCATIONS */
  #endif /* not NDEBUG */

  tmpString = allocString();

  #ifndef NDEBUG
    #ifdef TRACE_STRING_ALLOCATIONS
      pthread_once(&debugStringInitFlag,debugStringInit);

      pthread_mutex_lock(&debugStringLock);
      {
        // update allocation info
        debugStringAllocList.memorySize += sizeof(struct __String)+tmpString->maxLength;

        // allocate new debug node
        debugStringNode = (DebugStringNode*)__List_newNode(__fileName__,__lineNb__,sizeof(DebugStringNode));
        if (debugStringNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
        debugStringAllocList.memorySize += sizeof(DebugStringNode);

        // init string node
        debugStringNode->allocFileName  = __fileName__;
        debugStringNode->allocLineNb    = __lineNb__;
        #ifdef HAVE_BACKTRACE
          debugStringNode->stackTraceSize = backtrace((void*)debugStringNode->stackTrace,SIZE_OF_ARRAY(debugStringNode->stackTrace));
        #endif /* HAVE_BACKTRACE */
        debugStringNode->deleteFileName = NULL;
        debugStringNode->deleteLineNb   = 0L;
        #ifdef HAVE_BACKTRACE
          debugStringNode->deleteStackTraceSize = 0;
        #endif /* HAVE_BACKTRACE */
        debugStringNode->string         = tmpString;

        // add string to allocated-list
        debugAddString(&debugStringAllocList,debugStringNode);
      }
      pthread_mutex_unlock(&debugStringLock);
    #else /* not TRACE_STRING_ALLOCATIONS */
      UNUSED_VARIABLE(__fileName__);
      UNUSED_VARIABLE(__lineNb__);
    #endif /* TRACE_STRING_ALLOCATIONS */
  #endif /* not NDEBUG */

  return tmpString;
}

/***********************************************************************\
* Name   : assignTmpString
* Purpose: assign data of string to string
* Input  : string    - string
*          tmpString - tmp strong
* Output : -
* Return : -
* Notes  : tmpString will be freed and become invalid after operation!
\***********************************************************************/

LOCAL_INLINE void assignTmpString(struct __String *string, struct __String *tmpString)
{
  #ifndef NDEBUG
    #ifdef TRACE_STRING_ALLOCATIONS
      DebugStringNode *debugStringNode;
    #endif /* TRACE_STRING_ALLOCATIONS */
  #endif /* not NDEBUG */

  assert(string != NULL);
  assert(string->data != NULL);
  assert((string->type == STRING_TYPE_DYNAMIC) || (string->type == STRING_TYPE_STATIC));
  assert(tmpString != NULL);
  assert(tmpString->data != NULL);

  // assign temporary string
  free(string->data);
  string->length    = tmpString->length;
  string->maxLength = tmpString->maxLength;
  string->data      = tmpString->data;
  #ifndef NDEBUG
    tmpString->length    = 0L;
    tmpString->maxLength = 0L;
    tmpString->data      = NULL;
  #endif /* not NDEBUG */

  #ifndef NDEBUG
    #ifdef TRACE_STRING_ALLOCATIONS
      pthread_once(&debugStringInitFlag,debugStringInit);

      pthread_mutex_lock(&debugStringLock);
      {
        // remove string from allocated list
        debugStringNode = debugFindString(&debugStringAllocList,tmpString);
        if (debugStringNode == NULL)
        {
          HALT_INTERNAL_ERROR("Temporary string not found in allocated string list!");
        }
        debugRemoveString(&debugStringAllocList,debugStringNode);
        assert(debugStringAllocList.memorySize >= sizeof(DebugStringNode)+sizeof(struct __String)+string->maxLength);
        debugStringAllocList.memorySize -= sizeof(DebugStringNode)+sizeof(struct __String)+string->maxLength;
        LIST_DELETE_NODE(debugStringNode);
      }
      pthread_mutex_unlock(&debugStringLock);
    #endif /* TRACE_STRING_ALLOCATIONS */
  #endif /* not NDEBUG */

  // free resources
  free(tmpString);

  STRING_UPDATE_VALID(string);
}

/***********************************************************************\
* Name   : ensureStringLength
* Purpose: ensure min. length of string
* Input  : string    - string
*          newLength - new min. length of string
* Output : -
* Return : TRUE if string length is ok, FALSE on insufficient memory
* Notes  : -
\***********************************************************************/

LOCAL_INLINE void ensureStringLength(struct __String *string, ulong newLength)
{
  char  *newData;
  ulong newMaxLength;

  switch (string->type)
  {
    case STRING_TYPE_DYNAMIC:
      if ((newLength + 1) > string->maxLength)
      {
        newMaxLength = ((newLength + 1) + STRING_DELTA_LENGTH - 1) & ~(STRING_DELTA_LENGTH - 1);
        assert(newMaxLength >= (newLength + 1));
        newData = realloc(string->data,newMaxLength*sizeof(char));
//??? error message?
        if (newData == NULL)
        {
          fprintf(stderr,"FATAL ERROR: insufficient memory for allocating string (%lu bytes) - program halted: %s\n",newMaxLength*sizeof(char),strerror(errno));
          abort();
        }
        #ifndef NDEBUG
          #ifdef TRACE_STRING_ALLOCATIONS
            pthread_once(&debugStringInitFlag,debugStringInit);

            pthread_mutex_lock(&debugStringLock);
            {
              debugStringAllocList.memorySize += (newMaxLength-string->maxLength);
            }
            pthread_mutex_unlock(&debugStringLock);
          #endif /* TRACE_STRING_ALLOCATIONS */
          #ifdef FILL_MEMORY
            memset(&newData[string->maxLength],DEBUG_FILL_BYTE,newMaxLength-string->maxLength);
          #endif /* FILL_MEMORY */
        #endif /* not NDEBUG */

        string->data      = newData;
        string->maxLength = newMaxLength;
      }
      break;
    case STRING_TYPE_STATIC:
      if ((newLength + 1) > string->maxLength)
      {
        fprintf(stderr,"FATAL ERROR: exceeded static string (required length %lu, max. length %lu) - program halted\n",newLength,(ulong)string->maxLength-1);
        abort();
      }
      break;
    case STRING_TYPE_CONST:
      printErrorConstString(string);
      break; // not reached
    default:
      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      break; // not reached
  }
}

/***********************************************************************\
* Name   : parseNextFormatToken
* Purpose: parse next format token
* Input  : format - format string
* Output : formatToken - format token
* Return : next char after format specifier
* Notes  : -
\***********************************************************************/

LOCAL const char *parseNextFormatToken(const char *format, FormatToken *formatToken)
{
  #define ADD_CHAR(formatToken,ch) \
    do \
    { \
      assert(formatToken->length<sizeof(formatToken->token)); \
      formatToken->token[formatToken->length] = ch; formatToken->length++; \
    } while (0)

  assert(format != NULL);
  assert(formatToken != NULL);

  formatToken->length           = 0;
  formatToken->alternateFlag    = FALSE;
  formatToken->zeroPaddingFlag  = FALSE;
  formatToken->leftAdjustedFlag = FALSE;
  formatToken->blankFlag        = FALSE;
  formatToken->signFlag         = FALSE;
  formatToken->width            = 0;
  formatToken->precision        = 0;
  formatToken->lengthType       = FORMAT_LENGTH_TYPE_INTEGER;
  formatToken->quoteChar        = NUL;
  formatToken->conversionChar   = NUL;

  // format start character
  assert((*format) == '%');
  ADD_CHAR(formatToken,(*format));
  format++;

  // flags
  while (   ((*format) != NUL)
         && (   ((*format) == '#')
             || ((*format) == '0')
             || ((*format) == '-')
             || ((*format) == ' ')
             || ((*format) == '+')
            )
        )
  {
    ADD_CHAR(formatToken,(*format));
    switch (*format)
    {
      case '#': formatToken->alternateFlag    = TRUE; break;
      case '0': formatToken->zeroPaddingFlag  = TRUE; break;
      case '-': formatToken->leftAdjustedFlag = TRUE; break;
      case ' ': formatToken->blankFlag        = TRUE; break;
      case '+': formatToken->blankFlag        = TRUE; break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    format++;
  }

  // width, precision
  while (   ((*format) != NUL)
         && isdigit((int)(*format))
        )
  {
    ADD_CHAR(formatToken,(*format));

    formatToken->width=formatToken->width*10+((*format)-'0');
    format++;
  }

  // precision
  if (   ((*format) != NUL)
      && ((*format) == '.')
     )
  {
    ADD_CHAR(formatToken,(*format));
    format++;
    while (isdigit((int)(*format)))
    {
      ADD_CHAR(formatToken,(*format));

      formatToken->precision=formatToken->precision*10+((*format)-'0');
      format++;
    }
  }

  // quoting character
  if (   ((*format) != NUL)
      && !isalpha(*format)
      && ((*format) != '%')
      && (   (*(format+1) == 's')
          || (*((format+1)) == 'S')
         )
     )
  {
    formatToken->quoteChar = (*format);
    format++;
  }

  // length modifier
  if ((*format) != NUL)
  {
    if      (((*format) == 'h') && (*((format+1)) == 'h'))
    {
      ADD_CHAR(formatToken,(*(format+0)));
      ADD_CHAR(formatToken,(*(format+1)));

      formatToken->lengthType = FORMAT_LENGTH_TYPE_INTEGER;
      format += 2;
    }
    else if ((*format) == 'h')
    {
      ADD_CHAR(formatToken,(*format));

      formatToken->lengthType = FORMAT_LENGTH_TYPE_INTEGER;
      format++;
    }
    else if (((*format) == 'l') && (*((format+1)) == 'l'))
    {
      ADD_CHAR(formatToken,(*(format+0)));
      ADD_CHAR(formatToken,(*(format+1)));

      formatToken->lengthType = FORMAT_LENGTH_TYPE_LONGLONG;
      format += 2;
    }
    else if ((*format) == 'l')
    {
      ADD_CHAR(formatToken,(*format));

      formatToken->lengthType = FORMAT_LENGTH_TYPE_LONG;
      format++;
    }
    else if ((*format) == 'q')
    {
      ADD_CHAR(formatToken,(*format));

      formatToken->lengthType = FORMAT_LENGTH_TYPE_QUAD;
      format++;
    }
    else if ((*format) == 'j')
    {
      ADD_CHAR(formatToken,(*format));

      formatToken->lengthType = FORMAT_LENGTH_TYPE_INTEGER;
      format++;
    }
    else if ((*format) == 'z')
    {
      ADD_CHAR(formatToken,(*format));

      formatToken->lengthType = FORMAT_LENGTH_TYPE_INTEGER;
      format++;
    }
    else if ((*format) == 't')
    {
      ADD_CHAR(formatToken,(*format));

      formatToken->lengthType = FORMAT_LENGTH_TYPE_INTEGER;
      format++;
    }
  }

  // conversion character
  if ((*format) != NUL)
  {
    switch (*format)
    {
      case 'S':
        ADD_CHAR(formatToken,'s');
        formatToken->conversionChar = 'S';
        break;
      default:
        ADD_CHAR(formatToken,(*format));
        formatToken->conversionChar = (*format);
        break;
    }
    format++;
  }

  ADD_CHAR(formatToken,NUL);

  return format;

  #undef ADD_CHAR
}

/***********************************************************************\
* Name   : formatString
* Purpose: format a string and append (like printf)
* Input  : String    - string
*          format    - format string
*          arguments - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef __GNUC__
/* we have here some snprintf()-calls with a string variable as format
   string. This cause a warning. The string variable is OK, thus disable
   this warning in this function.
*/
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wformat-security"
#endif /* __GNUC__ */

LOCAL void formatString(struct __String *string,
                        const char      *format,
                        va_list         arguments
                       )
{
  FormatToken  formatToken;
  union
  {
    int                i;
    long               l;
    #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
      long long          ll;
    #endif
    unsigned int       ui;
    unsigned long      ul;
    #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
      unsigned long long ull;
    #endif
    float              f;
    double             d;
    const char         *s;
    void               *p;
    #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
      unsigned long long bits;
    #else
      unsigned long      bits;
    #endif
    struct __String    *string;
  } data;
  char          buffer[64];
  uint          length;
  const char    *s;
  ulong         i;
  char          ch;
  uint          j;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  while ((*format) != NUL)
  {
    if ((*format) == '%')
    {
      // get format token
      format = parseNextFormatToken(format,&formatToken);

      // format and store string
      switch (formatToken.conversionChar)
      {
        case 'c':
          data.i = va_arg(arguments,int);
          length = snprintf(buffer,sizeof(buffer),formatToken.token,data.i);
          if (length < sizeof(buffer))
          {
            String_appendCString(string,buffer);
          }
          else
          {
            ensureStringLength(string,string->length+length);
            snprintf(&string->data[string->length],length+1,formatToken.token,data.i);
            string->length += length;
            STRING_UPDATE_VALID(string);
          }
          break;
        case 'i':
        case 'd':
          switch (formatToken.lengthType)
          {
            case FORMAT_LENGTH_TYPE_INTEGER:
              {
                data.i = va_arg(arguments,int);
                length = snprintf(buffer,sizeof(buffer),formatToken.token,data.i);
                if (length < sizeof(buffer))
                {
                  String_appendCString(string,buffer);
                }
                else
                {
                  ensureStringLength(string,string->length+length);
                  snprintf(&string->data[string->length],length+1,formatToken.token,data.i);
                  string->length += length;
                  STRING_UPDATE_VALID(string);
                }
              }
              break;
            case FORMAT_LENGTH_TYPE_LONG:
              {
                data.l = va_arg(arguments,long);
                length = snprintf(buffer,sizeof(buffer),formatToken.token,data.l);
                if (length < sizeof(buffer))
                {
                  String_appendCString(string,buffer);
                }
                else
                {
                  ensureStringLength(string,string->length+length);
                  snprintf(&string->data[string->length],length+1,formatToken.token,data.l);
                  string->length += length;
                  STRING_UPDATE_VALID(string);
                }
              }
              break;
            case FORMAT_LENGTH_TYPE_LONGLONG:
              {
                #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
                  data.ll = va_arg(arguments,long long);
                  length = snprintf(buffer,sizeof(buffer),formatToken.token,data.ll);
                  if (length < sizeof(buffer))
                  {
                    String_appendCString(string,buffer);
                  }
                  else
                  {
                    ensureStringLength(string,string->length+length);
                    snprintf(&string->data[string->length],length+1,formatToken.token,data.ll);
                    string->length += length;
                    STRING_UPDATE_VALID(string);
                  }
                #else /* not _LONG_LONG || HAVE_LONG_LONG */
                  HALT_INTERNAL_ERROR("long long not supported");
                #endif /* _LONG_LONG || HAVE_LONG_LONG */
              }
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }
          break;
        case 'o':
        case 'u':
        case 'x':
        case 'X':
          switch (formatToken.lengthType)
          {
            case FORMAT_LENGTH_TYPE_INTEGER:
              {
                data.ui = va_arg(arguments,unsigned int);
                length = snprintf(buffer,sizeof(buffer),formatToken.token,data.ui);
                if (length < sizeof(buffer))
                {
                  String_appendCString(string,buffer);
                }
                else
                {
                  ensureStringLength(string,string->length+length);
                  snprintf(&string->data[string->length],length+1,formatToken.token,data.ui);
                  string->length += length;
                  STRING_UPDATE_VALID(string);
                }
              }
              break;
            case FORMAT_LENGTH_TYPE_LONG:
              {
                data.ul = va_arg(arguments,unsigned long);
                length = snprintf(buffer,sizeof(buffer),formatToken.token,data.ul);
                if (length < sizeof(buffer))
                {
                  String_appendCString(string,buffer);
                }
                else
                {
                  ensureStringLength(string,string->length+length);
                  snprintf(&string->data[string->length],length+1,formatToken.token,data.ul);
                  string->length += length;
                  STRING_UPDATE_VALID(string);
                }
              }
              break;
            case FORMAT_LENGTH_TYPE_LONGLONG:
              {
                #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
                  data.ull = va_arg(arguments,unsigned long long);
                  length = snprintf(buffer,sizeof(buffer),formatToken.token,data.ull);
                  if (length < sizeof(buffer))
                  {
                    String_appendCString(string,buffer);
                  }
                  else
                  {
                    ensureStringLength(string,string->length+length);
                    snprintf(&string->data[string->length],length+1,formatToken.token,data.ull);
                    string->length += length;
                    STRING_UPDATE_VALID(string);
                  }
                #else /* not _LONG_LONG || HAVE_LONG_LONG */
                  HALT_INTERNAL_ERROR("long long not supported");
                #endif /* _LONG_LONG || HAVE_LONG_LONG */
              }
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }
          break;
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
          switch (formatToken.lengthType)
          {
            case FORMAT_LENGTH_TYPE_INTEGER:
            case FORMAT_LENGTH_TYPE_LONG:
              {
                data.d = va_arg(arguments,double);
                length = snprintf(buffer,sizeof(buffer),formatToken.token,data.d);
                if (length < sizeof(buffer))
                {
                  String_appendCString(string,buffer);
                }
                else
                {
                  ensureStringLength(string,string->length+length);
                  snprintf(&string->data[string->length],length+1,formatToken.token,data.d);
                  string->length += length;
                  STRING_UPDATE_VALID(string);
                }
              }
              break;
            case FORMAT_LENGTH_TYPE_LONGLONG:
              {
                HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
              }
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              #endif /* NDEBUG */
              break; /* not reached */
          }
          break;
        case 's':
          data.s = va_arg(arguments,const char*);

          if (formatToken.quoteChar != NUL)
          {
            // quoted string
            String_appendChar(string,formatToken.quoteChar);
            if (data.s != NULL)
            {
              s = data.s;
              while ((ch = (*s)) != NUL)
              {
                if (ch == formatToken.quoteChar)
                {
                  String_appendChar(string,STRING_ESCAPE_CHARACTER);
                  String_appendChar(string,ch);
                }
                else
                {
                  // check if mapped character
                  j = 0;
                  while ((j < STRING_ESCAPE_CHARACTER_MAP_LENGTH) && (STRING_ESCAPE_CHARACTERS_MAP_FROM[j] != ch))
                  {
                    j++;
                  }

                  if (j < STRING_ESCAPE_CHARACTER_MAP_LENGTH)
                  {
                    assert(j < SIZE_OF_ARRAY(STRING_ESCAPE_CHARACTERS_MAP_TO));

                    // mapped character
                    String_appendChar(string,STRING_ESCAPE_CHARACTER);
                    String_appendChar(string,STRING_ESCAPE_CHARACTERS_MAP_TO[j]);
                  }
                  else if (ch == STRING_ESCAPE_CHARACTER)
                  {
                    // escape character
                    String_appendChar(string,STRING_ESCAPE_CHARACTER);
                    String_appendChar(string,STRING_ESCAPE_CHARACTER);
                  }
                  else
                  {
                    // non-mapped character
                    String_appendChar(string,ch);
                  }
                }
                s++;
              }
            }
            String_appendChar(string,formatToken.quoteChar);
          }
          else
          {
            // non quoted string
            if (data.s != NULL)
            {
              length = snprintf(buffer,sizeof(buffer),formatToken.token,data.s);
              if (length < sizeof(buffer))
              {
                String_appendCString(string,buffer);
              }
              else
              {
                ensureStringLength(string,string->length+length);
                snprintf(&string->data[string->length],length+1,formatToken.token,data.s);
                string->length += length;
                STRING_UPDATE_VALID(string);
              }
            }
          }
          break;
        case 'p':
        case 'n':
          data.p = va_arg(arguments,void*);

          if (data.p != NULL)
          {
            length = snprintf(buffer,sizeof(buffer),formatToken.token,data.p);
            if (length < sizeof(buffer))
            {
              String_appendCString(string,buffer);
            }
            else
            {
              ensureStringLength(string,string->length+length);
              snprintf(&string->data[string->length],length+1,formatToken.token,data.p);
              string->length += length;
              STRING_UPDATE_VALID(string);
            }
          }
          break;
        case 'S':
          data.string = (struct __String*)va_arg(arguments,void*);
          assert(string != NULL);
          STRING_CHECK_VALID(data.string);
          STRING_CHECK_ASSIGNABLE(string);

          if (formatToken.quoteChar != NUL)
          {
            // quoted string
            String_appendChar(string,formatToken.quoteChar);
            i = 0L;
            while (i < String_length(data.string))
            {
              ch = String_index(data.string,i);
              if (ch == formatToken.quoteChar)
              {
                String_appendChar(string,STRING_ESCAPE_CHARACTER);
                String_appendChar(string,ch);
              }
              else
              {
                // check if mapped character
                j = 0;
                while ((j < STRING_ESCAPE_CHARACTER_MAP_LENGTH) && (STRING_ESCAPE_CHARACTERS_MAP_FROM[j] != ch))
                {
                  j++;
                }

                if      (j < STRING_ESCAPE_CHARACTER_MAP_LENGTH)
                {
                  // mapped character
                  assert(j < SIZE_OF_ARRAY(STRING_ESCAPE_CHARACTERS_MAP_TO));

                  String_appendChar(string,STRING_ESCAPE_CHARACTER);
                  String_appendChar(string,STRING_ESCAPE_CHARACTERS_MAP_TO[j]);
                }
                else if (ch == STRING_ESCAPE_CHARACTER)
                {
                  // escape character
                  String_appendChar(string,STRING_ESCAPE_CHARACTER);
                  String_appendChar(string,STRING_ESCAPE_CHARACTER);
                }
                else
                {
                  // non-mapped character
                  String_appendChar(string,ch);
                }
              }
              i++;
            }
            String_appendChar(string,formatToken.quoteChar);
          }
          else
          {
            // non quoted string
            length = snprintf(buffer,sizeof(buffer),formatToken.token,String_cString(data.string));
            if (length < sizeof(buffer))
            {
              String_appendCString(string,buffer);
            }
            else
            {
              ensureStringLength(string,string->length+length);
              snprintf(&string->data[string->length],length+1,formatToken.token,String_cString(data.string));
              string->length += length;
              STRING_UPDATE_VALID(string);
            }
          }
          break;
        case 'b':
          // binaray value
          switch (formatToken.lengthType)
          {
            case FORMAT_LENGTH_TYPE_INTEGER:
              #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
                data.bits = (unsigned long long)va_arg(arguments,unsigned int);
              #else
                data.bits = (unsigned long)va_arg(arguments,unsigned int);
              #endif /* _LONG_LONG || HAVE_LONG_LONG */
              break;
            case FORMAT_LENGTH_TYPE_LONG:
              #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
                data.bits = (unsigned long long)va_arg(arguments,unsigned long);
              #else
                data.bits = (unsigned long)va_arg(arguments,unsigned long);
              #endif /* _LONG_LONG || HAVE_LONG_LONG */
              break;
            case FORMAT_LENGTH_TYPE_LONGLONG:
              #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
                data.bits = va_arg(arguments,unsigned long long);
              #else /* not _LONG_LONG || HAVE_LONG_LONG */
                HALT_INTERNAL_ERROR("long long not supported");
              #endif /* _LONG_LONG || HAVE_LONG_LONG */
              break;
            case FORMAT_LENGTH_TYPE_DOUBLE:
            case FORMAT_LENGTH_TYPE_QUAD:
            case FORMAT_LENGTH_TYPE_POINTER:
              #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
                data.bits = (unsigned long long)va_arg(arguments,unsigned int);
              #else
                data.bits = (unsigned long)va_arg(arguments,unsigned int);
              #endif /* _LONG_LONG || HAVE_LONG_LONG */
              break;
            #ifndef NDEBUG
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break; /* not reached */
            #endif /* NDEBUG */
          }

          // get width
          i = formatToken.width;
          #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
            while ((unsigned long long)(1 << i) < data.bits)
          #else
            while ((unsigned long)(1 << i) < data.bits)
          #endif
          {
            i++;
          }

          // format bits
          while (i > 0L)
          {
            String_appendChar(string,((data.bits & (1 << (i-1))) != 0) ? '1' : '0');
            i--;
          }
          break;
        case 'y':
          data.i = va_arg(arguments,int);
          String_appendCString(string,(data.i != 0) ? "yes" : "no");
          break;
        case '%':
          String_appendChar(string,'%');
          break;
        default:
HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
#if 0
          length = snprintf(buffer,sizeof(buffer),formatToken.token);
          if (length < sizeof(buffer))
          {
            String_appendCString(string,buffer);
          }
          else
          {
            ensureStringLength(string,string->length+length);
            snprintf(&string->data[string->length],length+1,formatToken.token);
            string->length += length;
            STRING_UPDATE_VALID(string);
          }
#endif
          break;
      }
    }
    else
    {
      String_appendChar(string,(*format));
      format++;
    }
  }
}

#ifdef __GNUC__
#pragma GCC pop_options
#endif /* __GNUC__ */

/***********************************************************************\
* Name   : parseString
* Purpose: parse a string (like scanf)
* Input  : String       - string
*          format       - format string
*          arguments    - arguments
*          stringQuotes - string chars or NULL
* Output : nextIndex - index of next character in string not parsed
*                      (can be NULL)
* Return : TRUE if parsing sucessful, FALSE otherwise
* Notes  : Additional conversion chars:
*            S - string
*            y - boolean
*          Not implemented conversion chars:
*            p
*            n
\***********************************************************************/

LOCAL bool parseString(const char *string,
                       ulong      length,
                       ulong      index,
                       const char *format,
                       va_list    arguments,
                       const char *stringQuotes,
                       long       *nextIndex
                      )
{
  FormatToken formatToken;
  union
  {
    int                *i;
    long               *l;
    long long          *ll;
    unsigned int       *ui;
    unsigned long      *ul;
    unsigned long long *ull;
    float              *f;
    double             *d;
    char               *c;
    char               *s;
    void               *p;
    bool               *b;
    struct __String    *string;
  } value;
  char        buffer[64];
  ulong       i;
  uint        z;
  const char  *stringQuote;
  bool        foundFlag;

  while ((*format) != NUL)
  {
    // skip white spaces in format
    while (((*format) != NUL) && isspace(*format))
    {
      format++;
    }

    // skip white-spaces in string
    while ((index < length) && isspace(string[index]))
    {
      index++;
    }

    if ((*format) != NUL)
    {
      if ((*format) == '%')
      {
        // get format token
        format = parseNextFormatToken(format,&formatToken);

        // parse string and store values
        switch (formatToken.conversionChar)
        {
          case 'i':
          case 'd':
            i = 0L;

            // get +,-
            if ((index < length) && ((string[index] == '+') || (string[index] == '-')))
            {
              buffer[i] = string[index];
              i++;
              index++;
            }

            // get data
            while (   (index < length)
                   && (i < sizeof(buffer)-1)
                   && isdigit(string[index])
                  )
            {
              buffer[i] = string[index];
              i++;
              index++;
            }
            buffer[i] = NUL;

            // convert
            if (i > 0)
            {
              switch (formatToken.lengthType)
              {
                case FORMAT_LENGTH_TYPE_INTEGER:
                  value.i = va_arg(arguments,int*);
                  if (value.i != NULL) (*value.i) = strtol(buffer,NULL,10);
                  break;
                case FORMAT_LENGTH_TYPE_LONG:
                  value.l = va_arg(arguments,long int*);
                  if (value.l != NULL) (*value.l) = strtol(buffer,NULL,10);
                  break;
                case FORMAT_LENGTH_TYPE_LONGLONG:
                  value.ll = va_arg(arguments,long long int*);
                  if (value.ll != NULL) (*value.ll) = strtoll(buffer,NULL,10);
                  break;
                default:
                  #ifndef NDEBUG
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  #endif /* NDEBUG */
                  break; /* not reached */
              }
            }
            else
            {
              return FALSE;
            }
            break;
          case 'u':
            // skip +
            if ((index < length) && (string[index] == '+'))
            {
              index++;
            }

            // get data
            i = 0L;
            while (   (index < length)
                   && (i < sizeof(buffer)-1)
                   && isdigit(string[index])
                  )
            {
              buffer[i] = string[index];
              i++;
              index++;
            }
            buffer[i] = NUL;

            // convert
            if (i > 0)
            {
              switch (formatToken.lengthType)
              {
                case FORMAT_LENGTH_TYPE_INTEGER:
                  value.ui = va_arg(arguments,unsigned int*);
                  if (value.ui != NULL) (*value.i) = (unsigned int)strtol(buffer,NULL,10);
                  break;
                case FORMAT_LENGTH_TYPE_LONG:
                  value.ul = va_arg(arguments,unsigned long int*);
                  if (value.ul != NULL) (*value.l) = (unsigned long int)strtol(buffer,NULL,10);
                  break;
                case FORMAT_LENGTH_TYPE_LONGLONG:
                  value.ull = va_arg(arguments,unsigned long long int*);
                  if (value.ull != NULL) (*value.ull) = (unsigned long long int)strtoll(buffer,NULL,10);
                  break;
                default:
                  #ifndef NDEBUG
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  #endif /* NDEBUG */
                  break; /* not reached */
              }
            }
            else
            {
              return FALSE;
            }
            break;
          case 'c':
            // convert
            if (index < length)
            {
              value.c = va_arg(arguments,char*);
              if (value.c != NULL) (*value.c) = string[index];
              index++;
            }
            else
            {
              return FALSE;
            }
            break;
          case 'o':
            // get data
            i = 0L;
            while (   (index < length)
                   && (i < sizeof(buffer)-1)
                   && (string[index] >= '0')
                   && (string[index] <= '7')
                  )
            {
              buffer[i] = string[index];
              i++;
              index++;
            }
            buffer[i] = NUL;

            // convert
            if (i > 0)
            {
              switch (formatToken.lengthType)
              {
                case FORMAT_LENGTH_TYPE_INTEGER:
                  value.i = va_arg(arguments,int*);
                  if (value.i != NULL) (*value.i) = strtol(buffer,NULL,8);
                  break;
                case FORMAT_LENGTH_TYPE_LONG:
                  value.l = va_arg(arguments,long int*);
                  if (value.l != NULL) (*value.l) = strtol(buffer,NULL,10);
                  break;
                case FORMAT_LENGTH_TYPE_LONGLONG:
                  value.ll = va_arg(arguments,long long int*);
                  if (value.ll != NULL) (*value.ll) = strtoll(buffer,NULL,10);
                  break;
                default:
                  #ifndef NDEBUG
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  #endif /* NDEBUG */
                  break; /* not reached */
              }
            }
            else
            {
              return FALSE;
            }
            break;
          case 'x':
          case 'X':
            // skip prefix 0x
            if (((index+1) < length) && (string[index+0] == '0') && (string[index+1] == 'x'))
            {
              index += 2;
            }

            // get data
            i = 0L;
            while (   (index < length)
                   && (i < sizeof(buffer)-1)
                   && isdigit(string[index])
                  )
            {
              buffer[i] = string[index];
              i++;
              index++;
            }
            buffer[i] = NUL;

            // convert
            if (i > 0)
            {
              switch (formatToken.lengthType)
              {
                case FORMAT_LENGTH_TYPE_INTEGER:
                  value.i = va_arg(arguments,int*);
                  if (value.i != NULL) (*value.i) = strtol(buffer,NULL,16);
                  break;
                case FORMAT_LENGTH_TYPE_LONG:
                  value.l = va_arg(arguments,long int*);
                  if (value.l != NULL) (*value.l) = strtol(buffer,NULL,16);
                  break;
                case FORMAT_LENGTH_TYPE_LONGLONG:
                  value.ll = va_arg(arguments,long long int*);
                  if (value.ll != NULL) (*value.ll) = strtol(buffer,NULL,16);
                  break;
                default:
                  #ifndef NDEBUG
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  #endif /* NDEBUG */
                  break; /* not reached */
              }
            }
            else
            {
              return FALSE;
            }
            break;
          case 'e':
          case 'E':
          case 'f':
          case 'F':
          case 'g':
          case 'G':
          case 'a':
          case 'A':
            i = 0L;

            // get +,0,.
            if ((index < length) && ((string[index] == '+') || (string[index] == '-')  || (string[index] == '.')))
            {
              buffer[i] = string[index];
              i++;
              index++;
            }

            // get data
            while (   (index < length)
                   && (i < sizeof(buffer)-1)
                   && isdigit(string[index])
                  )
            {
              buffer[i] = string[index];
              i++;
              index++;
            }
            if ((index < length) && (string[index] == '.'))
            {
              buffer[i] = '.';
              i++;
              index++;
              while (   (index < length)
                     && (i < sizeof(buffer)-1)
                     && isdigit(string[index])
                    )
              {
                buffer[i] = string[index];
                i++;
                index++;
              }
            }
            buffer[i] = NUL;

            // convert
            if (i > 0)
            {
              switch (formatToken.lengthType)
              {
                case FORMAT_LENGTH_TYPE_INTEGER:
                  value.f = va_arg(arguments,float*);
                  if (value.f != NULL) (*value.f) = strtod(buffer,NULL);
                  break;
                case FORMAT_LENGTH_TYPE_LONG:
                  value.d = va_arg(arguments,double*);
                  if (value.d != NULL) (*value.d) = strtod(buffer,NULL);
                  break;
                case FORMAT_LENGTH_TYPE_LONGLONG:
                  HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
                  break;
                default:
                  #ifndef NDEBUG
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  #endif /* NDEBUG */
                  break; /* not reached */
              }
            }
            else
            {
              return FALSE;
            }
            break;
          case 's':
            // get and copy data
            value.s = va_arg(arguments,char*);
            assert(formatToken.width > 0);

            i = 0L;
            if (index < length)
            {
              while (   (index < length)
                     && (formatToken.blankFlag || !isspace(string[index]))
                     && (string[index] != (*format))
                    )
              {
                if (   (string[index] == STRING_ESCAPE_CHARACTER)
                    && ((index+1) < length)
                    && !formatToken.blankFlag
                   )
                {
                  // quoted character
                  if ((formatToken.width == 0) || (i < formatToken.width-1))
                  {
                    String_appendChar(value.string,string[index+1]);
                    i++;
                  }
                  index += 2;
                }
                else
                {
                  // check for string quote
                  stringQuote = NULL;
                  if ((formatToken.quoteChar != NUL) && (formatToken.quoteChar == string[index])) stringQuote = &formatToken.quoteChar;
                  if ((stringQuote == NULL) && (stringQuotes != NULL)) stringQuote = strchr(stringQuotes,string[index]);

                  if (   (stringQuote != NULL)
                      && !formatToken.blankFlag
                     )
                  {
                    do
                    {
                      // skip quote-char
                      index++;

                      // get string
                      while ((index < length) && (string[index] != (*stringQuote)))
                      {
                        if (   ((index+1) < length)
                            && (string[index] == STRING_ESCAPE_CHARACTER)
                            && (string[index+1] == (*stringQuote))
                           )
                        {
                          if ((formatToken.width == 0) || (i < formatToken.width-1))
                          {
                            if (value.s != NULL) value.s[i] = string[index+1];
                            i++;
                          }
                          index += 2;
                        }
                        else
                        {
                          if (i < (formatToken.width-1))
                          {
                            if (value.s != NULL) value.s[i] = string[index];
                            i++;
                          }
                          index++;
                        }
                      }

                      // skip quote-char
                      if (index < length)
                      {
                        index++;
                      }

                      stringQuote = NULL;
                      if (index < length)
                      {
                        if ((formatToken.quoteChar != NUL) && (formatToken.quoteChar == string[index])) stringQuote = &formatToken.quoteChar;
                        if ((stringQuote == NULL) && (stringQuotes != NULL)) stringQuote = strchr(stringQuotes,string[index]);
                      }
                    }
                    while (stringQuote != NULL);
                  }
                  else
                  {
                    if (i < (formatToken.width-1))
                    {
                      if (value.s != NULL) value.s[i] = string[index];
                      i++;
                    }
                    index++;
                  }
                }
              }
            }
            if (value.s != NULL) value.s[i] = NUL;
            break;
          case 'p':
          case 'n':
            break;
          case 'S':
            // get and copy data
            value.string = va_arg(arguments,String);
            STRING_CHECK_VALID(value.string);
            STRING_CHECK_ASSIGNABLE(value.string);

            String_clear(value.string);
            if (index < length)
            {
              i = 0;
              while (   (index < length)
                     && (formatToken.blankFlag || !isspace(string[index]))
// NUL in string here a problem?
                     && (string[index] != (*format))
                    )
              {
                if (   (string[index] == STRING_ESCAPE_CHARACTER)
                    && ((index+1) < length)
                    && !formatToken.blankFlag
                   )
                {
                  // quoted character
                  if ((formatToken.width == 0) || (i < formatToken.width-1))
                  {
                    String_appendChar(value.string,string[index+1]);
                    i++;
                  }
                  index += 2;
                }
                else
                {
                  // check for string quote
                  stringQuote = NULL;
                  if ((formatToken.quoteChar != NUL) && (formatToken.quoteChar == string[index])) stringQuote = &formatToken.quoteChar;
                  if ((stringQuote == NULL) && (stringQuotes != NULL)) stringQuote = strchr(stringQuotes,string[index]);

                  if (   (stringQuote != NULL)
                      && !formatToken.blankFlag
                     )
                  {
                    do
                    {
                      // skip quote-char
                      index++;

                      // get string
                      while ((index < length) && (string[index] != (*stringQuote)))
                      {
                        if (   ((index+1) < length)
                            && (string[index] == STRING_ESCAPE_CHARACTER)
                            && (string[index+1] == (*stringQuote))
                           )
                        {
                          if ((formatToken.width == 0) || (i < formatToken.width-1))
                          {
                            String_appendChar(value.string,string[index+1]);
                            i++;
                          }
                          index += 2;
                        }
                        else
                        {
                          if ((formatToken.width == 0) || (i < formatToken.width-1))
                          {
                            String_appendChar(value.string,string[index]);
                            i++;
                          }
                          index++;
                        }
                      }

                      // skip quote-char
                      if (index < length)
                      {
                        index++;
                      }

                      // check for string quote
                      stringQuote = NULL;
                      if (index < length)
                      {
                        if ((formatToken.quoteChar != NUL) && (formatToken.quoteChar == string[index])) stringQuote = &formatToken.quoteChar;
                        if ((stringQuote == NULL) && (stringQuotes != NULL)) stringQuote = strchr(stringQuotes,string[index]);
                      }
                    }
                    while (stringQuote != NULL);
                  }
                  else
                  {
                    if ((formatToken.width == 0) || (i < formatToken.width-1))
                    {
                      String_appendChar(value.string,string[index]);
                      i++;
                    }
                    index++;
                  }
                }
              }
            }
            break;
#if 0
still not implemented
          case 'b':
            // binaray value
            switch (formatToken.lengthType)
            {
              case FORMAT_LENGTH_TYPE_INTEGER:
                {
                  unsigned int bits;

                  bits = va_arg(arguments,unsigned int);
                }
                break;
              case FORMAT_LENGTH_TYPE_LONG:
                {
                  unsigned long bits;

                  bits = va_arg(arguments,unsigned long);
                }
                break;
              case FORMAT_LENGTH_TYPE_LONGLONG:
                {
                  #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
                    unsigned long long bits;

                    bits = va_arg(arguments,unsigned long long);
                  }
                  #else /* not _LONG_LONG || HAVE_LONG_LONG */
                    HALT_INTERNAL_ERROR("long long not supported");
                  #endif /* _LONG_LONG || HAVE_LONG_LONG */
                break;
              #ifndef NDEBUG
                default:
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                  break; /* not reached */
              #endif /* NDEBUG */
            }
            bufferCount = 0;
            FLUSH_OUTPUT();
  HALT_NOT_YET_IMPLEMENTED();
            break;
#endif /* 0 */
          case 'y':
            // get data
            i = 0L;
            while (   (index < length)
                   && !isspace(string[index])
                  )
            {
              if (i < sizeof(buffer)-1)
              {
                buffer[i] = string[index];
                i++;
              }
              index++;
            }
            buffer[i] = NUL;

            // convert
            if (i > 0)
            {
              value.b = va_arg(arguments,bool*);
              foundFlag = FALSE;
              z = 0;
              while (!foundFlag && (z < SIZE_OF_ARRAY(DEFAULT_TRUE_STRINGS)))
              {
                if (strcmp(buffer,DEFAULT_TRUE_STRINGS[z]) == 0)
                {
                  if (value.b != NULL) (*value.b) = TRUE;
                  foundFlag = TRUE;
                }
                z++;
              }
              z = 0;
              while (!foundFlag && (z < SIZE_OF_ARRAY(DEFAULT_FALSE_STRINGS)))
              {
                if (strcmp(buffer,DEFAULT_FALSE_STRINGS[z]) == 0)
                {
                  if (value.b != NULL) (*value.b) = FALSE;
                  foundFlag = TRUE;
                }
                z++;
              }

              if (!foundFlag)
              {
                return FALSE;
              }
            }
            else
            {
              return FALSE;
            }
            break;
          case '*':
            // skip value
            while (   (index < length)
                   && !isspace(string[index])
                   && (string[index] != (*format))
                  )
            {
              index++;
            }
            break;
          case '%':
            if ((index >= length) || (string[index] != '%'))
            {
              return FALSE;
            }
            index++;
            break;
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        }
      }
      else
      {
        if ((index >= length) || (string[index] != (*format)))
        {
          return FALSE;
        }
        index++;
        format++;
      }
    }
  }
  if (nextIndex != NULL)
  {
    (*nextIndex) = (index >= length) ? STRING_END : index;
  }
  else
  {
    if (index < length)
    {
      return FALSE;
    }
  }

  return TRUE;
}

/***********************************************************************\
* Name   : getUnitFactor
* Purpose: get unit factor
* Input  : stringUnits     - string units
*          stringUnitCount - number of string units
*          string          - string start
*          unitString      - unit string start
* Output : nextIndex - index of next character in string not parsed or
*                      STRING_END if string completely parsed (can be
*                      NULL)
* Return : unit factor
* Notes  : -
\***********************************************************************/

LOCAL ulong getUnitFactor(const StringUnit stringUnits[],
                          uint             stringUnitCount,
                          const char       *string,
                          const char       *unitString,
                          long             *nextIndex
                         )
{
  uint  z;
  ulong factor;

  assert(stringUnits != NULL);
  assert(string != NULL);
  assert(unitString != NULL);

  z = 0;
  while ((z < stringUnitCount) && (strcmp(unitString,stringUnits[z].name) != 0))
  {
    z++;
  }
  if (z < stringUnitCount)
  {
    factor = stringUnits[z].factor;
    if (nextIndex != NULL) (*nextIndex) = STRING_END;
  }
  else
  {
    factor = 1L;
    if (nextIndex != NULL) (*nextIndex) = (ulong)(unitString-string);
  }

  return factor;
}

/***********************************************************************\
* Name   : matchString
* Purpose: match string
* Input  : string    - string to patch
*          index     - start index in string
*          pattern   - regualar expression pattern
*          nextIndex - variable for index of next not matched
*                      character (can be NULL)
* Output : nextIndex - index of next not matched character
* Return : TRUE if string matched, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool matchString(ConstString  string,
                       ulong        index,
                       const char   *pattern,
                       long         *nextIndex
                      )
{
  bool       matchFlag;
  #if defined(HAVE_PCRE) || defined(HAVE_REGEX_H)
    regex_t    regex;
    regmatch_t subMatches[1];
  #endif /* HAVE_PCRE || HAVE_REGEX_H */

  assert(string != NULL);
  assert(string->data != NULL);
  assert(pattern != NULL);

  if (index < string->length)
  {
    #if defined(HAVE_PCRE) || defined(HAVE_REGEX_H)
      // compile pattern
      if (regcomp(&regex,pattern,REG_ICASE|REG_EXTENDED) != 0)
      {
        return FALSE;
      }

      // match
      matchFlag = (regexec(&regex,
                           &string->data[index],
                           1,  // subMatchCount
                           subMatches,
                           0  // eflags
                          ) == 0
                  );

      // get next index
      if (matchFlag)
      {
        if (nextIndex != NULL)
        {
          assert(subMatches[0].rm_eo >= subMatches[0].rm_so);
          (*nextIndex) = index+subMatches[0].rm_eo-subMatches[0].rm_so;
        }
      }

      // free resources
      regfree(&regex);
    #else /* not HAVE_PCRE || HAVE_REGEX_H */
      UNUSED_VARIABLE(pattern);
      UNUSED_VARIABLE(nextIndex);

      matchFlag = FALSE;
    #endif /* HAVE_PCRE || HAVE_REGEX_H */
  }
  else
  {
    matchFlag = FALSE;
  }

  return matchFlag;
}

/***********************************************************************\
* Name   : vmatchString
* Purpose: match string with arguments
* Input  : string            - string to patch
*          index             - start index in string
*          pattern           - regualar expression pattern
*          nextIndex         - variable for index of next not matched
*                              character (can be NULL)
*          matchedString     - matched string variable (can be NULL)
*          matchedSubStrings - matched sub-string variables
* Output : nextIndex         - index of next not matched character
*          matchedString     - matched string
*          matchedSubStrings - matched sub-strings
* Return : TRUE if string matched, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool vmatchString(ConstString  string,
                        ulong        index,
                        const char   *pattern,
                        long         *nextIndex,
                        String       matchedString,
                        va_list      matchedSubStrings
                       )
{
  bool       matchFlag;
  #if defined(HAVE_PCRE) || defined(HAVE_REGEX_H)
    regex_t    regex;
    va_list    arguments;
    String     matchedSubString;
    regmatch_t *subMatches;
    uint       subMatchCount;
    uint       z;
  #endif /* HAVE_PCRE || HAVE_REGEX_H */

  assert(string != NULL);
  assert(string->data != NULL);
  assert(pattern != NULL);

  if (index < string->length)
  {
    #if defined(HAVE_PCRE) || defined(HAVE_REGEX_H)
      // compile pattern
      if (regcomp(&regex,pattern,REG_ICASE|REG_EXTENDED) != 0)
      {
        return FALSE;
      }

      // count sub-patterns (=1 for total matched string + number of matched-sub-strings)
      va_copy(arguments,matchedSubStrings);
      subMatchCount = 1;
      do
      {
        matchedSubString = (String)va_arg(arguments,void*);
        if (matchedSubString != NULL) subMatchCount++;
      }
      while (matchedSubString != NULL);
      va_end(arguments);

      // allocate sub-patterns array
      subMatches = (regmatch_t*)malloc(subMatchCount*sizeof(regmatch_t));
      if (subMatches == NULL)
      {
        regfree(&regex);
        return FALSE;
      }

      // match
      matchFlag = (regexec(&regex,
                           &string->data[index],
                           subMatchCount,
                           subMatches,
                           0  // eflags
                          ) == 0
                  );

      // get next index, sub-matches
      if (matchFlag)
      {
        if (nextIndex != NULL)
        {
          assert(subMatches[0].rm_eo >= subMatches[0].rm_so);
          (*nextIndex) = index+subMatches[0].rm_eo-subMatches[0].rm_so;
        }

        if (matchedString != STRING_NO_ASSIGN)
        {
          String_setBuffer(matchedString,&string->data[subMatches[0].rm_so],subMatches[0].rm_eo-subMatches[0].rm_so);
        }

        va_copy(arguments,matchedSubStrings);
        for (z = 1; z < subMatchCount; z++)
        {
          matchedSubString = (String)va_arg(arguments,void*);
          assert(matchedSubString != NULL);
          if (matchedSubString != STRING_NO_ASSIGN)
          {
            if (subMatches[z].rm_so != -1)
            {
              assert(subMatches[0].rm_eo >= subMatches[0].rm_so);
              String_setBuffer(matchedSubString,&string->data[subMatches[z].rm_so],subMatches[z].rm_eo-subMatches[z].rm_so);
            }
          }
        }
        va_end(arguments);
      }

      // free resources
      free(subMatches);
      regfree(&regex);
    #else /* not HAVE_PCRE || HAVE_REGEX_H */
      UNUSED_VARIABLE(pattern);
      UNUSED_VARIABLE(nextIndex);
      UNUSED_VARIABLE(matchedString);
      UNUSED_VARIABLE(matchedSubStrings);

      matchFlag = FALSE;
    #endif /* HAVE_PCRE || HAVE_REGEX_H */
  }
  else
  {
    matchFlag = FALSE;
  }

  return matchFlag;
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
String String_new(void)
#else /* not DEBUG */
String __String_new(const char *__fileName__, ulong __lineNb__)
#endif /* NDEBUG */
{
  struct __String *string;
  #ifndef NDEBUG
    #ifdef TRACE_STRING_ALLOCATIONS
      DebugStringNode *debugStringNode;
    #endif /* TRACE_STRING_ALLOCATIONS */
    #ifdef MAX_STRINGS_CHECK
      ulong debugStringCount;
    #endif /* MAX_STRINGS_CHECK */
  #endif /* not NDEBUG */

  string = allocString();
  if (string == NULL)
  {
    return NULL;
  }

  #ifndef NDEBUG
    pthread_once(&debugStringInitFlag,debugStringInit);

    pthread_mutex_lock(&debugStringLock);
    {
      #ifdef TRACE_STRING_ALLOCATIONS
        // update allocation info
        debugStringAllocList.memorySize += sizeof(struct __String)+string->maxLength;

        // find string in free-list; reuse or allocate new debug node
        debugStringNode = debugFindString(&debugStringFreeList,string);
        if (debugStringNode != NULL)
        {
          debugRemoveString(&debugStringFreeList,debugStringNode);
          assert(debugStringFreeList.memorySize >= sizeof(DebugStringNode));
          debugStringFreeList.memorySize -= sizeof(DebugStringNode);
        }
        else
        {
          debugStringNode = (DebugStringNode*)__List_newNode(__fileName__,__lineNb__,sizeof(DebugStringNode));
          if (debugStringNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
        }
        debugStringAllocList.memorySize += sizeof(DebugStringNode);

        // init string node
        debugStringNode->allocFileName  = __fileName__;
        debugStringNode->allocLineNb    = __lineNb__;
        #ifdef HAVE_BACKTRACE
          debugStringNode->stackTraceSize = backtrace((void*)debugStringNode->stackTrace,SIZE_OF_ARRAY(debugStringNode->stackTrace));
        #endif /* HAVE_BACKTRACE */
        debugStringNode->deleteFileName = NULL;
        debugStringNode->deleteLineNb   = 0L;
        #ifdef HAVE_BACKTRACE
          debugStringNode->deleteStackTraceSize = 0;
        #endif /* HAVE_BACKTRACE */
        debugStringNode->string         = string;

        // add string to allocated-list
        debugAddString(&debugStringAllocList,debugStringNode);
        #ifdef MAX_STRINGS_CHECK
          debugStringCount = List_count(&debugStringAllocList);
          if (debugStringCount > debugMaxStringNextWarningCount)
          {
            fprintf(stderr,"DEBUG WARNING: %lu strings allocated!\n",debugStringCount);
            debugMaxStringNextWarningCount += WARN_MAX_STRINGS_DELTA;
//String_debugDumpInfo(stderr);
//          sleep(1);
          }
        #endif /* MAX_STRINGS_CHECK */
      #else /* not TRACE_STRING_ALLOCATIONS */
        UNUSED_VARIABLE(__fileName__);
        UNUSED_VARIABLE(__lineNb__);
      #endif /* TRACE_STRING_ALLOCATIONS */
    }
    pthread_mutex_unlock(&debugStringLock);
  #endif /* not NDEBUG */

  STRING_UPDATE_VALID(string);

  return string;
}

#ifdef NDEBUG
String String_newCString(const char *s)
#else /* not NDEBUG */
String __String_newCString(const char *__fileName__, ulong __lineNb__, const char *s)
#endif /* NDEBUG */
{
  String string;

  #ifdef NDEBUG
    string = String_new();
  #else /* not DEBUG */
    string = __String_new(__fileName__,__lineNb__);
  #endif /* NDEBUG */
  String_setCString(string,s);

  STRING_UPDATE_VALID(string);

  return string;
}

#ifdef NDEBUG
String String_newChar(char ch)
#else /* not NDEBUG */
String __String_newChar(const char *__fileName__, ulong __lineNb__, char ch)
#endif /* NDEBUG */
{
  String string;

  #ifdef NDEBUG
    string = String_new();
  #else /* not DEBUG */
    string = __String_new(__fileName__,__lineNb__);
  #endif /* NDEBUG */
  String_setChar(string,ch);

  STRING_UPDATE_VALID(string);

  return string;
}

#ifdef NDEBUG
String String_newBuffer(const void *buffer, ulong bufferLength)
#else /* not NDEBUG */
String __String_newBuffer(const char *__fileName__, ulong __lineNb__, const void *buffer, ulong bufferLength)
#endif /* NDEBUG */
{
  String string;

  #ifdef NDEBUG
    string = String_new();
  #else /* not DEBUG */
    string = __String_new(__fileName__,__lineNb__);
  #endif /* NDEBUG */
  String_setBuffer(string,buffer,bufferLength);

  STRING_UPDATE_VALID(string);

  return string;
}

#ifdef NDEBUG
String String_duplicate(ConstString fromString)
#else /* not NDEBUG */
String __String_duplicate(const char *__fileName__, ulong __lineNb__, ConstString fromString)
#endif /* NDEBUG */
{
  struct __String *string;

  #ifdef NDEBUG
    STRING_CHECK_VALID(fromString);
  #else /* not NDEBUG */
    STRING_CHECK_VALID_AT(__fileName__,__lineNb__,fromString);
  #endif /* NDEBUG */

  if (fromString != NULL)
  {
    assert(fromString->data != NULL);

    #ifdef NDEBUG
      string = String_new();
    #else /* not DEBUG */
      string = __String_new(__fileName__,__lineNb__);
    #endif /* NDEBUG */
    if (string == NULL)
    {
      return NULL;
    }

    ensureStringLength(string,fromString->length);
    memcpy(&string->data[0],&fromString->data[0],fromString->length);
    string->data[fromString->length] =NUL;
    string->length = fromString->length;

    STRING_UPDATE_VALID(string);
  }
  else
  {
    string = NULL;
  }

  return string;
}

#ifdef NDEBUG
String String_copy(String *string, ConstString fromString)
#else /* not NDEBUG */
String __String_copy(const char *__fileName__, ulong __lineNb__, String *string, ConstString fromString)
#endif /* NDEBUG */
{
  #ifdef NDEBUG
    STRING_CHECK_VALID(string);
  #else /* not NDEBUG */
    if (string != NULL) STRING_CHECK_VALID_AT(__fileName__,__lineNb__,*string);
  #endif /* NDEBUG */
  STRING_CHECK_ASSIGNABLE(*string);
  #ifdef NDEBUG
    STRING_CHECK_VALID(fromString);
  #else /* not NDEBUG */
    STRING_CHECK_VALID_AT(__fileName__,__lineNb__,fromString);
  #endif /* NDEBUG */

  if (fromString != NULL)
  {
    assert(fromString->data != NULL);

    if ((*string) == NULL)
    {
      #ifdef NDEBUG
        (*string) = String_new();
      #else /* not DEBUG */
        (*string) = __String_new(__fileName__,__lineNb__);
      #endif /* NDEBUG */
      if ((*string) == NULL)
      {
        return NULL;
      }
    }

    ensureStringLength((*string),fromString->length);
    memcpy(&(*string)->data[0],&fromString->data[0],fromString->length);
    (*string)->data[fromString->length] = NUL;
    (*string)->length                   = fromString->length;

    STRING_UPDATE_VALID(*string);
  }
  else
  {
    if ((*string) != NULL)
    {
      (*string)->data[0] = NUL;
      (*string)->length  = 0L;

      STRING_UPDATE_VALID(*string);
    }
  }

  return (*string);
}

#ifdef NDEBUG
void String_delete(ConstString string)
#else /* not NDEBUG */
void __String_delete(const char *__fileName__, ulong __lineNb__, ConstString string)
#endif /* NDEBUG */
{
  #ifndef NDEBUG
    #ifdef TRACE_STRING_ALLOCATIONS
      DebugStringNode *debugStringNode;
    #endif /* TRACE_STRING_ALLOCATIONS */
  #endif /* not NDEBUG */

  #ifdef NDEBUG
    STRING_CHECK_VALID(string);
  #else /* not NDEBUG */
    STRING_CHECK_VALID_AT(__fileName__,__lineNb__,string);
  #endif /* NDEBUG */
  STRING_CHECK_DYNAMIC(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    #ifndef NDEBUG
      pthread_once(&debugStringInitFlag,debugStringInit);

      #ifdef TRACE_STRING_ALLOCATIONS
        pthread_mutex_lock(&debugStringLock);
        {
          // find string in free-list to check for duplicate free
          debugStringNode = debugFindString(&debugStringFreeList,string);
          if (debugStringNode != NULL)
          {
            fprintf(stderr,"DEBUG WARNING: multiple free of string %p at %s, %lu and previously at %s, %lu which was allocated at %s, %lu!\n",
                    string,
                    __fileName__,
                    __lineNb__,
                    debugStringNode->deleteFileName,
                    debugStringNode->deleteLineNb,
                    debugStringNode->allocFileName,
                    debugStringNode->allocLineNb
                   );
            #ifdef HAVE_BACKTRACE
              fprintf(stderr,"  allocated at\n");
              debugDumpStackTrace(stderr,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugStringNode->stackTrace,debugStringNode->stackTraceSize,0);
              fprintf(stderr,"  deleted at\n");
              debugDumpStackTrace(stderr,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugStringNode->deleteStackTrace,debugStringNode->deleteStackTraceSize,0);
            #endif /* HAVE_BACKTRACE */
            HALT_INTERNAL_ERROR("string delete fail");
          }

          // remove string from allocated list, add string to free-list, shorten list
          debugStringNode = debugFindString(&debugStringAllocList,string);
          if (debugStringNode != NULL)
          {
            // remove from allocated list
            debugRemoveString(&debugStringAllocList,debugStringNode);
            assert(debugStringAllocList.memorySize >= sizeof(DebugStringNode)+sizeof(struct __String)+string->maxLength);
            debugStringAllocList.memorySize -= sizeof(DebugStringNode)+sizeof(struct __String)+string->maxLength;

            // add to free list
            debugStringNode->deleteFileName = __fileName__;
            debugStringNode->deleteLineNb   = __lineNb__;
            #ifdef HAVE_BACKTRACE
              debugStringNode->deleteStackTraceSize = backtrace((void*)debugStringNode->deleteStackTrace,SIZE_OF_ARRAY(debugStringNode->deleteStackTrace));
            #endif /* HAVE_BACKTRACE */
            debugAddString(&debugStringFreeList,debugStringNode);
            debugStringFreeList.memorySize += sizeof(DebugStringNode);

            // shorten free list
            while (debugStringFreeList.count > DEBUG_MAX_FREE_LIST)
            {
              debugStringNode = debugStringFreeList.head;
              debugRemoveString(&debugStringFreeList,debugStringNode);
              debugStringFreeList.memorySize -= sizeof(DebugStringNode);
              LIST_DELETE_NODE(debugStringNode);
            }
          }
          else
          {
            fprintf(stderr,"DEBUG WARNING: string '%s' not found in debug list at %s, line %lu\n",
                    string->data,
                    __fileName__,
                    __lineNb__
                   );
            #ifdef HAVE_BACKTRACE
              debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
            #endif /* HAVE_BACKTRACE */
            HALT_INTERNAL_ERROR("string delete fail");
          }
        }
        pthread_mutex_unlock(&debugStringLock);
      #endif /* TRACE_STRING_ALLOCATIONS */
    #endif /* not NDEBUG */

    free(string->data);
    free((String)string);
  }
}

String String_clear(String string)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    string->data[0] = NUL;
    string->length  = 0L;

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_erase(String string)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    memset(string->data,0,string->maxLength);
    string->length = 0L;

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_set(String string, ConstString sourceString)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (sourceString != NULL)
    {
      assert(sourceString->data != NULL);

      ensureStringLength(string,sourceString->length);
      memmove(&string->data[0],&sourceString->data[0],sourceString->length);
      string->data[sourceString->length] = NUL;
      string->length                     = sourceString->length;
    }
    else
    {
      string->data[0] = NUL;
      string->length  = 0L;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_setCString(String string, const char *s)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      String_setBuffer(string,s,strlen(s));
    }
    else
    {
      string->data[0] = NUL;
      string->length  = 0L;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_setChar(String string, char ch)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  String_setBuffer(string,&ch,1);

  STRING_UPDATE_VALID(string);

  return string;
}

String String_setBuffer(String string, const void *buffer, ulong bufferLength)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (buffer != NULL)
    {
      ensureStringLength(string,bufferLength);
      memmove(&string->data[0],buffer,bufferLength);
      string->data[bufferLength] = NUL;
      string->length             = bufferLength;
    }
    else
    {
      string->data[0] = NUL;
      string->length  = 0L;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_format(String string, const char *format, ...)
{
  va_list arguments;

  assert(string != NULL);
  assert(format != NULL);

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    string->length = 0;
    STRING_UPDATE_VALID(string);

    va_start(arguments,format);
    formatString(string,format,arguments);
    va_end(arguments);
  }

  return string;
}

String String_vformat(String string, const char *format, va_list arguments)
{
  assert(string != NULL);
  assert(format != NULL);

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    string->length = 0;
    STRING_UPDATE_VALID(string);

    formatString(string,format,arguments);
  }

  return string;
}

String String_append(String string, ConstString appendString)
{
  ulong n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);
  STRING_CHECK_VALID(appendString);

  if (string != NULL)
  {
    assert(string->data != NULL);
    if (appendString != NULL)
    {
      n = string->length+appendString->length;
      ensureStringLength(string,n);
      memmove(&string->data[string->length],&appendString->data[0],appendString->length);
      string->data[n] = NUL;
      string->length  = n;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_appendSub(String string, ConstString fromString, ulong fromIndex, long fromLength)
{
  ulong n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);
  STRING_CHECK_VALID(fromString);

  if (string != NULL)
  {
    assert(string->data != NULL);
    if (fromString != NULL)
    {
      assert(fromString->data != NULL);

      if (fromIndex < fromString->length)
      {
        if (fromIndex == STRING_END)
        {
          n = MIN(fromString->length,fromString->length-(ulong)fromLength);
        }
        else
        {
          n = MIN((ulong)fromLength,fromString->length-fromIndex);
        }
        ensureStringLength(string,string->length+n);
        memmove(&string->data[string->length],&fromString->data[fromIndex],n);
        string->data[string->length+n] = NUL;
        string->length += n;
      }
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_appendCString(String string, const char *s)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      String_appendBuffer(string,s,strlen(s));
    }
  }

  return string;
}

String String_appendChar(String string, char ch)
{
  ulong n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);
    n = string->length+1;
    ensureStringLength(string,n);
    string->data[string->length] = ch;
    string->data[n] = NUL;
    string->length  = n;

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_appendCharUTF8(String string, Codepoint codepoint)
{
  size_t l;
  ulong  n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);
    l = charUTF8Length(codepoint);
    n = string->length+l;
    ensureStringLength(string,n);
    memcpy(&string->data[string->length],charUTF8(codepoint),l);
    string->data[n] = NUL;
    string->length  = n;

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_appendBuffer(String string, const char *buffer, ulong bufferLength)
{
  ulong n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);
    if (buffer != NULL)
    {
      n = string->length+bufferLength;
      ensureStringLength(string,n);
      memmove(&string->data[string->length],buffer,bufferLength);
      string->data[n] = NUL;
      string->length  = n;

      STRING_UPDATE_VALID(string);
    }
  }

  return string;
}

String String_appendFormat(String string, const char *format, ...)
{
  va_list arguments;

  assert(string != NULL);
  assert(format != NULL);

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    va_start(arguments,format);
    formatString(string,format,arguments);
    va_end(arguments);
  }

  return string;
}

String String_appendVFormat(String string, const char *format, va_list arguments)
{
  assert(string != NULL);
  assert(format != NULL);

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    formatString(string,format,arguments);
  }

  return string;
}

String String_insert(String string, ulong index, ConstString insertString)
{
  ulong n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);
  STRING_CHECK_VALID(insertString);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (insertString != NULL)
    {
      if      (index == STRING_END)
      {
        n = string->length+insertString->length;
        ensureStringLength(string,n);
        memmove(&string->data[string->length],&insertString->data[0],insertString->length);
        string->data[n] = NUL;
        string->length  = n;
      }
      else if (index <= string->length)
      {
        n = string->length+insertString->length;
        ensureStringLength(string,n);
        memmove(&string->data[index+insertString->length],&string->data[index],string->length-index);
        memmove(&string->data[index],&insertString->data[0],insertString->length);
        string->data[n] = NUL;
        string->length  = n;
      }
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_insertSub(String string, ulong index, ConstString fromString, ulong fromIndex, long fromLength)
{
  ulong n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);
  STRING_CHECK_VALID(fromString);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (fromString != NULL)
    {
      if (fromIndex < fromString->length)
      {
        if (fromIndex == STRING_END)
        {
          n = MIN(fromString->length,fromString->length-(ulong)fromLength);
        }
        else
        {
          n = MIN((ulong)fromLength,fromString->length-fromIndex);
        }

        if      (index == STRING_END)
        {
          ensureStringLength(string,string->length+n);
          memmove(&string->data[string->length],&fromString->data[fromIndex],n);
          string->data[string->length+n] = NUL;
          string->length += n;
        }
        else if (index <= string->length)
        {
          ensureStringLength(string,string->length+n);
          memmove(&string->data[index+n],&string->data[index],string->length-index);
          memmove(&string->data[index],&fromString->data[fromIndex],n);
          string->data[string->length+n] = NUL;
          string->length += n;
        }
      }
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_insertCString(String string, ulong index, const char *s)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      String_insertBuffer(string,index,s,strlen(s));
    }
  }

  return string;
}

String String_insertChar(String string, ulong index, char ch)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    String_insertBuffer(string,index,&ch,1);
  }

  return string;
}

String String_insertBuffer(String string, ulong index, const char *buffer, ulong bufferLength)
{
  ulong n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (buffer != NULL)
    {
      if      (index == STRING_END)
      {
        n = string->length+bufferLength;
        ensureStringLength(string,n);
        memmove(&string->data[string->length],buffer,bufferLength);
        string->data[n] = NUL;
        string->length  = n;
      }
      else if (index <= string->length)
      {
        n = string->length+bufferLength;
        ensureStringLength(string,n);
        memmove(&string->data[index+bufferLength],&string->data[index],string->length-index);
        memmove(&string->data[index],buffer,bufferLength);
        string->data[n] = NUL;
        string->length  = n;
      }
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_remove(String string, ulong index, ulong length)
{
  ulong n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if      (index == STRING_END)
    {
      n = (string->length > length) ? string->length-length : 0L;
      string->data[n] = NUL;
      string->length  = n;
    }
    else if (index < string->length)
    {
      if ((index + length) < string->length)
      {
        memmove(&string->data[index],&string->data[index + length],string->length - (index + length));
        n = string->length - length;
      }
      else
      {
        n = index;
      }
      string->data[n] = NUL;
      string->length  = n;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_replace(String string, ulong index, ulong length, ConstString insertString)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);
  STRING_CHECK_VALID(insertString);

  String_remove(string,index,length);
  String_insert(string,index,insertString);

  return string;
}

String String_replaceCString(String string, ulong index, ulong length, const char *s)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  String_remove(string,index,length);
  String_insertCString(string,index,s);

  return string;
}

String String_replaceChar(String string, ulong index, ulong length, char ch)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  String_remove(string,index,length);
  String_insertChar(string,index,ch);

  return string;
}

String String_replaceBuffer(String string, ulong index, ulong length, const char *buffer, ulong bufferLength)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  String_remove(string,index,length);
  String_insertBuffer(string,index,buffer,bufferLength);

  return string;
}

String String_replaceAll(String string, ulong index, ConstString fromString, ConstString toString)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  return String_map(string,index,&fromString,&toString,1);
}

String String_replaceAllCString(String string, ulong index, const char *from, const char *to)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  return String_mapCString(string,index,&from,&to,1);
}

String String_replaceAllChar(String string, ulong index, char fromCh, char toCh)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  return String_mapChar(string,index,&fromCh,&toCh,1);
}

String String_map(String string, ulong index, ConstString from[], ConstString to[], uint count)
{
  uint  z;
  ulong l0,l1;
  bool  replaceFlag;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  while (index < String_length(string))
  {
    replaceFlag = FALSE;
    for (z = 0; z < count; z++)
    {
      l0 = String_length(from[z]);
      l1 = String_length(to[z]);

      if (String_subEquals(string,from[z],index,l0))
      {
        String_replace(string,index,l0,to[z]);
        index += l1;
        replaceFlag = TRUE;
        break;
      }
    }
    if (!replaceFlag) index++;
  }

  return string;
}

String String_mapCString(String string, ulong index, const char* from[], const char* to[], uint count)
{
  uint  z;
  ulong l0,l1;
  bool  replaceFlag;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  while (index < String_length(string))
  {
    replaceFlag = FALSE;
    for (z = 0; z < count; z++)
    {
      l0 = strlen(from[z]);
      l1 = strlen(to[z]);

      if (String_subEqualsCString(string,from[z],index,l0))
      {
        String_replaceCString(string,index,l0,to[z]);
        index += l1;
        replaceFlag = TRUE;
        break;
      }
    }
    if (!replaceFlag) index++;
  }

  return string;
}

String String_mapChar(String string, ulong index, const char from[], const char to[], uint count)
{
  uint z;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    while (index < string->length)
    {
      for (z = 0; z < count; z++)
      {
        if (string->data[index] == from[z])
        {
          string->data[index] = to[z];
          break;
        }
      }
      index++;
    }
  }

  return string;
}

String String_sub(String string, ConstString fromString, ulong fromIndex, long fromLength)
{
  ulong n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);
  STRING_CHECK_VALID(fromString);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (fromString != NULL)
    {
      assert(fromString->data != NULL);

      if      (fromIndex == STRING_END)
      {
        if (fromLength == STRING_END)
        {
          n = fromString->length;
        }
        else
        {
          n = MIN((ulong)fromLength,fromString->length);
        }
        ensureStringLength(string,n);
        memmove(&string->data[0],&fromString->data[fromString->length-n],n);
        string->data[n] = NUL;
        string->length  = n;
      }
      else if (fromIndex < fromString->length)
      {
        if (fromLength == STRING_END)
        {
          n = fromString->length-fromIndex;
        }
        else
        {
          n = MIN((ulong)fromLength,fromString->length-fromIndex);
        }
        ensureStringLength(string,n);
        memmove(&string->data[0],&fromString->data[fromIndex],n);
        string->data[n] = NUL;
        string->length  = n;
      }
      else
      {
        string->data[0] = NUL;
        string->length  = 0;
      }
    }
    else
    {
      string->data[0] = NUL;
      string->length  = 0L;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

char *String_subCString(char *s, ConstString fromString, ulong fromIndex, long fromLength)
{
  ulong n;

  assert(s != NULL);

  STRING_CHECK_VALID(fromString);

  if (fromLength > 0)
  {
    if (fromString != NULL)
    {
      assert(fromString->data != NULL);

      if      (fromIndex == STRING_END)
      {
        if (fromLength == STRING_END)
        {
          n = fromString->length;
        }
        else
        {
          n = MIN((ulong)fromLength,fromString->length);
        }
        memmove(s,&fromString->data[fromString->length-n],n);
        s[n] = NUL;
      }
      else if (fromIndex < fromString->length)
      {
        if (fromLength == STRING_END)
        {
          n = fromString->length-fromIndex;
        }
        else
        {
          n = MIN((ulong)fromLength,fromString->length-fromIndex);
        }
        memmove(s,&fromString->data[fromIndex],n);
        s[n] = NUL;
      }
      else
      {
        s[0] = NUL;
      }
    }
    else
    {
      s[0] = NUL;
    }
  }

  return s;
}

char *String_subBuffer(char *buffer, ConstString fromString, ulong fromIndex, long fromLength)
{
  ulong n;

  assert(buffer != NULL);

  STRING_CHECK_VALID(fromString);

  if (fromLength > 0)
  {
    if (fromString != NULL)
    {
      assert(fromString->data != NULL);

      if      (fromIndex == STRING_END)
      {
        if (fromLength == STRING_END)
        {
          n = fromString->length;
        }
        else
        {
          n = MIN((ulong)fromLength,fromString->length);
        }
        memmove(&buffer[0],&fromString->data[fromString->length-n],n);
        memset(&buffer[n],0,fromLength-n);
      }
      else if (fromIndex < fromString->length)
      {
        if (fromLength == STRING_END)
        {
          n = fromString->length-fromIndex;
        }
        else
        {
          n = MIN((ulong)fromLength,fromString->length-fromIndex);
        }
        memmove(&buffer[0],&fromString->data[fromIndex],n);
        memset(&buffer[n],0,fromLength-n);
      }
      else
      {
        memset(buffer,0,fromLength);
      }
    }
    else
    {
      memset(buffer,0,fromLength);
    }
  }

  return buffer;
}

String String_join(String string, ConstString joinString, char joinChar)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);
  STRING_CHECK_VALID(joinString);

  if (!String_isEmpty(string)) String_appendChar(string,joinChar);
  String_append(string,joinString);

  return string;
}

String String_joinCString(String string, const char *s, char joinChar)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (!String_isEmpty(string)) String_appendChar(string,joinChar);
  String_appendCString(string,s);

  return string;
}

String String_joinChar(String string, char ch, char joinChar)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (!String_isEmpty(string)) String_appendChar(string,joinChar);
  String_appendChar(string,ch);

  return string;
}

String String_joinBuffer(String string, const char *buffer, ulong bufferLength, char joinChar)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (!String_isEmpty(string)) String_appendChar(string,joinChar);
  String_appendBuffer(string,buffer,bufferLength);

  return string;
}

int String_compare(ConstString           string1,
                   ConstString           string2,
                   StringCompareFunction stringCompareFunction,
                   void                  *stringCompareUserData
                  )
{
  ulong n;
  ulong i;
  int   result;

  assert(string1 != NULL);
  assert(string2 != NULL);

  STRING_CHECK_VALID(string1);
  STRING_CHECK_VALID(string2);

  result = 0;
  n = MIN(string1->length,string2->length);
  i = 0L;
  if (stringCompareFunction != NULL)
  {
    while ((result == 0) && (i < n))
    {
      result = stringCompareFunction(string1->data[i],string2->data[i],stringCompareUserData);
      i++;
    }
  }
  else
  {
    while ((result == 0) && (i < n))
    {
      if      (string1->data[i] < string2->data[i]) result = -1;
      else if (string1->data[i] > string2->data[i]) result =  1;
      i++;
    }
  }
  if (result == 0)
  {
    if      (string1->length < string2->length) result = -1;
    else if (string1->length > string2->length) result =  1;
  }

  return result;
}

bool String_equals(ConstString string1, ConstString string2)
{
  bool equalFlag;

  if ((string1 != NULL) && (string2 != NULL))
  {
    STRING_CHECK_VALID(string1);
    STRING_CHECK_VALID(string2);

    equalFlag = String_equalsBuffer(string1,string2->data,string2->length);
  }
  else
  {
    equalFlag = ((string1 == NULL) && (string2 == NULL));
  }

  return equalFlag;
}

bool String_equalsCString(ConstString string, const char *s)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      equalFlag = String_equalsBuffer(string,s,strlen(s));
    }
    else
    {
      equalFlag = (string->length == 0L);
    }
  }
  else
  {
    equalFlag = (s == NULL);
  }

  return equalFlag;
}

bool String_equalsChar(ConstString string, char ch)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    equalFlag = ((string->length == 1) && (string->data[0] == ch));
  }
  else
  {
    equalFlag = FALSE;
  }

  return equalFlag;
}

bool String_equalsBuffer(ConstString string, const char *buffer, ulong bufferLength)
{
  bool  equalFlag;
  ulong i;

  assert(string != NULL);
  assert(buffer != NULL);

  if (string != NULL)
  {
    STRING_CHECK_VALID(string);

    if (string->length == bufferLength)
    {
      equalFlag = TRUE;
      i         = 0L;
      while (equalFlag && (i < string->length))
      {
        equalFlag = (string->data[i] == buffer[i]);
        i++;
      }
    }
    else
    {
      equalFlag = FALSE;
    }
  }
  else
  {
    equalFlag = (string == NULL) && (bufferLength == 0);
  }

  return equalFlag;
}

bool String_equalsIgnoreCase(ConstString string1, ConstString string2)
{
  bool equalFlag;

  if ((string1 != NULL) && (string2 != NULL))
  {
    STRING_CHECK_VALID(string1);
    STRING_CHECK_VALID(string2);

    equalFlag = String_equalsIgnoreCaseBuffer(string1,string2->data,string2->length);
  }
  else
  {
    equalFlag = ((string1 == NULL) && (string2 == NULL));
  }

  return equalFlag;
}

bool String_equalsIgnoreCaseCString(ConstString string, const char *s)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      equalFlag = String_equalsIgnoreCaseBuffer(string,s,strlen(s));
    }
    else
    {
      equalFlag = (string->length == 0L);
    }
  }
  else
  {
    equalFlag = (s == NULL);
  }

  return equalFlag;
}

bool String_equalsIgnoreCaseChar(ConstString string, char ch)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    equalFlag = ((string->length == 1) && (toupper(string->data[0]) == toupper(ch)));
  }
  else
  {
    equalFlag = FALSE;
  }

  return equalFlag;
}

bool String_equalsIgnoreCaseBuffer(ConstString string, const char *buffer, ulong bufferLength)
{
  bool  equalFlag;
  ulong i;

  assert(string != NULL);
  assert(buffer != NULL);

  if (string != NULL)
  {
    STRING_CHECK_VALID(string);

    if (string->length == bufferLength)
    {
      equalFlag = TRUE;
      i         = 0L;
      while (equalFlag && (i < string->length))
      {
        equalFlag = (toupper(string->data[i]) == toupper(buffer[i]));
        i++;
      }
    }
    else
    {
      equalFlag = FALSE;
    }
  }
  else
  {
    equalFlag = (string == NULL) && (bufferLength == 0);
  }

  return equalFlag;
}

bool String_subEquals(ConstString string1, ConstString string2, long index, ulong length)
{
  bool  equalFlag;

  assert(string1 != NULL);
  assert(string2 != NULL);

  STRING_CHECK_VALID(string1);
  STRING_CHECK_VALID(string2);

  if ((string1 != NULL) && (string2 != NULL))
  {
    equalFlag = String_subEqualsBuffer(string1,string2->data,string2->length,index,length);
  }
  else
  {
    equalFlag = ((string1 == NULL) && (string2 == NULL));
  }

  return equalFlag;
}

bool String_subEqualsCString(ConstString string, const char *s, long index, ulong length)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      equalFlag = String_subEqualsBuffer(string,s,strlen(s),index,length);
    }
    else
    {
      equalFlag = (string->length == 0);
    }
  }
  else
  {
    equalFlag = (s == NULL);
  }

  return equalFlag;
}

bool String_subEqualsChar(ConstString string, char ch, long index)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    equalFlag = ((index < (long)string->length) && (string->data[index] == ch));
  }
  else
  {
    equalFlag = FALSE;
  }

  return equalFlag;
}

bool String_subEqualsBuffer(ConstString string, const char *buffer, ulong bufferLength, long index, ulong length)
{
  long  i;
  bool  equalFlag;
  ulong j;

  assert(buffer != NULL);

  if (string != NULL)
  {
    STRING_CHECK_VALID(string);

    i = (index != STRING_END) ? index : (long)string->length-(long)length;
    if (   (i >= 0)
        && ((i+length) <= string->length)
        && (length <= bufferLength)
       )
    {
      equalFlag = TRUE;
      j         = 0L;
      while (equalFlag && (j < length))
      {
        equalFlag = (string->data[i+j] == buffer[j]);
        j++;
      }
    }
    else
    {
      equalFlag = FALSE;
    }
  }
  else
  {
    equalFlag = (bufferLength == 0L);
  }

  return equalFlag;
}

bool String_subEqualsIgnoreCase(ConstString string1, ConstString string2, long index, ulong length)
{
  bool  equalFlag;

  assert(string1 != NULL);
  assert(string2 != NULL);

  STRING_CHECK_VALID(string1);
  STRING_CHECK_VALID(string2);

  if ((string1 != NULL) && (string2 != NULL))
  {
    equalFlag = String_subEqualsIgnoreCaseBuffer(string1,string2->data,string2->length,index,length);
  }
  else
  {
    equalFlag = ((string1 == NULL) && (string2 == NULL));
  }

  return equalFlag;
}

bool String_subEqualsIgnoreCaseCString(ConstString string, const char *s, long index, ulong length)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      equalFlag = String_subEqualsIgnoreCaseBuffer(string,s,strlen(s),index,length);
    }
    else
    {
      equalFlag = (string->length == 0);
    }
  }
  else
  {
    equalFlag = (s == NULL);
  }

  return equalFlag;
}

bool String_subEqualsIgnoreCaseChar(ConstString string, char ch, long index)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    equalFlag = ((index < (long)string->length) && (toupper(string->data[index]) == toupper(ch)));
  }
  else
  {
    equalFlag = FALSE;
  }

  return equalFlag;
}

bool String_subEqualsIgnoreCaseBuffer(ConstString string, const char *buffer, ulong bufferLength, long index, ulong length)
{
  long  i;
  bool  equalFlag;
  ulong j;

  assert(buffer != NULL);

  if (string != NULL)
  {
    STRING_CHECK_VALID(string);

    i = (index != STRING_END) ? index : (long)string->length-(long)length;
    if (   (i >= 0)
        && ((i+length) <= string->length)
        && (length <= bufferLength)
       )
    {
      equalFlag = TRUE;
      j         = 0L;
      while (equalFlag && (j < length))
      {
        equalFlag = (toupper(string->data[i+j]) == toupper(buffer[j]));
        j++;
      }
    }
    else
    {
      equalFlag = FALSE;
    }
  }
  else
  {
    equalFlag = (bufferLength == 0L);
  }

  return equalFlag;
}

bool String_startsWith(ConstString string1, ConstString string2)
{
  bool equalFlag;

  STRING_CHECK_VALID(string1);
  STRING_CHECK_VALID(string2);

  if ((string1 != NULL) && (string2 != NULL))
  {
    equalFlag = String_startsWithBuffer(string1,string2->data,string2->length);
  }
  else
  {
    equalFlag = ((string1 == NULL) || (string1->length == 0L)) && ((string2 == NULL) || (string2->length == 0L));
  }

  return equalFlag;
}

bool String_startsWithCString(ConstString string, const char *s)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if ((string != NULL) && (s != NULL))
  {
    equalFlag = String_startsWithBuffer(string,s,(ulong)strlen(s));
  }
  else
  {
    equalFlag = ((string == NULL) && (s == NULL));
  }

  return equalFlag;
}

bool String_startsWithChar(ConstString string, char ch)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    equalFlag = ((string->length > 0L) && (string->data[0] == ch));
  }
  else
  {
    equalFlag = FALSE;
  }

  return equalFlag;
}

bool String_startsWithBuffer(ConstString string, const char *buffer, ulong bufferLength)
{
  bool  equalFlag;
  ulong i;

  assert(buffer != NULL);

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    if (string->length >= bufferLength)
    {
      equalFlag = TRUE;
      i         = 0L;
      while (equalFlag && (i < bufferLength))
      {
        equalFlag = (string->data[i] == buffer[i]);
        i++;
      }
    }
    else
    {
      equalFlag = FALSE;
    }
  }
  else
  {
    equalFlag = (bufferLength == 0L);
  }

  return equalFlag;
}

bool String_endsWith(ConstString string1, ConstString string2)
{
  bool equalFlag;

  STRING_CHECK_VALID(string1);
  STRING_CHECK_VALID(string2);

  if ((string1 != NULL) && (string2 != NULL))
  {
    equalFlag = String_endsWithBuffer(string1,string2->data,string2->length);
  }
  else
  {
    equalFlag = ((string1 == NULL) && (string2 == NULL));
  }

  return equalFlag;
}

bool String_endsWithCString(ConstString string, const char *s)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if ((string != NULL) && (s != NULL))
  {
    equalFlag = String_endsWithBuffer(string,s,(ulong)strlen(s));
  }
  else
  {
    equalFlag = ((string == NULL) && (s == NULL));
  }

  return equalFlag;
}

bool String_endsWithChar(ConstString string, char ch)
{
  bool equalFlag;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    equalFlag = ((string->length > 0L) && (string->data[string->length-1] == ch));
  }
  else
  {
    equalFlag = FALSE;
  }

  return equalFlag;
}

bool String_endsWithBuffer(ConstString string, const char *buffer, ulong bufferLength)
{
  bool  equalFlag;
  ulong j;
  ulong i;

  assert(buffer != NULL);

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    if (string->length >= bufferLength)
    {
      equalFlag = TRUE;
      j         = 0L;
      i         = string->length-bufferLength;
      while (equalFlag && (j < bufferLength))
      {
        equalFlag = (string->data[i] == buffer[j]);
        j++;
        i++;
      }
    }
    else
    {
      equalFlag = FALSE;
    }
  }
  else
  {
    equalFlag = (bufferLength == 0L);
  }

  return equalFlag;
}

long String_find(ConstString string, ulong index, ConstString findString)
{
  long  findIndex;
  long  i;
  ulong j;

  assert(string != NULL);
  assert(findString != NULL);

  STRING_CHECK_VALID(string);
  STRING_CHECK_VALID(findString);

  findIndex = -1;

  i = (index != STRING_BEGIN) ? index : 0L;
  while (((i+(long)findString->length) <= (long)string->length) && (findIndex < 0))
  {
    j = 0L;
    while ((j < findString->length) && (string->data[i+j] == findString->data[j]))
    {
      j++;
    }
    if (j >=  findString->length) findIndex = i;

    i++;
  }

  return findIndex;
}

long String_findCString(ConstString string, ulong index, const char *s)
{
  long  findIndex;
  ulong sLength;
  long  i;
  ulong j;

  assert(string != NULL);
  assert(s != NULL);

  STRING_CHECK_VALID(string);

  findIndex = -1L;

  sLength = (ulong)strlen(s);
  i = (index != STRING_BEGIN) ? index : 0L;
  while (((i+sLength) <= string->length) && (findIndex < 0))
  {
    j = 0L;
    while ((j < sLength) && (string->data[i+j] == s[j]))
    {
      j++;
    }
    if (j >= sLength) findIndex = i;

    i++;
  }

  return findIndex;
}

long String_findChar(ConstString string, ulong index, char ch)
{
  long i;

  assert(string != NULL);

  STRING_CHECK_VALID(string);

  i = (index != STRING_BEGIN) ? index : 0L;
  while ((i < (long)string->length) && (string->data[i] != ch))
  {
    i++;
  }

  return (i < (long)string->length) ? i : -1L;
}

long String_findLast(ConstString string, long index, ConstString findString)
{
  long  findIndex;
  long  i;
  ulong j;

  assert(string != NULL);
  assert(findString != NULL);

  STRING_CHECK_VALID(string);

  findIndex = -1;

  i = (index != STRING_END) ? index : (long)string->length-1;
  while ((i >= 0) && (findIndex < 0))
  {
    j = 0L;
    while ((j < findString->length) && (string->data[i+j] == findString->data[j]))
    {
      j++;
    }
    if (j >= findString->length) findIndex = i;

    i--;
  }

  return findIndex;
}

long String_findLastCString(ConstString string, long index, const char *s)
{
  long  findIndex;
  ulong sLength;
  long  i;
  ulong j;

  assert(string != NULL);
  assert(s != NULL);

  STRING_CHECK_VALID(string);

  findIndex = -1L;

  sLength = (ulong)strlen(s);
  i = (index != STRING_END) ? index : (long)string->length-1;
  while ((i >= 0) && (findIndex < 0))
  {
    j = 0L;
    while ((j < sLength) && (string->data[i+j] == s[j]))
    {
      j++;
    }
    if (j >=  sLength) findIndex = i;

    i--;
  }

  return findIndex;
}

long String_findLastChar(ConstString string, long index, char ch)
{
  long i;

  assert(string != NULL);

  STRING_CHECK_VALID(string);

  i = (index != STRING_END) ? index : (long)string->length-1;
  while ((i >= 0) && (string->data[i] != ch))
  {
    i--;
  }

  return (i >= 0) ? i : -1;
}

String String_iterate(                      String string,
                      StringIterateFunction stringIterateFunction,
                      void                  *stringIterateUserData
                     )
{
  ulong      j;
  const char *s;
  ulong      n;

  assert(stringIterateFunction != NULL);

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    j = 0L;
    while (j < string->length)
    {
      s = stringIterateFunction(string->data[j],stringIterateUserData);
      if (s != NULL)
      {
        n = strlen(s);
        ensureStringLength(string,string->length+n-1);
        memmove(&string->data[j+n],&string->data[j+1],string->length-(j+1));
        memmove(&string->data[j],s,n);
        string->data[string->length+n-1] = NUL;
        string->length += n-1;

        j += n;
      }
      else
      {
        j += 1;
      }
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_toLower(String string)
{
  ulong i;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    for (i = 0L; i < string->length; i++)
    {
      string->data[i] = tolower(string->data[i]);
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_toUpper(String string)
{
  ulong i;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    for (i = 0L; i < string->length; i++)
    {
      string->data[i] = toupper(string->data[i]);
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_trim(String string, const char *chars)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  String_trimBegin(string,chars);
  String_trimEnd(string,chars);

  return string;
}

String String_trimBegin(String string, const char *chars)
{
  ulong i,n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    i = 0L;
    while ((i < string->length) && (strchr(chars,string->data[i]) != NULL))
    {
      i++;
    }
    if (i > 0)
    {
      n = string->length-i;
      memmove(&string->data[0],&string->data[i],n);
      string->data[n] = NUL;
      string->length  = n;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_trimEnd(String string, const char *chars)
{
  ulong n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    n = string->length;
    while ((n > 0) && (strchr(chars,string->data[n-1]) != NULL))
    {
      n--;
    }
    string->data[n] = NUL;
    string->length  = n;

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_escape(String     string,
                     char       escapeChar,
                     const char *chars,
                     const char from[],
                     const char to[],
                     uint       count
                    )
{
  String s;
  ulong  i;
  uint   z;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    #ifdef NDEBUG
      s = allocTmpString();
    #else /* not NDEBUG */
      s = allocTmpString(__FILE__,__LINE__);
    #endif /* NDEBUG */
    for (i = 0L; i < string->length; i++)
    {
      if      (string->data[i] == escapeChar)
      {
        // escape character
        String_appendChar(s,escapeChar);
        String_appendChar(s,escapeChar);
      }
      else if ((chars != NULL) && (strchr(chars,string->data[i]) != NULL))
      {
        // escaped character
        String_appendChar(s,escapeChar);
        String_appendChar(s,string->data[i]);
      }
      else if ((from != NULL) && (to != NULL))
      {
        // check if mapped character
        z = 0;
        while ((z < count) && (string->data[i] != from[z]))
        {
          z++;
        }
        if (z < count)
        {
          // mapped character
          String_appendChar(s,escapeChar);
          String_appendChar(s,to[z]);
        }
        else
        {
          // not-mapped character
          String_appendChar(s,string->data[i]);
        }
      }
      else
      {
        // not-escaped character
        String_appendChar(s,string->data[i]);
      }
    }
    assignTmpString(string,s);
  }

  return string;
}

String String_unescape(String     string,
                       char       escapeChar,
                       const char from[],
                       const char to[],
                       uint       count
                      )
{
  String s;
  ulong  i;
  uint   z;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    #ifdef NDEBUG
      s = allocTmpString();
    #else /* not NDEBUG */
      s = allocTmpString(__FILE__,__LINE__);
    #endif /* NDEBUG */
    i = 0L;
    while (i < string->length)
    {
      if (   (string->data[i] == escapeChar)
          && ((i+1) < string->length)
         )
      {
        i++;

        if ((from != NULL) && (to != NULL))
        {
          // check if mapped character
          z = 0;
          while ((z < count) && (string->data[i] != from[z]))
          {
            z++;
          }
          if (z < count)
          {
            // mapped character
            String_appendChar(s,to[z]);
          }
          else
          {
            // not-mapped character
            String_appendChar(s,string->data[i]);
          }
        }
        else
        {
          // not-escaped character
          String_appendChar(s,string->data[i]);
        }
      }
      else
      {
        // not-escaped character
        String_appendChar(s,string->data[i]);
      }
      i++;
    }
    assignTmpString(string,s);
  }

  return string;
}

String String_quote(String string, char quoteChar)
{
  String s;
  ulong  i;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    #ifdef NDEBUG
      s = allocTmpString();
    #else /* not NDEBUG */
      s = allocTmpString(__FILE__,__LINE__);
    #endif /* NDEBUG */
    String_appendChar(s,quoteChar);
    for (i = 0L; i < string->length; i++)
    {
      if (string->data[i] == quoteChar)
      {
        String_appendChar(s,STRING_ESCAPE_CHARACTER);
      }
      String_appendChar(s,string->data[i]);
    }
    String_appendChar(s,quoteChar);
    assignTmpString(string,s);
  }

  return string;
}

String String_unquote(String string, const char *quoteChars)
{
  const char *t0,*t1;
  String     s;
  ulong      i;

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (string->length > 0)
    {
      t0 = strchr(quoteChars,string->data[0]);
      t1 = strchr(quoteChars,string->data[string->length-1]);
      if ((t0 != NULL) && (t1 != NULL) && ((*t0) == (*t1)))
      {
        #ifdef NDEBUG
          s = allocTmpString();
        #else /* not NDEBUG */
          s = allocTmpString(__FILE__,__LINE__);
        #endif /* NDEBUG */
        i = 1;
        while (i < (string->length-1))
        {
          if (   (string->data[i] == STRING_ESCAPE_CHARACTER)
              && ((i+1) < string->length-1)
              && (string->data[i+1] == (*t0))
             )
          {
            // escaped quote character
            i++;
            String_appendChar(s,string->data[i]);
          }
          else
          {
            String_appendChar(s,string->data[i]);
          }
          i++;
        }
        assignTmpString(string,s);
      }
    }
  }

  return string;
}

String String_padRight(String string, ulong length, char ch)
{
  ulong n;

  assert(string != NULL);

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    if (string->length < length)
    {
      n = length-string->length;
      ensureStringLength(string,length);
      memset(&string->data[string->length],ch,n);
      string->data[length] = NUL;
      string->length       = length;

      STRING_UPDATE_VALID(string);
    }
  }

  return string;
}

String String_padLeft(String string, ulong length, char ch)
{
  ulong n;

  assert(string != NULL);

  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    if (string->length < length)
    {
      n = length-string->length;
      ensureStringLength(string,length);
      memmove(&string->data[n],&string->data[0],string->length);
      memset(&string->data[0],ch,n);
      string->data[length] = NUL;
      string->length       = length;

      STRING_UPDATE_VALID(string);
    }
  }

  return string;
}

String String_fillChar(String string, ulong length, char ch)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_ASSIGNABLE(string);

  if (string != NULL)
  {
    ensureStringLength(string,length);
    memset(&string->data[0],ch,length);
    string->data[length] = NUL;
    string->length       = length;

    STRING_UPDATE_VALID(string);
  }

  return string;
}

void String_initTokenizer(StringTokenizer *stringTokenizer,
                          ConstString     string,
                          ulong           index,
                          const char      *separatorChars,
                          const char      *stringQuotes,
                          bool            skipEmptyTokens
                         )
{
  assert(stringTokenizer != NULL);
  assert(string != NULL);

  STRING_CHECK_VALID(string);

  stringTokenizer->data            = string->data;
  stringTokenizer->length          = string->length;
  stringTokenizer->index           = index;
  stringTokenizer->separatorChars  = separatorChars;
  stringTokenizer->stringQuotes    = stringQuotes;
  stringTokenizer->skipEmptyTokens = skipEmptyTokens;
  #ifdef NDEBUG
    stringTokenizer->token         = String_new();
  #else /* not DEBUG */
    stringTokenizer->token         = __String_new(__FILE__,__LINE__);
  #endif /* NDEBUG */
}

void String_initTokenizerCString(StringTokenizer *stringTokenizer,
                                 const char      *s,
                                 const char      *separatorChars,
                                 const char      *stringQuotes,
                                 bool            skipEmptyTokens
                                )
{
  assert(stringTokenizer != NULL);
  assert(s != NULL);

  stringTokenizer->data            = s;
  stringTokenizer->length          = strlen(s);
  stringTokenizer->index           = 0;
  stringTokenizer->separatorChars  = separatorChars;
  stringTokenizer->stringQuotes    = stringQuotes;
  stringTokenizer->skipEmptyTokens = skipEmptyTokens;
  #ifdef NDEBUG
    stringTokenizer->token         = String_new();
  #else /* not DEBUG */
    stringTokenizer->token         = __String_new(__FILE__,__LINE__);
  #endif /* NDEBUG */
}

void String_doneTokenizer(StringTokenizer *stringTokenizer)
{
  assert(stringTokenizer != NULL);
  assert(stringTokenizer->data != NULL);

  String_delete(stringTokenizer->token);
}

bool String_getNextToken(StringTokenizer *stringTokenizer,
                         ConstString     *token,
                         long            *tokenIndex
                        )
{
  const char *s;

  assert(stringTokenizer != NULL);

  // check index
  if (stringTokenizer->index >= (long)stringTokenizer->length)
  {
    return FALSE;
  }

  if (stringTokenizer->skipEmptyTokens)
  {
    // skip separator chars
    while (   (stringTokenizer->index < (long)stringTokenizer->length)
           && (strchr(stringTokenizer->separatorChars,stringTokenizer->data[stringTokenizer->index]) != NULL)
          )
    {
      stringTokenizer->index++;
    }
    if (stringTokenizer->index >= (long)stringTokenizer->length) return FALSE;
  }

  // get token
  if (tokenIndex != NULL) (*tokenIndex) = stringTokenizer->index;
  String_clear(stringTokenizer->token);
  if (stringTokenizer->stringQuotes != NULL)
  {
    while (   (stringTokenizer->index < (long)stringTokenizer->length)
           && (strchr(stringTokenizer->separatorChars,stringTokenizer->data[stringTokenizer->index]) == NULL)
          )
    {
      s = strchr(stringTokenizer->stringQuotes,stringTokenizer->data[stringTokenizer->index]);
      if (s != NULL)
      {
        stringTokenizer->index++;
        while (   (stringTokenizer->index < (long)stringTokenizer->length)
               && (stringTokenizer->data[stringTokenizer->index] != (*s))
              )
        {
          if (   ((stringTokenizer->index+1) < (long)stringTokenizer->length)
              && (   (stringTokenizer->data[stringTokenizer->index] == STRING_ESCAPE_CHARACTER)
                  || (stringTokenizer->data[stringTokenizer->index] == (*s))
                 )
             )
          {
            String_appendChar(stringTokenizer->token,stringTokenizer->data[stringTokenizer->index+1]);
            stringTokenizer->index += 2;
          }
          else
          {
            String_appendChar(stringTokenizer->token,stringTokenizer->data[stringTokenizer->index]);
            stringTokenizer->index++;
          }
        }
        if (stringTokenizer->index < (long)stringTokenizer->length) stringTokenizer->index++;
      }
      else
      {
        String_appendChar(stringTokenizer->token,stringTokenizer->data[stringTokenizer->index]);
        stringTokenizer->index++;
      }
    }
  }
  else
  {
    while (   (stringTokenizer->index < (long)stringTokenizer->length)
           && (strchr(stringTokenizer->separatorChars,stringTokenizer->data[stringTokenizer->index]) == NULL)
          )
    {
      String_appendChar(stringTokenizer->token,stringTokenizer->data[stringTokenizer->index]);
      stringTokenizer->index++;
    }
  }
  if (token != NULL) (*token) = stringTokenizer->token;

  // skip token separator
  if (   (stringTokenizer->index < (long)stringTokenizer->length)
      && (strchr(stringTokenizer->separatorChars,stringTokenizer->data[stringTokenizer->index]) != NULL)
     )
  {
    stringTokenizer->index++;
  }

  return TRUE;
}

bool String_scan(ConstString string, ulong index, const char *format, ...)
{
  va_list arguments;
  long    nextIndex;
  bool    result;

  assert(string != NULL);
  assert((index == STRING_BEGIN) || (index == STRING_END) || (index < string->length));
  assert(format != NULL);

  STRING_CHECK_VALID(string);

  va_start(arguments,format);
  result = parseString(string->data,string->length,index,format,arguments,NULL,&nextIndex);
  UNUSED_VARIABLE(nextIndex);
  va_end(arguments);

  return result;
}

bool String_scanCString(const char *s, const char *format, ...)
{
  va_list arguments;
  long    nextIndex;
  bool    result;

  assert(s != NULL);
  assert(format != NULL);

  va_start(arguments,format);
  result = parseString(s,strlen(s),0,format,arguments,NULL,&nextIndex);
  UNUSED_VARIABLE(nextIndex);
  va_end(arguments);

  return result;
}

bool String_parse(ConstString string, ulong index, const char *format, long *nextIndex, ...)
{
  va_list arguments;
  bool    result;

  assert(string != NULL);
  assert((index == STRING_BEGIN) || (index == STRING_END) || (index < string->length));
  assert(format != NULL);

  STRING_CHECK_VALID(string);

  va_start(arguments,nextIndex);
  result = parseString(string->data,string->length,index,format,arguments,STRING_QUOTES,nextIndex);
  va_end(arguments);

  return result;
}

bool String_parseCString(const char *s, const char *format, long *nextIndex, ...)
{
  va_list arguments;
  bool    result;

  assert(s != NULL);
  assert(format != NULL);

  va_start(arguments,nextIndex);
  result = parseString(s,strlen(s),0,format,arguments,STRING_QUOTES,nextIndex);
  va_end(arguments);

  return result;
}

bool String_match(ConstString string, ulong index, ConstString pattern, long *nextIndex, String matchedString, ...)
{
  va_list arguments;
  bool    matchFlag;

  if (matchedString != NULL)
  {
    va_start(arguments,matchedString);
    matchFlag = vmatchString(string,index,String_cString(pattern),nextIndex,matchedString,arguments);
    va_end(arguments);
  }
  else
  {
    matchFlag = matchString(string,index,String_cString(pattern),nextIndex);
  }

  return matchFlag;
}

bool String_matchCString(ConstString string, ulong index, const char *pattern, long *nextIndex, String matchedString, ...)
{
  va_list arguments;
  bool    matchFlag;

  if (matchedString != NULL)
  {
    va_start(arguments,matchedString);
    matchFlag = vmatchString(string,index,pattern,nextIndex,matchedString,arguments);
    va_end(arguments);
  }
  else
  {
    matchFlag = matchString(string,index,pattern,nextIndex);
  }

  return matchFlag;
}

int String_toInteger(ConstString convertString, ulong index, long *nextIndex, const StringUnit stringUnits[], uint stringUnitCount)
{
  double d;
  int    n;
  char   *nextData;

  assert(convertString != NULL);

  STRING_CHECK_VALID(convertString);

  if (index < convertString->length)
  {
    d = strtod(&convertString->data[index],&nextData);
    if ((ulong)(nextData-convertString->data) < convertString->length)
    {
      if (stringUnitCount > 0)
      {
        n = (int)(d*getUnitFactor(stringUnits,stringUnitCount,convertString->data,nextData,nextIndex));
      }
      else
      {
        n = 0;
        if (nextIndex != NULL) (*nextIndex) = (ulong)(nextData-convertString->data);
      }
    }
    else
    {
      n = (int)d;
      if (nextIndex != NULL) (*nextIndex) = STRING_END;
    }
  }
  else
  {
    n = 0;
    if (nextIndex != NULL) (*nextIndex) = index;
  }

  return n;
}

int64 String_toInteger64(ConstString convertString, ulong index, long *nextIndex, const StringUnit stringUnits[], uint stringUnitCount)
{
  double d;
  int64  n;
  char   *nextData;

  assert(convertString != NULL);

  if (index < convertString->length)
  {
    d = strtod(&convertString->data[index],&nextData);
    if ((ulong)(nextData-convertString->data) < convertString->length)
    {
      if (stringUnitCount > 0)
      {
        n = (int64)(d*(int64)getUnitFactor(stringUnits,stringUnitCount,convertString->data,nextData,nextIndex));
      }
      else
      {
        n = 0LL;
        if (nextIndex != NULL) (*nextIndex) = (ulong)(nextData-convertString->data);
      }
    }
    else
    {
      n = (uint64)d;
      if (nextIndex != NULL) (*nextIndex) = STRING_END;
    }
  }
  else
  {
    n = 0LL;
    if (nextIndex != NULL) (*nextIndex) = index;
  }

  return n;
}

double String_toDouble(ConstString convertString, ulong index, long *nextIndex, const StringUnit stringUnits[], uint stringUnitCount)
{
  double n;
  char   *nextData;

  assert(convertString != NULL);

  STRING_CHECK_VALID(convertString);

  if (index < convertString->length)
  {
    n = strtod(&convertString->data[index],&nextData);
    if ((ulong)(nextData-convertString->data) < convertString->length)
    {
      if (stringUnitCount > 0)
      {
        n = n*(double)(long)getUnitFactor(stringUnits,stringUnitCount,convertString->data,nextData,nextIndex);
      }
      else
      {
        n = 0;
        if (nextIndex != NULL) (*nextIndex) = (ulong)(nextData-convertString->data);
      }
    }
    else
    {
      if (nextIndex != NULL) (*nextIndex) = STRING_END;
    }
  }
  else
  {
    n = 0.0;
    if (nextIndex != NULL) (*nextIndex) = index;
  }

  return n;
}

bool String_toBoolean(ConstString convertString, ulong index, long *nextIndex, const char *trueStrings[], uint trueStringCount, const char *falseStrings[], uint falseStringCount)
{
  bool       n;
  bool       foundFlag;
  const char **strings;
  uint       stringCount;
  uint       z;

  assert(convertString != NULL);

  STRING_CHECK_VALID(convertString);

  n = FALSE;

  if (index < convertString->length)
  {
    foundFlag = FALSE;
    if (!foundFlag)
    {
      if (trueStrings != NULL)
      {
        strings     = trueStrings;
        stringCount = trueStringCount;
      }
      else
      {
        strings     = DEFAULT_TRUE_STRINGS;
        stringCount = SIZE_OF_ARRAY(DEFAULT_TRUE_STRINGS);
      }
      z = 0;
      while (!foundFlag && (z < stringCount))
      {
        if (strcmp(&convertString->data[index],strings[z]) == 0)
        {
          n = TRUE;
          foundFlag = TRUE;
        }
        z++;
      }
    }
    if (!foundFlag)
    {
      if (falseStrings != NULL)
      {
        strings     = falseStrings;
        stringCount = falseStringCount;
      }
      else
      {
        strings     = DEFAULT_FALSE_STRINGS;
        stringCount = SIZE_OF_ARRAY(DEFAULT_FALSE_STRINGS);
      }
      z = 0;
      while (!foundFlag && (z < stringCount))
      {
        if (strcmp(&convertString->data[index],strings[z]) == 0)
        {
          n = FALSE;
          foundFlag = TRUE;
        }
        z++;
      }
    }
    if (!foundFlag)
    {
      if (nextIndex != NULL) (*nextIndex) = index;
    }
  }
  else
  {
    n = 0;
    if (nextIndex != NULL) (*nextIndex) = index;
  }

  return n;
}

String String_toString(String string, ConstString convertString, ulong index, long *nextIndex, const char *stringQuotes)
{
  char *stringQuote;

  if (index < convertString->length)
  {
    while ((index < convertString->length) && !isspace(convertString->data[index]))
    {
      stringQuote = (stringQuotes != NULL) ? strchr(stringQuotes,convertString->data[index]) : NULL;
      if (stringQuote != NULL)
      {
        do
        {
          // skip string-char
          index++;
          // get string
          while ((index < convertString->length) && (convertString->data[index] != (*stringQuote)))
          {
            if (convertString->data[index] == STRING_ESCAPE_CHARACTER)
            {
              if (index < convertString->length)
              {
                String_appendChar(string,convertString->data[index]);
                index++;
              }
            }
            else
            {
              String_appendChar(string,convertString->data[index]);
              index++;
            }
          }
          // skip string-char
          index++;
          // next string char
          stringQuote = ((stringQuotes != NULL) && (index < convertString->length)) ? strchr(stringQuotes,convertString->data[index]) : NULL;
        }
        while (stringQuote != NULL);
      }
      else
      {
        String_appendChar(string,convertString->data[index]);
        index++;
      }
    }
    if (nextIndex != NULL) (*nextIndex) = index;
  }
  else
  {
    if (nextIndex != NULL) (*nextIndex) = index;
  }

  return string;
}

char* String_toCString(ConstString string)
{
  char *cString;

  assert(string != NULL);

  STRING_CHECK_VALID(string);

  cString = (char*)malloc(string->length+1);
  if (cString == NULL)
  {
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return NULL;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }

  memcpy(cString,string->data,string->length);
  cString[string->length] = NUL;

  return cString;
}

#if 0
//TODO
String Misc_toUtf8(String string, ConstString fromString)
{
  return Misc_toUtf8CString(string,String_cString(fromString),String_length(fromString));
}

String Misc_toUtf8(String string, const char *fromString, uint fromStringLength)
{
  String_clear(string);

      ucnv_convertEx(uConverterTo,
                     uConverterFrom,
                     &to,toBuffer+256,
                     &from,from+strlen(dir->d_name),
                     NULL,NULL,NULL,NULL,
                     TRUE,TRUE,
                     &uError
                    );
      assert(U_SUCCESS(uError));


  return string;
}

//  uConverterFrom = ucnv_open("ISO-8859-1", &uError);
//  uConverterFrom = ucnv_open("windows-1251", &uError);
//  uConverterFrom = ucnv_open("windows-1252", &uError);
  uConverterFrom = ucnv_open(NULL, &uError);
//  uConverterFrom = ucnv_open("UTF-8", &uError);
  assert(U_SUCCESS(uError));

//  uConvertTo = ucnv_open("ISO-8859-1", &uError);
//  uConvertTo = ucnv_open("windows-1251", &uError);
  uConverterTo = ucnv_open("UTF-8", &uError);
  assert(U_SUCCESS(uError));

  fromName = ucnv_getName(uConverterFrom, &uError);
  assert(U_SUCCESS(uError));
}
#endif

#ifndef NDEBUG

void String_debugDone(void)
{
  #ifdef TRACE_STRING_ALLOCATIONS
    pthread_once(&debugStringInitFlag,debugStringInit);

    String_debugCheck();

    pthread_mutex_lock(&debugStringLock);
    {
      debugMaxStringNextWarningCount = 0LL;
      List_done(&debugStringFreeList,NULL,NULL);
      List_done(&debugStringAllocList,NULL,NULL);
    }
    pthread_mutex_unlock(&debugStringLock);
  #endif /* TRACE_STRING_ALLOCATIONS */
}

void String_debugCheckValid(const char *__fileName__, ulong __lineNb__, ConstString string)
{
  #ifdef TRACE_STRING_ALLOCATIONS
    DebugStringNode *debugStringNode;
  #endif /* TRACE_STRING_ALLOCATIONS */

  if ((string != NULL) && (string != STRING_EMPTY))
  {
    ulong checkSum;

    checkSum = STRING_CHECKSUM(string->length,string->maxLength,string->data);
    if (checkSum != string->checkSum)
    {
      #ifdef TRACE_STRING_ALLOCATIONS
        if (STRING_IS_DYNAMIC(string))
        {
          pthread_once(&debugStringInitFlag,debugStringInit);

          pthread_mutex_lock(&debugStringLock);
          {
            debugStringNode = debugFindString(&debugStringAllocList,string);
            if (debugStringNode != NULL)
            {
              #ifdef HAVE_BACKTRACE
                debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
              #endif /* HAVE_BACKTRACE */
              HALT_INTERNAL_ERROR_AT(__fileName__,
                                     __lineNb__,
                                     "Invalid checksum 0x%08lx in string %p, length %lu (max. %lu) allocated at %s, %lu (expected 0x%08lx)!",
                                     string->checkSum,
                                     string,
                                     string->length,
                                     (ulong)string->maxLength,
                                     debugStringNode->allocFileName,
                                     debugStringNode->allocLineNb,
                                     checkSum
                                    );
            }
            else
            {
              debugStringNode = debugFindString(&debugStringFreeList,string);
              if (debugStringNode != NULL)
              {
                fprintf(stderr,"DEBUG WARNING: string %p at %s, %lu was already freed at %s, %lu!\n",
                        string,
                        __fileName__,
                        __lineNb__,
                        debugStringNode->deleteFileName,
                        debugStringNode->deleteLineNb
                       );
              }
              else
              {
                fprintf(stderr,"DEBUG WARNING: string %p is not allocated and not known at %s, %lu!\n",
                        string,
                        __fileName__,
                        __lineNb__
                       );
              }
              #ifdef HAVE_BACKTRACE
                debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
              #endif /* HAVE_BACKTRACE */
              HALT_INTERNAL_ERROR_AT(__fileName__,
                                     __lineNb__,
                                     "Invalid checksum 0x%08lx in unknown string %p, length %lu (max. %lu) (expected 0x%08lx)!",
                                     string->checkSum,
                                     string,
                                     string->length,
                                     (ulong)string->maxLength,
                                     checkSum
                                    );
            }
          }
          pthread_mutex_unlock(&debugStringLock);
        }
        else
        {
          #ifdef HAVE_BACKTRACE
            debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
          #endif /* HAVE_BACKTRACE */
          HALT_INTERNAL_ERROR_AT(__fileName__,
                                 __lineNb__,
                                 "Invalid checksum 0x%08lx in static string %p, length %lu (max. %lu) (expected 0x%08lx)!",
                                 string->checkSum,
                                 string,
                                 string->length,
                                 (ulong)string->maxLength,
                                 checkSum
                                );
        }
      #else /* not TRACE_STRING_ALLOCATIONS */
        #ifdef HAVE_BACKTRACE
          debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
        #endif /* HAVE_BACKTRACE */
        HALT_INTERNAL_ERROR_AT(__fileName__,
                               __lineNb__,
                               "Invalid checksum 0x%08lx in static string %p, length %lu (max. %lu) (expected 0x%08lx)!",
                               string->checkSum,
                               string,
                               string->length,
                               (ulong)string->maxLength,
                               checkSum
                              );
      #endif /* TRACE_STRING_ALLOCATIONS */
    }

    #ifdef TRACE_STRING_ALLOCATIONS
      if (STRING_IS_DYNAMIC(string))
      {
        pthread_once(&debugStringInitFlag,debugStringInit);

        pthread_mutex_lock(&debugStringLock);
        {
          debugStringNode = debugFindString(&debugStringAllocList,string);
          if (debugStringNode == NULL)
          {
            debugStringNode = debugFindString(&debugStringFreeList,string);

            #ifdef HAVE_BACKTRACE
              debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
            #endif /* HAVE_BACKTRACE */
            if (debugStringNode != NULL)
            {
              HALT_INTERNAL_ERROR_AT(__fileName__,
                                     __lineNb__,
                                     "String %p allocated at %s, %lu is already freed at %s, %lu!",
                                     string,
                                     debugStringNode->allocFileName,
                                     debugStringNode->allocLineNb,
                                     debugStringNode->deleteFileName,
                                     debugStringNode->deleteLineNb
                                    );
            }
            else
            {
              HALT_INTERNAL_ERROR_AT(__fileName__,
                                     __lineNb__,
                                     "String %p is not allocated and not known!",
                                     string
                                    );
            }
          }
        }
        pthread_mutex_unlock(&debugStringLock);
      }
    #endif /* TRACE_STRING_ALLOCATIONS */
  }
}

void String_debugDumpInfo(FILE                   *handle,
                          StringDumpInfoFunction stringDumpInfoFunction,
                          void                   *stringDumpInfoUserData,
                          uint                   stringDumpInfoTypes
                         )
{
  typedef struct StringHistogramNode
  {
    LIST_NODE_HEADER(struct StringHistogramNode);

    const DebugStringNode *debugStringNode;
    uint                  count;

  } StringHistogramNode;
  typedef struct
  {
    LIST_HEADER(StringHistogramNode);
  } StringHistogramList;

  /***********************************************************************\
  * Name   : compareStringHistogramNodes
  * Purpose: compare string histogram nodes
  * Input  : node1,node2 - string histogram nodes to compare
  * Output : -
  * Return : -1 iff node1->count > node2->count
  *           1 iff node1->count < node2->count
  *           0 iff node1->count == node2->count
  * Notes  : -
  \***********************************************************************/

  auto int compareStringHistogramNodes(const StringHistogramNode *node1, const StringHistogramNode *node2, void *userData);
  int compareStringHistogramNodes(const StringHistogramNode *node1, const StringHistogramNode *node2, void *userData)
  {
    assert(node1 != NULL);
    assert(node2 != NULL);

    UNUSED_VARIABLE(userData);

    if      (node1->count > node2->count) return -1;
    else if (node1->count < node2->count) return  1;
    else                                  return  0;
  }

  #ifdef TRACE_STRING_ALLOCATIONS
    ulong               n;
    ulong               count;
    DebugStringNode     *debugStringNode;
    StringHistogramList stringHistogramList;
    StringHistogramNode *stringHistogramNode;

    pthread_once(&debugStringInitFlag,debugStringInit);

    pthread_mutex_lock(&debugStringLock);
    {
      // init variables
      List_init(&stringHistogramList);
      n     = 0L;
      count = 0L;

      // collect histogram data
      if (IS_SET(stringDumpInfoTypes,DUMP_INFO_TYPE_HISTOGRAM))
      {
        LIST_ITERATE(&debugStringAllocList,debugStringNode)
        {
          stringHistogramNode = LIST_FIND(&stringHistogramList,
                                          stringHistogramNode,
                                             (stringHistogramNode->debugStringNode->allocFileName == debugStringNode->allocFileName)
                                          && (stringHistogramNode->debugStringNode->allocLineNb   == debugStringNode->allocLineNb)
                                         );
          if (stringHistogramNode == NULL)
          {
            stringHistogramNode = LIST_NEW_NODE(StringHistogramNode);
            if (stringHistogramNode == NULL)
            {
              HALT_INSUFFICIENT_MEMORY();
            }
            stringHistogramNode->debugStringNode = debugStringNode;
            stringHistogramNode->count           = 0;
            List_append(&stringHistogramList,stringHistogramNode);
          }

          stringHistogramNode->count++;
        }

        List_sort(&stringHistogramList,(ListNodeCompareFunction)CALLBACK(compareStringHistogramNodes,NULL));
      }

      // get count
      if (IS_SET(stringDumpInfoTypes,DUMP_INFO_TYPE_ALLOCATED))
      {
        count += List_count(&debugStringAllocList);
      }
      if (IS_SET(stringDumpInfoTypes,DUMP_INFO_TYPE_HISTOGRAM))
      {
        count += List_count(&stringHistogramList);
      }

      // dump allocations
      if (IS_SET(stringDumpInfoTypes,DUMP_INFO_TYPE_ALLOCATED))
      {
        LIST_ITERATE(&debugStringAllocList,debugStringNode)
        {
          fprintf(handle,"DEBUG: string %p '%s' allocated at %s, line %lu\n",
                  debugStringNode->string,
                  debugStringNode->string->data,
                  debugStringNode->allocFileName,
                  debugStringNode->allocLineNb
                 );
          #ifdef HAVE_BACKTRACE
            fprintf(handle,"  allocated at\n");
            debugDumpStackTrace(handle,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugStringNode->stackTrace,debugStringNode->stackTraceSize,0);
          #endif /* HAVE_BACKTRACE */

          if (stringDumpInfoFunction != NULL)
          {
            if (!stringDumpInfoFunction(debugStringNode->string,
                                        debugStringNode->allocFileName,
                                        debugStringNode->allocLineNb,
                                        n,
                                        count,
                                        stringDumpInfoUserData
                                       )
               )
            {
              break;
            }
          }

          n++;
        }
      }

      // dump histogram
      if (IS_SET(stringDumpInfoTypes,DUMP_INFO_TYPE_HISTOGRAM))
      {
        LIST_ITERATE(&stringHistogramList,stringHistogramNode)
        {
          fprintf(handle,"DEBUG: string allocated %u times at %s, line %lu\n",
                  stringHistogramNode->count,
                  stringHistogramNode->debugStringNode->allocFileName,
                  stringHistogramNode->debugStringNode->allocLineNb
                 );
          #ifdef HAVE_BACKTRACE
            fprintf(handle,"  allocated at least at\n");
            debugDumpStackTrace(handle,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,(const void**)stringHistogramNode->debugStringNode->stackTrace,stringHistogramNode->debugStringNode->stackTraceSize,0);
          #endif /* HAVE_BACKTRACE */

          if (stringDumpInfoFunction != NULL)
          {
            if (!stringDumpInfoFunction(stringHistogramNode->debugStringNode->string,
                                        stringHistogramNode->debugStringNode->allocFileName,
                                        stringHistogramNode->debugStringNode->allocLineNb,
                                        n,
                                        count,
                                        stringDumpInfoUserData
                                       )
               )
            {
              break;
            }
          }

          n++;
        }
      }

      // free resources
      if (IS_SET(stringDumpInfoTypes,DUMP_INFO_TYPE_HISTOGRAM))
      {
        List_done(&stringHistogramList,CALLBACK(NULL,NULL));
      }
    }
    pthread_mutex_unlock(&debugStringLock);
  #else /* not TRACE_STRING_ALLOCATIONS */
    UNUSED_VARIABLE(handle);
    UNUSED_VARIABLE(stringDumpInfoFunction);
    UNUSED_VARIABLE(stringDumpInfoUserData);
    UNUSED_VARIABLE(stringDumpInfoTypes);
  #endif /* TRACE_STRING_ALLOCATIONS */
}

void String_debugPrintInfo(StringDumpInfoFunction stringDumpInfoFunction,
                           void                   *stringDumpInfoUserData,
                           uint                   stringDumpInfoTypes
                          )
{
  String_debugDumpInfo(stderr,stringDumpInfoFunction,stringDumpInfoUserData,stringDumpInfoTypes);
}

void String_debugPrintStatistics(void)
{
  #ifdef TRACE_STRING_ALLOCATIONS
    pthread_once(&debugStringInitFlag,debugStringInit);

    pthread_mutex_lock(&debugStringLock);
    {
      fprintf(stderr,"DEBUG: %lu string(s) allocated, total %lu bytes\n",
              List_count(&debugStringAllocList),
              debugStringAllocList.memorySize
             );
      fprintf(stderr,"DEBUG: %lu string(s) in free list, total %lu bytes\n",
              List_count(&debugStringFreeList),
              debugStringFreeList.memorySize
             );
    }
    pthread_mutex_unlock(&debugStringLock);
  #endif /* TRACE_STRING_ALLOCATIONS */
}

void String_debugCheck()
{
  pthread_once(&debugStringInitFlag,debugStringInit);

  String_debugPrintInfo(CALLBACK_NULL,DUMP_INFO_TYPE_ALLOCATED);
  String_debugPrintStatistics();

  #ifdef TRACE_STRING_ALLOCATIONS
    pthread_mutex_lock(&debugStringLock);
    {
      if (!List_isEmpty(&debugStringAllocList))
      {
        HALT_INTERNAL_ERROR_LOST_RESOURCE();
      }
    }
    pthread_mutex_unlock(&debugStringLock);
  #endif /* TRACE_STRING_ALLOCATIONS */
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
