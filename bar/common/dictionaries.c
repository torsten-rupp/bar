/***********************************************************************\
*
* Contents: dictionary functions
* Systems: all
*
\***********************************************************************/

#define __DICTIONARY_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "common/semaphores.h"

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

LOCAL_INLINE ulong rotHash(ulong hash, uint n)
{
  uint shift;

  assert(n < 32);

  if (n > 0)
  {
    shift = 32-n;
    hash = ((hash & (0xFFFFffff << shift)) >> shift) | (hash << n);
  }

  return hash;
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

  if (   (hash == entry->hash)
      && (entry->keyData != NULL)
      && (entry->keyLength == keyLength))
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
        if (   !entryTable->entries[entryIndex].removeFlag
            && equalsEntry(&entryTable->entries[entryIndex],
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
      if (   !entryTable->entries[entryIndex].removeFlag
          && equalsEntry(&entryTable->entries[entryIndex],
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
        if (   !entryTable->entries[entryIndex].removeFlag
            && equalsEntry(&entryTable->entries[entryIndex],
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
        if (   !entryTable->entries[entryIndex].removeFlag
            && equalsEntry(&entryTable->entries[entryIndex],
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
        if (   !entryTable->entries[entryIndex].removeFlag
            && equalsEntry(&entryTable->entries[entryIndex],
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
        if (entryTable->entries[entryIndex].keyData == NULL)
        {
          return entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING */
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
      entryIndex = modulo(hash,TABLE_SIZES[z]);
      if (entryTable->entries[entryIndex].keyData == NULL)
      {
        return entryIndex;
      }
      for (i = 0; i < QUADRATIC_PROBING_COUNT; i++)
      {
        entryIndex = addModulo(hash,i*i,TABLE_SIZES[z]);
        if (entryTable->entries[entryIndex].keyData == NULL)
        {
          return entryIndex;
        }
        entryIndex = subModulo(hash,i*i,TABLE_SIZES[z]);
        if (entryTable->entries[entryIndex].keyData == NULL)
        {
          return entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING */
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH
      for (i = 0; i < REHASHING_COUNT; i++)
      {
        entryIndex = rotHash(hash,i)%TABLE_SIZES[z];
        if (entryTable->entries[entryIndex].keyData == NULL)
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
    memClear(&entries[oldSize],(newSize-oldSize)*sizeof(DictionaryEntry));
  }

  return entries;
}

/*---------------------------------------------------------------------*/

#ifdef NDEBUG
  bool Dictionary_init(Dictionary                *dictionary,
                       DictionaryCopyFunction    dictionaryCopyFunction,
                       void                      *dictionaryCopyUserData,
                       DictionaryFreeFunction    dictionaryFreeFunction,
                       void                      *dictionaryFreeUserData,
                       DictionaryCompareFunction dictionaryCompareFunction,
                       void                      *dictionaryCompareUserData
                      )
#else /* not NDEBUG */
  bool __Dictionary_init(const char                *__fileName__,
                         ulong                     __lineNb__,
                         Dictionary                *dictionary,
                         DictionaryCopyFunction    dictionaryCopyFunction,
                         void                      *dictionaryCopyUserData,
                         DictionaryFreeFunction    dictionaryFreeFunction,
                         void                      *dictionaryFreeUserData,
                         DictionaryCompareFunction dictionaryCompareFunction,
                         void                      *dictionaryCompareUserData
                        )
#endif /* NDEBUG */
{
  assert(dictionary != NULL);

  if (!Semaphore_init(&dictionary->lock,SEMAPHORE_TYPE_BINARY))
  {
    return FALSE;
  }

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

  dictionary->dictionaryCopyFunction    = dictionaryCopyFunction;
  dictionary->dictionaryCopyUserData    = dictionaryCopyUserData;
  dictionary->dictionaryFreeFunction    = dictionaryFreeFunction;
  dictionary->dictionaryFreeUserData    = dictionaryFreeUserData;
  dictionary->dictionaryCompareFunction = dictionaryCompareFunction;
  dictionary->dictionaryCompareUserData = dictionaryCompareUserData;

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(dictionary,Dictionary);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,dictionary,Dictionary);
  #endif /* NDEBUG */

  return TRUE;
}

#ifdef NDEBUG
  void Dictionary_done(Dictionary *dictionary)
#else /* not NDEBUG */
  void __Dictionary_done(const char *__fileName__,
                         ulong      __lineNb__,
                         Dictionary *dictionary
                        )
#endif /* NDEBUG */
{
  uint i;
  uint index;

  assert(dictionary != NULL);
  assert(dictionary->entryTables != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(dictionary,Dictionary);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,dictionary,Dictionary);
  #endif /* NDEBUG */

  // free resources
  for (i = 0; i < dictionary->entryTableCount; i++)
  {
    assert(dictionary->entryTables[i].entries != NULL);

    for (index = 0; index < TABLE_SIZES[dictionary->entryTables[i].sizeIndex]; index++)
    {
      if (dictionary->entryTables[i].entries[index].keyData != NULL)
      {
        if (dictionary->dictionaryFreeFunction != NULL)
        {
          dictionary->dictionaryFreeFunction(dictionary->entryTables[i].entries[index].data,
                                             dictionary->entryTables[i].entries[index].length,
                                             dictionary->dictionaryFreeUserData
                                            );
        }
        if (dictionary->entryTables[i].entries[index].allocatedFlag)
        {
          assert(dictionary->entryTables[i].entries[index].data != NULL);
          free(dictionary->entryTables[i].entries[index].data);
        }

        free(dictionary->entryTables[i].entries[index].keyData);
      }
    }
    free(dictionary->entryTables[i].entries);
  }
  free(dictionary->entryTables);
  Semaphore_done(&dictionary->lock);
}

void Dictionary_clear(Dictionary *dictionary)
{
  uint i;
  uint index;

  assert(dictionary != NULL);
  assert(dictionary->entryTables != NULL);

  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    for (i = 0; i < dictionary->entryTableCount; i++)
    {
      assert(dictionary->entryTables[i].entries != NULL);

      for (index = 0; index < TABLE_SIZES[dictionary->entryTables[i].sizeIndex]; index++)
      {
        if (dictionary->entryTables[i].entries[index].keyData != NULL)
        {
          if (dictionary->dictionaryFreeFunction != NULL)
          {
            dictionary->dictionaryFreeFunction(dictionary->entryTables[i].entries[index].data,
                                               dictionary->entryTables[i].entries[index].length,
                                               dictionary->dictionaryFreeUserData
                                              );
          }
          if (dictionary->entryTables[i].entries[index].allocatedFlag)
          {
            assert(dictionary->entryTables[i].entries[index].data != NULL);
            free(dictionary->entryTables[i].entries[index].data);
            dictionary->entryTables[i].entries[index].allocatedFlag = FALSE;
          }

          free(dictionary->entryTables[i].entries[index].keyData);

          dictionary->entryTables[i].entries[index].hash    = 0;
          dictionary->entryTables[i].entries[index].keyData = NULL;
        }
      }
    }
  }
}

ulong Dictionary_count(Dictionary *dictionary)
{
  ulong count;
  uint  i;

  assert(dictionary != NULL);
  assert(dictionary->entryTables != NULL);

  count = 0;
  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    for (i = 0; i < dictionary->entryTableCount; i++)
    {
      count += dictionary->entryTables[i].entryCount;
    }
  }

  return count;
}

bool Dictionary_byteCopy(const void *fromData, void *toData, ulong length, void *userData)
{
  assert(toData != NULL);

  UNUSED_VARIABLE(userData);

  if (fromData != NULL)
  {
    memcpy(toData,fromData,length);
  }

  return TRUE;
}

void Dictionary_byteFree(void *data, ulong length, void *userData)
{
  assert(data != NULL);

  UNUSED_VARIABLE(length);
  UNUSED_VARIABLE(userData);

  free(data);
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
  uint                 i,j;
  DictionaryEntry      *newEntries;
  DictionaryEntryTable *entryTables;

  assert(dictionary != NULL);

  hash = calculateHash(keyData,keyLength);

  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // update entry
    if (findEntry(dictionary,hash,keyData,keyLength,&dictionaryEntryTable,&entryIndex))
    {
      assert(dictionaryEntryTable->entries != NULL);

      if (dictionary->dictionaryCopyFunction != NULL)
      {
        // allocate/resize data memory
        if (dictionaryEntryTable->entries[entryIndex].length != length)
        {
          // re-allocate data memory
          newData = realloc(dictionaryEntryTable->entries[entryIndex].data,length);
          if (newData == NULL)
          {
            Semaphore_unlock(&dictionary->lock);
            return FALSE;
          }

          dictionaryEntryTable->entries[entryIndex].data          = newData;
          dictionaryEntryTable->entries[entryIndex].length        = length;
          dictionaryEntryTable->entries[entryIndex].allocatedFlag = TRUE;
        }

        // copy data
        if (!dictionary->dictionaryCopyFunction(data,
                                                dictionaryEntryTable->entries[entryIndex].data,
                                                length,
                                                dictionary->dictionaryCopyUserData
                                               )
           )
        {
          Semaphore_unlock(&dictionary->lock);
          return FALSE;
        }
      }
      else
      {
        // free old entry
        if (dictionary->dictionaryFreeFunction != NULL)
        {
          dictionary->dictionaryFreeFunction(dictionaryEntryTable->entries[entryIndex].data,
                                             dictionaryEntryTable->entries[entryIndex].length,
                                             dictionary->dictionaryFreeUserData
                                            );
        }
        if (dictionaryEntryTable->entries[entryIndex].allocatedFlag)
        {
          free(dictionaryEntryTable->entries[entryIndex].data);
        }

        // use orginal data
        dictionaryEntryTable->entries[entryIndex].data          = (void*)data;
        dictionaryEntryTable->entries[entryIndex].allocatedFlag = FALSE;
      }

      Semaphore_unlock(&dictionary->lock);
      return TRUE;
    }

    // add entry in existing table
    if (findFreeEntry(dictionary,hash,&dictionaryEntryTable,&entryIndex))
    {
      assert(dictionaryEntryTable->entries != NULL);

      // allocate key memory
      dictionaryEntryTable->entries[entryIndex].keyData = malloc(keyLength);
      if (dictionaryEntryTable->entries[entryIndex].keyData == NULL)
      {
        Semaphore_unlock(&dictionary->lock);
        return FALSE;
      }

      // copy key data
      dictionaryEntryTable->entries[entryIndex].hash = hash;
      memcpy(dictionaryEntryTable->entries[entryIndex].keyData,keyData,keyLength);
      dictionaryEntryTable->entries[entryIndex].keyLength = keyLength;

      if (dictionary->dictionaryCopyFunction != NULL)
      {
        // allocate data memory
        newData = malloc(length);
        if (newData == NULL)
        {
          free(dictionaryEntryTable->entries[entryIndex].keyData);
          Semaphore_unlock(&dictionary->lock);
          return FALSE;
        }

        // copy data
        if (!dictionary->dictionaryCopyFunction(data,
                                                newData,
                                                length,
                                                dictionary->dictionaryCopyUserData
                                               )
           )
        {
          free(newData);
          free(dictionaryEntryTable->entries[entryIndex].keyData);
          Semaphore_unlock(&dictionary->lock);
          return FALSE;
        }
        dictionaryEntryTable->entries[entryIndex].data          = newData;
        dictionaryEntryTable->entries[entryIndex].allocatedFlag = TRUE;
      }
      else
      {
        // use orginal data
        dictionaryEntryTable->entries[entryIndex].data          = (void*)data;
        dictionaryEntryTable->entries[entryIndex].allocatedFlag = FALSE;
      }
      dictionaryEntryTable->entries[entryIndex].length = length;

      dictionaryEntryTable->entryCount++;

      Semaphore_unlock(&dictionary->lock);
      return TRUE;
    }

    /* find a table which can be resized and where new entry can be
       stored in extended table, store entry in extended table
    */
    dictionaryEntryTable = NULL;
    i = 0;
    while ((i < dictionary->entryTableCount) && (dictionaryEntryTable == NULL))
    {
      assert(dictionary->entryTables != NULL);

      tableIndex = (hash+i)%dictionary->entryTableCount;
      newSizeIndex = dictionary->entryTables[tableIndex].sizeIndex+1;
      while ((newSizeIndex < SIZE_OF_ARRAY(TABLE_SIZES)) && (dictionaryEntryTable == NULL))
      {
        #if   COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING
          entryIndex = 0;
          j = 0;
          while ((j < LINEAR_PROBING_COUNT) && (entryIndex < TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex]))
          {
            entryIndex = addModulo(hash,j,TABLE_SIZES[newSizeIndex]);
            j++;
          }
        #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
          entryIndex = modulo(hash,TABLE_SIZES[newSizeIndex]);
          if (entryIndex < TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex])
          {
            j = 0;
            while (j < QUADRATIC_PROBING_COUNT)
            {
              entryIndex = addModulo(hash,j*j,TABLE_SIZES[newSizeIndex]);
              if (entryIndex >= TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex]) break;
              entryIndex = subModulo(hash,j*j,TABLE_SIZES[newSizeIndex]);
              if (entryIndex >= TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex]) break;
              j++;
            }
          }
        #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH
          entryIndex = 0;
          j = 0;
          while ((j < REHASHING_COUNT) && (entryIndex < TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex]))
          {
            entryIndex = rotHash(hash,j)%TABLE_SIZES[newSizeIndex];
            j++;
          }
        #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH */
        if (entryIndex >= TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex])
        {
//fprintf(stderr,"%s,%d: before grow %p\n",__FILE__,__LINE__,dictionary->entryTables[tableIndex].entries);
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
//fprintf(stderr,"%s,%d: after grow %p\n",__FILE__,__LINE__,dictionary->entryTables[tableIndex].entries);
        }
        newSizeIndex++;
      }
      i++;
    }
    if (dictionaryEntryTable != NULL)
    {
      assert(dictionaryEntryTable->entries != NULL);

      // allocate key memory
      dictionaryEntryTable->entries[entryIndex].keyData = malloc(keyLength);
      if (dictionaryEntryTable->entries[entryIndex].keyData == NULL)
      {
        Semaphore_unlock(&dictionary->lock);
        return FALSE;
      }

      // copy key data
      dictionaryEntryTable->entries[entryIndex].hash = hash;
      memcpy(dictionaryEntryTable->entries[entryIndex].keyData,keyData,keyLength);
      dictionaryEntryTable->entries[entryIndex].keyLength = keyLength;

      if (dictionary->dictionaryCopyFunction != NULL)
      {
        // allocate data memory
        newData = malloc(length);
        if (newData == NULL)
        {
          free(dictionaryEntryTable->entries[entryIndex].keyData);
          Semaphore_unlock(&dictionary->lock);
          return FALSE;
        }

        // copy data
        if (!dictionary->dictionaryCopyFunction(data,
                                                newData,
                                                length,
                                                dictionary->dictionaryCopyUserData
                                               )
           )
        {
          free(newData);
          free(dictionaryEntryTable->entries[entryIndex].keyData);
          Semaphore_unlock(&dictionary->lock);
          return FALSE;
        }
        dictionaryEntryTable->entries[entryIndex].data          = newData;
        dictionaryEntryTable->entries[entryIndex].allocatedFlag = TRUE;
      }
      else
      {
        // use orginal data
        dictionaryEntryTable->entries[entryIndex].data          = (void*)data;
        dictionaryEntryTable->entries[entryIndex].allocatedFlag = FALSE;
      }
      dictionaryEntryTable->entries[entryIndex].length = length;

      dictionaryEntryTable->entryCount++;

      Semaphore_unlock(&dictionary->lock);
      return TRUE;
    }

    // add new table and store entry in new table
    newEntries = (DictionaryEntry*)calloc(TABLE_SIZES[0],sizeof(DictionaryEntry));
    if (newEntries == NULL)
    {
      Semaphore_unlock(&dictionary->lock);
      return FALSE;
    }
    entryTables = (DictionaryEntryTable*)realloc(dictionary->entryTables,(dictionary->entryTableCount+1)*sizeof(DictionaryEntryTable));
    if (entryTables == NULL)
    {
      free(newEntries);
      Semaphore_unlock(&dictionary->lock);
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
      Semaphore_unlock(&dictionary->lock);
      return FALSE;
    }

    dictionaryEntryTable->entries[entryIndex].hash = hash;

    memcpy(dictionaryEntryTable->entries[entryIndex].keyData,keyData,keyLength);
    dictionaryEntryTable->entries[entryIndex].keyLength = keyLength;

    if (dictionary->dictionaryCopyFunction != NULL)
    {
      // allocate data memory
      newData = malloc(length);
      if (newData == NULL)
      {
        free(dictionaryEntryTable->entries[entryIndex].keyData);
        Semaphore_unlock(&dictionary->lock);
        return FALSE;
      }

      // copy data
      if (!dictionary->dictionaryCopyFunction(data,
                                              newData,
                                              length,
                                              dictionary->dictionaryCopyUserData
                                             )
         )
      {
        free(newData);
        free(dictionaryEntryTable->entries[entryIndex].keyData);
        Semaphore_unlock(&dictionary->lock);
        return FALSE;
      }
      dictionaryEntryTable->entries[entryIndex].data          = newData;
      dictionaryEntryTable->entries[entryIndex].allocatedFlag = TRUE;
    }
    else
    {
      // use orginal data
      dictionaryEntryTable->entries[entryIndex].data          = (void*)data;
      dictionaryEntryTable->entries[entryIndex].allocatedFlag = FALSE;
    }
    dictionaryEntryTable->entries[entryIndex].length = length;

    dictionaryEntryTable->entryCount++;
  }

  return TRUE;
}

void Dictionary_remove(Dictionary *dictionary,
                       const void *keyData,
                       ulong      keyLength
                      )
{
  ulong                hash;
  DictionaryEntryTable *dictionaryEntryTable;
  uint                 entryIndex;

  assert(dictionary != NULL);
  assert(keyData != NULL);

  hash = calculateHash(keyData,keyLength);

  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (findEntry(dictionary,hash,keyData,keyLength,&dictionaryEntryTable,&entryIndex))
    {
      assert(dictionaryEntryTable->entries != NULL);
      assert(dictionaryEntryTable->entryCount > 0);

      if (dictionary->dictionaryFreeFunction != NULL)
      {
        dictionary->dictionaryFreeFunction(dictionaryEntryTable->entries[entryIndex].data,
                                           dictionaryEntryTable->entries[entryIndex].length,
                                           dictionary->dictionaryFreeUserData
                                          );
      }
      if (dictionaryEntryTable->entries[entryIndex].allocatedFlag)
      {
        assert(dictionaryEntryTable->entries[entryIndex].data != NULL);
        free(dictionaryEntryTable->entries[entryIndex].data);
        dictionaryEntryTable->entries[entryIndex].allocatedFlag = FALSE;
      }

      free(dictionaryEntryTable->entries[entryIndex].keyData);

      dictionaryEntryTable->entries[entryIndex].hash    = 0;
      dictionaryEntryTable->entries[entryIndex].keyData = NULL;
      dictionaryEntryTable->entryCount--;
    }
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
  bool                 foundFlag;
  DictionaryEntryTable *dictionaryEntryTable;
  uint                 index;

  assert(dictionary != NULL);
  assert(keyData != NULL);

  hash = calculateHash(keyData,keyLength);

  foundFlag = FALSE;
  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    if (findEntry(dictionary,hash,keyData,keyLength,&dictionaryEntryTable,&index))
    {
      assert(dictionaryEntryTable->entries != NULL);

      if (data   != NULL) (*data)   = (void*)dictionaryEntryTable->entries[index].data;
      if (length != NULL) (*length) = dictionaryEntryTable->entries[index].length;
      foundFlag = TRUE;
    }
  }

  return foundFlag;
}

void Dictionary_initIterator(DictionaryIterator *dictionaryIterator,
                             Dictionary         *dictionary
                            )
{
  assert(dictionaryIterator != NULL);
  assert(dictionary != NULL);

  dictionaryIterator->dictionary = dictionary;
  dictionaryIterator->i          = 0;
  dictionaryIterator->j          = 0;

  DEBUG_ADD_RESOURCE_TRACE(dictionaryIterator,DictionaryIterator);

  Semaphore_forceLock(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE);
}

void Dictionary_doneIterator(DictionaryIterator *dictionaryIterator)
{
  assert(dictionaryIterator != NULL);
  assert(dictionaryIterator->dictionary != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(dictionaryIterator,DictionaryIterator);

  Semaphore_unlock(&dictionaryIterator->dictionary->lock);
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
      // get entry
      assert(dictionaryIterator->dictionary->entryTables[dictionaryIterator->i].entries != NULL);
      dictionaryEntry = &dictionaryIterator->dictionary->entryTables[dictionaryIterator->i].entries[dictionaryIterator->j];

      // check if used/empty
      if (dictionaryEntry->keyData != NULL)
      {
        if (keyData   != NULL) (*keyData)   = dictionaryEntry->keyData;
        if (keyLength != NULL) (*keyLength) = dictionaryEntry->keyLength;
        if (data      != NULL) (*data)      = (void*)dictionaryEntry->data;
        if (length    != NULL) (*length)    = dictionaryEntry->length;
        foundFlag = TRUE;
      }

      // next entry
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

void Dictionary_debugDump(Dictionary *dictionary)
{
  assert(dictionary != NULL);

  Dictionary_iterate(dictionary,
                     CALLBACK_INLINE(bool,(const void *keyData,
                                           ulong      keyLength,
                                           void       *data,
                                           ulong      length,
                                           void       *userData
                                          ),
                     {
                       UNUSED_VARIABLE(keyLength);
                       UNUSED_VARIABLE(length);
                       UNUSED_VARIABLE(userData);

                       fwrite(keyData,1,keyLength,stdout); printf(": %p %lu\n",(const char*)data,length);

                       return TRUE;
                     },
                     NULL)
                    );
}

void Dictionary_printStatistic(Dictionary *dictionary)
{
  ulong totalEntryCount,totalIndexCount;
  uint  i;

  assert(dictionary != NULL);

  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    fprintf(stderr,"Dictionary statistics:\n");
    fprintf(stderr,"  tables : %d\n",dictionary->entryTableCount);

    totalEntryCount = 0;
    totalIndexCount = 0;
    for (i = 0; i < dictionary->entryTableCount; i++)
    {
      fprintf(stderr,"    table #%02d: %u entries/%u size\n",i,dictionary->entryTables[i].entryCount,TABLE_SIZES[dictionary->entryTables[i].sizeIndex]);
      totalEntryCount += dictionary->entryTables[i].entryCount;
      totalIndexCount += TABLE_SIZES[dictionary->entryTables[i].sizeIndex];
    }

    fprintf(stderr,"  total entries: %lu\n",totalEntryCount);
    fprintf(stderr,"  total size:    %lu\n",totalIndexCount);
  }
}

#endif /* NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
