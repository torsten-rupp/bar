/***********************************************************************\
*
* Contents: hash table functions
* Systems: all
*
\***********************************************************************/

#define __HASH_TABLE_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"

#include "hashtables.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
#define DATA_START_SIZE (16*1024)
#define DATA_DELTA_SIZE ( 4*1024)
#define MAX_DATA_SIZE   (2*1024*1024*1024)

// hash table sizes
LOCAL const uint TABLE_SIZES[] =
{
  63,
  127,
  257,
  521,
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

/***********************************************************************\
* Name   : getIndex
* Purpose: get index from hash
* Input  : hash - hash
* Output : -
* Return : index in hash table
* Notes  : -
\***********************************************************************/

LOCAL_INLINE ulong getIndex(const HashTable *hashTable, ulong hash)
{
  assert(hashTable != NULL);

  return hash%hashTable->size;
}

/***********************************************************************\
* Name   : getHashTableSize
* Purpose: get size of hash table
* Input  : minSize - min. size
* Output : -
* Return : size
* Notes  : -
\***********************************************************************/

LOCAL ulong getHashTableSize(ulong minSize)
{
  uint i;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(TABLE_SIZES))
         && (minSize > TABLE_SIZES[i])
        )
  {
    i++;
  }

  return (i < SIZE_OF_ARRAY(TABLE_SIZES)) ? TABLE_SIZES[i] : 0L;
}

/***********************************************************************\
* Name   : defaultHashFunction
* Purpose: default function to calculate hash value
* Input  : keyData   - key data
*          keyLength - length of key data
*          userData  - user data
* Output : -
* Return : hash value
* Notes  : -
\***********************************************************************/

LOCAL ulong defaultHashFunction(const void *keyData, ulong keyLength, void *userData)
{
  byte       hashBytes[4];
  const byte *p;
  uint       z;

  assert(keyData != NULL);
  assert(keyLength > 0);

  UNUSED_VARIABLE(userData);

  p = (const byte*)keyData;

  hashBytes[0] = (keyLength > 0) ? (*p) : 0; p++;
  hashBytes[1] = (keyLength > 1) ? (*p) : 0; p++;
  hashBytes[2] = (keyLength > 2) ? (*p) : 0; p++;
  hashBytes[3] = (keyLength > 3) ? (*p) : 0; p++;
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
* Name   : defaultEqualsFunction
* Purpose: default function to check if key data equals
* Input  : data0,data1 - data entries to compare
*          length      - length of data entries
*          userData    - user data
* Output : -
* Return : TRUE if equal, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool defaultEqualsFunction(const void *data0, const void *data1, ulong length, void *userData)
{
  UNUSED_VARIABLE(userData);

  return memEquals(data0,length,data1,length);
}

// TODO: still not used
#if 0
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
#endif

/***********************************************************************\
* Name   : findEntry
* Purpose: find entry in hash table
* Input  : hashTable          - hash table
*          hash               - data hash value
*          data               - data
*          length             - length of data
*          hashTableEntry     - hash table entry (can be NULL)
*          prevHashTableEntry - orevious hash table entry (can be NULL)
* Output : hashTableEntry     - hash table entry
*          prevHashTableEntry - orevious hash table entry
* Return : TRUE iff entry found
* Notes  : -
\***********************************************************************/

LOCAL bool findEntry(HashTable      *hashTable,
                     ulong          hash,
                     const void     *keyData,
                     ulong          keyLength,
                     HashTableEntry **foundHashTableEntry,
                     HashTableEntry **prevHashTableEntry
                    )
{
  HashTableEntry *hashTableEntry;
  ulong          tableIndex;

  assert(hashTable != NULL);
  assert(keyData != NULL);
  assert(keyLength > 0);

  tableIndex = getIndex(hashTable,hash);

  #if   COLLISION_ALGORITHM==COLLISION_ALGORITHM_NONE
    if (prevHashTableEntry != NULL) (*prevHashTableEntry) = NULL;
    hashTableEntry = hashTable->entries[tableIndex];
    assert((hashTableEntry == NULL) || ((hashTableEntry->keyData != NULL) && (hashTableEntry->keyLength > 0)));
    while (   (hashTableEntry != NULL)
           && (   (keyLength != hashTableEntry->keyLength)
               || !hashTable->equalsFunction(keyData,
                                             hashTableEntry->keyData,
                                             keyLength,
                                             hashTable->equalsUserData
                                            )
              )
          )
    {
      if (prevHashTableEntry != NULL) (*prevHashTableEntry) = hashTableEntry;
      hashTableEntry = hashTableEntry->next;
      assert((hashTableEntry == NULL) || ((hashTableEntry->keyData != NULL) && (hashTableEntry->keyLength > 0)));
    }
    if (hashTableEntry != NULL)
    {
      assert(hashTableEntry->keyData != NULL);
      assert(hashTableEntry->keyLength > 0);
      if (foundHashTableEntry != NULL) (*foundHashTableEntry) = hashTableEntry;
      return TRUE;
    }
    else
    {
      return FALSE;
    }
  #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING
    i = 0L;
    do
    {
      hashTableEntry = &hashTable->entries[(tableIndex+i)%hashTable->size];
      foundFlag = hashTable->equalsFunction(keyData,
                                            hashTableEntry->data,
                                            keyLength,
                                            hashTable->equalsUserData
                                           );
      if (!foundFlag) i++;
    }
    while (   !foundFlag
           && (i < hashTable->size)
          );
    return foundFlag ? hashTableEntry : NULL;
  #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
    #error NYI
  #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH
    #error NYI
  #else
    #error No hash table collision algorithm defined!
  #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_... */
}

/***********************************************************************\
* Name   : allocateEntry
* Purpose: allocate entry in hash table
* Input  : hashTable - hash table
*          hash      - data hash value
* Output : -
* Return : hash table entry
* Notes  : -
\***********************************************************************/

LOCAL HashTableEntry* allocateEntry(HashTable *hashTable,
                                    ulong     hash
                                   )
{
  HashTableEntry *hashTableEntry;
  ulong          tableIndex;

  assert(hashTable != NULL);

  tableIndex = getIndex(hashTable,hash);

  #if   COLLISION_ALGORITHM==COLLISION_ALGORITHM_NONE
    hashTableEntry = (HashTableEntry*)malloc(sizeof(HashTableEntry));
    if (hashTableEntry != NULL)
    {
      hashTableEntry->next = hashTable->entries[tableIndex];
      hashTable->entries[tableIndex] = hashTableEntry;
    }
  #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING
    i = 0L;
    do
    {
      hashTableEntry = &hashTable->entries[(tableIndex+i)%hashTable->size];
      if (hashTableEntry->keyData != NULL)
      {
        i++;
      }
    }
    while (   (hashTableEntry->keyData != NULL)
           && (i < hashTable->size)
          );
    if (i >= hashTable->size) hashTableEntry = NULL;
  #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
    #error NYI
  #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH
    #error NYI
  #else
    #error No hash table collision algorithm defined!
  #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_... */

  return hashTableEntry;
}

/***********************************************************************\
* Name   : freeEntry
* Purpose: free entry in hash table
* Input  : hashTable      - hash table
*          hashTableEntry - hash table entry
*          hash           - data hash value
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeEntry(HashTable      *hashTable,
                     HashTableEntry *hashTableEntry,
                     ulong          hash
                    )
{
  ulong tableIndex;

  assert(hashTable != NULL);

  tableIndex = getIndex(hashTable,hash);

  #if   COLLISION_ALGORITHM==COLLISION_ALGORITHM_NONE
    assert(hashTable->entries[tableIndex] == hashTableEntry);

    hashTable->entries[tableIndex] = hashTableEntry->next;
    free(hashTableEntry);
  #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_LINEAR_PROBING
    // nothing to do
  #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_QUADRATIC_PROBING
    // nothing to do
  #elif COLLISION_ALGORITHM==COLLISION_ALGORITHM_REHASH
    // nothing to do
  #else
    #error No hash table collision algorithm defined!
  #endif /* COLLISION_ALGORITHM==COLLISION_ALGORITHM_... */
}

// TODO: use?
#if 0
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

LOCAL HashTableEntry *growTable(HashTableEntry *entries, uint oldSize, uint newSize)
{
  assert(entries != NULL);
  assert(newSize > oldSize);

  entries = realloc(entries,newSize*sizeof(HashTableEntry));
  if (entries != NULL)
  {
    memset(&entries[oldSize],0,(newSize-oldSize)*sizeof(HashTableEntry));
  }

  return entries;
}
#endif

/***********************************************************************\
* Name   : getNext
* Purpose: get next hash table entry
* Input  : hashTableIterator - hash table iterator
* Output : -
* Return : hash table entry or NULL if no more hash table entries
* Notes  : -
\***********************************************************************/

LOCAL HashTableEntry *getNext(HashTableIterator *hashTableIterator)
{
  HashTableEntry *hashTableEntry;

  assert(hashTableIterator != NULL);
  assert(hashTableIterator->hashTable != NULL);
  assert(hashTableIterator->hashTable->entries != NULL);

  hashTableEntry = NULL;

  #if HASH_TABLE_COLLISION_ALGORITHM == HASH_TABLE_COLLISION_ALGORITHM_NONE
    if (hashTableIterator->nextHashTableEntry != NULL)
    {
      hashTableEntry = hashTableIterator->nextHashTableEntry;
      hashTableIterator->nextHashTableEntry = hashTableIterator->nextHashTableEntry->next;
    }
  #endif

  if (   (hashTableEntry == NULL)
      && (hashTableIterator->i < hashTableIterator->hashTable->size)
     )
  {
    do
    {
      // check if used/empty
      #if HASH_TABLE_COLLISION_ALGORITHM == HASH_TABLE_COLLISION_ALGORITHM_NONE
        if (hashTableIterator->hashTable->entries[hashTableIterator->i] != NULL)
        {
          hashTableEntry = hashTableIterator->hashTable->entries[hashTableIterator->i];
          if (hashTableEntry->next != NULL)
          {
            hashTableIterator->nextHashTableEntry = hashTableEntry->next;
          }
        }
      #else
        if (hashTableIterator->hashTable->entries[hashTableIterator->i].keyData != NULL)
        {
          hashTableEntry = &hashTableIterator->hashTable->entries[hashTableIterator->i];
        }
      #endif

      hashTableIterator->i++;
    }
    while (   (hashTableEntry == NULL)
           && (hashTableIterator->i < hashTableIterator->hashTable->size)
          );
  }

  return hashTableEntry;
}

/*---------------------------------------------------------------------*/

bool HashTable_init(HashTable               *hashTable,
                    ulong                   minSize,
                    HashTableHashFunction   hashFunction,
                    void                    *hashUserData,
                    HashTableEqualsFunction equalsFunction,
                    void                    *equalsUserData,
                    HashTableFreeFunction   freeFunction,
                    void                    *freeUserData
                   )
{
  ulong size;

  assert(hashTable != NULL);

  size = getHashTableSize(minSize);

  #if HASH_TABLE_COLLISION_ALGORITHM == HASH_TABLE_COLLISION_ALGORITHM_NONE
    hashTable->entries = (HashTableEntry**)calloc(size,sizeof(HashTableEntry*));
  #else
    hashTable->entries = (HashTableEntry*)calloc(size,sizeof(HashTableEntry));
  #endif
  if (hashTable->entries == NULL)
  {
    return FALSE;
  }
  hashTable->entryCount     = 0;
  hashTable->size           = size;

  hashTable->hashFunction   = (hashFunction != NULL) ? hashFunction : defaultHashFunction;
  hashTable->hashUserData   = hashUserData;
  hashTable->equalsFunction = (equalsFunction != NULL) ? equalsFunction : defaultEqualsFunction;
  hashTable->equalsUserData = equalsUserData;
  hashTable->freeFunction   = freeFunction;
  hashTable->freeUserData   = freeUserData;

  return TRUE;
}

void HashTable_done(HashTable *hashTable)
{
  uint           i;
  HashTableEntry *hashTableEntry;

  assert(hashTable != NULL);
  assert(hashTable->entries != NULL);

  for (i = 0; i < hashTable->size; i++)
  {
    #if HASH_TABLE_COLLISION_ALGORITHM == HASH_TABLE_COLLISION_ALGORITHM_NONE
      while (hashTable->entries[i] != NULL)
      {
        hashTableEntry = hashTable->entries[i];
        assert(hashTableEntry->keyData != NULL);
        assert(hashTableEntry->keyLength > 0);

        if (hashTable->freeFunction != NULL)
        {
          hashTable->freeFunction(hashTableEntry->data,
                                  hashTableEntry->length,
                                  hashTable->freeUserData
                                 );
        }
        if (hashTableEntry->data != NULL) free(hashTableEntry->data);
        free(hashTableEntry->keyData);

        hashTable->entries[i] = hashTableEntry->next;
        free(hashTableEntry);
      }
    #else
      if (hashTable->entries[i].keyData != NULL)
      {
        if (hashTable->freeFunction != NULL)
        {
          hashTable->freeFunction(hashTable->entries[i].data,
                                  hashTable->entries[i].length,
                                  hashTable->freeUserData
                                 );
        }
        if (hashTable->entries[i].data != NULL) free(hashTable->entries[i].data);
        free(hashTable->entries[i].keyData);
      }
    #endif
  }
  free(hashTable->entries);
}

void HashTable_clear(HashTable *hashTable)
{
  uint           i;
  HashTableEntry *hashTableEntry;

  assert(hashTable != NULL);
  assert(hashTable->entries != NULL);

  for (i = 0; i < hashTable->size; i++)
  {
    #if HASH_TABLE_COLLISION_ALGORITHM == HASH_TABLE_COLLISION_ALGORITHM_NONE
      while (hashTable->entries[i] != NULL)
      {
        hashTableEntry = hashTable->entries[i];
        assert(hashTableEntry->keyData != NULL);
        assert(hashTableEntry->keyLength > 0);

        if (hashTable->freeFunction != NULL)
        {
          hashTable->freeFunction(hashTableEntry->data,
                                  hashTableEntry->length,
                                  hashTable->freeUserData
                                 );
        }
        if (hashTableEntry->data != NULL) free(hashTableEntry->data);;
        free(hashTableEntry->keyData);

        hashTable->entries[i] = hashTableEntry->next;
        free(hashTableEntry);
      }
    #else
      if (hashTableEntry->keyData != NULL)
      {
        if (hashTable->freeFunction != NULL)
        {
          hashTable->freeFunction(hashTable->entries[i].data,
                                  hashTable->entries[i].length,
                                  hashTable->freeUserData
                                 );
        }
        free(hashTable->entries[i].data);
        free(hashTable->entries[i].keyData);

        hashTableEntry->keyData = NULL;
      }
    #endif
  }
  hashTable->entryCount = 0L;
}

HashTableEntry *HashTable_put(HashTable *hashTable,
                              const void *keyData,
                              ulong      keyLength,
                              const void *data,
                              ulong      length
                             )
{
  ulong          hash;
  HashTableEntry *hashTableEntry;
  void           *newData;

  assert(hashTable != NULL);
  assert(hashTable->hashFunction != NULL);
  assert(hashTable->entries != NULL);
  assert(keyData != NULL);
  assert(keyLength > 0);

  hash = hashTable->hashFunction(keyData,keyLength,hashTable->hashUserData);

  if (findEntry(hashTable,hash,keyData,keyLength,&hashTableEntry,NULL))
  {
    // update entry

    // allocate/resize data memory
    if (hashTableEntry->length != length)
    {
      newData = realloc(hashTableEntry->data,length);
      if (newData == NULL)
      {
        return NULL;
      }
      hashTableEntry->data   = newData;
      hashTableEntry->length = length;
    }

    // copy data
    memCopyFast(hashTableEntry->data,hashTableEntry->length,data,length);
  }
  else
  {
    // add entry

    // debug: check for duplicate
    #ifndef NDEBUG
    {
      HashTableIterator    hashTableIterator;
      const HashTableEntry *hashTableEntry;

      HashTable_initIterator(&hashTableIterator,hashTable);
      hashTableEntry = getNext(&hashTableIterator);
      while (hashTableEntry != NULL)
      {
        assert(!memEquals(keyData,keyLength,hashTableEntry->keyData,hashTableEntry->keyLength));
        hashTableEntry = getNext(&hashTableIterator);
      }
      HashTable_doneIterator(&hashTableIterator);
    }
    #endif

    hashTableEntry = allocateEntry(hashTable,hash);
    if (hashTableEntry != NULL)
    {
      // copy key data
      hashTableEntry->keyData = malloc(keyLength);
      if (hashTableEntry->keyData == NULL)
      {
        freeEntry(hashTable,hashTableEntry,hash);
        return NULL;
      }
      memCopyFast(hashTableEntry->keyData,keyLength,keyData,keyLength);
      hashTableEntry->keyLength = keyLength;

      // copy data
      hashTableEntry->data = malloc(length);
      if (hashTableEntry->data == NULL)
      {
        free(hashTableEntry->keyData);
        freeEntry(hashTable,hashTableEntry,hash);
        return NULL;
      }
      memCopyFast(hashTableEntry->data,length,data,length);
      hashTableEntry->length = length;

      hashTable->entryCount++;
    }
  }

  return hashTableEntry;
}

void HashTable_remove(HashTable  *hashTable,
                      const void *keyData,
                      ulong      keyLength
                     )
{
  ulong          hash;
  HashTableEntry *hashTableEntry,*prevHashTableEntry;

  assert(hashTable != NULL);
  assert(hashTable->entries != NULL);
  assert(keyData != NULL);
  assert(keyLength > 0);

  hash = hashTable->hashFunction(keyData,keyLength,hashTable->hashUserData);

  // remove entry
  if (findEntry(hashTable,hash,keyData,keyLength,&hashTableEntry,&prevHashTableEntry))
  {
    assert(hashTable->entryCount > 0);

    if (hashTable->freeFunction != NULL)
    {
      hashTable->freeFunction(hashTableEntry->data,
                              hashTableEntry->length,
                              hashTable->freeUserData
                             );
    }
    if (hashTableEntry->data != NULL) free(hashTableEntry->data);;
    free(hashTableEntry->keyData);

    #if HASH_TABLE_COLLISION_ALGORITHM == HASH_TABLE_COLLISION_ALGORITHM_NONE
      if      (prevHashTableEntry != NULL)
      {
        prevHashTableEntry->next = hashTableEntry->next;
      }
      else
      {
        hashTable->entries[getIndex(hashTable,hash)] = hashTableEntry->next;
      }

      free(hashTableEntry);
    #else
      hashTableEntry->keyData = NULL;
    #endif

    hashTable->entryCount--;
  }
}

HashTableEntry *HashTable_find(HashTable  *hashTable,
                               const void *keyData,
                               ulong      keyLength
                              )
{
  ulong          hash;
  HashTableEntry *hashTableEntry;

  assert(hashTable != NULL);
  assert(hashTable->entries != NULL);
  assert(keyData != NULL);
  assert(keyLength > 0);

  hash = hashTable->hashFunction(keyData,keyLength,hashTable->hashUserData);

  if (findEntry(hashTable,hash,keyData,keyLength,&hashTableEntry,NULL))
  {
    return hashTableEntry;
  }
  else
  {
    return NULL;
  }
}

void HashTable_initIterator(HashTableIterator *hashTableIterator,
                            const HashTable   *hashTable
                           )
{
  assert(hashTableIterator != NULL);
  assert(hashTable != NULL);
  assert(hashTable->entries != NULL);

  hashTableIterator->hashTable          = hashTable;
  hashTableIterator->i                  = 0;
  hashTableIterator->nextHashTableEntry = NULL;
}

void HashTable_doneIterator(HashTableIterator *hashTableIterator)
{
  assert(hashTableIterator != NULL);

  UNUSED_VARIABLE(hashTableIterator);
}

HashTableEntry *HashTable_getNext(HashTableIterator *hashTableIterator,
                                  const void        **keyData,
                                  ulong             *keyLength,
                                  const void        **data,
                                  ulong             *length
                                 )
{
  HashTableEntry *hashTableEntry;

  assert(hashTableIterator != NULL);
  assert(hashTableIterator->hashTable != NULL);
  assert(hashTableIterator->hashTable->entries != NULL);

  hashTableEntry = getNext(hashTableIterator);
  if (hashTableEntry != NULL)
  {
    assert(hashTableEntry->keyData != NULL);
    assert(hashTableEntry->keyLength > 0);

    if (keyData   != NULL) (*keyData)   = hashTableEntry->keyData;
    if (keyLength != NULL) (*keyLength) = hashTableEntry->keyLength;
    if (data      != NULL) (*data)      = hashTableEntry->data;
    if (length    != NULL) (*length)    = hashTableEntry->length;
  }

  return hashTableEntry;
}

bool HashTable_iterate(HashTable                *hashTable,
                       HashTableIterateFunction iterateFunction,
                       void                     *iterateUserData
                      )
{
  HashTableIterator    hashTableIterator;
  bool                 okFlag;
  const HashTableEntry *hashTableEntry;

  assert(hashTable != NULL);
  assert(iterateFunction != NULL);

  okFlag = TRUE;
  HashTable_initIterator(&hashTableIterator,hashTable);
  hashTableEntry = getNext(&hashTableIterator);
  while (   (hashTableEntry != NULL)
         && okFlag
        )
  {
    assert(hashTableEntry->keyData != NULL);
    assert(hashTableEntry->keyLength > 0);

    okFlag = iterateFunction(hashTableEntry,iterateUserData);
    hashTableEntry = getNext(&hashTableIterator);
  }
  HashTable_doneIterator(&hashTableIterator);

  return okFlag;
}

#ifndef NDEBUG

void HashTable_dump(FILE *handle, const HashTable *hashTable)
{
  HashTableIterator    hashTableIterator;
  const HashTableEntry *hashTableEntry;

  assert(hashTable != NULL);

  fprintf(handle,"Hash table %p:\n",hashTable);
  HashTable_initIterator(&hashTableIterator,hashTable);
  hashTableEntry = getNext(&hashTableIterator);
  while (hashTableEntry != NULL)
  {
    assert(hashTableEntry->keyData != NULL);
    assert(hashTableEntry->keyLength > 0);

    fprintf(handle,
            "  %p %lu %p %lu\n",
            hashTableEntry->keyData,
            hashTableEntry->keyLength,
            hashTableEntry->data,
            hashTableEntry->length
           );

    hashTableEntry = getNext(&hashTableIterator);
  }
  HashTable_doneIterator(&hashTableIterator);
}

void HashTable_print(const HashTable *hashTable)
{
  assert(hashTable != NULL);

  HashTable_dump(stdout,hashTable);
}

void HashTable_printStatistic(const HashTable *hashTable)
{
  assert(hashTable != NULL);

  fprintf(stderr,"Hash table statistics:\n");
  fprintf(stderr,"    %lu entries\n",hashTable->entryCount);
  fprintf(stderr,"    %lu size\n",hashTable->size);
}

#endif /* NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
