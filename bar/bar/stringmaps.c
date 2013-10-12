/***********************************************************************\
*
* $Revision: 1092 $
* $Date: 2013-07-28 05:34:57 +0200 (Sun, 28 Jul 2013) $
* $Author: torsten $
* Contents:
* Systems: all
*
\***********************************************************************/

#define __STRINGMAPS_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#if defined(HAVE_PCRE)
  #include <pcreposix.h>
#elif defined(HAVE_REGEX_H)
  #include <regex.h>
#else
  #error No regular expression library available!
#endif /* HAVE_PCRE || HAVE_REGEX_H */
#include <assert.h>

#include "lists.h"
#include "strings.h"

#include "stringmaps.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define STRINGMAP_START_SIZE 16   // string map start size
#define STRINGMAP_DELTA_SIZE 16   // string map delta increasing size

const StringMapValue STRINGMAP_VALUE_NONE = {NULL,{0}};

/***************************** Datatypes *******************************/

struct __StringMap
{
  uint           size;
  StringMapEntry *stringMapEntries;
};

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : calculateHash
* Purpose: calculate hash
* Input  : data   - data
*          length - length of data
* Output : -
* Return : hash value
* Notes  : -
\***********************************************************************/

LOCAL uint calculateHash(const char *name)
{
  uint n;
  byte hashBytes[4];
  uint i;

  assert(name != NULL);

  n = strlen(name);

  hashBytes[0] = (n > 0) ? name[0] : 0;
  hashBytes[1] = (n > 1) ? name[1] : 0;
  hashBytes[2] = (n > 2) ? name[2] : 0;
  hashBytes[3] = (n > 3) ? name[3] : 0;
  for (i = 4; i < n; i++)
  {
    hashBytes[i%4] ^= name[i];
  }

  return (uint)(hashBytes[3] << 24) |
         (uint)(hashBytes[2] << 16) |
         (uint)(hashBytes[1] <<  8) |
         (uint)(hashBytes[0] <<  0);
}

/***********************************************************************\
* Name   : addStringMapEntry
* Purpose: add string entry to string map
* Input  : __fileName__ - file naem (debug only)
*          __lineNb__   - line number (debug only)
*          stringMap    - string map
*          name         - name
* Output : -
* Return : string map entry or NULL
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
LOCAL StringMapEntry *addStringMapEntry(struct __StringMap *stringMap, const char *name)
#else /* not NDEBUG */
LOCAL StringMapEntry *addStringMapEntry(const char *__fileName__, ulong __lineNb__, struct __StringMap *stringMap, const char *name)
#endif /* NDEBUG */
{
  uint           i;
  uint           n;
  StringMapEntry *newStringMapEntries;

  assert(stringMap != NULL);

  i = calculateHash(name)%stringMap->size;
  n = 0;
  while (   (stringMap->stringMapEntries[i].name != NULL)
         && (n < stringMap->size)
        )
  {
    i = (i+1)%stringMap->size;
    n++;
  }

  if (n >= stringMap->size)
  {
    newStringMapEntries = (StringMapEntry*)realloc(stringMap->stringMapEntries,sizeof(StringMapEntry)*(stringMap->size+STRINGMAP_DELTA_SIZE));
    if (newStringMapEntries == NULL)
    {
      return NULL;
    }
    for (i = stringMap->size; i < stringMap->size+STRINGMAP_DELTA_SIZE; i++)
    {
      stringMap->stringMapEntries[i].name = NULL;
    }
    stringMap->size += STRINGMAP_DELTA_SIZE;
  }

  if (n < stringMap->size)
  {
    stringMap->stringMapEntries[i].name = strdup(name);
    #ifndef NDEBUG
      stringMap->stringMapEntries[i].fileName  = __fileName__;
      stringMap->stringMapEntries[i].lineNb    = __lineNb__;
    #endif /* NDEBUG */

    return &stringMap->stringMapEntries[i];
  }
  else
  {
    return NULL;
  }
}

/***********************************************************************\
* Name   : removeStringMapEntry
* Purpose: remove string entry to string map
* Input  : stringMapEntry - string map entry
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void removeStringMapEntry(StringMapEntry *stringMapEntry)
{
  assert(stringMapEntry != NULL);

  if (stringMapEntry->value.text != NULL)
  {
    String_delete(stringMapEntry->value.text);
    stringMapEntry->value.text = NULL;
  }
  switch (stringMapEntry->type)
  {
    case STRINGMAP_TYPE_NONE:
      break;
    case STRINGMAP_TYPE_INT:
    case STRINGMAP_TYPE_INT64:
    case STRINGMAP_TYPE_UINT:
    case STRINGMAP_TYPE_UINT64:
    case STRINGMAP_TYPE_DOUBLE:
    case STRINGMAP_TYPE_BOOL:
    case STRINGMAP_TYPE_CHAR:
    case STRINGMAP_TYPE_DATA:
      break;
    case STRINGMAP_TYPE_CSTRING:
      free(stringMapEntry->value.data.s);
      break;
    case STRINGMAP_TYPE_STRING:
      String_delete(stringMapEntry->value.data.string);
      break;
  }
  free(stringMapEntry->name);
  stringMapEntry->name = NULL;
}

/***********************************************************************\
* Name   : findStringMapEntry
* Purpose: find string entry in string map
* Input  : stringMap - string map
*          name      - name
* Output : -
* Return : string map entry or NULL
* Notes  : -
\***********************************************************************/

LOCAL StringMapEntry *findStringMapEntry(const struct __StringMap *stringMap, const char *name)
{
  uint i;
  uint n;

  assert(stringMap != NULL);

  i = calculateHash(name)%stringMap->size;
  n = 0;
  while (   ((stringMap->stringMapEntries[i].name == NULL) || (strcmp(stringMap->stringMapEntries[i].name,name) != 0))
         && (n < stringMap->size)
        )
  {
    i = (i+1)%stringMap->size;
    n++;
  }

  return (n < stringMap->size) ? &stringMap->stringMapEntries[i] : NULL;
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
StringMap StringMap_new(uint size)
#else /* not NDEBUG */
StringMap __StringMap_new(const char *__fileName__, ulong __lineNb__)
#endif /* NDEBUG */
{
  struct __StringMap *stringMap;
  uint               i;

  stringMap = (struct __StringMap *)malloc(sizeof(struct __StringMap));
  if (stringMap == NULL)
  {
    return NULL;
  }

  stringMap->size             = STRINGMAP_START_SIZE;
  stringMap->stringMapEntries = malloc(sizeof(StringMapEntry)*STRINGMAP_START_SIZE);
  if (stringMap == NULL)
  {
    free(stringMap);
    return NULL;
  }
  for (i = 0; i < STRINGMAP_START_SIZE; i++)
  {
    stringMap->stringMapEntries[i].name         = NULL;
    stringMap->stringMapEntries[i].type         = STRINGMAP_TYPE_NONE;
    stringMap->stringMapEntries[i].value.text   = NULL;
    stringMap->stringMapEntries[i].value.data.p = NULL;
    #ifndef NDEBUG
      stringMap->stringMapEntries[i].fileName = NULL;
      stringMap->stringMapEntries[i].lineNb   = 0L;
    #endif /* NDEBUG */
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE("stringMap",stringMap);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,"stringMap",stringMap);
  #endif /* NDEBUG */

  return stringMap;
}

#ifdef NDEBUG
StringMap StringMap_duplicate(const StringMap stringMap)
#else /* not NDEBUG */
StringMap __StringMap_duplicate(const char *__fileName__, ulong __lineNb__, const StringMap stringMap)
#endif /* NDEBUG */
{
  struct __StringMap *newStringMap;

  assert(stringMap != NULL);

  #ifdef NDEBUG
    newStringMap = StringMap_new(stringMap->size);
  #else /* not NDEBUG */
    newStringMap = __StringMap_new(__fileName__,__lineNb__);
  #endif /* NDEBUG */
  if (newStringMap == NULL)
  {
    return NULL;
  }

  StringMap_copy(newStringMap,stringMap);

  return newStringMap;
}

void StringMap_copy(StringMap stringMap, const StringMap fromStringMap)
{
  uint i;

  assert(stringMap != NULL);
  assert(fromStringMap != NULL);

  stringMap->size = fromStringMap->size;
  for (i = 0; i < stringMap->size; i++)
  {
    if (fromStringMap->stringMapEntries[i].name != NULL)
    {
      stringMap->stringMapEntries[i].name  = strdup(fromStringMap->stringMapEntries[i].name);
      stringMap->stringMapEntries[i].value = fromStringMap->stringMapEntries[i].value;
    }
    else
    {
      stringMap->stringMapEntries[i].name = NULL;
    }
  }
}

#ifdef NDEBUG
void StringMap_delete(StringMap stringMap)
#else /* not NDEBUG */
void __StringMap_delete(const char *__fileName__, ulong __lineNb__, StringMap stringMap)
#endif /* NDEBUG */
{
  uint i;

  assert(stringMap != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(stringMap);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,stringMap);
  #endif /* NDEBUG */

  for (i = 0; i < stringMap->size; i++)
  {
    if (stringMap->stringMapEntries[i].name != NULL)
    {
      removeStringMapEntry(&stringMap->stringMapEntries[i]);
    }
  }
  free(stringMap->stringMapEntries);
  free(stringMap);
}

StringMap StringMap_clear(StringMap stringMap)
{
  uint i;

  assert(stringMap != NULL);

  for (i = 0; i < stringMap->size; i++)
  {
    if (stringMap->stringMapEntries[i].name != NULL)
    {
      removeStringMapEntry(&stringMap->stringMapEntries[i]);
      stringMap->stringMapEntries[i].name = NULL;
    }
  }

  return stringMap;
}

uint StringMap_count(const StringMap stringMap)
{
  uint count;
  uint i;

  assert(stringMap != NULL);

  count = 0L;
  for (i = 0; i < stringMap->size; i++)
  {
    if (stringMap->stringMapEntries[i].name != NULL)
    {
      count++;
    }
  }

  return count;
}

const StringMapEntry *StringMap_index(const StringMap stringMap, uint index)
{
  StringMapEntry *stringMapEntry;
  uint           i;

  assert(stringMap != NULL);

  stringMapEntry = NULL;
  for (i = 0; i < stringMap->size; i++)
  {
    if (stringMap->stringMapEntries[i].name != NULL)
    {
      if (index > 0)
      {
        index--;
      }
      else
      {
        stringMapEntry = &stringMap->stringMapEntries[i];
        break;
      }
    }
  }

  return stringMapEntry;
}

const char *StringMap_indexName(const StringMap stringMap, uint index)
{
  const StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);

  stringMapEntry = StringMap_index(stringMap,index);
  return (stringMapEntry != NULL) ? stringMapEntry->name : NULL;
}

StringMapValue StringMap_indexValue(const StringMap stringMap, uint index)
{
  const StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);

  stringMapEntry = StringMap_index(stringMap,index);
  return (stringMapEntry != NULL) ? stringMapEntry->value : STRINGMAP_VALUE_NONE;
}

#ifdef NDEBUG
void __StringMap_putText(StringMap stringMap, const char *name, String text)
#else /* not NDEBUG */
void __StringMap_putText(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, String text)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */

  if (stringMapEntry != NULL)
  {
    stringMapEntry->type       = STRINGMAP_TYPE_NONE;
    stringMapEntry->value.text = String_duplicate(text);
  }
}

#ifdef NDEBUG
void __StringMap_putTextCString(StringMap stringMap, const char *name, String text)
#else /* not NDEBUG */
void __StringMap_putTextCString(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, const char *text)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */

  if (stringMapEntry != NULL)
  {
    stringMapEntry->type       = STRINGMAP_TYPE_NONE;
    stringMapEntry->value.text = String_newCString(text);
  }
}

#ifdef NDEBUG
void StringMap_put(StringMap stringMap, const char *name, void *value)
#else /* not NDEBUG */
void __StringMap_put(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, void *value)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */
  if (stringMapEntry != NULL)
  {
    stringMapEntry->type         = STRINGMAP_TYPE_STRING;
    stringMapEntry->value.text   = NULL;
    stringMapEntry->value.data.p = value;
  }
}

#ifdef NDEBUG
void StringMap_putInt(StringMap stringMap, const char *name, int value)
#else /* not NDEBUG */
void __StringMap_putInt(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, int value)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */
  if (stringMapEntry != NULL)
  {
    stringMapEntry->type         = STRINGMAP_TYPE_INT;
    stringMapEntry->value.text   = NULL;
    stringMapEntry->value.data.i = value;
  }
}

#ifdef NDEBUG
void StringMap_putInt64(StringMap stringMap, const char *name, int64 value)
#else /* not NDEBUG */
void __StringMap_putInt64(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, int64 value)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */
  if (stringMapEntry != NULL)
  {
    stringMapEntry->type         = STRINGMAP_TYPE_INT64;
    stringMapEntry->value.text   = NULL;
    stringMapEntry->value.data.l = value;
  }
}

#ifdef NDEBUG
void StringMap_putDouble(StringMap stringMap, const char *name, double value)
#else /* not NDEBUG */
void __StringMap_putDouble(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, double value)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */
  if (stringMapEntry != NULL)
  {
    stringMapEntry->type         = STRINGMAP_TYPE_DOUBLE;
    stringMapEntry->value.text   = NULL;
    stringMapEntry->value.data.d = value;
  }
}

#ifdef NDEBUG
void StringMap_putBool(StringMap stringMap, const char *name, bool value)
#else /* not NDEBUG */
void __StringMap_putBool(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, bool value)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */
  if (stringMapEntry != NULL)
  {
    stringMapEntry->type         = STRINGMAP_TYPE_BOOL;
    stringMapEntry->value.text   = NULL;
    stringMapEntry->value.data.b = value;
  }
}

#ifdef NDEBUG
void StringMap_putChar(StringMap stringMap, const char *name, char value)
#else /* not NDEBUG */
void __StringMap_putChar(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, char value)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */
  if (stringMapEntry != NULL)
  {
    stringMapEntry->type         = STRINGMAP_TYPE_CHAR;
    stringMapEntry->value.text   = NULL;
    stringMapEntry->value.data.c = value;
  }
}

#ifdef NDEBUG
void StringMap_putCString(StringMap stringMap, const char *name, const char *value)
#else /* not NDEBUG */
void __StringMap_putCString(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, const char *value)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */

  if (stringMapEntry != NULL)
  {
    stringMapEntry->type         = STRINGMAP_TYPE_CSTRING;
    stringMapEntry->value.text   = NULL;
    stringMapEntry->value.data.s = strdup(value);
  }
}

#ifdef NDEBUG
void StringMap_putString(StringMap stringMap, const char *name, String value)
#else /* not NDEBUG */
void __StringMap_putString(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, String value)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */

  if (stringMapEntry != NULL)
  {
    stringMapEntry->type              = STRINGMAP_TYPE_STRING;
    stringMapEntry->value.text        = NULL;
    stringMapEntry->value.data.string = String_duplicate(value);
  }
}

#ifdef NDEBUG
void StringMap_putData(StringMap stringMap, const char *name, void *data, StringMapFormatFunction stringMapFormatFunction, void *stringMapFormatUserData)
#else /* not NDEBUG */
void __StringMap_putData(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, void *data, StringMapFormatFunction stringMapFormatFunction, void *stringMapFormatUserData)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */

  if (stringMapEntry != NULL)
  {
    stringMapEntry->type       = STRINGMAP_TYPE_DATA;
    stringMapEntry->value.text = stringMapFormatFunction(data,stringMapFormatUserData);
  }
}

String StringMap_getText(const StringMap stringMap, const char *name, const String defaultValue)
{
  const StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    return stringMapEntry->value.text;
  }
  else
  {
    return defaultValue;
  }
}

const char *StringMap_getTextCString(const StringMap stringMap, const char *name, const char *defaultValue)
{
  const StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    return String_cString(stringMapEntry->value.text);
  }
  else
  {
    return defaultValue;
  }
}

StringMapValue StringMap_get(const StringMap stringMap, const char *name)
{
  const StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if (stringMapEntry != NULL)
  {
    return stringMapEntry->value;
  }
  else
  {
    return STRINGMAP_VALUE_NONE;
  }
}

bool StringMap_getInt(const StringMap stringMap, const char *name, int *data, int defaultValue)
{
  StringMapEntry *stringMapEntry;
  char           *nextData;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    (*data) = strtol(String_cString(stringMapEntry->value.text),&nextData,0);
    return ((*nextData) == '\0');
  }
  else
  {
    (*data) = defaultValue;
    return FALSE;
  }
}

bool StringMap_getInt64(const StringMap stringMap, const char *name, int64 *data, int64 defaultValue)
{
  StringMapEntry *stringMapEntry;
  char           *nextData;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    (*data) = strtoll(String_cString(stringMapEntry->value.text),&nextData,0);
    return ((*nextData) == '\0');
  }
  else
  {
    (*data) = defaultValue;
    return FALSE;
  }
}

bool StringMap_getUInt(const StringMap stringMap, const char *name, uint *data, uint defaultValue)
{
  StringMapEntry *stringMapEntry;
  char           *nextData;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    (*data) = (uint)strtol(String_cString(stringMapEntry->value.text),&nextData,0);
    return ((*nextData) == '\0');
  }
  else
  {
    (*data) = defaultValue;
    return FALSE;
  }
}

bool StringMap_getUInt64(const StringMap stringMap, const char *name, uint64 *data, uint64 defaultValue)
{
  StringMapEntry *stringMapEntry;
  char           *nextData;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    (*data) = (uint64)strtoll(String_cString(stringMapEntry->value.text),&nextData,0);
    return ((*nextData) == '\0');
  }
  else
  {
    (*data) = defaultValue;
    return FALSE;
  }
}

bool StringMap_getDouble(const StringMap stringMap, const char *name, double *data, double defaultValue)
{
  StringMapEntry *stringMapEntry;
  char           *nextData;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    (*data) = strtod(String_cString(stringMapEntry->value.text),&nextData);
    return ((*nextData) == '\0');
  }
  else
  {
    (*data) = defaultValue;
    return FALSE;
  }
}

bool StringMap_getBool(const StringMap stringMap, const char *name, bool *data, bool defaultValue)
{
  const char *TRUE_STRINGS[] =
  {
    "1",
    "true",
    "yes",
    "on",
  };

  StringMapEntry *stringMapEntry;
  uint           z;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    (*data) = FALSE;
    for (z = 0; z < SIZE_OF_ARRAY(TRUE_STRINGS); z++)
    {
      if (String_equalsIgnoreCaseCString(stringMapEntry->value.text,TRUE_STRINGS[z]))
      {
        (*data) = TRUE;
      }
    }
    return TRUE;
  }
  else
  {
    (*data) = defaultValue;
    return FALSE;
  }
}

bool StringMap_getEnum(const StringMap stringMap, const char *name, void *data, StringMapParseEnumFunction stringMapParseEnumFunction, int defaultValue)
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);
  assert(stringMapParseEnumFunction != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    return stringMapParseEnumFunction(String_cString(stringMapEntry->value.text),(int*)data);
  }
  else
  {
    (*(int*)data) = defaultValue;
    return FALSE;
  }
}

bool StringMap_getChar(const StringMap stringMap, const char *name, char *data, char defaultValue)
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    if (String_length(stringMapEntry->value.text) > 0)
    {
      (*data) = String_index(stringMapEntry->value.text,0);
      return TRUE;
    }
    (*data) = defaultValue;
    return FALSE;
  }
  else
  {
    (*data) = defaultValue;
    return FALSE;
  }
}

bool StringMap_getCString(const StringMap stringMap, const char *name, char *data, uint maxLength, const char *defaultValue)
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);
  assert(maxLength > 0);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    strncpy(data,String_cString(stringMapEntry->value.text),maxLength);
    return TRUE;
  }
  else
  {
    if (defaultValue != NULL)
    {
      strncpy(data,defaultValue,maxLength);
    }
    else
    {
      data[0] = '\0';
    }
    return FALSE;
  }
}

bool StringMap_getString(const StringMap stringMap, const char *name, String data, const String defaultValue)
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    String_set(data,stringMapEntry->value.text);
    return TRUE;
  }
  else
  {
    if (defaultValue != NULL)
    {
      String_set(data,defaultValue);
    }
    else
    {
      String_clear(data);
    }
    return FALSE;
  }
}

bool StringMap_getData(const StringMap stringMap, const char *name, void *data, StringMapParseFunction stringMapParseFunction, void *stringMapParseUserData)
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    return stringMapParseFunction(stringMapEntry->value.text,data,stringMapParseUserData);
  }
  else
  {
    return FALSE;
  }
}

#ifdef NDEBUG
void StringMap_remove(StringMap stringMap, const char *name)
#else /* not NDEBUG */
void __StringMap_remove(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if (stringMapEntry != NULL)
  {
    free(stringMapEntry->name); stringMapEntry->name = NULL;
    #ifndef NDEBUG
      stringMapEntry->fileName = __fileName__;
      stringMapEntry->lineNb   = __lineNb__;
    #endif /* NDEBUG */
  }
}

StringMapEntry *StringMap_find(const StringMap stringMap, const char *name)
{
  assert(stringMap != NULL);
  assert(name != NULL);

  return findStringMapEntry(stringMap,name);
}

bool StringMap_contain(const StringMap stringMap, const char *name)
{
  assert(stringMap != NULL);
  assert(name != NULL);

  return (findStringMapEntry(stringMap,name) != NULL);
}

bool StringMap_parse(StringMap stringMap, const String string, const char *quoteChars, ulong index, long *nextIndex)
{
  assert(stringMap != NULL);

  STRING_CHECK_VALID(string);

  return StringMap_parseCString(stringMap,String_cString(string),quoteChars,index,nextIndex);
}

bool StringMap_parseCString(StringMap stringMap, const char *s, const char *quoteChars, ulong index, long *nextIndex)
{
  const char *quoteChar;
  uint       length;
  String     name;
  String     text;
  int        i;

  assert(stringMap != NULL);
  assert(s != NULL);

  // parse
  length = strlen(s);
  name   = String_new();
  text   = String_new();

  index = STRING_BEGIN;
  while (index < length)
  {
    // skip spaces
    while ((index < length) && isspace(s[index]))
    {
      index++;
    }

    // get name
    String_clear(name);
    if (   (index < length)
        && (!isalpha(s[index]) && (s[index] != '_'))
       )
    {
      if (nextIndex != NULL) (*nextIndex) = index;
      String_delete(text);
      String_delete(name);
      return FALSE;
    }
    do
    {
      String_appendChar(name,s[index]);
      index++;
    }
    while (   (index < length)
           && (isalnum(s[index]) || (s[index] == '_'))
          );

    // check '='
    if (   (index >= length) || (s[index] != '='))
    {
      if (nextIndex != NULL) (*nextIndex) = index;
      String_delete(text);
      String_delete(name);
      return FALSE;
    }
    index++;

    // get value as text
    String_clear(text);
    while ((index < length) && !isspace(s[index]))
    {
      if (   (s[index] == '\\')
          && ((index+1) < length)
          && (strchr(quoteChars,s[index+1]) != NULL)
         )
      {
        // quoted quote
        String_appendChar(text, s[index+1]);
        index += 2;
      }
      else
      {
        // check for string quote
        quoteChar = strchr(quoteChars,s[index]);
        if (quoteChar != NULL)
        {
          // skip quote-char
          index++;

          // get string
          while ((index < length) && (s[index] != (*quoteChar)))
          {
            if (   ((index+1) < length)
                && (s[index] == '\\')
               )
            {
              index++;

              if      (strchr(quoteChars,s[index]) != NULL)
              {
                // quoted quote
                String_appendChar(text,s[index]);
              }
              else
              {
                // search for known escaped character
                i = STRING_ESCAPE_LENGTH-1;
                while ((i >= 0) && (STRING_ESCAPE_MAP[i] != s[index]))
                {
                  i--;
                }

                if (i >= 0)
                {
                  // escaped characater
                  String_appendChar(text,STRING_ESCAPE_CHARACTERS[i]);
                }
                else
                {
                  // other escaped character
                  String_appendChar(text,s[index]);
                }
              }
            }
            else
            {
              String_appendChar(text,s[index]);
            }
            index++;
          }

          // skip quote-char
          if (index < length)
          {
            index++;
          }
        }
        else
        {
          String_appendChar(text,s[index]);
          index++;
        }
      }
    }

    // store value
    StringMap_putText(stringMap,name->data,text);
  }

  if (nextIndex != NULL)
  {
    (*nextIndex) = index;
  }

  // free resources
  String_delete(text);
  String_delete(name);

  return TRUE;
}

void* const *StringMap_valueArray(const StringMap stringMap)
{
  uint count;
  void **valueArray;
  uint n,i;

  assert(stringMap != NULL);

  count = StringMap_count(stringMap);

  valueArray = (void**)malloc(count*sizeof(char*));
  if (valueArray != NULL)
  {
    n = 0;
    for (i = 0; i < stringMap->size; i++)
    {
      if (stringMap->stringMapEntries[i].name != NULL)
      {
        assert(n < count);
        valueArray[n] = stringMap->stringMapEntries[i].value.data.p; n++;
      }
    }
  }

  return valueArray;
}

#ifndef NDEBUG
void StringMap_debugDump(FILE *handle, const StringMap stringMap)
{
  uint i;

  assert(stringMap != NULL);

  for (i = 0; i < stringMap->size; i++)
  {
    if (stringMap->stringMapEntries[i].name != NULL)
    {
      fprintf(handle,"DEBUG %u: %s = %lx\n",i,stringMap->stringMapEntries[i].name,(unsigned long)stringMap->stringMapEntries[i].value.data.p);
    }
  }
}

void StringMap_debugPrint(const StringMap stringMap)
{
  StringMap_debugDump(stderr,stringMap);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
