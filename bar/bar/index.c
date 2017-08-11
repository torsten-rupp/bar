/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index functions
* Systems: all
*
\***********************************************************************/

#define __INDEX_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "global.h"
#include "threads.h"
#include "strings.h"
#include "database.h"
#include "files.h"
#include "filesystems.h"
#include "misc.h"
#include "errors.h"

#include "storage.h"
#include "server_io.h"
#include "index_definition.h"
#include "archive.h"
#include "bar.h"
#include "bar_global.h"

#include "index.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
LOCAL const struct
{
  const char  *name;
  IndexStates indexState;
} INDEX_STATES[] =
{
  { "NONE",             INDEX_STATE_NONE             },
  { "OK",               INDEX_STATE_OK               },
  { "CREATE",           INDEX_STATE_CREATE           },
  { "UPDATE_REQUESTED", INDEX_STATE_UPDATE_REQUESTED },
  { "UPDATE",           INDEX_STATE_UPDATE           },
  { "ERROR",            INDEX_STATE_ERROR            }
};

LOCAL const struct
{
  const char *name;
  IndexModes indexMode;
} INDEX_MODES[] =
{
  { "MANUAL", INDEX_MODE_MANUAL },
  { "AUTO",   INDEX_MODE_AUTO   },
//TODO: requried?
  { "*",      INDEX_MODE_SET_ALL }
};

LOCAL const struct
{
  const char *name;
  IndexTypes indexType;
} INDEX_TYPES[] =
{
  { "FILE",      INDEX_TYPE_FILE      },
  { "IMAGE",     INDEX_TYPE_IMAGE     },
  { "DIRECTORY", INDEX_TYPE_DIRECTORY },
  { "LINK",      INDEX_TYPE_LINK      },
  { "HARDLINK",  INDEX_TYPE_HARDLINK  },
  { "SPECIAL",   INDEX_TYPE_SPECIAL   },
};

LOCAL const struct
{
  const char            *name;
  IndexStorageSortModes sortMode;
} INDEX_STORAGE_SORT_MODES[] =
{
  { "NAME",    INDEX_STORAGE_SORT_MODE_NAME    },
  { "SIZE",    INDEX_STORAGE_SORT_MODE_SIZE    },
  { "CREATED", INDEX_STORAGE_SORT_MODE_CREATED },
  { "STATE",   INDEX_STORAGE_SORT_MODE_STATE   },
};

LOCAL const struct
{
  const char          *name;
  IndexEntrySortModes sortMode;
} INDEX_ENTRY_SORT_MODES[] =
{
  { "NAME",     INDEX_ENTRY_SORT_MODE_NAME     },
  { "TYPE",     INDEX_ENTRY_SORT_MODE_TYPE     },
  { "SIZE",     INDEX_ENTRY_SORT_MODE_SIZE     },
  { "MODIFIED", INDEX_ENTRY_SORT_MODE_MODIFIED },
};

LOCAL const char *INDEX_STORAGE_SORT_MODE_COLUMNS[] =
{
  [INDEX_STORAGE_SORT_MODE_NONE   ] = NULL,

  [INDEX_STORAGE_SORT_MODE_NAME   ] = "storage.name",
  [INDEX_STORAGE_SORT_MODE_SIZE   ] = "storage.totalEntrySize",
  [INDEX_STORAGE_SORT_MODE_CREATED] = "storage.created",
  [INDEX_STORAGE_SORT_MODE_STATE  ] = "storage.state"
};

LOCAL const char *INDEX_ENTRY_SORT_MODE_COLUMNS[] =
{
  [INDEX_ENTRY_SORT_MODE_NONE    ] = NULL,

  [INDEX_ENTRY_SORT_MODE_NAME    ] = "entries.name",
  [INDEX_ENTRY_SORT_MODE_TYPE    ] = "entries.type",
  [INDEX_ENTRY_SORT_MODE_SIZE    ] = "entries.size",
  [INDEX_ENTRY_SORT_MODE_MODIFIED] = "entries.timeModified"
};
LOCAL const char *INDEX_ENTRY_NEWEST_SORT_MODE_COLUMNS[] =
{
  [INDEX_ENTRY_SORT_MODE_NONE    ] = NULL,

  [INDEX_ENTRY_SORT_MODE_NAME    ] = "entriesNewest.name",
  [INDEX_ENTRY_SORT_MODE_TYPE    ] = "entriesNewest.type",
  [INDEX_ENTRY_SORT_MODE_SIZE    ] = "entriesNewest.size",
  [INDEX_ENTRY_SORT_MODE_MODIFIED] = "entriesNewest.timeModified"
};

// time for index clean-up [s]
#define TIME_INDEX_CLEANUP (4*S_PER_HOUR)

// sleep time [s]
#define SLEEP_TIME_INDEX_CLEANUP_THREAD 60L

// server i/o timeout
#define SERVER_IO_TIMEOUT (5LL*MS_PER_SECOND)

/***************************** Datatypes *******************************/
#define INDEX_OPEN_MODE_READ         (1 << 0)
#define INDEX_OPEN_MODE_READ_WRITE   (1 << 1)
#define INDEX_OPEN_MODE_CREATE       (1 << 2)
#define INDEX_OPEN_MODE_NO_JOURNAL   (1 << 3)
#define INDEX_OPEN_MODE_FOREIGN_KEYS (1 << 4)

/***************************** Variables *******************************/
LOCAL const char                 *indexDatabaseFileName = NULL;
LOCAL bool                       indexInitializedFlag = FALSE;
LOCAL Semaphore                  indexLock;
LOCAL uint                       indexUseCount = 0;
LOCAL IndexPauseCallbackFunction indexPauseCallbackFunction = NULL;
LOCAL void                       *indexPauseCallbackUserData;

LOCAL Semaphore                  indexThreadTrigger;
LOCAL Thread                     indexThread;    // upgrad/clean-up thread
LOCAL bool                       quitFlag;

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : INDEX_DOX
* Purpose: index block-operation
* Input  : indexHandle - index handle
*          block       - code block
* Output : result - result
* Return : -
* Notes  : -
\***********************************************************************/

#define INDEX_DOX(result,indexHandle,block) \
  do \
  { \
    Database_lock(&indexHandle->databaseHandle); \
    atomicIncrement(&indexUseCount,1); \
    result = ({ \
               auto typeof(result) __closure__(void); \
               \
               typeof(result) __closure__(void)block; __closure__; \
             })(); \
    assert(indexUseCount > 0); \
    atomicIncrement(&indexUseCount,-1); \
    Database_unlock(&indexHandle->databaseHandle); \
  } \
  while (0)

#ifndef NDEBUG
  #define createIndex(...) __createIndex(__FILE__,__LINE__, ## __VA_ARGS__)
  #define openIndex(...)   __openIndex  (__FILE__,__LINE__, ## __VA_ARGS__)
  #define closeIndex(...)  __closeIndex (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : openIndex
* Purpose: open index database
* Input  : databaseFileName - database file name NULL for "in memory"
*          indexOpenModes   - open modes; see INDEX_OPEN_MODE_...
*          timeout          - timeout [ms]
* Output : indexHandle - index handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors openIndex(IndexHandle *indexHandle,
                         const char  *databaseFileName,
                         ServerIO    *masterIO,
                         uint        indexOpenModes,
                         long        timeout
                        )
#else /* not NDEBUG */
  LOCAL Errors __openIndex(const char  *__fileName__,
                           ulong       __lineNb__,
                           IndexHandle *indexHandle,
                           const char  *databaseFileName,
                           ServerIO    *masterIO,
                           uint        indexOpenModes,
                           long        timeout
                          )
#endif /* NDEBUG */
{
  Errors error;

  assert(indexHandle != NULL);

  // init variables
  indexHandle->databaseFileName = databaseFileName;
  indexHandle->masterIO         = masterIO;
  indexHandle->upgradeError     = ERROR_NONE;
  #ifndef NDEBUG
    indexHandle->threadId = pthread_self();
  #endif /* NDEBUG */

  // open index database
  if ((indexOpenModes & INDEX_OPEN_MODE_CREATE) != 0)
  {
    // delete old file
    if (databaseFileName != NULL)
    {
      (void)File_deleteCString(databaseFileName,FALSE);
    }

    // open database
    #ifdef NDEBUG
      error = Database_open(&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE,NO_WAIT);
    #else /* not NDEBUG */
      error = __Database_open(__fileName__,__lineNb__,&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE,NO_WAIT);
    #endif /* NDEBUG */
    if (error != ERROR_NONE)
    {
      return error;
    }

    // create tables
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             INDEX_DEFINITION
                            );
    if (error != ERROR_NONE)
    {
      #ifdef NDEBUG
        Database_close(&indexHandle->databaseHandle);
      #else /* not NDEBUG */
        __Database_close(__fileName__,__lineNb__,&indexHandle->databaseHandle);
      #endif /* NDEBUG */
      return error;
    }
  }
  else
  {
    // open database
    #ifdef NDEBUG
      error = Database_open(&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE,timeout);
    #else /* not NDEBUG */
      error = __Database_open(__fileName__,__lineNb__,&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE,timeout);
    #endif /* NDEBUG */
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // disable sync, enable foreign keys
  if (   ((indexOpenModes & INDEX_OPEN_MODE_READ_WRITE) != 0)
      || ((indexOpenModes & INDEX_OPEN_MODE_NO_JOURNAL) != 0)
     )
  {
    // disable synchronous mode and journal to increase transaction speed
    (void)Database_setEnabledSync(&indexHandle->databaseHandle,FALSE);
  }
  if ((indexOpenModes & INDEX_OPEN_MODE_FOREIGN_KEYS) != 0)
  {
    // enable foreign key constrains
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : closeIndex
* Purpose: close index database
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors closeIndex(IndexHandle *indexHandle)
#else /* not NDEBUG */
  LOCAL Errors __closeIndex(const char  *__fileName__,
                            ulong       __lineNb__,
                            IndexHandle *indexHandle
                           )
#endif /* NDEBUG */
{
  assert(indexHandle != NULL);

  #ifdef NDEBUG
    Database_close(&indexHandle->databaseHandle);
  #else /* not NDEBUG */
    __Database_close(__fileName__,__lineNb__,&indexHandle->databaseHandle);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : pauseCallback
* Purpose: pause callback
* Input  : userData - user data
* Output : -
* Return : TRUE iff pause
* Notes  : -
\***********************************************************************/

LOCAL bool pauseCallback(void *userData)
{
  bool          result;
  SemaphoreLock semaphoreLock;

  UNUSED_VARIABLE(userData);

  result = FALSE;
  if (indexPauseCallbackFunction != NULL)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      result = indexPauseCallbackFunction(indexPauseCallbackUserData);
    }
  }

  return result;
}

/***********************************************************************\
* Name   : getIndexVersion
* Purpose: get index version
* Input  : databaseFileName - database file name
* Output : indexVersion - index version
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getIndexVersion(const char *databaseFileName, int64 *indexVersion)
{
  Errors         error;
  IndexHandle indexHandle;

  // open index database
  error = openIndex(&indexHandle,databaseFileName,NULL,INDEX_OPEN_MODE_READ,NO_WAIT);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get database version
  error = Database_getInteger64(&indexHandle.databaseHandle,
                                indexVersion,
                                "meta",
                                "value",
                                "WHERE name='version'"
                               );
  if (error != ERROR_NONE)
  {
    (void)closeIndex(&indexHandle);
    return error;
  }

  // close index database
  (void)closeIndex(&indexHandle);

  return ERROR_NONE;
}

// TODO
#ifndef NDEBUG
LOCAL void verify(IndexHandle *indexHandle,
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

/***********************************************************************\
* Name   : fixBrokenIds
* Purpose: fix broken ids
* Input  : indexHandle - index handle
*          tableName   - table name
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void fixBrokenIds(IndexHandle *indexHandle, const char *tableName)
{
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK(NULL,NULL),  // databaseRowFunction
                         NULL,  // changedRowCount
                         "UPDATE %s SET id=rowId WHERE id IS NULL;",
                         tableName
                        );
}

#if 0
TODO obsolete
/***********************************************************************\
* Name   : rebuildNewestInfo
* Purpose:
* Input  : indexHandle     - index handle
*          entryId         - index entry id
*          storageId       - index storage id
*          name            - name
*          size            - size or 0
*          timeLastChanged - time stamp last changed [s]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors updateNewestInfo(IndexHandle *indexHandle,
                              DatabaseId  storageId,
                              DatabaseId  entryId,
                              IndexTypes  type,
                              const char  *name,
                              uint64      size,
                              uint64      timeLastChanged
                             )
{
  Errors           error;
  DatabaseQueryHandle databaseQueryHandle;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       newestStorageId;
  DatabaseId       newestEntryId;
  uint64           newestSize;
  uint64           newestTimeLastChanged;

return ERROR_NONE;

  // get current newest entry data (if exists)
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT storageId,entryId,size,MAX(timeLastChanged) \
                              FROM entriesNewest \
                              WHERE name=%'s \
                           ",
                           name
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseQueryHandle,
                           "%lld %lld %llu %llu",
                           &newestStorageId,
                           &newestEntryId,
                           &newestSize,
                           &newestTimeLastChanged
                          )
     )
  {
    newestStorageId       = DATABASE_ID_NONE;
    newestEntryId         = DATABASE_ID_NONE;
    newestSize            = 0LL;
    newestTimeLastChanged = 0LL;
  }
  Database_finalize(&databaseQueryHandle);

  // update newest entry data
  if ((newestEntryId == DATABASE_ID_NONE) || (timeLastChanged >= newestTimeLastChanged))
  {
    // update newest info
    if (newestEntryId != DATABASE_ID_NONE)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE entriesNewest \
                                SET entryId=%lld,size=%llu,timeLastChanged=%llu \
                                WHERE id=%llu \
                               ",
                               entryId,
                               size,
                               timeLastChanged,
                               newestEntryId
                              );
    }
    else
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entriesNewest \
                                   ( \
                                    entryId, \
                                    storageId, \
                                    name, \
                                    size, \
                                    timeLastChanged \
                                   ) \
                                 VALUES \
                                   ( \
                                    %lld, \
                                    %lld, \
                                    %'s, \
                                    %llu, \
                                    %llu \
                                   )",
                               entryId,
                               storageId,
                               name,
                               size,
                               timeLastChanged
                              );
    }
    if (error != ERROR_NONE)
    {
      return error;
    }

    // update newest counter/size
    switch (type)
    {
      case INDEX_TYPE_FILE     :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest=totalEntryCountNewest-1, \
                                      totalEntrySizeNewest =totalEntrySizeNewest -%llu, \
                                      totalFileCountNewest =totalFileCountNewest -1, \
                                      totalFileSizeNewest  =totalFileSizeNewest  -%llu \
                                  WHERE id=%llu \
                                 ",
                                 newestSize,
                                 newestSize,
                                 newestStorageId
                                );
        break;
      case INDEX_TYPE_IMAGE    :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest=totalEntryCountNewest-1, \
                                      totalEntrySizeNewest =totalEntrySizeNewest -%llu, \
                                      totalImageCountNewest=totalImageCountNewest-1, \
                                      totalImageSizeNewest =totalImageSizeNewest -%llu \
                                  WHERE id=%lld \
                                 ",
                                 newestSize,
                                 newestSize,
                                 newestStorageId
                                );
        break;
      case INDEX_TYPE_DIRECTORY:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest=totalEntryCountNewest-1 \
                                  WHERE id=%lld \
                                 ",
                                 newestStorageId
                                );
        break;
      case INDEX_TYPE_LINK     :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest=totalEntryCountNewest-1 \
                                  WHERE id=%lld \
                                 ",
                                 newestStorageId
                                );
        break;
      case INDEX_TYPE_HARDLINK :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest   =totalEntryCountNewest   -1, \
                                      totalEntrySizeNewest    =totalEntrySizeNewest    -%llu, \
                                      totalHardlinkCountNewest=totalHardlinkCountNewest-1, \
                                      totalHardlinkSizeNewest =totalHardlinkSizeNewest -%llu \
                                  WHERE id=%lld \
                                 ",
                                 newestSize,
                                 newestSize,
                                 newestStorageId
                                );
        break;
      case INDEX_TYPE_SPECIAL  :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest=totalEntryCountNewest-1 \
                                  WHERE id=%lld \
                                 ",
                                 newestStorageId
                                );
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* not NDEBUG */
        break;
    }
    if (error != ERROR_NONE)
    {
      return error;
    }
//if (size > 0) fprintf(stderr,"%s, %d: sub %llu add %llu \n",__FILE__,__LINE__,newestSize,size);
    switch (type)
    {
      case INDEX_TYPE_FILE     :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest=totalEntryCountNewest+1, \
                                      totalEntrySizeNewest =totalEntrySizeNewest +%llu, \
                                      totalFileCountNewest =totalFileCountNewest +1, \
                                      totalFileSizeNewest  =totalFileSizeNewest  +%llu \
                                  WHERE id=%lld \
                                 ",
                                 size,
                                 size,
                                 storageId
                                );
        break;
      case INDEX_TYPE_IMAGE    :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest=totalEntryCountNewest+1, \
                                      totalEntrySizeNewest =totalEntrySizeNewest +%llu, \
                                      totalImageCountNewest=totalImageCountNewest+1, \
                                      totalImageSizeNewest =totalImageSizeNewest +%llu \
                                  WHERE id=%lld \
                                 ",
                                 size,
                                 size,
                                 storageId
                                );
        break;
      case INDEX_TYPE_DIRECTORY:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest=totalEntryCountNewest+1 \
                                  WHERE id=%lld \
                                 ",
                                 storageId
                                );
        break;
      case INDEX_TYPE_LINK     :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest=totalEntryCountNewest+1 \
                                  WHERE id=%lld \
                                 ",
                                 storageId
                                );
        break;
      case INDEX_TYPE_HARDLINK :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest   =totalEntryCountNewest   +1, \
                                      totalEntrySizeNewest    =totalEntrySizeNewest    +%llu, \
                                      totalHardlinkCountNewest=totalHardlinkCountNewest+1, \
                                      totalHardlinkSizeNewest =totalHardlinkSizeNewest +%llu \
                                  WHERE id=%lld \
                                 ",
                                 size,
                                 size,
                                 storageId
                                );
        break;
      case INDEX_TYPE_SPECIAL  :
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET totalEntryCountNewest=totalEntryCountNewest+1 \
                                  WHERE id=%lld \
                                 ",
                                 storageId
                                );
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* not NDEBUG */
        break;
    }
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}
#endif

#include "index_version1.c"
#include "index_version2.c"
#include "index_version3.c"
#include "index_version4.c"
#include "index_version5.c"
#include "index_version6.c"

/***********************************************************************\
* Name   : importIndex
* Purpose: upgrade and import index
* Input  : indexHandle         - index handle
*          oldDatabaseFileName - old database file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors importIndex(IndexHandle *indexHandle, ConstString oldDatabaseFileName)
{
  Errors      error;
  IndexHandle oldIndexHandle;
  int64       indexVersion;

  // open old index
  error = openIndex(&oldIndexHandle,String_cString(oldDatabaseFileName),NULL,INDEX_OPEN_MODE_READ,NO_WAIT);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get index version
  error = Database_getInteger64(&oldIndexHandle.databaseHandle,
                                &indexVersion,
                                "meta",
                                "value",
                                "WHERE name='version'"
                               );
  if (error != ERROR_NONE)
  {
    (void)closeIndex(&oldIndexHandle);
    return error;
  }

  // upgrade index structure
  switch (indexVersion)
  {
    case 1:
      error = upgradeFromVersion1(&oldIndexHandle,indexHandle);
      break;
    case 2:
      error = upgradeFromVersion2(&oldIndexHandle,indexHandle);
      break;
    case 3:
      error = upgradeFromVersion3(&oldIndexHandle,indexHandle);
      break;
    case 4:
      error = upgradeFromVersion4(&oldIndexHandle,indexHandle);
      break;
    case 5:
      error = upgradeFromVersion5(&oldIndexHandle,indexHandle);
      break;
    case 6:
      error = upgradeFromVersion6(&oldIndexHandle,indexHandle);
      break;
    default:
      // unknown version if index
      error = ERROR_DATABASE_VERSION_UNKNOWN;
      break;
  }
  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Imported index database '%s' (version %d)\n",
                String_cString(oldDatabaseFileName),
                indexVersion
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Import index database '%s' (version %d) fail: %s\n",
                String_cString(oldDatabaseFileName),
                indexVersion,
                Error_getText(error)
               );
  }

  // close old index
  (void)closeIndex(&oldIndexHandle);

  return error;
}

/***********************************************************************\
* Name   : cleanUpDuplicateMeta
* Purpose: delete duplicate meta data
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpDuplicateMeta(IndexHandle *indexHandle)
{
  String              name;
  DatabaseQueryHandle databaseQueryHandle;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  name = String_new();

  if (Database_prepare(&databaseQueryHandle,
                       &indexHandle->databaseHandle,
                       "SELECT name FROM meta GROUP BY name"
                      ) == ERROR_NONE
     )
  {
    while (Database_getNextRow(&databaseQueryHandle,
                               "%S",
                               name
                              )
          )
    {
      (void)Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM meta \
                              WHERE     name=%'S \
                                    AND (rowid NOT IN (SELECT rowid FROM meta WHERE name=%'S ORDER BY rowId DESC LIMIT 0,1)); \
                             ",
                             name,
                             name
                            );
    }
    Database_finalize(&databaseQueryHandle);
  }

  // free resources
  String_delete(name);

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Clean-up duplicate meta data\n"
             );

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpIncompleteUpdate
* Purpose: reset incomplete updated database entries
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpIncompleteUpdate(IndexHandle *indexHandle)
{
  Errors           error;
  IndexId          storageId;
  StorageSpecifier storageSpecifier;
  String           storageName,printableStorageName;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK(NULL,NULL),  // databaseRowFunction
                         NULL,  // changedRowCount
                         "UPDATE entities SET lockedCount=0"
                        );

  error = ERROR_NONE;
  while (Index_findStorageByState(indexHandle,
                                  INDEX_STATE_SET(INDEX_STATE_UPDATE),
                                  NULL,  // uuidId
                                  NULL,  // jobUUID
                                  NULL,  // entityId
                                  NULL,  // scheduleUUID
                                  &storageId,
                                  storageName,
                                  NULL,  // createdDateTime
                                  NULL,  // size
                                  NULL,  // indexMode
                                  NULL,  // lastCheckedDateTime
                                  NULL,  // errorMessage
                                  NULL,  // totalEntryCount
                                  NULL  // totalEntrySize
                                 )
         && (error == ERROR_NONE)
        )
  {
    // get printable name (if possible)
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error == ERROR_NONE)
    {
      Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);
    }
    else
    {
      String_set(printableStorageName,storageName);
    }

    error = Index_setState(indexHandle,
                           storageId,
                           INDEX_STATE_UPDATE_REQUESTED,
                           0LL,
                           NULL
                          );
    if (error == ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Requested update index #%lld: %s\n",
                  storageId,
                  String_cString(printableStorageName)
                 );
    }
  }

  // free resources
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Clean-up incomplete updates\n"
             );

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpIncompleteCreate
* Purpose: delete incomplete created database entries
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpIncompleteCreate(IndexHandle *indexHandle)
{
  Errors           error;
  IndexId          storageId;
  StorageSpecifier storageSpecifier;
  String           storageName,printableStorageName;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  error = ERROR_NONE;
  while (Index_findStorageByState(indexHandle,
                                  INDEX_STATE_SET(INDEX_STATE_CREATE),
                                  NULL,  // uuidId
                                  NULL,  // jobUUID
                                  NULL,  // entityId
                                  NULL,  // scheduleUUID
                                  &storageId,
                                  storageName,
                                  NULL,  // createdDateTime
                                  NULL,  // size
                                  NULL,  // indexMode
                                  NULL,  // lastCheckedDateTime
                                  NULL,  // errorMessage
                                  NULL,  // totalEntryCount
                                  NULL  // totalEntrySize
                                 )
         && (error == ERROR_NONE)
        )
  {
    // get printable name (if possible)
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error == ERROR_NONE)
    {
      Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);
    }
    else
    {
      String_set(printableStorageName,storageName);
    }

    error = Index_deleteStorage(indexHandle,storageId);
    if (error == ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Deleted incomplete index #%lld: '%s'\n",
                  storageId,
                  String_cString(printableStorageName)
                 );
    }
  }

  // free resources
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Clean-up incomplete created entries\n"
             );

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpOrphanedEntries
* Purpose: delete orphaned entries (entries without storage)
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpOrphanedEntries(IndexHandle *indexHandle)
{
  Errors           error;
  String           storageName;
  ulong            n;
  IndexQueryHandle indexQueryHandle;
  IndexId          entryId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // initialize variables
  storageName = String_new();

  n = 0L;

  // clean-up entries without storage name
  error = Index_beginTransaction(indexHandle,INDEX_TIMEOUT);
  if (error != ERROR_NONE)
  {
    String_delete(storageName);
    return error;
  }
  error = Index_initListEntries(&indexQueryHandle,
                                indexHandle,
                                NULL,  // indexIds
                                0,  // indexIdCount
                                NULL,  // entryIds
                                0,  // entryIdCount
                                INDEX_TYPE_SET_ANY_ENTRY,
                                NULL,  // entryPattern,
                                INDEX_ENTRY_SORT_MODE_NONE,
                                DATABASE_ORDERING_NONE,
                                FALSE,  // newestOnly
                                0LL,  // offset
                                INDEX_UNLIMITED
                               );
  if (error != ERROR_NONE)
  {
    (void)Index_rollbackTransaction(indexHandle);
    String_delete(storageName);
    return error;
  }
  while (   !quitFlag
         && Index_getNextEntry(&indexQueryHandle,
                               NULL,  // uuidId,
                               NULL,  // jobUUID,
                               NULL,  // entityId,
                               NULL,  // scheduleUUID,
                               NULL,  // archiveType,
                               NULL,  // storageId,
                               storageName,
                               NULL,  // storageDateTime,
                               &entryId,
                               NULL,  // name,
                               NULL,  // destinationName,
                               NULL,  // fileSystemType,
                               NULL,  // size,
                               NULL,  // timeModified,
                               NULL,  // userId,
                               NULL,  // groupId,
                               NULL,  // permission,
                               NULL,  // fragmentOffset,
                               NULL   // fragmentSize
                              )
        )
  {
    if (String_isEmpty(storageName))
    {
      Index_deleteEntry(indexHandle,entryId);
      n++;
    }
  }
  Index_doneList(&indexQueryHandle);
  (void)Index_endTransaction(indexHandle);

  // clean-up *Entries without entry
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK_INLINE(Errors,(uint count, const char* names[], const char* values[], void *userData),
                         {
                           DatabaseId databaseId;
                           Errors     error;

                           assert(count == 1);
                           assert(values[0] != NULL);

                           databaseId = (DatabaseId)atoll(values[0]);

                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(names);
                           UNUSED_VARIABLE(userData);

                           error = Database_execute(&indexHandle->databaseHandle,
                                                    CALLBACK(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM fileEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT fileEntries.id FROM fileEntries \
                          LEFT JOIN entries ON entries.id=fileEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK_INLINE(Errors,(uint count, const char* names[], const char* values[], void *userData),
                         {
                           DatabaseId databaseId;
                           Errors     error;

                           assert(count == 1);
                           assert(values[0] != NULL);

                           databaseId = (DatabaseId)atoll(values[0]);

                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(names);
                           UNUSED_VARIABLE(userData);

                           error = Database_execute(&indexHandle->databaseHandle,
                                                    CALLBACK(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM imageEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT imageEntries.id FROM imageEntries \
                          LEFT JOIN entries ON entries.id=imageEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK_INLINE(Errors,(uint count, const char* names[], const char* values[], void *userData),
                         {
                           DatabaseId databaseId;
                           Errors     error;

                           assert(count == 1);
                           assert(values[0] != NULL);

                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(names);
                           UNUSED_VARIABLE(userData);

                           databaseId = (DatabaseId)atoll(values[0]);

                           error = Database_execute(&indexHandle->databaseHandle,
                                                    CALLBACK(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM directoryEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT directoryEntries.id FROM directoryEntries \
                          LEFT JOIN entries ON entries.id=directoryEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK_INLINE(Errors,(uint count, const char* names[], const char* values[], void *userData),
                         {
                           DatabaseId databaseId;
                           Errors     error;

                           assert(count == 1);
                           assert(values[0] != NULL);

                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(names);
                           UNUSED_VARIABLE(userData);

                           databaseId = (DatabaseId)atoll(values[0]);

                           error = Database_execute(&indexHandle->databaseHandle,
                                                    CALLBACK(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM linkEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT linkEntries.id FROM linkEntries \
                          LEFT JOIN entries ON entries.id=linkEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK_INLINE(Errors,(uint count, const char* names[], const char* values[], void *userData),
                         {
                           DatabaseId databaseId;
                           Errors     error;

                           assert(count == 1);
                           assert(values[0] != NULL);

                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(names);
                           UNUSED_VARIABLE(userData);

                           databaseId = (DatabaseId)atoll(values[0]);

                           error = Database_execute(&indexHandle->databaseHandle,
                                                    CALLBACK(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM hardlinkEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT hardlinkEntries.id FROM hardlinkEntries \
                          LEFT JOIN entries ON entries.id=hardlinkEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK_INLINE(Errors,(uint count, const char* names[], const char* values[], void *userData),
                         {
                           DatabaseId databaseId;
                           Errors     error;

                           assert(count == 1);
                           assert(values[0] != NULL);

                           UNUSED_VARIABLE(count);
                           UNUSED_VARIABLE(names);
                           UNUSED_VARIABLE(userData);

                           databaseId = (DatabaseId)atoll(values[0]);

                           error = Database_execute(&indexHandle->databaseHandle,
                                                    CALLBACK(NULL,NULL),  // databaseRowFunction
                                                    NULL,  // changedRowCount
                                                    "DELETE FROM specialEntries WHERE id=%lld",
                                                    databaseId
                                                   );
                           if (error != ERROR_NONE)
                           {
                             return error;
                           }

                           n++;

                           return ERROR_NONE;
                         },NULL),
                         NULL,  // changedRowCount
                         "SELECT specialEntries.id FROM specialEntries \
                          LEFT JOIN entries ON entries.id=specialEntries.entryId \
                          WHERE entries.id IS NULL \
                         "
                        );

  if (n > 0L)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up %lu orphaned entries\n",
                n
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up orphaned entries\n"
               );
  }

  // free resources
  String_delete(storageName);

  return error;
}

/***********************************************************************\
* Name   : cleanUpStoragenNoName
* Purpose: delete storage entries without name
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpStorageNoName(IndexHandle *indexHandle)
{
  Errors           error;
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           printableStorageName;
  ulong            n;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  printableStorageName = String_new();

  // clean-up
  n = 0L;
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 INDEX_ID_ANY,  // entityId
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 NULL,  // indexIds
                                 0,  // storageIdCount,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 INDEX_STORAGE_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error == ERROR_NONE)
  {
    while (Index_getNextStorage(&indexQueryHandle,
                                NULL,  // uuidId
                                NULL,  // jobUUID
                                NULL,  // entityId
                                NULL,  // scheduleUUID
                                NULL,  // archiveType
                                &storageId,
                                storageName,
                                NULL,  // createdDateTime
                                NULL,  // size,
                                NULL,  // indexState
                                NULL,  // indexMode
                                NULL,  // lastCheckedDateTime
                                NULL,  // errorMessage
                                NULL,  // totalEntryCount
                                NULL  // totalEntrySize
                               )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteStorage(indexHandle,storageId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  if (n > 0L)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Cleaned %lu indizes without name\n",
                n
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up indizes without name\n"
               );
  }

  // free resource
  String_delete(printableStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpStorageNoEntity
* Purpose: assign entity to storage entries without any entity
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpStorageNoEntity(IndexHandle *indexHandle)
{
//TODO
#if 0
  Errors              error;
  String              name1,name2;
  DatabaseQueryHandle databaseQueryHandle1,databaseQueryHandle2;
  DatabaseId          storageId;
  StaticString        (uuid,MISC_UUID_STRING_LENGTH);
  uint64              createdDateTime;
  DatabaseId          entityId;
  bool                equalsFlag;
  ulong               i;
  String              oldDatabaseFileName;

  // try to set entityId in storage entries
  name1 = String_new();
  name2 = String_new();
  error = Database_prepare(&databaseQueryHandle1,
                           &indexHandle->databaseHandle,
                           "SELECT uuid, \
                                   name, \
                                   UNIXTIMESTAMP(created) \
                            FROM storage \
                            WHERE entityId=0 \
                            ORDER BY id,created ASC \
                           "
                          );
  if (error == ERROR_NONE)
  {
    while (Database_getNextRow(&databaseQueryHandle1,
                               "%S %llu",
                               uuid,
                               name1,
                               &createdDateTime
                              )
       )
    {
      // find matching entity/create default entity
      error = Database_prepare(&databaseQueryHandle2,
                               &oldIndexHandle->databaseHandle,
                               "SELECT id, \
                                       name \
                                FROM storage \
                                WHERE uuid=%'S \
                               ",
                               uuid
                              );
      if (error == ERROR_NONE)
      {
        while (Database_getNextRow(&databaseQueryHandle2,
                                   "%lld %S",
                                   &storageId,
                                   name2
                                  )
              )
        {
          // compare names (equals except digits)
          equalsFlag = String_length(name1) == String_length(name2);
          i = STRING_BEGIN;
          while (equalsFlag
                 && (i < String_length(name1))
                 && (   isdigit(String_index(name1,i))
                     || (String_index(name1,i) == String_index(name2,i))
                    )
                )
          {
            i++;
          }
          if (equalsFlag)
          {
            // assign entity id
            (void)Database_execute(&newIndexHandle->databaseHandle,
                                   CALLBACK(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "UPDATE storage \
                                    SET entityId=%lld \
                                    WHERE id=%lld; \
                                   ",
                                   entityId,
                                   storageId
                                  );
          }
        }
        Database_finalize(&databaseQueryHandle2);
      }

      error = Database_execute(&newIndexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entities \
                                  ( \
                                   jobUUID, \
                                   scheduleUUID, \
                                   created, \
                                   type, \
                                   bidFlag\
                                  ) \
                                VALUES \
                                  ( \
                                   %'S, \
                                   '', \
                                   DATETIME(%llu,'unixepoch'), \
                                   %d, \
                                   %d\
                                  ); \
                               ",
                               uuid,
                               createdDateTime,
                               ARCHIVE_TYPE_FULL,
                               0
                              );
      if (error == ERROR_NONE)
      {
        // get entity id
        entityId = Database_getLastRowId(&newIndexHandle->databaseHandle);

        // assign entity id for all storage entries with same uuid and matching name (equals except digits)
        error = Database_prepare(&databaseQueryHandle2,
                                 &oldIndexHandle->databaseHandle,
                                 "SELECT id, \
                                         name \
                                  FROM storage \
                                  WHERE uuid=%'S \
                                 ",
                                 uuid
                                );
        if (error == ERROR_NONE)
        {
          while (Database_getNextRow(&databaseQueryHandle2,
                                     "%lld %S",
                                     &storageId,
                                     name2
                                    )
                )
          {
            // compare names (equals except digits)
            equalsFlag = String_length(name1) == String_length(name2);
            i = STRING_BEGIN;
            while (equalsFlag
                   && (i < String_length(name1))
                   && (   isdigit(String_index(name1,i))
                       || (String_index(name1,i) == String_index(name2,i))
                      )
                  )
            {
              i++;
            }
            if (equalsFlag)
            {
              // assign entity id
              (void)Database_execute(&newIndexHandle->databaseHandle,
                                     CALLBACK(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storage \
                                      SET entityId=%lld \
                                      WHERE id=%lld; \
                                     ",
                                     entityId,
                                     storageId
                                    );
            }
          }
          Database_finalize(&databaseQueryHandle2);
        }
      }
    }
    Database_finalize(&databaseQueryHandle1);
  }
  String_delete(name2);
  String_delete(name1);

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Clean-up no entity-entries\n"
             );
#else
UNUSED_VARIABLE(indexHandle);
return ERROR_NONE;
#endif
}

/***********************************************************************\
* Name   : pruneStorages
* Purpose: prune all storages which are OK, but empty
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneStorages(IndexHandle *indexHandle)
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId          storageId;

  assert(indexHandle != NULL);
  assert(Database_isLocked(&indexHandle->databaseHandle));

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT id \
                            FROM storage \
                            WHERE state=%d \
                           ",
                           INDEX_STATE_OK
                          );
  if (error == ERROR_NONE)
  {
    while (   (error == ERROR_NONE)
           && Database_getNextRow(&databaseQueryHandle,
                                  "%llu",
                                  &storageId
                                 )
          )
    {
      error = Index_pruneStorage(indexHandle,INDEX_ID_STORAGE(storageId));
    }
    Database_finalize(&databaseQueryHandle);
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Pruned storages\n"
             );

  return error;
}

/***********************************************************************\
* Name   : pruneEntities
* Purpose: prune all entities which are empty
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneEntities(IndexHandle *indexHandle)
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId          entityId;

  assert(indexHandle != NULL);
  assert(Database_isLocked(&indexHandle->databaseHandle));

  // Note: keep default entity!
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT id \
                            FROM entities \
                            WHERE id!=%lld \
                           ",
                           INDEX_CONST_DEFAULT_ENTITY_ID
                          );
  if (error == ERROR_NONE)
  {
    while (   (error == ERROR_NONE)
           && Database_getNextRow(&databaseQueryHandle,
                                  "%llu",
                                  &entityId
                                 )
          )
    {
      error = Index_pruneEntity(indexHandle,INDEX_ID_ENTITY(entityId));
    }
    Database_finalize(&databaseQueryHandle);
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Pruned entities\n"
             );

  return error;
}

/***********************************************************************\
* Name   : pruneUUIDs
* Purpose: prune all UUIDs which are empty
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors pruneUUIDs(IndexHandle *indexHandle)
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId          uuidId;

  assert(indexHandle != NULL);
  assert(Database_isLocked(&indexHandle->databaseHandle));

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT id \
                            FROM uuids \
                           "
                          );
  if (error == ERROR_NONE)
  {
    while (   (error == ERROR_NONE)
           && Database_getNextRow(&databaseQueryHandle,
                                  "%llu",
                                  &uuidId
                                 )
          )
    {
      error = Index_pruneUUID(indexHandle,INDEX_ID_UUID(uuidId));
    }
    Database_finalize(&databaseQueryHandle);
  }

  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Pruned UUIDs\n"
             );

  return error;
}

/***********************************************************************\
* Name   : cleanUpDuplicateIndizes
* Purpose: delete duplicate storage entries
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpDuplicateIndizes(IndexHandle *indexHandle)
{
  Errors           error;
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           duplicateStorageName;
  String           printableStorageName;
  ulong            n;
  IndexId          storageId;
  IndexQueryHandle indexQueryHandle1,indexQueryHandle2;
  IndexId          duplicateStorageId;
  IndexId          deleteStorageId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  duplicateStorageName = String_new();
  printableStorageName = String_new();

  // get storage entry
  n = 0L;
  error = Index_initListStorages(&indexQueryHandle1,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 INDEX_ID_ANY,  // entityId
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 NULL,  // indexIds
                                 0,  // storageIdCount
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 INDEX_STORAGE_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error == ERROR_NONE)
  {
    while (   !quitFlag
           && Index_getNextStorage(&indexQueryHandle1,
                                   NULL,  // uuidId
                                   NULL,  // jobUUID
                                   NULL,  // entityId
                                   NULL,  // scheduleUUID
                                   NULL,  // archiveType
                                   &storageId,
                                   storageName,
                                   NULL,  // createdDateTime
                                   NULL,  // size,
                                   NULL,  // indexState
                                   NULL,  // indexMode
                                   NULL,  // lastCheckedDateTime
                                   NULL,  // errorMessage
                                   NULL,  // totalEntryCount
                                   NULL  // totalEntrySize
                                  )
          )
    {
      do
      {
        deleteStorageId = INDEX_ID_NONE;

        // check for duplicate entry
        error = Index_initListStorages(&indexQueryHandle2,
                                       indexHandle,
                                       INDEX_ID_ANY,  // uuidId
                                       INDEX_ID_ANY,  // entityId
                                       NULL,  // jobUUID
                                       NULL,  // scheduleUUID,
                                       NULL,  // indexIds
                                       0,  // storageIdCount
                                       INDEX_STATE_SET_ALL,
                                       INDEX_MODE_SET_ALL,
                                       NULL,  // name
                                       INDEX_STORAGE_SORT_MODE_NONE,
                                       DATABASE_ORDERING_NONE,
                                       0L,  // offset
                                       INDEX_UNLIMITED
                                      );
        if (error != ERROR_NONE)
        {
          continue;
        }
        while (   !quitFlag
               && (deleteStorageId == INDEX_ID_NONE)
               && Index_getNextStorage(&indexQueryHandle2,
                                       NULL,  // uuidId
                                       NULL,  // jobUUID
                                       NULL,  // entityId
                                       NULL,  // scheduleUUID
                                       NULL,  // archiveType
                                       &duplicateStorageId,
                                       duplicateStorageName,
                                       NULL,  // createdDateTime
                                       NULL,  // size,
                                       NULL,  // indexState
                                       NULL,  // indexMode
                                       NULL,  // lastCheckedDateTime
                                       NULL,  // errorMessage
                                       NULL,  // totalEntryCount
                                       NULL  // totalEntrySize
                                      )
              )
        {
          if (   (storageId != duplicateStorageId)
              && Storage_equalNames(storageName,duplicateStorageName)
             )
          {
            // get storage id to delete
            deleteStorageId = duplicateStorageId;

            // get printable name (if possible)
            error = Storage_parseName(&storageSpecifier,duplicateStorageName);
            if (error == ERROR_NONE)
            {
              Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);
            }
            else
            {
              String_set(printableStorageName,duplicateStorageName);
            }
          }
        }
        Index_doneList(&indexQueryHandle2);
#if 0
        // request update index
        if (deletedIndexFlag)
        {
          (void)Index_setState(indexHandle,
                               storageId,
                               INDEX_STATE_UPDATE_REQUESTED,
                               0LL,  // lastCheckedDateTime
                               NULL  // errorMessage
                              );
        }
#endif

        if (deleteStorageId != INDEX_ID_NONE)
        {
          // delete storage
          error = Index_deleteStorage(indexHandle,deleteStorageId);
          if (error == ERROR_NONE)
          {
            plogMessage(NULL,  // logHandle
                        LOG_TYPE_INDEX,
                        "INDEX",
                        "Deleted duplicate index #%lld: '%s'\n",
                        duplicateStorageId,
                        String_cString(printableStorageName)
                       );
            n++;
          }
          else
          {
            deleteStorageId = INDEX_ID_NONE;
          }
        }
      }
      while (deleteStorageId != INDEX_ID_NONE);
    }
    Index_doneList(&indexQueryHandle1);
  }
  if (n > 0L)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up %lu duplicate indizes\n",
                n
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Clean-up duplicate indizes\n"
               );
  }

  // free resources
  String_delete(printableStorageName);
  String_delete(duplicateStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : deleteFromIndex
* Purpose: delete from index with delay/check if index-usage
* Input  : indexHandle - index handle
*          tableName   - table name
*          filter      - filter
*          ...         - optional arguments for filter
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors deleteFromIndex(IndexHandle *indexHandle,
                             const char  *tableName,
                             const char  *filter,
                             ...
                            )
{
  String  filterString;
  va_list arguments;
  Errors  error;
  ulong   changedRowCount;
  bool    transactionFlag;

  // init variables
  filterString = String_new();

  // get filter
  va_start(arguments,filter);
  String_vformat(filterString,filter,arguments);
  va_end(arguments);

  changedRowCount = 0;
  transactionFlag = FALSE;
  do
  {
    // only delete if index is currently not in use
    if (indexUseCount == 0)
    {
      // begin transaction
      if (!transactionFlag)
      {
        error = Index_beginTransaction(indexHandle,1000L);
        if (error == ERROR_NONE)
        {
          transactionFlag = TRUE;
        }
      }
      else
      {
        error = ERROR_NONE;
      }

      if (error == ERROR_NONE)
      {
        // delete some entries
//fprintf(stderr,"%s, %d: deleteFromIndex %s: %s %s\n",__FILE__,__LINE__,Thread_getCurrentName(),tableName,String_cString(filterString));
        BLOCK_DOX(error,
                  Database_lock(&indexHandle->databaseHandle),
                  Database_unlock(&indexHandle->databaseHandle),
        {
          return Database_execute(&indexHandle->databaseHandle,
                                  CALLBACK(NULL,NULL),  // databaseRowFunction
                                  &changedRowCount,
                                  "DELETE FROM %s \
                                   WHERE %S \
                                   LIMIT 1000 \
                                  ",
                                  tableName,
                                  filterString
                                 );
        });
      }
//fprintf(stderr,"%s, %d: deleted from %s where filter=%s entries %lu %s\n",__FILE__,__LINE__,tableName,String_cString(filterString),changedRowCount,Error_getText(error));
    }
    else
    {
      // end transaction
      if (transactionFlag)
      {
        (void)Index_endTransaction(indexHandle);
        transactionFlag = FALSE;
      }

      // short delay
      Misc_udelay(500*US_PER_MS);
    }
  }
  while (   (changedRowCount >= 1000)
         && (error == ERROR_NONE)
        );


  // end transaction
  if (transactionFlag)
  {
    (void)Index_endTransaction(indexHandle);
    transactionFlag = FALSE;
  }

  // free resources
  String_delete(filterString);

  return error;
}

#if 0
//TODO: not used, remove
/***********************************************************************\
* Name   : refreshStoragesInfos
* Purpose: refresh storages info
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors refreshStoragesInfos(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;

  assert(indexHandle != NULL);

  // init variables

  // clean-up
  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 INDEX_ID_ANY,  // entityId
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 NULL,  // indexIds
                                 0,  // storageIdCount,
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 INDEX_STORAGE_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error == ERROR_NONE)
  {
    while (Index_getNextStorage(&indexQueryHandle,
                                NULL,  // uuidId
                                NULL,  // jobUUID
                                NULL,  // entityId
                                NULL,  // scheduleUUID
                                NULL,  // archiveType
                                &storageId,
                                NULL,  // storageName,
                                NULL,  // createdDateTime
                                NULL,  // size,
                                NULL,  // indexState
                                NULL,  // indexMode
                                NULL,  // lastCheckedDateTime
                                NULL,  // errorMessage
                                NULL,  // totalEntryCount
                                NULL  // totalEntrySize
                               )
          )
    {
      error = Index_updateStorageInfos(indexHandle,storageId);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed storages infos\n"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed storages infos fail (error: %s)\n",
                Error_getText(error)
               );
  }

  // free resource

  return error;
}

//TODO: not used, remove
/***********************************************************************\
* Name   : refreshEntitiesInfos
* Purpose: refresh entities infos
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors refreshEntitiesInfos(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;

  assert(indexHandle != NULL);

  // init variables

  // clean-up
  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 NULL,  // jobUUID,
                                 NULL,  // scheduldUUID
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error == ERROR_NONE)
  {
    while (Index_getNextEntity(&indexQueryHandle,
                               NULL,  // uuidId,
                               NULL,  // jobUUID,
                               NULL,  // scheduleUUID,
                               &entityId,
                               NULL,  // archiveType,
                               NULL,  // createdDateTime,
                               NULL,  // lastErrorMessage
                               NULL,  // totalEntryCount
                               NULL,  // totalEntrySize
                               NULL  // lockedCount
                              )
          )
    {
      error = Index_updateEntityInfos(indexHandle,entityId);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed entities infos\n"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed entities infos fail (error: %s)\n",
                Error_getText(error)
               );
  }

  // free resource

  return error;
}

//TODO: not used, remove
/***********************************************************************\
* Name   : refreshUUIDsInfos
* Purpose: refresh UUIDs infos
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors refreshUUIDsInfos(IndexHandle *indexHandle)
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          uuidId;

  assert(indexHandle != NULL);

  // init variables

  // clean-up
  error = Index_initListUUIDs(&indexQueryHandle,
                              indexHandle,
                              INDEX_STATE_SET_ALL,
                              INDEX_MODE_SET_ALL,
                              NULL,  // name
                              0LL,  // offset
                              INDEX_UNLIMITED
                             );
  if (error == ERROR_NONE)
  {
    while (Index_getNextUUID(&indexQueryHandle,
                             &uuidId,
                             NULL,  // jobUUID
                             NULL,  // lastCheckedDateTime
                             NULL,  // lastErrorMessage
                             NULL,  // totalEntryCount
                             NULL  // totalEntrySize
                            )
          )
    {
      error = Index_updateUUIDInfos(indexHandle,uuidId);
    }
    Index_doneList(&indexQueryHandle);
  }
  if (error == ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed UUID infos\n"
               );
  }
  else
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Refreshed UUID infos fail (error: %s)\n",
                Error_getText(error)
               );
  }

  // free resource

  return error;
}
#endif

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
                                0,  // indexIdCount
                                NULL,  // entryIds
                                0,  // entryIdCount
                                INDEX_TYPE_SET_ANY_ENTRY,
                                NULL,  // entryPattern,
                                INDEX_ENTRY_SORT_MODE_NONE,
                                DATABASE_ORDERING_NONE,
                                FALSE,  // newestOnly
                                0LL,  // offset
                                INDEX_UNLIMITED
                               );
  if (error != ERROR_NONE)
  {
    return error;
  }
  name = String_new();
  while (Index_getNextEntry(&indexQueryHandle,
                            NULL,  // uuidId,
                            NULL,  // jobUUID,
                            NULL,  // entityId,
                            NULL,  // scheduleUUID,
                            NULL,  // archiveType,
                            NULL,  // storageId,
                            NULL,  // storageName,
                            NULL,  // storageDateTime,
                            &entryId,
                            name,
                            NULL,  // destinationName,
                            NULL,  // fileSystemType,
                            &size,
                            &timeModified,
                            NULL,  // userId,
                            NULL,  // groupId,
                            NULL,  // permission,
                            NULL,  // fragmentOffset,
                            NULL   // fragmentSize
                           )
        )
  {
  }
  String_delete(name);
  Index_doneList(&indexQueryHandle);

  return ERROR_NONE;
}
#endif

/***********************************************************************\
* Name   : indexThreadCode
* Purpose: index upgrade and cleanup thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void indexThreadCode(void)
{
  IndexHandle         indexHandle;
  String              absoluteFileName;
  String              directoryName;
  Errors              error;
  DirectoryListHandle directoryListHandle;
  uint                i;
  String              oldDatabaseFileName;
  uint                oldDatabaseCount;
  uint64              lastCleanupTimestamp;
  SemaphoreLock       semaphoreLock;
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId          databaseId;
  String              storageName;
  uint                sleepTime;

  assert(indexDatabaseFileName != NULL);

  // open index
  error = openIndex(&indexHandle,indexDatabaseFileName,NULL,INDEX_OPEN_MODE_READ_WRITE,INDEX_TIMEOUT);
  if (error != ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_ERROR,
                "INDEX",
                "Cannot open index database '$s' fail: %s\n",
                indexDatabaseFileName,
                Error_getText(error)
               );
    return;
  }

  // get absolute database file name
  absoluteFileName = File_getAbsoluteFileNameCString(String_new(),indexDatabaseFileName);

  // open directory where database is located
  directoryName = File_getDirectoryName(String_new(),absoluteFileName);
  error = File_openDirectoryList(&directoryListHandle,directoryName);
  if (error != ERROR_NONE)
  {
    String_delete(directoryName);
    closeIndex(&indexHandle);
    plogMessage(NULL,  // logHandle
                LOG_TYPE_ERROR,
                "INDEX",
                "Import index database '%s' fail: %s\n",
                indexDatabaseFileName,
                Error_getText(error)
               );
    return;
  }
  String_delete(directoryName);

  // process all *.oldNNN files
  i                   = 0;
  oldDatabaseFileName = String_new();
  oldDatabaseCount    = 0;
  while (   !quitFlag
         && (File_readDirectoryList(&directoryListHandle,oldDatabaseFileName) == ERROR_NONE)
        )
  {
    if (   String_startsWith(oldDatabaseFileName,absoluteFileName)\
        && String_matchCString(oldDatabaseFileName,STRING_BEGIN,".*\\.old\\d\\d\\d$",NULL,NULL)
       )
    {
      if (i == 0)
      {
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_INDEX,
                    "INDEX",
                    "Started import old index databases\n"
                   );
      }
      error = importIndex(&indexHandle,oldDatabaseFileName);
      if (error == ERROR_NONE)
      {
        oldDatabaseCount++;
      }
      (void)File_delete(oldDatabaseFileName,FALSE);

      i++;
    }
  }
  if (i > 0)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done import old index databases (%d)\n",
                oldDatabaseCount
               );
  }
  String_delete(oldDatabaseFileName);

  // close directory
  File_closeDirectoryList(&directoryListHandle);

  // free resources
  String_delete(absoluteFileName);

  // single clean-ups
  if (!quitFlag)
  {
    BLOCK_DO(Database_lock(&indexHandle.databaseHandle),
             Database_unlock(&indexHandle.databaseHandle),
    {
      if (!quitFlag) (void)cleanUpStorageNoName(&indexHandle);
      if (!quitFlag) (void)cleanUpStorageNoEntity(&indexHandle);
//TODO: required here or only regulary below?
//      if (!quitFlag) (void)cleanUpOrphanedEntries(&indexHandle);
//      if (!quitFlag) (void)cleanUpDuplicateIndizes(&indexHandle);
      if (!quitFlag) (void)pruneStorages(&indexHandle);
      if (!quitFlag) (void)pruneEntities(&indexHandle);
      if (!quitFlag) (void)pruneUUIDs(&indexHandle);
    });
//TODO: too slow
//    (void)refreshStoragesInfos(&indexHandle);
    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Done initial clean-up index database\n"
               );
  }

  // index is initialized and ready to use
  indexInitializedFlag = TRUE;

  // regular clean-ups
  storageName          = String_new();
  lastCleanupTimestamp = Misc_getTimestamp();
  while (!quitFlag)
  {
    // remove deleted storages from index
    do
    {
      // find next storage to remove (Note: get single entry to remove to avoid long-running prepare!)
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle.databaseHandle,
                               "SELECT id,name FROM storage WHERE state=%d",
                               INDEX_STATE_DELETED
                              );
      if (error == ERROR_NONE)
      {
        if (!Database_getNextRow(&databaseQueryHandle,
                                 "%lld %S",
                                 &databaseId,
                                 storageName
                                )
           )
        {
          databaseId = DATABASE_ID_NONE;
        }
        Database_finalize(&databaseQueryHandle);
      }
      else
      {
        databaseId = DATABASE_ID_NONE;
      }

      // remove from database
      if (databaseId != DATABASE_ID_NONE)
      {
        if (error == ERROR_NONE)
        {
          error = deleteFromIndex(&indexHandle,
                                  "fileEntries",
                                  "storageId=%lld",
                                  databaseId
                                 );
        }
        if (error == ERROR_NONE)
        {
          error = deleteFromIndex(&indexHandle,
                                  "imageEntries",
                                  "storageId=%lld",
                                  databaseId
                                 );
        }
        if (error == ERROR_NONE)
        {
          error = deleteFromIndex(&indexHandle,
                                  "directoryEntries",
                                  "storageId=%lld",
                                  databaseId
                                 );
        }
        if (error == ERROR_NONE)
        {
          error = deleteFromIndex(&indexHandle,
                                  "linkEntries",
                                  "storageId=%lld",
                                  databaseId
                                 );
        }
        if (error == ERROR_NONE)
        {
          error = deleteFromIndex(&indexHandle,
                                  "hardlinkEntries",
                                  "storageId=%lld",
                                  databaseId
                                 );
        }
        if (error == ERROR_NONE)
        {
          error = deleteFromIndex(&indexHandle,
                                  "specialEntries",
                                  "storageId=%lld",
                                  databaseId
                                 );
        }
        if (error == ERROR_NONE)
        {
          error = deleteFromIndex(&indexHandle,
                                  "entriesNewest",
                                  "storageId=%lld",
                                  databaseId
                                 );
        }
        if (error == ERROR_NONE)
        {
          error = deleteFromIndex(&indexHandle,
                                  "entries",
                                  "storageId=%lld",
                                  databaseId
                                 );
        }
        if (error == ERROR_NONE)
        {
          error = deleteFromIndex(&indexHandle,
                                  "storage",
                                  "id=%lld",
                                  databaseId
                                 );
        }
        if (error == ERROR_NONE)
        {
          if (!String_isEmpty(storageName))
          {
            plogMessage(NULL,  // logHandle
                        LOG_TYPE_INDEX,
                        "INDEX",
                        "Removed storage #%llu from index: '%s'\n",
                        databaseId,
                        String_cString(storageName)
                       );
          }
        }
      }
    }
    while ((databaseId != DATABASE_ID_NONE) && (error == ERROR_NONE));

    if (Misc_getTimestamp() > (lastCleanupTimestamp+TIME_INDEX_CLEANUP*US_PER_SECOND))
    {
      // regular clean-up database
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Started regular clean-up index database\n"
                 );
      if (!quitFlag) (void)cleanUpOrphanedEntries(&indexHandle);
      if (!quitFlag) (void)cleanUpDuplicateIndizes(&indexHandle);
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_INDEX,
                  "INDEX",
                  "Done regular clean-up index database\n"
                 );

      lastCleanupTimestamp = Misc_getTimestamp();
    }

    // check quit flag/trigger, sleep
    sleepTime = 0;
    SEMAPHORE_LOCKED_DO(semaphoreLock,&indexThreadTrigger,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      while (   !quitFlag
             && (sleepTime < SLEEP_TIME_INDEX_CLEANUP_THREAD)
             && !Semaphore_waitModified(&indexThreadTrigger,60*MS_PER_SECOND)
        )
      {
        sleepTime += 60;
      }
    }
  }
  String_delete(storageName);

  // free resources
  closeIndex(&indexHandle);
}

/***********************************************************************\
* Name   : initIndexQueryHandle
* Purpose: init index query handle
* Input  : indexQueryHandle - index query handle
*          indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initIndexQueryHandle(IndexQueryHandle *indexQueryHandle, IndexHandle *indexHandle)
{
  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  indexQueryHandle->indexHandle = indexHandle;
}

/***********************************************************************\
* Name   : doneIndexQueryHandle
* Purpose: done index query handle
* Input  : indexQueryHandle - index query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneIndexQueryHandle(IndexQueryHandle *indexQueryHandle)
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  UNUSED_VARIABLE(indexQueryHandle);
}

/***********************************************************************\
* Name   : getIndexTypeSetString
* Purpose: get index type filter string
* Input  : string       - string variable
*          indexTypeSet - index type set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

LOCAL String getIndexTypeSetString(String string, IndexTypeSet indexTypeSet)
{
  IndexTypes indexType;

  String_clear(string);
  for (indexType = INDEX_TYPE_MIN; indexType <= INDEX_TYPE_MAX; indexType++)
  {
    if (IN_SET(indexTypeSet,indexType))
    {
      if (!String_isEmpty(string)) String_appendCString(string,",");
      String_format(string,"%d",indexType);
    }
  }

  return string;
}

/***********************************************************************\
* Name   : getIndexStateSetString
* Purpose: get index state filter string
* Input  : string        - string variable
*          indexStateSet - index state set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

LOCAL String getIndexStateSetString(String string, IndexStateSet indexStateSet)
{
  IndexStates indexState;

  String_clear(string);
  for (indexState = INDEX_STATE_MIN; indexState <= INDEX_STATE_MAX; indexState++)
  {
    if (IN_SET(indexStateSet,indexState))
    {
      if (!String_isEmpty(string)) String_appendCString(string,",");
      String_format(string,"%d",indexState);
    }
  }

  return string;
}

/***********************************************************************\
* Name   : getIndexModeSetString
* Purpose: get index mode filter string
* Input  : string       - string variable
*          indexModeSet - index mode set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

LOCAL String getIndexModeSetString(String string, IndexModeSet indexModeSet)
{
  IndexModes indexMode;

  String_clear(string);
  for (indexMode = INDEX_MODE_MIN; indexMode <= INDEX_MODE_MAX; indexMode++)
  {
    if (IN_SET(indexModeSet,indexMode))
    {
      if (!String_isEmpty(string)) String_appendCString(string,",");
      String_format(string,"%d",indexMode);
    }
  }

  return string;
}

/***********************************************************************\
* Name   : getFTSString
* Purpose: get full-text-search filter string
* Input  : string      - string variable
*          patternText - pattern text
* Output : -
* Return : string for WHERE filter-statement
* Notes  : -
\***********************************************************************/

LOCAL String getFTSString(String string, ConstString patternText)
{
  StringTokenizer stringTokenizer;
  ConstString     token;
  bool            addedPatternFlag;
  ulong           i;
  char            ch;

  String_clear(string);
  if (!String_isEmpty(patternText))
  {
    String_clear(string);
    String_appendChar(string,'\'');
    String_initTokenizer(&stringTokenizer,
                         patternText,
                         STRING_BEGIN,
                         STRING_WHITE_SPACES,
                         STRING_QUOTES,
                         TRUE
                        );
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      addedPatternFlag = FALSE;
      i                = 0;
      while (i < String_length(token))
      {
        ch = String_index(token,i);
        if (isalnum(ch))
        {
          if (addedPatternFlag)
          {
            String_appendChar(string,' ');
            addedPatternFlag = FALSE;
          }
          String_appendChar(string,ch);
        }
        else
        {
          if (!addedPatternFlag)
          {
            String_appendChar(string,'*');
            addedPatternFlag = TRUE;
          }
        }
        i++;
      }
      if (!String_isEmpty(string) && !addedPatternFlag) String_appendChar(string,'*');
    }
    String_doneTokenizer(&stringTokenizer);
    String_appendChar(string,'\'');
  }

  return string;
}

/***********************************************************************\
* Name   : getREGEXPString
* Purpose: get REGEXP filter string
* Input  : string      - string variable
*          patternText - pattern text
* Output : -
* Return : string for WHERE filter-statement
* Notes  : -
\***********************************************************************/

LOCAL String getREGEXPString(String string, ConstString patternText)
{
  StringTokenizer stringTokenizer;
  ConstString     token;
  ulong           i;
  char            ch;

  String_clear(string);
  if (!String_isEmpty(patternText))
  {
    String_clear(string);
    String_initTokenizer(&stringTokenizer,
                         patternText,
                         STRING_BEGIN,
                         STRING_WHITE_SPACES,
                         STRING_QUOTES,
                         TRUE
                        );
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      String_appendChar(string,'\'');
      i = 0;
      while (i < String_length(token))
      {
        ch = String_index(token,i);
        switch (ch)
        {
          case '.':
          case '[':
          case ']':
          case '(':
          case ')':
          case '{':
          case '}':
          case '+':
          case '|':
          case '^':
          case '$':
          case '\\':
            String_appendChar(string,'\\');
            String_appendChar(string,ch);
            i++;
            break;
          case '*':
            String_appendCString(string,".*");
            i++;
            break;
          case '?':
            String_appendChar(string,'.');
            i++;
            break;
          case '\'':
            String_appendCString(string,"''");
            i++;
            break;
          default:
            String_appendChar(string,ch);
            i++;
            break;
        }
      }
      String_appendChar(string,'\'');
    }
    String_doneTokenizer(&stringTokenizer);
  }

  return string;
}

/***********************************************************************\
* Name   : filterAppend
* Purpose: append to SQL filter string
* Input  : filterString - filter string
*          condition    - append iff true
*          concatenator - concatenator string
*          format       - format string (printf-style)
*          ...          - optional arguments for format
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void filterAppend(String filterString, bool condition, const char *concatenator, const char *format, ...)
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
    String_vformat(filterString,format,arguments);
    va_end(arguments);
  }
}

/***********************************************************************\
* Name   : appendOrdering
* Purpose: append to SQL ordering string
* Input  : filterString - filter string
*          condition    - append iff true
*          columnName   - column name
*          ordering     - database ordering
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendOrdering(String orderString, bool condition, const char *columnName, DatabaseOrdering ordering)
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

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : updateDirectoryContentAggregates
* Purpose: update directory content count/size
* Input  : indexHandle - index handle
*          storageId   - index storage id
*          entryId     - index entry id
*          fileName    - file name
*          size        - size
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors updateDirectoryContentAggregates(IndexHandle *indexHandle,
                                              IndexId     storageId,
                                              IndexId     entryId,
                                              ConstString fileName,
                                              uint64      size
                                             )
{
  DatabaseId databaseId;
  String     directoryName;
  Errors     error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(Index_getType(entryId) == INDEX_TYPE_ENTRY);
  assert(fileName != NULL);

  // get newest id
  error = Database_getId(&indexHandle->databaseHandle,
                         &databaseId,
                         "entriesNewest",
                         "WHERE entryId=%llu",
                         Index_getDatabaseId(entryId)
                        );
  if (error != ERROR_NONE)
  {
    return error;
  }

  directoryName = File_getDirectoryName(String_new(),fileName);
  error = ERROR_NONE;
  while ((error == ERROR_NONE) && !String_isEmpty(directoryName))
  {
//fprintf(stderr,"%s, %d: path=%s %llu\n",__FILE__,__LINE__,String_cString(path),size);
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE directoryEntries \
                              SET totalEntryCount=totalEntryCount+1, \
                                  totalEntrySize =totalEntrySize +%llu \
                              WHERE storageId=%llu \
                                AND name=%'S \
                             ",
                             size,
                             Index_getDatabaseId(storageId),
                             directoryName
                            );
    if (error != ERROR_NONE)
    {
      break;
    }

    if (databaseId != DATABASE_ID_NONE)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE directoryEntries \
                                SET totalEntryCountNewest=totalEntryCountNewest+1, \
                                    totalEntrySizeNewest =totalEntrySizeNewest +%llu \
                                WHERE storageId=%llu \
                                  AND name=%'S \
                               ",
                               size,
                               Index_getDatabaseId(storageId),
                               directoryName
                              );
      if (error != ERROR_NONE)
      {
        break;
      }
    }

    File_getDirectoryName(directoryName,directoryName);
  }
  String_delete(directoryName);

  return ERROR_NONE;
}

// ----------------------------------------------------------------------

/***********************************************************************\
* Name   : assignStorageToStorage
* Purpose: assign storage entries to other storage
* Input  : indexHandle - index handle
*          storageId   - storage id
*          toStorageId - to storage id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignStorageToStorage(IndexHandle *indexHandle,
                                    IndexId     storageId,
                                    IndexId     toStorageId
                                   )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(Index_getType(toStorageId) == INDEX_TYPE_STORAGE);
  assert(Database_isLocked(&indexHandle->databaseHandle));

  // assign storage entries to other storage
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "UPDATE entries \
                            SET storageId=%lld \
                            WHERE storageId=%lld; \
                           ",
                           Index_getDatabaseId(toStorageId),
                           Index_getDatabaseId(storageId)
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "UPDATE entriesNewest \
                            SET storageId=%lld \
                            WHERE storageId=%lld; \
                           ",
                           Index_getDatabaseId(toStorageId),
                           Index_getDatabaseId(storageId)
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // delete storage if empty
  error = Index_pruneStorage(indexHandle,storageId);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignStorageToEntity
* Purpose: assign storage entries to other entity
* Input  : indexHandle - index handle
*          storageId   - storage id
*          toEntityId  - to entity id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignStorageToEntity(IndexHandle *indexHandle,
                                   IndexId     storageId,
                                   IndexId     toEntityId
                                  )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(Index_getType(toEntityId) == INDEX_TYPE_ENTITY);
  assert(Database_isLocked(&indexHandle->databaseHandle));

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "UPDATE storage \
                            SET entityId=%lld \
                            WHERE id=%lld; \
                           ",
                           Index_getDatabaseId(toEntityId),
                           Index_getDatabaseId(storageId)
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : assignEntityToStorage
* Purpose: assign all storage entries of entity to other storage
* Input  : indexHandle - index handle
*          storageId   - storage id
*          toStorageId - to storage id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToStorage(IndexHandle *indexHandle,
                                   IndexId     entityId,
                                   IndexId     toStorageId
                                  )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);
  assert(Index_getType(toStorageId) == INDEX_TYPE_STORAGE);
  assert(Database_isLocked(&indexHandle->databaseHandle));

  error = Index_initListStorages(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 entityId,
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID,
                                 NULL,  // indexIds
                                 0,  // storageIdCount
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 INDEX_STORAGE_SORT_MODE_NONE,
                                 DATABASE_ORDERING_NONE,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextStorage(&indexQueryHandle,
                              NULL,  // uuidId
                              NULL,  // jobUUID
                              NULL,  // entityId
                              NULL,  // scheduleUUID
                              NULL,  // archiveType
                              &storageId,
                              NULL,  // storageName
                              NULL,  // createdDateTime
                              NULL,  // size,
                              NULL,  // indexState
                              NULL,  // indexMode
                              NULL,  // lastCheckedDateTime
                              NULL,  // errorMessage
                              NULL,  // totalEntryCount
                              NULL  // totalEntrySize
                             )
        )
  {
    // assign storage entries to other storage
    error = assignStorageToStorage(indexHandle,storageId,toStorageId);
    if (error != ERROR_NONE)
    {
      Index_doneList(&indexQueryHandle);
      return error;
    }

    // delete storage if empty
    error = Index_pruneStorage(indexHandle,storageId);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  Index_doneList(&indexQueryHandle);

  // delete entity if empty
  error = Index_pruneEntity(indexHandle,entityId);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignEntityToEntity
* Purpose: assign all storage entries of entity to other entity
* Input  : indexHandle   - index handle
*          entityId      - entity id
*          toEntityId    - to entity id
*          toArchiveType - archive type or ARCHIVE_TYPE_NONE
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToEntity(IndexHandle  *indexHandle,
                                  IndexId      entityId,
                                  IndexId      toEntityId,
                                  ArchiveTypes toArchiveType
                                 )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);
  assert((toEntityId == INDEX_ID_NONE) || Index_getType(toEntityId) == INDEX_TYPE_ENTITY);
  assert(Database_isLocked(&indexHandle->databaseHandle));

  // assign to entity
  if (entityId != toEntityId)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE storage \
                              SET entityId=%lld \
                              WHERE entityId=%lld; \
                             ",
                             Index_getDatabaseId(toEntityId),
                             Index_getDatabaseId(entityId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

//TODO
    // delete uuid if empty
    error = Index_pruneEntity(indexHandle,entityId);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  // set entity type
  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entities \
                              SET type=%d \
                              WHERE id=%lld; \
                             ",
                             toArchiveType,
                             Index_getDatabaseId(toEntityId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignEntityToJob
* Purpose: assign entity to other job
* Input  : indexHandle   - index handle
*          entityId      - entity id
*          toJobUUID     - to job UUID
*          toArchiveType - archive type or ARCHIVE_TYPE_NONE
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignEntityToJob(IndexHandle  *indexHandle,
                               IndexId      entityId,
                               ConstString  toJobUUID,
                               ArchiveTypes toArchiveType
                              )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);
  assert(toJobUUID != NULL);
  assert(Database_isLocked(&indexHandle->databaseHandle));

  // assign to job
  if (toJobUUID != NULL)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entities \
                              SET jobUUID=%'S \
                              WHERE id=%lld; \
                             ",
                             toJobUUID,
                             Index_getDatabaseId(entityId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

//TODO
    // delete uuid if empty
  }

  // set entity type
  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entities \
                              SET type=%d \
                              WHERE id=%lld; \
                             ",
                             toArchiveType,
                             Index_getDatabaseId(entityId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : assignJobToStorage
* Purpose: assign all storage entries of all entities of job to other
*          storage
* Input  : indexHandle - index handle
*          storageId   - storage id
*          toStorageId - to storage id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignJobToStorage(IndexHandle *indexHandle,
                                ConstString jobUUID,
                                IndexId     toStorageId
                               )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;

  assert(indexHandle != NULL);
  assert(Index_getType(toStorageId) == INDEX_TYPE_STORAGE);
  assert(Database_isLocked(&indexHandle->databaseHandle));

  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 jobUUID,
                                 NULL, // scheduldUUID
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextEntity(&indexQueryHandle,
                             NULL,  // uuidId,
                             NULL,  // jobUUID,
                             NULL,  // scheduleUUID,
                             &entityId,
                             NULL,  // archiveType,
                             NULL,  // createdDateTime,
                             NULL,  // lastErrorMessage
                             NULL,  // totalEntryCount
                             NULL,  // totalEntrySize
                             NULL  // lockedCount
                            )
        )
  {
    // assign all storage entries of entity to other storage
    error = assignEntityToStorage(indexHandle,entityId,toStorageId);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  Index_doneList(&indexQueryHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignJobToEntity
* Purpose: assign all entities of job to other entity
* Input  : indexHandle   - index handle
*          jobUUID       - job UUID
*          toEntityId    - to entity id
*          toArchiveType - archive type or ARCHIVE_TYPE_NONE
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignJobToEntity(IndexHandle  *indexHandle,
                               ConstString  jobUUID,
                               IndexId      toEntityId,
                               ArchiveTypes toArchiveType
                              )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;

  assert(indexHandle != NULL);
  assert(Index_getType(toEntityId) == INDEX_TYPE_ENTITY);
  assert(Database_isLocked(&indexHandle->databaseHandle));

  error = Index_initListEntities(&indexQueryHandle,
                                 indexHandle,
                                 INDEX_ID_ANY,  // uuidId
                                 jobUUID,
                                 NULL, // scheduldUUID
                                 INDEX_STATE_SET_ALL,
                                 INDEX_MODE_SET_ALL,
                                 NULL,  // name
                                 DATABASE_ORDERING_ASCENDING,
                                 0LL,  // offset
                                 INDEX_UNLIMITED
                                );
  if (error != ERROR_NONE)
  {
    return error;
  }
  while (Index_getNextEntity(&indexQueryHandle,
                             NULL,  // uuidId,
                             NULL,  // jobUUID,
                             NULL,  // scheduleUUID,
                             &entityId,
                             NULL,  // archiveType,
                             NULL,  // createdDateTime,
                             NULL,  // lastErrorMessage
                             NULL,  // totalEntryCount
                             NULL,  // totalEntrySize
                             NULL  // lockedCount
                            )
        )
  {
    // assign all storage entries of entity to other entity
    error = assignEntityToEntity(indexHandle,entityId,toEntityId,toArchiveType);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  Index_doneList(&indexQueryHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : assignJobToJob
* Purpose: assign all entities of job to other job
*          entity
* Input  : indexHandle - index handle
*          jobUUID     - job UUID
*          toJobUUID   - to job UUID
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors assignJobToJob(IndexHandle  *indexHandle,
                            ConstString  jobUUID,
                            ConstString  toJobUUID,
                            ArchiveTypes toArchiveType
                           )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(!String_isEmpty(toJobUUID));
  assert(Database_isLocked(&indexHandle->databaseHandle));

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),  // databaseRowFunction
                           NULL,  // changedRowCount
                           "UPDATE entities \
                            SET jobUUID=%'S \
                            WHERE jobUUID=%'S; \
                           ",
                           toJobUUID,
                           jobUUID
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  // set entity type
  if (toArchiveType != ARCHIVE_TYPE_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entities \
                              SET type=%d \
                              WHERE jobUUID=%'S; \
                             ",
                             toArchiveType,
                             jobUUID
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Index_initAll(void)
{
  Errors error;

  // init variables
  Semaphore_init(&indexLock);

  // init database
  error = Database_initAll();
  if (error != ERROR_NONE)
  {
    Semaphore_done(&indexLock);
    return error;
  }

  return ERROR_NONE;
}

void Index_doneAll(void)
{
  Semaphore_done(&indexLock);
}

const char *Index_stateToString(IndexStates indexState, const char *defaultValue)
{
  uint       i;
  const char *name;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_STATES))
         && (INDEX_STATES[i].indexState != indexState)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_STATES))
  {
    name = INDEX_STATES[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseState(const char *name, IndexStates *indexState, void *userData)
{
  uint i;

  assert(name != NULL);
  assert(indexState != NULL);

  UNUSED_VARIABLE(userData);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_STATES))
         && !stringEqualsIgnoreCase(INDEX_STATES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_STATES))
  {
    (*indexState) = INDEX_STATES[i].indexState;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Index_modeToString(IndexModes indexMode, const char *defaultValue)
{
  uint       i;
  const char *name;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_MODES))
         && (INDEX_MODES[i].indexMode != indexMode)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_MODES))
  {
    name = INDEX_MODES[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseMode(const char *name, IndexModes *indexMode, void *userData)
{
  uint i;

  assert(name != NULL);
  assert(indexMode != NULL);

  UNUSED_VARIABLE(userData);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_MODES))
         && !stringEqualsIgnoreCase(INDEX_MODES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_MODES))
  {
    (*indexMode) = INDEX_MODES[i].indexMode;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Index_parseType(const char *name, IndexTypes *indexType)
{
  uint i;

  assert(name != NULL);
  assert(indexType != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_TYPES))
         && !stringEqualsIgnoreCase(INDEX_TYPES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_TYPES))
  {
    (*indexType) = INDEX_TYPES[i].indexType;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Index_parseStorageSortMode(const char *name, IndexStorageSortModes *indexStorageSortMode, void *userData)
{
  uint i;

  assert(name != NULL);
  assert(indexStorageSortMode != NULL);

  UNUSED_VARIABLE(userData);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_STORAGE_SORT_MODES))
         && !stringEqualsIgnoreCase(INDEX_STORAGE_SORT_MODES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_STORAGE_SORT_MODES))
  {
    (*indexStorageSortMode) = INDEX_STORAGE_SORT_MODES[i].sortMode;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Index_parseEntrySortMode(const char *name, IndexEntrySortModes *indexEntrySortMode)
{
  uint i;

  assert(name != NULL);
  assert(indexEntrySortMode != NULL);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(INDEX_ENTRY_SORT_MODES))
         && !stringEqualsIgnoreCase(INDEX_ENTRY_SORT_MODES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(INDEX_ENTRY_SORT_MODES))
  {
    (*indexEntrySortMode) = INDEX_ENTRY_SORT_MODES[i].sortMode;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool Index_parseOrdering(const char *name, DatabaseOrdering *databaseOrdering, void *userData)
{
  assert(name != NULL);
  assert(databaseOrdering != NULL);

  UNUSED_VARIABLE(userData);

  if      (stringEqualsIgnoreCase("ASCENDING",name))
  {
    (*databaseOrdering) = DATABASE_ORDERING_ASCENDING;
    return TRUE;
  }
  else if (stringEqualsIgnoreCase("DESCENDING",name))
  {
    (*databaseOrdering) = DATABASE_ORDERING_DESCENDING;
    return TRUE;
  }
  else if (stringEqualsIgnoreCase("NONE",name))
  {
    (*databaseOrdering) = DATABASE_ORDERING_NONE;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

Errors Index_init(const char *fileName)
{
  bool        createFlag;
  Errors      error;
  int64       indexVersion;
  String      oldDatabaseFileName;
  uint        n;
  IndexHandle indexHandleReference,indexHandle;

  assert(fileName != NULL);

  // init variables

  // get database file name
  indexDatabaseFileName = strdup(fileName);
  if (indexDatabaseFileName == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  createFlag = FALSE;

  // check if index exists, check version
  if (!createFlag)
  {
    if (File_existsCString(indexDatabaseFileName))
    {
      // check index version
      error = getIndexVersion(indexDatabaseFileName,&indexVersion);
      if (error == ERROR_NONE)
      {
        if (indexVersion < INDEX_VERSION)
        {
          // rename existing index for upgrade
          oldDatabaseFileName = String_new();
          n = 0;
          do
          {
            oldDatabaseFileName = String_newCString(indexDatabaseFileName);
            String_appendCString(oldDatabaseFileName,".old");
            String_format(oldDatabaseFileName,"%03d",n);
            n++;
          }
          while (File_exists(oldDatabaseFileName));
          (void)File_renameCString(indexDatabaseFileName,
                                   String_cString(oldDatabaseFileName),
                                   NULL
                                  );
          String_delete(oldDatabaseFileName);

          // upgrade version -> create new
          createFlag = TRUE;
          plogMessage(NULL,  // logHandle
                      LOG_TYPE_ERROR,
                      "INDEX",
                      "Old index database version %d in '%s' - create new\n",
                      indexVersion,
                      indexDatabaseFileName
                     );
        }
      }
      else
      {
        // unknown version -> create new
        createFlag = TRUE;
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Unknown index database version in '%s' - create new\n",
                    indexDatabaseFileName
                   );
      }
    }
    else
    {
      // does not exists -> create new
      createFlag = TRUE;
    }
  }

  if (!createFlag)
  {
    // check if database is corrupt
    if (File_existsCString(indexDatabaseFileName))
    {
      error = openIndex(&indexHandleReference,NULL,NULL,INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_CREATE,NO_WAIT);
      if (error == ERROR_NONE)
      {
        error = openIndex(&indexHandle,indexDatabaseFileName,NULL,INDEX_OPEN_MODE_READ,NO_WAIT);
        if (error == ERROR_NONE)
        {
          error = Database_compare(&indexHandleReference.databaseHandle,&indexHandle.databaseHandle);
          closeIndex(&indexHandle);
        }
        closeIndex(&indexHandleReference);
      }
      if (error != ERROR_NONE)
      {
        // rename existing index for upgrade
        oldDatabaseFileName = String_new();
        n = 0;
        do
        {
          oldDatabaseFileName = String_newCString(indexDatabaseFileName);
          String_appendCString(oldDatabaseFileName,".old");
          String_format(oldDatabaseFileName,"%03d",n);
          n++;
        }
        while (File_exists(oldDatabaseFileName));
        (void)File_renameCString(indexDatabaseFileName,
                                 String_cString(oldDatabaseFileName),
                                 NULL
                                );
        String_delete(oldDatabaseFileName);

        // corrupt -> create new
        createFlag = TRUE;
        plogMessage(NULL,  // logHandle
                    LOG_TYPE_ERROR,
                    "INDEX",
                    "Outdated or corrupt index database '%s' (error: %s) - create new\n",
                    indexDatabaseFileName,
                    Error_getText(error)
                   );
      }
    }
  }

  if (createFlag)
  {
    // create new index
    error = openIndex(&indexHandle,indexDatabaseFileName,NULL,INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_CREATE,NO_WAIT);
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_ERROR,
                  "INDEX",
                  "Create new index database '%s' fail: %s\n",
                  indexDatabaseFileName,
                  Error_getText(error)
                 );
      return error;
    }
    closeIndex(&indexHandle);

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Created new index database '%s' (version %d)\n",
                indexDatabaseFileName,
                INDEX_VERSION
               );
  }
  else
  {
    // get index version
    error = getIndexVersion(indexDatabaseFileName,&indexVersion);
    if (error != ERROR_NONE)
    {
      plogMessage(NULL,  // logHandle
                  LOG_TYPE_ERROR,
                  "INDEX",
                  "Cannot get index database version from '%s': %s\n",
                  indexDatabaseFileName,
                  Error_getText(error)
                 );
      return error;
    }

    plogMessage(NULL,  // logHandle
                LOG_TYPE_INDEX,
                "INDEX",
                "Opened index database '%s' (version %d)\n",
                indexDatabaseFileName,
                indexVersion
               );
  }

  // initial clean-up
  error = openIndex(&indexHandle,indexDatabaseFileName,NULL,INDEX_OPEN_MODE_READ_WRITE,NO_WAIT);
  if (error != ERROR_NONE)
  {
    plogMessage(NULL,  // logHandle
                LOG_TYPE_ERROR,
                "INDEX",
                "Cannot get index database version from '%s': %s\n",
                indexDatabaseFileName,
                Error_getText(error)
               );
    return error;
  }
  plogMessage(NULL,  // logHandle
              LOG_TYPE_INDEX,
              "INDEX",
              "Started initial clean-up index database\n"
             );
  BLOCK_DO(Database_lock(&indexHandle.databaseHandle),
           Database_unlock(&indexHandle.databaseHandle),
  {
    (void)cleanUpDuplicateMeta(&indexHandle);
    (void)cleanUpIncompleteUpdate(&indexHandle);
    (void)cleanUpIncompleteCreate(&indexHandle);
  });
  closeIndex(&indexHandle);

  // start clean-up thread
  quitFlag = FALSE;
  Semaphore_init(&indexThreadTrigger);
  if (!Thread_init(&indexThread,"Index",0,indexThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize index thread!");
  }

  return ERROR_NONE;
}

void Index_done(void)
{
  // stop threads
  quitFlag = TRUE;
  Thread_join(&indexThread);

  // free resources
  Thread_done(&indexThread);
  Semaphore_done(&indexThreadTrigger);
  free((char*)indexDatabaseFileName);
}

bool Index_isAvailable(void)
{
  return indexDatabaseFileName != NULL;
}

bool Index_isInitialized(void)
{
  return indexInitializedFlag;
}

void Index_setPauseCallback(IndexPauseCallbackFunction pauseCallbackFunction,
                            void                       *pauseCallbackUserData
                           )
{
  SemaphoreLock semaphoreLock;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    indexPauseCallbackFunction = pauseCallbackFunction;
    indexPauseCallbackUserData = pauseCallbackUserData;
  }
}

#ifdef NDEBUG
IndexHandle *Index_open(ServerIO *masterIO,
                        long     timeout
                       )
#else /* not NDEBUG */
IndexHandle *__Index_open(const char *__fileName__,
                          ulong      __lineNb__,
                          ServerIO   *masterIO,
                          long       timeout
                         )
#endif /* NDEBUG */
{
  IndexHandle *indexHandle;
  Errors      error;

  indexHandle = NULL;

  if (Index_isAvailable())
  {
    indexHandle = (IndexHandle*)malloc(sizeof(IndexHandle));
    if (indexHandle == NULL)
    {
      return NULL;
    }

    #ifdef NDEBUG
      error = openIndex(indexHandle,
                        indexDatabaseFileName,
                        masterIO,
                        INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_FOREIGN_KEYS,
                        timeout
                       );
    #else /* not NDEBUG */
      error = __openIndex(__fileName__,
                          __lineNb__,
                          indexHandle,
                          indexDatabaseFileName,
                          masterIO,
                          INDEX_OPEN_MODE_READ_WRITE|INDEX_OPEN_MODE_FOREIGN_KEYS,
                          timeout
                         );
    #endif /* NDEBUG */
    if (error != ERROR_NONE)
    {
      free(indexHandle);
      return NULL;
    }

    #ifdef NDEBUG
      DEBUG_ADD_RESOURCE_TRACE(indexHandle,sizeof(IndexHandle));
    #else /* not NDEBUG */
      DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,indexHandle,sizeof(IndexHandle));
    #endif /* NDEBUG */
  }

  return indexHandle;
}

void Index_close(IndexHandle *indexHandle)
{
  if (indexHandle != NULL)
  {
    DEBUG_REMOVE_RESOURCE_TRACE(indexHandle,sizeof(IndexHandle));

    closeIndex(indexHandle);
    free(indexHandle);
  }
}

void Index_interrupt(IndexHandle *indexHandle)
{
  assert(indexHandle != NULL);

  Database_interrupt(&indexHandle->databaseHandle);
}

#ifdef NDEBUG
Errors Index_beginTransaction(IndexHandle *indexHandle, ulong timeout)
#else /* not NDEBUG */
Errors __Index_beginTransaction(const char  *__fileName__,
                                ulong       __lineNb__,
                                IndexHandle *indexHandle,
                                ulong       timeout
                               )
#endif /* NDEBUG */
{
  assert(indexHandle != NULL);

  UNUSED_VARIABLE(timeout);

  #ifdef NDEBUG
    return Database_beginTransaction(&indexHandle->databaseHandle);
  #else /* not NDEBUG */
    return __Database_beginTransaction(__fileName__,__lineNb__,&indexHandle->databaseHandle);
  #endif /* NDEBUG */
}

Errors Index_endTransaction(IndexHandle *indexHandle)
{
  assert(indexHandle != NULL);

  return Database_endTransaction(&indexHandle->databaseHandle);
}

Errors Index_rollbackTransaction(IndexHandle *indexHandle)
{
  assert(indexHandle != NULL);

  return Database_rollbackTransaction(&indexHandle->databaseHandle);
}

Errors Index_flush(IndexHandle *indexHandle)
{
  assert(indexHandle != NULL);

  return Database_flush(&indexHandle->databaseHandle);
}

bool Index_containsType(const IndexId indexIds[],
                        uint          indexIdCount,
                        IndexTypes    indexType
                       )
{
  uint i;

  assert(indexIds != NULL);

  for (i = 0; i < indexIdCount; i++)
  {
    if (Index_getType(indexIds[i]) == indexType)
    {
      return TRUE;
    }
  }

  return FALSE;
}

bool Index_findUUID(IndexHandle  *indexHandle,
                    ConstString  findJobUUID,
                    ConstString  findScheduleUUID,
                    IndexId      *uuidId,
                    uint64       *lastExecutedDateTime,
                    String       lastErrorMessage,
                    ulong        *executionCount,
                    uint64       *averageDurationNormal,
                    uint64       *averageDurationFull,
                    uint64       *averageDurationIncremental,
                    uint64       *averageDurationDifferential,
                    uint64       *averageDurationContinuous,
                    ulong        *totalEntityCount,
                    ulong        *totalStorageCount,
                    uint64       *totalStorageSize,
                    ulong        *totalEntryCount,
                    uint64       *totalEntrySize
                   )
{
  String              filterString;
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  bool                result;
  DatabaseId          uuidId_;
  StringMap           resultMap;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  // init variables
  filterString = String_newCString("1");

//TODO get errorMessage
  if (indexHandle->masterIO == NULL)
  {
    // filters
    filterAppend(filterString,!String_isEmpty(findJobUUID),"AND","uuids.jobUUID=%'S",findJobUUID);
    filterAppend(filterString,!String_isEmpty(findScheduleUUID),"AND","entities.scheduleUUID=%'S",findScheduleUUID);

    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT uuids.id, \
                                       (SELECT UNIXTIMESTAMP(storage.created) FROM entities LEFT JOIN storage ON storage.entityId=entities.id WHERE entities.jobUUID=uuids.jobUUID ORDER BY storage.created DESC LIMIT 0,1), \
                                       (SELECT storage.errorMessage FROM entities LEFT JOIN storage ON storage.entityId=entities.id WHERE entities.jobUUID=uuids.jobUUID ORDER BY storage.created DESC LIMIT 0,1), \
                                       (SELECT COUNT(history.id) FROM history WHERE history.jobUUID=uuids.jobUUID), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%d), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%d), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%d), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%d), \
                                       (SELECT AVG(history.duration) FROM history WHERE history.jobUUID=uuids.jobUUID AND IFNULL(history.errorMessage,'')='' AND history.type=%d), \
                                       COUNT(entities.id), \
                                       COUNT(storage.id), \
                                       TOTAL(storage.size), \
                                       TOTAL(storage.totalEntryCount) , \
                                       TOTAL(storage.totalEntrySize) \
                                FROM uuids \
                                  LEFT JOIN entities ON entities.jobUUID=uuids.jobUUID \
                                  LEFT JOIN storage ON storage.entityId=entities.id \
                                WHERE %S \
                                GROUP BY uuids.id \
                               ",
                               ARCHIVE_TYPE_NORMAL,
                               ARCHIVE_TYPE_FULL,
                               ARCHIVE_TYPE_INCREMENTAL,
                               ARCHIVE_TYPE_DIFFERENTIAL,
                               ARCHIVE_TYPE_CONTINUOUS,
                               filterString
                              );
//Database_debugPrintQueryInfo(&databaseQueryHandle);
      if (error != ERROR_NONE)
      {
        return error;
      }

      result = Database_getNextRow(&databaseQueryHandle,
                                   "%lld %lld %S %lu %llu %llu %llu %llu %llu %lu %lu %llu %lu %llu",
                                   &uuidId_,
                                   lastExecutedDateTime,
                                   lastErrorMessage,
                                   executionCount,
                                   averageDurationNormal,
                                   averageDurationFull,
                                   averageDurationIncremental,
                                   averageDurationDifferential,
                                   averageDurationContinuous,
                                   totalEntityCount,
                                   totalStorageCount,
                                   totalStorageSize,
                                   totalEntryCount,
                                   totalEntrySize
                                  );

      Database_finalize(&databaseQueryHandle);

      return ERROR_NONE;
    });

    if (uuidId != NULL) (*uuidId) = INDEX_ID_UUID(uuidId_);

    String_delete(filterString);
  }
  else
  {
    resultMap        = StringMap_new();
    lastErrorMessage = String_new();

    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    resultMap,
                                    "INDEX_FIND_UUID jobUUID=%S scheduleUUID=%s",
                                    findJobUUID,
                                    (findScheduleUUID != NULL) ? String_cString(findScheduleUUID) : ""
                                   );
    if (error == ERROR_NONE)
    {
      StringMap_getInt64 (resultMap,"uuidId",uuidId,INDEX_ID_NONE);
      if ((*uuidId) != INDEX_ID_NONE)
      {
        if (lastExecutedDateTime        != NULL) StringMap_getUInt64(resultMap,"lastExecutedDateTime",       lastExecutedDateTime,       0LL );
        if (lastErrorMessage            != NULL) StringMap_getString(resultMap,"lastErrorMessage",           lastErrorMessage,           NULL);
        if (executionCount              != NULL) StringMap_getULong (resultMap,"executionCount",             executionCount,             0L  );
        if (averageDurationNormal       != NULL) StringMap_getUInt64(resultMap,"averageDurationNormal",      averageDurationNormal,      0LL );
        if (averageDurationFull         != NULL) StringMap_getUInt64(resultMap,"averageDurationFull",        averageDurationFull,        0LL );
        if (averageDurationIncremental  != NULL) StringMap_getUInt64(resultMap,"averageDurationIncremental", averageDurationIncremental, 0LL );
        if (averageDurationDifferential != NULL) StringMap_getUInt64(resultMap,"averageDurationDifferential",averageDurationDifferential,0LL );
        if (averageDurationContinuous   != NULL) StringMap_getUInt64(resultMap,"averageDurationContinuous",  averageDurationContinuous,  0LL );
        if (totalEntityCount            != NULL) StringMap_getULong (resultMap,"totalEntityCount",           totalEntityCount,           0L  );
        if (totalStorageCount           != NULL) StringMap_getULong (resultMap,"totalStorageCount",          totalStorageCount,          0L  );
        if (totalStorageSize            != NULL) StringMap_getUInt64(resultMap,"totalStorageSize",           totalStorageSize,           0LL );
        if (totalEntryCount             != NULL) StringMap_getULong (resultMap,"totalEntryCount",            totalEntryCount,            0L  );
        if (totalEntrySize              != NULL) StringMap_getUInt64(resultMap,"totalEntrySize",             totalEntrySize,             0LL );

        result = TRUE;
      }
      else
      {
        result = FALSE;
      }
    }

    String_delete(lastErrorMessage);
    StringMap_delete(resultMap);
  }
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  return result;
}

bool Index_findEntity(IndexHandle  *indexHandle,
                      ConstString  findJobUUID,
                      ConstString  findScheduleUUID,
                      ArchiveTypes findArchiveType,
                      uint64       findCreatedDateTime,
                      IndexId      *uuidId,
                      IndexId      *entityId,
                      ArchiveTypes *archiveType,
                      uint64       *createdDateTime,
                      String       lastErrorMessage,
                      ulong        *totalEntryCount,
                      uint64       *totalEntrySize
                     )
{
  String              filterString;
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  bool                result;
  DatabaseId          uuidId_,entityId_;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  // init variables
  filterString = String_newCString("1");

  // get filters
  filterAppend(filterString,!String_isEmpty(findJobUUID),"AND","jobUUID=%'S",findJobUUID);
  filterAppend(filterString,!String_isEmpty(findScheduleUUID),"AND","scheduleUUID=%'S",findScheduleUUID);
  filterAppend(filterString,findArchiveType != ARCHIVE_TYPE_NONE,"AND","entities.type=%u",findArchiveType);
  filterAppend(filterString,findCreatedDateTime != 0LL,"AND","entities.created=%llu",findCreatedDateTime);

//TODO get errorMessage
  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT IFNULL(uuids.id,0), \
                                     entities.id, \
                                     UNIXTIMESTAMP(entities.created), \
                                     entities.type, \
                                     '', \
                                     TOTAL(storage.totalEntryCount), \
                                     TOTAL(storage.totalEntrySize) \
                              FROM entities \
                                LEFT JOIN storage ON storage.entityId=entities.id \
                                LEFT JOIN uuids ON uuids.jobUUID=entityId.jobUUID \
                              WHERE %S \
                              GROUP BY entities.id \
                              LIMIT 0,1 \
                             ",
                             filterString
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    result = Database_getNextRow(&databaseQueryHandle,
                                 "%lld %lld %lld %d %S %lu %llu",
                                 &uuidId_,
                                 &entityId_,
                                 createdDateTime,
                                 archiveType,
                                 lastErrorMessage,
                                 totalEntryCount,
                                 totalEntrySize
                                );

    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(filterString);
    return FALSE;
  }

  if (uuidId != NULL) (*uuidId) = INDEX_ID_UUID(uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_ENTITY(entityId_);

  // free resources
  String_delete(filterString);

  return result;
}

bool Index_findStorageById(IndexHandle *indexHandle,
                           IndexId     findStorageId,
                           String      jobUUID,
                           String      scheduleUUID,
                           IndexId     *uuidId,
                           IndexId     *entityId,
                           String      storageName,
                           uint64      *createdDateTime,
                           uint64      *size,
                           IndexStates *indexState,
                           IndexModes  *indexMode,
                           uint64      *lastCheckedDateTime,
                           String      errorMessage,
                           ulong       *totalEntryCount,
                           uint64      *totalEntrySize
                          )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  bool                result;
  DatabaseId          uuidId_,entityId_;

  assert(indexHandle != NULL);
  assert(Index_getType(findStorageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT IFNULL(uuids.id,0), \
                                     entities.jobUUID, \
                                     IFNULL(entities.id,0), \
                                     entities.scheduleUUID, \
                                     storage.name, \
                                     UNIXTIMESTAMP(storage.created), \
                                     storage.size, \
                                     storage.state, \
                                     storage.mode, \
                                     UNIXTIMESTAMP(storage.lastChecked), \
                                     storage.errorMessage, \
                                     storage.totalEntryCount, \
                                     storage.totalEntrySize \
                              FROM storage \
                                LEFT JOIN entities ON storage.entityId=entities.id \
                                LEFT JOIN uuids ON entities.jobUUID=uuids.jobUUID \
                              WHERE storage.id=%lld \
                              GROUP BY storage.id \
                              LIMIT 0,1 \
                             ",
                             Index_getDatabaseId(findStorageId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
//Database_debugPrintQueryInfo(&databaseQueryHandle);

    result = Database_getNextRow(&databaseQueryHandle,
                                 "%lld %S %lld %S %S %llu %llu %d %d %llu %S %llu %llu",
                                 &uuidId_,
                                 jobUUID,
                                 &entityId_,
                                 scheduleUUID,
                                 storageName,
                                 createdDateTime,
                                 size,
                                 indexState,
                                 indexMode,
                                 lastCheckedDateTime,
                                 errorMessage,
                                 totalEntryCount,
                                 totalEntrySize
                                );

    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  if (uuidId != NULL) (*uuidId) = INDEX_ID_UUID(uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_ENTITY(entityId_);

  return result;
}

bool Index_findStorageByName(IndexHandle            *indexHandle,
                             const StorageSpecifier *findStorageSpecifier,
                             ConstString            findArchiveName,
                             IndexId                *uuidId,
                             IndexId                *entityId,
                             String                 jobUUID,
                             String                 scheduleUUID,
                             IndexId                *storageId,
                             uint64                 *createdDateTime,
                             uint64                 *size,
                             IndexStates            *indexState,
                             IndexModes             *indexMode,
                             uint64                 *lastCheckedDateTime,
                             String                 errorMessage,
                             ulong                  *totalEntryCount,
                             uint64                 *totalEntrySize
                            )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  String              storageName;
  StorageSpecifier    storageSpecifier;
  bool                foundFlag;
  DatabaseId          uuidId_,entityId_,storageId_;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  (*storageId) = INDEX_ID_NONE;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT IFNULL(uuids.id,0), \
                                     entities.jobUUID, \
                                     IFNULL(entities.id,0), \
                                     storage.id, \
                                     entities.scheduleUUID, \
                                     storage.name, \
                                     UNIXTIMESTAMP(storage.created), \
                                     storage.size, \
                                     storage.state, \
                                     storage.mode, \
                                     UNIXTIMESTAMP(storage.lastChecked), \
                                     storage.errorMessage, \
                                     storage.totalEntryCount, \
                                     storage.totalEntrySize \
                              FROM storage \
                                LEFT JOIN entities ON storage.entityId=entities.id \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE storage.state!=%d \
                              GROUP BY storage.id \
                             ",
                             INDEX_STATE_DELETED
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    storageName = String_new();
    Storage_initSpecifier(&storageSpecifier);
    foundFlag   = FALSE;
    while (   !foundFlag
           && Database_getNextRow(&databaseQueryHandle,
                                  "%lld %S %lld %lld %S %S %llu %llu %d %d %llu %S %llu %llu",
                                  &uuidId_,
                                  jobUUID,
                                  &entityId_,
                                  &storageId_,
                                  scheduleUUID,
                                  storageName,
                                  createdDateTime,
                                  size,
                                  indexState,
                                  indexMode,
                                  lastCheckedDateTime,
                                  errorMessage,
                                  totalEntryCount,
                                  totalEntrySize
                                 )
          )
    {
      if (Storage_parseName(&storageSpecifier,storageName) == ERROR_NONE)
      {
        foundFlag = Storage_equalSpecifiers(findStorageSpecifier,findArchiveName,
                                            &storageSpecifier,NULL
                                           );
        if (foundFlag)
        {
          if (uuidId != NULL) (*uuidId) = INDEX_ID_UUID(uuidId_);
          if (entityId != NULL) (*entityId) = INDEX_ID_ENTITY(entityId_);
          if (storageId != NULL) (*storageId) = INDEX_ID_STORAGE(storageId_);
        }
      }
    }
    Storage_doneSpecifier(&storageSpecifier);
    String_delete(storageName);

    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  return foundFlag;
}

bool Index_findStorageByState(IndexHandle   *indexHandle,
                              IndexStateSet findIndexStateSet,
                              IndexId       *uuidId,
                              String        jobUUID,
                              IndexId       *entityId,
                              String        scheduleUUID,
                              IndexId       *storageId,
                              String        storageName,
                              uint64        *createdDateTime,
                              uint64        *size,
                              IndexModes    *indexMode,
                              uint64        *lastCheckedDateTime,
                              String        errorMessage,
                              ulong         *totalEntryCount,
                              uint64        *totalEntrySize
                             )
{
  Errors              error;
  String              indexStateSetString;
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId          uuidId_,entityId_,storageId_;
  bool                result;

  assert(indexHandle != NULL);
  assert(storageId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  // init variables
  indexStateSetString = String_new();

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT IFNULL(uuids.id,0), \
                                     entities.jobUUID, \
                                     IFNULL(entities.id,0), \
                                     storage.id, \
                                     entities.scheduleUUID, \
                                     storage.name, \
                                     UNIXTIMESTAMP(storage.created), \
                                     storage.size, \
                                     storage.mode, \
                                     UNIXTIMESTAMP(storage.lastChecked), \
                                     storage.errorMessage, \
                                     storage.totalEntryCount, \
                                     storage.totalEntrySize \
                              FROM storage \
                                LEFT JOIN entities ON storage.entityId=entities.id \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE (storage.state IN (%S)) \
                              LIMIT 0,1 \
                             ",
                             getIndexStateSetString(indexStateSetString,findIndexStateSet)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    result = Database_getNextRow(&databaseQueryHandle,
                                 "%lld %S %lld %lld %S %S %llu %llu %d %llu %S %llu %llu",
                                 &uuidId_,
                                 jobUUID,
                                 &entityId_,
                                 &storageId_,
                                 scheduleUUID,
                                 storageName,
                                 createdDateTime,
                                 size,
                                 indexMode,
                                 lastCheckedDateTime,
                                 errorMessage,
                                 totalEntryCount,
                                 totalEntrySize
                                );

    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(indexStateSetString);
    return FALSE;
  }

  if (uuidId != NULL) (*uuidId) = INDEX_ID_UUID(uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_ENTITY(entityId_);
  if (storageId != NULL) (*storageId) = INDEX_ID_STORAGE(storageId_);

  // free resources
  String_delete(indexStateSetString);

  return result;
}

Errors Index_getState(IndexHandle *indexHandle,
                      IndexId     storageId,
                      IndexStates *indexState,
                      uint64      *lastCheckedDateTime,
                      String      errorMessage
                     )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT state, \
                                     UNIXTIMESTAMP(lastChecked), \
                                     errorMessage \
                              FROM storage \
                              WHERE id=%lld \
                             ",
                             Index_getDatabaseId(storageId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    if (!Database_getNextRow(&databaseQueryHandle,
                             "%d %llu %S",
                             indexState,
                             lastCheckedDateTime,
                             errorMessage
                            )
       )
    {
      (*indexState) = INDEX_STATE_UNKNOWN;
      if (errorMessage != NULL) String_clear(errorMessage);
    }

    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_setState(IndexHandle *indexHandle,
                      IndexId     indexId,
                      IndexStates indexState,
                      uint64      lastCheckedDateTime,
                      const char  *errorMessage,
                      ...
                     )
{
  Errors  error;
  va_list arguments;
  String  errorText;

  assert(indexHandle != NULL);
  assert((Index_getType(indexId) == INDEX_TYPE_ENTITY) || (Index_getType(indexId) == INDEX_TYPE_STORAGE));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // format error message (if any)
  if (errorMessage != NULL)
  {
    va_start(arguments,errorMessage);
    errorText = String_vformat(String_new(),errorMessage,arguments);
    va_end(arguments);
  }
  else
  {
    errorText = NULL;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      switch (Index_getType(indexId))
      {
        case INDEX_TYPE_ENTITY:
          error = Database_execute(&indexHandle->databaseHandle,
                                   CALLBACK(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "UPDATE storage \
                                    SET state       =%d, \
                                        errorMessage=NULL \
                                    WHERE entityId=%lld; \
                                   ",
                                   indexState,
                                   Index_getDatabaseId(indexId)
                                  );
          if (error != ERROR_NONE)
          {
            return error;
          }

          if (lastCheckedDateTime != 0LL)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storage \
                                      SET lastChecked=DATETIME(%llu,'unixepoch') \
                                      WHERE entityId=%lld; \
                                     ",
                                     lastCheckedDateTime,
                                     Index_getDatabaseId(indexId)
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          if (errorText != NULL)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storage \
                                      SET errorMessage=%'S \
                                      WHERE entityId=%lld; \
                                     ",
                                     errorText,
                                     Index_getDatabaseId(indexId)
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
          break;
        case INDEX_TYPE_STORAGE:
          error = Database_execute(&indexHandle->databaseHandle,
                                   CALLBACK(NULL,NULL),  // databaseRowFunction
                                   NULL,  // changedRowCount
                                   "UPDATE storage \
                                    SET state       =%d, \
                                        errorMessage=NULL \
                                    WHERE id=%lld; \
                                   ",
                                   indexState,
                                   Index_getDatabaseId(indexId)
                                  );
          if (error != ERROR_NONE)
          {
            return error;
          }

          if (lastCheckedDateTime != 0LL)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storage \
                                      SET lastChecked=DATETIME(%llu,'unixepoch') \
                                      WHERE id=%lld; \
                                     ",
                                     lastCheckedDateTime,
                                     Index_getDatabaseId(indexId)
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }

          if (errorText != NULL)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "UPDATE storage \
                                      SET errorMessage=%'S \
                                      WHERE id=%lld; \
                                     ",
                                     errorText,
                                     Index_getDatabaseId(indexId)
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* not NDEBUG */
          break;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    NULL,  // resultMap
                                    "INDEX_SET_STATE indexId=%lld indexState=%s lastCheckedDateTime=%llu errorMessage=%'S",
                                    indexId,
                                    Index_stateToString(indexState,NULL),
                                    lastCheckedDateTime,
                                    errorText
                                   );
  }
  if (error != ERROR_NONE)
  {
    if (errorMessage != NULL) String_delete(errorText);
    return error;
  }

  // free resources
  if (errorMessage != NULL) String_delete(errorText);

  return ERROR_NONE;
}

long Index_countState(IndexHandle *indexHandle,
                      IndexStates indexState
                     )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  long                count;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return 0L;
  }

  INDEX_DOX(count,
            indexHandle,
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT COUNT(id) \
                              FROM storage \
                              WHERE state=%d \
                             ",
                             indexState
                            );
    if (error != ERROR_NONE)
    {
      return -1L;
    }

    if (!Database_getNextRow(&databaseQueryHandle,
                             "%ld",
                             &count
                            )
       )
    {
      Database_finalize(&databaseQueryHandle);
      return -1L;
    }

    Database_finalize(&databaseQueryHandle);

    return count;
  });

  return count;
}

Errors Index_initListHistory(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             IndexId          uuidId,
                             ConstString      jobUUID,
                             DatabaseOrdering ordering,
                             uint64           offset,
                             uint64           limit
                            )
{
  String filterString;
  String orderString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  filterString = String_newCString("1");
  orderString  = String_new();

  // get filters
  filterAppend(filterString,(uuidId != INDEX_ID_ANY),"AND","uuids.id=%lld",uuidId);
  filterAppend(filterString,!String_isEmpty(jobUUID),"AND","history.jobUUID=%'S",jobUUID);

  // get ordering
  appendOrdering(orderString,TRUE,"history.created",ordering);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT history.id, \
                                   IFNULL(uuids.id,0), \
                                   history.jobUUID, \
                                   history.scheduleUUID, \
                                   history.hostName, \
                                   history.type, \
                                   UNIXTIMESTAMP(history.created), \
                                   history.errorMessage, \
                                   history.duration, \
                                   history.totalEntryCount, \
                                   history.totalEntrySize, \
                                   history.skippedEntryCount, \
                                   history.skippedEntrySize, \
                                   history.errorEntryCount, \
                                   history.errorEntrySize \
                            FROM history \
                              LEFT JOIN uuids ON uuids.jobUUID=history.jobUUID \
                            WHERE %S \
                            %S \
                            LIMIT %llu,%llu \
                           ",
                           filterString,
                           orderString,
                           offset,
                           limit
                          );
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    Database_unlock(&indexHandle->databaseHandle);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextHistory(IndexQueryHandle *indexQueryHandle,
                          IndexId          *historyId,
                          IndexId          *uuidId,
                          String           jobUUID,
                          String           scheduleUUID,
                          String           hostName,
                          ArchiveTypes     *archiveType,
                          uint64           *createdDateTime,
                          String           errorMessage,
                          uint64           *duration,
                          ulong            *totalEntryCount,
                          uint64           *totalEntrySize,
                          ulong            *skippedEntryCount,
                          uint64           *skippedEntrySize,
                          ulong            *errorEntryCount,
                          uint64           *errorEntrySize
                         )
{
  DatabaseId historyId_;
  DatabaseId uuidId_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return indexQueryHandle->indexHandle->upgradeError;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %lld %S %S %S %u %llu %S %llu %lu %llu %lu %llu %lu %llu",
                           &historyId_,
                           &uuidId_,
                           jobUUID,
                           scheduleUUID,
                           hostName,
                           archiveType,
                           createdDateTime,
                           errorMessage,
                           duration,
                           totalEntryCount,
                           totalEntrySize,
                           skippedEntryCount,
                           skippedEntrySize,
                           errorEntryCount,
                           errorEntrySize
                          )
     )
  {
    return FALSE;
  }
  if (historyId != NULL) (*historyId) = INDEX_ID_HISTORY(historyId_);
  if (uuidId != NULL) (*uuidId) = INDEX_ID_UUID(uuidId_);

  return TRUE;
}

Errors Index_newHistory(IndexHandle  *indexHandle,
                        ConstString  jobUUID,
                        ConstString  scheduleUUID,
                        ConstString  hostName,
                        ArchiveTypes archiveType,
                        uint64       createdDateTime,
                        const char   *errorMessage,
                        uint64       duration,
                        ulong        totalEntryCount,
                        uint64       totalEntrySize,
                        ulong        skippedEntryCount,
                        uint64       skippedEntrySize,
                        ulong        errorEntryCount,
                        uint64       errorEntrySize,
                        IndexId      *historyId
                       )
{
  Errors    error;
  StringMap resultMap;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      Errors error;

      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                              "INSERT INTO history \
                                 ( \
                                  jobUUID, \
                                  scheduleUUID, \
                                  hostName, \
                                  type, \
                                  created, \
                                  errorMessage, \
                                  duration, \
                                  totalEntryCount, \
                                  totalEntrySize, \
                                  skippedEntryCount, \
                                  skippedEntrySize, \
                                  errorEntryCount, \
                                  errorEntrySize \
                                 ) \
                               VALUES \
                                 ( \
                                  %'S, \
                                  %'S, \
                                  %'S, \
                                  %d, \
                                  %llu, \
                                  %'s, \
                                  %llu, \
                                  %lu, \
                                  %llu, \
                                  %lu, \
                                  %llu, \
                                  %lu, \
                                  %llu \
                                 ); \
                              ",
                              jobUUID,
                              scheduleUUID,
                              hostName,
                              archiveType,
                              createdDateTime,
                              errorMessage,
                              duration,
                              totalEntryCount,
                              totalEntrySize,
                              skippedEntryCount,
                              skippedEntrySize,
                              errorEntryCount,
                              errorEntrySize
                             );
      if (error != ERROR_NONE)
      {
        return error;
      }

      if (historyId != NULL) (*historyId) = INDEX_ID_HISTORY(Database_getLastRowId(&indexHandle->databaseHandle));

      return ERROR_NONE;
    });
  }
  else
  {
    resultMap = StringMap_new();

    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    resultMap,
                                    "INDEX_NEW_HISTORY jobUUID=%S scheduleUUID=%s hostName=%'S archiveType=%s createdDateTime=%llu errorMessage=%'s duration=%llu totalEntryCount=%lu totalEntrySize=%llu skippedEntryCount=%lu skippedEntrySize=%llu errorEntryCount=%lu errorEntrySize=%llu",
                                    jobUUID,
                                    (scheduleUUID != NULL) ? String_cString(scheduleUUID) : "",
                                    hostName,
                                    Archive_archiveTypeToString(archiveType,NULL),
                                    createdDateTime,
                                    errorMessage,
                                    duration,
                                    totalEntryCount,
                                    totalEntrySize,
                                    skippedEntryCount,
                                    skippedEntrySize,
                                    errorEntryCount,
                                    errorEntrySize
                                   );
    if (error == ERROR_NONE)
    {
      if (historyId != NULL)
      {
        if (!StringMap_getInt64(resultMap,"historyId",historyId,INDEX_ID_NONE))
        {
          error = ERROR_EXPECTED_PARAMETER;
        }
      }
    }

    StringMap_delete(resultMap);
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_deleteHistory(IndexHandle *indexHandle,
                           IndexId     historyId
                          )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // delete history entry
  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM history WHERE id=%lld;",
                             Index_getDatabaseId(historyId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_getUUIDsInfos(IndexHandle   *indexHandle,
                           IndexId       uuidId,
//TODO: remove?
                           ConstString   jobUUID,
                           ConstString   scheduleUUID,
                           ConstString   name,
                           uint64        *lastExecutedDateTime,
                           ulong         *entityCount,
                           ulong         *totalEntryCount,
                           uint64        *totalEntrySize
                          )
{
  String              ftsName;
  String              regexpName;
  String              filterString;
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  double              totalEntryCount_,totalEntrySize_;

  assert(indexHandle != NULL);
  assert((uuidId == INDEX_ID_ANY) || (Index_getType(uuidId) == INDEX_TYPE_UUID));

  // init variables

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  regexpName   = String_new();
  filterString = String_newCString("1");

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  filterAppend(filterString,(uuidId != INDEX_ID_ANY),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,!String_isEmpty(jobUUID),"AND","uuids.jobUUID='%S'",jobUUID);
  filterAppend(filterString,!String_isEmpty(scheduleUUID),"AND","entities.scheduleUUID='%S'",scheduleUUID);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","uuids.id IN (SELECT uuidId FROM FTS_uuids WHERE FTS_uuids MATCH %S)",ftsName);
//  filterAppend(filterString,!String_isEmpty(regexpName),"AND","REGEXP(%S,0,uuids.name)",regexpName);

  INDEX_DOX(error,
            indexHandle,
  {
    // get storage count, entry count, entry size
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT MAX(UNIXTIMESTAMP(entities.created)), \
                                     COUNT(entities.id),\
                                     TOTAL(storage.totalEntryCount), \
                                     TOTAL(storage.totalEntrySize) \
                              FROM uuids \
                                LEFT JOIN entities ON entities.jobUUID=uuids.jobUUID \
                                LEFT JOIN storage ON storage.entityId=entities.id \
                              WHERE     %S \
                                    AND storage.state!=%d \
                             ",
                             filterString,
                             INDEX_STATE_DELETED
                            );
//Database_debugPrintQueryInfo(&databaseQueryHandle);
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (Database_getNextRow(&databaseQueryHandle,
                            "%llu %lu %lf %lf",
                            lastExecutedDateTime,
                            entityCount,
                            &totalEntryCount_,
                            &totalEntrySize_
                           )
          )
    {
      assert(totalEntryCount_ >= 0.0);
      assert(totalEntrySize_ >= 0.0);
      if (totalEntryCount != NULL) (*totalEntryCount) = (totalEntryCount_ >= 0.0) ? (ulong)totalEntryCount_ : 0L;
      if (totalEntrySize != NULL) (*totalEntrySize) = (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
    }
    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(filterString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(regexpName);
  String_delete(ftsName);

  return ERROR_NONE;
}

//TODO: remove
Errors Index_updateUUIDInfos(IndexHandle *indexHandle,
                             IndexId     uuidId
                            )
{
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(uuidId);
return ERROR_STILL_NOT_IMPLEMENTED;
}

Errors Index_initListUUIDs(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           IndexStateSet    indexStateSet,
                           IndexModeSet     indexModeSet,
                           ConstString      name,
                           uint64           offset,
                           uint64           limit
                          )
{
  String ftsName;
  String regexpName;
  String filterString;
  String string;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  regexpName   = String_new();
  filterString = String_newCString("1");

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get filters
  string = String_new();
  filterAppend(filterString,!String_isEmpty(name),"AND","storage.id IN (SELECT storageId FROM FTS_storage WHERE FTS_storage MATCH %S)",ftsName);
  filterAppend(filterString,TRUE,"AND","storage.state IN (%S)",getIndexStateSetString(string,indexStateSet));
  filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storage.mode IN (%S)",getIndexModeSetString(string,indexModeSet));
  String_delete(string);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT uuids.id, \
                                   uuids.jobUUID, \
                                   (SELECT MAX(UNIXTIMESTAMP(entities.created)) FROM entities WHERE entities.jobUUID=uuids.jobUUID), \
                                   (SELECT storage.errorMessage FROM entities LEFT JOIN storage ON storage.entityId=entities.id WHERE entities.jobUUID=uuids.jobUUID ORDER BY storage.created DESC LIMIT 0,1), \
                                   TOTAL(storage.totalEntryCount), \
                                   TOTAL(storage.totalEntrySize) \
                            FROM uuids \
                              LEFT JOIN entities ON entities.jobUUID=uuids.jobUUID \
                              LEFT JOIN storage ON storage.entityId=entities.id \
                            WHERE %S \
                            GROUP BY uuids.id \
                            LIMIT %llu,%llu \
                           ",
                           filterString,
                           offset,
                           limit
                          );
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextUUID(IndexQueryHandle *indexQueryHandle,
                       IndexId          *uuidId,
                       String           jobUUID,
                       uint64           *lastExecutedDateTime,
                       String           lastErrorMessage,
                       ulong            *totalEntryCount,
                       uint64           *totalEntrySize
                      )
{
  DatabaseId databaseId;
  double     totalEntryCount_;
  double     totalEntrySize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %lf %lf",
                           &databaseId,
                           jobUUID,
                           lastExecutedDateTime,
                           lastErrorMessage,
                           &totalEntryCount_,
                           &totalEntrySize_
                          )
     )
  {
    return FALSE;
  }
  if (totalEntryCount != NULL) (*totalEntryCount) = (ulong)totalEntryCount_;
  if (totalEntrySize  != NULL) (*totalEntrySize ) = (uint64)totalEntrySize_;
  if (uuidId          != NULL) (*uuidId         ) = INDEX_ID_UUID(databaseId);

  return TRUE;
}

Errors Index_newUUID(IndexHandle *indexHandle,
                     ConstString jobUUID,
                     IndexId     *uuidId
                    )
{
  Errors    error;
  StringMap resultMap;

  assert(indexHandle != NULL);
  assert(uuidId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO uuids \
                                  ( \
                                   jobUUID \
                                  ) \
                                VALUES \
                                  ( \
                                   %'S \
                                  ); \
                               ",
                               jobUUID
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      (*uuidId) = INDEX_ID_UUID(Database_getLastRowId(&indexHandle->databaseHandle));

      return ERROR_NONE;
    });
  }
  else
  {
    resultMap = StringMap_new();

    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    resultMap,
                                    "INDEX_NEW_UUID jobUUID=%S",
                                    jobUUID
                                   );
    if (error == ERROR_NONE)
    {
      if (!StringMap_getInt64(resultMap,"uuidId",uuidId,INDEX_ID_NONE))
      {
        StringMap_delete(resultMap);
        error = ERROR_EXPECTED_PARAMETER;
      }
    }

    StringMap_delete(resultMap);
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_deleteUUID(IndexHandle *indexHandle,
                        IndexId     uuidId
                       )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  IndexId             entityId;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    // delete entities
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT entities.id \
                              FROM entities \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE uuids.id=%lld \
                             ",
                             Index_getDatabaseId(uuidId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    while (   Database_getNextRow(&databaseQueryHandle,
                                  "%lld",
                                  &entityId
                                 )
           && (error == ERROR_NONE)
          )
    {
      error = Index_deleteEntity(indexHandle,INDEX_ID_ENTITY(entityId));
    }
    Database_finalize(&databaseQueryHandle);
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    // delete UUID
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM uuids \
                              WHERE id=%lld; \
                             ",
                             Index_getDatabaseId(uuidId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_isEmptyUUID(IndexHandle *indexHandle,
                         IndexId     uuidId
                        )
{
  bool emptyFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(uuidId) == INDEX_TYPE_UUID);

  INDEX_DOX(emptyFlag,
            indexHandle,
  {
    return !Database_exists(&indexHandle->databaseHandle,
                            "entities",
                            "entities.id",
                            "LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID WHERE uuids.id=%lld",
                            Index_getDatabaseId(uuidId)
                           );
  });

  return emptyFlag;
}

Errors Index_getEntitiesInfos(IndexHandle   *indexHandle,
                              IndexId       uuidId,
                              IndexId       entityId,
//TODO: remove?
                              ConstString   jobUUID,
                              const IndexId indexIds[],
                              uint          indexIdCount,
                              ConstString   name,
                              ulong         *storageCount,
                              ulong         *totalEntryCount,
                              uint64        *totalEntrySize
                             )
{
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(uuidId);
UNUSED_VARIABLE(entityId);
UNUSED_VARIABLE(jobUUID);
UNUSED_VARIABLE(indexIds);
UNUSED_VARIABLE(indexIdCount);
UNUSED_VARIABLE(name);
UNUSED_VARIABLE(storageCount);
UNUSED_VARIABLE(totalEntryCount);
UNUSED_VARIABLE(totalEntrySize);

return ERROR_STILL_NOT_IMPLEMENTED;
}

//TODO: remove
Errors Index_updateEntityInfos(IndexHandle *indexHandle,
                               IndexId     entityId
                              )
{
UNUSED_VARIABLE(indexHandle);
UNUSED_VARIABLE(entityId);

return ERROR_STILL_NOT_IMPLEMENTED;
}

Errors Index_initListEntities(IndexQueryHandle *indexQueryHandle,
                              IndexHandle      *indexHandle,
                              IndexId          uuidId,
                              ConstString      jobUUID,
                              ConstString      scheduleUUID,
                              IndexStateSet    indexStateSet,
                              IndexModeSet     indexModeSet,
                              ConstString      name,
                              DatabaseOrdering ordering,
                              ulong            offset,
                              uint64           limit
                             )
{
  String ftsName;
  String regexpName;
  String filterString;
  String string;
  String orderString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((uuidId == INDEX_ID_ANY) || (Index_getType(uuidId) == INDEX_TYPE_UUID));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  regexpName   = String_new();
  filterString = String_newCString("1");
  orderString  = String_new();

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get filters
  string = String_new();
  filterAppend(filterString,(uuidId != INDEX_ID_ANY),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,!String_isEmpty(jobUUID),"AND","entities.jobUUID=%'S",jobUUID);
  filterAppend(filterString,!String_isEmpty(scheduleUUID),"AND","entities.scheduleUUID=%'S",scheduleUUID);
  filterAppend(filterString,!String_isEmpty(name),"AND","EXISTS(SELECT storageId FROM FTS_storage WHERE FTS_storage MATCH %S)",ftsName);
  filterAppend(filterString,TRUE,"AND","storage.state IN (%S)",getIndexStateSetString(string,indexStateSet));
  filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storage.mode IN (%S)",getIndexModeSetString(string,indexModeSet));
  String_delete(string);

  // get ordering
  appendOrdering(orderString,TRUE,"entities.created",ordering);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT IFNULL(uuids.id,0), \
                                   entities.jobUUID, \
                                   entities.id, \
                                   entities.scheduleUUID, \
                                   UNIXTIMESTAMP(entities.created), \
                                   entities.type, \
                                   (SELECT errorMessage FROM storage WHERE storage.entityId=entities.id ORDER BY created DESC LIMIT 0,1), \
                                   TOTAL(storage.totalEntryCount), \
                                   TOTAL(storage.totalEntrySize), \
                                   entities.lockedCount \
                            FROM entities \
                              LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              LEFT JOIN storage ON storage.entityId=entities.id \
                            WHERE %S \
                            GROUP BY entities.id \
                            %S \
                            LIMIT %llu,%llu \
                           ",
                           filterString,
                           orderString,
                           offset,
                           limit
                          );
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(orderString);
    String_delete(filterString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(orderString);
  String_delete(filterString);
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextEntity(IndexQueryHandle *indexQueryHandle,
                         IndexId          *uuidId,
                         String           jobUUID,
                         String           scheduleUUID,
                         IndexId          *entityId,
                         ArchiveTypes     *archiveType,
                         uint64           *createdDateTime,
                         String           lastErrorMessage,
                         ulong            *totalEntryCount,
                         uint64           *totalEntrySize,
                         uint             *lockedCount
                        )
{
  DatabaseId uuidId_,entityId_;
  double     totalEntryCount_;
  double     totalEntrySize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %lld %S %llu %u %S %lf %lf %d",
                           &uuidId_,
                           jobUUID,
                           &entityId_,
                           scheduleUUID,
                           createdDateTime,
                           archiveType,
                           lastErrorMessage,
                           &totalEntryCount_,
                           &totalEntrySize_,
                           &lockedCount
                          )
     )
  {
    return FALSE;
  }
  if (uuidId != NULL) (*uuidId) = INDEX_ID_ENTITY(uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_ENTITY(entityId_);
  if (totalEntryCount != NULL) (*totalEntryCount) = (ulong)totalEntryCount_;
  if (totalEntrySize != NULL) (*totalEntrySize) = (uint64)totalEntrySize_;

  return TRUE;
}

Errors Index_newEntity(IndexHandle  *indexHandle,
                       ConstString  jobUUID,
                       ConstString  scheduleUUID,
                       ArchiveTypes archiveType,
                       uint64       createdDateTime,
                       bool         locked,
                       IndexId      *entityId
                      )
{
  Errors    error;
  StringMap resultMap;

  assert(indexHandle != NULL);
  assert(entityId != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT OR IGNORE INTO uuids \
                                  ( \
                                   jobUUID \
                                  ) \
                                VALUES \
                                  ( \
                                   %'S \
                                  ); \
                               ",
                               jobUUID
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entities \
                                  ( \
                                   jobUUID, \
                                   scheduleUUID, \
                                   created, \
                                   type, \
                                   parentJobUUID, \
                                   bidFlag, \
                                   lockedCount \
                                  ) \
                                VALUES \
                                  ( \
                                   %'S, \
                                   %'S, \
                                   %llu, \
                                   %u, \
                                   '', \
                                   0, \
                                   %d \
                                  ); \
                               ",
                               jobUUID,
                               scheduleUUID,
                               (createdDateTime != 0LL) ? createdDateTime : Misc_getCurrentDateTime(),
                               archiveType,
                               locked ? 1 : 0
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      (*entityId) = INDEX_ID_ENTITY(Database_getLastRowId(&indexHandle->databaseHandle));

      return ERROR_NONE;
    });
  }
  else
  {
    resultMap = StringMap_new();

    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    resultMap,
                                    "INDEX_NEW_ENTITY jobUUID=%S scheduleUUID=%s archiveType=%s createdDateTime=%llu locked=%y",
                                    jobUUID,
                                    (scheduleUUID != NULL) ? String_cString(scheduleUUID) : "",
                                    Archive_archiveTypeToString(archiveType,NULL),
                                    createdDateTime,
                                    locked
                                   );
    if (error == ERROR_NONE)
    {
      if (!StringMap_getInt64 (resultMap,"entityId",entityId,INDEX_ID_NONE))
      {
        StringMap_delete(resultMap);
          error = ERROR_EXPECTED_PARAMETER;
      }
    }

    StringMap_delete(resultMap);
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_deleteEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  IndexId             storageId;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    // delete storages of entity
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT id \
                              FROM storage \
                              WHERE entityId=%lld; \
                             ",
                             Index_getDatabaseId(entityId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    while (   Database_getNextRow(&databaseQueryHandle,
                                  "%lld",
                                  &storageId
                                 )
           && (error == ERROR_NONE)
          )
    {
      error = Index_deleteStorage(indexHandle,INDEX_ID_STORAGE(storageId));
    }
    Database_finalize(&databaseQueryHandle);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // delete entity
    if (Index_getDatabaseId(entityId) != INDEX_DEFAULT_ENTITY_ID)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "DELETE FROM entities WHERE id=%lld;",
                               Index_getDatabaseId(entityId)
                              );
      if (error != ERROR_NONE)
      {
        (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
        return error;
      }
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_lockEntity(IndexHandle *indexHandle,
                        IndexId     entityId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entities SET lockedCount=lockedCount+1 WHERE id=%lld;",
                             Index_getDatabaseId(entityId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_unlockEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE entities SET lockedCount=lockedCount-1 WHERE id=%lld AND lockedCount>0;",
                             Index_getDatabaseId(entityId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_isEmptyEntity(IndexHandle *indexHandle,
                           IndexId     entityId
                          )
{
  bool emptyFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  INDEX_DOX(emptyFlag,
            indexHandle,
  {
    return !Database_exists(&indexHandle->databaseHandle,
                            "storage",
                            "id",
                            "WHERE entityId=%lld",
                            Index_getDatabaseId(entityId)
                           );
  });

  return emptyFlag;
}

Errors Index_getStoragesInfos(IndexHandle   *indexHandle,
                              IndexId       uuidId,
                              IndexId       entityId,
                              ConstString   jobUUID,
                              ConstString   scheduleUUID,
                              const IndexId indexIds[],
                              uint          indexIdCount,
                              IndexStateSet indexStateSet,
                              IndexModeSet  indexModeSet,
                              ConstString   name,
                              ulong         *storageCount,
                              ulong         *totalEntryCount,
                              uint64        *totalEntrySize,
                              uint64        *totalEntryContentSize
                             )
{
  String              ftsName;
  String              regexpName;
  String              filterString;
  String              uuidIdsString,entityIdsString,storageIdsString;
  uint                i;
  String              filterIdsString;
  String              indexStateSetString;
  String              indexModeSetString;
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  double              totalEntryCount_,totalEntrySize_,totalEntryContentSize_;

  assert(indexHandle != NULL);
  assert((uuidId == INDEX_ID_ANY) || (Index_getType(uuidId) == INDEX_TYPE_UUID));
  assert((entityId == INDEX_ID_ANY) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert((indexIdCount == 0) || (indexIds != NULL));

  // init variables
  if (storageCount          != NULL) (*storageCount         ) = 0L;
  if (totalEntryCount       != NULL) (*totalEntryCount      ) = 0L;
  if (totalEntrySize        != NULL) (*totalEntrySize       ) = 0LL;
  if (totalEntryContentSize != NULL) (*totalEntryContentSize) = 0LL;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  regexpName   = String_new();
  filterString = String_newCString("1");

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get id sets
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  storageIdsString = String_new();
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_format(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_format(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
        String_format(storageIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  filterIdsString     = String_new();
  indexStateSetString = String_new();
  indexModeSetString  = String_new();
  filterAppend(filterIdsString,!String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterIdsString,!String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  filterAppend(filterIdsString,!String_isEmpty(storageIdsString),"OR","storage.id IN (%S)",storageIdsString);
  filterAppend(filterString,(uuidId != INDEX_ID_ANY),"AND","entity.uuidId=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,(entityId != INDEX_ID_ANY),"AND","storage.entityId=%lld",Index_getDatabaseId(entityId));
  filterAppend(filterString,jobUUID != NULL,"AND","entities.jobUUID='%S'",jobUUID);
  filterAppend(filterString,scheduleUUID != NULL,"AND","entities.scheduleUUID='%S'",scheduleUUID);
  filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","storage.id IN (SELECT storageId FROM FTS_storage WHERE FTS_storage MATCH %S)",ftsName);
//  filterAppend(filterString,!String_isEmpty(regexpName),"AND","REGEXP(%S,0,storage.name)",regexpName);
  filterAppend(filterString,TRUE,"AND","storage.state IN (%S)",getIndexStateSetString(indexStateSetString,indexStateSet));
  filterAppend(filterString,TRUE,"AND","storage.mode IN (%S)",getIndexModeSetString(indexModeSetString,indexModeSet));
  String_delete(indexModeSetString);
  String_delete(indexStateSetString);
  String_delete(filterIdsString);

  INDEX_DOX(error,
            indexHandle,
  {
    // get storage count, entry count, entry size
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
//TODO newest
                             "SELECT COUNT(storage.id),\
                                     TOTAL(storage.totalEntryCount), \
                                     TOTAL(storage.totalEntrySize) \
                              FROM storage \
                                LEFT JOIN entities ON entities.id=storage.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE %S \
                             ",
                             filterString
                            );
//Database_debugPrintQueryInfo(&databaseQueryHandle);
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (Database_getNextRow(&databaseQueryHandle,
                            "%lu %lf %lf",
                            storageCount,
                            &totalEntryCount_,
                            &totalEntrySize_
                           )
          )
    {
      assert(totalEntryCount_ >= 0.0);
//TODO: may happen?
//      assert(totalEntrySize_ >= 0.0);
      if (totalEntryCount != NULL) (*totalEntryCount) = (totalEntryCount_ >= 0.0) ? (ulong)totalEntryCount_ : 0L;
      if (totalEntrySize != NULL) (*totalEntrySize) = (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
    }
    Database_finalize(&databaseQueryHandle);

    // get entry content size
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
//TODO newest
                             "SELECT TOTAL(directoryEntries.totalEntrySize) \
                              FROM storage \
                                LEFT JOIN directoryEntries ON directoryEntries.storageId=storage.id \
                                LEFT JOIN entities ON entities.id=storage.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE %S \
                             ",
                             filterString
                            );
//Database_debugPrintQueryInfo(&databaseQueryHandle);
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (Database_getNextRow(&databaseQueryHandle,
                            "%lf",
                            &totalEntryContentSize_
                           )
          )
    {
//TODO: may happen?
//      assert(totalEntryContentSize_ >= 0.0);
      if (totalEntryContentSize != NULL) (*totalEntryContentSize) = (totalEntryContentSize_ >= 0.0) ? (uint64)totalEntryContentSize_ : 0LL;
    }
    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(storageIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(filterString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(storageIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(filterString);
  String_delete(regexpName);
  String_delete(ftsName);

  return ERROR_NONE;
}

Errors Index_updateStorageInfos(IndexHandle *indexHandle,
                                IndexId     storageId
                               )
{
  Errors              error;
  ulong               totalFileCount;
  uint64              totalFileSize;
  ulong               totalImageCount;
  uint64              totalImageSize;
  ulong               totalDirectoryCount;
  ulong               totalLinkCount;
  ulong               totalHardlinkCount;
  uint64              totalHardlinkSize;
  ulong               totalSpecialCount;
  DatabaseQueryHandle databaseQueryHandle;
  double              totalFileSize_;
  double              totalImageSize_;
  double              totalHardlinkSize_;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // get file aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
  //TODO: use entries.size?
                               "SELECT COUNT(entries.id), \
                                       TOTAL(fileEntries.fragmentSize) \
                                FROM entries \
                                  LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                                WHERE     entries.type=%d \
                                      AND entries.storageId=%lld; \
                               ",
                               INDEX_TYPE_FILE,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalFileCount,
                          &totalFileSize_
                         );
      assert(totalFileSize_ >= 0.0);
      totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get image aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
  //TODO: use entries.size?
                               "SELECT COUNT(entries.id),\
                                       TOTAL(imageEntries.blockSize*imageEntries.blockCount) \
                                FROM entries \
                                  LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                                WHERE     entries.type=%d \
                                      AND entries.storageId=%lld; \
                               ",
                               INDEX_TYPE_IMAGE,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalImageCount,
                          &totalImageSize_
                         );
      assert(totalImageSize_ >= 0.0);
      totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get directory aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entries.id) \
                                FROM entries \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                WHERE     entries.type=%d \
                                      AND entries.storageId=%lld; \
                               ",
                               INDEX_TYPE_DIRECTORY,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalDirectoryCount
                         );
      Database_finalize(&databaseQueryHandle);

      // get link aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entries.id) \
                                FROM entries \
                                  LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                                WHERE     entries.type=%d \
                                      AND entries.storageId=%lld; \
                               ",
                               INDEX_TYPE_LINK,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalLinkCount
                         );
      Database_finalize(&databaseQueryHandle);

      // get hardlink aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
  //TODO: use entries.size?
                               "SELECT COUNT(entries.id), \
                                       TOTAL(hardlinkEntries.fragmentSize) \
                                FROM entries \
                                  LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                                WHERE     entries.type=%d \
                                      AND entries.storageId=%lld; \
                               ",
                               INDEX_TYPE_HARDLINK,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalHardlinkCount,
                          &totalHardlinkSize_
                         );
      assert(totalHardlinkSize_ >= 0.0);
      totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get special aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entries.id) \
                                FROM entries \
                                  LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                                WHERE     entries.type=%d \
                                      AND entries.storageId=%lld; \
                               ",
                               INDEX_TYPE_SPECIAL,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalSpecialCount
                         );
      Database_finalize(&databaseQueryHandle);

      // update aggregate data
//fprintf(stderr,"%s, %d: aggregate %llu %llu\n",__FILE__,__LINE__,totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,totalFileSize+totalImageSize+totalHardlinkSize);
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE storage \
                                SET totalEntryCount    =%llu, \
                                    totalEntrySize     =%llu, \
                                    totalFileCount     =%llu, \
                                    totalFileSize      =%llu, \
                                    totalImageCount    =%llu, \
                                    totalImageSize     =%llu, \
                                    totalDirectoryCount=%llu, \
                                    totalLinkCount     =%llu, \
                                    totalHardlinkCount =%llu, \
                                    totalHardlinkSize  =%llu, \
                                    totalSpecialCount  =%llu \
                                WHERE id=%lld; \
                               ",
                               totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,
                               totalFileSize+totalImageSize+totalHardlinkSize,
                               totalFileCount,
                               totalFileSize,
                               totalImageCount,
                               totalImageSize,
                               totalDirectoryCount,
                               totalLinkCount,
                               totalHardlinkCount,
                               totalHardlinkSize,
                               totalSpecialCount,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // -----------------------------------------------------------------

      // get newest file aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
  //TODO: use entriesNewest.size?
                               "SELECT COUNT(entriesNewest.id), \
                                       TOTAL(fileEntries.fragmentSize) \
                                FROM entriesNewest \
                                  LEFT JOIN fileEntries ON fileEntries.entryId=entriesNewest.entryId \
                                WHERE     entriesNewest.type=%d \
                                      AND entriesNewest.storageId=%lld; \
                               ",
                               INDEX_TYPE_FILE,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalFileCount,
                          &totalFileSize_
                         );
      assert(totalFileSize_ >= 0.0);
      totalFileSize = (totalFileSize_ >= 0.0) ? (uint64)totalFileSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get newest image aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
  //TODO: use entriesNewest.size?
                               "SELECT COUNT(entriesNewest.id), \
                                       TOTAL(imageEntries.blockSize*imageEntries.blockCount) \
                                FROM entriesNewest \
                                  LEFT JOIN imageEntries ON imageEntries.entryId=entriesNewest.entryId \
                                WHERE     entriesNewest.type=%d \
                                      AND entriesNewest.storageId=%lld; \
                               ",
                               INDEX_TYPE_IMAGE,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalImageCount,
                          &totalImageSize_
                         );
      assert(totalImageSize_ >= 0.0);
      totalImageSize = (totalImageSize_ >= 0.0) ? (uint64)totalImageSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get newest directory aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entriesNewest.id) \
                                FROM entriesNewest \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                WHERE     entriesNewest.type=%d \
                                      AND entriesNewest.storageId=%lld; \
                               ",
                               INDEX_TYPE_DIRECTORY,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalDirectoryCount
                         );
      Database_finalize(&databaseQueryHandle);

      // get newest link aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entriesNewest.id) \
                                FROM entriesNewest \
                                  LEFT JOIN linkEntries ON linkEntries.entryId=entriesNewest.entryId \
                                WHERE     entriesNewest.type=%d \
                                      AND entriesNewest.storageId=%lld; \
                               ",
                               INDEX_TYPE_LINK,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalLinkCount
                         );
      Database_finalize(&databaseQueryHandle);

      // get newest hardlink aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
  //TODO: use entriesNewest.size?
                               "SELECT COUNT(entriesNewest.id), \
                                       TOTAL(hardlinkEntries.fragmentSize) \
                                FROM entriesNewest \
                                  LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entriesNewest.entryId \
                                WHERE     entriesNewest.type=%d \
                                      AND entriesNewest.storageId=%lld; \
                               ",
                               INDEX_TYPE_HARDLINK,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu %lf",
                          &totalHardlinkCount,
                          &totalHardlinkSize_
                         );
      assert(totalHardlinkSize_ >= 0.0);
      totalHardlinkSize = (totalHardlinkSize_ >= 0.0) ? (uint64)totalHardlinkSize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get newest special aggregate data
      error = Database_prepare(&databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT COUNT(entriesNewest.id) \
                                FROM entriesNewest \
                                  LEFT JOIN specialEntries ON specialEntries.entryId=entriesNewest.entryId \
                                WHERE     entriesNewest.type=%d \
                                      AND entriesNewest.storageId=%lld; \
                               ",
                               INDEX_TYPE_SPECIAL,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      Database_getNextRow(&databaseQueryHandle,
                          "%lu",
                          &totalSpecialCount
                         );
      Database_finalize(&databaseQueryHandle);

      // update newest aggregate data
//fprintf(stderr,"%s, %d: newest aggregate %llu %llu\n",__FILE__,__LINE__,totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,totalFileSize+totalImageSize+totalHardlinkSize);
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE storage \
                                SET totalEntryCountNewest    =%llu, \
                                    totalEntrySizeNewest     =%llu, \
                                    totalFileCountNewest     =%llu, \
                                    totalFileSizeNewest      =%llu, \
                                    totalImageCountNewest    =%llu, \
                                    totalImageSizeNewest     =%llu, \
                                    totalDirectoryCountNewest=%llu, \
                                    totalLinkCountNewest     =%llu, \
                                    totalHardlinkCountNewest =%llu, \
                                    totalHardlinkSizeNewest  =%llu, \
                                    totalSpecialCountNewest  =%llu \
                                WHERE id=%lld; \
                               ",
                               totalFileCount+totalImageCount+totalDirectoryCount+totalLinkCount+totalHardlinkCount+totalSpecialCount,
                               totalFileSize+totalImageSize+totalHardlinkSize,
                               totalFileCount,
                               totalFileSize,
                               totalImageCount,
                               totalImageSize,
                               totalDirectoryCount,
                               totalLinkCount,
                               totalHardlinkCount,
                               totalHardlinkSize,
                               totalSpecialCount,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    NULL,  // resultMap
                                    "INDEX_UPDATE_STORAGE_INFOS storageId=%lld",
                                    storageId
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_initListStorages(IndexQueryHandle      *indexQueryHandle,
                              IndexHandle           *indexHandle,
                              IndexId               uuidId,
                              IndexId               entityId,
                              ConstString           jobUUID,
                              ConstString           scheduleUUID,
                              const IndexId         indexIds[],
                              uint                  indexIdCount,
                              IndexStateSet         indexStateSet,
                              IndexModeSet          indexModeSet,
                              ConstString           name,
                              IndexStorageSortModes sortMode,
                              DatabaseOrdering      ordering,
                              uint64                offset,
                              uint64                limit
                             )
{
  String ftsName;
  String regexpName;
  String filterString;
  String string;
  String orderString;
  String uuidIdsString,entityIdsString,storageIdsString;
  String filterIdsString;
  Errors error;
  uint   i;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((uuidId == INDEX_ID_ANY) || (Index_getType(uuidId) == INDEX_TYPE_UUID));
  assert((entityId == INDEX_ID_ANY) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));
  assert((indexIdCount == 0) || (indexIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  regexpName   = String_new();
  filterString = String_newCString("1");
  orderString  = String_new();

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get id sets
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  storageIdsString = String_new();
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_format(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_format(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
        String_format(storageIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      default:
        // ignore other types
        break;
    }
  }

  // get filters
  filterIdsString = String_new();
  string = String_new();
  filterAppend(filterIdsString,!String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterIdsString,!String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  filterAppend(filterIdsString,!String_isEmpty(storageIdsString),"OR","storage.id IN (%S)",storageIdsString);
  filterAppend(filterString,(uuidId != INDEX_ID_ANY),"AND","uuids.id=%lld",Index_getDatabaseId(uuidId));
  filterAppend(filterString,(entityId != INDEX_ID_ANY),"AND","storage.entityId=%lld",Index_getDatabaseId(entityId));
  filterAppend(filterString,jobUUID != NULL,"AND","entities.jobUUID='%S'",jobUUID);
  filterAppend(filterString,scheduleUUID != NULL,"AND","entities.scheduleUUID='%S'",scheduleUUID);
  filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","storage.id IN (SELECT storageId FROM FTS_storage WHERE FTS_storage MATCH %S)",ftsName);
//  filterAppend(filterString,!String_isEmpty(regexpName),"AND","REGEXP(%S,0,storage.name)",regexpName);
  filterAppend(filterString,TRUE,"AND","storage.state IN (%S)",getIndexStateSetString(string,indexStateSet));
  filterAppend(filterString,indexModeSet != INDEX_MODE_SET_ALL,"AND","storage.mode IN (%S)",getIndexModeSetString(string,indexModeSet));
  String_delete(string);
  String_delete(filterIdsString);

  // get sort mode, ordering
  appendOrdering(orderString,sortMode != INDEX_STORAGE_SORT_MODE_NONE,INDEX_STORAGE_SORT_MODE_COLUMNS[sortMode],ordering);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
//TODO newest
                           "SELECT IFNULL(uuids.id,0), \
                                   entities.jobUUID, \
                                   IFNULL(entities.id,0), \
                                   entities.scheduleUUID, \
                                   entities.type, \
                                   storage.id, \
                                   storage.name, \
                                   UNIXTIMESTAMP(storage.created), \
                                   storage.size, \
                                   storage.state, \
                                   storage.mode, \
                                   UNIXTIMESTAMP(storage.lastChecked), \
                                   storage.errorMessage, \
                                   storage.totalEntryCount, \
                                   storage.totalEntrySize \
                            FROM storage \
                              LEFT JOIN entities ON entities.id=storage.entityId \
                              LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                            WHERE %S \
                            GROUP BY storage.id \
                            %S \
                            LIMIT %llu,%llu \
                           ",
                           filterString,
                           orderString,
                           offset,
                           limit
                          );
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(orderString);
    String_delete(filterString);
    String_delete(storageIdsString);
    String_delete(entityIdsString);
    String_delete(uuidIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);

  // free resources
  String_delete(orderString);
  String_delete(filterString);
  String_delete(storageIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextStorage(IndexQueryHandle *indexQueryHandle,
                          IndexId          *uuidId,
                          String           jobUUID,
                          IndexId          *entityId,
                          String           scheduleUUID,
                          ArchiveTypes     *archiveType,
                          IndexId          *storageId,
                          String           storageName,
                          uint64           *createdDateTime,
                          uint64           *size,
                          IndexStates      *indexState,
                          IndexModes       *indexMode,
                          uint64           *lastCheckedDateTime,
                          String           errorMessage,
                          ulong            *totalEntryCount,
                          uint64           *totalEntrySize
                         )
{
  DatabaseId uuidId_,entityId_,storageId_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %lld %S %d %lld %S %llu %llu %d %d %llu %S %lu %llu",
                           &uuidId_,
                           jobUUID,
                           &entityId_,
                           scheduleUUID,
                           archiveType,
                           &storageId_,
                           storageName,
                           createdDateTime,
                           size,
                           indexState,
                           indexMode,
                           lastCheckedDateTime,
                           errorMessage,
                           totalEntryCount,
                           totalEntrySize
                          )
    )
  {
    return FALSE;
  }
  if (uuidId != NULL) (*uuidId) = INDEX_ID_UUID(uuidId_);
  if (entityId != NULL) (*entityId) = INDEX_ID_ENTITY(entityId_);
  if (storageId != NULL) (*storageId) = INDEX_ID_STORAGE(storageId_);

  return TRUE;
}

Errors Index_newStorage(IndexHandle *indexHandle,
                        IndexId     entityId,
                        ConstString storageName,
                        uint64      createdDateTime,
                        uint64      size,
                        IndexStates indexState,
                        IndexModes  indexMode,
                        IndexId     *storageId
                       )
{
  Errors    error;
  StringMap resultMap;

  assert(indexHandle != NULL);
  assert(storageId != NULL);
  assert((entityId == INDEX_ID_NONE) || (Index_getType(entityId) == INDEX_TYPE_ENTITY));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

//TODO
fprintf(stderr,"%s, %d: new storageName=%s\n",__FILE__,__LINE__,String_cString(storageName));
  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO storage \
                                  ( \
                                   entityId, \
                                   name, \
                                   created, \
                                   size, \
                                   state, \
                                   mode, \
                                   lastChecked\
                                  ) \
                                VALUES \
                                  ( \
                                   %d, \
                                   %'S, \
                                   DATETIME(%llu,'unixepoch'), \
                                   %llu, \
                                   %d, \
                                   %d, \
                                   DATETIME('now') \
                                  ); \
                               ",
                               Index_getDatabaseId(entityId),
                               storageName,
                               createdDateTime,
                               size,
                               indexState,
                               indexMode
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      (*storageId) = INDEX_ID_STORAGE(Database_getLastRowId(&indexHandle->databaseHandle));

      return ERROR_NONE;
    });
  }
  else
  {
    resultMap = StringMap_new();

    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    resultMap,
                                    "INDEX_NEW_STORAGE entityId=%lld storageName=%'s createdDateTime=%llu size=%llu indexState=%s indexMode=%s",
                                    entityId,
                                    (storageName != NULL) ? String_cString(storageName) : "",
                                    createdDateTime,
                                    size,
                                    Index_stateToString(indexState,NULL),
                                    Index_modeToString(indexMode,NULL)
                                   );
    if (error == ERROR_NONE)
    {
      if (!StringMap_getInt64 (resultMap,"storageId",storageId,INDEX_ID_NONE))
      {
        error = ERROR_EXPECTED_PARAMETER;
      }
    }

    StringMap_delete(resultMap);
  }

  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_updateStorage(IndexHandle *indexHandle,
                           IndexId     storageId,
                           ConstString storageName,
                           uint64      createdDateTime,
                           uint64      size
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    if (storageName != NULL)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE storage \
                                  SET name=%'S \
                                  WHERE id=%lld; \
                               ",
                               storageName,
                               createdDateTime,
                               size,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "UPDATE storage \
                                SET created=DATETIME(%llu,'unixepoch'), \
                                    size=%llu \
                                WHERE id=%lld; \
                             ",
                             createdDateTime,
                             size,
                             Index_getDatabaseId(storageId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_deleteStorage(IndexHandle *indexHandle,
                           IndexId     storageId
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE storage \
                                SET state=%d \
                                WHERE id=%lld; \
                               ",
                               INDEX_STATE_DELETED,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // trigger clean-up thread
      Semaphore_signalModified(&indexThreadTrigger);

      return ERROR_NONE;
    });
  }
  else
  {
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
  }

  return error;
}

Errors Index_isEmptyStorage(IndexHandle *indexHandle,
                            IndexId     storageId
                           )
{
  bool emptyFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(emptyFlag,
              indexHandle,
    {
      return !Database_exists(&indexHandle->databaseHandle,
                              "entries",
                              "id",
                              "WHERE storageId=%lld",
                              Index_getDatabaseId(storageId)
                             );
    });
  }
  else
  {
//TODO
emptyFlag=TRUE;
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
  }

  return emptyFlag;
}

Errors Index_clearStorage(IndexHandle *indexHandle,
                          IndexId     storageId
                         )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = deleteFromIndex(indexHandle,
                            "fileEntries",
                            "storageId=%lld",
                            Index_getDatabaseId(storageId)
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entriesNewest",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_FILE
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entries",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_FILE
                           );
    if (error != ERROR_NONE) return error;

    error = deleteFromIndex(indexHandle,
                            "imageEntries",
                            "storageId=%lld",
                            Index_getDatabaseId(storageId)
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entriesNewest",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_IMAGE
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entries",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_IMAGE
                           );
    if (error != ERROR_NONE) return error;

    error = deleteFromIndex(indexHandle,
                            "directoryEntries",
                            "storageId=%lld",
                            Index_getDatabaseId(storageId)
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entriesNewest",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_DIRECTORY
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entries",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_DIRECTORY
                           );
    if (error != ERROR_NONE) return error;

    error = deleteFromIndex(indexHandle,
                            "linkEntries",
                            "storageId=%lld",
                            Index_getDatabaseId(storageId)
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entriesNewest",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_LINK
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entries",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_LINK
                           );
    if (error != ERROR_NONE) return error;

    error = deleteFromIndex(indexHandle,
                            "hardlinkEntries",
                            "storageId=%lld",
                            Index_getDatabaseId(storageId)
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entriesNewest",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_HARDLINK
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entries",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_HARDLINK
                           );
    if (error != ERROR_NONE) return error;

    error = deleteFromIndex(indexHandle,
                            "specialEntries",
                            "storageId=%lld",
                            Index_getDatabaseId(storageId)
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entriesNewest",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_SPECIAL
                           );
    if (error != ERROR_NONE) return error;
    error = deleteFromIndex(indexHandle,
                            "entries",
                            "storageId=%lld AND type=%d",
                            Index_getDatabaseId(storageId),
                            INDEX_TYPE_SPECIAL
                           );
    if (error != ERROR_NONE) return error;

    return ERROR_NONE;
  });

  return error;
}


Errors Index_getStorage(IndexHandle *indexHandle,
                        IndexId      storageId,
                        IndexId      *uuidId,
                        String       jobUUID,
                        IndexId      *entityId,
                        String       scheduleUUID,
                        ArchiveTypes archiveType,
                        String       storageName,
                        uint64       *createdDateTime,
                        uint64       *size,
                        IndexStates  *indexState,
                        IndexModes   *indexMode,
                        uint64       *lastCheckedDateTime,
                        String       errorMessage,
                        uint64       *totalEntryCount,
                        uint64       *totalEntrySize
                       )
{
  Errors              error;
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId          uuidId_,entityId_;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT uuids.id, \
                                     uuids.jobUUID, \
                                     entities.id, \
                                     entities.scheduleUUID, \
                                     entities.type, \
                                     storage.name, \
                                     UNIXTIMESTAMP(storage.created), \
                                     storage.size, \
                                     storage.state, \
                                     storage.mode, \
                                     UNIXTIMESTAMP(storage.lastChecked), \
                                     storage.errorMessage, \
                                     storage.totalEntryCount, \
                                     storage.totalEntrySize \
                              FROM storage \
                                LEFT JOIN entities ON entities.id=storage.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE id=%d \
                             ",
                             Index_getDatabaseId(storageId)
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
    if (!Database_getNextRow(&databaseQueryHandle,
                             "%llu %S %llu %S %d %S %llu %llu %d %d %llu %S %llu %llu",
                             &uuidId_,
                             jobUUID,
                             &entityId_,
                             scheduleUUID,
                             archiveType,
                             storageName,
                             createdDateTime,
                             size,
                             indexState,
                             indexMode,
                             lastCheckedDateTime,
                             errorMessage,
                             totalEntryCount,
                             totalEntrySize
                            )
       )
    {
      Database_finalize(&databaseQueryHandle);
      return ERROR_DATABASE_INDEX_NOT_FOUND;
    }
    if (uuidId != NULL)   (*uuidId  ) = INDEX_ID_(INDEX_TYPE_UUID,  uuidId_  );
    if (entityId != NULL) (*entityId) = INDEX_ID_(INDEX_TYPE_ENTITY,entityId_);
    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_storageUpdate(IndexHandle *indexHandle,
                           IndexId     storageId,
                           ConstString storageName,
                           uint64      storageSize
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // update name
      if (storageName != NULL)
      {
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "UPDATE storage \
                                  SET name=%'S \
                                  WHERE id=%lld; \
                                 ",
                                 storageName,
                                 Index_getDatabaseId(storageId)
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }

      // update size
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "UPDATE storage \
                                SET size=%llu \
                                WHERE id=%lld; \
                               ",
                               storageSize,
                               Index_getDatabaseId(storageId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    NULL,  // resultMap
                                    "INDEX_STORAGE_UPDATE storageId=%lld storageName=%'S storageSize=%llu",
                                    storageId,
                                    storageName,
                                    storageSize
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_getEntriesInfo(IndexHandle   *indexHandle,
                            const IndexId indexIds[],
                            uint          indexIdCount,
                            const IndexId entryIds[],
                            uint          entryIdCount,
                            IndexTypeSet  indexTypeSet,
                            ConstString   name,
                            bool          newestOnly,
                            ulong         *totalEntryCount,
                            uint64        *totalEntrySize,
                            uint64        *totalEntryContentSize
                           )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  String              ftsName;
  String              regexpName;
  String              uuidIdsString,entityIdsString,storageIdsString;
  String              entryIdsString;
  uint                i;
  String              filterString,filterIdsString;
  String              indexTypeSetString;
  double              totalEntryCount_,totalEntrySize_,totalEntryContentSize_;

  assert(indexHandle != NULL);
  assert((indexIdCount == 0) || (indexIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // init variables
  if (totalEntryCount != NULL) (*totalEntryCount) = 0L;
  if (totalEntrySize != NULL) (*totalEntrySize) = 0LL;
  if (totalEntryContentSize != NULL) (*totalEntryContentSize) = 0LL;

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName    = String_new();
  regexpName = String_new();

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get id sets
  uuidIdsString    = String_new();
  entityIdsString  = String_new();
  storageIdsString = String_new();
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_format(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdsString)) String_appendChar(entityIdsString,',');
        String_format(entityIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
        String_format(storageIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* not NDEBUG */
        break;
    }
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
    String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
  }

  error              = ERROR_NONE;
  filterString       = String_newCString("1");
  indexTypeSetString = String_new();

  // get filters
  filterIdsString = String_new();
  filterAppend(filterIdsString,!String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterIdsString,!String_isEmpty(entityIdsString),"OR","entities.id IN (%S)",entityIdsString);
  filterAppend(filterIdsString,!String_isEmpty(storageIdsString),"OR","storage.id IN (%S)",storageIdsString);
  filterAppend(filterString,!String_isEmpty(filterIdsString),"AND","(%S)",filterIdsString);
  String_delete(filterIdsString);

  if      (String_isEmpty(ftsName) && String_isEmpty(entryIdsString))
  {
    // no names/no entries selected

    INDEX_DOX(error,
              indexHandle,
    {
      if (IN_SET(indexTypeSet,INDEX_TYPE_FILE))
      {
        // get file count, file size
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s),\
                                         TOTAL(%s) \
                                  FROM storage \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 newestOnly ? "storage.totalFileCountNewest" : "storage.totalFileCount",
                                 newestOnly ? "storage.totalFileSizeNewest" : "storage.totalFileSize",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (!Database_getNextRow(&databaseQueryHandle,
                                 "%lf %lf",
                                 &totalEntryCount_,
                                 &totalEntrySize_
                                )
           )
        {
          Database_finalize(&databaseQueryHandle);
          return ERROR_INTERRUPTED;
        }
        assert(totalEntryCount_ >= 0.0);
        assert(totalEntrySize_ >= 0.0);
        if (totalEntryCount != NULL) (*totalEntryCount) += (totalEntryCount_ >= 0.0) ? (ulong)totalEntryCount_ : 0L;
        if (totalEntrySize != NULL) (*totalEntrySize) += (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
        Database_finalize(&databaseQueryHandle);
      }
      if (IN_SET(indexTypeSet,INDEX_TYPE_IMAGE))
      {
        // get image count, image size
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s),\
                                         TOTAL(%s) \
                                  FROM storage \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 newestOnly ? "storage.totalImageCountNewest" : "storage.totalImageCount",
                                 newestOnly ? "storage.totalImageSizeNewest" : "storage.totalImageSize",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (!Database_getNextRow(&databaseQueryHandle,
                                 "%lf %lf",
                                 &totalEntryCount_,
                                 &totalEntrySize_
                                )
           )
        {
          Database_finalize(&databaseQueryHandle);
          return ERROR_INTERRUPTED;
        }
        assert(totalEntryCount_ >= 0.0);
        assert(totalEntrySize_ >= 0.0);
        if (totalEntryCount != NULL) (*totalEntryCount) += (totalEntryCount_ >= 0.0) ? (ulong)totalEntryCount_ : 0L;
        if (totalEntrySize != NULL) (*totalEntrySize) += (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
        Database_finalize(&databaseQueryHandle);
      }
      if (IN_SET(indexTypeSet,INDEX_TYPE_DIRECTORY))
      {
        // get directory count
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s) \
                                  FROM storage \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 newestOnly ? "storage.totalDirectoryCountNewest" : "storage.totalDirectoryCount",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (!Database_getNextRow(&databaseQueryHandle,
                                 "%lf",
                                 &totalEntryCount_
                                )
           )
        {
          Database_finalize(&databaseQueryHandle);
          return ERROR_INTERRUPTED;
        }
        assert(totalEntryCount_ >= 0.0);
        if (totalEntryCount != NULL) (*totalEntryCount) += (totalEntryCount_ >= 0.0) ? (ulong)totalEntryCount_ : 0L;
        Database_finalize(&databaseQueryHandle);

        // get directory content size
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s) \
                                  FROM storage \
                                    LEFT JOIN directoryEntries ON directoryEntries.storageId=storage.id \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 newestOnly ? "directoryEntries.totalEntrySizeNewest" : "directoryEntries.totalEntrySize",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (!Database_getNextRow(&databaseQueryHandle,
                                 "%lf",
                                  &totalEntryContentSize_
                                )
           )
        {
          Database_finalize(&databaseQueryHandle);
          return ERROR_INTERRUPTED;
        }
// TODO: may happen?
//        assert(totalEntryContentSize_ >= 0.0);
        if (totalEntryContentSize != NULL) (*totalEntryContentSize) += (totalEntryContentSize_ >= 0.0) ? (ulong)totalEntryContentSize_ : 0L;
        Database_finalize(&databaseQueryHandle);
      }
      if (IN_SET(indexTypeSet,INDEX_TYPE_LINK))
      {
        // get link count
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s) \
                                  FROM storage \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 newestOnly ? "storage.totalLinkCountNewest" : "storage.totalLinkCount",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (!Database_getNextRow(&databaseQueryHandle,
                                 "%lf",
                                  &totalEntryCount_
                                )
           )
        {
          Database_finalize(&databaseQueryHandle);
          return ERROR_INTERRUPTED;
        }
        assert(totalEntryCount_ >= 0.0);
        if (totalEntryCount != NULL) (*totalEntryCount) += (totalEntryCount_ >= 0.0) ? (ulong)totalEntryCount_ : 0L;
        Database_finalize(&databaseQueryHandle);
      }
      if (IN_SET(indexTypeSet,INDEX_TYPE_HARDLINK))
      {
        // get hardlink count, hardlink size
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s),\
                                         TOTAL(%s) \
                                  FROM storage \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 newestOnly ? "storage.totalHardlinkCountNewest" : "storage.totalHardlinkCount",
                                 newestOnly ? "storage.totalHardlinkSizeNewest" : "storage.totalHardlinkSize",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (!Database_getNextRow(&databaseQueryHandle,
                                 "%lf %lf",
                                 &totalEntryCount_,
                                 &totalEntrySize_
                                )
           )
        {
          Database_finalize(&databaseQueryHandle);
          return ERROR_INTERRUPTED;
        }
        assert(totalEntryCount_ >= 0.0);
        assert(totalEntrySize_ >= 0.0);
        if (totalEntryCount != NULL) (*totalEntryCount) += (totalEntryCount_ >= 0.0) ? (ulong)totalEntryCount_ : 0L;
        if (totalEntrySize != NULL) (*totalEntrySize) += (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
        Database_finalize(&databaseQueryHandle);
      }
      if (IN_SET(indexTypeSet,INDEX_TYPE_SPECIAL))
      {
        // get special count
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(%s) \
                                  FROM storage \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 newestOnly ? "storage.totalSpecialCountNewest" : "storage.totalSpecialCount",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
        if (error != ERROR_NONE)
        {
          return error;
        }
        if (!Database_getNextRow(&databaseQueryHandle,
                                 "%lf",
                                 &totalEntryCount_
                                )
           )
        {
          Database_finalize(&databaseQueryHandle);
          return ERROR_INTERRUPTED;
        }
        assert(totalEntryCount_ >= 0.0);
        if (totalEntryCount != NULL) (*totalEntryCount) += (totalEntryCount_ >= 0.0) ? (ulong)totalEntryCount_ : 0L;
        Database_finalize(&databaseQueryHandle);
      }

      return ERROR_NONE;
    });
  }
  else if (String_isEmpty(ftsName) && !String_isEmpty(entryIdsString))
  {
    // entries selected

    // get filters
    if (newestOnly)
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
      filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entriesNewest.type IN (%S)",getIndexTypeSetString(indexTypeSetString,indexTypeSet));
    }
    else
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
      filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entries.type IN (%S)",getIndexTypeSetString(indexTypeSetString,indexTypeSet));
    }
    INDEX_DOX(error,
              indexHandle,
    {
      // get entry count, entry size
      if (newestOnly)
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(entriesNewest.id),\
                                         TOTAL(entriesNewest.size) \
                                  FROM entriesNewest \
                                    LEFT JOIN entries ON entries.id=entriesNewest.entryId \
                                    LEFT JOIN storage ON storage.id=entries.storageId \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     entriesNewest.id IS NOT NULL \
                                        AND %S \
                                        AND storage.state!=%d \
                                 ",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
      }
      else
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(entries.id), \
                                         TOTAL(entries.size) \
                                  FROM entries \
                                    LEFT JOIN storage ON storage.id=entries.storageId \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
      }
      if (error != ERROR_NONE)
      {
        return error;
      }
//Database_debugPrintQueryInfo(&databaseQueryHandle);
      if (!Database_getNextRow(&databaseQueryHandle,
                               "%lu %lf",
                               totalEntryCount,
                               &totalEntrySize_
                              )
         )
      {
        Database_finalize(&databaseQueryHandle);
        return ERROR_UNKNOWN;
      }
      assert(totalEntrySize_ >= 0.0);
      if (totalEntrySize != NULL) (*totalEntrySize) += (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get entry content size
      if (newestOnly)
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                  FROM entriesNewest \
                                    LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                    LEFT JOIN entries ON entries.id=entriesNewest.entryId \
                                    LEFT JOIN storage ON storage.id=entries.storageId \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
      }
      else
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                  FROM entries \
                                    LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                    LEFT JOIN storage ON storage.id=entries.storageId \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
      }
      if (error != ERROR_NONE)
      {
        return error;
      }
//Database_debugPrintQueryInfo(&databaseQueryHandle);
      if (!Database_getNextRow(&databaseQueryHandle,
                               "%lf",
                               &totalEntryContentSize_
                              )
         )
      {
        Database_finalize(&databaseQueryHandle);
        return ERROR_INTERRUPTED;
      }
//TODO: may happend?
//      assert(totalEntryContentSize_ >= 0.0);
      if (totalEntryContentSize != NULL) (*totalEntryContentSize) += (totalEntryContentSize_ >= 0.0) ? (ulong)totalEntryContentSize_ : 0L;
      Database_finalize(&databaseQueryHandle);

      return ERROR_NONE;
    });
  }
  else /* (!String_isEmpty(ftsName) && String_isEmpty(entryIdsString)) */
  {
    // names (and optinally entries) selected

    // get filters
    filterAppend(filterString,!String_isEmpty(ftsName),"AND","FTS_entries MATCH %S",ftsName);
    if (newestOnly)
    {
//      filterAppend(filterString,!String_isEmpty(pattern),"AND","REGEXP(%S,0,entriesNewest.name)",regexpString);
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
      filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entriesNewest.type IN (%S)",getIndexTypeSetString(indexTypeSetString,indexTypeSet));
    }
    else
    {
//      filterAppend(filterString,!String_isEmpty(pattern),"AND","REGEXP(%S,0,entries.name)",regexpString);
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
      filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entries.type IN (%S)",getIndexTypeSetString(indexTypeSetString,indexTypeSet));
    }
    INDEX_DOX(error,
              indexHandle,
    {
      // get entry count, entry size
      if (newestOnly)
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(entriesNewest.id),\
                                         TOTAL(entriesNewest.size) \
                                  FROM FTS_entries \
                                    LEFT JOIN entriesNewest ON entriesNewest.entryId=FTS_entries.entryId \
                                    LEFT JOIN entries ON entries.id=entriesNewest.entryId \
                                    LEFT JOIN storage ON storage.id=entries.storageId \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     entriesNewest.id IS NOT NULL \
                                        AND %S \
                                        AND storage.state!=%d \
                                 ",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
      }
      else
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT COUNT(entries.id), \
                                         TOTAL(entries.size) \
                                  FROM FTS_entries \
                                    LEFT JOIN entries ON entries.id=FTS_entries.entryId \
                                    LEFT JOIN storage ON storage.id=entries.storageId \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
      }
      if (error != ERROR_NONE)
      {
        return error;
      }
//Database_debugPrintQueryInfo(&databaseQueryHandle);
      if (!Database_getNextRow(&databaseQueryHandle,
                               "%lu %lf",
                               totalEntryCount,
                               &totalEntrySize_
                              )
         )
      {
        Database_finalize(&databaseQueryHandle);
        return ERROR_UNKNOWN;
      }
      assert(totalEntrySize_ >= 0.0);
      if (totalEntrySize != NULL) (*totalEntrySize) += (totalEntrySize_ >= 0.0) ? (uint64)totalEntrySize_ : 0LL;
      Database_finalize(&databaseQueryHandle);

      // get entry content size
      if (newestOnly)
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                  FROM FTS_entries \
                                    LEFT JOIN entriesNewest ON entriesNewest.entryId=FTS_entries.entryId \
                                    LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                    LEFT JOIN entries ON entries.id=entriesNewest.entryId \
                                    LEFT JOIN storage ON storage.id=entries.storageId \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
      }
      else
      {
        error = Database_prepare(&databaseQueryHandle,
                                 &indexHandle->databaseHandle,
                                 "SELECT TOTAL(directoryEntries.totalEntrySize) \
                                  FROM FTS_entries \
                                    LEFT JOIN directoryEntries ON directoryEntries.entryId=FTS_entries.entryId \
                                    LEFT JOIN entries ON entries.id=FTS_entries.entryId \
                                    LEFT JOIN storage ON storage.id=entries.storageId \
                                    LEFT JOIN entities ON entities.id=storage.entityId \
                                    LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  WHERE     %S \
                                        AND storage.state!=%d \
                                 ",
                                 filterString,
                                 INDEX_STATE_DELETED
                                );
      }
      if (error != ERROR_NONE)
      {
        return error;
      }
//Database_debugPrintQueryInfo(&databaseQueryHandle);
      if (!Database_getNextRow(&databaseQueryHandle,
                               "%lf",
                               &totalEntryContentSize_
                              )
         )
      {
        Database_finalize(&databaseQueryHandle);
        return ERROR_INTERRUPTED;
      }
//TODO: may happend?
//      assert(totalEntryContentSize_ >= 0.0);
      if (totalEntryContentSize != NULL) (*totalEntryContentSize) += (totalEntryContentSize_ >= 0.0) ? (ulong)totalEntryContentSize_ : 0L;
      Database_finalize(&databaseQueryHandle);

      return ERROR_NONE;
    });
  }

  // free resources
  String_delete(indexTypeSetString);
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(entityIdsString);
  String_delete(uuidIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  return error;
}

Errors Index_initListEntries(IndexQueryHandle    *indexQueryHandle,
                             IndexHandle         *indexHandle,
                             const IndexId       indexIds[],
                             uint                indexIdCount,
                             const IndexId       entryIds[],
                             uint                entryIdCount,
                             IndexTypeSet        indexTypeSet,
                             ConstString         name,
                             IndexEntrySortModes sortMode,
                             DatabaseOrdering    ordering,
                             bool                newestOnly,
                             uint64              offset,
                             uint64              limit
                            )
{
  String ftsName;
  String regexpName;
  String uuidIdsString,entityIdString,storageIdsString;
  String entryIdsString;
  DatabaseQueryHandle databaseQueryHandle;
  DatabaseId storageId;
  uint   i;
  String filterString,orderString;
  String string;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((indexIdCount == 0) || (indexIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName          = String_new();
  regexpName       = String_new();
  uuidIdsString    = String_new();
  entityIdString   = String_new();
  storageIdsString = String_new();
  entryIdsString   = String_new();
  filterString     = String_new();
  orderString      = String_new();
  string           = String_new();

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get id sets
  for (i = 0; i < indexIdCount; i++)
  {
    switch (Index_getType(indexIds[i]))
    {
      case INDEX_TYPE_UUID:
        if (!String_isEmpty(uuidIdsString)) String_appendChar(uuidIdsString,',');
        String_format(uuidIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_ENTITY:
        if (!String_isEmpty(entityIdString)) String_appendChar(entityIdString,',');
        String_format(entityIdString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      case INDEX_TYPE_STORAGE:
        if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
        String_format(storageIdsString,"%lld",Index_getDatabaseId(indexIds[i]));
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* not NDEBUG */
        break;
    }
  }
  for (i = 0; i < entryIdCount; i++)
  {
    if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
    String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
  }

  // get storage id set (Note: collecting storage ids is faster than SQL joins of tables)
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  String_setCString(filterString,"0");
  filterAppend(filterString,!String_isEmpty(uuidIdsString),"OR","uuids.id IN (%S)",uuidIdsString);
  filterAppend(filterString,!String_isEmpty(entityIdString),"OR","entities.id IN (%S)",entityIdString);
  filterAppend(filterString,!String_isEmpty(uuidIdsString),"OR","storage.id IN (%S)",storageIdsString);
  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_prepare(&databaseQueryHandle,
                             &indexHandle->databaseHandle,
                             "SELECT storage.id \
                              FROM storage \
                                LEFT JOIN entities ON entities.id=storage.entityId \
                                LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                              WHERE %S; \
                             ",
                             filterString
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
//Database_debugPrintQueryInfo(&databaseQueryHandle);
    while (Database_getNextRow(&databaseQueryHandle,
                               "%llu",
                               &storageId
                              )
          )
    {
      if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%lld",storageId);
    }
    Database_finalize(&databaseQueryHandle);

    return ERROR_NONE;
  });
  if (error != ERROR_NONE)
  {
    String_delete(string);
    String_delete(orderString);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(entityIdString);
    String_delete(uuidIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // get filters
  String_setCString(filterString,"1");
  if (newestOnly)
  {
    filterAppend(filterString,!String_isEmpty(storageIdsString),"AND","entriesNewest.storageId IN (%S)",storageIdsString);  // Note: use entries.storageId instead of storage.id: this is must faster
    filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entriesNewest.type IN (%S)",getIndexTypeSetString(string,indexTypeSet));
  }
  else
  {
    filterAppend(filterString,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);  // Note: use entries.storageId instead of storage.id: this is must faster
    filterAppend(filterString,indexTypeSet != INDEX_TYPE_SET_ANY_ENTRY,"AND","entries.type IN (%S)",getIndexTypeSetString(string,indexTypeSet));
  }

  // get sort mode, ordering
  if (newestOnly)
  {
    appendOrdering(orderString,sortMode != INDEX_ENTRY_SORT_MODE_NONE,INDEX_ENTRY_NEWEST_SORT_MODE_COLUMNS[sortMode],ordering);
  }
  else
  {
    appendOrdering(orderString,sortMode != INDEX_ENTRY_SORT_MODE_NONE,INDEX_ENTRY_SORT_MODE_COLUMNS[sortMode],ordering);
  }

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  if      (String_isEmpty(ftsName) && String_isEmpty(entryIdsString))
  {
    // no names/no entries selected

    if (newestOnly)
    {
      error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT uuids.id, \
                                       uuids.jobUUID, \
                                       entities.id, \
                                       entities.scheduleUUID, \
                                       entities.type, \
                                       storage.id, \
                                       storage.name, \
                                       UNIXTIMESTAMP(storage.created), \
                                       entriesNewest.entryId, \
                                       entriesNewest.type, \
                                       entriesNewest.name, \
                                       entriesNewest.timeLastChanged, \
                                       entriesNewest.userId, \
                                       entriesNewest.groupId, \
                                       entriesNewest.permission, \
                                       fileEntries.size, \
                                       fileEntries.fragmentOffset, \
                                       fileEntries.fragmentSize, \
                                       imageEntries.size, \
                                       imageEntries.fileSystemType, \
                                       imageEntries.blockSize, \
                                       imageEntries.blockOffset, \
                                       imageEntries.blockCount, \
                                       directoryEntries.totalEntrySizeNewest, \
                                       linkEntries.destinationName, \
                                       hardlinkEntries.size \
                                FROM entriesNewest \
                                  LEFT JOIN storage ON storage.id=entriesNewest.storageId \
                                  LEFT JOIN entities ON entities.id=storage.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  LEFT JOIN fileEntries ON fileEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN imageEntries ON imageEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN linkEntries ON linkEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entriesNewest.entryId \
                                WHERE %S \
                                %S \
                                LIMIT %llu,%llu; \
                               ",
                               filterString,
                               orderString,
                               offset,
                               limit
                              );
    }
    else
    {
      error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT uuids.id, \
                                       uuids.jobUUID, \
                                       entities.id, \
                                       entities.scheduleUUID, \
                                       entities.type, \
                                       storage.id, \
                                       storage.name, \
                                       UNIXTIMESTAMP(storage.created), \
                                       entries.id, \
                                       entries.type, \
                                       entries.name, \
                                       entries.timeLastChanged, \
                                       entries.userId, \
                                       entries.groupId, \
                                       entries.permission, \
                                       fileEntries.size, \
                                       fileEntries.fragmentOffset, \
                                       fileEntries.fragmentSize, \
                                       imageEntries.size, \
                                       imageEntries.fileSystemType, \
                                       imageEntries.blockSize, \
                                       imageEntries.blockOffset, \
                                       imageEntries.blockCount, \
                                       directoryEntries.totalEntrySize, \
                                       linkEntries.destinationName, \
                                       hardlinkEntries.size \
                                FROM entries \
                                  LEFT JOIN storage ON storage.id=entries.storageId \
                                  LEFT JOIN entities ON entities.id=storage.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                                  LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                  LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                                  LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                                WHERE %S \
                                %S \
                                LIMIT %llu,%llu; \
                               ",
                               filterString,
                               orderString,
                               offset,
                               limit
                              );
    }
  }
  else if (String_isEmpty(ftsName) && !String_isEmpty(entryIdsString))
  {
    // entries selected

    // get additional filters
    if (newestOnly)
    {
//TODO: use entriesNewest.entryId?
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
    }
    else
    {
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
    }

    if (newestOnly)
    {
      error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT uuids.id, \
                                       uuids.jobUUID, \
                                       entities.id, \
                                       entities.scheduleUUID, \
                                       entities.type, \
                                       storage.id, \
                                       storage.name, \
                                       UNIXTIMESTAMP(storage.created), \
                                       entriesNewest.entryId, \
                                       entriesNewest.type, \
                                       entriesNewest.name, \
                                       entriesNewest.timeLastChanged, \
                                       entriesNewest.userId, \
                                       entriesNewest.groupId, \
                                       entriesNewest.permission, \
                                       fileEntries.size, \
                                       fileEntries.fragmentOffset, \
                                       fileEntries.fragmentSize, \
                                       imageEntries.size, \
                                       imageEntries.fileSystemType, \
                                       imageEntries.blockSize, \
                                       imageEntries.blockOffset, \
                                       imageEntries.blockCount, \
                                       directoryEntries.totalEntrySizeNewest, \
                                       linkEntries.destinationName, \
                                       hardlinkEntries.size \
                                FROM entriesNewest \
                                  LEFT JOIN storage ON storage.id=entriesNewest.storageId \
                                  LEFT JOIN entities ON entities.id=storage.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  LEFT JOIN fileEntries ON fileEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN imageEntries ON imageEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN linkEntries ON linkEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entriesNewest.entryId \
                                WHERE     entriesNewest.id IS NOT NULL \
                                      AND %S \
                                %S \
                                LIMIT %llu,%llu; \
                               ",
                               filterString,
                               orderString,
                               offset,
                               limit
                              );
    }
    else
    {
      error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT uuids.id, \
                                       uuids.jobUUID, \
                                       entities.id, \
                                       entities.scheduleUUID, \
                                       entities.type, \
                                       storage.id, \
                                       storage.name, \
                                       UNIXTIMESTAMP(storage.created), \
                                       entries.id, \
                                       entries.type, \
                                       entries.name, \
                                       entries.timeLastChanged, \
                                       entries.userId, \
                                       entries.groupId, \
                                       entries.permission, \
                                       fileEntries.size, \
                                       fileEntries.fragmentOffset, \
                                       fileEntries.fragmentSize, \
                                       imageEntries.size, \
                                       imageEntries.fileSystemType, \
                                       imageEntries.blockSize, \
                                       imageEntries.blockOffset, \
                                       imageEntries.blockCount, \
                                       directoryEntries.totalEntrySize, \
                                       linkEntries.destinationName, \
                                       hardlinkEntries.size \
                                FROM entries \
                                  LEFT JOIN storage ON storage.id=entries.storageId \
                                  LEFT JOIN entities ON entities.id=storage.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                                  LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                  LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                                  LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                                WHERE %S \
                                %S \
                                LIMIT %llu,%llu; \
                               ",
                               filterString,
                               orderString,
                               offset,
                               limit
                              );
    }
  }
  else /* if (!String_isEmpty(ftsName) && String_isEmpty(entryIdsString)) */
  {
    // names (and optional entries) selected

    // get additional filters
    filterAppend(filterString,!String_isEmpty(ftsName),"AND","FTS_entries MATCH %S",ftsName);
    if (newestOnly)
    {
//      filterAppend(filterString,!String_isEmpty(regexpName),"AND","REGEXP(%S,0,entries.name)",regexpString);
//TODO: use entriesNewest.entryId?
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entriesNewest.entryId IN (%S)",entryIdsString);
    }
    else
    {
//      filterAppend(filterString,!String_isEmpty(regexpName),"AND","REGEXP(%S,0,entries.name)",regexpString);
      filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);
    }

    if (newestOnly)
    {
      error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT uuids.id, \
                                       uuids.jobUUID, \
                                       entities.id, \
                                       entities.scheduleUUID, \
                                       entities.type, \
                                       storage.id, \
                                       storage.name, \
                                       UNIXTIMESTAMP(storage.created), \
                                       entriesNewest.entryId, \
                                       entriesNewest.type, \
                                       entriesNewest.name, \
                                       entriesNewest.timeLastChanged, \
                                       entriesNewest.userId, \
                                       entriesNewest.groupId, \
                                       entriesNewest.permission, \
                                       fileEntries.size, \
                                       fileEntries.fragmentOffset, \
                                       fileEntries.fragmentSize, \
                                       imageEntries.size, \
                                       imageEntries.fileSystemType, \
                                       imageEntries.blockSize, \
                                       imageEntries.blockOffset, \
                                       imageEntries.blockCount, \
                                       directoryEntries.totalEntrySizeNewest, \
                                       linkEntries.destinationName, \
                                       hardlinkEntries.size \
                                FROM FTS_entries \
                                  LEFT JOIN entriesNewest ON entriesNewest.entryId=FTS_entries.entryId \
                                  LEFT JOIN storage ON storage.id=entriesNewest.storageId \
                                  LEFT JOIN entities ON entities.id=storage.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  LEFT JOIN fileEntries ON fileEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN imageEntries ON imageEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN linkEntries ON linkEntries.entryId=entriesNewest.entryId \
                                  LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entriesNewest.entryId \
                                WHERE     entriesNewest.id IS NOT NULL \
                                      AND %S \
                                %S \
                                LIMIT %llu,%llu; \
                               ",
                               filterString,
                               orderString,
                               offset,
                               limit
                              );
    }
    else
    {
      error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                               &indexHandle->databaseHandle,
                               "SELECT uuids.id, \
                                       uuids.jobUUID, \
                                       entities.id, \
                                       entities.scheduleUUID, \
                                       entities.type, \
                                       storage.id, \
                                       storage.name, \
                                       UNIXTIMESTAMP(storage.created), \
                                       entries.id, \
                                       entries.type, \
                                       entries.name, \
                                       entries.timeLastChanged, \
                                       entries.userId, \
                                       entries.groupId, \
                                       entries.permission, \
                                       fileEntries.size, \
                                       fileEntries.fragmentOffset, \
                                       fileEntries.fragmentSize, \
                                       imageEntries.size, \
                                       imageEntries.fileSystemType, \
                                       imageEntries.blockSize, \
                                       imageEntries.blockOffset, \
                                       imageEntries.blockCount, \
                                       directoryEntries.totalEntrySize, \
                                       linkEntries.destinationName, \
                                       hardlinkEntries.size \
                                FROM FTS_entries \
                                  LEFT JOIN entries ON entries.id=FTS_entries.entryId \
                                  LEFT JOIN storage ON storage.id=entries.storageId \
                                  LEFT JOIN entities ON entities.id=storage.entityId \
                                  LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID \
                                  LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                                  LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                                  LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                                  LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                                  LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                                WHERE %S \
                                %S \
                                LIMIT %llu,%llu; \
                               ",
                               filterString,
                               orderString,
                               offset,
                               limit
                              );
    }
  }
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(string);
    String_delete(orderString);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(entityIdString);
    String_delete(uuidIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);

  // free resources
  String_delete(string);
  String_delete(orderString);
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(entityIdString);
  String_delete(uuidIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextEntry(IndexQueryHandle  *indexQueryHandle,
                        IndexId           *uuidId,
                        String            jobUUID,
                        IndexId           *entityId,
                        String            scheduleUUID,
                        ArchiveTypes      *archiveType,
                        IndexId           *storageId,
                        String            storageName,
                        uint64            *storageDateTime,
                        IndexId           *entryId,
                        String            entryName,
                        String            destinationName,
                        FileSystemTypes   *fileSystemType,
                        uint64            *size,
                        uint64            *timeModified,
                        uint32            *userId,
                        uint32            *groupId,
                        uint32            *permission,
                        uint64            *fragmentOrBlockOffset,
                        uint64            *fragmentSizeOrBlockCount
                       )
{
  IndexTypes indexType;
  DatabaseId uuidId_,entityId_,entryId_,storageId_;
  int64      fileSize_,imageSize_,hardlinkSize_;
  int64      fragmentOffset_,fragmentSize_;
  int64      blockOffset_,blockCount_;
  int64      directorySize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %d %llu %S %llu %llu %d %S %llu %d %d %d %llu %llu %llu %llu %d %llu %llu %llu %llu %S %llu",
                           &uuidId_,
                           jobUUID,
                           &entityId_,
                           scheduleUUID,
                           archiveType,
                           &storageId_,
                           storageName,
                           storageDateTime,
                           &entryId_,
                           &indexType,
                           entryName,
                           timeModified,
                           userId,
                           groupId,
                           permission,
                           &fileSize_,
                           &fragmentOffset_,
                           &fragmentSize_,
                           &imageSize_,
                           fileSystemType,
                           NULL,  // imageEntryBlockSize,
                           &blockOffset_,
                           &blockCount_,
                           &directorySize_,
                           destinationName,
                           &hardlinkSize_
                          )
     )
  {
    return FALSE;
  }
  assert(fileSize_ >= 0LL);
  assert(fragmentOffset_ >= 0LL);
  assert(fragmentSize_ >= 0LL);
  assert(imageSize_ >= 0LL);
  assert(blockOffset_ >= 0LL);
  assert(blockCount_ >= 0LL);
//TODO: may happen
//  assert(directorySize_ >= 0LL);
  assert(hardlinkSize_ >= 0LL);
  if (uuidId    != NULL) (*uuidId   ) = INDEX_ID_(INDEX_TYPE_UUID,   uuidId_   );
  if (entityId  != NULL) (*entityId ) = INDEX_ID_(INDEX_TYPE_ENTITY, entityId_ );
  if (storageId != NULL) (*storageId) = INDEX_ID_(INDEX_TYPE_STORAGE,storageId_);
  if (entryId   != NULL) (*entryId  ) = INDEX_ID_(indexType,         entryId_  );
  if (size != NULL)
  {
    switch (indexType)
    {
      case INDEX_TYPE_FILE:      (*size) = fileSize_;                                      break;
      case INDEX_TYPE_IMAGE:     (*size) = imageSize_;                                     break;
      case INDEX_TYPE_DIRECTORY: (*size) = (directorySize_ >= 0LL) ? directorySize_ : 0LL; break;
      case INDEX_TYPE_HARDLINK:  (*size) = hardlinkSize_;                                  break;
      default:                   (*size) = 0LL;                                            break;
    }
  }
  if (fragmentOrBlockOffset != NULL)
  {
    switch (indexType)
    {
      case INDEX_TYPE_FILE:  (*fragmentOrBlockOffset) = fragmentOffset_; break;
      case INDEX_TYPE_IMAGE: (*fragmentOrBlockOffset) = blockOffset_;    break;
      default:               (*fragmentOrBlockOffset) = 0LL;             break;
    }
  }
  if (fragmentSizeOrBlockCount != NULL)
  {
    switch (indexType)
    {
      case INDEX_TYPE_FILE:  (*fragmentSizeOrBlockCount) = fragmentSize_; break;
      case INDEX_TYPE_IMAGE: (*fragmentSizeOrBlockCount) = blockCount_;   break;
      default:               (*fragmentSizeOrBlockCount) = 0LL;           break;
    }
  }

  return TRUE;
}

Errors Index_deleteEntry(IndexHandle *indexHandle,
                         IndexId     entryId
                        )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(   (Index_getType(entryId) == INDEX_TYPE_FILE)
         || (Index_getType(entryId) == INDEX_TYPE_IMAGE)
         || (Index_getType(entryId) == INDEX_TYPE_DIRECTORY)
         || (Index_getType(entryId) == INDEX_TYPE_LINK)
         || (Index_getType(entryId) == INDEX_TYPE_HARDLINK)
         || (Index_getType(entryId) == INDEX_TYPE_SPECIAL)
        );

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    switch (Index_getType(entryId))
    {
      case INDEX_TYPE_FILE:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM fileEntries WHERE entryId=%lld;",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_IMAGE:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM imageEntries WHERE entryId=%lld;",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_DIRECTORY:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM directoryEntries WHERE entryId=%lld;",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_LINK:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM linkEntries WHERE entryId=%lld;",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_HARDLINK:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM hardlinkEntries WHERE entryId=%lld;",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      case INDEX_TYPE_SPECIAL:
        error = Database_execute(&indexHandle->databaseHandle,
                                 CALLBACK(NULL,NULL),  // databaseRowFunction
                                 NULL,  // changedRowCount
                                 "DELETE FROM specialEntries WHERE entryId=%lld;",
                                 Index_getDatabaseId(entryId)
                                );
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* not NDEBUG */
        break;
    }
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(entryId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(entryId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalFileCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalFileSize<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalImageCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalImageSize<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalDirectoryCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalLinkCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalHardlinkCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalHardlinkSize<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalSpecialCount<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}

Errors Index_initListFiles(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    storageIds[],
                           uint             storageIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      name
                          )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  regexpName   = String_new();
  filterString = String_new();

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_FILE)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_FILE);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filterString,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,entries.name)",regexpString);
  filterAppend(filterString,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   UNIXTIMESTAMP(storage.created), \
                                   entries.name, \
                                   entries.size, \
                                   entries.timeModified, \
                                   entries.userId, \
                                   entries.groupId, \
                                   entries.permission, \
                                   fileEntries.fragmentOffset, \
                                   fileEntries.fragmentSize \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=entries.storageId \
                              LEFT JOIN fileEntries ON fileEntries.entryId=entries.id \
                            WHERE %S \
                           ",
                           filterString
                          );
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextFile(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       String           storageName,
                       uint64           *storageDateTime,
                       String           fileName,
                       uint64           *size,
                       uint64           *timeModified,
                       uint32           *userId,
                       uint32           *groupId,
                       uint32           *permission,
                       uint64           *fragmentOffset,
                       uint64           *fragmentSize
                      )
{
  DatabaseId databaseId;
  int64      fragmentOffset_,fragmentSize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %llu %llu %d %d %d %llu %llu",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           fileName,
                           size,
                           timeModified,
                           userId,
                           groupId,
                           permission,
                           &fragmentOffset_,
                           &fragmentSize_
                          )
     )
  {
    return FALSE;
  }
  assert(fragmentOffset_ >= 0LL);
  assert(fragmentSize_ >= 0LL);
  if (indexId != NULL) (*indexId) = INDEX_ID_FILE(databaseId);
  if (fragmentOffset != NULL) (*fragmentOffset) = (fragmentOffset_ >= 0LL) ? fragmentOffset_ : 0LL;
  if (fragmentSize != NULL) (*fragmentSize) = (fragmentSize_ >= 0LL) ? fragmentSize_ : 0LL;

  return TRUE;
}

#if 0
//TODO: obsolete
Errors Index_deleteFile(IndexHandle *indexHandle,
                        IndexId     indexId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_FILE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM fileEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalFileCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalFileSize<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

Errors Index_initListImages(IndexQueryHandle *indexQueryHandle,
                            IndexHandle      *indexHandle,
                            const IndexId    storageIds[],
                            uint             storageIdCount,
                            const IndexId    entryIds[],
                            uint             entryIdCount,
                            ConstString      name
                           )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  regexpName   = String_new();
  filterString = String_new();

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_IMAGE)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filterString,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,entries.name)",regexpString);
  filterAppend(filterString,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   UNIXTIMESTAMP(storage.created), \
                                   entries.name, \
                                   imageEntries.fileSystemType, \
                                   entries.size, \
                                   imageEntries.blockOffset, \
                                   imageEntries.blockCount \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=images.storageId \
                              LEFT JOIN imageEntries ON imageEntries.entryId=entries.id \
                            WHERE %S \
                           ",
                           filterString
                          );
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextImage(IndexQueryHandle *indexQueryHandle,
                        IndexId          *indexId,
                        String           storageName,
                        uint64           *storageDateTime,
                        String           imageName,
                        FileSystemTypes  *fileSystemType,
                        uint64           *size,
                        uint64           *blockOffset,
                        uint64           *blockCount
                       )
{
  DatabaseId databaseId;
  int64      blockOffset_,blockCount_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %u %llu %llu %llu",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           imageName,
                           fileSystemType,
                           size,
                           &blockOffset_,
                           &blockCount_
                          )
     )
  {
    return FALSE;
  }
  assert(blockOffset_ >= 0LL);
  assert(blockCount_ >= 0LL);
  if (indexId != NULL) (*indexId) = INDEX_ID_IMAGE(databaseId);
  if (blockOffset != NULL) (*blockOffset) = (blockOffset_ >= 0LL) ? blockOffset_ : 0LL;
  if (blockCount != NULL) (*blockCount) = (blockCount_ >= 0LL) ? blockCount_ : 0LL;

  return TRUE;
}

#if 0
//TODO: obsolete
Errors Index_deleteImage(IndexHandle *indexHandle,
                         IndexId     indexId
                        )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_IMAGE);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM imageEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalImageCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalImageSize<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

Errors Index_initListDirectories(IndexQueryHandle *indexQueryHandle,
                                 IndexHandle      *indexHandle,
                                 const IndexId    storageIds[],
                                 uint             storageIdCount,
                                 const IndexId    entryIds[],
                                 uint             entryIdCount,
                                 ConstString      name
                                )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  regexpName   = String_new();
  filterString = String_new();

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_DIRECTORY)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filterString,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,entries.name)",regexpString);
  filterAppend(filterString,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
//Database_debugEnable(1);
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   UNIXTIMESTAMP(storage.created), \
                                   entries.name, \
                                   entries.timeModified, \
                                   entries.userId, \
                                   entries.groupId, \
                                   entries.permission \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=entries.storageId \
                              LEFT JOIN directoryEntries ON directoryEntries.entryId=entries.id \
                            WHERE %S \
                           ",
                           filterString
                          );
//Database_debugEnable(0);
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextDirectory(IndexQueryHandle *indexQueryHandle,
                            IndexId          *indexId,
                            String           storageName,
                            uint64           *storageDateTime,
                            String           directoryName,
                            uint64           *timeModified,
                            uint32           *userId,
                            uint32           *groupId,
                            uint32           *permission
                           )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %llu %d %d %d",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           directoryName,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_DIRECTORY(databaseId);

  return TRUE;
}

#if 0
//TODO: obsolete
Errors Index_deleteDirectory(IndexHandle *indexHandle,
                             IndexId     indexId
                            )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_DIRECTORY);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM directoryEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalDirectoryCount<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

Errors Index_initListLinks(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    storageIds[],
                           uint             storageIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      name
                          )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  regexpName   = String_new();
  filterString = String_new();

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_LINK)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }
  String_appendCString(entryIdsString,"))");

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filterString,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,entries.name)",regexpString);
  filterAppend(filterString,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   UNIXTIMESTAMP(storage.created), \
                                   entries.name, \
                                   linkEntries.destinationName, \
                                   entries.timeModified, \
                                   entries.userId, \
                                   entries.groupId, \
                                   entries.permission \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=links.storageId \
                              LEFT JOIN linkEntries ON linkEntries.entryId=entries.id \
                            WHERE %S \
                           ",
                           filterString
                          );
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextLink(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       String           storageName,
                       uint64           *storageDateTime,
                       String           linkName,
                       String           destinationName,
                       uint64           *timeModified,
                       uint32           *userId,
                       uint32           *groupId,
                       uint32           *permission
                      )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %S %llu %d %d %d",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           linkName,
                           destinationName,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_LINK(databaseId);

  return TRUE;
}

#if 0
//TODO: obsolete
Errors Index_deleteLink(IndexHandle *indexHandle,
                        IndexId     indexId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_LINK);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM linkEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalLinkCount<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

Errors Index_initListHardLinks(IndexQueryHandle *indexQueryHandle,
                               IndexHandle      *indexHandle,
                               const IndexId    storageIds[],
                               uint             storageIdCount,
                               const IndexId    entryIds[],
                               uint             entryIdCount,
                               ConstString      name
                              )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  regexpName   = String_new();
  filterString = String_new();

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get id sets
  storageIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  entryIdsString = String_new();
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_HARDLINK)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filterString,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,entries.name)",regexpString);
  filterAppend(filterString,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   UNIXTIMESTAMP(storage.created), \
                                   entries.name, \
                                   entries.size, \
                                   entries.timeModified, \
                                   entries.userId, \
                                   entries.groupId, \
                                   entries.permission, \
                                   hardlinkEntries.fragmentOffset, \
                                   hardlinkEntries.fragmentSize \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=hardlinks.storageId \
                              LEFT JOIN hardlinkEntries ON hardlinkEntries.entryId=entries.id \
                            WHERE %S \
                           ",
                           filterString
                          );
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextHardLink(IndexQueryHandle *indexQueryHandle,
                           IndexId          *indexId,
                           String           storageName,
                           uint64           *storageDateTime,
                           String           fileName,
                           uint64           *size,
                           uint64           *timeModified,
                           uint32           *userId,
                           uint32           *groupId,
                           uint32           *permission,
                           uint64           *fragmentOffset,
                           uint64           *fragmentSize
                          )
{
  DatabaseId databaseId;
  int64      fragmentOffset_,fragmentSize_;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %llu %llu %d %d %d %llu %llu",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           fileName,
                           size,
                           timeModified,
                           userId,
                           groupId,
                           permission,
                           &fragmentOffset_,
                           &fragmentSize_
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_HARDLINK(databaseId);
  assert(fragmentOffset_ >= 0LL);
  assert(fragmentSize_ >= 0LL);
  if (fragmentOffset != NULL) (*fragmentOffset) = (fragmentOffset_ >= 0LL) ? fragmentOffset_ : 0LL;
  if (fragmentSize != NULL) (*fragmentSize) = (fragmentSize_ >= 0LL) ? fragmentSize_ : 0LL;

  return TRUE;
}

#if 0
//TODO: obsolete
Errors Index_deleteHardLink(IndexHandle *indexHandle,
                            IndexId     indexId
                           )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_HARDLINK);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM hardlinkEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalHardlinkCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalHardlinkSize<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

Errors Index_initListSpecial(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const IndexId    storageIds[],
                             uint             storageIdCount,
                             const IndexId    entryIds[],
                             uint             entryIdCount,
                             ConstString      name
                            )
{
  String ftsName;
  String regexpName;
  String storageIdsString;
  String entryIdsString;
  uint   i;
  String filterString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert((storageIdCount == 0) || (storageIds != NULL));
  assert((entryIdCount == 0) || (entryIds != NULL));

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  // init variables
  ftsName      = String_new();
  regexpName   = String_new();
  filterString = String_new();

  // get FTS/regex patterns
  getFTSString(ftsName,name);
  getREGEXPString(regexpName,name);

  // get id sets
  storageIdsString = String_new();
  entryIdsString = String_new();
  for (i = 0; i < storageIdCount; i++)
  {
    assert(Index_getType(storageIds[i]) == INDEX_TYPE_STORAGE);
    if (!String_isEmpty(storageIdsString)) String_appendChar(storageIdsString,',');
    String_format(storageIdsString,"%lld",Index_getDatabaseId(storageIds[i]));
  }
  for (i = 0; i < entryIdCount; i++)
  {
    if (Index_getType(entryIds[i]) == INDEX_TYPE_SPECIAL)
    {
      if (!String_isEmpty(entryIdsString)) String_appendChar(entryIdsString,',');
      String_format(entryIdsString,"%lld",Index_getDatabaseId(entryIds[i]));
    }
  }

  // get filters
  filterAppend(filterString,TRUE,"AND","entries.type=%d",INDEX_TYPE_DIRECTORY);
  filterAppend(filterString,!String_isEmpty(ftsName),"AND","entries.id IN (SELECT entryId FROM FTS_entries WHERE FTS_entries MATCH %S)",ftsName);
//  filterAppend(filterString,!String_isEmpty(regexpString),"AND","REGEXP(%S,0,special.name)",regexpString);
  filterAppend(filterString,!String_isEmpty(storageIdsString),"AND","entries.storageId IN (%S)",storageIdsString);
  filterAppend(filterString,!String_isEmpty(entryIdsString),"AND","entries.id IN (%S)",entryIdsString);

  // lock
  Database_lock(&indexHandle->databaseHandle);

  // prepare list
  initIndexQueryHandle(indexQueryHandle,indexHandle);
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entries.id, \
                                   storage.name, \
                                   UNIXTIMESTAMP(storage.created), \
                                   entries.name, \
                                   entries.timeModified, \
                                   entries.userId, \
                                   entries.groupId, \
                                   entries.permission \
                            FROM entries \
                              LEFT JOIN storage ON storage.id=special.storageId \
                              LEFT JOIN specialEntries ON specialEntries.entryId=entries.id \
                            WHERE %S \
                           ",
                           filterString
                          );
  if (error != ERROR_NONE)
  {
    doneIndexQueryHandle(indexQueryHandle);
    Database_unlock(&indexHandle->databaseHandle);
    String_delete(filterString);
    String_delete(entryIdsString);
    String_delete(storageIdsString);
    String_delete(regexpName);
    String_delete(ftsName);
    return error;
  }

  // free resources
  String_delete(filterString);
  String_delete(entryIdsString);
  String_delete(storageIdsString);
  String_delete(regexpName);
  String_delete(ftsName);

  DEBUG_ADD_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  return ERROR_NONE;
}

bool Index_getNextSpecial(IndexQueryHandle *indexQueryHandle,
                          IndexId          *indexId,
                          String           storageName,
                          uint64           *storageDateTime,
                          String           name,
                          uint64           *timeModified,
                          uint32           *userId,
                          uint32           *groupId,
                          uint32           *permission
                         )
{
  DatabaseId databaseId;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);

  // check init error
  if (indexQueryHandle->indexHandle->upgradeError != ERROR_NONE)
  {
    return FALSE;
  }

  if (!Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                           "%lld %S %llu %S %llu %d %d %d",
                           &databaseId,
                           storageName,
                           storageDateTime,
                           name,
                           timeModified,
                           userId,
                           groupId,
                           permission
                          )
     )
  {
    return FALSE;
  }
  if (indexId != NULL) (*indexId) = INDEX_ID_SPECIAL(databaseId);

  return TRUE;
}

#if 0
//TODO: obsolete
Errors Index_deleteSpecial(IndexHandle *indexHandle,
                           IndexId     indexId
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(indexId) == INDEX_TYPE_SPECIAL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM specialEntries WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entriesNewest WHERE entryId=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM entries WHERE id=%lld;",
                             Index_getDatabaseId(indexId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    #ifndef NDEBUG
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntrySize<0");
      verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalSpecialCount<0");

      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
      verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
    #endif /* not NDEBUG */

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}
#endif

void Index_doneList(IndexQueryHandle *indexQueryHandle)
{
  assert(indexQueryHandle != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(indexQueryHandle,sizeof(IndexQueryHandle));

  Database_finalize(&indexQueryHandle->databaseQueryHandle);
  doneIndexQueryHandle(indexQueryHandle);
  Database_unlock(&indexQueryHandle->indexHandle->databaseHandle);
}

Errors Index_addFile(IndexHandle *indexHandle,
                     IndexId     storageId,
                     ConstString fileName,
                     uint64      size,
                     uint64      timeLastAccess,
                     uint64      timeModified,
                     uint64      timeLastChanged,
                     uint32      userId,
                     uint32      groupId,
                     uint32      permission,
                     uint64      fragmentOffset,
                     uint64      fragmentSize
                    )
{
  Errors  error;
  IndexId entryId;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(fileName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entries \
                                  ( \
                                   storageId, \
                                   type, \
                                   name, \
                                   timeLastAccess, \
                                   timeModified, \
                                   timeLastChanged, \
                                   userId, \
                                   groupId, \
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               INDEX_TYPE_FILE,
                               fileName,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = INDEX_ID_ENTRY(Database_getLastRowId(&indexHandle->databaseHandle));

      // add file entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO fileEntries \
                                  ( \
                                   storageId, \
                                   entryId, \
                                   size, \
                                   fragmentOffset, \
                                   fragmentSize\
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %llu, \
                                   %llu, \
                                   %llu\
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               Index_getDatabaseId(entryId),
                               size,
                               fragmentOffset,
                               fragmentSize
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               storageId,
                                               entryId,
                                               fileName,
                                               size
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntrySize<0");
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalFileCount<0");
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalFileSize<0");

        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    NULL,  // resultMap
                                    "INDEX_ADD_FILE storageId=%llu fileName=%'S size=%llu timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o fragmentOffset=%llu fragmentSize=%llu",
                                    storageId,
                                    fileName,
                                    size,
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission,
                                    fragmentOffset,
                                    fragmentSize
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_addImage(IndexHandle     *indexHandle,
                      IndexId         storageId,
                      ConstString     imageName,
                      FileSystemTypes fileSystemType,
                      int64           size,
                      ulong           blockSize,
                      uint64          blockOffset,
                      uint64          blockCount
                     )
{
  Errors     error;
  DatabaseId entryId;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(imageName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entries \
                                  ( \
                                   storageId, \
                                   type, \
                                   name, \
                                   timeLastAccess, \
                                   timeModified, \
                                   timeLastChanged, \
                                   userId, \
                                   groupId, \
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               INDEX_TYPE_IMAGE,
                               imageName,
                               0LL,
                               0LL,
                               0LL,
                               0,
                               0,
                               0
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = Database_getLastRowId(&indexHandle->databaseHandle);

      // add image entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO imageEntries \
                                  ( \
                                   storageId, \
                                   entryId, \
                                   size, \
                                   fragmentOffset, \
                                   fragmentSize\
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %d, \
                                   %llu, \
                                   %u, \
                                   %llu, \
                                   %llu\
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               entryId,
                               fileSystemType,
                               size,
                               blockSize,
                               blockOffset,
                               blockCount
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE id=%lld AND totalEntrySize<0",Index_getDatabaseId(storageId));
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE id=%lld AND totalFileSize<0",Index_getDatabaseId(storageId));
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    NULL,  // resultMap
                                    "INDEX_ADD_IMAGE storageId=%llu type=IMAGE name=%'S size=%llu blockSize=%lu blockOffset=%llu blockCount=%llu",
                                    storageId,
                                    imageName,
                                    size,
                                    blockSize,
                                    blockOffset,
                                    blockCount
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_addDirectory(IndexHandle *indexHandle,
                          IndexId     storageId,
                          String      directoryName,
                          uint64      timeLastAccess,
                          uint64      timeModified,
                          uint64      timeLastChanged,
                          uint32      userId,
                          uint32      groupId,
                          uint32      permission
                         )
{
  Errors  error;
  IndexId entryId;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(directoryName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entries \
                                  ( \
                                   storageId, \
                                   type, \
                                   name, \
                                   timeLastAccess, \
                                   timeModified, \
                                   timeLastChanged, \
                                   userId, \
                                   groupId, \
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               INDEX_TYPE_DIRECTORY,
                               directoryName,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = INDEX_ID_ENTRY(Database_getLastRowId(&indexHandle->databaseHandle));

      // add directory entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO directoryEntries \
                                  ( \
                                   storageId, \
                                   entryId, \
                                   name \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %'S\
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               Index_getDatabaseId(entryId),
                               directoryName
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               storageId,
                                               entryId,
                                               directoryName,
                                               0LL
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalDirectoryCount<0");

        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    NULL,  // resultMap
                                    "INDEX_ADD_DIRECTORY storageId=%llu type=DIRECTORY name=%'S timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o",
                                    storageId,
                                    directoryName,
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_addLink(IndexHandle *indexHandle,
                     IndexId     storageId,
                     ConstString linkName,
                     ConstString destinationName,
                     uint64      timeLastAccess,
                     uint64      timeModified,
                     uint64      timeLastChanged,
                     uint32      userId,
                     uint32      groupId,
                     uint32      permission
                    )
{
  Errors  error;
  IndexId entryId;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(linkName != NULL);
  assert(destinationName != NULL);

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entries \
                                  ( \
                                   storageId, \
                                   type, \
                                   name, \
                                   timeLastAccess, \
                                   timeModified, \
                                   timeLastChanged, \
                                   userId, \
                                   groupId, \
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               INDEX_TYPE_LINK,
                               linkName,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = INDEX_ID_ENTRY(Database_getLastRowId(&indexHandle->databaseHandle));

      // add link entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO linkEntries \
                                  ( \
                                   storageId, \
                                   entryId, \
                                   destinationName \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %'S \
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               Index_getDatabaseId(entryId),
                               destinationName
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               storageId,
                                               entryId,
                                               linkName,
                                               0LL
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    NULL,  // resultMap
                                    "INDEX_ADD_LINK storageId=%llu type=LINK name=%'S destinationName=%'S timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o",
                                    storageId,
                                    linkName,
                                    destinationName,
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_addHardlink(IndexHandle *indexHandle,
                         IndexId     storageId,
                         ConstString fileName,
                         uint64      size,
                         uint64      timeLastAccess,
                         uint64      timeModified,
                         uint64      timeLastChanged,
                         uint32      userId,
                         uint32      groupId,
                         uint32      permission,
                         uint64      fragmentOffset,
                         uint64      fragmentSize
                        )
{
  Errors  error;
  IndexId entryId;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(fileName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entries \
                                  ( \
                                   storageId, \
                                   type, \
                                   name, \
                                   timeLastAccess, \
                                   timeModified, \
                                   timeLastChanged, \
                                   userId, \
                                   groupId, \
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               INDEX_TYPE_HARDLINK,
                               fileName,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = INDEX_ID_ENTRY(Database_getLastRowId(&indexHandle->databaseHandle));

      // add hard link entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO hardlinkEntries \
                                  ( \
                                   storageId, \
                                   entryId, \
                                   size, \
                                   fragmentOffset, \
                                   fragmentSize\
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %llu, \
                                   %llu, \
                                   %llu\
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               Index_getDatabaseId(entryId),
                               size,
                               fragmentOffset,
                               fragmentSize
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               storageId,
                                               entryId,
                                               fileName,
                                               size
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      #ifndef NDEBUG
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntrySize<0");
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalFileCount<0");
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalFileSize<0");

        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    NULL,  // resultMap
                                    "INDEX_ADD_HARDLINK storageId=%llu type=HARDLINK name=%'S size=%llu timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o fragmentOffset=%llu fragmentSize=%llu",
                                    storageId,
                                    fileName,
                                    size,
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission,
                                    fragmentOffset,
                                    fragmentSize
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_addSpecial(IndexHandle      *indexHandle,
                        IndexId          storageId,
                        ConstString      name,
                        FileSpecialTypes specialType,
                        uint64           timeLastAccess,
                        uint64           timeModified,
                        uint64           timeLastChanged,
                        uint32           userId,
                        uint32           groupId,
                        uint32           permission,
                        uint32           major,
                        uint32           minor
                       )
{
  Errors  error;
  IndexId entryId;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(name != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      // add entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO entries \
                                  ( \
                                   storageId, \
                                   type, \
                                   name, \
                                   timeLastAccess, \
                                   timeModified, \
                                   timeLastChanged, \
                                   userId, \
                                   groupId, \
                                   permission \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %d, \
                                   %'S, \
                                   %llu, \
                                   %llu, \
                                   %llu, \
                                   %u, \
                                   %u, \
                                   %u \
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               INDEX_TYPE_SPECIAL,
                               name,
                               timeLastAccess,
                               timeModified,
                               timeLastChanged,
                               userId,
                               groupId,
                               permission
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
      entryId = INDEX_ID_ENTRY(Database_getLastRowId(&indexHandle->databaseHandle));

      // add special entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "INSERT INTO specialEntries \
                                  ( \
                                   storageId, \
                                   entryId, \
                                   specialType, \
                                   major, \
                                   minor \
                                  ) \
                                VALUES \
                                  ( \
                                   %lld, \
                                   %lld, \
                                   %d, \
                                   %d, \
                                   %d\
                                  ); \
                               ",
                               Index_getDatabaseId(storageId),
                               Index_getDatabaseId(entryId),
                               specialType,
                               major,
                               minor
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      // update directory content count/size aggregates
      error = updateDirectoryContentAggregates(indexHandle,
                                               storageId,
                                               entryId,
                                               name,
                                               0LL
                                              );
      if (error != ERROR_NONE)
      {
        return error;
      }

      return ERROR_NONE;
    });
  }
  else
  {
    error = ServerIO_executeCommand(indexHandle->masterIO,
                                    SERVER_IO_TIMEOUT,
                                    NULL,  // resultMap
                                    "INDEX_ADD_SPECIAL storageId=%llu type=SPECIAL name=%'S specialType=%u timeLastAccess=%llu timeModified=%llu timeLastChanged=%llu userId=%u groupId=%u permission=%o major=%u minor=%u",
                                    storageId,
                                    name,
                                    specialType,
                                    timeLastAccess,
                                    timeModified,
                                    timeLastChanged,
                                    userId,
                                    groupId,
                                    permission,
                                    major,
                                    minor
                                   );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_assignTo(IndexHandle  *indexHandle,
                      ConstString  jobUUID,
                      IndexId      entityId,
                      IndexId      storageId,
                      ConstString  toJobUUID,
                      IndexId      toEntityId,
                      ArchiveTypes toArchiveType,
                      IndexId      toStorageId
                     )
{
  Errors error;

  assert(indexHandle != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      if      (toJobUUID != NULL)
      {
        if (entityId != INDEX_ID_NONE)
        {
          assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

          // assign entity to other job
          error = assignEntityToJob(indexHandle,
                                    entityId,
                                    toJobUUID,
                                    toArchiveType
                                   );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!String_isEmpty(jobUUID))
        {
          // assign all entities of job to other job
          error = assignJobToJob(indexHandle,
                                 jobUUID,
                                 toJobUUID,
                                 toArchiveType
                                );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
      }
      else if (toEntityId != INDEX_ID_NONE)
      {
        // assign to other entity

        if (storageId != INDEX_ID_NONE)
        {
          assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

          // assign storage to other entity
          error = assignStorageToEntity(indexHandle,
                                        storageId,
                                        toEntityId
                                       );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (entityId != INDEX_ID_NONE)
        {
          assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

          // assign all storage entries of entity to other entity
          error = assignEntityToEntity(indexHandle,
                                       entityId,
                                       toEntityId,
                                       toArchiveType
                                      );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!String_isEmpty(jobUUID))
        {
          // assign all entities of job to other entity
          error = assignJobToEntity(indexHandle,
                                    jobUUID,
                                    toEntityId,
                                    toArchiveType
                                   );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
      }
      else if (toStorageId != INDEX_ID_NONE)
      {
        // assign to other storage

        if (storageId != INDEX_ID_NONE)
        {
          assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

          // assign storage entries to other storage
          error = assignStorageToStorage(indexHandle,storageId,toStorageId);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (entityId != INDEX_ID_NONE)
        {
          assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

          // assign all storage entries of entity to other storage
          error = assignEntityToStorage(indexHandle,entityId,toStorageId);
          if (error != ERROR_NONE)
          {
            return error;
          }
        }

        if (!String_isEmpty(jobUUID))
        {
          // assign all storage entries of all entities of job to other storage
          error = assignJobToStorage(indexHandle,
                                     jobUUID,
                                     toStorageId
                                    );
          if (error != ERROR_NONE)
          {
            return error;
          }
        }
      }

      #ifndef NDEBUG
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalEntrySize<0");
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalFileSize<0");
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalImageSize<0");
        verify(indexHandle,"storage","COUNT(id)",0,"WHERE totalHardlinkSize<0");

        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntryCount<0");
        verify(indexHandle,"directoryEntries","COUNT(id)",0,"WHERE totalEntrySize<0");
      #endif /* not NDEBUG */

      return ERROR_NONE;
    });
  }
  else
  {
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
  }

  return error;
}

Errors Index_pruneUUID(IndexHandle *indexHandle,
                       IndexId     uuidId
                      )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          entityId;
  bool             existsEntityFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(uuidId) == INDEX_TYPE_UUID);

  // prune entities of uuid
  INDEX_DOX(error,
            indexHandle,
  {
    error = Index_initListEntities(&indexQueryHandle,
                                   indexHandle,
                                   uuidId,
                                   NULL,  // jobUUID
                                   NULL,  // scheduldUUID
                                   INDEX_STATE_SET_ALL,
                                   INDEX_MODE_SET_ALL,
                                   NULL,  // name
                                   DATABASE_ORDERING_ASCENDING,
                                   0LL,  // offset
                                   INDEX_UNLIMITED
                                  );
    if (error != ERROR_NONE)
    {
      return error;
    }
    while (Index_getNextEntity(&indexQueryHandle,
                               NULL,  // uuidId,
                               NULL,  // jobUUID,
                               NULL,  // scheduleUUID,
                               &entityId,
                               NULL,  // archiveType,
                               NULL,  // createdDateTime,
                               NULL,  // lastErrorMessage
                               NULL,  // totalEntryCount
                               NULL,  // totalEntrySize
                               NULL  // lockedCount
                              )
          )
    {
      error = Index_pruneEntity(indexHandle,entityId);
      if (error != ERROR_NONE)
      {
        break;
      }
    }
    Index_doneList(&indexQueryHandle);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // check if entity exists
    existsEntityFlag = Database_exists(&indexHandle->databaseHandle,
                                       "entities",
                                       "entities.id",
                                       "LEFT JOIN uuids ON uuids.jobUUID=entities.jobUUID WHERE uuids.id=%lld",
                                       Index_getDatabaseId(uuidId)
                                      );

    // prune uuid if empty
    if (!existsEntityFlag)
    {
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "DELETE FROM uuids WHERE id=%lld;",
                               Index_getDatabaseId(uuidId)
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    return ERROR_NONE;
  });

  return ERROR_NONE;
}

Errors Index_pruneEntity(IndexHandle *indexHandle,
                         IndexId     entityId
                         )
{
  Errors           error;
  IndexQueryHandle indexQueryHandle;
  IndexId          storageId;
  int64            lockedCount;
  bool             existsStorageFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(entityId) == INDEX_TYPE_ENTITY);

  // prune storages of entity
  if (indexHandle->masterIO == NULL)
  {
    INDEX_DOX(error,
              indexHandle,
    {
      error = Index_initListStorages(&indexQueryHandle,
                                     indexHandle,
                                     INDEX_ID_ANY,  // uuidId
                                     entityId,
                                     NULL,  // jobUUID
                                     NULL,  // scheduleUUID,
                                     NULL,   // storageIds
                                     0,  // storageIdCount
                                     INDEX_STATE_SET_ALL,
                                     INDEX_MODE_SET_ALL,
                                     NULL,  // name
                                     INDEX_STORAGE_SORT_MODE_NONE,
                                     DATABASE_ORDERING_NONE,
                                     0LL,   // offset
                                     INDEX_UNLIMITED
                                    );
      if (error != ERROR_NONE)
      {
        return error;
      }
      while (Index_getNextStorage(&indexQueryHandle,
                                  NULL,  // uuidId
                                  NULL,  // jobUUID
                                  NULL,  // entityId
                                  NULL,  // scheduleUUID
                                  NULL,  // archiveType
                                  &storageId,
                                  NULL,  // storageName,
                                  NULL,  // createdDateTime
                                  NULL,  // size,
                                  NULL,  // indexState
                                  NULL,  // indexMode
                                  NULL,  // lastCheckedDateTime
                                  NULL,  // errorMessage
                                  NULL,  // totalEntryCount
                                  NULL  // totalEntrySize
                                 )
            )
      {
        error = Index_pruneStorage(indexHandle,storageId);
        if (error != ERROR_NONE)
        {
          break;
        }
      }
      Index_doneList(&indexQueryHandle);
      if (error != ERROR_NONE)
      {
        return error;
      }

      if (Index_getDatabaseId(entityId) != INDEX_DEFAULT_ENTITY_ID)
      {
        // get locked count
        error = Database_getInteger64(&indexHandle->databaseHandle,
                                      &lockedCount,
                                      "entities",
                                      "lockedCount",
                                      "WHERE id=%lld",
                                      Index_getDatabaseId(entityId)
                                     );
        if (error != ERROR_NONE)
        {
          return error;
        }

        if (lockedCount == 0LL)
        {
          // check if storage exists
          existsStorageFlag = Database_exists(&indexHandle->databaseHandle,
                                              "storage",
                                              "id",
                                              "WHERE entityId=%lld",
                                              Index_getDatabaseId(entityId)
                                             );

          // prune entity if empty
          if (!existsStorageFlag)
          {
            error = Database_execute(&indexHandle->databaseHandle,
                                     CALLBACK(NULL,NULL),  // databaseRowFunction
                                     NULL,  // changedRowCount
                                     "DELETE FROM entities WHERE id=%lld;",
                                     Index_getDatabaseId(entityId)
                                    );
            if (error != ERROR_NONE)
            {
              return error;
            }
          }
        }
      }

      return ERROR_NONE;
    });
  }
  else
  {
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
  }

  return ERROR_NONE;
}

Errors Index_pruneStorage(IndexHandle *indexHandle,
                          IndexId     storageId
                         )
{
  Errors  error;
  bool    existsEntryFlag;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);

  // check if entries exists for storage
  INDEX_DOX(error,
            indexHandle,
  {
    existsEntryFlag = Database_exists(&indexHandle->databaseHandle,
                                      "entries",
                                      "id",
                                      "WHERE storageId=%lld",
                                      Index_getDatabaseId(storageId)
                                     );

    // prune storage if empty and not in use
    if (!existsEntryFlag)
    {
      // delete storage entry
      error = Database_execute(&indexHandle->databaseHandle,
                               CALLBACK(NULL,NULL),  // databaseRowFunction
                               NULL,  // changedRowCount
                               "DELETE FROM storage \
                                WHERE     id=%lld \
                                      AND state IN (%d,%d) \
                               ",
                               Index_getDatabaseId(storageId),
                               INDEX_STATE_OK,
                               INDEX_STATE_ERROR
                              );
      if (error != ERROR_NONE)
      {
        return error;
      }
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_initListSkippedEntry(IndexQueryHandle *indexQueryHandle,
                                  IndexHandle      *indexHandle,
                                  const IndexId    indexIds[],
                                  uint             indexIdCount,
                                  const IndexId    entryIds[],
                                  uint             entryIdCount,
                                  IndexTypeSet     indexTypeSet,
                                  ConstString      name,
                                  DatabaseOrdering ordering,
                                  uint64           offset,
                                  uint64           limit
                                 )
{
  UNUSED_VARIABLE(indexQueryHandle);
  UNUSED_VARIABLE(indexHandle);
  UNUSED_VARIABLE(indexIds);
  UNUSED_VARIABLE(indexIdCount);
  UNUSED_VARIABLE(entryIds);
  UNUSED_VARIABLE(entryIdCount);
  UNUSED_VARIABLE(indexTypeSet);
  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(ordering);
  UNUSED_VARIABLE(offset);
  UNUSED_VARIABLE(limit);

  return ERROR_STILL_NOT_IMPLEMENTED;
}

bool Index_getNextSkippedEntry(IndexQueryHandle *indexQueryHandle,
                               IndexId          *uuidId,
                               String           jobUUID,
                               IndexId          *entityId,
                               String           scheduleUUID,
                               ArchiveTypes     *archiveType,
                               IndexId          *storageId,
                               String           storageName,
                               uint64           *storageDateTime,
                               IndexId          *entryId,
                               String           entryName
                              )
{
//TODO
  UNUSED_VARIABLE(indexQueryHandle);
  UNUSED_VARIABLE(uuidId);
  UNUSED_VARIABLE(jobUUID);
  UNUSED_VARIABLE(entityId);
  UNUSED_VARIABLE(scheduleUUID);
  UNUSED_VARIABLE(archiveType);
  UNUSED_VARIABLE(storageId);
  UNUSED_VARIABLE(storageName);
  UNUSED_VARIABLE(storageDateTime);
  UNUSED_VARIABLE(entryId);
  UNUSED_VARIABLE(entryName);

  return FALSE;
}

Errors Index_addSkippedEntry(IndexHandle *indexHandle,
                             IndexId     storageId,
                             IndexTypes  type,
                             ConstString entryName
                            )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(Index_getType(storageId) == INDEX_TYPE_STORAGE);
  assert(   (type == INDEX_TYPE_FILE           )
         || (type == INDEX_CONST_TYPE_IMAGE    )
         || (type == INDEX_CONST_TYPE_DIRECTORY)
         || (type == INDEX_CONST_TYPE_LINK     )
         || (type == INDEX_CONST_TYPE_HARDLINK )
         || (type == INDEX_CONST_TYPE_SPECIAL  )
        );
  assert(entryName != NULL);

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "INSERT INTO skippedEntries \
                                ( \
                                 storageId, \
                                 type, \
                                 name \
                                ) \
                              VALUES \
                                ( \
                                 %lld, \
                                 %d, \
                                 %'S \
                                ); \
                             ",
                             Index_getDatabaseId(storageId),
                             type,
                             entryName
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }

    return ERROR_NONE;
  });

  return error;
}

Errors Index_deleteSkippedEntry(IndexHandle *indexHandle,
                                IndexId     entryId
                               )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(   (Index_getType(entryId) == INDEX_TYPE_FILE           )
         || (Index_getType(entryId) == INDEX_CONST_TYPE_IMAGE    )
         || (Index_getType(entryId) == INDEX_CONST_TYPE_DIRECTORY)
         || (Index_getType(entryId) == INDEX_CONST_TYPE_LINK     )
         || (Index_getType(entryId) == INDEX_CONST_TYPE_HARDLINK )
         || (Index_getType(entryId) == INDEX_CONST_TYPE_SPECIAL  )
        );

  // check init error
  if (indexHandle->upgradeError != ERROR_NONE)
  {
    return indexHandle->upgradeError;
  }

  INDEX_DOX(error,
            indexHandle,
  {
    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,FALSE);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),  // databaseRowFunction
                             NULL,  // changedRowCount
                             "DELETE FROM skippedEntries WHERE id=%lld;",
                             Index_getDatabaseId(entryId)
                            );
    if (error != ERROR_NONE)
    {
      (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);
      return error;
    }

    (void)Database_setEnabledForeignKeys(&indexHandle->databaseHandle,TRUE);

    return ERROR_NONE;
  });

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
