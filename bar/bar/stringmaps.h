/***********************************************************************\
*
* $Revision: 1092 $
* $Date: 2013-07-28 05:34:57 +0200 (Sun, 28 Jul 2013) $
* $Author: torsten $
* Contents: string map functions
* Systems: all
*
\***********************************************************************/

#ifndef __STRINGMAPS__
#define __STRINGMAPS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// string map types
typedef enum
{
  STRINGMAP_TYPE_NONE,

  STRINGMAP_TYPE_INT,
  STRINGMAP_TYPE_INT64,
  STRINGMAP_TYPE_UINT,
  STRINGMAP_TYPE_UINT64,
  STRINGMAP_TYPE_DOUBLE,
  STRINGMAP_TYPE_BOOL,
  STRINGMAP_TYPE_CHAR,
  STRINGMAP_TYPE_CSTRING,
  STRINGMAP_TYPE_STRING,
  STRINGMAP_TYPE_DATA
} StringMapTypes;

/***************************** Datatypes *******************************/

// string map type
typedef struct
{
  const char     *name;
  StringMapTypes type;
} StringMapType;

// string map value
typedef struct
{
  String text;
  union
  {
    int    i;
    int64  l;
    uint   ui;
    uint64 ul;
    double d;
    bool   b;
    char   c;
    char   *s;
    String string;
    void   *p;
  } data;
} StringMapValue;

extern const StringMapValue STRINGMAP_VALUE_NONE;

// string map entry
typedef struct
{
  char           *name;
  StringMapTypes type;
  StringMapValue value;
  #ifndef NDEBUG
    const char *fileName;
    ulong      lineNb;
  #endif /* NDEBUG */
} StringMapEntry;

// string map
typedef struct __StringMap* StringMap;

// format/convert value
typedef String(*StringMapFormatFunction)(void *value, void *userData);
typedef bool(*StringMapParseFunction)(const String string, void *data, void *userData);

// convert to enum value
typedef bool(*StringMapParseEnumFunction)(const char*, int *value);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define StringMap_new()                                                                        __StringMap_new(__FILE__,__LINE__)
  #define StringMap_duplicate(stringMap)                                                         __StringMap_duplicate(__FILE__,__LINE__,stringMap)
  #define StringMap_delete(stringMap)                                                            __StringMap_delete(__FILE__,__LINE__,stringMap)
  #define StringMap_putText(stringMap,name,value)                                                __StringMap_putText(__FILE__,__LINE__,stringMap,name,text)
  #define StringMap_putTextCString(stringMap,name,value)                                         __StringMap_putTextCString(__FILE__,__LINE__,stringMap,name,text)
  #define StringMap_put(stringMap,name,value)                                                    __StringMap_put(__FILE__,__LINE__,stringMap,name,value)
  #define StringMap_putInt(stringMap,name,data)                                                  __StringMap_putInt(__FILE__,__LINE__,stringMap,name,data)
  #define StringMap_putInt64(stringMap,name,data)                                                __StringMap_putInt64(__FILE__,__LINE__,stringMap,name,data)
  #define StringMap_putUInt(stringMap,name,data)                                                 __StringMap_putUInt(__FILE__,__LINE__,stringMap,name,data)
  #define StringMap_putUInt64(stringMap,name,data)                                               __StringMap_putUInt64(__FILE__,__LINE__,stringMap,name,data)
  #define StringMap_putDouble(stringMap,name,data)                                               __StringMap_putDouble(__FILE__,__LINE__,stringMap,name,data)
  #define StringMap_putChar(stringMap,name,data)                                                 __StringMap_putChar(__FILE__,__LINE__,stringMap,name,data)
  #define StringMap_putCString(stringMap,name,data)                                              __StringMap_putCString(__FILE__,__LINE__,stringMap,name,data)
  #define StringMap_putString(stringMap,name,data)                                               __StringMap_putString(__FILE__,__LINE__,stringMap,name,data)
  #define StringMap_putData(stringMap,name,data,stringMapFormatFunction,stringMapFormatUserData) __StringMap_putData(__FILE__,__LINE__,stringMap,name,data,stringMapFormatFunction,stringMapFormatUserData)
  #define StringMap_remove(stringMap,name)                                                       __StringMap_remove(__FILE__,__LINE__,stringMap,name)
#endif /* not NDEBUG */

/***********************************************************************\
* Name   : STRINGMAP_ITERATE
* Purpose: iterate over string map
* Input  : stringMap       - string map
*          iteratorVariable - iterator variable (type int)
*          name             - iteration name (must not be initalised!)
*          value            - iteration value (must not be initalised!)
* Output : -
* Return : -
* Notes  : variable will contain all strings in list
*          usage:
*            int iteratorVariable;
*            const char *name;
*            void       *value;
*
*            STRINGLIST_ITERATE(stringMap,iteratorVariable,name,value)
*            {
*              ... = name
*              ... = value.i
*            }
\***********************************************************************/

#define STRINGMAP_ITERATE(stringMap,iteratorVariable,name_,value_) \
  for ((iteratorVariable) = 0, name_ = StringMap_indexName(stringMap,0), value_ = StringMap_indexValue(stringMap,0); \
       (iteratorVariable) < StringMap_count(stringMap); \
       (iteratorVariable)++, name_ = StringMap_indexName(stringMap,iteratorVariable), value_ = StringMap_indexValue(stringMap,iteratorVariable) \
      )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : StringMap_new
* Purpose: allocate new string map
* Input  : -
* Output : -
* Return : string map or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
StringMap StringMap_new(void);
#else /* not NDEBUG */
StringMap __StringMap_new(const char *__fileName__, ulong __lineNb__);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : StringMap_duplicate
* Purpose: duplicate string map
* Input  : stringMap - string map to duplicate (strings will be copied!)
* Output : -
* Return : string map or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
StringMap StringMap_duplicate(const StringMap *stringMap);
#else /* not NDEBUG */
StringMap __StringMap_duplicate(const char *__fileName__, ulong __lineNb__, const StringMap stringMap);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : StringMap_copy
* Purpose: copy sting list
* Input  : stringMap     - string map
*          fromStringMap - string map to copy (strings will be copied!)
* Output : -
* Return : string map or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

void StringMap_copy(StringMap stringMap, const StringMap fromStringMap);

/***********************************************************************\
* Name   : StringMap_delete
* Purpose: free all strings and delete string map
* Input  : stringMap - list to free
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void StringMap_delete(StringMap stringMap);
#else /* not NDEBUG */
void __StringMap_delete(const char *__fileName__, ulong __lineNb__, StringMap stringMap);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : StringMap_clear
* Purpose: remove all entry in list
* Input  : stringMap - string map
* Output : -
* Return : string map
* Notes  : -
\***********************************************************************/

StringMap StringMap_clear(StringMap stringMap);

/***********************************************************************\
* Name   : StringMap_isEmpty
* Purpose: check if list is empty
* Input  : stringMap - string map
* Output : -
* Return : TRUE if string map is empty, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool StringMap_isEmpty(const StringMap stringMap);
#if defined(NDEBUG) || defined(__STRINGLISTS_IMPLEMENATION__)
INLINE bool StringMap_isEmpty(const StringMap stringMap)
{
  assert(stringMap != NULL);

  return StringMap_count(stringMap) == 0;
}
#endif /* NDEBUG || __STRINGLISTS_IMPLEMENATION__ */

/***********************************************************************\
* Name   : StringMap_count
* Purpose: get number of elements in list
* Input  : stringMap - string map
* Output : -
* Return : number of elements in string map
* Notes  : -
\***********************************************************************/

uint StringMap_count(const StringMap stringMap);

const StringMapEntry *StringMap_index(const StringMap stringMap, uint index);
const char *StringMap_indexName(const StringMap stringMap, uint index);
StringMapValue StringMap_indexValue(const StringMap stringMap, uint index);

/***********************************************************************\
* Name   : StringMap_putText, StringMap_putTextCString
* Purpose: put text into map
* Input  : stringMap - string map
*          name      - name
*          text      - text
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void StringMap_putText(StringMap stringMap, const char *name, String text);
void StringMap_putTextCString(StringMap stringMap, const char *name, const char *text);
#else /* not NDEBUG */
void __StringMap_putText(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, String text);
void __StringMap_putTextCString(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, const char *text);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : StringMap_put
* Purpose: put value into map
* Input  : stringMap - string map
*          name      - name
*          value     - value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void StringMap_put(StringMap stringMap, const char *name, void *value);
#else /* not NDEBUG */
void __StringMap_put(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, void *value);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : StringMap_put*
* Purpose: put data into map
* Input  : stringMap - string map
*          name      - name
*          data      - data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void StringMap_putInt(StringMap stringMap, const char *name, int data);
void StringMap_putInt64(StringMap stringMap, const char *name, int64 data);
void StringMap_putUInt(StringMap stringMap, const char *name, int data);
void StringMap_putUInt64(StringMap stringMap, const char *name, int64 data);
void StringMap_putDouble(StringMap stringMap, const char *name, double data);
void StringMap_putBool(StringMap stringMap, const char *name, bool data);
void StringMap_putChar(StringMap stringMap, const char *name, char data);
void StringMap_putCString(StringMap stringMap, const char *name, const char *data);
void StringMap_putString(StringMap stringMap, const char *name, String data);
void StringMap_putData(StringMap stringMap, const char *name, void *data, StringMapFormatFunction stringMapFormatFunction, void *stringMapFormatUserData);
#else /* not NDEBUG */
void __StringMap_putInt(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, int data);
void __StringMap_putInt64(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, int64 data);
void __StringMap_putUInt(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, int data);
void __StringMap_putUInt64(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, int64 data);
void __StringMap_putDouble(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, double data);
void __StringMap_putBool(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, bool data);
void __StringMap_putChar(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, char data);
void __StringMap_putCString(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, const char *data);
void __StringMap_putString(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, String data);
void __StringMap_putData(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, void *data, StringMapFormatFunction stringMapFormatFunction, void *stringMapFormatUserData);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : StringMap_getText, StringMap_getTextCString
* Purpose: get text value from string map
* Input  : stringMap    - stringMap
*          name         - value name
*          defaultValue - default value
* Output : value value or default value
* Return : string or NULL
* Notes  : -
\***********************************************************************/

String StringMap_getText(const StringMap stringMap, const char *name, const String defaultValue);
const char *StringMap_getTextCString(const StringMap stringMap, const char *name, const char *defaultValue);

/***********************************************************************\
* Name   : StringMap_get
* Purpose: get value from string map
* Input  : stringMap - stringMap
*          name      - value name
* Output : -
* Return : string map value or NULL
* Notes  : -
\***********************************************************************/

StringMapValue StringMap_get(const StringMap stringMap, const char *name);

/***********************************************************************\
* Name   : StringMap_get*
* Purpose: get data from string map
* Input  : stringMap    - stringMap
*          name         - value name
*          maxLength    - max. length of C-string (including NUL)
*          defaultValue - default data
* Output : data - value or default value
* Return : TRUE if read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool StringMap_getInt(const StringMap stringMap, const char *name, int *data, int defaultValue);
bool StringMap_getInt64(const StringMap stringMap, const char *name, int64 *data, int64 defaultValue);
bool StringMap_getUInt(const StringMap stringMap, const char *name, uint *data, uint defaultValue);
bool StringMap_getUInt64(const StringMap stringMap, const char *name, uint64 *data, uint64 defaultValue);
bool StringMap_getDouble(const StringMap stringMap, const char *name, double *data, double defaultValue);
bool StringMap_getBool(const StringMap stringMap, const char *name, bool *data, bool defaultValue);
bool StringMap_getEnum(const StringMap stringMap, const char *name, void *data, StringMapParseEnumFunction stringMapParseEnumFunction, int defaultValue);
bool StringMap_getChar(const StringMap stringMap, const char *name, char *data, char defaultValue);
bool StringMap_getCString(const StringMap stringMap, const char *name, char *data, uint maxLength, const char *defaultValue);
bool StringMap_getString(const StringMap stringMap, const char *name, String data, const String defaultValue);
bool StringMap_getData(const StringMap stringMap, const char *name, void *data, StringMapParseFunction stringMapParseFunction, void *stringMapParseUserData);

/***********************************************************************\
* Name   : StringMap_remove
* Purpose: remove string node from list
* Input  : stringMap - string map
*          stringNode - string node to remove
* Output : -
* Return : next node in list or NULL
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void StringMap_remove(StringMap stringMap, const char *name);
#else /* not NDEBUG */
void __StringMap_remove(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name);
#endif /* NDEBUG */

/***********************************************************************\
* Name   : StringMap_find, StringMap_findCString
* Purpose: find string in string map
* Input  : stringMap - string map
*          string,s   - string to find
* Output : -
* Return : string node or NULL of string not found
* Notes  : -
\***********************************************************************/

StringMapEntry *StringMap_find(const StringMap stringMap, const char *name);

/***********************************************************************\
* Name   : StringMap_contain
* Purpose: check if string map contain string
* Input  : stringMap - string map
*          string,s   - string to find
* Output : -
* Return : TRUE if string found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool StringMap_contain(const StringMap stringMap, const char *name);

/***********************************************************************\
* Name   : StringMap_parse
* Purpose: parse string map
* Input  : stringMap  - stringMap variable
*          quoteChars - quote characters
*          index      - start index or STRING_BEGIN
* Output : nextIndex  - index of next character in string not parsed or
*                       STRING_END if string completely parsed (can be
*                       NULL)
* Return : TRUE is fully parsed or nextIndex != NULL , FALSE on error
* Notes  : parses map of the format:
*            <name>=<value> ...
\***********************************************************************/

bool StringMap_parse(StringMap stringMap, const String string, const char *quoteChars, ulong index, long *nextIndex);
bool StringMap_parseCString(StringMap stringMap, const char *s, const char *quoteChars, ulong index, long *nextIndex);

/***********************************************************************\
* Name   : StringMap_toCStringArray
* Purpose: allocate array with C strings from string map
* Input  : stringMap - string map
* Output : -
* Return : C string array or NULL if insufficient memory
* Notes  : free memory after usage!
\***********************************************************************/

void* const *StringMap_valueArray(const StringMap stringMap);

#ifndef NDEBUG
/***********************************************************************\
* Name   : StringMap_debugDumpInfo, StringMap_debugPrintInfo
* Purpose: string map debug function: output not allocated strings
* Input  : handle - output channel
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void StringMap_debugDumpInfo(FILE *handle, const StringMap stringMap);
void StringMap_debugPrintInfo(const StringMap stringMap);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __STRINGMAPS__ */

/* end of file */
