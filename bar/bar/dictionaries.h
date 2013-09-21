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

#include "global.h"
#include "semaphores.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

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
  DictionaryEntryTable      *entryTables;                // tables array
  uint                      entryTableCount;             // number of tables
  DictionaryCompareFunction dictionaryCompareFunction;
  void                      *dictionaryCompareUserData;
} Dictionary;

typedef struct
{
  Dictionary *dictionary;
  uint       i,j;
} DictionaryIterator;

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

#ifndef NDEBUG
  #define Dictionary_init(dictionary,dictionaryCompareFunction,dictionaryCompareUserData) __Dictionary_init(__FILE__,__LINE__,dictionary,dictionaryCompareFunction,dictionaryCompareUserData)
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
*          dictionaryCompareFunction - compare function or NULL
*          dictionaryCompareUserData - compare function user data
* Output : -
* Return : TRUE if dictionary initialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  bool Dictionary_init(Dictionary                *dictionary,
                       DictionaryCompareFunction dictionaryCompareFunction,
                       void                      *dictionaryCompareUserData
                      );
#else /* not NDEBUG */
  bool __Dictionary_init(const char                *__fileName__,
                         ulong                     __lineNb__,
                         Dictionary                *dictionary,
                         DictionaryCompareFunction dictionaryCompareFunction,
                         void                      *dictionaryCompareUserData
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Dictionary_done
* Purpose: deinitialize dictionary
* Input  : dictionary             - dictionary
*          dictionaryFreeFunction - dictionary entry free function or
*                                   NULL
*          dictionaryFreeUserData - dictionary entry free function user
*                                   data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_done(Dictionary             *dictionary,
                     DictionaryFreeFunction dictionaryFreeFunction,
                     void                   *dictionaryFreeUserData
                    );

/***********************************************************************\
* Name   : Dictionary_clear
* Purpose: clear dictionary
* Input  : dictionary             - dictionary
*          dictionaryFreeFunction - dictionary entry free function or
*                                   NULL
*          dictionaryFreeUserData - dictionary entry free function user
*                                   data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_clear(Dictionary             *dictionary,
                      DictionaryFreeFunction dictionaryFreeFunction,
                      void                   *dictionaryFreeUserData
                     );

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
* Input  : dictionary             - dictionary
*          keyData                - key data
*          keyLength              - length of key data
*          dictionaryFreeFunction - dictionary entry free function or
*                                   NULL
*          dictionaryFreeUserData - dictionary entry free function user
*                                   data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_remove(Dictionary             *dictionary,
                       const void             *keyData,
                       ulong                  keyLength,
                       DictionaryFreeFunction dictionaryFreeFunction,
                       void                   *dictionaryFreeUserData
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
#if defined(NDEBUG) || defined(__DICTIONARY_IMPLEMENATION__)
INLINE bool Dictionary_contains(Dictionary *dictionary,
                                const void *keyData,
                                ulong      keyLength
                               )
{
  assert(dictionary != NULL);

  return Dictionary_find(dictionary,keyData,keyLength,NULL,NULL);
}
#endif /* defined(NDEBUG) || defined(__DICTIONARY_IMPLEMENATION__) */

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
* Name   : Dictionary_getNext
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
* Output : keyData   - internal key data (can be NULL)
*          keyLength - length of key data (can be NULL)
*          data      - internal entry data (can be NULL)
*          length    - length of data (can be NULL)
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
* Return : -
* Notes  : -
\***********************************************************************/

bool Dictionary_iterate(Dictionary                *dictionary,
                        DictionaryIterateFunction dictionaryIterateFunction,
                        void                      *dictionaryIterateUserData
                       );

/***********************************************************************\
* Name   : Dictionary_printStatistic
* Purpose: print statistics (debug only)
* Input  : dictionary - dictionary
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
void Dictionary_printStatistic(Dictionary *dictionary);
#endif /* NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __DICTIONARIES__ */

/* end of file */
