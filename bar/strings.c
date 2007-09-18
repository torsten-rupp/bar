/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/strings.c,v $
* $Revision: 1.6 $
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
  #include "lists.h"
#endif /* not NDEBUG */

#include "strings.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define STRING_START_LENGTH 64   // string start length
#define STRING_DELTA_LENGTH 32   // string delta increasing/decreasing

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
  char             conversionChar;
} FormatToken;

struct __String
{
  unsigned long length;
  unsigned long maxLength;
  char          *data;
};

#ifndef NDEBUG
  typedef struct DebugStringNode
  {
    NODE_HEADER(struct DebugStringNode);

    const char      *fileName;
    unsigned long   lineNb;
    struct __String *string;
  } DebugStringNode;

  typedef struct
  {
    LIST_HEADER(DebugStringNode);
  } DebugStringList;
#endif /* not NDEBUG */

/***************************** Variables *******************************/
#ifndef NDEBUG
  DebugStringList debugStringList = LIST_STATIC_INIT;
#endif /* not NDEBUG */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : ensureStringLength
* Purpose: ensure min. length of string
* Input  : string    - string
*          newLength - new min. length of string
* Output : -
* Return : TRUE if string length is ok, FALSE on insufficient memory
* Notes  : -
\***********************************************************************/

LOCAL void ensureStringLength(struct __String *string, unsigned long newLength)
{
  char          *newData;
  unsigned long newMaxLength;

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
* Purpose: format a string (like printf)
* Input  : String    - string
*          format    - format string
*          arguments - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void formatString(struct __String *string,
                        const char      *format,
                        va_list         arguments)
{
  FormatToken  formatToken;
  union
  {
    long int          i;
    unsigned long int u;
    double            d;
    const char        *s;
    void              *p;
    struct __String   *string;
  } data;
  char         buffer[64];
  unsigned int length;

  while ((*format) != '\0')
  {
    if ((*format) == '%')
    {
      /* get format token */
      format = parseNextFormatToken(format,&formatToken);

      /* format and store string */
      switch (formatToken.conversionChar)
      {
        case 'i':
        case 'd':
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
        case 'o':
        case 'u':
        case 'x':
        case 'X':
          data.u = va_arg(arguments,unsigned int);
          length = snprintf(buffer,sizeof(buffer),formatToken.token,data.u);
          if (length < sizeof(buffer))
          {
            String_appendCString(string,buffer);
          }
          else
          {
            ensureStringLength(string,string->length+length);
            snprintf(&string->data[string->length],sizeof(buffer),formatToken.token,data.u);
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
          break;
        case 'p':
        case 'n':
          data.p = va_arg(arguments,void*);
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
            #ifdef HAVE_LONG_LONG
              case FORMAT_LENGTH_TYPE_LONGLONG:
                {
                  unsigned long long bits;

                  bits = va_arg(arguments,unsigned long long);
                }
                break;
            #endif /* HAVE_LONG_LONG */
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
* Purpose: parse a string (like sscanf)
* Input  : String    - string
*          format    - format string
*          arguments - arguments
* Output : TRUE if parsing sucessful, FALSE otherwise
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool parseString(struct __String *string,
                       const char      *format,
                       va_list         arguments)
{
  unsigned long   index;
  FormatToken     formatToken;
  union
  {
    int             *i;
    long int        *l;
    long long int   *ll;
    double          *d;
    char            *c;
    char            *s;
    void            *p;
    struct __String *string;
  } value;
  char          buffer[64];
  unsigned long z;

  index = 0;
  while ((*format) != '\0')
  {
    if ((*format) == '%')
    {
      /* skip white-spaces */
      while ((index < string->length) && isspace(string->data[index]))
      {
        index++;
      }

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
              (*value.ll) = strtol(buffer,NULL,10);
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

          value.d = va_arg(arguments,double*);
          assert(value.d != NULL);
          (*value.d) = strtod(buffer,NULL);
          break;
        case 's':
          /* get and copy data */
          value.s = va_arg(arguments,char*);
          assert(value.s != NULL);
          z = 0;
          while (   (index < string->length)
                 && ((formatToken.width == 0) || (z < formatToken.width-1))
                 && !isspace(string->data[index])
                 && (string->data[index] != (*format))
                )
          {
            value.s[z] = string->data[index];
            z++;
            index++;
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
          String_clear(value.string);
          while ((index < string->length) && !isspace(string->data[index]))
          {
            String_appendChar(value.string,string->data[index]);
            index++;
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
            #ifdef HAVE_LONG_LONG
              case FORMAT_LENGTH_TYPE_LONGLONG:
                {
                  unsigned long long bits;

                  bits = va_arg(arguments,unsigned long long);
                }
                break;
            #endif /* HAVE_LONG_LONG */
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

  return TRUE;
}

#ifndef NDEBUG
#endif /* not NDEBUG */

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
String String_new(void)
#else /* not DEBUG */
String __String_new(const char *fileName, unsigned long lineNb)
#endif /* NDEBUG */
{
  struct __String *string;
  #ifndef NDEBUG
    DebugStringNode *debugStringNode;;
  #endif /* not NDEBUG */

  string = (struct __String*)malloc(sizeof(struct __String));
  if (string == NULL)
  {
    return NULL;
  }
  string->data = (char*)malloc(STRING_START_LENGTH);
  if (string->data == NULL)
  {
    free(string);
    return NULL;
  }

  string->length    = 0;
  string->maxLength = STRING_START_LENGTH;
  string->data[0]   = '\0';

  #ifndef NDEBUG
    debugStringNode = LIST_NEW_NODE(DebugStringNode);
    if (debugStringNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    debugStringNode->fileName = fileName;
    debugStringNode->lineNb   = lineNb;
    debugStringNode->string   = string;
    List_append(&debugStringList,debugStringNode);
  #endif /* not NDEBUG */

  return string;
}

#ifdef NDEBUG
String String_newCString(const char *s)
#else /* not NDEBUG */
String __String_newCString(const char *fileName, unsigned long lineNb, const char *s)
#endif /* NDEBUG */
{
  String string;

  #ifdef NDEBUG
    string = String_new();
  #else /* not DEBUG */
    string = __String_new(fileName,lineNb);
  #endif /* NDEBUG */
  String_setCString(string,s);

  return string;
}

#ifdef NDEBUG
String String_newChar(char ch)
#else /* not NDEBUG */
String __String_newChar(const char *fileName, unsigned long lineNb, char ch)
#endif /* NDEBUG */
{
  String string;

  #ifdef NDEBUG
    string = String_new();
  #else /* not DEBUG */
    string = __String_new(fileName,lineNb);
  #endif /* NDEBUG */
  String_setChar(string,ch);

  return string;
}

#ifdef NDEBUG
String String_newBuffer(char *buffer, ulong bufferLength)
#else /* not NDEBUG */
String __String_newBuffer(const char *fileName, unsigned long lineNb, char *buffer, ulong bufferLength)
#endif /* NDEBUG */
{
  String string;

  #ifdef NDEBUG
    string = String_new();
  #else /* not DEBUG */
    string = __String_new(fileName,lineNb);
  #endif /* NDEBUG */
  String_setBuffer(string,buffer,bufferLength);

  return string;
}

void String_delete(String string)
{
  #ifndef NDEBUG
    DebugStringNode *debugStringNode;;
  #endif /* not NDEBUG */

  if (string != NULL)
  {
    assert(string->data != NULL);

    #ifndef NDEBUG
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
        fprintf(stderr,"DEBUG WARNING: string '%s' not found in debug list!\n",
                string->data
               );
      }
    #endif /* not NDEBUG */

    free(string->data);
    free(string);
  }
}

String String_clear(String string)
{
  if (string != NULL)
  {
    assert(string->data != NULL);

    string->data[0] = '\0';
    string->length = 0;
  }

  return string;
}

String String_set(String string, String sourceString)
{
  unsigned long n;

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

  return string;
}

String String_setCString(String string, const char *s)
{
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

  return string;
}

String String_setChar(String string, char ch)
{
  String_setBuffer(string,&ch,1); 

  return string;
}

String String_setBuffer(String string, const char *buffer, ulong bufferLength)
{
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

  return string;
}

#ifdef NDEBUG
String String_copy(String fromString)
#else /* not NDEBUG */
String __String_copy(const char *fileName, unsigned long lineNb, String fromString)
#endif /* NDEBUG */
{
  struct __String *string;

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
  }

  return string;
}

String String_sub(String string, String fromString, unsigned long index, long length)
{
  unsigned long n;

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (fromString != NULL)
    {
      assert(fromString->data != NULL);

      if (index < fromString->length)
      {
        n = MIN(length,fromString->length-index);
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

  return string;
}

String String_append(String string, String appendString)
{
  unsigned long n;

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

  return string;
}

String String_appendBuffer(String string, const char *buffer, ulong bufferLength)
{
  unsigned long n;

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

  return string;
}

String String_appendCString(String string, const char *s)
{
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
  if (string != NULL)
  {
    assert(string->data != NULL);

    String_appendBuffer(string,&ch,1);
  }

  return string;
}

String String_insert(String string, unsigned long index, String insertString)
{
  unsigned long n;

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

  return string;
}

String String_insertBuffer(String string, unsigned long index, const char *buffer, ulong bufferLength)
{
  unsigned long n;

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

  return string;
}

String String_insertCString(String string, unsigned long index, const char *s)
{
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

String String_insertChar(String string, unsigned long index, char ch)
{
  if (string != NULL)
  {
    assert(string->data != NULL);

    String_insertBuffer(string,index,&ch,1);
  }

  return string;
}

String String_remove(String string, unsigned long index, unsigned long length)
{
  unsigned long n;

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (index < string->length)
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

  return string;
}

String String_replace(String string, unsigned long index, unsigned long length, String insertString)
{
  String_remove(string,index,length);
  String_insert(string,index,insertString);

  return string;
}

String String_replaceBuffer(String string, unsigned long index, unsigned long length, const char *buffer, ulong bufferLength)
{
  String_remove(string,index,length);
  String_insertBuffer(string,index,buffer,bufferLength);

  return string;
}

String String_replaceCString(String string, unsigned long index, unsigned long length, const char *s)
{
  String_remove(string,index,length);
  String_insertCString(string,index,s);

  return string;
}

String String_replaceChar(String string, unsigned long index, unsigned long length, char ch)
{
  String_remove(string,index,length);
  String_insertChar(string,index,ch);

  return string;
}

unsigned long String_length(String string)
{
  return (string != NULL)?string->length:0;
}

char String_index(String string, unsigned long index)
{
  return ((string != NULL) && (index < string->length))?string->data[index]:'\0';
}

const char *String_cString(String string)
{
  return (string != NULL)?&string->data[0]:NULL;
}

int String_compare(String string1, String string2, StringCompareFunction stringCompareFunction, void *userData)
{
  unsigned long n;
  unsigned long z;
  int           result;

  assert(string1 != NULL);
  assert(string2 != NULL);

  result = 0;
  n = MIN(string1->length,string2->length);
  z = 0;
  if (stringCompareFunction != NULL)
  {
    while ((result == 0) && (z < n))
    {
      result = stringCompareFunction(userData,string1->data[z],string2->data[z]);
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

bool String_equals(String string1, String string2)
{
  bool          equalFlag;
  unsigned long z;

  assert(string1 != NULL);
  assert(string2 != NULL);

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

bool String_equalsCString(String string, const char *s)
{
  struct __String cString;

  if (string != NULL)
  {
    assert(string->data != NULL);

    if (s != NULL)
    {
      cString.length = strlen(s);
      cString.data   = (char*)s;

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

bool String_equalsChar(String string, char ch)
{
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

long String_find(String string, unsigned long index, String findString)
{
  long z,i;
  long findIndex;

  assert(string != NULL);
  assert(findString != NULL);

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

long String_findCString(String string, unsigned long index, const char *s)
{
  long findIndex;
  long sLength;
  long z,i;

  assert(string != NULL);
  assert(s != NULL);

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

long String_findChar(String string, unsigned long index, char ch)
{
  long z;

  assert(string != NULL);

  z = (index != STRING_BEGIN)?index:0;
  while ((z < string->length) && (string->data[z] != ch))
  {
    z++;
  }

  return (z < string->length)?z:-1;
}

long String_findLast(String string, long index, String findString)
{
  long z,i;
  long findIndex;

  assert(string != NULL);
  assert(findString != NULL);

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

long String_findLastCString(String string, long index, const char *s)
{
  long findIndex;
  long sLength;
  long z,i;

  assert(string != NULL);
  assert(s != NULL);

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

long String_findLastChar(String string, long index, char ch)
{
  long z;

  assert(string != NULL);

  z = (index != STRING_END)?index:string->length-1;
  while ((z >= 0) && (string->data[z] != ch))
  {
    z--;
  }

  return (z >= 0)?z:-1;
}

String String_iterate(String string, StringIterateFunction stringIterateFunction, void *userData)
{
  unsigned long z;

  assert(stringIterateFunction != NULL);

  if (string != NULL)
  {
    assert(string->data != NULL);

    for (z = 0; z < string->length; z++)
    {
      string->data[z] = stringIterateFunction(userData,string->data[z]);
    }
  }

  return string;
}

String String_toLower(String string)
{
  unsigned long z;

  if (string != NULL)
  {
    assert(string->data != NULL);

    for (z = 0; z < string->length; z++)
    {
      string->data[z] = tolower(string->data[z]);
    }
  }

  return string;
}

String String_toUpper(String string)
{
  unsigned long z;

  if (string != NULL)
  {
    assert(string->data != NULL);

    for (z = 0; z < string->length; z++)
    {
      string->data[z] = toupper(string->data[z]);
    }
  }

  return string;
}

String String_rightPad(String string, unsigned long length, char ch)
{
  unsigned long n;

  assert(string != NULL);

  if (string->length < length)
  {
    n = length-string->length;
    ensureStringLength(string,length+1);
    memset(&string->data[string->length],ch,n);
    string->data[length] = '\0';
    string->length = length;
  }

  return string;
}

String String_leftPad(String string, unsigned long length, char ch)
{
  unsigned long n;

  assert(string != NULL);

  if (string->length < length)
  {
    n = length-string->length;
    ensureStringLength(string,length+1);
    memmove(&string->data[n],&string->data[0],string->length);
    memset(&string->data[0],ch,n);
    string->data[length] = '\0';
    string->length = length;
  }

  return string;
}

String String_format(String string, const char *format, ...)
{
  va_list arguments;

  va_start(arguments,format);
  formatString(string,format,arguments);
  va_end(arguments);

  return string;
}

void String_initTokenizer(StringTokenizer *stringTokenizer,
                          const String    string,
                          const char      *separatorChars,
                          const char      *stringChars,
                          bool            skipEmptyTokens
                         )
{
  assert(stringTokenizer != NULL);

  stringTokenizer->string          = string;
  stringTokenizer->index           = 0;
  stringTokenizer->separatorChars  = separatorChars;
  stringTokenizer->stringChars     = stringChars;
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
  if (stringTokenizer->stringChars != NULL)
  {
    while (   (stringTokenizer->index < stringTokenizer->string->length)
           && (strchr(stringTokenizer->separatorChars,stringTokenizer->string->data[stringTokenizer->index]) == NULL)
          )
    {
      s = strchr(stringTokenizer->stringChars,stringTokenizer->string->data[stringTokenizer->index]);
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
        if (stringTokenizer->index >= stringTokenizer->string->length) stringTokenizer->index++;
      }
      else
      {
        String_appendChar(stringTokenizer->token,stringTokenizer->string->data[stringTokenizer->index]);
        stringTokenizer->index++;
      }

      stringTokenizer->index++;
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

bool String_parse(String string, const char *format, ...)
{
  va_list arguments;
  bool    result;

  va_start(arguments,format);
  result = parseString(string,format,arguments);
  va_end(arguments);

  return result;
}

#ifndef NDEBUG
void String_debug(void)
{
  #ifndef NDEBUG
    DebugStringNode *debugStringNode;;
  #endif /* not NDEBUG */

  #ifndef NDEBUG
    for (debugStringNode = debugStringList.head; debugStringNode != NULL; debugStringNode = debugStringNode->next)
    {
      fprintf(stderr,"DEBUG WARNING: string '%s' allocated at %s, %ld!\n",
              debugStringNode->string->data,
              debugStringNode->fileName,
              debugStringNode->lineNb
             );
    }
  #endif /* not NDEBUG */
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
