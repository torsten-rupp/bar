/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: dynamic string functions
* Systems: all
*
\***********************************************************************/

#define __STRINGS_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(HAVE_PCRE)
  #include <pcreposix.h>
#elif defined(HAVE_REGEX_H)
  #include <regex.h>
#else
  #error No regular expression library available!
#endif /* HAVE_PCRE || HAVE_REGEX_H */
#include <errno.h>
#ifdef HAVE_BACKTRACE
  #include <execinfo.h>
#endif
#include <assert.h>

#include "global.h"
#ifndef NDEBUG
  #include <pthread.h>
  #include "lists.h"
#endif /* not NDEBUG */

#include "strings.h"

/****************** Conditional compilation switches *******************/
#define HALT_ON_INSUFFICIENT_MEMORY
#define _FILL_MEMORY

#ifndef NDEBUG
  // max. string check: print warning and delay if to many strings allocated
  #define MAX_STRINGS_CHECK
  #define WARN_MAX_STRINGS       2000
  #define WARN_MAX_STRINGS_DELTA  500
#endif /* not NDEBUG */

/***************************** Constants *******************************/
#define STRING_START_LENGTH 64   // string start length
#define STRING_DELTA_LENGTH 32   // string delta increasing/decreasing

const char STRING_ESCAPE_CHARACTERS[] = {'\0','\007','\b','\t','\n','\v','\f','\r','\033'};
const char STRING_ESCAPE_MAP[]        = {'0', 'a',   'b', 't', 'n', 'v', 'f', 'r', 'e'   };

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
      void const *stackTrace[16];
      int        stackTraceSize;
    #endif /* HAVE_BACKTRACE */

    const char      *deleteFileName;
    ulong           deleteLineNb;
    #ifdef HAVE_BACKTRACE
      void const *deleteStackTrace[16];
      int        deleteStackTraceSize;
    #endif /* HAVE_BACKTRACE */

    struct __String *string;
  } DebugStringNode;

  typedef struct
  {
    LIST_HEADER(DebugStringNode);
    ulong allocatedMemory;
  } DebugStringList;
#endif /* not NDEBUG */

/***************************** Variables *******************************/
#ifndef NDEBUG
  LOCAL pthread_once_t  debugStringInitFlag = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutex_t debugStringLock;
  LOCAL DebugStringList debugStringAllocList;
  LOCAL DebugStringList debugStringFreeList;
  #ifdef MAX_STRINGS_CHECK
    LOCAL ulong debugMaxStringNextWarningCount;
  #endif /* MAX_STRINGS_CHECK */
#endif /* not NDEBUG */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : debugStringInit
* Purpose: initialize debug functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
LOCAL void debugStringInit(void)
{
  pthread_mutex_init(&debugStringLock,NULL);
  List_init(&debugStringAllocList);
  debugStringAllocList.allocatedMemory = 0L;
  List_init(&debugStringFreeList);
  debugStringFreeList.allocatedMemory = 0L;
  #ifdef MAX_STRINGS_CHECK
    debugMaxStringNextWarningCount = WARN_MAX_STRINGS;
  #endif /* MAX_STRINGS_CHECK */
}
#endif /* not NDEBUG */

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
  string->data[0]   = '\0';
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
    DebugStringNode *debugStringNode;
  #endif /* not NDEBUG */

  tmpString = allocString();

  #ifndef NDEBUG
    pthread_once(&debugStringInitFlag,debugStringInit);

    pthread_mutex_lock(&debugStringLock);
    {
      // update allocation info
      debugStringAllocList.allocatedMemory += sizeof(struct __String)+tmpString->maxLength;

      // allocate new debug node
      debugStringNode = (DebugStringNode*)__List_newNode(__fileName__,__lineNb__,sizeof(DebugStringNode));
      if (debugStringNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
      debugStringAllocList.allocatedMemory += sizeof(DebugStringNode);

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
      List_append(&debugStringAllocList,debugStringNode);
    }
    pthread_mutex_unlock(&debugStringLock);
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
    DebugStringNode *debugStringNode;
  #endif /* not NDEBUG */

  assert(string != NULL);
  assert(string->data != NULL);
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
    pthread_once(&debugStringInitFlag,debugStringInit);

    pthread_mutex_lock(&debugStringLock);
    {
      // remove string from allocated list
      debugStringNode = debugStringAllocList.head;
      while ((debugStringNode != NULL) && (debugStringNode->string != tmpString))
      {
        debugStringNode = debugStringNode->next;
      }
      if (debugStringNode == NULL)
      {
        HALT_INTERNAL_ERROR("Temporary string not found in allocated string list!");
      }
      List_remove(&debugStringAllocList,debugStringNode);
      assert(debugStringAllocList.allocatedMemory >= sizeof(DebugStringNode)+sizeof(struct __String)+string->maxLength);
      debugStringAllocList.allocatedMemory -= sizeof(DebugStringNode)+sizeof(struct __String)+string->maxLength;
      LIST_DELETE_NODE(debugStringNode);
    }
    pthread_mutex_unlock(&debugStringLock);
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

  if ((newLength + 1) > string->maxLength)
  {
    newMaxLength = ((newLength + 1) + STRING_DELTA_LENGTH - 1) & ~(STRING_DELTA_LENGTH - 1);
    assert(newMaxLength >= (newLength + 1));
    newData = realloc(string->data,newMaxLength);
//??? error message?
    if (newData == NULL)
    {
      fprintf(stderr,"FATAL ERROR: insufficient memory for allocating string (%lu bytes) - program halted: %s\n",newMaxLength,strerror(errno));
      exit(128);
    }
    #ifndef NDEBUG
      pthread_once(&debugStringInitFlag,debugStringInit);

      pthread_mutex_lock(&debugStringLock);
      {
        debugStringAllocList.allocatedMemory += (newMaxLength-string->maxLength);
      }
      pthread_mutex_unlock(&debugStringLock);
      #ifdef FILL_MEMORY
        memset(&newData[string->maxLength],DEBUG_FILL_BYTE,newMaxLength-string->maxLength);
      #endif /* FILL_MEMORY */
    #endif /* not NDEBUG */

    string->data      = newData;
    string->maxLength = newMaxLength;
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
  formatToken->quoteChar        = '\0';
  formatToken->conversionChar   = '\0';

  // format start character
  assert((*format) == '%');
  ADD_CHAR(formatToken,(*format));
  format++;

  // flags
  while (   ((*format) != '\0')
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
  while (   ((*format) != '\0')
         && isdigit((int)(*format))
        )
  {
    ADD_CHAR(formatToken,(*format));

    formatToken->width=formatToken->width*10+((*format)-'0');
    format++;
  }

  // precision
  if (   ((*format) != '\0')
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
  if (   ((*format) != '\0')
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
  if ((*format) != '\0')
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
  if ((*format) != '\0')
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

  ADD_CHAR(formatToken,'\0');

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
                        const va_list   arguments
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
  int           j;

  STRING_CHECK_VALID(string);

  while ((*format) != '\0')
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
          assert(sizeof(STRING_ESCAPE_CHARACTERS) == STRING_ESCAPE_LENGTH);
          assert(sizeof(STRING_ESCAPE_MAP) == STRING_ESCAPE_LENGTH);

          data.s = va_arg(arguments,const char*);
          assert(data.s != NULL);

          if (formatToken.quoteChar != '\0')
          {
            // quoted string
            String_appendChar(string,formatToken.quoteChar);
            s = data.s;
            while ((ch = (*s)) != '\0')
            {
              if (ch == formatToken.quoteChar)
              {
                String_appendChar(string,'\\');
                String_appendChar(string,ch);
              }
              else
              {
                // check if escape character
                j = STRING_ESCAPE_LENGTH-1;
                while ((j >= 0) && (STRING_ESCAPE_CHARACTERS[j] != ch))
                {
                  j--;
                }

                if (j >= 0)
                {
                  // escape character
                  String_appendChar(string,'\\');
                  String_appendChar(string,STRING_ESCAPE_MAP[j]);
                }
                else
                {
                  // other character
                  String_appendChar(string,ch);
                }
              }
              s++;
            }
            String_appendChar(string,formatToken.quoteChar);
          }
          else
          {
            // non quoted string
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
          break;
        case 'p':
        case 'n':
          data.p = va_arg(arguments,void*);
          assert(data.p != NULL);

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
          break;
        case 'S':
          data.string = (struct __String*)va_arg(arguments,void*);
          assert(string != NULL);
          STRING_CHECK_VALID(data.string);

          if (formatToken.quoteChar != '\0')
          {
            // quoted string
            String_appendChar(string,formatToken.quoteChar);
            i = 0L;
            while (i < String_length(data.string))
            {
              ch = String_index(data.string,i);
              if (ch == formatToken.quoteChar)
              {
                String_appendChar(string,'\\');
                String_appendChar(string,ch);
              }
              else
              {
                // check if escape character
                j = SIZE_OF_ARRAY(STRING_ESCAPE_CHARACTERS)-1;
                while ((j >= 0) && (STRING_ESCAPE_CHARACTERS[j] != ch))
                {
                  j--;
                }

                if (j >= 0)
                {
                  // escape character
                  assert(SIZE_OF_ARRAY(STRING_ESCAPE_CHARACTERS) == SIZE_OF_ARRAY(STRING_ESCAPE_MAP));
                  assert((uint)j < SIZE_OF_ARRAY(STRING_ESCAPE_MAP));

                  String_appendChar(string,'\\');
                  String_appendChar(string,STRING_ESCAPE_MAP[j]);
                }
                else
                {
                  // other character
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

LOCAL bool parseString(const char    *string,
                       ulong         length,
                       ulong         index,
                       const char    *format,
                       const va_list arguments,
                       const char    *stringQuotes,
                       long          *nextIndex
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
  ulong       z;
  const char  *stringQuote;
  bool        foundFlag;

  while ((*format) != '\0')
  {
    // skip white spaces in format
    while (((*format) != '\0') && isspace(*format))
    {
      format++;
    }

    // skip white-spaces in string
    while ((index < length) && isspace(string[index]))
    {
      index++;
    }

    if ((*format) != '\0')
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
            // get data
            z = 0L;
            if ((index < length) && ((string[index] == '+') || (string[index] == '-')))
            {
              buffer[z] = string[index];
              z++;
              index++;
            }
            while (   (index < length)
                   && (z < sizeof(buffer)-1)
                   && isdigit(string[index])
                  )
            {
              buffer[z] = string[index];
              z++;
              index++;
            }
            buffer[z] = '\0';

            // convert
            if (z > 0)
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
            // get data
            z = 0L;
            if ((index < length) && (string[index] == '+'))
            {
              index++;
            }
            while (   (index < length)
                   && (z < sizeof(buffer)-1)
                   && isdigit(string[index])
                  )
            {
              buffer[z] = string[index];
              z++;
              index++;
            }
            buffer[z] = '\0';

            // convert
            if (z > 0)
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
            z = 0L;
            while (   (index < length)
                   && (z < sizeof(buffer)-1)
                   && (string[index] >= '0')
                   && (string[index] <= '7')
                  )
            {
              buffer[z] = string[index];
              z++;
              index++;
            }
            buffer[z] = '\0';

            // convert
            if (z > 0)
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
            // get data
            if (((index+1) < length) && (string[index+0] == '0') && (string[index+0] == 'x'))
            {
              index += 2;
            }
            z = 0L;
            while (   (index < length)
                   && (z < sizeof(buffer)-1)
                   && isdigit(string[index])
                  )
            {
              buffer[z] = string[index];
              z++;
              index++;
            }
            buffer[z] = '\0';

            // convert
            if (z > 0)
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
            // get data
            z = 0L;
            if ((index < length) && ((string[index] == '+') || (string[index] == '-')  || (string[index] == '.')))
            {
              buffer[z] = string[index];
              z++;
              index++;
            }
            while (   (index < length)
                   && (z < sizeof(buffer)-1)
                   && isdigit(string[index])
                  )
            {
              buffer[z] = string[index];
              z++;
              index++;
            }
            if ((index < length) && (string[index] == '.'))
            {
              buffer[z] = '.';
              z++;
              index++;
              while (   (index < length)
                     && (z < sizeof(buffer)-1)
                     && isdigit(string[index])
                    )
              {
                buffer[z] = string[index];
                z++;
                index++;
              }
            }
            buffer[z] = '\0';

            // convert
            if (z > 0)
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

            z = 0L;
            if (index < length)
            {
              while (   (index < length)
                     && (formatToken.blankFlag || !isspace(string[index]))
                     && (string[index] != (*format))
                    )
              {
                if (   (string[index] == '\\')
                    && ((index+1) < length)
                    && !formatToken.blankFlag
                   )
                {
                  // quoted character
                  if ((formatToken.width == 0) || (z < formatToken.width-1))
                  {
                    String_appendChar(value.string,string[index+1]);
                    z++;
                  }
                  index += 2;
                }
                else
                {
                  // check for string quote
                  stringQuote = NULL;
                  if ((formatToken.quoteChar != '\0') && (formatToken.quoteChar == string[index])) stringQuote = &formatToken.quoteChar;
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
                            && (string[index] == '\\')
                            && (string[index+1] == (*stringQuote))
                           )
                        {
                          if ((formatToken.width == 0) || (z < formatToken.width-1))
                          {
                            if (value.s != NULL) value.s[z] = string[index+1];
                            z++;
                          }
                          index += 2;
                        }
                        else
                        {
                          if (z < (formatToken.width-1))
                          {
                            if (value.s != NULL) value.s[z] = string[index];
                            z++;
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
                        if ((formatToken.quoteChar != '\0') && (formatToken.quoteChar == string[index])) stringQuote = &formatToken.quoteChar;
                        if ((stringQuote == NULL) && (stringQuotes != NULL)) stringQuote = strchr(stringQuotes,string[index]);
                      }
                    }
                    while (stringQuote != NULL);
                  }
                  else
                  {
                    if (z < (formatToken.width-1))
                    {
                      if (value.s != NULL) value.s[z] = string[index];
                      z++;
                    }
                    index++;
                  }
                }
              }
            }
            if (value.s != NULL) value.s[z] = '\0';
            break;
          case 'p':
          case 'n':
            break;
          case 'S':
            // get and copy data
            value.string = va_arg(arguments,String);
            STRING_CHECK_VALID(value.string);

            String_clear(value.string);
            if (index < length)
            {
              z = 0L;
              while (   (index < length)
                     && (formatToken.blankFlag || !isspace(string[index]))
// NUL in string here a problem?
                     && (string[index] != (*format))
                    )
              {
                if (   (string[index] == '\\')
                    && ((index+1) < length)
                    && !formatToken.blankFlag
                   )
                {
                  // quoted character
                  if ((formatToken.width == 0) || (z < formatToken.width-1))
                  {
                    String_appendChar(value.string,string[index+1]);
                    z++;
                  }
                  index += 2;
                }
                else
                {
                  // check for string quote
                  stringQuote = NULL;
                  if ((formatToken.quoteChar != '\0') && (formatToken.quoteChar == string[index])) stringQuote = &formatToken.quoteChar;
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
                            && (string[index] == '\\')
                            && (string[index+1] == (*stringQuote))
                           )
                        {
                          if ((formatToken.width == 0) || (z < formatToken.width-1))
                          {
                            String_appendChar(value.string,string[index+1]);
                            z++;
                          }
                          index += 2;
                        }
                        else
                        {
                          if ((formatToken.width == 0) || (z < formatToken.width-1))
                          {
                            String_appendChar(value.string,string[index]);
                            z++;
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
                        if ((formatToken.quoteChar != '\0') && (formatToken.quoteChar == string[index])) stringQuote = &formatToken.quoteChar;
                        if ((stringQuote == NULL) && (stringQuotes != NULL)) stringQuote = strchr(stringQuotes,string[index]);
                      }
                    }
                    while (stringQuote != NULL);
                  }
                  else
                  {
                    if ((formatToken.width == 0) || (z < formatToken.width-1))
                    {
                      String_appendChar(value.string,string[index]);
                      z++;
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
            z = 0L;
            while (   (index < length)
                   && !isspace(string[index])
                  )
            {
              if (z < sizeof(buffer)-1)
              {
                buffer[z] = string[index];
                z++;
              }
              index++;
            }
            buffer[z] = '\0';

            // convert
            if (z > 0)
            {
              value.b = va_arg(arguments,bool*);
              foundFlag = FALSE;
              z = 0L;
              while (!foundFlag && (z < SIZE_OF_ARRAY(DEFAULT_TRUE_STRINGS)))
              {
                if (strcmp(buffer,DEFAULT_TRUE_STRINGS[z]) == 0)
                {
                  if (value.b != NULL) (*value.b) = TRUE;
                  foundFlag = TRUE;
                }
                z++;
              }
              z = 0L;
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
            z = 0L;
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
* Input  : string            - string to patch
*          index             - start index in string
*          pattern           - regualar expression pattern
*          nextIndex         - variable for index of next not matched
*                              character (can be NULL)
*          matchedString     - matched string (can be NULL)
*          matchedSubStrings - matched sub-strings
* Output : nextIndex         - index of next not matched character
*          matchedString     - matched string (can be NULL)
*          matchedSubStrings - matched sub-strings
* Return : TRUE if string matched, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool matchString(const String  string,
                       ulong         index,
                       const char    *pattern,
                       long          *nextIndex,
                       String        matchedString,
                       va_list       matchedSubStrings
                      )
{
  regex_t    regex;
  va_list    arguments;
  String     matchedSubString;
  regmatch_t *subMatches;
  uint       subMatchCount;
  bool       matchFlag;
  uint       z;

  assert(string != NULL);
  assert(string->data != NULL);
  assert(pattern != NULL);

  if (index < string->length)
  {
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
                         0
                        ) == 0
                );

    // get sub-matches
    if (matchFlag)
    {
      if (nextIndex != NULL)
      {
        (*nextIndex) = subMatches[0].rm_eo-subMatches[0].rm_so;
      }

      if (matchedString != NULL)
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
            String_setBuffer(matchedSubString,&string->data[subMatches[z].rm_so],subMatches[z].rm_eo-subMatches[z].rm_so);
          }
        }
      }
      va_end(arguments);
    }

    // free resources
    free(subMatches);
    regfree(&regex);
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
    DebugStringNode *debugStringNode;
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
      // update allocation info
      debugStringAllocList.allocatedMemory += sizeof(struct __String)+string->maxLength;

      // find string in free-list; reuse or allocate new debug node
      debugStringNode = debugStringFreeList.head;
      while ((debugStringNode != NULL) && (debugStringNode->string != string))
      {
        debugStringNode = debugStringNode->next;
      }
      if (debugStringNode != NULL)
      {
        List_remove(&debugStringFreeList,debugStringNode);
        assert(debugStringFreeList.allocatedMemory >= sizeof(DebugStringNode));
        debugStringFreeList.allocatedMemory -= sizeof(DebugStringNode);
      }
      else
      {
        debugStringNode = (DebugStringNode*)__List_newNode(__fileName__,__lineNb__,sizeof(DebugStringNode));
        if (debugStringNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
      }
      debugStringAllocList.allocatedMemory += sizeof(DebugStringNode);

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
      List_append(&debugStringAllocList,debugStringNode);
      #ifdef MAX_STRINGS_CHECK
        debugStringCount = List_count(&debugStringAllocList);
        if (debugStringCount > debugMaxStringNextWarningCount)
        {
          fprintf(stderr,"DEBUG WARNING: %lu strings allocated!\n",debugStringCount);
          debugMaxStringNextWarningCount += WARN_MAX_STRINGS_DELTA;
//String_debugPrintAllocated();
          sleep(1);
        }
      #endif /* MAX_STRINGS_CHECK */
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
String String_duplicate(const String fromString)
#else /* not NDEBUG */
String __String_duplicate(const char *__fileName__, ulong __lineNb__, const String fromString)
#endif /* NDEBUG */
{
  struct __String *string;

  STRING_CHECK_VALID(fromString);

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
    string->data[fromString->length] ='\0';
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
String String_copy(String *string, const String fromString)
#else /* not NDEBUG */
String __String_copy(const char *__fileName__, ulong __lineNb__, String *string, const String fromString)
#endif /* NDEBUG */
{
  STRING_CHECK_VALID(fromString);

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
    (*string)->data[fromString->length] ='\0';
    (*string)->length = fromString->length;

    STRING_UPDATE_VALID(*string);
  }
  else
  {
    if ((*string) != NULL)
    {
      (*string)->data[0] ='\0';
      (*string)->length = 0L;

      STRING_UPDATE_VALID(*string);
    }
  }

  return (*string);
}

#ifdef NDEBUG
void String_delete(String string)
#else /* not NDEBUG */
void __String_delete(const char *__fileName__, ulong __lineNb__, String string)
#endif /* NDEBUG */
{
  #ifndef NDEBUG
    DebugStringNode *debugStringNode;
  #endif /* not NDEBUG */

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    #ifndef NDEBUG
      pthread_once(&debugStringInitFlag,debugStringInit);

      pthread_mutex_lock(&debugStringLock);
      {
        // find string in free-list to check for duplicate free
        debugStringNode = debugStringFreeList.head;
        while ((debugStringNode != NULL) && (debugStringNode->string != string))
        {
          debugStringNode = debugStringNode->next;
        }
        if (debugStringNode != NULL)
        {
          fprintf(stderr,"DEBUG WARNING: multiple free of string %p at %s, %lu and previously at %s, %lu which was allocated at %s, %ld!\n",
                  string,
                  __fileName__,
                  __lineNb__,
                  debugStringNode->deleteFileName,
                  debugStringNode->deleteLineNb,
                  debugStringNode->allocFileName,
                  debugStringNode->allocLineNb
                 );
          #ifdef HAVE_BACKTRACE
            debugDumpStackTrace(stderr,"allocated at",2,debugStringNode->stackTrace,debugStringNode->stackTraceSize);
            debugDumpStackTrace(stderr,"deleted at",2,debugStringNode->deleteStackTrace,debugStringNode->deleteStackTraceSize);
          #endif /* HAVE_BACKTRACE */
          HALT_INTERNAL_ERROR("");
        }

        // remove string from allocated list, add string to free-list, shorten list
        debugStringNode = debugStringAllocList.head;
        while ((debugStringNode != NULL) && (debugStringNode->string != string))
        {
          debugStringNode = debugStringNode->next;
        }
        if (debugStringNode != NULL)
        {
          // remove from allocated list
          List_remove(&debugStringAllocList,debugStringNode);
          assert(debugStringAllocList.allocatedMemory >= sizeof(DebugStringNode)+sizeof(struct __String)+string->maxLength);
          debugStringAllocList.allocatedMemory -= sizeof(DebugStringNode)+sizeof(struct __String)+string->maxLength;

          // add to free list
          debugStringNode->deleteFileName = __fileName__;
          debugStringNode->deleteLineNb   = __lineNb__;
          #ifdef HAVE_BACKTRACE
            debugStringNode->deleteStackTraceSize = backtrace((void*)debugStringNode->deleteStackTrace,SIZE_OF_ARRAY(debugStringNode->deleteStackTrace));
          #endif /* HAVE_BACKTRACE */
          List_append(&debugStringFreeList,debugStringNode);
          debugStringFreeList.allocatedMemory += sizeof(DebugStringNode);

          // shorten free list
          while (debugStringFreeList.count > DEBUG_MAX_FREE_LIST)
          {
            debugStringNode = (DebugStringNode*)List_getFirst(&debugStringFreeList);
            debugStringFreeList.allocatedMemory -= sizeof(DebugStringNode);
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
            debugDumpCurrentStackTrace(stderr,"",0);
          #endif /* HAVE_BACKTRACE */
          HALT_INTERNAL_ERROR("");
        }
      }
      pthread_mutex_unlock(&debugStringLock);
    #endif /* not NDEBUG */

    free(string->data);
    free(string);
  }
}

String String_clear(String string)
{
  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    string->data[0] = '\0';
    string->length = 0L;

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_erase(String string)
{
  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    memset(string->data,0,string->maxLength);
    string->length = 0L;

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_set(String string, const String sourceString)
{
  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (sourceString != NULL)
    {
      assert(sourceString->data != NULL);

      ensureStringLength(string,sourceString->length);
      memcpy(&string->data[0],&sourceString->data[0],sourceString->length);
      string->data[sourceString->length] = '\0';
      string->length = sourceString->length;
    }
    else
    {
      string->data[0] = '\0';
      string->length = 0L;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_setCString(String string, const char *s)
{
  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      String_setBuffer(string,s,strlen(s));
    }
    else
    {
      string->data[0] = '\0';
      string->length = 0L;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_setChar(String string, char ch)
{
  STRING_CHECK_VALID(string);

  String_setBuffer(string,&ch,1);

  STRING_UPDATE_VALID(string);

  return string;
}

String String_setBuffer(String string, const void *buffer, ulong bufferLength)
{
  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (buffer != NULL)
    {
      ensureStringLength(string,bufferLength);
      memcpy(&string->data[0],buffer,bufferLength);
      string->data[bufferLength] = '\0';
      string->length = bufferLength;
    }
    else
    {
      string->data[0] = '\0';
      string->length = 0L;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_sub(String string, const String fromString, ulong fromIndex, long fromLength)
{
  ulong n;

  STRING_CHECK_VALID(string);
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
        ensureStringLength(string,n);
        memcpy(&string->data[0],&fromString->data[fromIndex],n);
        string->data[n] ='\0';
        string->length = n;
      }
    }
    else
    {
      string->data[0] = '\0';
      string->length = 0L;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

char *String_subCString(char *s, const String fromString, ulong fromIndex, long fromLength)
{
  ulong n;

  assert(s != NULL);

  STRING_CHECK_VALID(fromString);

  if (fromLength > 0)
  {
    if (fromString != NULL)
    {
      assert(fromString->data != NULL);

      if (fromIndex < fromString->length)
      {
        n = MIN((ulong)fromLength,fromString->length-fromIndex);
        memcpy(s,&fromString->data[fromIndex],n);
        s[n] = '\0';
      }
      else
      {
        s[0] = '\0';
      }
    }
    else
    {
      s[0] = '\0';
    }
  }

  return s;
}

char *String_subBuffer(char *buffer, const String fromString, ulong fromIndex, long fromLength)
{
  ulong n;

  assert(buffer != NULL);

  STRING_CHECK_VALID(fromString);

  if (fromLength > 0)
  {
    if (fromString != NULL)
    {
      assert(fromString->data != NULL);

      if (fromIndex < fromString->length)
      {
        n = MIN((ulong)fromLength,fromString->length-fromIndex);
        memcpy(&buffer[0],&fromString->data[fromIndex],n);
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

String String_append(String string, const String appendString)
{
  ulong n;

  STRING_CHECK_VALID(string);
  STRING_CHECK_VALID(appendString);

  if (string != NULL)
  {
    assert(string->data != NULL);
    if (appendString != NULL)
    {
      n = string->length+appendString->length;
      ensureStringLength(string,n);
      memcpy(&string->data[string->length],&appendString->data[0],appendString->length);
      string->data[n] = '\0';
      string->length = n;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_appendSub(String string, const String fromString, ulong fromIndex, long fromLength)
{
  ulong n;

  STRING_CHECK_VALID(string);
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
        memcpy(&string->data[string->length],&fromString->data[fromIndex],n);
        string->data[string->length+n] ='\0';
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

  if (string != NULL)
  {
    assert(string->data != NULL);
    n = string->length+1;
    ensureStringLength(string,n);
    string->data[string->length] = ch;
    string->data[n] = '\0';
    string->length = n;

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_appendBuffer(String string, const char *buffer, ulong bufferLength)
{
  ulong n;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);
    if (buffer != NULL)
    {
      n = string->length+bufferLength;
      ensureStringLength(string,n);
      memcpy(&string->data[string->length],buffer,bufferLength);
      string->data[n] = '\0';
      string->length = n;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_insert(String string, ulong index, const String insertString)
{
  ulong n;

  STRING_CHECK_VALID(string);
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
        memcpy(&string->data[string->length],&insertString->data[0],insertString->length);
        string->data[n] = '\0';
        string->length = n;
      }
      else if (index <= string->length)
      {
        n = string->length+insertString->length;
        ensureStringLength(string,n);
        memmove(&string->data[index+insertString->length],&string->data[index],string->length-index);
        memcpy(&string->data[index],&insertString->data[0],insertString->length);
        string->data[n] = '\0';
        string->length = n;
      }
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_insertSub(String string, ulong index, const String fromString, ulong fromIndex, long fromLength)
{
  ulong n;

  STRING_CHECK_VALID(string);
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
          memcpy(&string->data[string->length],&fromString->data[fromIndex],n);
          string->data[string->length+n] = '\0';
          string->length += n;
        }
        else if (index <= string->length)
        {
          ensureStringLength(string,string->length+n);
          memmove(&string->data[index+n],&string->data[index],string->length-index);
          memcpy(&string->data[index],&fromString->data[fromIndex],n);
          string->data[string->length+n] = '\0';
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

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (buffer != NULL)
    {
      if      (index == STRING_END)
      {
        n = string->length+bufferLength;
        ensureStringLength(string,n);
        memcpy(&string->data[string->length],buffer,bufferLength);
        string->data[n] = '\0';
        string->length = n;
      }
      else if (index <= string->length)
      {
        n = string->length+bufferLength;
        ensureStringLength(string,n);
        memmove(&string->data[index+bufferLength],&string->data[index],string->length-index);
        memcpy(&string->data[index],buffer,bufferLength);
        string->data[n] = '\0';
        string->length = n;
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

  if (string != NULL)
  {
    assert(string->data != NULL);

    if      (index == STRING_END)
    {
      n = (string->length > length) ? string->length-length : 0L;
      string->data[n] = '\0';
      string->length = n;
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
      string->data[n] = '\0';
      string->length = n;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_replace(String string, ulong index, ulong length, const String insertString)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_VALID(insertString);

  String_remove(string,index,length);
  String_insert(string,index,insertString);

  return string;
}

String String_replaceCString(String string, ulong index, ulong length, const char *s)
{
  STRING_CHECK_VALID(string);

  String_remove(string,index,length);
  String_insertCString(string,index,s);

  return string;
}

String String_replaceChar(String string, ulong index, ulong length, char ch)
{
  STRING_CHECK_VALID(string);

  String_remove(string,index,length);
  String_insertChar(string,index,ch);

  return string;
}

String String_replaceBuffer(String string, ulong index, ulong length, const char *buffer, ulong bufferLength)
{
  STRING_CHECK_VALID(string);

  String_remove(string,index,length);
  String_insertBuffer(string,index,buffer,bufferLength);

  return string;
}

String String_replaceAll(String string, ulong index, const String fromString, const String toString)
{
  STRING_CHECK_VALID(string);

  return String_map(string,index,&fromString,&toString,1);
}

String String_replaceAllCString(String string, ulong index, const char *from, const char *to)
{
  STRING_CHECK_VALID(string);

  return String_mapCString(string,index,&from,&to,1);
}

String String_replaceAllChar(String string, ulong index, char fromCh, char toCh)
{
  STRING_CHECK_VALID(string);

  return String_mapChar(string,index,&fromCh,&toCh,1);
}

String String_map(String string, ulong index, const String from[], const String to[], uint count)
{
  uint  z;
  ulong l0,l1;
  bool  replaceFlag;

  STRING_CHECK_VALID(string);

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

String String_mapChar(String string, ulong index, char from[], char to[], uint count)
{
  uint z;
  bool replaceFlag;

  STRING_CHECK_VALID(string);

  while (index < String_length(string))
  {
    replaceFlag = FALSE;
    for (z = 0; z < count; z++)
    {
      if (String_subEqualsChar(string,from[z],index))
      {
        String_replaceChar(string,index,1,to[z]);
        index += 1;
        replaceFlag = TRUE;
        break;
      }
    }
    if (!replaceFlag) index++;
  }

  return string;
}

String String_join(String string, const String joinString, char joinChar)
{
  STRING_CHECK_VALID(string);
  STRING_CHECK_VALID(joinString);

  if (!String_isEmpty(string)) String_appendChar(string,joinChar);
  String_append(string,joinString);

  return string;
}

String String_joinCString(String string, const char *s, char joinChar)
{
  STRING_CHECK_VALID(string);

  if (!String_isEmpty(string)) String_appendChar(string,joinChar);
  String_appendCString(string,s);

  return string;
}

String String_joinChar(String string, char ch, char joinChar)
{
  STRING_CHECK_VALID(string);

  if (!String_isEmpty(string)) String_appendChar(string,joinChar);
  String_appendChar(string,ch);

  return string;
}

String String_joinBuffer(String string, const char *buffer, ulong bufferLength, char joinChar)
{
  STRING_CHECK_VALID(string);

  if (!String_isEmpty(string)) String_appendChar(string,joinChar);
  String_appendBuffer(string,buffer,bufferLength);

  return string;
}

int String_compare(const String          string1,
                   const String          string2,
                   StringCompareFunction stringCompareFunction,
                   void                  *stringCompareUserData
                  )
{
  ulong n;
  ulong z;
  int   result;

  assert(string1 != NULL);
  assert(string2 != NULL);

  STRING_CHECK_VALID(string1);
  STRING_CHECK_VALID(string2);

  result = 0;
  n = MIN(string1->length,string2->length);
  z = 0L;
  if (stringCompareFunction != NULL)
  {
    while ((result == 0) && (z < n))
    {
      result = stringCompareFunction(stringCompareUserData,string1->data[z],string2->data[z]);
      z++;
    }
  }
  else
  {
    while ((result == 0) && (z < n))
    {
      if      (string1->data[z] < string2->data[z]) result = -1;
      else if (string1->data[z] > string2->data[z]) result =  1;
      z++;
    }
  }
  if (result == 0)
  {
    if      (string1->length < string2->length) result = -1;
    else if (string1->length > string2->length) result =  1;
  }

  return result;
}

bool String_equals(const String string1, const String string2)
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

bool String_equalsCString(const String string, const char *s)
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

bool String_equalsChar(const String string, char ch)
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

bool String_equalsBuffer(const String string, const char *buffer, ulong bufferLength)
{
  bool  equalFlag;
  ulong z;

  assert(string != NULL);
  assert(buffer != NULL);

  if (string != NULL)
  {
    STRING_CHECK_VALID(string);

    if (string->length == bufferLength)
    {
      equalFlag = TRUE;
      z         = 0L;
      while (equalFlag && (z < string->length))
      {
        equalFlag = (string->data[z] == buffer[z]);
        z++;
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

bool String_equalsIgnoreCase(const String string1, const String string2)
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

bool String_equalsIgnoreCaseCString(const String string, const char *s)
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

bool String_equalsIgnoreCaseChar(const String string, char ch)
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

bool String_equalsIgnoreCaseBuffer(const String string, const char *buffer, ulong bufferLength)
{
  bool  equalFlag;
  ulong z;

  assert(string != NULL);
  assert(buffer != NULL);

  if (string != NULL)
  {
    STRING_CHECK_VALID(string);

    if (string->length == bufferLength)
    {
      equalFlag = TRUE;
      z         = 0L;
      while (equalFlag && (z < string->length))
      {
        equalFlag = (toupper(string->data[z]) == toupper(buffer[z]));
        z++;
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

bool String_subEquals(const String string1, const String string2, long index, ulong length)
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

bool String_subEqualsCString(const String string, const char *s, long index, ulong length)
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

bool String_subEqualsChar(const String string, char ch, long index)
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

bool String_subEqualsBuffer(const String string, const char *buffer, ulong bufferLength, long index, ulong length)
{
  long  i;
  bool  equalFlag;
  ulong z;

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
      z         = 0L;
      while (equalFlag && (z < length))
      {
        equalFlag = (string->data[i+z] == buffer[z]);
        z++;
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

bool String_subEqualsIgnoreCase(const String string1, const String string2, long index, ulong length)
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

bool String_subEqualsIgnoreCaseCString(const String string, const char *s, long index, ulong length)
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

bool String_subEqualsIgnoreCaseChar(const String string, char ch, long index)
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

bool String_subEqualsIgnoreCaseBuffer(const String string, const char *buffer, ulong bufferLength, long index, ulong length)
{
  long  i;
  bool  equalFlag;
  ulong z;

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
      z         = 0L;
      while (equalFlag && (z < length))
      {
        equalFlag = (toupper(string->data[i+z]) == toupper(buffer[z]));
        z++;
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

bool String_startsWith(const String string1, const String string2)
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

bool String_startsWithCString(const String string, const char *s)
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

bool String_startsWithChar(const String string, char ch)
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

bool String_startsWithBuffer(const String string, const char *buffer, ulong bufferLength)
{
  bool  equalFlag;
  ulong z;

  assert(buffer != NULL);

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    if (string->length >= bufferLength)
    {
      equalFlag = TRUE;
      z         = 0L;
      while (equalFlag && (z < bufferLength))
      {
        equalFlag = (string->data[z] == buffer[z]);
        z++;
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

bool String_endsWith(const String string1, const String string2)
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

bool String_endsWithCString(const String string, const char *s)
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

bool String_endsWithChar(const String string, char ch)
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

bool String_endsWithBuffer(const String string, const char *buffer, ulong bufferLength)
{
  bool  equalFlag;
  ulong z;
  ulong i;

  assert(buffer != NULL);

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    if (string->length >= bufferLength)
    {
      equalFlag = TRUE;
      z         = 0L;
      i         = string->length-bufferLength;
      while (equalFlag && (z < bufferLength))
      {
        equalFlag = (string->data[i] == buffer[z]);
        z++;
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

long String_find(const String string, ulong index, const String findString)
{
  long  findIndex;
  long  i;
  ulong z;

  assert(string != NULL);
  assert(findString != NULL);

  STRING_CHECK_VALID(string);
  STRING_CHECK_VALID(findString);

  findIndex = -1;

  i = (index != STRING_BEGIN) ? index : 0L;
  while (((i+(long)findString->length) <= (long)string->length) && (findIndex < 0))
  {
    z = 0L;
    while ((z < findString->length) && (string->data[i+z] == findString->data[z]))
    {
      z++;
    }
    if (z >=  findString->length) findIndex = i;

    i++;
  }

  return findIndex;
}

long String_findCString(const String string, ulong index, const char *s)
{
  long  findIndex;
  ulong sLength;
  long  i;
  ulong z;

  assert(string != NULL);
  assert(s != NULL);

  STRING_CHECK_VALID(string);

  findIndex = -1L;

  sLength = (ulong)strlen(s);
  i = (index != STRING_BEGIN) ? index : 0L;
  while (((i+sLength) <= string->length) && (findIndex < 0))
  {
    z = 0L;
    while ((z < sLength) && (string->data[i+z] == s[z]))
    {
      z++;
    }
    if (z >= sLength) findIndex = i;

    i++;
  }

  return findIndex;
}

long String_findChar(const String string, ulong index, char ch)
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

long String_findLast(const String string, long index, String findString)
{
  long  findIndex;
  long  i;
  ulong z;

  assert(string != NULL);
  assert(findString != NULL);

  STRING_CHECK_VALID(string);

  findIndex = -1;

  i = (index != STRING_END) ? index : (long)string->length-1;
  while ((i >= 0) && (findIndex < 0))
  {
    z = 0L;
    while ((z < findString->length) && (string->data[i+z] == findString->data[z]))
    {
      z++;
    }
    if (z >= findString->length) findIndex = i;

    i--;
  }

  return findIndex;
}

long String_findLastCString(const String string, long index, const char *s)
{
  long  findIndex;
  ulong sLength;
  long  i;
  ulong z;

  assert(string != NULL);
  assert(s != NULL);

  STRING_CHECK_VALID(string);

  findIndex = -1L;

  sLength = (ulong)strlen(s);
  i = (index != STRING_END) ? index : (long)string->length-1;
  while ((i >= 0) && (findIndex < 0))
  {
    z = 0L;
    while ((z < sLength) && (string->data[i+z] == s[z]))
    {
      z++;
    }
    if (z >=  sLength) findIndex = i;

    i--;
  }

  return findIndex;
}

long String_findLastChar(const String string, long index, char ch)
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
  ulong      z;
  const char *s;
  ulong      n;

  assert(stringIterateFunction != NULL);

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    z = 0L;
    while (z < string->length)
    {
      s = stringIterateFunction(stringIterateUserData,string->data[z]);
      if (s != NULL)
      {
        n = strlen(s);
        ensureStringLength(string,string->length+n-1);
        memmove(&string->data[z+n],&string->data[z+1],string->length-(z+1));
        memcpy(&string->data[z],s,n);
        string->data[string->length+n-1] = '\0';
        string->length += n-1;

        z += n;
      }
      else
      {
        z += 1;
      }
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_toLower(String string)
{
  ulong z;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    for (z = 0L; z < string->length; z++)
    {
      string->data[z] = tolower(string->data[z]);
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_toUpper(String string)
{
  ulong z;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    for (z = 0L; z < string->length; z++)
    {
      string->data[z] = toupper(string->data[z]);
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_trim(String string, const char *chars)
{
  STRING_CHECK_VALID(string);

  String_trimRight(string,chars);
  String_trimLeft(string,chars);

  return string;
}

String String_trimRight(String string, const char *chars)
{
  ulong n;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    n = string->length;
    while ((n > 0) && (strchr(chars,string->data[n - 1]) != NULL))
    {
      n--;
    }
    string->data[n] = '\0';
    string->length = n;

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_trimLeft(String string, const char *chars)
{
  ulong z,n;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    z = 0L;
    while ((z < string->length) && (strchr(chars,string->data[z]) != NULL))
    {
      z++;
    }
    if (z > 0)
    {
      n = string->length - z;
      memmove(&string->data[0],&string->data[z],n);
      string->data[n] = '\0';
      string->length = n;
    }

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_escape(String string, const char *chars, char escapeChar)
{
  String s;
  ulong  z;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    #ifdef NDEBUG
      s = allocTmpString();
    #else /* not NDEBUG */
      s = allocTmpString(__FILE__,__LINE__);
    #endif /* NDEBUG */
    for (z = 0L; z < string->length; z++)
    {
      if ((string->data[z] == escapeChar) || ((chars != NULL) && (strchr(chars,string->data[z]) != NULL)))
      {
        String_appendChar(s,escapeChar);
      }
      String_appendChar(s,string->data[z]);
    }
    assignTmpString(string,s);
  }

  return string;
}

String String_unescape(String string, char escapeChar)
{
  String s;
  ulong  z;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    #ifdef NDEBUG
      s = allocTmpString();
    #else /* not NDEBUG */
      s = allocTmpString(__FILE__,__LINE__);
    #endif /* NDEBUG */
    z = 0L;
    while (z < string->length)
    {
      if (string->data[z] == escapeChar)
      {
        z++;
        if (z < string->length)
        {
          String_appendChar(s,string->data[z]);
        }
      }
      else
      {
        String_appendChar(s,string->data[z]);
      }
      z++;
    }
    assignTmpString(string,s);
  }

  return string;
}

String String_quote(String string, char quoteChar)
{
  String s;
  ulong  z;

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    #ifdef NDEBUG
      s = allocTmpString();
    #else /* not NDEBUG */
      s = allocTmpString(__FILE__,__LINE__);
    #endif /* NDEBUG */
    String_appendChar(s,quoteChar);
    for (z = 0; z < string->length; z++)
    {
      if (string->data[z] == quoteChar)
      {
        String_appendChar(s,'\\');
      }
      String_appendChar(s,string->data[z]);
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
  ulong      z;

  STRING_CHECK_VALID(string);

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
        z = 1;
        while (z < string->length-1)
        {
          if (string->data[z] == '\\')
          {
            z++;
            if (z < string->length-1)
            {
              String_appendChar(s,string->data[z]);
            }
          }
          else
          {
            String_appendChar(s,string->data[z]);
          }
          z++;
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

  if (string != NULL)
  {
    if (string->length < length)
    {
      n = length-string->length;
      ensureStringLength(string,length);
      memset(&string->data[string->length],ch,n);
      string->data[length] = '\0';
      string->length = length;

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

  if (string != NULL)
  {
    if (string->length < length)
    {
      n = length-string->length;
      ensureStringLength(string,length);
      memmove(&string->data[n],&string->data[0],string->length);
      memset(&string->data[0],ch,n);
      string->data[length] = '\0';
      string->length = length;

      STRING_UPDATE_VALID(string);
    }
  }

  return string;
}

String String_fillChar(String string, ulong length, char ch)
{
  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    ensureStringLength(string,length);
    memset(&string->data[0],ch,length);
    string->data[length] = '\0';
    string->length = length;

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

  if (string != NULL)
  {
    va_start(arguments,format);
    formatString(string,format,arguments);
    va_end(arguments);

    STRING_UPDATE_VALID(string);
  }

  return string;
}

String String_vformat(String string, const char *format, va_list arguments)
{
  assert(string != NULL);
  assert(format != NULL);

  STRING_CHECK_VALID(string);

  if (string != NULL)
  {
    formatString(string,format,arguments);

    STRING_UPDATE_VALID(string);
  }

  return string;
}

void String_initTokenizer(StringTokenizer *stringTokenizer,
                          const String    string,
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

bool String_getNextToken(StringTokenizer *stringTokenizer, String *const token, long *tokenIndex)
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
              && (   (stringTokenizer->data[stringTokenizer->index] == '\\')
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

bool String_scan(const String string, ulong index, const char *format, ...)
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

bool String_parse(const String string, ulong index, const char *format, long *nextIndex, ...)
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

bool String_match(const String string, ulong index, const String pattern, long *nextIndex, String matchedString, ...)
{
  va_list arguments;
  bool    matchFlag;

  va_start(arguments,matchedString);
  matchFlag = matchString(string,index,String_cString(pattern),nextIndex,matchedString,arguments);
  va_end(arguments);

  return matchFlag;
}

bool String_matchCString(const String string, ulong index, const char *pattern, long *nextIndex, String matchedString, ...)
{
  va_list arguments;
  bool    matchFlag;

  va_start(arguments,matchedString);
  matchFlag = matchString(string,index,pattern,nextIndex,matchedString,arguments);
  va_end(arguments);

  return matchFlag;
}

int String_toInteger(const String convertString, ulong index, long *nextIndex, const StringUnit stringUnits[], uint stringUnitCount)
{
  int  n;
  char *nextData;

  assert(convertString != NULL);

  STRING_CHECK_VALID(convertString);

  if (index < convertString->length)
  {
    n = strtol(&convertString->data[index],&nextData,0);
    if ((ulong)(nextData-convertString->data) < convertString->length)
    {
      if (stringUnitCount > 0)
      {
        n = n*(int)getUnitFactor(stringUnits,stringUnitCount,convertString->data,nextData,nextIndex);
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
    n = 0;
    if (nextIndex != NULL) (*nextIndex) = index;
  }

  return n;
}

int64 String_toInteger64(const String convertString, ulong index, long *nextIndex, const StringUnit stringUnits[], uint stringUnitCount)
{
  int64 n;
  char  *nextData;

  assert(convertString != NULL);

  if (index < convertString->length)
  {
    n = strtoll(&convertString->data[index],&nextData,0);
    if ((ulong)(nextData-convertString->data) < convertString->length)
    {
      if (stringUnitCount > 0)
      {
        n = n*(int64)getUnitFactor(stringUnits,stringUnitCount,convertString->data,nextData,nextIndex);
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
    n = 0LL;
    if (nextIndex != NULL) (*nextIndex) = index;
  }

  return n;
}

double String_toDouble(const String convertString, ulong index, long *nextIndex, const StringUnit stringUnits[], uint stringUnitCount)
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

bool String_toBoolean(const String convertString, ulong index, long *nextIndex, const char *trueStrings[], uint trueStringCount, const char *falseStrings[], uint falseStringCount)
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

String String_toString(String string, const String convertString, ulong index, long *nextIndex, const char *stringQuotes)
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
            if (convertString->data[index] == '\\')
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

char* String_toCString(const String string)
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
  cString[string->length] = '\0';

  return cString;
}

#ifndef NDEBUG

void String_debugCheckValid(const char *__fileName__, ulong __lineNb__, const String string)
{
  DebugStringNode *debugStringNode;

  if (string != NULL)
  {
    ulong checkSum;

    checkSum = STRING_CHECKSUM(string);
    if (checkSum != string->checkSum)
    {
      debugDumpCurrentStackTrace(stderr,"",0);
      HALT_INTERNAL_ERROR_AT(__fileName__,
                             __lineNb__,
                             "Invalid checksum 0x%08x in string %p, length %ld (max. %ld) (expected 0x%08x)!",
                             string->checkSum,
                             string,
                             string->length,
                             string->maxLength,
                             checkSum
                            );
    }

    pthread_once(&debugStringInitFlag,debugStringInit);

    pthread_mutex_lock(&debugStringLock);
    {
      debugStringNode = debugStringAllocList.head;
      while ((debugStringNode != NULL) && (debugStringNode->string != string))
      {
        debugStringNode = debugStringNode->next;
      }
      if (debugStringNode == NULL)
      {
        debugStringNode = debugStringFreeList.head;
        while ((debugStringNode != NULL) && (debugStringNode->string != string))
        {
          debugStringNode = debugStringNode->next;
        }

        debugDumpCurrentStackTrace(stderr,"",0);
        if (debugStringNode != NULL)
        {
          HALT_INTERNAL_ERROR_AT(__fileName__,
                                 __lineNb__,
                                 "String %p allocated at %s, %lu is already freed at %s, %l!",
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
}

void String_debugDone(void)
{
  pthread_once(&debugStringInitFlag,debugStringInit);

  String_debugCheck();

  pthread_mutex_lock(&debugStringLock);
  {
    debugMaxStringNextWarningCount = 0LL;
    List_done(&debugStringFreeList,NULL,NULL);
    List_done(&debugStringAllocList,NULL,NULL);
  }
  pthread_mutex_unlock(&debugStringLock);
}

void String_debugDumpInfo(FILE *handle)
{
  DebugStringNode *debugStringNode;

  pthread_once(&debugStringInitFlag,debugStringInit);

  pthread_mutex_lock(&debugStringLock);
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
        debugDumpStackTrace(handle,"allocated at",2,debugStringNode->stackTrace,debugStringNode->stackTraceSize);
      #endif /* HAVE_BACKTRACE */
    }
  }
  pthread_mutex_unlock(&debugStringLock);
}

void String_debugPrintInfo()
{
  String_debugDumpInfo(stderr);
}

void String_debugPrintStatistics(void)
{
  pthread_once(&debugStringInitFlag,debugStringInit);

  pthread_mutex_lock(&debugStringLock);
  {
    fprintf(stderr,"DEBUG: %lu string(s) allocated, total %lu bytes\n",
            List_count(&debugStringAllocList),
            debugStringAllocList.allocatedMemory
           );
    fprintf(stderr,"DEBUG: %lu string(s) in free list, total %lu bytes\n",
            List_count(&debugStringFreeList),
            debugStringFreeList.allocatedMemory
           );
  }
  pthread_mutex_unlock(&debugStringLock);
}

void String_debugCheck()
{
  pthread_once(&debugStringInitFlag,debugStringInit);

  String_debugPrintInfo();
  String_debugPrintStatistics();

  pthread_mutex_lock(&debugStringLock);
  {
    if (!List_isEmpty(&debugStringAllocList))
    {
      HALT_INTERNAL_ERROR_LOST_RESOURCE();
    }
  }
  pthread_mutex_unlock(&debugStringLock);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
