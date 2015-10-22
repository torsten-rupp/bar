/***********************************************************************\
*
* $Revision: 4195 $
* $Date: 2015-10-17 10:41:02 +0200 (Sat, 17 Oct 2015) $
* $Author: torsten $
* Contents: continous functions
* Systems: all
*
\***********************************************************************/

#define __CONTINOUS_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "global.h"
#include "threads.h"
#include "strings.h"
#include "files.h"
#include "database.h"
#include "errors.h"

#include "continuous.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// sleep time [s]
#define SLEEP_TIME_CONTINUOUS_CLEANUP_THREAD (4*60*60)

#define CONTINOUS_VERSION 1

#define CONTINUOUS_TABLE_DEFINITION \
  "CREATE TABLE IF NOT EXISTS meta(" \
  "  name  TEXT UNIQUE," \
  "  value TEXT" \
  ");" \
  "INSERT OR IGNORE INTO meta (name,value) VALUES ('version',1);" \
  "INSERT OR IGNORE INTO meta (name,value) VALUES ('datetime',DATETIME('now'));" \
  "" \
  "CREATE TABLE IF NOT EXISTS names(" \
  "  id      INTEGER PRIMARY KEY," \
  "  jobUUID TEXT NOT NULL," \
  "  name    TEXT NOT NULL," \
  "  UNIQUE (jobUUID,name) " \
  ");" \
  "CREATE INDEX IF NOT EXISTS namesIndex ON names (jobUUID);"

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL DatabaseHandle continuousDatabaseHandle;
LOCAL Thread         cleanupContinuousThread;    // clean-up thread
LOCAL bool quitFlag;

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define openContinuous(...)  __openContinuous(__FILE__,__LINE__,__VA_ARGS__)
  #define createContinuous(...) __createContinuous(__FILE__,__LINE__,__VA_ARGS__)
  #define closeContinuous(...) __closeContinuous(__FILE__,__LINE__,__VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : openContinuous
* Purpose: open continuous database
* Input  : databaseFileName - database file name
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors openContinuous(DatabaseHandle *databaseHandle,
                              const char     *databaseFileName
                             )
#else /* not NDEBUG */
  LOCAL Errors __openContinuous(const char     *__fileName__,
                                uint           __lineNb__,
                                DatabaseHandle *databaseHandle,
                                const char     *databaseFileName
                               )
#endif /* NDEBUG */
{
  Errors error;

  assert(databaseHandle != NULL);
  assert(databaseFileName != NULL);

  // open continuous database
  #ifdef NDEBUG
    error = Database_open(databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE);
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE);
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }

  // disable synchronous mode and journal to increase transaction speed
  Database_setEnabledSync(databaseHandle,FALSE);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : createContinuous
* Purpose: create empty continuous database
* Input  : databaseFileName - database file name
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors createContinuous(DatabaseHandle *databaseHandle,
                                const char     *databaseFileName
                               )
#else /* not NDEBUG */
  LOCAL Errors __createContinuous(const char     *__fileName__,
                                  uint           __lineNb__,
                                  DatabaseHandle *databaseHandle,
                                  const char     *databaseFileName
                                 )
#endif /* NDEBUG */
{
  Errors error;

  assert(databaseHandle != NULL);
  assert(databaseFileName != NULL);

  // create continuous database
  (void)File_deleteCString(databaseFileName,FALSE);
  #ifdef NDEBUG
    error = Database_open(databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE);
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE);
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }

  // disable synchronous mode and journal to increase transaction speed
  Database_setEnabledSync(databaseHandle,FALSE);

  // create tables
  error = Database_execute(databaseHandle,
                           CALLBACK(NULL,NULL),
                           CONTINUOUS_TABLE_DEFINITION
                          );
  if (error != ERROR_NONE)
  {
    #ifdef NDEBUG
      Database_close(databaseHandle);
    #else /* not NDEBUG */
      __Database_close(__fileName__,__lineNb__,databaseHandle);
    #endif /* NDEBUG */
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : closeContinuous
* Purpose: close continuous database
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors closeContinuous(DatabaseHandle *databaseHandle)
#else /* not NDEBUG */
  LOCAL Errors __closeContinuous(const char     *__fileName__,
                                 uint           __lineNb__,
                                 DatabaseHandle *databaseHandle
                                )
#endif /* NDEBUG */
{
  assert(databaseHandle != NULL);

  #ifdef NDEBUG
    Database_close(databaseHandle);
  #else /* not NDEBUG */
    __Database_close(__fileName__,__lineNb__,databaseHandle);
  #endif /* NDEBUG */

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : getContinuousVersion
* Purpose: get continuous version
* Input  : databaseFileName - database file name
* Output : continuousVersion - continuous version
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getContinuousVersion(const char *databaseFileName, int64 *continuousVersion)
{
  Errors         error;
  DatabaseHandle databaseHandle;

  // open continuous database
  error = openContinuous(&databaseHandle,databaseFileName);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get database version
  error = Database_getInteger64(&databaseHandle,
                                continuousVersion,
                                "meta",
                                "value",
                                "WHERE name='version'"
                               );
  if (error != ERROR_NONE)
  {
    (void)closeContinuous(&databaseHandle);
    return error;
  }

  // close continuous database
  (void)closeContinuous(&databaseHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpOrphanedEntries
* Purpose: delete orphaned entries (entries without storage)
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpOrphanedEntries(DatabaseHandle *databaseHandle)
{
#if 0
  Errors           error;
  String           storageName;
  ulong            n;
  IndexQueryHandle indexQueryHandle;
  DatabaseId       databaseId;

  assert(databaseHandle != NULL);

  // initialize variables
  storageName = String_new();

  // clean-up
  n = 0L;
  error = Index_initListFiles(&indexQueryHandle,
                              databaseHandle,
                              NULL,
                              0,
                              NULL
                             );
  if (error == ERROR_NONE)
  {
    while (   !quitFlag
           && Index_getNextFile(&indexQueryHandle,
                                &databaseId,
                                storageName,
                                NULL,  // storageDateTime,
                                NULL,  // name,
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
        (void)Index_deleteFile(databaseHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  error = Index_initListImages(&indexQueryHandle,
                               databaseHandle,
                               NULL,
                               0,
                               NULL
                              );
  if (error == ERROR_NONE)
  {
    while (   !quitFlag
           && Index_getNextImage(&indexQueryHandle,
                                 &databaseId,
                                 storageName,
                                 NULL,  // storageDateTime,
                                 NULL,  // imageName,
                                 NULL,  // fileSystemType,
                                 NULL,  // size,
                                 NULL,  // blockOffset,
                                 NULL   // blockCount
                                )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteImage(databaseHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  error = Index_initListDirectories(&indexQueryHandle,
                                    databaseHandle,
                                    NULL,
                                    0,
                                    NULL
                                   );
  if (error == ERROR_NONE)
  {
    while (   !quitFlag
           && Index_getNextDirectory(&indexQueryHandle,
                                     &databaseId,
                                     storageName,
                                     NULL,  // storageDateTime,
                                     NULL,  // directoryName,
                                     NULL,  // timeModified,
                                     NULL,  // userId,
                                     NULL,  // groupId,
                                     NULL   // permission
                                    )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteDirectory(databaseHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  error = Index_initListLinks(&indexQueryHandle,
                                    databaseHandle,
                                    NULL,
                                    0,
                                    NULL
                                   );
  if (error == ERROR_NONE)
  {
    while (   !quitFlag
           && Index_getNextLink(&indexQueryHandle,
                                &databaseId,
                                storageName,
                                NULL,  // storageDateTime,
                                NULL,  // name,
                                NULL,  // destinationName
                                NULL,  // timeModified,
                                NULL,  // userId,
                                NULL,  // groupId,
                                NULL   // permission
                               )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteLink(databaseHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  error = Index_initListHardLinks(&indexQueryHandle,
                                  databaseHandle,
                                  NULL,
                                  0,
                                  NULL
                                 );
  if (error == ERROR_NONE)
  {
    while (   !quitFlag
           && Index_getNextHardLink(&indexQueryHandle,
                                    &databaseId,
                                    storageName,
                                    NULL,  // storageDateTime,
                                    NULL,  // fileName,
                                    NULL,  // size,
                                    NULL,  // timeModified,
                                    NULL,  // userId,
                                    NULL,  // groupId,
                                    NULL,  // permission,
                                    NULL,  // fragmentOffset
                                    NULL   // fragmentSize
                                   )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteHardLink(databaseHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  error = Index_initListSpecial(&indexQueryHandle,
                                databaseHandle,
                                NULL, // storage ids
                                0,    // storage id count
                                NULL  // pattern
                               );
  if (error == ERROR_NONE)
  {
    while (   !quitFlag
           && Index_getNextSpecial(&indexQueryHandle,
                                   &databaseId,
                                   storageName,
                                   NULL,  // storageDateTime,
                                   NULL,  // name,
                                   NULL,  // timeModified,
                                   NULL,  // userId,
                                   NULL,  // groupId,
                                   NULL   // permission
                                  )
          )
    {
      if (String_isEmpty(storageName))
      {
        (void)Index_deleteSpecial(databaseHandle,databaseId);
        n++;
      }
    }
    Index_doneList(&indexQueryHandle);
  }
  if (n > 0L) plogMessage(LOG_TYPE_INDEX,"INDEX","Cleaned %lu orphaned entries\n",n);

  // free resources
  String_delete(storageName);
#endif

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUpDuplicateIndizes
* Purpose: delete duplicate storage entries
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors cleanUpDuplicateIndizes(DatabaseHandle *databaseHandle)
{
#if 0
  Errors           error;
  StorageSpecifier storageSpecifier;
  String           storageName;
  String           duplicateStorageName;
  String           printableStorageName;
  ulong            n;
  DatabaseId       storageId;
  bool             deletedIndex;
  IndexQueryHandle indexQueryHandle1,indexQueryHandle2;
  int64            duplicateStorageId;

  assert(databaseHandle != NULL);

  // init variables
  Storage_initSpecifier(&storageSpecifier);
  storageName          = String_new();
  duplicateStorageName = String_new();
  printableStorageName = String_new();

  // clean-up
  n = 0L;
  do
  {
    deletedIndex = FALSE;

    // get storage entry
    error = Index_initListStorage(&indexQueryHandle1,
                                  databaseHandle,
                                  NULL, // uuid
                                  DATABASE_ID_ANY, // entity id
                                  STORAGE_TYPE_ANY,
                                  NULL, // storageName
                                  NULL, // hostName
                                  NULL, // loginName
                                  NULL, // deviceName
                                  NULL, // fileName
                                  INDEX_STATE_SET_ALL
                                 );
    if (error != ERROR_NONE)
    {
      break;
    }
    while (   !quitFlag
           && !deletedIndex
           && Index_getNextStorage(&indexQueryHandle1,
                                   &storageId,
                                   NULL, // entity id
                                   NULL, // job UUID
                                   NULL, // schedule UUID
                                   NULL, // archive type
                                   storageName,
                                   NULL, // createdDateTime
                                   NULL, // entries
                                   NULL, // size
                                   NULL, // indexState
                                   NULL, // indexMode
                                   NULL, // lastCheckedDateTime
                                   NULL  // errorMessage
                                  )
          )
    {
      // check for duplicate entry
      error = Index_initListStorage(&indexQueryHandle2,
                                    databaseHandle,
                                    NULL, // uuid
                                    DATABASE_ID_ANY, // entity id
                                    STORAGE_TYPE_ANY,
                                    NULL, // storageName
                                    NULL, // hostName
                                    NULL, // loginName
                                    NULL, // deviceName
                                    NULL, // fileName
                                    INDEX_STATE_SET_ALL
                                   );
      if (error != ERROR_NONE)
      {
        continue;
      }
      while (   !quitFlag
             && Index_getNextStorage(&indexQueryHandle2,
                                     &duplicateStorageId,
                                     NULL, // entity id
                                     NULL, // job UUID
                                     NULL, // schedule UUID
                                     NULL, // archive type
                                     duplicateStorageName,
                                     NULL, // createdDateTime
                                     NULL, // entries
                                     NULL, // size
                                     NULL, // indexState
                                     NULL, // indexMode
                                     NULL, // lastCheckedDateTime
                                     NULL  // errorMessage
                                    )
            )
      {
        if (   (storageId != duplicateStorageId)
            && Storage_equalNames(storageName,duplicateStorageName)
           )
        {
          // get printable name (if possible)
          error = Storage_parseName(&storageSpecifier,storageName);
          if (error == ERROR_NONE)
          {
            String_set(printableStorageName,Storage_getPrintableName(&storageSpecifier,NULL));
          }
          else
          {
            String_set(printableStorageName,storageName);
          }

          error = Index_deleteStorage(databaseHandle,duplicateStorageId);
          if (error == ERROR_NONE)
          {
            plogMessage(LOG_TYPE_INDEX,
                        "INDEX",
                        "Deleted duplicate index #%lld: '%s'\n",
                        duplicateStorageId,
                        String_cString(printableStorageName)
                       );
            n++;
            break;
          }
          deletedIndex = TRUE;
          break;
        }
      }
      Index_doneList(&indexQueryHandle2);
    }
    Index_doneList(&indexQueryHandle1);
  }
  while (!quitFlag &&  deletedIndex);
  if (n > 0L) plogMessage(LOG_TYPE_INDEX,"INDEX","Cleaned %lu duplicate indizes\n",n);

  // free resources
  String_delete(printableStorageName);
  String_delete(duplicateStorageName);
  String_delete(storageName);
  Storage_doneSpecifier(&storageSpecifier);
#endif

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanupContinuousThreadCode
* Purpose: cleanup index thread
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void cleanupContinuousThreadCode(DatabaseHandle *databaseHandle)
{
  String              absoluteFileName;
  String              pathName;
  Errors              error;
  DirectoryListHandle directoryListHandle;
  String              prefixFileName;
  String              oldDatabaseFileName;
  uint                sleepTime;

#if 0
  // single clean-ups
  plogMessage(LOG_TYPE_INDEX,"INDEX","Clean-up index database\n");
  (void)cleanUpDuplicateMeta(databaseHandle);
  (void)cleanUpIncompleteUpdate(databaseHandle);
  (void)cleanUpIncompleteCreate(databaseHandle);
  (void)cleanUpStorageNoName(databaseHandle);
  (void)cleanUpStorageNoEntity(databaseHandle);
  plogMessage(LOG_TYPE_INDEX,"INDEX","Done clean-up index database\n");

  // regular clean-ups
  while (!quitFlag)
  {
    // clean-up database
    (void)cleanUpOrphanedEntries(databaseHandle);
    (void)cleanUpDuplicateIndizes(databaseHandle);

    // sleep, check quit flag
    sleepTime = 0;
    while ((sleepTime < SLEEP_TIME_INDEX_CLEANUP_THREAD) && !quitFlag)
    {
      Misc_udelay(10LL*MISC_US_PER_SECOND);
      sleepTime += 10;
    }
  }
#endif
}

/*---------------------------------------------------------------------*/

Errors Continuous_initAll(void)
{
  return ERROR_NONE;
}

void Continuous_doneAll(void)
{
}

Errors Continuous_init(const char *databaseFileName)
{
  Errors error;
  int64  continuousVersion;
  String oldDatabaseFileName;
  uint   n;

  assert(databaseFileName != NULL);

  // init variables
  quitFlag = FALSE;

  // check if continuous database exists, create database
  if (File_existsCString(databaseFileName))
  {
    // get continuous version
    error = getContinuousVersion(databaseFileName,&continuousVersion);
    if (error != ERROR_NONE)
    {
      return error;
    }

    if (continuousVersion < CONTINOUS_VERSION)
    {
      // discard existing continuous, create new
      File_deleteCString(databaseFileName,FALSE);
      error = createContinuous(&continuousDatabaseHandle,databaseFileName);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
    else
    {
      // open continuous
      error = openContinuous(&continuousDatabaseHandle,databaseFileName);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
  }
  else
  {
    error = createContinuous(&continuousDatabaseHandle,databaseFileName);
    if (error != ERROR_NONE)
    {
      return error;
    }
  }


  // start clean-up thread
#if 0
  if (!Thread_init(&cleanupContinuousThread,"Continuous clean-up",0,cleanupContinuousThreadCode,&continuousDatabaseHandle))
  {
    HALT_FATAL_ERROR("Cannot initialize continuous clean-up thread!");
  }
#endif

  return ERROR_NONE;
}

void Continuous_done()
{
  quitFlag = TRUE;
//  Thread_join(&cleanupContinuousThread);

  (void)closeContinuous(&continuousDatabaseHandle);
}

Errors Continuous_add(ConstString jobUUID,
                      ConstString name
                     )
{
  return Database_execute(&continuousDatabaseHandle,
                          CALLBACK(NULL,NULL),
                          "INSERT OR IGNORE INTO names \
                             (\
                              jobUUID,\
                              name\
                             ) \
                           VALUES \
                             (\
                              %'S,\
                              %'S\
                             ); \
                          ",
                          jobUUID,
                          name
                         );
}

Errors Continuous_remove(DatabaseId databaseId)
{
  return Database_execute(&continuousDatabaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM names WHERE id=%ld;",
                          databaseId
                         );
}

Errors Continuous_initList(DatabaseQueryHandle *databaseQueryHandle,
                           ConstString         jobUUID
                          )
{
  Errors error;

  assert(databaseQueryHandle != NULL);
  assert(!String_isEmpty(jobUUID));

  error = Database_prepare(databaseQueryHandle,
                           &continuousDatabaseHandle,
                           "SELECT id,name FROM names WHERE jobUUID=%'S",
                           jobUUID
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return error;
}

void Continuous_doneList(DatabaseQueryHandle *databaseQueryHandle)
{
  assert(databaseQueryHandle != NULL);

  Database_finalize(databaseQueryHandle);
}

bool Continuous_getNext(DatabaseQueryHandle *databaseQueryHandle,
                        DatabaseId          *databaseId,
                        String              name
                       )
{
  assert(databaseQueryHandle != NULL);

  return Database_getNextRow(databaseQueryHandle,
                             "%lld %S",
                             databaseId,
                             name
                            );
}

#ifdef __cplusplus
  }
#endif

/* end of file */
