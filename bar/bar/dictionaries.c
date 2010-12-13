/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/dictionaries.c,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: dictionary functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"

#include "dictionaries.h"

/****************** Conditional compilation switches *******************/

#define COLLISION_ALGORITHM_LINEAR_PROBING    1
#define COLLISION_ALGORITHM_QUADRATIC_PROBING 2
#define COLLISION_ALGORITHM_REHASH            3

//#define COLLISION_ALGORITHM COLLISION_ALGORITHM_LINEAR_PROBING
//#define COLLISION_ALGORITHM COLLISION_ALGORITHM_QUADRATIC_PROBING
#define COLLISION_ALGORITHM COLLISION_ALGORITHM_REHASH

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
#define DATA_START_SIZE (16*1024)
#define DATA_DELTA_SIZE ( 4*1024)
#define MAX_DATA_SIZE   (2*1024*1024*1024)

/* hash table sizes */
LOCAL const uint TABLE_SIZES[] =
{
  1031,
  2053,
  4099,
  8209,
  16411,
  32771,
  65537,
  131101,
  262147,
  524309
};

#if COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING
  #define LINEAR_PROBING_COUNT 4
#endif
#if COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
  #define QUADRATIC_PROBING_COUNT 4
#endif
#if COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH
  #define REHASHING_COUNT 4
#endif

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#if COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
/***********************************************************************\
* Name   : modulo
* Purpose: n mod m
* Input  : n,m - numbers
* Output : -
* Return : n mod m
* Notes  : -
\***********************************************************************/

LOCAL_INLINE ulong modulo(ulong n, ulong m)
{
  return n%m;
}

/***********************************************************************\
* Name   : addModulo
* Purpose: add and modulo
* Input  : n,d,m - numbers
* Output : -
* Return : (n+d) mod m
* Notes  : -
\***********************************************************************/

LOCAL_INLINE ulong addModulo(ulong n, uint d, ulong m)
{
  return (n+d)%m;
}

/***********************************************************************\
* Name   : subModulo
* Purpose: sub and modulo
* Input  : n,d,m - numbers
* Output : -
* Return : (n-d) mod m
* Notes  : -
\***********************************************************************/

LOCAL_INLINE ulong subModulo(ulong n, uint d, ulong m)
{
  return (n+m-d)%m;
}
#endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING */

/***********************************************************************\
* Name   : rotHash
* Purpose: rotate hash value
* Input  : hash - hash value
*          n    - number of bits to rotate hash value
* Output : -
* Return : rotated hash value
* Notes  : -
\***********************************************************************/

LOCAL_INLINE ulong rotHash(ulong hash, int n)
{
  uint shift;

  assert(n < 32);

  shift = 32-n;
  return ((hash & (0xFFFFffff << shift)) >> shift) | (hash << n);
}

/***********************************************************************\
* Name   : calculateHash
* Purpose: calculate hash
* Input  : keyData   - key data
*          keyLength - length of key data
* Output : -
* Return : hash value
* Notes  : -
\***********************************************************************/

LOCAL ulong calculateHash(const void *keyData, ulong keyLength)
{
  ulong      hashBytes[4];
  const byte *p;
  uint       z;

  assert(keyData != NULL);

  p = (const byte*)keyData;

  hashBytes[0] = (keyLength > 0)?(*p):0; p++;
  hashBytes[1] = (keyLength > 1)?(*p):0; p++;
  hashBytes[2] = (keyLength > 2)?(*p):0; p++;
  hashBytes[3] = (keyLength > 3)?(*p):0; p++;
  for (z = 4; z < keyLength; z++)
  {
    hashBytes[z%4] ^= (*p); p++;
  }

  return (ulong)(hashBytes[3] << 24) |
         (ulong)(hashBytes[2] << 16) |
         (ulong)(hashBytes[1] <<  8) |
         (ulong)(hashBytes[0] <<  0);
}

/***********************************************************************\
* Name   : equalsEntry
* Purpose: check if entry is equal to data
* Input  : entry                     - entry
*          hash                      - data hash value
*          keyData                   - key data
*          keyLength                 - length of key data
*          dictionaryCompareFunction - compare function or NULL for
*                                      default compare
*          dictionaryCompareUserData - compare function user data
* Output : -
* Return : TRUE if entry is equal, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool equalsEntry(const DictionaryEntry     *entry,
                              ulong                     hash,
                              const void                *keyData,
                              ulong                     keyLength,
                              DictionaryCompareFunction dictionaryCompareFunction,
                              void                      *dictionaryCompareUserData
                             )
{
  assert(entry != NULL);
  assert(keyData != NULL);

  if ((hash == entry->hash) && (entry->keyLength == keyLength))
  {
    if (dictionaryCompareFunction != NULL)
    {
      if (dictionaryCompareFunction(dictionaryCompareUserData,entry->keyData,keyData,keyLength))
      {
        return TRUE;
      }
    }
    else
    {
      if (memcmp(entry->keyData,keyData,keyLength) == 0)
      {
        return TRUE;
      }
    }
  }

  return FALSE;
}

/***********************************************************************\
* Name   : findEntryIndex
* Purpose: find entry index of entry in table
* Input  : entryTable                - entry table
*          hash                      - data hash value
*          keyData                   - key data
*          keyLength                 - length of key data
*          dictionaryCompareFunction - compare function or NULL for
*                                      default compare
*          dictionaryCompareUserData - compare function user data
* Output : -
* Return : index of -1 if entry not found
* Notes  : -
\***********************************************************************/

LOCAL int findEntryIndex(DictionaryEntryTable      *entryTable,
                         ulong                     hash,
                         const void                *keyData,
                         ulong                     keyLength,
                         DictionaryCompareFunction dictionaryCompareFunction,
                         void                      *dictionaryCompareUserData
                        )
{
  uint z,i;
  int  entryIndex;

  assert(entryTable != NULL);
  assert(keyData != NULL);

  for (z = 0; z <= entryTable->sizeIndex; z++)
  {
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING
      for (i = 0; i < LINEAR_PROBING_COUNT; i++)
      {
        entryIndex = addModulo(hash,i,TABLE_SIZES[z]);
        if (equalsEntry(&entryTable->entries[entryIndex],
                        hash,
                        keyData,
                        keyLength,
                        dictionaryCompareFunction,
                        dictionaryCompareUserData
                       )
           )
        {
          return entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING */
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
      entryIndex = modulo(hash,TABLE_SIZES[z]);
      if (equalsEntry(&entryTable->entries[entryIndex],
                      hash,
                      keyData,
                      keyLength,
                      dictionaryCompareFunction,
                      dictionaryCompareUserData
                     )
         )
      {
        return entryIndex;
      }
      for (i = 1; i < QUADRATIC_PROBING_COUNT; i++)
      {
        entryIndex = addModulo(hash,i*i,TABLE_SIZES[z]);
        if (equalsEntry(&entryTable->entries[entryIndex],
                        hash,
                        keyData,
                        keyLength,
                        dictionaryCompareFunction,
                        dictionaryCompareUserData
                       )
           )
        {
          return entryIndex;
        }
        entryIndex = subModulo(hash,i*i,TABLE_SIZES[z]);
        if (equalsEntry(&entryTable->entries[entryIndex],
                        hash,
                        keyData,
                        keyLength,
                        dictionaryCompareFunction,
                        dictionaryCompareUserData
                       )
           )
        {
          return entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING */
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH
      for (i = 0; i < REHASHING_COUNT; i++)
      {
        entryIndex = rotHash(hash,i)%TABLE_SIZES[z];
        if (equalsEntry(&entryTable->entries[entryIndex],
                        hash,
                        keyData,
                        keyLength,
                        dictionaryCompareFunction,
                        dictionaryCompareUserData
                       )
           )
        {
          return entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH */
  }

  return -1;
}

/***********************************************************************\
* Name   : findFreeEntryIndex
* Purpose: find free index in hash table
* Input  : entryTable - entry table
*          hash       - hash value
* Output : -
* Return : index of -1 if not free entry in table
* Notes  : -
\***********************************************************************/

LOCAL int findFreeEntryIndex(DictionaryEntryTable *entryTable,
                             ulong                hash
                            )
{
  uint z,i;
  int  entryIndex;

  assert(entryTable != NULL);

  for (z = 0; z <= entryTable->sizeIndex; z++)
  {
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING
      for (i = 0; i < LINEAR_PROBING_COUNT; i++)
      {
        entryIndex = addModulo(hash,i,TABLE_SIZES[z]);
        if (entryTable->entries[entryIndex].data == NULL)
        {
          return entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING */
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
      entryIndex = modulo(hash,TABLE_SIZES[z]);
      if (entryTable->entries[entryIndex].data == NULL)
      {
        return entryIndex;
      }
      for (i = 0; i < QUADRATIC_PROBING_COUNT; i++)
      {
        entryIndex = addModulo(hash,i*i,TABLE_SIZES[z]);
        if (entryTable->entries[entryIndex].data == NULL)
        {
          return entryIndex;
        }
        entryIndex = subModulo(hash,i*i,TABLE_SIZES[z]);
        if (entryTable->entries[entryIndex].data == NULL)
        {
          return entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING */
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH
      for (i = 0; i < REHASHING_COUNT; i++)
      {
        entryIndex = rotHash(hash,i)%TABLE_SIZES[z];
        if (entryTable->entries[entryIndex].data == NULL)
        {
          return entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH */
  }

  return -1;
}

/***********************************************************************\
* Name   : findEntry
* Purpose: find entry in hash table
* Input  : dictionary                - dictionary
*          hash                      - data hash value
*          keyData                   - key data
*          keyLength                 - length of key data
* Output : dictionaryEntryTable - dictionary entry table
*          index                - index in table
* Return : TRUE if entry found, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool findEntry(Dictionary           *dictionary,
                     ulong                hash,
                     const void           *keyData,
                     ulong                keyLength,
                     DictionaryEntryTable **dictionaryEntryTable,
                     uint                 *index
                    )
{
  uint z;
  uint tableIndex;
  int  i;

  assert(dictionary != NULL);
  assert(keyData != NULL);
  assert(dictionaryEntryTable != NULL);
  assert(index != NULL);

  (*dictionaryEntryTable) = NULL;
  (*index)                = -1;
  z = 0;
  while ((z < dictionary->entryTableCount) && ((*dictionaryEntryTable) == NULL))
  {
    tableIndex = (hash+z)%dictionary->entryTableCount;
    i = findEntryIndex(&dictionary->entryTables[tableIndex],
                       hash,
                       keyData,
                       keyLength,
                       dictionary->dictionaryCompareFunction,
                       dictionary->dictionaryCompareUserData
                      );
    if (i >= 0)
    {
      (*dictionaryEntryTable) = &dictionary->entryTables[tableIndex];
      (*index)                = i;
    }
    z++;
  }

  return ((*dictionaryEntryTable) !=NULL);
}

/***********************************************************************\
* Name   : findFreeEntry
* Purpose: find free entry in hash table
* Input  : dictionary - dictionary
*          hash       - data hash value
* Output : dictionaryEntryTable - dictionary entry table
*          index                - index in table
* Return : TRUE if entry found, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool findFreeEntry(Dictionary           *dictionary,
                         ulong                hash,
                         DictionaryEntryTable **dictionaryEntryTable,
                         uint                 *index
                        )
{
  uint z;
  uint tableIndex;
  int  i;

  assert(dictionary != NULL);
  assert(dictionaryEntryTable != NULL);
  assert(index != NULL);

  (*dictionaryEntryTable) = NULL;
  (*index)                = -1;
  z = 0;
  while ((z < dictionary->entryTableCount) && ((*dictionaryEntryTable) == NULL))
  {
    tableIndex = (hash+z)%dictionary->entryTableCount;
    i = findFreeEntryIndex(&dictionary->entryTables[tableIndex],
                           hash
                          );
    if (i >= 0)
    {
      (*dictionaryEntryTable) = &dictionary->entryTables[tableIndex];
      (*index)                = i;
    }
    z++;
  }

  return ((*dictionaryEntryTable) !=NULL);
}

/***********************************************************************\
* Name   : growTable
* Purpose: grow table size
* Input  : entries - current table entries
*          oldSize - old number of entries in table
*          newSize - new number of entries in table
* Output : -
* Return : new table entries or NULL on insufficient memory
* Notes  : -
\***********************************************************************/

LOCAL DictionaryEntry *growTable(DictionaryEntry *entries, uint oldSize, uint newSize)
{
  assert(entries != NULL);
  assert(newSize > oldSize);

  entries = realloc(entries,newSize*sizeof(DictionaryEntry));
  if (entries != NULL)
  {
    memset(&entries[oldSize],0,(newSize-oldSize)*sizeof(DictionaryEntry));
  }

  return entries;
}

/*---------------------------------------------------------------------*/

bool Dictionary_init(Dictionary                *dictionary,
                     DictionaryCompareFunction dictionaryCompareFunction,
                     void                      *dictionaryCompareUserData
                    )
{
  assert(dictionary != NULL);

  dictionary->entryTables = (DictionaryEntryTable*)malloc(sizeof(DictionaryEntryTable)*1);
  if (dictionary->entryTables == NULL)
  {
    return FALSE;
  }
  dictionary->entryTableCount = 1;

  dictionary->entryTables[0].entries = (DictionaryEntry*)calloc(TABLE_SIZES[0],sizeof(DictionaryEntry));
  if (dictionary->entryTables[0].entries == NULL)
  {
    free(dictionary->entryTables);
    return FALSE;
  }
  dictionary->entryTables[0].sizeIndex  = 0;
  dictionary->entryTables[0].entryCount = 0;
//fprintf(stderr,"%s,%d: init entries %p\n",__FILE__,__LINE__,dictionary->entryTables[0].entries);

  dictionary->dictionaryCompareFunction = dictionaryCompareFunction;
  dictionary->dictionaryCompareUserData = dictionaryCompareUserData;

  return TRUE;
}

void Dictionary_done(Dictionary             *dictionary,
                     DictionaryFreeFunction dictionaryFreeFunction,
                     void                   *dictionaryFreeUserData
                    )
{
  uint z;
  uint index;

  assert(dictionary != NULL);
  assert(dictionary->entryTables != NULL);

  for (z = 0; z < dictionary->entryTableCount; z++)
  {
    assert(dictionary->entryTables[z].entries != NULL);

    for (index = 0; index < TABLE_SIZES[dictionary->entryTables[z].sizeIndex]; index++)
    {
      if (dictionary->entryTables[z].entries[index].data != NULL)
      {
        if (dictionaryFreeFunction != NULL)
        {
          dictionaryFreeFunction(dictionary->entryTables[z].entries[index].data,
                                 dictionary->entryTables[z].entries[index].length,
                                 dictionaryFreeUserData
                                );
        }
        free(dictionary->entryTables[z].entries[index].data);
        free(dictionary->entryTables[z].entries[index].keyData);
      }
    }
    free(dictionary->entryTables[z].entries);
  }
  free(dictionary->entryTables);
}

void Dictionary_clear(Dictionary             *dictionary,
                      DictionaryFreeFunction dictionaryFreeFunction,
                      void                   *dictionaryFreeUserData
                     )
{
  uint z;
  uint index;

  assert(dictionary != NULL);
  assert(dictionary->entryTables != NULL);

  for (z = 0; z < dictionary->entryTableCount; z++)
  {
    assert(dictionary->entryTables[z].entries != NULL);

    for (index = 0; index < TABLE_SIZES[dictionary->entryTables[z].sizeIndex]; index++)
    {
      if (dictionary->entryTables[z].entries[index].data != NULL)
      {
        if (dictionaryFreeFunction != NULL)
        {
          dictionaryFreeFunction(dictionary->entryTables[z].entries[index].data,
                                 dictionary->entryTables[z].entries[index].length,
                                 dictionaryFreeUserData
                                );
        }
        free(dictionary->entryTables[z].entries[index].data);
        free(dictionary->entryTables[z].entries[index].keyData);

        dictionary->entryTables[z].entries[index].data = NULL;
      }
    }
  }
}

ulong Dictionary_count(const Dictionary *dictionary)
{
  ulong count;
  uint  z;

  assert(dictionary != NULL);
  assert(dictionary->entryTables != NULL);

  count = 0;
  for (z = 0; z < dictionary->entryTableCount; z++)
  {
    count += dictionary->entryTables[z].entryCount;
  }

  return count;
}

bool Dictionary_add(Dictionary *dictionary,
                    const void *keyData,
                    ulong      keyLength,
                    const void *data,
                    ulong      length
                   )
{
  ulong                hash;
  DictionaryEntryTable *dictionaryEntryTable;
  uint                 entryIndex;
  void                 *newData;
  uint                 tableIndex;
  uint                 newSizeIndex;
  uint                 z,i;
  DictionaryEntry      *newEntries;
  DictionaryEntryTable *entryTables;

  assert(dictionary != NULL);

  hash = calculateHash(keyData,keyLength);

  /* update entry */
  if (findEntry(dictionary,hash,keyData,keyLength,&dictionaryEntryTable,&entryIndex))
  {
    assert(dictionaryEntryTable->entries != NULL);

    // allocate/resize data memory
    if (dictionaryEntryTable->entries[entryIndex].length != length)
    {
      newData = realloc(dictionaryEntryTable->entries[entryIndex].data,length);
      if (newData == NULL)
      {
        return FALSE;
      }
      dictionaryEntryTable->entries[entryIndex].data   = newData;
      dictionaryEntryTable->entries[entryIndex].length = length;
    }

    // copy data
    memcpy(dictionaryEntryTable->entries[entryIndex].data,data,length);

    return TRUE;
  }

  /* add entry in existing table */
  if (findFreeEntry(dictionary,hash,&dictionaryEntryTable,&entryIndex))
  {   
    assert(dictionaryEntryTable->entries != NULL);

    // allocate key memory
    dictionaryEntryTable->entries[entryIndex].keyData = malloc(keyLength);
    if (dictionaryEntryTable->entries[entryIndex].keyData == NULL)
    {
      return FALSE;
    }

    // allocate data memory
    dictionaryEntryTable->entries[entryIndex].data = malloc(length);
    if (dictionaryEntryTable->entries[entryIndex].data == NULL)
    {
      free(dictionaryEntryTable->entries[entryIndex].keyData);
      return FALSE;
    }

    // copy key data
    dictionaryEntryTable->entries[entryIndex].hash = hash;
    memcpy(dictionaryEntryTable->entries[entryIndex].keyData,keyData,keyLength);
    dictionaryEntryTable->entries[entryIndex].keyLength = keyLength;

    // copy data
    memcpy(dictionaryEntryTable->entries[entryIndex].data,data,length);
    dictionaryEntryTable->entries[entryIndex].length = length;

    dictionaryEntryTable->entryCount++;

    return TRUE;
  }

  /* find a table which can be resized and where new entry can be
     stored in extended table, store entry in extended table
  */
  dictionaryEntryTable = NULL;
  z = 0;
  while ((z < dictionary->entryTableCount) && (dictionaryEntryTable == NULL))
  {
    assert(dictionary->entryTables != NULL);

    tableIndex = (hash+z)%dictionary->entryTableCount;
    newSizeIndex = dictionary->entryTables[tableIndex].sizeIndex+1;
    while ((newSizeIndex < SIZE_OF_ARRAY(TABLE_SIZES)) && (dictionaryEntryTable == NULL))
    {
      #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING
        entryIndex = 0;
        i = 0;
        while ((i < LINEAR_PROBING_COUNT) && (entryIndex < TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex]))
        {
          entryIndex = addModulo(hash,i,TABLE_SIZES[newSizeIndex]);
          i++;
        }
      #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING */
      #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
        entryIndex = modulo(hash,TABLE_SIZES[newSizeIndex]);
        if (entryIndex < TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex])
        {
          i = 0;
          while (i < QUADRATIC_PROBING_COUNT)
          {
            entryIndex = addModulo(hash,i*i,TABLE_SIZES[newSizeIndex]);
            if (entryIndex >= TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex]) break;
            entryIndex = subModulo(hash,i*i,TABLE_SIZES[newSizeIndex]);
            if (entryIndex >= TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex]) break;
            i++;
          }
        }
      #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING */
      #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH
        entryIndex = 0;
        i = 0;
        while ((i < REHASHING_COUNT) && (entryIndex < TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex]))
        {
          entryIndex = rotHash(hash,i)%TABLE_SIZES[newSizeIndex];
          i++;
        }
      #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH */
      if (entryIndex >= TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex])
      {
//fprintf(stderr,"%s,%d: vor grow %p\n",__FILE__,__LINE__,dictionary->entryTables[tableIndex].entries);
        newEntries = growTable(dictionary->entryTables[tableIndex].entries,
                               TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex],
                               TABLE_SIZES[newSizeIndex]
                              );
        if (newEntries != NULL)
        {
          dictionary->entryTables[tableIndex].entries   = newEntries;
          dictionary->entryTables[tableIndex].sizeIndex = newSizeIndex;

          dictionaryEntryTable = &dictionary->entryTables[tableIndex];
        }
//fprintf(stderr,"%s,%d: nach grow %p\n",__FILE__,__LINE__,dictionary->entryTables[tableIndex].entries);
      }
      newSizeIndex++;
    }
    z++;
  }
  if (dictionaryEntryTable != NULL)
  {   
    assert(dictionaryEntryTable->entries != NULL);

    // allocate key memory
    dictionaryEntryTable->entries[entryIndex].keyData = malloc(keyLength);
    if (dictionaryEntryTable->entries[entryIndex].keyData == NULL)
    {
      return FALSE;
    }

    // allocate data memory
    dictionaryEntryTable->entries[entryIndex].data = malloc(length);
    if (dictionaryEntryTable->entries[entryIndex].data == NULL)
    {
      free(dictionaryEntryTable->entries[entryIndex].keyData);
      return FALSE;
    }

    // clopy key data
    dictionaryEntryTable->entries[entryIndex].hash = hash;
    memcpy(dictionaryEntryTable->entries[entryIndex].keyData,keyData,keyLength);
    dictionaryEntryTable->entries[entryIndex].keyLength = keyLength;

    // copy data
    memcpy(dictionaryEntryTable->entries[entryIndex].data,data,length);
    dictionaryEntryTable->entries[entryIndex].length = length;

    dictionaryEntryTable->entryCount++;

    return TRUE;
  }

  /* add new table and store entry in new table */
  newEntries = (DictionaryEntry*)calloc(TABLE_SIZES[0],sizeof(DictionaryEntry));
  if (newEntries == NULL)
  {
    return FALSE;
  }
  entryTables = (DictionaryEntryTable*)realloc(dictionary->entryTables,(dictionary->entryTableCount+1)*sizeof(DictionaryEntryTable));
  if (entryTables == NULL)
  {
    free(newEntries);
    return FALSE;
  }
  entryTables[dictionary->entryTableCount].entries    = newEntries;
  entryTables[dictionary->entryTableCount].sizeIndex  = 0;
  entryTables[dictionary->entryTableCount].entryCount = 0;
  dictionary->entryTables = entryTables;
  dictionary->entryTableCount++;

  dictionaryEntryTable = &entryTables[dictionary->entryTableCount-1];
  entryIndex = rotHash(hash,0)%TABLE_SIZES[0];
  dictionaryEntryTable->entries[entryIndex].keyData = malloc(keyLength);
  if (dictionaryEntryTable->entries[entryIndex].keyData == NULL)
  {
    return FALSE;
  }
  dictionaryEntryTable->entries[entryIndex].data = malloc(length);
  if (dictionaryEntryTable->entries[entryIndex].data == NULL)
  {
    free(dictionaryEntryTable->entries[entryIndex].keyData);
    return FALSE;
  }
  dictionaryEntryTable->entries[entryIndex].hash = hash;
  memcpy(dictionaryEntryTable->entries[entryIndex].keyData,keyData,keyLength);
  dictionaryEntryTable->entries[entryIndex].keyLength = keyLength;
  memcpy(dictionaryEntryTable->entries[entryIndex].data,data,length);
  dictionaryEntryTable->entries[entryIndex].length = length;
  dictionaryEntryTable->entryCount++;

  return TRUE;
}

void Dictionary_rem(Dictionary             *dictionary,
                    const void             *keyData,
                    ulong                  keyLength,
                    DictionaryFreeFunction dictionaryFreeFunction,
                    void                   *dictionaryFreeUserData
                   )
{
  ulong                hash;
  DictionaryEntryTable *dictionaryEntryTable;
  uint                 index;

  assert(dictionary != NULL);

  hash = calculateHash(keyData,keyLength);

  /* remove entry */
  if (findEntry(dictionary,hash,keyData,keyLength,&dictionaryEntryTable,&index))
  {
    assert(dictionaryEntryTable->entries != NULL);
    assert(dictionaryEntryTable->entryCount > 0);

    if (dictionaryFreeFunction != NULL)
    {
      dictionaryFreeFunction(dictionaryEntryTable->entries[index].data,
                             dictionaryEntryTable->entries[index].length,
                             dictionaryFreeUserData
                            );
    }
    free(dictionaryEntryTable->entries[index].data);
    free(dictionaryEntryTable->entries[index].keyData);

    dictionaryEntryTable->entries[index].data      = NULL;
    dictionaryEntryTable->entries[index].length    = 0;
    dictionaryEntryTable->entries[index].keyData   = NULL;
    dictionaryEntryTable->entries[index].keyLength = 0;

    dictionaryEntryTable->entryCount--;
  }
}

bool Dictionary_find(Dictionary *dictionary,
                     const void *keyData,
                     ulong      keyLength,
                     void       **data,
                     ulong      *length
                    )
{
  ulong                hash;
  DictionaryEntryTable *dictionaryEntryTable;
  uint                 index;

  assert(dictionary != NULL);

  hash = calculateHash(keyData,keyLength);

  if (findEntry(dictionary,hash,keyData,keyLength,&dictionaryEntryTable,&index))
  {
    assert(dictionaryEntryTable->entries != NULL);

    if (data   != NULL) (*data)   = dictionaryEntryTable->entries[index].data;
    if (length != NULL) (*length) = dictionaryEntryTable->entries[index].length;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Dictionary_contain(Dictionary *dictionary,
                        const void *keyData,
                        ulong      keyLength
                       )
{
  assert(dictionary != NULL);

  return Dictionary_find(dictionary,keyData,keyLength,NULL,NULL);
}

void Dictionary_initIterator(DictionaryIterator *dictionaryIterator,
                             const Dictionary   *dictionary
                            )
{
  assert(dictionaryIterator != NULL);
  assert(dictionary != NULL);

  dictionaryIterator->dictionary = dictionary;
  dictionaryIterator->i          = 0;
  dictionaryIterator->j          = 0;
}

void Dictionary_doneIterator(DictionaryIterator *dictionaryIterator)
{
  assert(dictionaryIterator != NULL);

  UNUSED_VARIABLE(dictionaryIterator);
}

bool Dictionary_getNext(DictionaryIterator *dictionaryIterator,
                        const void         **keyData,
                        ulong              *keyLength,
                        void               **data,
                        ulong              *length
                       )
{
  bool            foundFlag;
  DictionaryEntry *dictionaryEntry;

  assert(dictionaryIterator != NULL);

  if (keyData   != NULL) (*keyData)   = NULL;
  if (keyLength != NULL) (*keyLength) = 0;
  if (data      != NULL) (*data)      = NULL;
  if (length    != NULL) (*length)    = 0;

  foundFlag = FALSE;
  if (dictionaryIterator->i < dictionaryIterator->dictionary->entryTableCount)
  {
    assert(dictionaryIterator->dictionary->entryTables != NULL);

    do
    {
      /* get entry */
      assert(dictionaryIterator->dictionary->entryTables[dictionaryIterator->i].entries != NULL);
      dictionaryEntry = &dictionaryIterator->dictionary->entryTables[dictionaryIterator->i].entries[dictionaryIterator->j];

      /* check if used/empty */
      if (dictionaryEntry->data != NULL)
      {
        if (keyData   != NULL) (*keyData)   = dictionaryEntry->keyData;
        if (keyLength != NULL) (*keyLength) = dictionaryEntry->keyLength;
        if (data      != NULL) (*data)      = dictionaryEntry->data;
        if (length    != NULL) (*length)    = dictionaryEntry->length;
        foundFlag = TRUE;
      }

      /* next entry */    
      if (dictionaryIterator->j < TABLE_SIZES[dictionaryIterator->dictionary->entryTables[dictionaryIterator->i].sizeIndex]-1)
      {
        dictionaryIterator->j++;
      }
      else
      {
        dictionaryIterator->i++;
        dictionaryIterator->j = 0;
      }
    }
    while (!foundFlag
           && (dictionaryIterator->i < dictionaryIterator->dictionary->entryTableCount)
          );
  }

  return foundFlag;
}

bool Dictionary_iterate(Dictionary                *dictionary,
                        DictionaryIterateFunction dictionaryIterateFunction,
                        void                      *dictionaryIterateUserData
                       )
{
  DictionaryIterator dictionaryIterator;
  bool               okFlag;
  const void         *keyData;
  ulong              keyLength;
  void               *data;
  ulong              length;

  assert(dictionary != NULL);
  assert(dictionaryIterateFunction != NULL);

  okFlag = TRUE;
  Dictionary_initIterator(&dictionaryIterator,dictionary);
  while (   Dictionary_getNext(&dictionaryIterator,
                               &keyData,
                               &keyLength,
                               &data,
                               &length
                              )
         && okFlag
        )
  {
    okFlag = dictionaryIterateFunction(keyData,
                                       keyLength,
                                       data,
                                       length,
                                       dictionaryIterateUserData
                                      );
  }
  Dictionary_doneIterator(&dictionaryIterator);

  return okFlag;
}

#ifndef NDEBUG
void Dictionary_printStatistic(const Dictionary *dictionary)
{
  ulong totalEntryCount,totalIndexCount;
  uint  z;

  assert(dictionary != NULL);

  fprintf(stderr,"Dictionary statistics:\n");
  fprintf(stderr,"  tables : %d\n",dictionary->entryTableCount);

  totalEntryCount = 0;
  totalIndexCount = 0;
  for (z = 0; z < dictionary->entryTableCount; z++)
  {
    fprintf(stderr,"    table #%02d: %u entries/%u size\n",z,dictionary->entryTables[z].entryCount,TABLE_SIZES[dictionary->entryTables[z].sizeIndex]);
    totalEntryCount += dictionary->entryTables[z].entryCount;
    totalIndexCount += TABLE_SIZES[dictionary->entryTables[z].sizeIndex];
  }

  fprintf(stderr,"  total entries: %lu\n",totalEntryCount);
  fprintf(stderr,"  total size:    %lu\n",totalIndexCount);
}
#endif /* NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
