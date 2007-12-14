/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/dictionaries.h,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: hash table functions
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

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/* dictionary compare function */
typedef int(*DictionaryCompareFunction)(void *userData, const void *data0, const void *data1, ulong length);

typedef struct
{
  ulong hash;
  void  *keyData;                            // key data
  ulong keyLength;                           // length of key data
  void  *data;                               // data of entry
  ulong length;                              // length of data in entry
} DictionaryEntry;

typedef struct
{
  DictionaryEntry *entries;
  uint            sizeIndex; 
  uint            entryCount;
} DictionaryEntryTable;

typedef struct
{
  DictionaryEntryTable      *entryTables;
  uint                      entryTableCount;
  DictionaryCompareFunction dictionaryCompareFunction;
  void                      *dictionaryCompareUserData;
} Dictionary;

typedef struct
{
  const Dictionary *dictionary;
  uint             i,j;
} DictionaryIterator;

/* delete dictionary entry function */
typedef void(*DictionaryFreeFunction)(void *userData, const void *data, ulong length);

/* iterator function */
typedef bool(*DictionaryIterateFunction)(void *userData, const void *keyData, ulong keyLength, const void *data, ulong length);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

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

bool Dictionary_init(Dictionary                *dictionary,
                     DictionaryCompareFunction dictionaryCompareFunction,
                     void                      *dictionaryCompareUserData
                    );

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

ulong Dictionary_count(const Dictionary *dictionary);

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
* Notes  : -
\***********************************************************************/

bool Dictionary_add(Dictionary *dictionary,
                    const void *keyData,
                    ulong      keyLength,
                    const void *data,
                    ulong      length
                   );

/***********************************************************************\
* Name   : Dictionary_rem
* Purpose: remove entry from dictionary
* Input  : dictionary             - dictionary
*          data                   - data
*          length                 - length of data
*          dictionaryFreeFunction - dictionary entry free function or
*                                   NULL
*          dictionaryFreeUserData - dictionary entry free function user
*                                   data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_rem(Dictionary             *dictionary,
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
* Name   : Dictionary_contain
* Purpose: check if entry is in dictionary
* Input  : dictionary - dictionary
*          data       - data
*          length     - length of data
* Output : -
* Return : TRUE if entry is in dictionary, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Dictionary_contain(Dictionary *dictionary,
                        const void *keyData,
                        ulong      keyLength
                       );

/***********************************************************************\
* Name   : Dictionary_initIterator
* Purpose: init dictionary iterator
* Input  : dictionary         - dictionary
*          dictionaryIterator - iterator variable
* Output : dictionaryIterator - initialized iterator variable
* Return : -
* Notes  : -
\***********************************************************************/

void Dictionary_initIterator(DictionaryIterator *dictionaryIterator,
                             const Dictionary   *dictionary
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
* Output : keyData   - key data (can be NULL)
*          keyLength - length of key data (can be NULL)
*          data      - entry data (can be NULL)
*          length    - length of data (can be NULL)
* Return : TRUE if got entry, FALSE if no more entries
* Notes  : -
\***********************************************************************/

bool Dictionary_getNext(DictionaryIterator *dictionaryIterator,
                        const void         **keyData,
                        ulong              *keyLength,
                        const void         **data,
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

#ifndef NDEBUG
void Dictionary_printStatistic(const Dictionary *dictionary);
#endif /* NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __DICTIONARIES__ */

/* end of file */
