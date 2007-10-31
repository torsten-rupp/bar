/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/strings.c,v $
* $Revision: 1.18 $
* $Author: torsten $
* Contents: dynamic string functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "global.h"
#ifndef NDEBUG
  #include <pthread.h>
  #include "lists.h"
#endif /* not NDEBUG */

#include "strings.h"

/****************** Conditional compilation switches *******************/
#define HALT_ON_INSUFFICIENT_MEMORY

/***************************** Constants *******************************/
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

/***************************** Datatypes *******************************/
typedef enum
{
  FORMAT_LENGTH_TYPE_INTEGER,
  FORMAT_LENGTH_TYPE_LONG,
  FORMAT_LENGTH_TYPE_LONGLONG,
  FORMAT_LENGTH_TYPE_DOUBLE,
  FORMAT_LENGTH_TYPE_QUAD,
  FORMAT_LENGTH_TYPE_POINTER,
} FormatLengthType;

typedef struct
{
  char             token[16];
  unsigned int     length;
  bool             alternateFlag;
  bool             zeroPaddingFlag;
  bool             leftAdjustedFlag;
  bool             blankFlag;
  bool             signFlag;
  unsigned int     width;
  unsigned int     precision;
  FormatLengthType lengthType;
  char             quoteChar;
  char             conversionChar;
} FormatToken;

struct __String
{
  ulong length;
  ulong maxLength;
  char  *data;
  #ifndef NDEBUG
    ulong checkSum;
  #endif /* not NDEBUG */
};

#ifndef NDEBUG
  typedef struct DebugStringNode
  {
    NODE_HEADER(struct DebugStringNode);

    const char      *fileName;
    ulong           lineNb;
    struct __String *string;
  } DebugStringNode;

  typedef struct
  {
    LIST_HEADER(DebugStringNode);
  } DebugStringList;
#endif /* not NDEBUG */

/***************************** Variables *******************************/
#ifndef NDEBUG
  pthread_once_t  debugStringInitFlag = PTHREAD_ONCE_INIT;
  pthread_mutex_t debugStringLock;
  DebugStringList debugStringList;
#endif /* not NDEBUG */

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define CHECK_VALID(string) \
    do { \
      if (string != NULL) \
      { \
        if (((ulong)(string)->length^(ulong)(string)->maxLength^(ulong)(string)->data) != (string)->checkSum) \
        { \
          HALT_INTERNAL_ERROR("Invalid string %p!",string); \
        } \
      } \
    } while (0)
  #define UPDATE_VALID(string) \
    do { \
      if (string != NULL) \
      { \
        (string)->checkSum = (ulong)(string)->length^(ulong)(string)->maxLength^(ulong)(string)->data; \
        } \
    } while (0)
#else /* NDEBUG */
  #define CHECK_VALID(string) \
    do { \
    } while (0)
  #define UPDATE_VALID(string) \
    do { \
    } while (0)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifndef NDEBUG
LOCAL void debugStringInit(void)
{
  pthread_mutex_init(&debugStringLock,NULL);
  List_init(&debugStringList);
}
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : ensureStringLength
* Purpose: ensure min. length of string
* Input  : string    - string
*          newLength - new min. length of string
* Output : -
* Return : TRUE if string length is ok, FALSE on insufficient memory
* Notes  : -
\***********************************************************************/

LOCAL void ensureStringLength(struct __String *string, ulong newLength)
{
  char  *newData;
  ulong newMaxLength;

  if ((newLength + 1) > string->maxLength)
  {
    newMaxLength = ((newLength + 1) + STRING_DELTA_LENGTH - 1) & ~(STRING_DELTA_LENGTH - 1);
    newData = realloc(string->data,newMaxLength);
//??? error message?
    if (newData == NULL)
    {
      perror("insufficient memory for allocating string - program halted");
      exit(128);
    }

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

  assert(format!=NULL);
  assert(formatToken!=NULL);

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

  /* format start character */
  assert((*format)=='%');
  ADD_CHAR(formatToken,(*format));
  format++;

  /* flags */
  while (   ((*format) == '#')
         || ((*format) == '0')
         || ((*format) == '-')
         || ((*format) == ' ')
         || ((*format) == '+')
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

  /* width, precision */
  while (isdigit((int)(*format)))
  {
    ADD_CHAR(formatToken,(*format));

    formatToken->width=formatToken->width*10+((*format)-'0');
    format++;
  }

  /* precision */
  if ((*format) == '.')
  {
    format++;
    while (isdigit((int)(*format)))
    {
      ADD_CHAR(formatToken,(*format));

      formatToken->precision=formatToken->precision*10+((*format)-'0');
      format++;
    }
  }

  /* quoting character */
  if ((*(format+1) == 's') || (*((format+1)) == 'S'))
  {
    formatToken->quoteChar = (*format);
    format++;
  }

  /* length modifier */
  if      (((*format) == 'h') && (*((format+1)) == 'h'))
  {
    ADD_CHAR(formatToken,(*(format+0)));
    ADD_CHAR(formatToken,(*(format+1)));

    formatToken->lengthType = FORMAT_LENGTH_TYPE_INTEGER;
    format+=2;
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
    format+=2;
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

  /* conversion character */
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

LOCAL void formatString(struct __String *string,
                        const char      *format,
                        const va_list   arguments)
{
  FormatToken  formatToken;
  union
  {
    int                i;
    long               l;
    long long          ll;
    unsigned int       ui;
    unsigned long      ul;
    unsigned long long ull;
    double             d;
    const char         *s;
    void               *p;
    struct __String    *string;
  } data;
  char          buffer[64];
  unsigned int  length;
  const char    *s;
  unsigned long i;
  char          ch;

  while ((*format) != '\0')
  {
    if ((*format) == '%')
    {
      /* get format token */
      format = parseNextFormatToken(format,&formatToken);

      /* format and store string */
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
            snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,data.i);
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
                  snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,data.i);
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
                  snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,data.l);
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
                    snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,data.ll);
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
                data.ui = va_arg(arguments,int);
                length = snprintf(buffer,sizeof(buffer),formatToken.token,data.ui);
                if (length < sizeof(buffer))
                {
                  String_appendCString(string,buffer);
                }
                else
                {
                  ensureStringLength(string,string->length+length);
                  snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,data.ui);
                }
              }
              break;
            case FORMAT_LENGTH_TYPE_LONG:
              {
                data.ul = va_arg(arguments,long);
                length = snprintf(buffer,sizeof(buffer),formatToken.token,data.ul);
                if (length < sizeof(buffer))
                {
                  String_appendCString(string,buffer);
                }
                else
                {
                  ensureStringLength(string,string->length+length);
                  snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,data.ul);
                }
              }
              break;
            case FORMAT_LENGTH_TYPE_LONGLONG:
              {
                #if defined(_LONG_LONG) || defined(HAVE_LONG_LONG)
                  data.ull = va_arg(arguments,long long);
                  length = snprintf(buffer,sizeof(buffer),formatToken.token,data.ull);
                  if (length < sizeof(buffer))
                  {
                    String_appendCString(string,buffer);
                  }
                  else
                  {
                    ensureStringLength(string,string->length+length);
                    snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,data.ull);
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
          data.d = va_arg(arguments,double);
          length = snprintf(buffer,sizeof(buffer),formatToken.token,data.d);
          if (length < sizeof(buffer))
          {
            String_appendCString(string,buffer);
          }
          else
          {
            ensureStringLength(string,string->length+length);
            snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,data.d);
          }
          break;
        case 's':
          data.s = va_arg(arguments,const char*);
          assert(data.s != NULL);

          if (formatToken.quoteChar != '\0')
          {
            /* quoted string */
            String_appendChar(string,formatToken.quoteChar);
            s = data.s;
            while ((ch = (*s)) != '\0')
            {
              if (ch == formatToken.quoteChar)
              {
                String_appendChar(string,'\\');
              }
              String_appendChar(string,ch);
              s++;
            }
            String_appendChar(string,formatToken.quoteChar);
          }
          else
          {
            /* non quoted string */
            length = snprintf(buffer,sizeof(buffer),formatToken.token,data.s);
            if (length < sizeof(buffer))
            {
              String_appendCString(string,buffer);
            }
            else
            {
              ensureStringLength(string,string->length+length);
              snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,data.d);
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
            snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,data.p);
          }
          break;

        case 'S':
          data.string = (struct __String*)va_arg(arguments,void*);
          assert(string != NULL);
          CHECK_VALID(data.string);

          if (formatToken.quoteChar != '\0')
          {
            /* quoted string */
            String_appendChar(string,formatToken.quoteChar);
            i = 0;
            while (i < String_length(data.string))
            {
              ch = String_index(data.string,i);
              if (ch == formatToken.quoteChar)
              {
                String_appendChar(string,'\\');
              }
              String_appendChar(string,ch);
              i++;
            }
            String_appendChar(string,formatToken.quoteChar);
          }
          else
          {
            /* non quoted string */
            length = snprintf(buffer,sizeof(buffer),formatToken.token,String_cString(data.string));
            if (length < sizeof(buffer))
            {
              String_appendCString(string,buffer);
            }
            else
            {
              ensureStringLength(string,string->length + length);
              snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,String_cString(data.string));
            }
          }
          break;
#if 0
still not implemented
        case 'U':
          /* UTF8 string (as reference+index) */
          {
            jamaica_ref   stringReference;
            jamaica_int32 stringIndex;
            jamaica_int32 n;
            jamaica_int32 i;

            stringReference = va_arg(arguments,jamaica_ref);
            stringIndex     = va_arg(arguments,jamaica_int32);

            bufferCount = 0;
            if (vm != NULL)
            {
              n = utf8String_Length(vm,stringReference,stringIndex);
              for (i=0;i<n;i++)
              {
                OUTPUT_CHAR(buffer,bufferCount,(char)utf8String_CharAt(vm,stringReference,stringIndex,i));
              }
            }
            else
            {
              bufferCount=TARGET_NATIVE_MISC_FORMAT_STRING2(buffer,sizeof(buffer),"%p:%ld",stringReference,stringIndex);
              assert(bufferCount<sizeof(buffer));
            }
          }
          break;
        case 'b':
          /* binaray value */
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
                JAMAICA_HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break; /* not reached */
            #endif /* NDEBUG */
          }
          bufferCount = 0;
          FLUSH_OUTPUT();
JAMAICA_HALT_NOT_YET_IMPLEMENTED();
          break;
#endif /* 0 */
        case 'y':
          data.i = va_arg(arguments,int);
          String_appendChar(string,(data.i != 0)?'1':'0');
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
            snprintf(&string->data[string->length],sizeof(buffer),formatToken.token);
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

LOCAL bool parseString(const struct __String *string,
                       const char            *format,
                       const va_list         arguments,
                       const char            *stringQuotes,
                       ulong                 *nextIndex
                      )
{
  ulong       index;
  FormatToken formatToken;
  union
  {
    int             *i;
    long int        *l;
    long long int   *ll;
    double          *d;
    char            *c;
    char            *s;
    void            *p;
    bool            *b;
    struct __String *string;
  } value;
  char        buffer[64];
  ulong       z;
  const char  *stringQuote;
  bool        foundFlag;

  index = 0;
  while ((*format) != '\0')
  {
    /* skip white spaces in format */
    while (((*format) != '\0') && isspace(*format))
    {
      format++;
    }

    /* skip white-spaces in string */
    while ((index < string->length) && isspace(string->data[index]))
    {
      index++;
    }

    if ((*format) == '%')
    {
      /* get format token */
      format = parseNextFormatToken(format,&formatToken);

      /* parse string and store values */
      switch (formatToken.conversionChar)
      {
        case 'i':
        case 'd':
        case 'u':
          /* get data */
          z = 0;
          while (   (index < string->length)
                 && (z < sizeof(buffer)-1)
                 && isdigit(string->data[index])
                )
          {
            buffer[z] = string->data[index];
            z++;
            index++;
          }
          buffer[z] = '\0';

          /* convert */
          switch (formatToken.lengthType)
          {
            case FORMAT_LENGTH_TYPE_INTEGER:
              value.i = va_arg(arguments,int*);
              assert(value.i != NULL);
              (*value.i) = strtol(buffer,NULL,10);
              break;
            case FORMAT_LENGTH_TYPE_LONG:
              value.l = va_arg(arguments,long int*);
              assert(value.l != NULL);
              (*value.l) = strtol(buffer,NULL,10);
              break;
            case FORMAT_LENGTH_TYPE_LONGLONG:
              value.ll = va_arg(arguments,long long int*);
              assert(value.ll != NULL);
              (*value.ll) = strtoll(buffer,NULL,10);
              break;
            case FORMAT_LENGTH_TYPE_DOUBLE:
              break;
            case FORMAT_LENGTH_TYPE_QUAD:
              break;
            case FORMAT_LENGTH_TYPE_POINTER:
              break;
          }
          break;
        case 'c':
          /* get data */
          if (index < string->length)
          {
            buffer[0] = string->data[index];
            index++;
          }

          /* convert */
          value.c = va_arg(arguments,char*);
          assert(value.c != NULL);
          (*value.c) = buffer[0];
          break;
        case 'o':
          /* get data */
          z = 0;
          while (   (index < string->length)
                 && (z < sizeof(buffer)-1)
                 && (string->data[index] >= '0')
                 && (string->data[index] <= '7')
                )
          {
            buffer[z] = string->data[index];
            z++;
            index++;
          }
          buffer[z] = '\0';

          /* convert */
          value.i = va_arg(arguments,int*);
          assert(value.i != NULL);
          (*value.i) = strtol(buffer,NULL,8);
          break;
        case 'x':
        case 'X':
          /* get data */
          if (((index+1) < string->length) && (string->data[index+0] == '0') && (string->data[index+0] == 'x'))
          {
            index+=2;
          }
          z = 0;
          while (   (index < string->length)
                 && (z < sizeof(buffer)-1)
                 && isdigit(string->data[index])
                )
          {
            buffer[z] = string->data[index];
            z++;
            index++;
          }
          buffer[z] = '\0';

          /* convert */
          switch (formatToken.lengthType)
          {
            case FORMAT_LENGTH_TYPE_INTEGER:
              value.i = va_arg(arguments,int*);
              assert(value.i != NULL);
              (*value.i) = strtol(buffer,NULL,16);
              break;
            case FORMAT_LENGTH_TYPE_LONG:
              value.l = va_arg(arguments,long int*);
              assert(value.l != NULL);
              (*value.l) = strtol(buffer,NULL,16);
              break;
            case FORMAT_LENGTH_TYPE_LONGLONG:
              value.ll = va_arg(arguments,long long int*);
              assert(value.ll != NULL);
              (*value.ll) = strtol(buffer,NULL,16);
              break;
            case FORMAT_LENGTH_TYPE_DOUBLE:
              break;
            case FORMAT_LENGTH_TYPE_QUAD:
              break;
            case FORMAT_LENGTH_TYPE_POINTER:
              break;
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
          /* get data */
          z = 0;
          while (   (index < string->length)
                 && (z < sizeof(buffer)-1)
                 && isdigit(string->data[index])
                )
          {
            buffer[z] = string->data[index];
            z++;
            index++;
          }
          if ((index < string->length) && (string->data[index] == '.'))
          {
            buffer[z] = '.';
            z++;
            index++;
            while (   (index < string->length)
                   && (z < sizeof(buffer)-1)
                   && isdigit(string->data[index])
                  )
            {
              buffer[z] = string->data[index];
              z++;
              index++;
            }
          }
          buffer[z] = '\0';

          /* convert */
          value.d = va_arg(arguments,double*);
          assert(value.d != NULL);
          (*value.d) = strtod(buffer,NULL);
          break;
        case 's':
          /* get and copy data */
          value.s = va_arg(arguments,char*);
          assert(value.s != NULL);

          assert(formatToken.width > 0);

          z = 0;
          while (   (index < string->length)
                 && !isspace(string->data[index])
                 && (string->data[index] != (*format))
                )
          {
            stringQuote = NULL;
            if ((formatToken.quoteChar != '\0') && (formatToken.quoteChar == string->data[index])) stringQuote = &formatToken.quoteChar;
            if (stringQuotes != NULL) stringQuote = strchr(stringQuotes,string->data[index]);

            if (stringQuote != NULL)
            {
              do
              {
                /* skip quote-char */
                index++;

                /* get string */
                while ((index < string->length) && (string->data[index] != (*stringQuote)))
                {
                  if (string->data[index] == '\\')
                  {
                    if (index < string->length)
                    {
                      if (z < (formatToken.width-1))
                      {
                        value.s[z] = string->data[index];
                        z++;
                      }
                      index++;
                    }
                  }
                  else
                  {
                    if (z < (formatToken.width-1))
                    {
                      value.s[z] = string->data[index];
                      z++;
                    }
                    index++;
                  }
                }

                /* skip quote-char */
                index++;

                stringQuote = NULL;
                if ((formatToken.quoteChar != '\0') && (formatToken.quoteChar == string->data[index])) stringQuote = &formatToken.quoteChar;
                if (stringQuotes != NULL) stringQuote = strchr(stringQuotes,string->data[index]);
              }
              while (stringQuote != NULL);
            }
            else
            {
              if (z < (formatToken.width-1))
              {
                value.s[z] = string->data[index];
                z++;
              }
              index++;
            }
          }
          value.s[z] = '\0';
          break;
        case 'p':
        case 'n':
          break;
        case 'S':
          /* get and copy data */
          value.string = va_arg(arguments,String);
          assert(value.string != NULL);

          CHECK_VALID(value.string);

          String_clear(value.string);
          z = 0;
          while ((index < string->length) && !isspace(string->data[index]))
          {
            stringQuote = NULL;
            if ((formatToken.quoteChar != '\0') && (formatToken.quoteChar == string->data[index])) stringQuote = &formatToken.quoteChar;
            if (stringQuotes != NULL) stringQuote = strchr(stringQuotes,string->data[index]);

            if (stringQuote != NULL)
            {
              do
              {
                /* skip quote-char */
                index++;

                /* get string */
                while ((index < string->length) && (string->data[index] != (*stringQuote)))
                {
                  if (string->data[index] == '\\')
                  {
                    if (index < string->length)
                    {
                      if ((formatToken.width == 0) || (z < formatToken.width-1))
                      {
                        String_appendChar(value.string,string->data[index]);
                        z++;
                      }
                      index++;
                    }
                  }
                  else
                  {
                    if ((formatToken.width == 0) || (z < formatToken.width-1))
                    {
                      String_appendChar(value.string,string->data[index]);
                      z++;
                    }
                    index++;
                  }
                }

                /* skip quote-char */
                index++;

                stringQuote = NULL;
                if ((formatToken.quoteChar != '\0') && (formatToken.quoteChar == string->data[index])) stringQuote = &formatToken.quoteChar;
                if (stringQuotes != NULL) stringQuote = strchr(stringQuotes,string->data[index]);
              }
              while (stringQuote != NULL);
            }
            else
            {
              if ((formatToken.width == 0) || (z < formatToken.width-1))
              {
                String_appendChar(value.string,string->data[index]);
                z++;
              }
              index++;
            }
          }
          break;
#if 0
still not implemented
        case 'U':
          /* UTF8 string (as reference+index) */
          {
            jamaica_ref   stringReference;
            jamaica_int32 stringIndex;
            jamaica_int32 n;
            jamaica_int32 i;

            stringReference = va_arg(arguments,jamaica_ref);
            stringIndex     = va_arg(arguments,jamaica_int32);

            bufferCount = 0;
            if (vm != NULL)
            {
              n = utf8String_Length(vm,stringReference,stringIndex);
              for (i=0;i<n;i++)
              {
                OUTPUT_CHAR(buffer,bufferCount,(char)utf8String_CharAt(vm,stringReference,stringIndex,i));
              }
            }
            else
            {
              bufferCount=TARGET_NATIVE_MISC_FORMAT_STRING2(buffer,sizeof(buffer),"%p:%ld",stringReference,stringIndex);
              assert(bufferCount<sizeof(buffer));
            }
          }
          break;
        case 'b':
          /* binaray value */
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
                JAMAICA_HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break; /* not reached */
            #endif /* NDEBUG */
          }
          bufferCount = 0;
          FLUSH_OUTPUT();
JAMAICA_HALT_NOT_YET_IMPLEMENTED();
          break;
#endif /* 0 */
        case 'y':
          /* get data */
          z = 0;
          while (   (index < string->length)
                 && !isspace(string->data[index])
                )
          {
            if (z < sizeof(buffer)-1)
            {
              buffer[z] = string->data[index];
              z++;
            }
            index++;
          }
          buffer[z] = '\0';

          /* convert */
          value.b = va_arg(arguments,bool*);
          foundFlag = FALSE;
          z = 0;
          while (!foundFlag && (z < SIZE_OF_ARRAY(DEFAULT_TRUE_STRINGS)))
          {
            if (strcmp(buffer,DEFAULT_TRUE_STRINGS[z]) == 0)
            {
              (*value.b) = TRUE;
              foundFlag = TRUE;
            }
            z++;
          }
          z = 0;
          while (!foundFlag && (z < SIZE_OF_ARRAY(DEFAULT_FALSE_STRINGS)))
          {
            if (strcmp(buffer,DEFAULT_FALSE_STRINGS[z]) == 0)
            {
              (*value.b) = FALSE;
              foundFlag = TRUE;
            }
            z++;
          }

          if (!foundFlag)
          {
            return FALSE;
          }
          break;
        case '%':
          if ((index >= string->length) || (string->data[index] != '%'))
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
      if ((index >= string->length) || (string->data[index] != (*format)))
      {
        return FALSE;
      }
      index++;
      format++;
    }
  }
  if (nextIndex != NULL)
  {
    (*nextIndex) = index;
  }
  else
  {
    if (index < string->length)
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
* Return : -
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

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
String String_new(void)
#else /* not DEBUG */
String __String_new(const char *fileName, ulong lineNb)
#endif /* NDEBUG */
{
  struct __String *string;
  #ifndef NDEBUG
    DebugStringNode *debugStringNode;;
  #endif /* not NDEBUG */

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

  string->length    = 0;
  string->maxLength = STRING_START_LENGTH;
  string->data[0]   = '\0';

  #ifndef NDEBUG
    pthread_once(&debugStringInitFlag,debugStringInit);

    debugStringNode = LIST_NEW_NODE(DebugStringNode);
    if (debugStringNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    debugStringNode->fileName = fileName;
    debugStringNode->lineNb   = lineNb;
    debugStringNode->string   = string;
    pthread_mutex_lock(&debugStringLock);
    List_append(&debugStringList,debugStringNode);
    pthread_mutex_unlock(&debugStringLock);
  #endif /* not NDEBUG */

  UPDATE_VALID(string);

  return string;
}

#ifdef NDEBUG
String String_newCString(const char *s)
#else /* not NDEBUG */
String __String_newCString(const char *fileName, ulong lineNb, const char *s)
#endif /* NDEBUG */
{
  String string;

  #ifdef NDEBUG
    string = String_new();
  #else /* not DEBUG */
    string = __String_new(fileName,lineNb);
  #endif /* NDEBUG */
  String_setCString(string,s);

  UPDATE_VALID(string);

  return string;
}

#ifdef NDEBUG
String String_newChar(char ch)
#else /* not NDEBUG */
String __String_newChar(const char *fileName, ulong lineNb, char ch)
#endif /* NDEBUG */
{
  String string;

  #ifdef NDEBUG
    string = String_new();
  #else /* not DEBUG */
    string = __String_new(fileName,lineNb);
  #endif /* NDEBUG */
  String_setChar(string,ch);

  UPDATE_VALID(string);

  return string;
}

#ifdef NDEBUG
String String_newBuffer(const char *buffer, ulong bufferLength)
#else /* not NDEBUG */
String __String_newBuffer(const char *fileName, ulong lineNb, const char *buffer, ulong bufferLength)
#endif /* NDEBUG */
{
  String string;

  #ifdef NDEBUG
    string = String_new();
  #else /* not DEBUG */
    string = __String_new(fileName,lineNb);
  #endif /* NDEBUG */
  String_setBuffer(string,buffer,bufferLength);

  UPDATE_VALID(string);

  return string;
}

#ifdef NDEBUG
void String_delete(String string)
#else /* not NDEBUG */
void __String_delete(const char *fileName, ulong lineNb, String string)
#endif /* NDEBUG */
{
  #ifndef NDEBUG
    DebugStringNode *debugStringNode;;
  #endif /* not NDEBUG */

  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    #ifndef NDEBUG
      pthread_once(&debugStringInitFlag,debugStringInit);

      pthread_mutex_lock(&debugStringLock);
      debugStringNode = debugStringList.head;
      while ((debugStringNode != NULL) && (debugStringNode->string != string))
      {
        debugStringNode = debugStringNode->next;
      }
      if (debugStringNode != NULL)
      {
        List_remove(&debugStringList,debugStringNode);
      }
      else
      {
        fprintf(stderr,"DEBUG WARNING: string '%s' not found in debug list at %s, %ld!\n",
                string->data,
                fileName,
                lineNb
               );
HALT_INTERNAL_ERROR("");
      }
      pthread_mutex_unlock(&debugStringLock);
    #endif /* not NDEBUG */

    free(string->data);
    free(string);
  }
}

String String_clear(String string)
{
  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    string->data[0] = '\0';
    string->length = 0;
  }

  UPDATE_VALID(string);

  return string;
}

String String_set(String string, const String sourceString)
{
  ulong n;

  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (sourceString != NULL)
    {
      assert(sourceString->data != NULL);

      n = sourceString->length;
      ensureStringLength(string,n+1);
      memcpy(&string->data[0],&sourceString->data[0],sourceString->length);
      string->data[n] = '\0';
      string->length = n;
    }
    else
    {
      string->data[0] = '\0';
      string->length = 0;
    }
  }

  UPDATE_VALID(string);

  return string;
}

String String_setCString(String string, const char *s)
{
  CHECK_VALID(string);

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
      string->length = 0;
    }
  }

  UPDATE_VALID(string);

  return string;
}

String String_setChar(String string, char ch)
{
  CHECK_VALID(string);

  String_setBuffer(string,&ch,1); 

  UPDATE_VALID(string);

  return string;
}

String String_setBuffer(String string, const char *buffer, ulong bufferLength)
{
  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (buffer != NULL)
    {
      assert(buffer != NULL);

      ensureStringLength(string,bufferLength+1);
      memcpy(&string->data[0],buffer,bufferLength);
      string->data[bufferLength] = '\0';
      string->length = bufferLength;
    }
    else
    {
      string->data[0] = '\0';
      string->length = 0;
    }
  }

  UPDATE_VALID(string);

  return string;
}

#ifdef NDEBUG
String String_copy(const String fromString)
#else /* not NDEBUG */
String __String_copy(const char *fileName, ulong lineNb, String fromString)
#endif /* NDEBUG */
{
  struct __String *string;

  CHECK_VALID(fromString);

  if (fromString != NULL)
  {
    assert(fromString->data != NULL);

    #ifdef NDEBUG
      string = String_new();
    #else /* not DEBUG */
      string = __String_new(fileName,lineNb);
    #endif /* NDEBUG */
    if (string == NULL)
    {
      return NULL;
    }

    ensureStringLength(string,fromString->length);
    memcpy(&string->data[0],&fromString->data[0],fromString->length);
    string->data[fromString->length] ='\0';
    string->length = fromString->length;

    UPDATE_VALID(string);
  }
  else
  {
    string = NULL;
  }

  return string;
}

String String_sub(String string, const String fromString, ulong index, long length)
{
  ulong n;

  CHECK_VALID(string);
  CHECK_VALID(fromString);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (fromString != NULL)
    {
      assert(fromString->data != NULL);

      if (index < fromString->length)
      {
        if (index == STRING_END)
        {
          n = MIN(fromString->length,fromString->length-index);
        }
        else
        {
          n = MIN(length,fromString->length-index);
        }
        ensureStringLength(string,n);
        memcpy(&string->data[0],&fromString->data[index],n);
        string->data[n] ='\0';
        string->length = n;
      }
    }
    else
    {
      string->data[0] = '\0';
      string->length = 0;
    }
  }

  UPDATE_VALID(string);

  return string;
}

char *String_subCString(char *s, const String fromString, ulong index, long length)
{
  ulong n;

  assert(s != NULL);

  CHECK_VALID(fromString);

  if (length > 0)
  {
    if (fromString != NULL)
    {
      assert(fromString->data != NULL);

      if (index < fromString->length)
      {
        n = MIN(length-1,fromString->length-index);
        memcpy(s,&fromString->data[index],n);
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

char *String_subBuffer(char *buffer, const String fromString, ulong index, long length)
{
  ulong n;

  assert(buffer != NULL);

  CHECK_VALID(fromString);

  if (length > 0)
  {
    if (fromString != NULL)
    {
      assert(fromString->data != NULL);

      if (index < fromString->length)
      {
        n = MIN(length,fromString->length-index);
        memcpy(&buffer[0],&fromString->data[index],n);
        memset(&buffer[n],0,length-n);
      }
      else
      {
        memset(buffer,0,length);
      }
    }
    else
    {
      memset(buffer,0,length);
    }
  }

  return buffer;
}

String String_append(String string, const String appendString)
{
  ulong n;

  CHECK_VALID(string);
  CHECK_VALID(appendString);

  if (string != NULL)
  {
    assert(string->data != NULL);
    if (appendString != NULL)
    {
      n = string->length+appendString->length;
      ensureStringLength(string,n+1);
      memcpy(&string->data[string->length],&appendString->data[0],appendString->length);
      string->data[n] = '\0';
      string->length = n;
    }
  }

  UPDATE_VALID(string);

  return string;
}

String String_appendBuffer(String string, const char *buffer, ulong bufferLength)
{
  ulong n;

  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);
    if (buffer != NULL)
    {
      n = string->length+bufferLength;
      ensureStringLength(string,n+1);
      memcpy(&string->data[string->length],buffer,bufferLength);
      string->data[n] = '\0';
      string->length = n;
    }
  }

  UPDATE_VALID(string);

  return string;
}

String String_appendCString(String string, const char *s)
{
  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      String_appendBuffer(string,s,strlen(s));
    }
  }

  UPDATE_VALID(string);

  return string;
}

String String_appendChar(String string, char ch)
{
  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    String_appendBuffer(string,&ch,1);
  }

  UPDATE_VALID(string);

  return string;
}

String String_insert(String string, ulong index, const String insertString)
{
  ulong n;

  CHECK_VALID(string);
  CHECK_VALID(insertString);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (insertString != NULL)
    {
      if      (index == STRING_END)
      {
        n = string->length+insertString->length;
        ensureStringLength(string,n+1);
        memcpy(&string->data[string->length],&insertString->data[0],insertString->length);
        string->data[n] = '\0';
        string->length = n;
      }
      else if (index <= string->length)
      {
        n = string->length+insertString->length;
        ensureStringLength(string,n+1);
        memmove(&string->data[index+insertString->length],&string->data[index],string->length-index);
        memcpy(&string->data[index],&insertString->data[0],insertString->length);
        string->data[n] = '\0';
        string->length = n;
      }
    }
  }

  UPDATE_VALID(string);

  return string;
}

String String_insertBuffer(String string, ulong index, const char *buffer, ulong bufferLength)
{
  ulong n;

  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (buffer != NULL)
    {
      if      (index == STRING_END)
      {
        n = string->length+bufferLength;
        ensureStringLength(string,n+1);
        memcpy(&string->data[string->length],buffer,bufferLength);
        string->data[n] = '\0';
        string->length = n;
      }
      else if (index <= string->length)
      {
        n = string->length+bufferLength;
        ensureStringLength(string,n+1);
        memmove(&string->data[index+bufferLength],&string->data[index],string->length-index);
        memcpy(&string->data[index],buffer,bufferLength);
        string->data[n] = '\0';
        string->length = n;
      }
    }
  }

  UPDATE_VALID(string);

  return string;
}

String String_insertCString(String string, ulong index, const char *s)
{
  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      String_insertBuffer(string,index,s,strlen(s));
    }
  }

  UPDATE_VALID(string);

  return string;
}

String String_insertChar(String string, ulong index, char ch)
{
  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    String_insertBuffer(string,index,&ch,1);
  }

  UPDATE_VALID(string);

  return string;
}

String String_remove(String string, ulong index, ulong length)
{
  ulong n;

  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if      (index == STRING_END)
    {
      n = (string->length > length)?string->length-length:0;
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
  }

  UPDATE_VALID(string);

  return string;
}

String String_replace(String string, ulong index, ulong length, const String insertString)
{
  CHECK_VALID(string);
  CHECK_VALID(insertString);

  String_remove(string,index,length);
  String_insert(string,index,insertString);

  UPDATE_VALID(string);

  return string;
}

String String_replaceBuffer(String string, ulong index, ulong length, const char *buffer, ulong bufferLength)
{
  CHECK_VALID(string);

  String_remove(string,index,length);
  String_insertBuffer(string,index,buffer,bufferLength);

  UPDATE_VALID(string);

  return string;
}

String String_replaceCString(String string, ulong index, ulong length, const char *s)
{
  CHECK_VALID(string);

  String_remove(string,index,length);
  String_insertCString(string,index,s);

  UPDATE_VALID(string);

  return string;
}

String String_replaceChar(String string, ulong index, ulong length, char ch)
{
  CHECK_VALID(string);

  String_remove(string,index,length);
  String_insertChar(string,index,ch);

  UPDATE_VALID(string);

  return string;
}

ulong String_length(const String string)
{
  CHECK_VALID(string);

  return (string != NULL)?string->length:0;
}

bool String_empty(const String string)
{
  CHECK_VALID(string);

  return (string != NULL)?(string->length == 0):TRUE;
}

char String_index(const String string, ulong index)
{
  char ch;

  CHECK_VALID(string);

  if (string != NULL)
  {
    if      (index == STRING_END)
    {
      ch = (string->length > 0)?string->data[string->length-1]:'\0';
    }
    else if (index < string->length)
    {
      ch = string->data[index];
    }
    else
    {
      ch = '\0';
    }
  }
  else
  {
    ch = '\0';
  }

  return ch;
}

const char *String_cString(const String string)
{
  CHECK_VALID(string);

  return (string != NULL)?&string->data[0]:NULL;
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

  CHECK_VALID(string1);
  CHECK_VALID(string2);

  result = 0;
  n = MIN(string1->length,string2->length);
  z = 0;
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
  bool  equalFlag;
  ulong z;

  assert(string1 != NULL);
  assert(string2 != NULL);

  CHECK_VALID(string1);
  CHECK_VALID(string2);

  if (string1->length == string2->length)
  {
    equalFlag = TRUE;
    z         = 0;
    while (equalFlag && (z < string1->length))
    {
      equalFlag = (string1->data[z] == string2->data[z]);
      z++;
    }
  }
  else
  {
    equalFlag = FALSE;
  }

  return equalFlag;
}

bool String_equalsCString(const String string, const char *s)
{
  struct __String cString;

  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      cString.length = strlen(s);
      cString.data   = (char*)s;

      UPDATE_VALID(&cString);

      return String_equals(string,&cString);
    }
    else
    {
      return (string->length == 0);
    }
  }
  else
  {
    return (s == NULL);
  }
}

bool String_equalsChar(const String string, char ch)
{
  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    return ((string->length == 1) && (string->data[0] == ch));
  }
  else
  {
    return FALSE;
  }
}

long String_find(const String string, ulong index, const String findString)
{
  long z,i;
  long findIndex;

  assert(string != NULL);
  assert(findString != NULL);

  CHECK_VALID(string);
  CHECK_VALID(findString);

  findIndex = -1;

  z = (index != STRING_BEGIN)?index:0;
  while (((z+findString->length) < string->length) && (findIndex < 0))
  {
    i = 0;
    while ((i < findString->length) && (string->data[z+i] == findString->data[i]))
    {
      i++;
    }
    if (i >=  findString->length) findIndex = z;

    z++;
  }

  return findIndex;
}

long String_findCString(const String string, ulong index, const char *s)
{
  long findIndex;
  long sLength;
  long z,i;

  assert(string != NULL);
  assert(s != NULL);

  CHECK_VALID(string);

  findIndex = -1;

  sLength = strlen(s);
  z = (index != STRING_BEGIN)?index:0;
  while (((z+sLength) < string->length) && (findIndex < 0))
  {
    i = 0;
    while ((i < sLength) && (string->data[z+i] == s[i]))
    {
      i++;
    }
    if (i >=  sLength) findIndex = z;

    z++;
  }

  return findIndex;
}

long String_findChar(const String string, ulong index, char ch)
{
  long z;

  assert(string != NULL);

  CHECK_VALID(string);

  z = (index != STRING_BEGIN)?index:0;
  while ((z < string->length) && (string->data[z] != ch))
  {
    z++;
  }

  return (z < string->length)?z:-1;
}

long String_findLast(const String string, long index, String findString)
{
  long z,i;
  long findIndex;

  assert(string != NULL);
  assert(findString != NULL);

  CHECK_VALID(string);

  findIndex = -1;

  z = (index != STRING_END)?index:string->length-1;
  while ((z >= 0) && (findIndex < 0))
  {
    i = 0;
    while ((i < findString->length) && (string->data[z+i] == findString->data[i]))
    {
      i++;
    }
    if (i >=  findString->length) findIndex = z;

    z--;
  }

  return findIndex;
}

long String_findLastCString(const String string, long index, const char *s)
{
  long findIndex;
  long sLength;
  long z,i;

  assert(string != NULL);
  assert(s != NULL);

  CHECK_VALID(string);

  findIndex = -1;

  sLength = strlen(s);
  z = (index != STRING_END)?index:string->length-1;
  while ((z >= 0) && (findIndex < 0))
  {
    i = 0;
    while ((i < sLength) && (string->data[z+i] == s[i]))
    {
      i++;
    }
    if (i >=  sLength) findIndex = z;

    z--;
  }

  return findIndex;
}

long String_findLastChar(const String string, long index, char ch)
{
  long z;

  assert(string != NULL);

  CHECK_VALID(string);

  z = (index != STRING_END)?index:string->length-1;
  while ((z >= 0) && (string->data[z] != ch))
  {
    z--;
  }

  return (z >= 0)?z:-1;
}

String String_iterate(const                 String string,
                      StringIterateFunction stringIterateFunction,
                      void                  *stringIterateUserData
                     )
{
  ulong z;

  assert(stringIterateFunction != NULL);

  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    for (z = 0; z < string->length; z++)
    {
      string->data[z] = stringIterateFunction(stringIterateUserData,string->data[z]);
    }
  }

  UPDATE_VALID(string);

  return string;
}

String String_toLower(String string)
{
  ulong z;

  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    for (z = 0; z < string->length; z++)
    {
      string->data[z] = tolower(string->data[z]);
    }
  }

  UPDATE_VALID(string);

  return string;
}

String String_toUpper(String string)
{
  ulong z;

  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    for (z = 0; z < string->length; z++)
    {
      string->data[z] = toupper(string->data[z]);
    }
  }

  UPDATE_VALID(string);

  return string;
}

String String_trim(String string, const char *chars)
{
  CHECK_VALID(string);

  String_trimRight(string,chars);
  String_trimLeft(string,chars);

  UPDATE_VALID(string);

  return string;
}

String String_trimRight(String string, const char *chars)
{
  ulong n;

  CHECK_VALID(string);

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
  }

  UPDATE_VALID(string);

  return string;
}

String String_trimLeft(String string, const char *chars)
{
  ulong z,n;

  CHECK_VALID(string);

  if (string != NULL)
  {
    assert(string->data != NULL);

    z = 0;
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
  }

  UPDATE_VALID(string);

  return string;
}

String String_padRight(String string, ulong length, char ch)
{
  ulong n;

  assert(string != NULL);

  CHECK_VALID(string);

  if (string->length < length)
  {
    n = length-string->length;
    ensureStringLength(string,length+1);
    memset(&string->data[string->length],ch,n);
    string->data[length] = '\0';
    string->length = length;
  }

  UPDATE_VALID(string);

  return string;
}

String String_padLeft(String string, ulong length, char ch)
{
  ulong n;

  assert(string != NULL);

  CHECK_VALID(string);

  if (string->length < length)
  {
    n = length-string->length;
    ensureStringLength(string,length+1);
    memmove(&string->data[n],&string->data[0],string->length);
    memset(&string->data[0],ch,n);
    string->data[length] = '\0';
    string->length = length;
  }

  UPDATE_VALID(string);

  return string;
}

String String_format(String string, const char *format, ...)
{
  va_list arguments;

  assert(string != NULL);
  assert(format != NULL);

  CHECK_VALID(string);

  va_start(arguments,format);
  formatString(string,format,arguments);
  va_end(arguments);

  UPDATE_VALID(string);

  return string;
}

String String_vformat(String string, const char *format, va_list arguments)
{
  assert(string != NULL);
  assert(format != NULL);

  CHECK_VALID(string);

  formatString(string,format,arguments);

  UPDATE_VALID(string);

  return string;
}

void String_initTokenizer(StringTokenizer *stringTokenizer,
                          const String    string,
                          const char      *separatorChars,
                          const char      *stringQuotes,
                          bool            skipEmptyTokens
                         )
{
  assert(stringTokenizer != NULL);
  assert(string != NULL);

  CHECK_VALID(string);

  stringTokenizer->string          = string;
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
  assert(stringTokenizer->string != NULL);

  String_delete(stringTokenizer->token);
}

bool String_getNextToken(StringTokenizer *stringTokenizer, String *const token, long *tokenIndex)
{
  const char *s;

  assert(stringTokenizer != NULL);

  /* check index */
  if (stringTokenizer->index >= stringTokenizer->string->length)
  {
    return FALSE;
  }

  if (stringTokenizer->skipEmptyTokens)
  {
    /* skip separator chars */
    while (   (stringTokenizer->index < stringTokenizer->string->length)
           && (strchr(stringTokenizer->separatorChars,stringTokenizer->string->data[stringTokenizer->index]) != NULL)
          )
    {
      stringTokenizer->index++;
    }
    if (stringTokenizer->index >= stringTokenizer->string->length) return FALSE;
  }

  /* get token */
  if (tokenIndex != NULL) (*tokenIndex) = stringTokenizer->index;
  String_clear(stringTokenizer->token);
  if (stringTokenizer->stringQuotes != NULL)
  {
    while (   (stringTokenizer->index < stringTokenizer->string->length)
           && (strchr(stringTokenizer->separatorChars,stringTokenizer->string->data[stringTokenizer->index]) == NULL)
          )
    {
      s = strchr(stringTokenizer->stringQuotes,stringTokenizer->string->data[stringTokenizer->index]);
      if (s != NULL)
      {
        stringTokenizer->index++;
        while (   (stringTokenizer->index < stringTokenizer->string->length)
               && (stringTokenizer->string->data[stringTokenizer->index] != (*s))
              )
        {
          String_appendChar(stringTokenizer->token,stringTokenizer->string->data[stringTokenizer->index]);
          stringTokenizer->index++;
        }
        if (stringTokenizer->index < stringTokenizer->string->length) stringTokenizer->index++;
      }
      else
      {
        String_appendChar(stringTokenizer->token,stringTokenizer->string->data[stringTokenizer->index]);
        stringTokenizer->index++;
      }
    }
  }
  else
  {
    while (   (stringTokenizer->index < stringTokenizer->string->length)
           && (strchr(stringTokenizer->separatorChars,stringTokenizer->string->data[stringTokenizer->index]) == NULL)
          )
    {
      String_appendChar(stringTokenizer->token,stringTokenizer->string->data[stringTokenizer->index]);
      stringTokenizer->index++;
    }
  }
  if (token != NULL) (*token) = stringTokenizer->token;

  /* skip token separator */
  if (   (stringTokenizer->index < stringTokenizer->string->length)
      && (strchr(stringTokenizer->separatorChars,stringTokenizer->string->data[stringTokenizer->index]) != NULL)
     )
  {
    stringTokenizer->index++;
  }

  return TRUE;
}

bool String_scan(const String string, const char *format, ...)
{
  va_list arguments;
  bool    result;

  assert(string != NULL);
  assert(format != NULL);

  CHECK_VALID(string);

  va_start(arguments,format);
  result = parseString(string,format,arguments,NULL,NULL);
  va_end(arguments);

  return result;
}

bool String_parse(const String string, const char *format, ulong *nextIndex, ...)
{
  va_list arguments;
  bool    result;

  assert(string != NULL);
  assert(format != NULL);

  CHECK_VALID(string);

  va_start(arguments,nextIndex);
  result = parseString(string,format,arguments,STRING_QUOTES,nextIndex);
  va_end(arguments);

  return result;
}

int String_toInteger(const String convertString, ulong index, long *nextIndex, const StringUnit stringUnits[], uint stringUnitCount)
{
  int  n;
  char *nextData;

  assert(convertString != NULL);

  CHECK_VALID(convertString);

  if (index < convertString->length)
  {
    n = strtol(&convertString->data[index],&nextData,0);
    if ((ulong)(nextData-convertString->data) < convertString->length)
    {
      n = n*getUnitFactor(stringUnits,stringUnitCount,convertString->data,nextData,nextIndex);
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
      n = n*(int64)getUnitFactor(stringUnits,stringUnitCount,convertString->data,nextData,nextIndex);
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

  CHECK_VALID(convertString);

  if (index < convertString->length)
  {
    n = strtod(&convertString->data[index],&nextData);
    if ((ulong)(nextData-convertString->data) < convertString->length)
    {
      n = n*(double)getUnitFactor(stringUnits,stringUnitCount,convertString->data,nextData,nextIndex);
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

  CHECK_VALID(convertString);

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
      stringQuote = (stringQuotes != NULL)?strchr(stringQuotes,convertString->data[index]):NULL;
      if (stringQuote != NULL)
      {
        do
        {
          /* skip string-char */
          index++;
          /* get string */
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
          /* skip string-char */
          index++;
          /* next string char */
          stringQuote = ((stringQuotes != NULL) && (index < convertString->length))?strchr(stringQuotes,convertString->data[index]):NULL;
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

  CHECK_VALID(string);

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
void String_debug(void)
{
  #ifndef NDEBUG
    DebugStringNode *debugStringNode;
  #endif /* not NDEBUG */

  #ifndef NDEBUG
    pthread_once(&debugStringInitFlag,debugStringInit);

    pthread_mutex_lock(&debugStringLock);
    for (debugStringNode = debugStringList.head; debugStringNode != NULL; debugStringNode = debugStringNode->next)
    {
      fprintf(stderr,"DEBUG WARNING: string '%s' allocated at %s, %ld!\n",
              debugStringNode->string->data,
              debugStringNode->fileName,
              debugStringNode->lineNb
             );
    }
    pthread_mutex_unlock(&debugStringLock);
  #endif /* not NDEBUG */
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
