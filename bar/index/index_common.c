/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index common functions
* Systems: all
*
\***********************************************************************/

#define __INDEX_COMMON_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#include "common/global.h"
#include "common/dictionaries.h"
#include "common/threads.h"
#include "common/strings.h"
#include "common/database.h"
#include "common/arrays.h"
#include "common/files.h"
#include "common/filesystems.h"
#include "common/misc.h"
#include "errors.h"

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "storage.h"
#include "server_io.h"
#include "index_definition.h"
#include "archive.h"

#include "index/index.h"
#include "index/index_entities.h"
#include "index/index_entries.h"
#include "index/index_storages.h"
#include "index/index_uuids.h"

#include "index/index_common.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

// TODO: add prefix
bool                       indexInitializedFlag = FALSE;
IndexIsMaintenanceTime     indexIsMaintenanceTimeFunction;
void                       *indexIsMaintenanceTimeUserData;
Semaphore                  indexLock;
Array                      indexUsedBy;
Thread                     indexThread;    // upgrade/clean-up thread
Semaphore                  indexThreadTrigger;
IndexHandle                *indexThreadIndexHandle;
Semaphore                  indexClearStorageLock;
bool                       indexQuitFlag;

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : DIMPORT
* Purpose: debug import index
* Input  : format - format string
*          ...    - optional arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef INDEX_DEBUG_IMPORT_OLD_DATABASE
  #define DIMPORT(format,...) \
    do \
    { \
      logImportIndex(__FILE__,__LINE__,format, ## __VA_ARGS__); \
    } \
    while (0)
#else /* not INDEX_DEBUG_IMPORT_OLD_DATABASE */
  #define DIMPORT(format,...) \
    do \
    { \
    } \
    while (0)
#endif /* INDEX_DEBUG_IMPORT_OLD_DATABASE */

#ifndef NDEBUG
  #define openIndex(...)   __openIndex  (__FILE__,__LINE__, ## __VA_ARGS__)
  #define closeIndex(...)  __closeIndex (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

const char *IndexCommon_getIndexInUseInfo(void)
{
  static char buffer[256];

  ArrayIterator arrayIterator;
  ThreadInfo    threadInfo;

  stringClear(buffer);
  SEMAPHORE_LOCKED_DO(&indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    ARRAY_ITERATE(&indexUsedBy,arrayIterator,threadInfo)
    {
      if (!stringIsEmpty(buffer)) stringAppendChar(buffer,sizeof(buffer),' ');
      stringAppendFormat(buffer,sizeof(buffer),"%s",Thread_getIdString(threadInfo.threadId));
    }
  }

  return buffer;
}

void IndexCommon_indexThreadInterrupt(void)
{
  if (   indexInitializedFlag
      && !IndexCommon_isMaintenanceTime(Misc_getCurrentDateTime())
      && !indexQuitFlag
     )
  {
    assert(indexThreadIndexHandle != NULL);

    Database_interrupt(&indexThreadIndexHandle->databaseHandle);
  }
}

// ---------------------------------------------------------------------

String IndexCommon_getIndexStateSetString(String string, IndexStateSet indexStateSet)
{
  IndexStates indexState;

  assert(string != NULL);

  String_clear(string);
  for (indexState = INDEX_STATE_MIN; indexState <= INDEX_STATE_MAX; indexState++)
  {
    if (IN_SET(indexStateSet,indexState))
    {
      if (!String_isEmpty(string)) String_appendChar(string,',');
      String_appendFormat(string,"%d",indexState);
    }
  }

  return string;
}

String IndexCommon_getIndexModeSetString(String string, IndexModeSet indexModeSet)
{
  IndexModes indexMode;

  assert(string != NULL);

  String_clear(string);
  for (indexMode = INDEX_MODE_MIN; indexMode <= INDEX_MODE_MAX; indexMode++)
  {
    if (IN_SET(indexModeSet,indexMode))
    {
      if (!String_isEmpty(string)) String_appendChar(string,',');
      String_appendFormat(string,"%d",indexMode);
    }
  }

  return string;
}

String IndexCommon_getPostgreSQLFTSTokens(String string, ConstString text)
{
  bool           spaceFlag;
  StringIterator stringIterator;
  Codepoint      codepoint;

  String_clear(string);

  if (text != NULL)
  {
    spaceFlag = FALSE;
    STRING_CHAR_ITERATE_UTF8(text,stringIterator,codepoint)
    {
      if (!isCharUTF8(codepoint))
      {
        if      (isalnum((int)codepoint))
        {
          String_appendCharUTF8(string,codepoint);
          spaceFlag = FALSE;
        }
        else if (!spaceFlag)
        {
          String_appendChar(string,' ');
          spaceFlag = TRUE;
        }
      }
      else
      {
        String_appendCharUTF8(string,codepoint);
        spaceFlag = FALSE;
      }
    }
  }

  return string;
}

String IndexCommon_getFTSMatchString(String         string,
                                     DatabaseHandle *databaseHandle,
                                     const char     *tableName,
                                     const char     *columnName,
                                     ConstString    patternText
                                    )
{
  String          pattern;
  StringTokenizer stringTokenizer;
  ConstString     token;
  bool            addedTextFlag,addedPatternFlag;
  StringIterator  stringIterator;
  Codepoint       codepoint;

  assert(string != NULL);
  assert(databaseHandle != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);

  // init variables
  pattern = String_new();

  String_clear(string);

  if (!String_isEmpty(patternText))
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        String_initTokenizer(&stringTokenizer,
                             patternText,
                             STRING_BEGIN,
                             STRING_WHITE_SPACES,
                             STRING_QUOTES,
                             TRUE
                            );
        while (String_getNextToken(&stringTokenizer,&token,NULL))
        {
          addedTextFlag    = FALSE;
          addedPatternFlag = FALSE;
          STRING_CHAR_ITERATE_UTF8(token,stringIterator,codepoint)
          {
            if (isalnum(codepoint) || isCharUTF8(codepoint))
            {
              // add text character
              if (addedPatternFlag)
              {
                String_appendChar(pattern,' ');
                addedPatternFlag = FALSE;
              }
              String_appendCharUTF8(pattern,codepoint);
              addedTextFlag = TRUE;
            }
            else
            {
              // add pattern
              if (addedTextFlag && !addedPatternFlag)
              {
                String_appendChar(pattern,'*');
                addedTextFlag    = FALSE;
                addedPatternFlag = TRUE;
              }
            }
          }
          if (addedTextFlag && !addedPatternFlag)
          {
            String_appendChar(pattern,'*');
          }
        }
        String_doneTokenizer(&stringTokenizer);

        if (!String_isEmpty(pattern)) String_appendFormat(string,"%s MATCH '%S'",tableName,pattern);

        break;
      case DATABASE_TYPE_MARIADB:
        String_initTokenizer(&stringTokenizer,
                             patternText,
                             STRING_BEGIN,
                             STRING_WHITE_SPACES,
                             STRING_QUOTES,
                             TRUE
                            );
        while (String_getNextToken(&stringTokenizer,&token,NULL))
        {
          addedTextFlag    = FALSE;
          addedPatternFlag = FALSE;
          STRING_CHAR_ITERATE_UTF8(token,stringIterator,codepoint)
          {
            if (isalnum(codepoint) || isCharUTF8(codepoint))
            {
              // add text character
              if (addedPatternFlag)
              {
                String_appendChar(pattern,' ');
                addedPatternFlag = FALSE;
              }
              String_appendCharUTF8(pattern,codepoint);
              addedTextFlag = TRUE;
            }
            else
            {
              // add pattern
              if (addedTextFlag && !addedPatternFlag)
              {
                String_appendChar(pattern,'*');
                addedTextFlag    = FALSE;
                addedPatternFlag = TRUE;
              }
            }
          }
          if (addedTextFlag && !addedPatternFlag)
          {
            String_appendChar(pattern,'*');
          }
        }
        String_doneTokenizer(&stringTokenizer);

        if (!String_isEmpty(pattern)) String_appendFormat(string,"MATCH(%s.%s) AGAINST('%S' IN BOOLEAN MODE)",tableName,columnName,pattern);
        break;
      case DATABASE_TYPE_POSTGRESQL:
        {
          bool firstTokenFlag;

          String_initTokenizer(&stringTokenizer,
                               patternText,
                               STRING_BEGIN,
                               STRING_WHITE_SPACES,
                               STRING_QUOTES,
                               TRUE
                              );
          firstTokenFlag = TRUE;
          while (String_getNextToken(&stringTokenizer,&token,NULL))
          {
            if (!firstTokenFlag)
            {
              String_appendCString(pattern," & ");
            }

            addedTextFlag    = FALSE;
            addedPatternFlag = FALSE;
            STRING_CHAR_ITERATE_UTF8(token,stringIterator,codepoint)
            {
              if (isalnum(codepoint) || isCharUTF8(codepoint))
              {
                // add text character
                if (addedPatternFlag)
                {
                  String_appendCString(pattern," & ");
                  addedPatternFlag = FALSE;
                }
                String_appendCharUTF8(pattern,codepoint);
                addedTextFlag = TRUE;
              }
              else
              {
                // add pattern
                if (addedTextFlag && !addedPatternFlag)
                {
                  String_appendCString(pattern,":*");
                  addedTextFlag    = FALSE;
                  addedPatternFlag = TRUE;
                }
              }
            }
            if (addedTextFlag && !addedPatternFlag)
            {
              String_appendCString(pattern,":*");
            }

            firstTokenFlag = FALSE;
          }
          String_doneTokenizer(&stringTokenizer);
          
          if (!String_isEmpty(pattern)) String_appendFormat(string,"%s.%s @@ to_tsquery('%S')",tableName,columnName,pattern);
        }
        break;
    }
  }
  
  // free resources
  String_delete(pattern);

  return string;
}

void IndexCommon_appendOrdering(String          orderString,
                                bool            condition,
                                const char      *columnName,
                                DatabaseOrdering ordering
                               )
{
  assert(orderString != NULL);

  if (condition && (ordering != DATABASE_ORDERING_NONE))
  {
    assert(columnName != NULL);

    if (!String_isEmpty(orderString))
    {
      String_appendChar(orderString,',');
    }
    String_appendCString(orderString,columnName);
    switch (ordering)
    {
      case DATABASE_ORDERING_NONE:       /* nothing tod do */                       break;
      case DATABASE_ORDERING_ASCENDING:  String_appendCString(orderString," ASC");  break;
      case DATABASE_ORDERING_DESCENDING: String_appendCString(orderString," DESC"); break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif /* NDEBUG */
    }
  }
}

void IndexCommon_initIndexQueryHandle(IndexQueryHandle *indexQueryHandle, IndexHandle *indexHandle)
{
  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  indexQueryHandle->indexHandle = indexHandle;
}

void IndexCommon_doneIndexQueryHandle(IndexQueryHandle *indexQueryHandle)
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  UNUSED_VARIABLE(indexQueryHandle);
}

Errors IndexCommon_beginInterruptableOperation(IndexHandle *indexHandle, bool *transactionFlag)
{
  Errors error;

  assert(indexHandle != NULL);
  assert(transactionFlag != NULL);

  (*transactionFlag) = FALSE;

  error = Index_beginTransaction(indexHandle,WAIT_FOREVER);
  if (error != ERROR_NONE)
  {
    return error;
  }

  (*transactionFlag) = TRUE;

  return ERROR_NONE;
}

Errors IndexCommon_endInterruptableOperation(IndexHandle *indexHandle, bool *transactionFlag)
{
  Errors error;

  assert(indexHandle != NULL);
  assert(transactionFlag != NULL);

  if (*transactionFlag)
  {
    error = Index_endTransaction(indexHandle);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  (*transactionFlag) = FALSE;

  return ERROR_NONE;
}

Errors IndexCommon_interruptOperation(IndexHandle *indexHandle, bool *transactionFlag, ulong time)
{
  bool   transactionSuspendFlag;
  Errors error;

  assert(indexHandle != NULL);
  assert(transactionFlag != NULL);
  assert(*transactionFlag);

  if (IndexCommon_isIndexInUse())
  {
    transactionSuspendFlag = (*transactionFlag);

    if (transactionSuspendFlag)
    {
      // temporary end transaction
      error = Index_endTransaction(indexHandle);
      if (error != ERROR_NONE)
      {
        return error;
      }
      (*transactionFlag) = FALSE;
    }

    // wait until index is unused
    WAIT_NOT_IN_USE(time);
    if (indexQuitFlag)
    {
      return ERROR_INTERRUPTED;
    }

    if (transactionSuspendFlag)
    {
      // begin transaction
      error = Index_beginTransaction(indexHandle,WAIT_FOREVER);
      if (error != ERROR_NONE)
      {
        return error;
      }
      (*transactionFlag) = TRUE;
    }
  }

  return ERROR_NONE;
}

Errors IndexCommon_delete(IndexHandle          *indexHandle,
                          bool                 *doneFlag,
                          ulong                *deletedCounter,
                          const char           *tableName,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount
                         )
{
  Errors error;
  ulong  changedRowCount;
  #ifdef INDEX_DEBUG_PURGE
    uint64 t0,t1;
  #endif
  bool   indexInUse;

  assert(indexHandle != NULL);
  assert(tableName != NULL);

//fprintf(stderr,"%s, %d: purge (%d): %s %s\n",__FILE__,__LINE__,IndexCommon_isIndexInUse(),tableName,String_cString(filterString));
  error = ERROR_NONE;
  do
  {
    changedRowCount = 0;
    #ifdef INDEX_DEBUG_PURGE
      t0 = Misc_getTimestamp();
    #endif
    error = Database_delete(&indexHandle->databaseHandle,
                            &changedRowCount,
                            tableName,
                            DATABASE_FLAG_NONE,
                            filter,
                            filters,
                            filterCount,
                            SINGLE_STEP_PURGE_LIMIT
                           );
    if (deletedCounter != NULL)(*deletedCounter) += changedRowCount;
    #ifdef INDEX_DEBUG_PURGE
      if (changedRowCount > 0)
      {
        t1 = Misc_getTimestamp();
        fprintf(stderr,"%s, %d: error: %s, deleted %lu entries from '%s': %"PRIu64"ms\n",__FILE__,__LINE__,
                Error_getText(error),
                changedRowCount,
                tableName,
                (t1-t0)/US_PER_MS
               );
      }
    #endif
//fprintf(stderr,"%s, %d: IndexCommon_isIndexInUse=%d\n",__FILE__,__LINE__,IndexCommon_isIndexInUse());
    indexInUse = IndexCommon_isIndexInUse();
  }
  while (   (error == ERROR_NONE)
         && (changedRowCount > 0)
         && !indexInUse
        );
  #ifdef INDEX_DEBUG_PURGE
    if (error == ERROR_INTERRUPTED)
    {
      fprintf(stderr,"%s, %d: delete interrupted\n",__FILE__,__LINE__);
    }
  #endif

  // update done-flag
  if ((error == ERROR_NONE) && (doneFlag != NULL))
  {
    if ((changedRowCount > 0) && indexInUse)
    {
      (*doneFlag) = FALSE;
    }
  }

  return error;
}

Errors IndexCommon_deleteByIds(IndexHandle      *indexHandle,
                               bool             *doneFlag,
                               ulong            *deletedCounter,
                               const char       *tableName,
                               const char       *columnName,
                               const DatabaseId ids[],
                               ulong            idCount
                              )
{
  const uint INCREMENT_SIZE = 1024;

  Errors error;
  ulong  i;
  ulong  changedRowCount;
  #ifdef INDEX_DEBUG_PURGE
    uint64 t0,t1;
  #endif
  bool   indexInUse;

  assert(indexHandle != NULL);
  assert(tableName != NULL);

  error = ERROR_NONE;
  i = 0;
  if (idCount > 0)
  {
    do
    {
      changedRowCount = 0;
      #ifdef INDEX_DEBUG_PURGE
        t0 = Misc_getTimestamp();
      #endif
      error = Database_deleteByIds(&indexHandle->databaseHandle,
                                   &changedRowCount,
                                   tableName,
                                   columnName,
                                   DATABASE_FLAG_NONE,
                                   &ids[i],
                                   MIN(idCount-i,INCREMENT_SIZE)
                                  );
      if (deletedCounter != NULL)(*deletedCounter) += changedRowCount;
      #ifdef INDEX_DEBUG_PURGE
        if (changedRowCount > 0)
        {
          t1 = Misc_getTimestamp();
          fprintf(stderr,"%s, %d: error: %s, deleted %lu entries from '%s': %"PRIu64"ms\n",__FILE__,__LINE__,
                  Error_getText(error),
                  changedRowCount,
                  tableName,
                  (t1-t0)/US_PER_MS
                 );
        }
      #endif

      indexInUse = IndexCommon_isIndexInUse();

      i += INCREMENT_SIZE;
    }
    while (   (i < idCount)
           && (error == ERROR_NONE)
           && (changedRowCount > 0)
           && !indexInUse
          );
    #ifdef INDEX_DEBUG_PURGE
      if (error == ERROR_INTERRUPTED)
      {
        fprintf(stderr,"%s, %d: delete interrupted\n",__FILE__,__LINE__);
      }
    #endif

    // update done-flag
    if ((error == ERROR_NONE) && (doneFlag != NULL))
    {
      if ((changedRowCount > 0) && indexInUse)
      {
        (*doneFlag) = FALSE;
      }
    }
  }

  return error;
}

/***********************************************************************\
* Name   : rebuildNewestInfo
* Purpose:
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

//TODO
#if 0
// not used
LOCAL Errors rebuildNewestInfo(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       entryId;
  String           name;
  uint64           size;
  uint64           timeModified;


  error = Index_initListEntries(&indexQueryHandle,
                                indexHandle,
                                NULL,  // indexIds
                                0L,  // indexIdCount
                                NULL,  // entryIds
                                0L,  // entryIdCount
                                INDEX_TYPESET_ANY_ENTRY,
                                NULL,  // entryPattern,
                                FALSE,  // newestOnly
                                FALSE,  // fragmentsCount
                                INDEX_ENTRY_SORT_MODE_NONE,
                                DATABASE_ORDERING_NONE,
                                0LL,  // offset
                                INDEX_UNLIMITED
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  name = String_new();
  while (Index_getNextEntry(&indexQueryHandle,
                            NULL,  // uuidId
                            NULL,  // jobUUID
                            NULL,  // entityId
                            NULL,  // scheduleUUID
                            NULL,  // hostName
                            NULL,  // userName
                            NULL,  // archiveType
                            NULL,  // storageId
                            NULL,  // storageName
                            NULL,  // storageDateTime
                            &entryId,
                            name,
                            NULL,  // storageId
                            NULL,  // storageName
                            NULL,  // destinationName
                            NULL,  // fileSystemType
                            &size,
                            &timeModified,
                            NULL,  // userId
                            NULL,  // groupId
                            NULL,  // permission
                            NULL  // fragmentCount
                           )
        )
  {
  }
  String_delete(name);
  Index_doneList(&indexQueryHandle);

  return ERROR_NONE;
}
#endif

#ifndef NDEBUG
void IndexCommon_verify(IndexHandle *indexHandle,
                        const char  *tableName,
                        const char  *columnName,
                        int64       value,
                        const char  *condition,
                        ...
                       )
{
//  va_list arguments;
//  Errors  error;
//  int64   n;

  assert(indexHandle != NULL);
  assert(tableName != NULL);
  assert(columnName != NULL);
  assert(condition != NULL);

UNUSED_VARIABLE(value);
//TODO
#if 0
  va_start(arguments,condition);
  error = Database_vgetInteger64(&indexHandle->databaseHandle,
                                &n,
                                tableName,
                                columnName,
                                condition,
                                arguments
                               );
  assert(error == ERROR_NONE);
  assert(n == value);
  va_end(arguments);
#endif
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
