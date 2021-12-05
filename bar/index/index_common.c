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

String IndexCommon_getFTSString(String         string,
                                DatabaseHandle *databaseHandle,
                                const char     *columnName,
                                ConstString    patternText
                               )
{
  StringTokenizer stringTokenizer;
  ConstString     token;
  bool            addedTextFlag,addedPatternFlag;
  size_t          iteratorVariable;
  Codepoint       codepoint;

  String_clear(string);

  if (!String_isEmpty(patternText))
  {
    switch (Database_getType(databaseHandle))
    {
      case DATABASE_TYPE_SQLITE3:
        String_formatAppend(string,"FTS_entries MATCH '");

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
          STRING_CHAR_ITERATE_UTF8(token,iteratorVariable,codepoint)
          {
            if (isalnum(codepoint) || (codepoint >= 128))
            {
              if (addedPatternFlag)
              {
                String_appendChar(string,' ');
                addedPatternFlag = FALSE;
              }
              String_appendCharUTF8(string,codepoint);
              addedTextFlag = TRUE;
            }
            else
            {
              if (addedTextFlag && !addedPatternFlag)
              {
                String_appendChar(string,'*');
                addedTextFlag    = FALSE;
                addedPatternFlag = TRUE;
              }
            }
          }
          if (addedTextFlag && !addedPatternFlag)
          {
            String_appendChar(string,'*');
          }
        }
        String_doneTokenizer(&stringTokenizer);

        String_formatAppend(string,"'");
        break;
      case DATABASE_TYPE_MYSQL:
        String_formatAppend(string,"MATCH(%s) AGAINST('",columnName);

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
          STRING_CHAR_ITERATE_UTF8(token,iteratorVariable,codepoint)
          {
            if (isalnum(codepoint) || (codepoint >= 128))
            {
              if (addedPatternFlag)
              {
                String_appendChar(string,' ');
                addedPatternFlag = FALSE;
              }
              String_appendCharUTF8(string,codepoint);
              addedTextFlag = TRUE;
            }
            else
            {
              if (addedTextFlag && !addedPatternFlag)
              {
                String_appendChar(string,'*');
                addedTextFlag    = FALSE;
                addedPatternFlag = TRUE;
              }
            }
          }
          if (addedTextFlag && !addedPatternFlag)
          {
            String_appendChar(string,'*');
          }
        }
        String_doneTokenizer(&stringTokenizer);

        String_formatAppend(string,"' IN BOOLEAN MODE)");
        break;
    }
  }

  return string;
}

void IndexCommon_filterAppend(String filterString, bool condition, const char *concatenator, const char *format, ...)
{
  va_list arguments;

  if (condition)
  {
    if (!String_isEmpty(filterString))
    {
      String_appendChar(filterString,' ');
      String_appendCString(filterString,concatenator);
      String_appendChar(filterString,' ');
    }
    va_start(arguments,format);
    String_appendVFormat(filterString,format,arguments);
    va_end(arguments);
  }
}

void IndexCommon_appendOrdering(String orderString, bool condition, const char *columnName, DatabaseOrdering ordering)
{
  if (condition && (ordering != DATABASE_ORDERING_NONE))
  {
    if (String_isEmpty(orderString))
    {
      String_appendCString(orderString,"ORDER BY ");
    }
    else
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
  Errors error;

  assert(indexHandle != NULL);
  assert(transactionFlag != NULL);
  assert(*transactionFlag);

  if (IndexCommon_isIndexInUse())
  {
    if (transactionFlag != NULL)
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

    if (transactionFlag != NULL)
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

Errors IndexCommon_purge(IndexHandle          *indexHandle,
                         bool                 *doneFlag,
                         ulong                *deletedCounter,
                         const char           *tableName,
                         const char           *filter,
                         const DatabaseFilter filters[],
                         uint                 filterCount
                        )
{
  #ifdef INDEX_DEBUG_PURGE
    uint64 t0;
  #endif

  Errors error;
  ulong  changedRowCount;

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
    if (error == ERROR_NONE)
    {
      if (deletedCounter != NULL)(*deletedCounter) += changedRowCount;
    }
    #ifdef INDEX_DEBUG_PURGE
      if (changedRowCount > 0)
      {
        fprintf(stderr,"%s, %d: error: %s, purged %lu entries from '%s': %llums\n",__FILE__,__LINE__,
                Error_getText(error),
                changedRowCount,
                tableName,
                (Misc_getTimestamp()-t0)/US_PER_MS
               );
      }
    #endif
//fprintf(stderr,"%s, %d: IndexCommon_isIndexInUse=%d\n",__FILE__,__LINE__,IndexCommon_isIndexInUse());
  }
  while (   (error == ERROR_NONE)
         && (changedRowCount > 0)
         && !IndexCommon_isIndexInUse()
        );
  #ifdef INDEX_DEBUG_PURGE
    if (error == ERROR_INTERRUPTED)
    {
      fprintf(stderr,"%s, %d: purge interrupted\n",__FILE__,__LINE__);
    }
  #endif

  // update done-flag
  if ((error == ERROR_NONE) && (doneFlag != NULL))
  {
//fprintf(stderr,"%s, %d: exists check %s: %s c=%lu count=%d inuse=%d\n",__FILE__,__LINE__,tableName,String_cString(filterString),x,changedRowCount,IndexCommon_isIndexInUse());
    if ((changedRowCount > 0) && IndexCommon_isIndexInUse())
    {
//fprintf(stderr,"%s, %d: clear done %d %d\n",__FILE__,__LINE__,changedRowCount,IndexCommon_isIndexInUse());
      (*doneFlag) = FALSE;
    }
//fprintf(stderr,"%s, %d: %d %d\n",__FILE__,__LINE__,*doneFlag,IndexCommon_isIndexInUse());
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
                                INDEX_TYPE_SET_ANY_ENTRY,
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

void IndexCommon_initProgress(ProgressInfo *progressInfo, const char *text)
{
  if (progressInfo != NULL)
  {
    progressInfo->text                  = text;
    progressInfo->startTimestamp        = 0ll;
    progressInfo->steps                 = 0;
    progressInfo->maxSteps              = 0;
    progressInfo->lastProgressSum       = 0L;
    progressInfo->lastProgressCount     = 0;
    progressInfo->lastProgressTimestamp = 0LL;
  }
}

void IndexCommon_resetProgress(ProgressInfo *progressInfo, uint64 maxSteps)
{
  if (progressInfo != NULL)
  {
    progressInfo->startTimestamp        = Misc_getTimestamp();
    progressInfo->steps                 = 0;
    progressInfo->maxSteps              = maxSteps;
    progressInfo->lastProgressSum       = 0L;
    progressInfo->lastProgressCount     = 0;
    progressInfo->lastProgressTimestamp = 0LL;
  }
}

void IndexCommon_doneProgress(ProgressInfo *progressInfo)
{
  if (progressInfo != NULL)
  {
    UNUSED_VARIABLE(progressInfo);
  }
}

void IndexCommon_progressStep(void *userData)
{
  ProgressInfo *progressInfo = (ProgressInfo*)userData;
  uint         progress;
  uint         importLastProgress;
  uint64       now;
  uint64       elapsedTime,totalTime;
  uint64       estimatedRestTime;

  if (progressInfo != NULL)
  {
    progressInfo->steps++;

    if (progressInfo->maxSteps > 0)
    {
      progress           = (progressInfo->steps*1000)/progressInfo->maxSteps;
      importLastProgress = (progressInfo->lastProgressCount > 0) ? (uint)(progressInfo->lastProgressSum/(ulong)progressInfo->lastProgressCount) : 0;
      now                = Misc_getTimestamp();
      if (   (progress >= (importLastProgress+1))
          && (now > (progressInfo->lastProgressTimestamp+60*US_PER_SECOND))
         )
      {
        elapsedTime       = now-progressInfo->startTimestamp;
        totalTime         = (elapsedTime*progressInfo->maxSteps)/progressInfo->steps;
        estimatedRestTime = totalTime-elapsedTime;

        plogMessage(NULL,  // logHandle
                    LOG_TYPE_INDEX,
                    "INDEX",
                    "%s %0.1f%%, estimated rest time %uh:%02umin:%02us",
                    progressInfo->text,
                    (float)progress/10.0,
                    (uint)((estimatedRestTime/US_PER_SECOND)/3600LL),
                    (uint)(((estimatedRestTime/US_PER_SECOND)%3600LL)/60),
                    (uint)((estimatedRestTime/US_PER_SECOND)%60LL)
                   );
        progressInfo->lastProgressSum       += progress;
        progressInfo->lastProgressCount     += 1;
        progressInfo->lastProgressTimestamp = now;
      }
    }
  }
}

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
