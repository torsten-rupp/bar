/***********************************************************************\
*
* Contents: hash table functions
* Systems: all
*
\***********************************************************************/

#ifndef __HASH_TABLES__
#define __HASH_TABLES__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"

/****************** Conditional compilation switches *******************/
#define HASH_TABLE_COLLISION_ALGORITHM_NONE              0
#define HASH_TABLE_COLLISION_ALGORITHM_LINEAR_PROBING    1
#define HASH_TABLE_COLLISION_ALGORITHM_QUADRATIC_PROBING 2
#define HASH_TABLE_COLLISION_ALGORITHM_REHASH            3

#define HASH_TABLE_COLLISION_ALGORITHM HASH_TABLE_COLLISION_ALGORITHM_NONE

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : HashTableHashFunction
* Purpose: compute hash value
* Input  : keyData   - key data
*          keyLength - length of key data
*          userData  - user data
* Output : -
* Return : hash value
* Notes  : -
\***********************************************************************/

typedef ulong(*HashTableHashFunction)(const void *keyData, ulong keyLength, void *userData);

/***********************************************************************\
* Name   : HashTableEqualsFunction
* Purpose: compare hash table entries
* Input  : data0,data1 - data to compare
*          length      - length of data
*          userData    - user data
* Output : -
* Return : TRUE iff equal
* Notes  : -
\***********************************************************************/

typedef bool(*HashTableEqualsFunction)(const void *data0, const void *data1, ulong length, void *userData);

/***********************************************************************\
* Name   : HashTableFreeFunction
* Purpose: delete hash table entry
* Input  : data     - data entry
*          length   - length of data entries
*          userData - user data
* Output : -
* Return : -
* Notes  : data is freed by the hash table!
\***********************************************************************/

typedef void(*HashTableFreeFunction)(const void *data, ulong length, void *userData);

// hash entry
typedef struct HashTablEntry
{
  #if HASH_TABLE_COLLISION_ALGORITHM == HASH_TABLE_COLLISION_ALGORITHM_NONE
    struct HashTablEntry *next;
  #endif

  void  *keyData;                            // key data of entry
  ulong keyLength;                           // length of key data in entry

  void  *data;                               // data of entry
  ulong length;                              // length of data in entry
} HashTableEntry;

// hash table
typedef struct
{
  #if HASH_TABLE_COLLISION_ALGORITHM == HASH_TABLE_COLLISION_ALGORITHM_NONE
    HashTableEntry        **entries;         // entries list array
  #else
    HashTableEntry        *entries;          // entries array
  #endif
  ulong                   entryCount;        // number of entries in array
  ulong                   size;              // size (see TABLE_SIZES)

  HashTableHashFunction   hashFunction;
  void                    *hashUserData;
  HashTableEqualsFunction equalsFunction;
  void                    *equalsUserData;
  HashTableFreeFunction   freeFunction;
  void                    *freeUserData;
} HashTable;

typedef struct
{
  const HashTable *hashTable;
  ulong           i;
  #if HASH_TABLE_COLLISION_ALGORITHM == HASH_TABLE_COLLISION_ALGORITHM_NONE
    HashTableEntry *nextHashTableEntry;
  #endif
} HashTableIterator;

/***********************************************************************\
* Name   : HashTableIterateFunction
* Purpose: hash table iterator function
* Input  : keyData   - key data
*          keyLength - length of key data
*          data      - data entry
*          length    - length of data entries
*          userData  - user data
* Output : -
* Return : TRUE to continue, FALSE otherwise
* Notes  : -
\***********************************************************************/

typedef bool(*HashTableIterateFunction)(const HashTableEntry *hashTableEntry, void *userData);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : HashTable_init
* Purpose: initialize hash table
* Input  : hashTable      - hash table variable
*          minSize        - min. size (will be rounded to next ceiling
*                           prime number)
*          hashFunction   - hash function or NULL
*          hashUserData   - hash function user data
*          equalsFunction - hash table entry free function or NULL
*          equalsUserData - hash table entry free function user data
*          freeFunction   - hash table entry free function or NULL
*          freeUserData   - hash table entry free function user data
* Output : hashTable - hash table
* Return : TRUE iff hash table initialized
* Notes  : -
\***********************************************************************/

bool HashTable_init(HashTable               *hashTable,
                    ulong                   minSize,
                    HashTableHashFunction   hashFunction,
                    void                    *hashUserData,
                    HashTableEqualsFunction equalsFunction,
                    void                    *equalsUserData,
                    HashTableFreeFunction   freeFunction,
                    void                    *freeUserData
                   );

/***********************************************************************\
* Name   : HashTable_done
* Purpose: deinitialize hash table
* Input  : hashTable             - hash table
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void HashTable_done(HashTable *hashTable);

/***********************************************************************\
* Name   : HashTable_new
* Purpose: allocate hash table
* Input  : hashFunction   - hash function or NULL
*          hashUserData   - hash function user data
*          equalsFunction - hash table entry free function or NULL
*          equalsUserData - hash table entry free function user data
*          freeFunction   - hash table entry free function or NULL
*          freeUserData   - hash table entry free function user data
* Output : hashTable - hash table
* Return : hash table or NULL
* Notes  : -
\***********************************************************************/

INLINE HashTable *HashTable_new(ulong                   minSize,
                                HashTableHashFunction   hashFunction,
                                void                    *hashUserData,
                                HashTableEqualsFunction equalsFunction,
                                void                    *equalsUserData,
                                HashTableFreeFunction   freeFunction,
                                void                    *freeUserData
                               );
#if defined(NDEBUG) || defined(__HASH_TABLE_IMPLEMENTATION__)
INLINE HashTable *HashTable_new(ulong                   minSize,
                                HashTableHashFunction   hashFunction,
                                void                    *hashUserData,
                                HashTableEqualsFunction equalsFunction,
                                void                    *equalsUserData,
                                HashTableFreeFunction   freeFunction,
                                void                    *freeUserData
                               )

{
  HashTable *hashTable;

  hashTable = (HashTable*)malloc(sizeof(HashTable));
  if (hashTable != NULL)
  {
    HashTable_init(hashTable,
                   minSize,
                   hashFunction,
                   hashUserData,
                   equalsFunction,
                   equalsUserData,
                   freeFunction,
                   freeUserData
                  );
  }

  return hashTable;
}
#endif /* NDEBUG || __HASH_TABLE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : HashTable_delete
* Purpose: delete hash table
* Input  : hashTable - hash table
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void HashTable_delete(HashTable *hashTable);
#if defined(NDEBUG) || defined(__HASH_TABLE_IMPLEMENTATION__)
INLINE void HashTable_delete(HashTable *hashTable)
{
  assert(hashTable != NULL);

  HashTable_done(hashTable);
  free(hashTable);
}
#endif /* NDEBUG || __HASH_TABLE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : HashTable_clear
* Purpose: clear hash table
* Input  : hashTable - hash table
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void HashTable_clear(HashTable *hashTable);

/***********************************************************************\
* Name   : HashTable_isEmpty
* Purpose: check if hash table is empty
* Input  : hashTable - hash table
* Output : -
* Return : TRUE iff empty
* Notes  : -
\***********************************************************************/

INLINE bool HashTable_isEmpty(const HashTable *hashTable);
#if defined(NDEBUG) || defined(__HASH_TABLE_IMPLEMENTATION__)
INLINE bool HashTable_isEmpty(const HashTable *hashTable)
{
  assert(hashTable != NULL);

  return hashTable->entryCount == 0;
}
#endif /* NDEBUG || __HASH_TABLE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : HashTable_count
* Purpose: get number of entries in hash table
* Input  : hashTable - hash table
* Output : -
* Return : number of entries in hash table
* Notes  : -
\***********************************************************************/

INLINE ulong HashTable_count(const HashTable *hashTable);
#if defined(NDEBUG) || defined(__HASH_TABLE_IMPLEMENTATION__)
INLINE ulong HashTable_count(const HashTable *hashTable)
{
  assert(hashTable != NULL);

  return hashTable->entryCount;
}
#endif /* NDEBUG || __HASH_TABLE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : HashTable_put
* Purpose: put entry to hash table
* Input  : hashTable - hash table
*          keyData   - key data
*          keyLength - length of key data
*          data      - entry data (can be NULL)
*          length    - length of entry data (can be 0)
* Output : -
* Return : hash table entry or NULL
* Notes  : -
\***********************************************************************/

HashTableEntry *HashTable_put(HashTable  *hashTable,
                              const void *keyData,
                              ulong      keyLength,
                              const void *data,
                              ulong      length
                             );

/***********************************************************************\
* Name   : HashTable_remove
* Purpose: remove entry from hash table
* Input  : hashTable - hash table
*          keyData   - key data
*          keyLength - length of key data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void HashTable_remove(HashTable  *hashTable,
                      const void *keyData,
                      ulong      keyLength
                     );

/***********************************************************************\
* Name   : HashTable_find
* Purpose: find entry in hash table
* Input  : hashTable - hash table
*          keyData   - key data
*          keyLength - length of key data
* Return : found entry or NULL
* Notes  : -
\***********************************************************************/

HashTableEntry *HashTable_find(HashTable  *hashTable,
                               const void *keyData,
                               ulong      keyLength
                              );

/***********************************************************************\
* Name   : HashTable_containss
* Purpose: check if entry is in hash table
* Input  : hashTable - hash table
*          keyData   - key data
*          keyLength - length of key data
* Output : -
* Return : TRUE iff entry is in hash table
* Notes  : -
\***********************************************************************/

INLINE bool HashTable_contains(HashTable *hashTable,
                               const void *keyData,
                               ulong      keyLength
                              );
#if defined(NDEBUG) || defined(__HASH_TABLE_IMPLEMENTATION__)
INLINE bool HashTable_contains(HashTable *hashTable,
                               const void *keyData,
                               ulong      keyLength
                              )
{
  assert(hashTable != NULL);

  return HashTable_find(hashTable,keyData,keyLength) != NULL;
}
#endif /* NDEBUG || __HASH_TABLE_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : HashTable_initIterator
* Purpose: init hash table iterator
* Input  : hashTableIterator - iterator variable
*          hashTable         - hash table
* Output : hashTableIterator - initialized iterator variable
* Return : -
* Notes  : -
\***********************************************************************/

void HashTable_initIterator(HashTableIterator *hashTableIterator,
                            const HashTable   *hashTable
                           );

/***********************************************************************\
* Name   : HashTable_doneIterator
* Purpose: deinit hash table iterator
* Input  : hashTableIterator - hash table iterator
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void HashTable_doneIterator(HashTableIterator *hashTableIterator);

/***********************************************************************\
* Name   : HashTable_getNext
* Purpose: get next entry from hash table
* Input  : hashTable     - hash table
* Output : keyData       - key data
*          keyDataLength - length of key data (can be NULL)
*          data          - data (can be NULL)
*          length        - length of data (can be NULL)
* Return : hash table entry or NULL if no more entries
* Notes  : -
\***********************************************************************/

HashTableEntry *HashTable_getNext(HashTableIterator *hashTableIterator,
                                  const void        **keyData,
                                  ulong             *keyLength,
                                  const void        **data,
                                  ulong             *length
                                 );

/***********************************************************************\
* Name   : HashTable_iterate
* Purpose: iterate over hash table
* Input  : hashTable                - hash table
*          hashTableIterateFunction - iterator function
*          hashTableIterateUserData - iterator function user data
* Output : -
* Return : TRUE iff all hash table entries iterated
* Notes  : -
\***********************************************************************/

bool HashTable_iterate(HashTable                *hashTable,
                       HashTableIterateFunction iterateFunction,
                       void                     *iterateUserData
                      );

#ifndef NDEBUG

/***********************************************************************\
* Name   : HashTable_dump
* Purpose: dump hashs table
* Input  : handle    - file handle
*          hashTable - hash table
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void HashTable_dump(FILE *handle, const HashTable *hashTable);

/***********************************************************************\
* Name   : HashTable_print
* Purpose: print hash table
* Input  : hashTable - hash table
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void HashTable_print(const HashTable *hashTable);

/***********************************************************************\
* Name   : HashTable_printStatistic
* Purpose: print hash table statistics
* Input  : hashTable - hash table
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void HashTable_printStatistic(const HashTable *hashTable);

#endif /* NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __HASH_TABLES__ */

/* end of file */
