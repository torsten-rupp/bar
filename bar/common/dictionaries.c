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
  return n % m;
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
  return (n + d) % m;
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
  return (n + m - d) % m;
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
* Input  : key       - key data
*          keyLength - length of key data
* Output : -
* Return : hash value
* Notes  : -
\***********************************************************************/

LOCAL ulong calculateHash(const void *key, ulong keyLength)
{
  if (keyLength > 0)
  {
    assert(key != NULL);

    const byte *p = (const byte*)key;

    ulong hashBytes[4];
    hashBytes[0] = (keyLength > 0)?(*p):0; p++;
    hashBytes[1] = (keyLength > 1)?(*p):0; p++;
    hashBytes[2] = (keyLength > 2)?(*p):0; p++;
    hashBytes[3] = (keyLength > 3)?(*p):0; p++;
    for (size_t i = 4; i < keyLength; i++)
    {
      hashBytes[i % 4] ^= (*p); p++;
    }

    return (ulong)(hashBytes[3] << 24) |
           (ulong)(hashBytes[2] << 16) |
           (ulong)(hashBytes[1] <<  8) |
           (ulong)(hashBytes[0] <<  0);
  }
  else
  {
    return (intptr_t)key;
  }
}

/***********************************************************************\
* Name   : equalsEntry
* Purpose: check if entry is equal to data
* Input  : entry                          - entry
*          hash                           - data hash value
*          key                            - key data
*          keyLength                      - length of key data
*          dictionaryCompareEntryFunction - compare function or NULL for
*                                           default compare
*          dictionaryCompareEntryUserData - compare function user data
* Output : -
* Return : TRUE if entry is equal, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL_INLINE bool equalsEntry(const DictionaryEntry          *entry,
                              ulong                          hash,
                              const void                     *key,
                              ulong                          keyLength,
                              DictionaryCompareEntryFunction dictionaryCompareEntryFunction,
                              void                           *dictionaryCompareEntryUserData
                             )
{
  assert(entry != NULL);

  if (   (hash == entry->hash)
      && (entry->keyLength == keyLength))
  {
    if      (dictionaryCompareEntryFunction != NULL)
    {
      if (dictionaryCompareEntryFunction(entry->key,key,keyLength,dictionaryCompareEntryUserData))
      {
        return TRUE;
      }
    }
    else if (keyLength > 0)
    {
      if (memcmp(entry->key,key,keyLength) == 0)
      {
        return TRUE;
      }
    }
    else
    {
      return entry->key == key;
    }
  }

  return FALSE;
}

/***********************************************************************\
* Name   : findEntryIndex
* Purpose: find entry index of entry in table
* Input  : entryTable                     - entry table
*          hash                           - data hash value
*          key                            - key data
*          keyLength                      - length of key data
*          dictionaryCompareEntryFunction - compare function or NULL for
*                                           default compare
*          dictionaryCompareEntryUserData - compare function user data
* Output : -
* Return : index of -1 if entry not found
* Notes  : -
\***********************************************************************/

LOCAL ssize_t findEntryIndex(DictionaryEntryTable           *entryTable,
                             ulong                          hash,
                             const void                     *key,
                             ulong                          keyLength,
                             DictionaryCompareEntryFunction dictionaryCompareEntryFunction,
                             void                           *dictionaryCompareEntryUserData
                            )
{
  size_t entryIndex;

  assert(entryTable != NULL);

  for (size_t z = 0; z <= entryTable->sizeIndex; z++)
  {
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING
      for (size_t i = 0; i < LINEAR_PROBING_COUNT; i++)
      {
        entryIndex = addModulo(hash,i,TABLE_SIZES[z]);
        if (   entryTable->entries[entryIndex].isUsed
            && equalsEntry(&entryTable->entries[entryIndex],
                           hash,
                           key,
                           keyLength,
                           dictionaryCompareEntryFunction,
                           dictionaryCompareEntryUserData
                          )
           )
        {
          return (ssize_t)entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING */
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
      entryIndex = modulo(hash,TABLE_SIZES[z]);
      if (   entryTable->entries[entryIndex].isUsed
          && equalsEntry(&entryTable->entries[entryIndex],
                         hash,
                         key,
                         keyLength,
                         dictionaryCompareEntryFunction,
                         dictionaryCompareEntryUserData
                        )
         )
      {
        return (ssize_t)entryIndex;
      }
      for (size_t i = 1; i < QUADRATIC_PROBING_COUNT; i++)
      {
        entryIndex = addModulo(hash,i*i,TABLE_SIZES[z]);
        if (   entryTable->entries[entryIndex].isUsed
            && equalsEntry(&entryTable->entries[entryIndex],
                           hash,
                           key,
                           keyLength,
                           dictionaryCompareEntryFunction,
                           dictionaryCompareEntryUserData
                          )
           )
        {
          return (ssize_t)entryIndex;
        }
        entryIndex = subModulo(hash,i*i,TABLE_SIZES[z]);
        if (   entryTable->entries[entryIndex].isUsed
            && equalsEntry(&entryTable->entries[entryIndex],
                           hash,
                           key,
                           keyLength,
                           dictionaryCompareEntryFunction,
                           dictionaryCompareEntryUserData
                          )
           )
        {
          return (ssize_t)entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING */
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH
      for (size_t i = 0; i < REHASHING_COUNT; i++)
      {
        entryIndex = rotHash(hash,i)%TABLE_SIZES[z];
        if (   entryTable->entries[entryIndex].isUsed
            && equalsEntry(&entryTable->entries[entryIndex],
                           hash,
                           key,
                           keyLength,
                           dictionaryCompareEntryFunction,
                           dictionaryCompareEntryUserData
                          )
           )
        {
          return (ssize_t)entryIndex;
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

LOCAL ssize_t findFreeEntryIndex(DictionaryEntryTable *entryTable,
                                 ulong                hash
                                )
{
  assert(entryTable != NULL);

  for (size_t i = 0; i <= entryTable->sizeIndex; i++)
  {
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING
      for (size_t j = 0; j < LINEAR_PROBING_COUNT; j++)
      {
        ssize_t entryIndex = addModulo(hash,j,TABLE_SIZES[i]);
        if (!entryTable->entries[entryIndex].isUsed)
        {
          return entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING */
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
      ssize_t entryIndex = modulo(hash,TABLE_SIZES[i]);
      if (!entryTable->entries[entryIndex].isUsed)
      {
        return entryIndex;
      }
      for (size_t j = 0; j < QUADRATIC_PROBING_COUNT; j++)
      {
        ssize_t entryIndex = addModulo(hash,j*j,TABLE_SIZES[i]);
        if (!entryTable->entries[entryIndex].isUsed)
        {
          return entryIndex;
        }
        entryIndex = subModulo(hash,j*j,TABLE_SIZES[i]);
        if (!entryTable->entries[entryIndex].isUsed)
        {
          return entryIndex;
        }
      }
    #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING */
    #if COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH
      for (size_t j = 0; j < REHASHING_COUNT; j++)
      {
        ssize_t entryIndex = rotHash(hash,j)%TABLE_SIZES[i];
        if (!entryTable->entries[entryIndex].isUsed)
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
* Input  : dictionary - dictionary
*          hash       - data hash value
*          key        - key data
*          keyLength  - length of key data
* Output : dictionaryEntryTable - dictionary entry table
*          index                - index in table
* Return : TRUE if entry found, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool findEntry(Dictionary           *dictionary,
                     ulong                hash,
                     const void           *key,
                     ulong                keyLength,
                     DictionaryEntryTable **dictionaryEntryTable,
                     size_t               *index
                    )
{
  assert(dictionary != NULL);
  assert(dictionaryEntryTable != NULL);
  assert(index != NULL);

  (*dictionaryEntryTable) = NULL;
  (*index)                = 0;
  size_t i = 0;
  while ((i < dictionary->entryTableCount) && ((*dictionaryEntryTable) == NULL))
  {
    uint tableIndex = (hash + i) % dictionary->entryTableCount;
    ssize_t entryIndex = findEntryIndex(&dictionary->entryTables[tableIndex],
                                        hash,
                                        key,
                                        keyLength,
                                        dictionary->dictionaryCompareEntryFunction,
                                        dictionary->dictionaryCompareEntryUserData
                                       );
    if (entryIndex >= 0)
    {
      (*dictionaryEntryTable) = &dictionary->entryTables[tableIndex];
      (*index)                = (size_t)entryIndex;
    }
    i++;
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
                         size_t               *index
                        )
{
  assert(dictionary != NULL);
  assert(dictionaryEntryTable != NULL);
  assert(index != NULL);

  (*dictionaryEntryTable) = NULL;
  (*index)                = 0;
  size_t i = 0;
  while ((i < dictionary->entryTableCount) && ((*dictionaryEntryTable) == NULL))
  {
    uint tableIndex = (hash + i) % dictionary->entryTableCount;
    ssize_t freeIndex = findFreeEntryIndex(&dictionary->entryTables[tableIndex],
                                           hash
                                          );
    if (freeIndex >= 0)
    {
      (*dictionaryEntryTable) = &dictionary->entryTables[tableIndex];
      (*index)                = (size_t)freeIndex;
    }
    i++;
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

bool Dictionary_byteInitEntry(const void *fromValue, void *toValue, ulong length, void *userData)
{
  assert(toValue != NULL);

  UNUSED_VARIABLE(userData);

  if (fromValue != NULL)
  {
    memcpy(toValue,fromValue,length);
  }

  return TRUE;
}

bool Dictionary_valueCompareEntry(const void *value0, const void *value1, ulong valueLength, void *userData)
{
  UNUSED_VARIABLE(valueLength);
  UNUSED_VARIABLE(userData);

  return value0 == value1;
}

#ifdef NDEBUG
  bool Dictionary_init(Dictionary                     *dictionary,
                       DictionaryInitEntryFunction    dictionaryInitEntryFunction,
                       void                           *dictionaryInitEntryUserData,
                       DictionaryDoneEntryFunction    dictionaryDoneEntryFunction,
                       void                           *dictionaryDoneEntryUserData,
                       DictionaryCompareEntryFunction dictionaryCompareEntryFunction,
                       void                           *dictionaryCompareEntryUserData
                      )
#else /* not NDEBUG */
  bool __Dictionary_init(const char                    *__fileName__,
                         size_t                        __lineNb__,
                         Dictionary                    *dictionary,
                         DictionaryInitEntryFunction    dictionaryInitEntryFunction,
                         void                           *dictionaryInitEntryUserData,
                         DictionaryDoneEntryFunction    dictionaryDoneEntryFunction,
                         void                           *dictionaryDoneEntryUserData,
                         DictionaryCompareEntryFunction dictionaryCompareEntryFunction,
                         void                           *dictionaryCompareEntryUserData
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
    Semaphore_done(&dictionary->lock);
    return FALSE;
  }
  dictionary->entryTableCount = 1;

  dictionary->entryTables[0].entries = (DictionaryEntry*)calloc(TABLE_SIZES[0],sizeof(DictionaryEntry));
  if (dictionary->entryTables[0].entries == NULL)
  {
    free(dictionary->entryTables);
    Semaphore_done(&dictionary->lock);
    return FALSE;
  }
  dictionary->entryTables[0].sizeIndex  = 0;
  dictionary->entryTables[0].entryCount = 0;

  dictionary->dictionaryInitEntryFunction    = dictionaryInitEntryFunction;
  dictionary->dictionaryInitEntryUserData    = dictionaryInitEntryUserData;
  dictionary->dictionaryDoneEntryFunction    = dictionaryDoneEntryFunction;
  dictionary->dictionaryDoneEntryUserData    = dictionaryDoneEntryUserData;
  dictionary->dictionaryCompareEntryFunction = dictionaryCompareEntryFunction;
  dictionary->dictionaryCompareEntryUserData = dictionaryCompareEntryUserData;

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
                         size_t     __lineNb__,
                         Dictionary *dictionary
                        )
#endif /* NDEBUG */
{
  assert(dictionary != NULL);
  assert(dictionary->entryTables != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(dictionary,Dictionary);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,dictionary,Dictionary);
  #endif /* NDEBUG */

  // free resources
  for (size_t i = 0; i < dictionary->entryTableCount; i++)
  {
    assert(dictionary->entryTables[i].entries != NULL);

    for (size_t index = 0; index < TABLE_SIZES[dictionary->entryTables[i].sizeIndex]; index++)
    {
      if (dictionary->entryTables[i].entries[index].isUsed)
      {
        assert(dictionary->entryTables[i].entryCount > 0);

        if (dictionary->dictionaryDoneEntryFunction != NULL)
        {
          dictionary->dictionaryDoneEntryFunction(dictionary->entryTables[i].entries[index].value,
                                                  dictionary->entryTables[i].entries[index].valueLength,
                                                  dictionary->dictionaryDoneEntryUserData
                                                 );
        }
        if (dictionary->entryTables[i].entries[index].isAllocated)
        {
          assert(dictionary->entryTables[i].entries[index].value != NULL);
          free(dictionary->entryTables[i].entries[index].value);
        }

        if (dictionary->entryTables[i].entries[index].keyLength > 0)
        {
          free(dictionary->entryTables[i].entries[index].key);
        }
      }
    }
    free(dictionary->entryTables[i].entries);
  }
  free(dictionary->entryTables);
  Semaphore_done(&dictionary->lock);
}

void Dictionary_clear(Dictionary *dictionary)
{
  assert(dictionary != NULL);
  assert(dictionary->entryTables != NULL);

  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    for (size_t i = 0; i < dictionary->entryTableCount; i++)
    {
      assert(dictionary->entryTables[i].entries != NULL);

      for (size_t index = 0; index < TABLE_SIZES[dictionary->entryTables[i].sizeIndex]; index++)
      {
        if (dictionary->entryTables[i].entries[index].isUsed)
        {
          assert(dictionary->entryTables[i].entryCount > 0);

          if (dictionary->dictionaryDoneEntryFunction != NULL)
          {
            dictionary->dictionaryDoneEntryFunction(dictionary->entryTables[i].entries[index].value,
                                                    dictionary->entryTables[i].entries[index].valueLength,
                                                    dictionary->dictionaryDoneEntryUserData
                                                   );
          }
          if (dictionary->entryTables[i].entries[index].isAllocated)
          {
            assert(dictionary->entryTables[i].entries[index].value != NULL);
            free(dictionary->entryTables[i].entries[index].value);
            dictionary->entryTables[i].entries[index].isAllocated = FALSE;
          }

          if (dictionary->entryTables[i].entries[index].keyLength > 0)
          {
            free(dictionary->entryTables[i].entries[index].key);
          }

          dictionary->entryTables[i].entries[index].isUsed = FALSE;

          dictionary->entryTables[i].entryCount--;
        }
      }
    }
  }
}

ulong Dictionary_count(Dictionary *dictionary)
{
  assert(dictionary != NULL);
  assert(dictionary->entryTables != NULL);

 ulong count = 0;
  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    for (size_t i = 0; i < dictionary->entryTableCount; i++)
    {
      count += dictionary->entryTables[i].entryCount;
    }
  }

  return count;
}

bool Dictionary_add(Dictionary *dictionary,
                    const void *key,
                    ulong      keyLength,
                    const void *value,
                    ulong      valueLength
                   )
{
  assert(dictionary != NULL);

  ulong hash = calculateHash(key,keyLength);

  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    // update entry
    DictionaryEntryTable *dictionaryEntryTable;
    size_t               entryIndex;
    if (findEntry(dictionary,hash,key,keyLength,&dictionaryEntryTable,&entryIndex))
    {
      assert(dictionaryEntryTable->entries != NULL);

      if (dictionary->dictionaryInitEntryFunction != NULL)
      {
        // allocate/resize data memory
        if (dictionaryEntryTable->entries[entryIndex].valueLength != valueLength)
        {
          // re-allocate data memory
          void *newData = realloc(dictionaryEntryTable->entries[entryIndex].value,valueLength);
          if (newData == NULL)
          {
            Semaphore_unlock(&dictionary->lock);
            return FALSE;
          }

          dictionaryEntryTable->entries[entryIndex].value       = newData;
          dictionaryEntryTable->entries[entryIndex].valueLength = valueLength;
          dictionaryEntryTable->entries[entryIndex].isAllocated = TRUE;
        }

        // init data
        if (!dictionary->dictionaryInitEntryFunction(value,
                                                     dictionaryEntryTable->entries[entryIndex].value,
                                                     valueLength,
                                                     dictionary->dictionaryInitEntryUserData
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
        if (dictionary->dictionaryDoneEntryFunction != NULL)
        {
          dictionary->dictionaryDoneEntryFunction(dictionaryEntryTable->entries[entryIndex].value,
                                                  dictionaryEntryTable->entries[entryIndex].valueLength,
                                                  dictionary->dictionaryDoneEntryUserData
                                                 );
        }
        if (dictionaryEntryTable->entries[entryIndex].isAllocated)
        {
          free(dictionaryEntryTable->entries[entryIndex].value);
        }

        // use orginal data
        dictionaryEntryTable->entries[entryIndex].value       = (void*)value;
        dictionaryEntryTable->entries[entryIndex].isAllocated = FALSE;
      }

      Semaphore_unlock(&dictionary->lock);

      return TRUE;
    }

    // add entry in existing table
    if (findFreeEntry(dictionary,hash,&dictionaryEntryTable,&entryIndex))
    {
      assert(dictionaryEntryTable->entries != NULL);

      // init key
      dictionaryEntryTable->entries[entryIndex].hash = hash;
      if (keyLength > 0)
      {
        // allocate key memory
        dictionaryEntryTable->entries[entryIndex].key = malloc(keyLength);
        if (dictionaryEntryTable->entries[entryIndex].key == NULL)
        {
          Semaphore_unlock(&dictionary->lock);
          return FALSE;
        }

        // copy key data
        memcpy(dictionaryEntryTable->entries[entryIndex].key,key,keyLength);
      }
      else
      {
        // use key data directly
        dictionaryEntryTable->entries[entryIndex].key = (void*)key;
      }
      dictionaryEntryTable->entries[entryIndex].keyLength = keyLength;

      if (dictionary->dictionaryInitEntryFunction != NULL)
      {
        // allocate data memory
        void *newData = malloc(valueLength);
        if (newData == NULL)
        {
          free(dictionaryEntryTable->entries[entryIndex].key);
          Semaphore_unlock(&dictionary->lock);
          return FALSE;
        }

        // init data
        if (!dictionary->dictionaryInitEntryFunction(value,
                                                     newData,
                                                     valueLength,
                                                     dictionary->dictionaryInitEntryUserData
                                                    )
           )
        {
          free(newData);
          free(dictionaryEntryTable->entries[entryIndex].key);
          Semaphore_unlock(&dictionary->lock);
          return FALSE;
        }
        dictionaryEntryTable->entries[entryIndex].value       = newData;
        dictionaryEntryTable->entries[entryIndex].isAllocated = TRUE;
      }
      else
      {
        // use orginal data
        dictionaryEntryTable->entries[entryIndex].value       = (void*)value;
        dictionaryEntryTable->entries[entryIndex].isAllocated = FALSE;
      }

      dictionaryEntryTable->entries[entryIndex].isUsed      = TRUE;
      dictionaryEntryTable->entries[entryIndex].valueLength = valueLength;

      dictionaryEntryTable->entryCount++;

      Semaphore_unlock(&dictionary->lock);
      return TRUE;
    }

    /* find a table which can be resized and where new entry can be
       stored in extended table, store entry in extended table
    */
    dictionaryEntryTable = NULL;
    uint i = 0;
    while ((i < dictionary->entryTableCount) && (dictionaryEntryTable == NULL))
    {
      assert(dictionary->entryTables != NULL);

      uint tableIndex   = (hash + i) % dictionary->entryTableCount;
      uint newSizeIndex = dictionary->entryTables[tableIndex].sizeIndex+1;
      while ((newSizeIndex < SIZE_OF_ARRAY(TABLE_SIZES)) && (dictionaryEntryTable == NULL))
      {
        #if   COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING
          entryIndex = 0;
          uint j = 0;
          while ((j < LINEAR_PROBING_COUNT) && (entryIndex < TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex]))
          {
            entryIndex = addModulo(hash,j,TABLE_SIZES[newSizeIndex]);
            j++;
          }
        #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
          entryIndex = modulo(hash,TABLE_SIZES[newSizeIndex]);
          if (entryIndex < TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex])
          {
            uint j = 0;
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
          uint j = 0;
          while ((j < REHASHING_COUNT) && (entryIndex < TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex]))
          {
            entryIndex = rotHash(hash,j)%TABLE_SIZES[newSizeIndex];
            j++;
          }
        #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH */
        if (entryIndex >= TABLE_SIZES[dictionary->entryTables[tableIndex].sizeIndex])
        {
//fprintf(stderr,"%s,%d: before grow %p\n",__FILE__,__LINE__,dictionary->entryTables[tableIndex].entries);
          DictionaryEntry *newEntries = growTable(dictionary->entryTables[tableIndex].entries,
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

      // init key
      dictionaryEntryTable->entries[entryIndex].hash = hash;
      if (keyLength > 0)
      {
        // allocate key memory
        dictionaryEntryTable->entries[entryIndex].key = malloc(keyLength);
        if (dictionaryEntryTable->entries[entryIndex].key == NULL)
        {
          Semaphore_unlock(&dictionary->lock);
          return FALSE;
        }

        // copy key data
        memcpy(dictionaryEntryTable->entries[entryIndex].key,key,keyLength);
      }
      else
      {
        dictionaryEntryTable->entries[entryIndex].key = (void*)key;
      }
      dictionaryEntryTable->entries[entryIndex].keyLength = keyLength;

      if (dictionary->dictionaryInitEntryFunction != NULL)
      {
        // allocate data memory
        void *newData = malloc(valueLength);
        if (newData == NULL)
        {
          free(dictionaryEntryTable->entries[entryIndex].key);
          Semaphore_unlock(&dictionary->lock);
          return FALSE;
        }

        // init data
        if (!dictionary->dictionaryInitEntryFunction(value,
                                                     newData,
                                                     valueLength,
                                                     dictionary->dictionaryInitEntryUserData
                                                    )
           )
        {
          free(newData);
          free(dictionaryEntryTable->entries[entryIndex].key);
          Semaphore_unlock(&dictionary->lock);
          return FALSE;
        }
        dictionaryEntryTable->entries[entryIndex].value       = newData;
        dictionaryEntryTable->entries[entryIndex].isAllocated = TRUE;
      }
      else
      {
        // use orginal data
        dictionaryEntryTable->entries[entryIndex].value       = (void*)value;
        dictionaryEntryTable->entries[entryIndex].isAllocated = FALSE;
      }

      dictionaryEntryTable->entries[entryIndex].isUsed      = TRUE;
      dictionaryEntryTable->entries[entryIndex].valueLength = valueLength;

      dictionaryEntryTable->entryCount++;

      Semaphore_unlock(&dictionary->lock);
      return TRUE;
    }

    // add new table and store entry in new table
    DictionaryEntry *newEntries = (DictionaryEntry*)calloc(TABLE_SIZES[0],sizeof(DictionaryEntry));
    if (newEntries == NULL)
    {
      Semaphore_unlock(&dictionary->lock);
      return FALSE;
    }
    DictionaryEntryTable *entryTables = (DictionaryEntryTable*)realloc(dictionary->entryTables,(dictionary->entryTableCount+1)*sizeof(DictionaryEntryTable));
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


    // init key
    dictionaryEntryTable->entries[entryIndex].hash = hash;
    if (keyLength > 0)
    {
      dictionaryEntryTable->entries[entryIndex].key = malloc(keyLength);
      if (dictionaryEntryTable->entries[entryIndex].key == NULL)
      {
        Semaphore_unlock(&dictionary->lock);
        return FALSE;
      }

      memcpy(dictionaryEntryTable->entries[entryIndex].key,key,keyLength);
    }
    else
    {
      dictionaryEntryTable->entries[entryIndex].key = (void*)key;
    }
    dictionaryEntryTable->entries[entryIndex].keyLength = keyLength;

    if (dictionary->dictionaryInitEntryFunction != NULL)
    {
      // allocate data memory
      void *newData = malloc(valueLength);
      if (newData == NULL)
      {
        if (keyLength > 0)
        {
          free(dictionaryEntryTable->entries[entryIndex].key);
        }
        Semaphore_unlock(&dictionary->lock);
        return FALSE;
      }

      // init data
      if (!dictionary->dictionaryInitEntryFunction(value,
                                                   newData,
                                                   valueLength,
                                                   dictionary->dictionaryInitEntryUserData
                                                  )
         )
      {
        free(newData);
        free(dictionaryEntryTable->entries[entryIndex].key);
        Semaphore_unlock(&dictionary->lock);
        return FALSE;
      }
      dictionaryEntryTable->entries[entryIndex].value       = newData;
      dictionaryEntryTable->entries[entryIndex].isAllocated = TRUE;
    }
    else
    {
      // use orginal data
      dictionaryEntryTable->entries[entryIndex].value       = (void*)value;
      dictionaryEntryTable->entries[entryIndex].isAllocated = FALSE;
    }

    dictionaryEntryTable->entries[entryIndex].isUsed      = TRUE;
    dictionaryEntryTable->entries[entryIndex].valueLength = valueLength;

    dictionaryEntryTable->entryCount++;
  }

  return TRUE;
}

void Dictionary_remove(Dictionary *dictionary,
                       const void *key,
                       ulong      keyLength
                      )
{

  assert(dictionary != NULL);

  ulong hash = calculateHash(key,keyLength);

  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    DictionaryEntryTable *dictionaryEntryTable;
    size_t               entryIndex;
    if (findEntry(dictionary,hash,key,keyLength,&dictionaryEntryTable,&entryIndex))
    {
      assert(dictionaryEntryTable->entries != NULL);
      assert(dictionaryEntryTable->entryCount > 0);

      if (dictionary->dictionaryDoneEntryFunction != NULL)
      {
        dictionary->dictionaryDoneEntryFunction(dictionaryEntryTable->entries[entryIndex].value,
                                                dictionaryEntryTable->entries[entryIndex].valueLength,
                                                dictionary->dictionaryDoneEntryUserData
                                               );
      }
      if (dictionaryEntryTable->entries[entryIndex].isAllocated)
      {
        assert(dictionaryEntryTable->entries[entryIndex].value != NULL);
        free(dictionaryEntryTable->entries[entryIndex].value);
        dictionaryEntryTable->entries[entryIndex].isAllocated = FALSE;
      }

      if (dictionaryEntryTable->entries[entryIndex].keyLength > 0)
      {
        free(dictionaryEntryTable->entries[entryIndex].key);
      }

      dictionaryEntryTable->entries[entryIndex].isUsed  = FALSE;

      dictionaryEntryTable->entryCount--;
    }
  }
}

bool Dictionary_find(Dictionary *dictionary,
                     const void *key,
                     ulong      keyLength,
                     void       **value,
                     ulong      *valueLength
                    )
{
  assert(dictionary != NULL);

  ulong hash = calculateHash(key,keyLength);

  bool foundFlag = FALSE;
  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    DictionaryEntryTable *dictionaryEntryTable;
    size_t               index;
    if (findEntry(dictionary,hash,key,keyLength,&dictionaryEntryTable,&index))
    {
      assert(dictionaryEntryTable->entries != NULL);

      if (value       != NULL) (*value)       = (void*)dictionaryEntryTable->entries[index].value;
      if (valueLength != NULL) (*valueLength) = dictionaryEntryTable->entries[index].valueLength;
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
                        const void         **key,
                        ulong              *keyLength,
                        void               **value,
                        ulong              *valueLength
                       )
{
  assert(dictionaryIterator != NULL);

  if (key         != NULL) (*key)         = NULL;
  if (keyLength   != NULL) (*keyLength)   = 0;
  if (value       != NULL) (*value)       = NULL;
  if (valueLength != NULL) (*valueLength) = 0;

  bool foundFlag = FALSE;
  if (dictionaryIterator->i < dictionaryIterator->dictionary->entryTableCount)
  {
    assert(dictionaryIterator->dictionary->entryTables != NULL);

    do
    {
      // get entry
      assert(dictionaryIterator->dictionary->entryTables[dictionaryIterator->i].entries != NULL);
      DictionaryEntry *dictionaryEntry = &dictionaryIterator->dictionary->entryTables[dictionaryIterator->i].entries[dictionaryIterator->j];

      // check if used/empty
      if (dictionaryEntry->isUsed)
      {
        if (key         != NULL) (*key)         = dictionaryEntry->key;
        if (keyLength   != NULL) (*keyLength)   = dictionaryEntry->keyLength;
        if (value       != NULL) (*value)       = (void*)dictionaryEntry->value;
        if (valueLength != NULL) (*valueLength) = dictionaryEntry->valueLength;
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

  assert(dictionary != NULL);
  assert(dictionaryIterateFunction != NULL);

  bool okFlag = TRUE;
  DictionaryIterator dictionaryIterator;
  Dictionary_initIterator(&dictionaryIterator,dictionary);
  const void *key;
  ulong      keyLength;
  void       *value;
  ulong      valueLength;
  while (   Dictionary_getNext(&dictionaryIterator,
                               &key,
                               &keyLength,
                               &value,
                               &valueLength
                              )
         && okFlag
        )
  {
    okFlag = dictionaryIterateFunction(key,
                                       keyLength,
                                       value,
                                       valueLength,
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

  ulong n = 0;
  Dictionary_iterate(dictionary,
                     CALLBACK_INLINE(bool,(const void *key,
                                           ulong      keyLength,
                                           void       *value,
                                           ulong      valueLength,
                                           void       *userData
                                          ),
                     {
                       UNUSED_VARIABLE(userData);

                       printf("%4lu %p %lu ",n,key,keyLength);
                       for (size_t i = 0; i < keyLength; i++)
                       {
                         printf("%02x",((const byte*)key)[i]);
                       }
                       printf(": ");
                       printf("%p %lu ",value,valueLength);
                       for (size_t i = 0; i < valueLength; i++)
                       {
                         printf("%02x",((const byte*)value)[i]);
                       }
                       printf("\n");
                       n++;

                       return TRUE;
                     },
                     NULL)
                    );
}

void Dictionary_printStatistic(Dictionary *dictionary)
{
  assert(dictionary != NULL);

  SEMAPHORE_LOCKED_DO(&dictionary->lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
  {
    fprintf(stderr,"Dictionary statistics:\n");
    fprintf(stderr,"  tables : %d\n",dictionary->entryTableCount);

    ulong totalEntryCount = 0;
    ulong totalIndexCount = 0;
    for (size_t i = 0; i < dictionary->entryTableCount; i++)
    {
      fprintf(stderr,"    table #%02zu: %u entries/%u size\n",i,dictionary->entryTables[i].entryCount,TABLE_SIZES[dictionary->entryTables[i].sizeIndex]);
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
