/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: dictionary functions
* Systems: all
*
\***********************************************************************/

#ifndef __DICTIONARIES__
#define __DICTIONARIES__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "semaphores.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : DictionaryCopyFunction
* Purpose: copy dictionary entry
* Input  : fromData - source data entry
*          toData   - memory for destination data entry
*          length   - length of data entries
*          userData - user data or NULL
* Output : -
* Return : TRUE if copied, FALSE otherwise
* Notes  : -
\***********************************************************************/

typedef bool(*DictionaryCopyFunction)(const void *fromData, void *toData, ulong length, void *userData);

/***********************************************************************\
* Name   : DictionaryFreeFunction
* Purpose: delete dictionary entry
* Input  : data     - data entry
*          length   - length of data entries
*          userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*DictionaryFreeFunction)(const void *data, ulong length, void *userData);

/***********************************************************************\
* Name   : DictionaryCompareFunction
* Purpose: compare dictionary entries
* Input  : userData    - user data
*          data0,data1 - data entries to compare
*          length      - length of data entries
* Output : -
* Return : TRUE if equal, FALSE otherwise
* Notes  : -
\***********************************************************************/

typedef bool(*DictionaryCompareFunction)(void *userData, const void *data0, const void *data1, ulong length);

// dictionary entry
typedef struct
{
  ulong hash;                                            // hash code
  void  *keyData;                                        // key data
  ulong keyLength;                                       // length of key data
  void  *data;                                           // data of entry
  ulong length;                                          // length of data in entry
  bool  allocatedFlag;                                   // TRUE iff data was allocated
  bool  removeFlag;                                      // TRUE iff entry should be removed
} DictionaryEntry;

// table with dictionary entries
typedef struct
{
  DictionaryEntry *entries;                              // entries array
  uint            sizeIndex;                             // array size index (see TABLE_SIZES)
  uint            entryCount;                            // number of entries in array
} DictionaryEntryTable;

// dictionary
typedef struct
{
  Semaphore                 lock;
  uint                      lockCount;
  DictionaryEntryTable      *entryTables;                // tables array
  uint                      entryTableCount;             // number of tables
  DictionaryCopyFunction    dictionaryCopyFunction;      // copy entry function call back or NULL
  void                      *dictionaryCopyUserData;
  DictionaryFreeFunction    dictionaryFreeFunction;      // free entry function call back or NULL
  void                      *dictionaryFreeUserData;
  DictionaryCompareFunction dictionaryCompareFunction;   // compare key data function call back or NULL
  void                      *dictionaryCompareUserData;
} Dictionary;

typedef struct
{
  Dictionary *dictionary;
  uint       i,j;
} DictionaryIterator;

/***********************************************************************\
* Name   : DictionaryIterateFunction
* Purpose: iterate over dictionary entires
* Input  : keyData   - key data
*          keyLength - length of key data
*          data      - entry data
*          length    - length of entry data
*          userData  - user data
* Output : -
* Return : TRUE to continue, FALSE abort
* Notes  : -
\***********************************************************************/

typedef bool(*DictionaryIterateFunction)(const void *keyData,
                                         ulong      keyLength,
                                         void       *data,
                                         ulong      length,
                                         void       *userData
                                        );

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#define DICTIONARY_BYTE_COPY CALLBACK_((DictionaryCopyFunction)Dictionary_byteCopy,NULL)
#define DICTIONARY_BYTE_FREE CALLBACK_((DictionaryFreeFunction)Dictionary_byteFree,NULL)

/***********************************************************************\
* Name   : DICTIONARY_ITERATE
* Purpose: iterated over dictionary and execute block
* Input  : dictionary - dictionary
*          variable   - iteration variable
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            LIST_ITERATE(list,variable)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define DICTIONARY_ITERATE(dictionary,keyData,keyLength,data,length) \
  for (DictionaryIterator dictionaryIterator, Dictionary_initIterator(&dictionaryIterator,dictionary); \
       Dictionary_getNext(&dictionaryIterator,keyData,keyLength,data,length); \
      )

/***********************************************************************\
* Name   : DICTIONARY_ITERATEX
* Purpose: iterated over dictionary and execute block
* Input  : dictionary - dictionary
*          variable   - iteration variable
*          condition  - additional condition
* Output : -
* Return : -
* Notes  : variable will contain all entries in list
*          usage:
*            LIST_ITERATEX(list,variable,TRUE)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define DICTIONARY_ITERATEX(dictionary,keyData,keyLength,data,length,condition) \
  for (DictionaryIterator dictionaryIterator, Dictionary_initIterator(&dictionaryIterator,dictionary); \
       Dictionary_getNext(&dictionaryIterator,keyData,keyLength,data,length) && (condition); \
      )

#ifndef NDEBUG
  #define Dictionary_init(...) __Dictionary_init(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Dictionary_done(...) __Dictionary_done(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Dictionary_init
* Purpose: initialize dictionary
* Input  : dictionary                - dictionary variable
*          dictionaryCopyFunction    - copy function call back or NULL
*          dictionaryCopyUserData    - copy function call back user data
*          dictionaryFreeFunction    - free function call back or NULL
*          dictionaryFreeUserData    - free function call back user data
*          dictionaryCompareFunction - compare function call back or NULL
*          dictionaryCompareUserData - compare function call back user data
* Output : -
* Return : TRUE if dictionary initialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  bool Dictionary_init(Dictionary                *dictionary,
                       DictionaryCopyFunction    dictionaryCopyFunction,
                       void                      *dictionaryCopyUserData,
                       DictionaryFreeFunction    dictionaryFreeFunction,
                       void                      *dictionaryFreeUserData,
                       DictionaryCompareFunction dictionaryCompareFunction,
                       void                      *dictionaryCompareUserData
                      );
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
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Dictionary_done
* Purpose: deinitialize dictionary
* Input  : dictionary - dictionary
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Dictionary_done(Dictionary *dictionary);
#else /* not NDEBUG */
  void __Dictionary_done(const char *__fileName__,
                         ulong      __lineNb__,
                         Dictionary *dictionary
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Dictionary_clear
* Purpose: clear dictionary
* Input  : dictionary - dictionary
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_clear(Dictionary *dictionary);

/***********************************************************************\
* Name   : Dictionary_count
* Purpose: get number of entries in dictionary
* Input  : dictionary - dictionary
* Output : -
* Return : number of entries in dictionary
* Notes  : -
\***********************************************************************/

ulong Dictionary_count(Dictionary *dictionary);

/***********************************************************************\
* Name   : Dictionary_byteCopy
* Purpose: byte copy data
* Input  : fromData - source data entry
*          toData   - memory for destination data entry
*          length   - length of data entries
*          userData - user data or NULL
* Output : -
* Return : TRUE
* Return : -
* Notes  : -
\***********************************************************************/

bool Dictionary_byteCopy(const void *fromData, void *toData, ulong length, void *userData);

/***********************************************************************\
* Name   : Dictionary_byteFree
* Purpose: free bytes
* Input  : data     - data entry
*          length   - length of data entries
*          userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_byteFree(void *data, ulong length, void *userData);

/***********************************************************************\
* Name   : Dictionary_add
* Purpose: add entry to dictionary
* Input  : dictionary - dictionary
*          keyData    - key data
*          keyLength  - length of key data
*          data       - entry data
*          length     - length of entry data
* Output : -
* Return : TRUE if entry added, FALSE otherwise
* Notes  : key data and data will be copied!
\***********************************************************************/

bool Dictionary_add(Dictionary *dictionary,
                    const void *keyData,
                    ulong      keyLength,
                    const void *data,
                    ulong      length
                   );

/***********************************************************************\
* Name   : Dictionary_remove
* Purpose: remove entry from dictionary
* Input  : dictionary - dictionary
*          keyData    - key data
*          keyLength  - length of key data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_remove(Dictionary *dictionary,
                       const void *keyData,
                       ulong      keyLength
                      );

/***********************************************************************\
* Name   : Dictionary_find
* Purpose: find entry in dictionary
* Input  : dictionary - dictionary
*          keyData    - key data
*          keyLength  - length of key data
* Output : data   - entry data iff entry found (can be NULL)
*          length - length of data (can be NULL)
* Return : TRUE if entry found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Dictionary_find(Dictionary *dictionary,
                     const void *keyData,
                     ulong      keyLength,
                     void       **data,
                     ulong      *length
                    );

/***********************************************************************\
* Name   : Dictionary_contains
* Purpose: check if entry is contained in dictionary
* Input  : dictionary - dictionary
*          keyData    - key data
*          keyLength  - length of key data
* Output : -
* Return : TRUE if entry is in dictionary, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Dictionary_contains(Dictionary *dictionary,
                                const void *keyData,
                                ulong      keyLength
                               );
#if defined(NDEBUG) || defined(__DICTIONARY_IMPLEMENTATION__)
INLINE bool Dictionary_contains(Dictionary *dictionary,
                                const void *keyData,
                                ulong      keyLength
                               )
{
  assert(dictionary != NULL);

  return Dictionary_find(dictionary,keyData,keyLength,NULL,NULL);
}
#endif /* defined(NDEBUG) || defined(__DICTIONARY_IMPLEMENTATION__) */

/***********************************************************************\
* Name   : Dictionary_initIterator
* Purpose: init dictionary iterator
* Input  : dictionaryIterator - iterator variable
*          dictionary         - dictionary
* Output : dictionaryIterator - initialized iterator variable
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_initIterator(DictionaryIterator *dictionaryIterator,
                             Dictionary         *dictionary
                            );

/***********************************************************************\
* Name   : Dictionary_doneIterator
* Purpose: deinit dictionary iterator
* Input  : dictionaryIterator - dictionary iterator
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_doneIterator(DictionaryIterator *dictionaryIterator);

/***********************************************************************\
* Name   : Dictionary_getNext
* Purpose: get next entry from dictionary
* Input  : dictionary - dictionary
* Output : keyData    - internal key data (can be NULL)
*          keyLength  - length of key data (can be NULL)
*          data       - internal entry data (can be NULL)
*          length     - length of data (can be NULL)
* Return : TRUE if got entry, FALSE if no more entries
* Notes  : -
\***********************************************************************/

bool Dictionary_getNext(DictionaryIterator *dictionaryIterator,
                        const void         **keyData,
                        ulong              *keyLength,
                        void               **data,
                        ulong              *length
                       );

/***********************************************************************\
* Name   : Dictionary_iterate
* Purpose: iterate over dictionary
* Input  : dictionary                - dictionary
*          dictionaryIterateFunction - iterator function
*          dictionaryIterateUserData - iterator function user data
* Output : -
* Return : TRUE if iteration done, FALSE if aborted
* Notes  : -
\***********************************************************************/

bool Dictionary_iterate(Dictionary                *dictionary,
                        DictionaryIterateFunction dictionaryIterateFunction,
                        void                      *dictionaryIterateUserData
                       );

#ifndef NDEBUG

/***********************************************************************\
* Name   : Dictionary_debugDump
* Purpose: print dictionary (debug only)
* Input  : dictionary - dictionary
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_debugDump(Dictionary *dictionary);

/***********************************************************************\
* Name   : Dictionary_printStatistic
* Purpose: print statistics (debug only)
* Input  : dictionary - dictionary
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_printStatistic(Dictionary *dictionary);

#endif /* NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __DICTIONARIES__ */

/* end of file */
