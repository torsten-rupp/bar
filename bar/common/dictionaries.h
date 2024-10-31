/***********************************************************************\
*
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
* Name   : DictionaryInitEntryFunction
* Purpose: copy dictionary entry
* Input  : fromValue - source value data
*          toValue   - destination value data memory
*          length    - length of data entries
*          userData  - user data or NULL
* Output : -
* Return : TRUE if copied, FALSE otherwise
* Notes  : -
\***********************************************************************/

typedef bool(*DictionaryInitEntryFunction)(const void *fromValue, void *toValue, ulong length, void *userData);

/***********************************************************************\
* Name   : DictionaryDoneEntryFunction
* Purpose: delete dictionary entry
* Input  : value       - value data
*          valueLength - length of value data
*          userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*DictionaryDoneEntryFunction)(void *value, ulong valueLength, void *userData);

/***********************************************************************\
* Name   : DictionaryCompareEntryFunction
* Purpose: compare dictionary entries
* Input  : value0,value1 - value data to compare
*          valueLength   - length of value data
*          userData      - user data
* Output : -
* Return : TRUE if equal, FALSE otherwise
* Notes  : -
\***********************************************************************/

typedef bool(*DictionaryCompareEntryFunction)(const void *value0, const void *value1, ulong valueLength, void *userData);

// dictionary entry
typedef struct
{
  bool  isUsed;                                                    // TRUE iff entry is used
  bool  isAllocated;                                               // TRUE iff key/value data was allocated
  ulong hash;                                                      // hash code
  void  *key;                                                      // key data
  ulong keyLength;                                                 // length of key data
  void  *value;                                                    // value data of entry
  ulong valueLength;                                               // length of value data in entry
} DictionaryEntry;

// table with dictionary entries
typedef struct
{
  DictionaryEntry *entries;                                        // entries array
  uint            sizeIndex;                                       // array size index (see TABLE_SIZES)
  uint            entryCount;                                      // number of entries in array
} DictionaryEntryTable;

// dictionary
typedef struct
{
  Semaphore                      lock;
  uint                           lockCount;
  DictionaryEntryTable           *entryTables;                     // tables array
  uint                           entryTableCount;                  // number of tables
  DictionaryInitEntryFunction    dictionaryInitEntryFunction;      // copy entry function call back or NULL
  void                           *dictionaryInitEntryUserData;
  DictionaryDoneEntryFunction    dictionaryDoneEntryFunction;      // free entry function call back or NULL
  void                          *dictionaryDoneEntryUserData;
  DictionaryCompareEntryFunction dictionaryCompareEntryFunction;   // compare key data function call back or NULL
  void                           *dictionaryCompareEntryUserData;
} Dictionary;

typedef struct
{
  Dictionary *dictionary;
  uint       i,j;
} DictionaryIterator;

/***********************************************************************\
* Name   : DictionaryIterateFunction
* Purpose: iterate over dictionary entires
* Input  : key         - key data
*          keyLength   - length of key data
*          value       - value data
*          valueLength - length of value data
*          userData  - user data
* Output : -
* Return : TRUE to continue, FALSE abort
* Notes  : -
\***********************************************************************/

typedef bool(*DictionaryIterateFunction)(const void *key,
                                         ulong      keyLength,
                                         void       *value,
                                         ulong      valueLength,
                                         void       *userData
                                        );

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#define DICTIONARY_BYTE_INIT_ENTRY    CALLBACK_(Dictionary_byteInitEntry,NULL)
#define DICTIONARY_BYTE_DONE_ENTRY    CALLBACK_(NULL,NULL)
#define DICTIONARY_BYTE_COMPARE_ENTRY CALLBACK_(NULL,NULL)

#define DICTIONARY_VALUE_INIT_ENTRY    CALLBACK_(NULL,NULL)
#define DICTIONARY_VALUE_DONE_ENTRY    CALLBACK_(NULL,NULL)
#define DICTIONARY_VALUE_COMPARE_ENTRY CALLBACK_(Dictionary_valueCompareEntry,NULL)

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

#define DICTIONARY_ITERATE(dictionary,key,keyLength,data,length) \
  for (DictionaryIterator dictionaryIterator, Dictionary_initIterator(&dictionaryIterator,dictionary); \
       Dictionary_getNext(&dictionaryIterator,key,keyLength,data,length); \
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

#define DICTIONARY_ITERATEX(dictionary,key,keyLength,data,length,condition) \
  for (DictionaryIterator dictionaryIterator, Dictionary_initIterator(&dictionaryIterator,dictionary); \
       Dictionary_getNext(&dictionaryIterator,key,keyLength,data,length) && (condition); \
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
* Name   : Dictionary_byteInitEntry
* Purpose: byte copy data
* Input  : fromValue - source value data
*          toValue   - destination value data memory
*          length    - length of data entries
*          userData  - user data or NULL
* Output : -
* Return : TRUE
* Return : -
* Notes  : -
\***********************************************************************/

bool Dictionary_byteInitEntry(const void *fromValue, void *toValue, ulong length, void *userData);

/***********************************************************************\
* Name   : Dictionary_valueCompareEntry
* Purpose: compare data values (pointers)
* Input  : value0,value1 - value data
*          valueLength   - length of value data (not used)
*          userData      - user data or NULL
* Output : -
* Return : TRUE
* Return : -
* Notes  : -
\***********************************************************************/

bool Dictionary_valueCompareEntry(const void *value0, const void *value1, ulong valueLength, void *userData);

/***********************************************************************\
* Name   : Dictionary_init/Dictionary_initValue
* Purpose: initialize dictionary
* Input  : dictionary                     - dictionary variable
*          dictionaryInitEntryFunction    - init value call back or
*                                           NULL
*          dictionaryInitEntryUserData    - init value call back user
*                                           data
*          dictionaryDoneEntryFunction    - done value call back or
*                                           NULL
*          dictionaryDoneEntryUserData    - done value call back user
*                                           data
*          dictionaryCompareEntryFunction - compare value call back
*                                           or NULL
*          dictionaryCompareEntryUserData - compare value call back
*                                           user data
* Output : -
* Return : TRUE if dictionary initialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  bool Dictionary_init(Dictionary                     *dictionary,
                       DictionaryInitEntryFunction    dictionaryInitEntryFunction,
                       void                           *dictionaryInitEntryUserData,
                       DictionaryDoneEntryFunction    dictionaryDoneEntryFunction,
                       void                           *dictionaryDoneEntryUserData,
                       DictionaryCompareEntryFunction dictionaryCompareEntryFunction,
                       void                           *dictionaryCompareEntryUserData
                      );
#else /* not NDEBUG */
  bool __Dictionary_init(const char                     *__fileName__,
                         ulong                          __lineNb__,
                         Dictionary                     *dictionary,
                         DictionaryInitEntryFunction    dictionaryInitEntryFunction,
                         void                           *dictionaryInitEntryUserData,
                         DictionaryDoneEntryFunction    dictionaryDoneEntryFunction,
                         void                           *dictionaryDoneEntryUserData,
                         DictionaryCompareEntryFunction dictionaryCompareEntryFunction,
                         void                           *dictionaryCompareEntryUserData
                        );
#endif /* NDEBUG */

static inline bool Dictionary_initValue(Dictionary *dictionary)
{
  return Dictionary_init(dictionary,
                         DICTIONARY_VALUE_INIT_ENTRY,
                         DICTIONARY_VALUE_DONE_ENTRY,
                         DICTIONARY_VALUE_COMPARE_ENTRY
                        );
}

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
* Name   : Dictionary_add/Dictionary_addValue
* Purpose: add entry/value to dictionary
* Input  : dictionary  - dictionary
*          key         - key data
*          keyLength   - length of key data
*          value       - value data
*          valueLength - length of value data
* Output : -
* Return : TRUE if entry added, FALSE otherwise
* Notes  : key data and data will be copied!
\***********************************************************************/

bool Dictionary_add(Dictionary *dictionary,
                    const void *key,
                    ulong      keyLength,
                    const void *value,
                    ulong      valueLength
                   );

static inline bool Dictionary_addValue(Dictionary *dictionary,
                                       intptr_t   key,
                                       intptr_t   data
                                      )
{
  return Dictionary_add(dictionary,(const void*)key,0,(void**)data,0);
}

/***********************************************************************\
* Name   : Dictionary_remove/Dictionary_removeValue
* Purpose: remove entry/value from dictionary
* Input  : dictionary - dictionary
*          key        - key data
*          keyLength  - length of key data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_remove(Dictionary *dictionary,
                       const void *key,
                       ulong      keyLength
                      );

static inline void Dictionary_removeValue(Dictionary *dictionary,
                                          intptr_t   key
                                         )
{
  Dictionary_remove(dictionary,(void*)key,0);
}

/***********************************************************************\
* Name   : Dictionary_find, Dictionary_findValue
* Purpose: find entry/value in dictionary
* Input  : dictionary - dictionary
*          key        - key data
*          keyLength  - length of key data
* Output : value       - value data iff entry found (can be NULL)
*          valueLength - length of value data (can be NULL)
* Return : TRUE if entry found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Dictionary_find(Dictionary *dictionary,
                     const void *key,
                     ulong      keyLength,
                     void       **value,
                     ulong      *valueLength
                    );

static inline bool Dictionary_findValue(Dictionary *dictionary,
                                        intptr_t   key,
                                        intptr_t   *value
                                       )
{
  return Dictionary_find(dictionary,(void*)key,0,(void**)value,0);
}

/***********************************************************************\
* Name   : Dictionary_contains/Dictionary_containsValue
* Purpose: check if entry/value is contained in dictionary
* Input  : dictionary - dictionary
*          key        - key data
*          keyLength  - length of key data
* Output : -
* Return : TRUE if entry is in dictionary, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Dictionary_contains(Dictionary *dictionary,
                                const void *key,
                                ulong      keyLength
                               );
#if defined(NDEBUG) || defined(__DICTIONARY_IMPLEMENTATION__)
INLINE bool Dictionary_contains(Dictionary *dictionary,
                                const void *key,
                                ulong      keyLength
                               )
{
  assert(dictionary != NULL);

  return Dictionary_find(dictionary,key,keyLength,NULL,NULL);
}
#endif /* defined(NDEBUG) || defined(__DICTIONARY_IMPLEMENTATION__) */

static inline bool Dictionary_containsValue(Dictionary *dictionary,
                                            intptr_t   key
                                           )
{
  return Dictionary_contains(dictionary,(const void*)key,0);
}

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
* Name   : Dictionary_getNext/Dictionary_getNextValue
* Purpose: get next entry/value from dictionary
* Input  : dictionary  - dictionary
* Output : key         - internal key data (can be NULL)
*          keyLength   - length of key data (can be NULL)
*          value       - internal value data (can be NULL)
*          valueLength - length of value data (can be NULL)
* Return : TRUE if got entry, FALSE if no more entries
* Notes  : -
\***********************************************************************/

bool Dictionary_getNext(DictionaryIterator *dictionaryIterator,
                        const void         **key,
                        ulong              *keyLength,
                        void               **value,
                        ulong              *valueLength
                       );

static inline bool Dictionary_getNextValue(DictionaryIterator *dictionaryIterator,
                                           intptr_t           *key,
                                           intptr_t           *value
                                          )
{
  return Dictionary_getNext(dictionaryIterator,(void*)key,0,(void**)value,0);
}

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
