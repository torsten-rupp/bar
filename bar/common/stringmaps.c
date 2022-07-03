/***********************************************************************\
*
* $Revision: 1092 $
* $Date: 2013-07-28 05:34:57 +0200 (Sun, 28 Jul 2013) $
* $Author: torsten $
* Contents:
* Systems: all
*
\***********************************************************************/

#define __STRINGMAPS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#if defined(HAVE_PCRE)
  #include <pcreposix.h>
#elif defined(HAVE_REGEX_H)
  #include <regex.h>
#else
  #warning No regular expression library available!
#endif /* HAVE_PCRE || HAVE_REGEX_H */
#include <assert.h>

#include "common/lists.h"
#include "common/strings.h"

#include "stringmaps.h"

/****************** Conditional compilation switches *******************/
#define HALT_ON_INSUFFICIENT_MEMORY

/***************************** Constants *******************************/
#define STRINGMAP_START_SIZE 32   // string map start size
#define STRINGMAP_DELTA_SIZE 16   // string map delta increasing size

const StringMapValue STRINGMAP_VALUE_NONE = {NULL,FALSE,{0}};

LOCAL const char *TRUE_STRINGS[] =
{
  "1",
  "true",
  "yes",
  "on",
};

#ifndef NDEBUG
const char *STRING_MAP_TYPE_NAMES[] =
{
  [STRINGMAP_TYPE_NONE   ] = "none",
  [STRINGMAP_TYPE_INT    ] = "int",
  [STRINGMAP_TYPE_INT64  ] = "int64",
  [STRINGMAP_TYPE_UINT   ] = "uint",
  [STRINGMAP_TYPE_UINT64 ] = "uint64",
  [STRINGMAP_TYPE_DOUBLE ] = "double",
  [STRINGMAP_TYPE_BOOL   ] = "bool",
  [STRINGMAP_TYPE_FLAG   ] = "flag",
  [STRINGMAP_TYPE_CHAR   ] = "char",
  [STRINGMAP_TYPE_CSTRING] = "c-string",
  [STRINGMAP_TYPE_STRING ] = "string",
  [STRINGMAP_TYPE_DATA   ] = "data"
};
#endif /* not NDEBUG */

/***************************** Datatypes *******************************/

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
LOCAL StringMapEntry *addStringMapEntry(StringMap stringMap, const char *name)
#else /* not NDEBUG */
LOCAL StringMapEntry *addStringMapEntry(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name)
#endif /* NDEBUG */
{
  uint           hashIndex;
  uint           n;
  uint           i;
  uint           newStringMapSize;
  StringMapEntry *newEntries;

  assert(stringMap != NULL);
  assert(stringMap->entries != NULL);

  hashIndex = calculateHash(name)%stringMap->size;
  n = 0;
  while (   (stringMap->entries[hashIndex].name != NULL)
         && (n < stringMap->size)
        )
  {
    hashIndex = (hashIndex+1)%stringMap->size;
    n++;
  }

  if (n >= stringMap->size)
  {
    // re-allocate new entries
    newStringMapSize    = stringMap->size+STRINGMAP_DELTA_SIZE;
    newEntries = (StringMapEntry*)malloc(sizeof(StringMapEntry)*newStringMapSize);
    if (newEntries == NULL)
    {
      return NULL;
    }

    // init new entries
    for (i = 0; i < newStringMapSize; i++)
    {
      newEntries[i].name             = NULL;
      newEntries[i].type             = STRINGMAP_TYPE_NONE;
      newEntries[i].value.text       = NULL;
      newEntries[i].value.quotedFlag = FALSE;
      newEntries[i].value.data.p     = NULL;
      #ifndef NDEBUG
        newEntries[i].fileName = NULL;
        newEntries[i].lineNb   = 0L;
      #endif /* NDEBUG */
    }

    // rehash existing entries
    for (i = 0; i < stringMap->size; i++)
    {
      assert(stringMap->entries[i].name != NULL);

      hashIndex = calculateHash(stringMap->entries[i].name)%newStringMapSize;
      n = 0;
      while (   (newEntries[hashIndex].name != NULL)
             && (n < newStringMapSize)
            )
      {
        hashIndex = (hashIndex+1)%newStringMapSize;
        n++;
      }
      assert(n < newStringMapSize);
      newEntries[hashIndex] = stringMap->entries[i];
    }

    // free old entries, set new entries
    free(stringMap->entries);
    stringMap->size    = newStringMapSize;
    stringMap->entries = newEntries;
  }

  if (n < stringMap->size)
  {
    stringMap->entries[hashIndex].name = stringDuplicate(name);
    #ifndef NDEBUG
      stringMap->entries[hashIndex].fileName  = __fileName__;
      stringMap->entries[hashIndex].lineNb    = __lineNb__;
    #endif /* NDEBUG */

    return &stringMap->entries[hashIndex];
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
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
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
    case STRINGMAP_TYPE_FLAG:
    case STRINGMAP_TYPE_CHAR:
      break;
    case STRINGMAP_TYPE_CSTRING:
      free(stringMapEntry->value.data.s);
      break;
    case STRINGMAP_TYPE_STRING:
      String_delete(stringMapEntry->value.data.string);
      break;
    case STRINGMAP_TYPE_DATA:
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
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

LOCAL StringMapEntry *findStringMapEntry(const StringMap stringMap, const char *name)
{
  uint i;
  uint n;

  assert(stringMap != NULL);
  assert(stringMap->entries != NULL);

  i = calculateHash(name)%stringMap->size;
  n = 0;
  while (   ((stringMap->entries[i].name == NULL) || !stringEquals(stringMap->entries[i].name,name))
         && (n < stringMap->size)
        )
  {
    i = (i+1)%stringMap->size;
    n++;
  }

  return (n < stringMap->size) ? &stringMap->entries[i] : NULL;
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
StringMap StringMap_new(void)
#else /* not NDEBUG */
StringMap __StringMap_new(const char *__fileName__,
                          ulong      __lineNb__
                         )
#endif /* NDEBUG */
{
  uint      i;
  StringMap stringMap;

  // allocate string map
  stringMap = (StringMap)malloc(sizeof(struct __StringMap));
  if (stringMap == NULL)
  {
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return NULL;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }

  // init
  stringMap->size    = STRINGMAP_START_SIZE;
  stringMap->entries = (StringMapEntry*)malloc(sizeof(StringMapEntry)*STRINGMAP_START_SIZE);
  if (stringMap->entries == NULL)
  {
    free(stringMap);
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return NULL;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }
  for (i = 0; i < STRINGMAP_START_SIZE; i++)
  {
    stringMap->entries[i].name = NULL;
  }

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(stringMap,StringMap);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,stringMap,StringMap);
  #endif /* NDEBUG */

  return stringMap;
}

#ifdef NDEBUG
StringMap StringMap_duplicate(const StringMap stringMap)
#else /* not NDEBUG */
StringMap __StringMap_duplicate(const char *__fileName__, ulong __lineNb__, const StringMap stringMap)
#endif /* NDEBUG */
{
  StringMap newStringMap;

  assert(stringMap != NULL);
  assert(stringMap->entries != NULL);

  #ifdef NDEBUG
    newStringMap = StringMap_new();
  #else /* not NDEBUG */
    newStringMap = __StringMap_new(__fileName__,__lineNb__);
  #endif /* NDEBUG */
  if (newStringMap == NULL)
  {
    return NULL;
  }

  if (StringMap_copy(newStringMap,stringMap) == NULL)
  {
    StringMap_delete(newStringMap);
    return NULL;
  }

  return newStringMap;
}

StringMap StringMap_copy(StringMap stringMap, const StringMap fromStringMap)
{
  StringMapEntry *newEntries;
  uint           i;

  assert(stringMap != NULL);
  assert(stringMap->entries != NULL);
  assert(fromStringMap != NULL);
  assert(fromStringMap->entries != NULL);

  // allocate new entries
  StringMap_clear(stringMap);
  newEntries = (StringMapEntry*)realloc(stringMap->entries,sizeof(StringMapEntry)*fromStringMap->size);
  if (newEntries == NULL)
  {
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return NULL;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }
  stringMap->size    = fromStringMap->size;
  stringMap->entries = newEntries;

  // copy entries
  for (i = 0; i < fromStringMap->size; i++)
  {
    if (fromStringMap->entries[i].name != NULL)
    {
      stringMap->entries[i].name             = stringDuplicate(fromStringMap->entries[i].name);
      stringMap->entries[i].type             = fromStringMap->entries[i].type;
      stringMap->entries[i].value.text       = String_duplicate(fromStringMap->entries[i].value.text);
      stringMap->entries[i].value.quotedFlag = fromStringMap->entries[i].value.quotedFlag;
      switch (fromStringMap->entries[i].type)
      {
        case STRINGMAP_TYPE_NONE:                                                                                                             break;
        case STRINGMAP_TYPE_INT:     stringMap->entries[i].value.data.i      = fromStringMap->entries[i].value.data.i;                        break;
        case STRINGMAP_TYPE_INT64:   stringMap->entries[i].value.data.l      = fromStringMap->entries[i].value.data.l;                        break;
        case STRINGMAP_TYPE_UINT:    stringMap->entries[i].value.data.ui     = fromStringMap->entries[i].value.data.ui;                       break;
        case STRINGMAP_TYPE_UINT64:  stringMap->entries[i].value.data.ul     = fromStringMap->entries[i].value.data.ul;                       break;
        case STRINGMAP_TYPE_DOUBLE:  stringMap->entries[i].value.data.d      = fromStringMap->entries[i].value.data.d;                        break;
        case STRINGMAP_TYPE_BOOL:    stringMap->entries[i].value.data.b      = fromStringMap->entries[i].value.data.b;                        break;
        case STRINGMAP_TYPE_FLAG:    stringMap->entries[i].value.data.flag   = fromStringMap->entries[i].value.data.flag;                     break;
        case STRINGMAP_TYPE_CHAR:    stringMap->entries[i].value.data.c      = fromStringMap->entries[i].value.data.c;                        break;
        case STRINGMAP_TYPE_CSTRING: stringMap->entries[i].value.data.s      = stringDuplicate(fromStringMap->entries[i].value.data.s);       break;
        case STRINGMAP_TYPE_STRING:  stringMap->entries[i].value.data.string = String_duplicate(fromStringMap->entries[i].value.data.string); break;
        case STRINGMAP_TYPE_DATA:    stringMap->entries[i].value.data.p      = fromStringMap->entries[i].value.data.p;                        break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }
      #ifndef NDEBUG
        stringMap->entries[i].fileName = fromStringMap->entries[i].fileName;
        stringMap->entries[i].lineNb   = fromStringMap->entries[i].lineNb;
      #endif /* NDEBUG */
    }
    else
    {
      stringMap->entries[i].name = NULL;
    }
  }

  return stringMap;
}

StringMap StringMap_move(StringMap stringMap, StringMap fromStringMap)
{
  StringMapEntry *newEntries;
  uint           i;

  assert(stringMap != NULL);
  assert(stringMap->entries != NULL);
  assert(fromStringMap != NULL);
  assert(fromStringMap->entries != NULL);

  // allocate new entries
  StringMap_clear(stringMap);
  newEntries = (StringMapEntry*)realloc(stringMap->entries,sizeof(StringMapEntry)*fromStringMap->size);
  if (newEntries == NULL)
  {
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return NULL;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }
  stringMap->size    = fromStringMap->size;
  stringMap->entries = newEntries;

  // move entries
  for (i = 0; i < fromStringMap->size; i++)
  {
    stringMap->entries[i].name             = fromStringMap->entries[i].name;
    stringMap->entries[i].type             = fromStringMap->entries[i].type;
    stringMap->entries[i].value.text       = fromStringMap->entries[i].value.text;
    stringMap->entries[i].value.quotedFlag = fromStringMap->entries[i].value.quotedFlag;
    switch (fromStringMap->entries[i].type)
    {
      case STRINGMAP_TYPE_NONE:                                                                                           break;
      case STRINGMAP_TYPE_INT:     stringMap->entries[i].value.data.i      = fromStringMap->entries[i].value.data.i;      break;
      case STRINGMAP_TYPE_INT64:   stringMap->entries[i].value.data.l      = fromStringMap->entries[i].value.data.l;      break;
      case STRINGMAP_TYPE_UINT:    stringMap->entries[i].value.data.ui     = fromStringMap->entries[i].value.data.ui;     break;
      case STRINGMAP_TYPE_UINT64:  stringMap->entries[i].value.data.ul     = fromStringMap->entries[i].value.data.ul;     break;
      case STRINGMAP_TYPE_DOUBLE:  stringMap->entries[i].value.data.d      = fromStringMap->entries[i].value.data.d;      break;
      case STRINGMAP_TYPE_BOOL:    stringMap->entries[i].value.data.b      = fromStringMap->entries[i].value.data.b;      break;
      case STRINGMAP_TYPE_FLAG:    stringMap->entries[i].value.data.flag   = fromStringMap->entries[i].value.data.flag;   break;
      case STRINGMAP_TYPE_CHAR:    stringMap->entries[i].value.data.c      = fromStringMap->entries[i].value.data.c;      break;
      case STRINGMAP_TYPE_CSTRING: stringMap->entries[i].value.data.s      = fromStringMap->entries[i].value.data.s;      break;
      case STRINGMAP_TYPE_STRING:  stringMap->entries[i].value.data.string = fromStringMap->entries[i].value.data.string; break;
      case STRINGMAP_TYPE_DATA:    stringMap->entries[i].value.data.p      = fromStringMap->entries[i].value.data.p;      break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }
    #ifndef NDEBUG
      stringMap->entries[i].fileName = fromStringMap->entries[i].fileName;
      stringMap->entries[i].lineNb   = fromStringMap->entries[i].lineNb;
    #endif /* NDEBUG */

    fromStringMap->entries[i].name = NULL;
  }

  return stringMap;
}

#ifdef NDEBUG
void StringMap_delete(StringMap stringMap)
#else /* not NDEBUG */
void __StringMap_delete(const char *__fileName__, ulong __lineNb__, StringMap stringMap)
#endif /* NDEBUG */
{
  uint i;

  assert(stringMap != NULL);
  assert(stringMap->entries != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(stringMap,StringMap);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,stringMap,StringMap);
  #endif /* NDEBUG */

  for (i = 0; i < stringMap->size; i++)
  {
    if (stringMap->entries[i].name != NULL)
    {
      removeStringMapEntry(&stringMap->entries[i]);
    }
  }
  free(stringMap->entries);
  free(stringMap);
}

StringMap StringMap_clear(StringMap stringMap)
{
  uint i;

  assert(stringMap != NULL);
  assert(stringMap->entries != NULL);

  for (i = 0; i < stringMap->size; i++)
  {
    if (stringMap->entries[i].name != NULL)
    {
      removeStringMapEntry(&stringMap->entries[i]);
      stringMap->entries[i].name = NULL;
    }
  }

  return stringMap;
}

uint StringMap_count(const StringMap stringMap)
{
  uint count;
  uint i;

  assert(stringMap != NULL);
  assert(stringMap->entries != NULL);

  count = 0L;
  for (i = 0; i < stringMap->size; i++)
  {
    if (stringMap->entries[i].name != NULL)
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
  assert(stringMap->entries != NULL);

  stringMapEntry = NULL;
  for (i = 0; i < stringMap->size; i++)
  {
    if (stringMap->entries[i].name != NULL)
    {
      if (index > 0)
      {
        index--;
      }
      else
      {
        stringMapEntry = &stringMap->entries[i];
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

StringMapTypes StringMap_indexType(const StringMap stringMap, uint index)
{
  const StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);

  stringMapEntry = StringMap_index(stringMap,index);
  return (stringMapEntry != NULL) ? stringMapEntry->type : STRINGMAP_TYPE_NONE;
}

StringMapValue StringMap_indexValue(const StringMap stringMap, uint index)
{
  const StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);

  stringMapEntry = StringMap_index(stringMap,index);
  return (stringMapEntry != NULL) ? stringMapEntry->value : STRINGMAP_VALUE_NONE;
}

#ifdef NDEBUG
void StringMap_putText(StringMap stringMap, const char *name, ConstString text)
#else /* not NDEBUG */
void __StringMap_putText(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, ConstString text)
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
    stringMapEntry->type             = STRINGMAP_TYPE_NONE;
    stringMapEntry->value.text       = String_duplicate(text);
    stringMapEntry->value.quotedFlag = FALSE;
  }
}

#ifdef NDEBUG
void StringMap_putTextCString(StringMap stringMap, const char *name, const char *text)
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
    stringMapEntry->type             = STRINGMAP_TYPE_NONE;
    stringMapEntry->value.text       = String_newCString(text);
    stringMapEntry->value.quotedFlag = FALSE;
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
    stringMapEntry->type             = STRINGMAP_TYPE_STRING;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.p     = value;
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
    stringMapEntry->type             = STRINGMAP_TYPE_INT;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.i     = value;
  }
}

#ifdef NDEBUG
void StringMap_putLong(StringMap stringMap, const char *name, long value)
#else /* not NDEBUG */
void __StringMap_putLong(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, long value)
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
    stringMapEntry->type             = STRINGMAP_TYPE_INT64;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.l     = value;
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
    stringMapEntry->type             = STRINGMAP_TYPE_INT64;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.l     = value;
  }
}

#ifdef NDEBUG
void StringMap_putUInt(StringMap stringMap, const char *name, uint value)
#else /* not NDEBUG */
void __StringMap_putUInt(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, uint value)
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
    stringMapEntry->type             = STRINGMAP_TYPE_UINT;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.ui    = value;
  }
}

#ifdef NDEBUG
void StringMap_putULong(StringMap stringMap, const char *name, ulong value)
#else /* not NDEBUG */
void __StringMap_putULong(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, ulong value)
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
    stringMapEntry->type             = STRINGMAP_TYPE_UINT64;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.ul    = value;
  }
}

#ifdef NDEBUG
void StringMap_putUInt64(StringMap stringMap, const char *name, uint64 value)
#else /* not NDEBUG */
void __StringMap_putUInt64(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, uint64 value)
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
    stringMapEntry->type             = STRINGMAP_TYPE_UINT64;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.ul    = value;
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
    stringMapEntry->type             = STRINGMAP_TYPE_DOUBLE;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.d     = value;
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
    stringMapEntry->type             = STRINGMAP_TYPE_BOOL;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.b     = value;
  }
}

#ifdef NDEBUG
void StringMap_putFlag(StringMap stringMap, const char *name, ulong value)
#else /* not NDEBUG */
void __StringMap_putFlag(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, ulong value)
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
    stringMapEntry->type             = STRINGMAP_TYPE_FLAG;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.flag  = value;
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
    stringMapEntry->type             = STRINGMAP_TYPE_CHAR;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.c     = value;
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
    stringMapEntry->type             = STRINGMAP_TYPE_CSTRING;
    stringMapEntry->value.text       = NULL;
    stringMapEntry->value.quotedFlag = FALSE;
    stringMapEntry->value.data.s     = stringDuplicate(value);
  }
}

#ifdef NDEBUG
void StringMap_putString(StringMap stringMap, const char *name, ConstString value)
#else /* not NDEBUG */
void __StringMap_putString(const char *__fileName__, ulong __lineNb__, StringMap stringMap, const char *name, ConstString value)
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
    stringMapEntry->value.quotedFlag  = FALSE;
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
    stringMapEntry->type             = STRINGMAP_TYPE_DATA;
    stringMapEntry->value.text       = stringMapFormatFunction(data,stringMapFormatUserData);
    stringMapEntry->value.quotedFlag = FALSE;
  }
}

#ifdef NDEBUG
void StringMap_putValue(StringMap stringMap, const char *name, StringMapTypes type, const StringMapValue *value)
#else /* not NDEBUG */
void __StringMap_putValue(const char *__fileName__, ulong __lineNb__,StringMap stringMap, const char *name, StringMapTypes type, const StringMapValue *value)
#endif /* NDEBUG */
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(value != NULL);

  #ifdef NDEBUG
    stringMapEntry = addStringMapEntry(stringMap,name);
  #else /* not NDEBUG */
    stringMapEntry = addStringMapEntry(__fileName__,__lineNb__,stringMap,name);
  #endif /* NDEBUG */

  if (stringMapEntry != NULL)
  {
    stringMapEntry->type             = type;
    stringMapEntry->value.text       = String_duplicate(value->text);
    stringMapEntry->value.quotedFlag = value->quotedFlag;
    switch (type)
    {
      case STRINGMAP_TYPE_NONE:                                                                              break;
      case STRINGMAP_TYPE_INT:     stringMapEntry->value.data.i      = value->data.i;                        break;
      case STRINGMAP_TYPE_INT64:   stringMapEntry->value.data.l      = value->data.l;                        break;
      case STRINGMAP_TYPE_UINT:    stringMapEntry->value.data.ui     = value->data.ui;                       break;
      case STRINGMAP_TYPE_UINT64:  stringMapEntry->value.data.ul     = value->data.ul;                       break;
      case STRINGMAP_TYPE_DOUBLE:  stringMapEntry->value.data.d      = value->data.d;                        break;
      case STRINGMAP_TYPE_BOOL:    stringMapEntry->value.data.b      = value->data.b;                        break;
      case STRINGMAP_TYPE_FLAG:    stringMapEntry->value.data.flag   = value->data.flag;                     break;
      case STRINGMAP_TYPE_CHAR:    stringMapEntry->value.data.c      = value->data.c;                        break;
      case STRINGMAP_TYPE_CSTRING: stringMapEntry->value.data.s      = stringDuplicate(value->data.s);       break;
      case STRINGMAP_TYPE_STRING:  stringMapEntry->value.data.string = String_duplicate(value->data.string); break;
      case STRINGMAP_TYPE_DATA:    stringMapEntry->value.data.p      = value->data.p;                        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }
  }
}

ConstString StringMap_getText(const StringMap stringMap, const char *name, ConstString defaultValue)
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

bool StringMap_getLong(const StringMap stringMap, const char *name, long *data, long defaultValue)
{
  StringMapEntry *stringMapEntry;
  char           *nextData;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    (*data) = (ulong)strtoll(String_cString(stringMapEntry->value.text),&nextData,0);
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
    (*data) = (int64)strtoll(String_cString(stringMapEntry->value.text),&nextData,0);
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

bool StringMap_getULong(const StringMap stringMap, const char *name, ulong *data, ulong defaultValue)
{
  StringMapEntry *stringMapEntry;
  char           *nextData;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    (*data) = (ulong)strtol(String_cString(stringMapEntry->value.text),&nextData,0);
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
  StringMapEntry *stringMapEntry;
  uint           i;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    (*data) = FALSE;
    for (i = 0; i < SIZE_OF_ARRAY(TRUE_STRINGS); i++)
    {
      if (String_equalsIgnoreCaseCString(stringMapEntry->value.text,TRUE_STRINGS[i]))
      {
        (*data) = TRUE;
        break;
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

bool StringMap_getFlag(const StringMap stringMap, const char *name, ulong *data, ulong value)
{
  StringMapEntry *stringMapEntry;
  uint           i;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);


  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    (*data) &= ~value ;
    for (i = 0; i < SIZE_OF_ARRAY(TRUE_STRINGS); i++)
    {
      if (String_equalsIgnoreCaseCString(stringMapEntry->value.text,TRUE_STRINGS[i]))
      {
        (*data) |= value;
        break;
      }
    }
    return TRUE;
  }
  else
  {
    (*data) &= ~value;
    return FALSE;
  }
}

bool StringMap_getEnum(const StringMap stringMap, const char *name, void *data, StringMapParseEnumFunction stringMapParseEnumFunction, void *stringMapParseEnumUserData, uint defaultValue)
{
  StringMapEntry *stringMapEntry;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);
  assert(stringMapParseEnumFunction != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if (   (stringMapEntry != NULL)
      && (stringMapEntry->value.text != NULL)
      && stringMapParseEnumFunction(String_cString(stringMapEntry->value.text),(uint*)data,stringMapParseEnumUserData)
     )
  {
    return TRUE;
  }
  else
  {
    (*(int*)data) = defaultValue;
    return FALSE;
  }
}

bool StringMap_getEnumSet(const StringMap stringMap, const char *name, uint64 *data, StringMapParseEnumFunction stringMapParseEnumFunction, void *stringMapParseEnumUserData, uint64 allValue, const char *separatorChars, uint64 defaultValue)
{
  StringMapEntry  *stringMapEntry;
  StringTokenizer stringTokenizer;
  ConstString     token;
  uint            value;

  assert(stringMap != NULL);
  assert(name != NULL);
  assert(data != NULL);
  assert(stringMapParseEnumFunction != NULL);

  stringMapEntry = findStringMapEntry(stringMap,name);
  if ((stringMapEntry != NULL) && (stringMapEntry->value.text != NULL))
  {
    (*data) = 0LL;

    String_initTokenizer(&stringTokenizer,stringMapEntry->value.text,STRING_BEGIN,separatorChars,NULL,TRUE);
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      if      (String_equalsCString(token,"*"))
      {
        (*data) = allValue;
      }
      else if (stringMapParseEnumFunction(String_cString(token),&value,stringMapParseEnumUserData))
      {
        (*data) |= (1 << value);
      }
      else
      {
        String_doneTokenizer(&stringTokenizer);
        return FALSE;
      }
    }
    String_doneTokenizer(&stringTokenizer);

    return TRUE;
  }
  else
  {
    (*data) = defaultValue;
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

bool StringMap_getString(const StringMap stringMap, const char *name, String data, const char *defaultValue)
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
      String_setCString(data,defaultValue);
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

bool StringMap_contains(const StringMap stringMap, const char *name)
{
  assert(stringMap != NULL);
  assert(name != NULL);

  return (findStringMapEntry(stringMap,name) != NULL);
}

bool StringMap_parse(StringMap stringMap, ConstString string, const char *assignChars, const char *quoteChars, const char *separatorChars, ulong index, long *nextIndex)
{
  assert(stringMap != NULL);

  return StringMap_parseCString(stringMap,String_cString(string),assignChars,quoteChars,separatorChars,index,nextIndex);
}

bool StringMap_parseCString(StringMap stringMap, const char *s, const char *assignChars, const char *quoteChars, const char *separatorChars, ulong index, long *nextIndex)
{
  const char *quoteChar;
  uint       length;
  String     name;
  String     value;
  int        i;

  assert(stringMap != NULL);
  assert(s != NULL);

  // clear map
  StringMap_clear(stringMap);

  // parse
  length = strlen(s);
  name   = String_new();
  value  = String_new();
  while (index < length)
  {
    // skip spaces, separators
    while (   (index < length)
           && (   isspace(s[index])
               || ((separatorChars != NULL) && (strchr(separatorChars,s[index]) != NULL))
              )
          )
    {
      index++;
    }

    if (index < length)
    {
      // get name
      String_clear(name);
      if (   (index < length)
          && (!isalpha(s[index]) && (s[index] != '_'))
         )
      {
        if (nextIndex != NULL) (*nextIndex) = index;
        String_delete(value);
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

      // check for assign
      if ((index >= length) || (strchr(assignChars,s[index]) == NULL))
      {
        if (nextIndex != NULL) (*nextIndex) = index;
        String_delete(value);
        String_delete(name);
        return FALSE;
      }
      index++;

      // get value as text
      String_clear(value);
      while (   (index < length)
             && !isspace(s[index])
             && ((separatorChars == NULL) || (strchr(separatorChars,s[index]) == NULL))
            )
      {
        if (   (s[index] == STRING_ESCAPE_CHARACTER)
            && ((index+1) < length)
            && (strchr(quoteChars,s[index+1]) != NULL)
           )
        {
          // quoted quote
          String_appendChar(value, s[index+1]);
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
                  && (s[index] == STRING_ESCAPE_CHARACTER)
                 )
              {
                index++;

                if      (strchr(quoteChars,s[index]) != NULL)
                {
                  // quoted quote
                  String_appendChar(value,s[index]);
                }
                else
                {
                  // search for known mapped character
                  i = 0;
                  while ((i < STRING_ESCAPE_CHARACTER_MAP_LENGTH) && (STRING_ESCAPE_CHARACTERS_MAP_TO[i] != s[index]))
                  {
                    i++;
                  }

                  if (i < STRING_ESCAPE_CHARACTER_MAP_LENGTH)
                  {
                    // mapped character
                    String_appendChar(value,STRING_ESCAPE_CHARACTERS_MAP_FROM[i]);
                  }
                  else
                  {
                    // non-mapped character
                    String_appendChar(value,s[index]);
                  }
                }
              }
              else
              {
                String_appendChar(value,s[index]);
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
            String_appendChar(value,s[index]);
            index++;
          }
        }
      }

      // store value
      StringMap_putText(stringMap,name->data,value);
    }
  }

  if (nextIndex != NULL)
  {
    (*nextIndex) = index;
  }

  // free resources
  String_delete(value);
  String_delete(name);

  return TRUE;
}

void* const *StringMap_valueArray(const StringMap stringMap)
{
  uint count;
  void **valueArray;
  uint n,i;

  assert(stringMap != NULL);
  assert(stringMap->entries != NULL);

  count = StringMap_count(stringMap);

  valueArray = (void**)malloc(count*sizeof(char*));
  if (valueArray != NULL)
  {
    n = 0;
    for (i = 0; i < stringMap->size; i++)
    {
      if (stringMap->entries[i].name != NULL)
      {
        assert(n < count);
        valueArray[n] = stringMap->entries[i].value.data.p; n++;
      }
    }
  }

  return valueArray;
}

bool StringMap_parseEnumNumber(const char *name, uint *value)
{
  assert(name != NULL);
  assert(value != NULL);

  (*value) = (uint)atoi(name);

  return TRUE;
}

#ifndef NDEBUG
void StringMap_debugDump(FILE *handle, uint indent, const StringMap stringMap)
{
  uint i,j;

  assert(stringMap != NULL);
  assert(stringMap->entries != NULL);

  for (i = 0; i < stringMap->size; i++)
  {
    if (stringMap->entries[i].name != NULL)
    {
      assert(stringMap->entries[i].type < SIZE_OF_ARRAY(STRING_MAP_TYPE_NAMES));
      for (j = 0; j < indent; j++) fputc(' ',handle);
      fprintf(handle,
              "#%3u (%-8s): %s = '%s' (0x%"PRIuPTR")\n",
              i,
              STRING_MAP_TYPE_NAMES[stringMap->entries[i].type],
              stringMap->entries[i].name,
              String_cString(stringMap->entries[i].value.text),
              (uintptr_t)stringMap->entries[i].value.data.p
             );
    }
  }
}

void StringMap_debugPrint(uint indent, const StringMap stringMap)
{
  StringMap_debugDump(stderr,indent,stringMap);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
