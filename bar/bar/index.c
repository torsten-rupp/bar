/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index database functions
* Systems: all
*
\***********************************************************************/

#define __INDEX_IMPLEMENATION__

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
#include "errors.h"

#include "storage.h"
#include "index_definition.h"

#include "index.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
LOCAL const struct
{
  const char  *name;
  IndexStates indexState;
} INDEX_STATES[] =
{
  { "none",             INDEX_STATE_NONE             },
  { "ok",               INDEX_STATE_OK               },
  { "create",           INDEX_STATE_CREATE           },
  { "update_requested", INDEX_STATE_UPDATE_REQUESTED },
  { "update",           INDEX_STATE_UPDATE           },
  { "error",            INDEX_STATE_ERROR            }
};

LOCAL const struct
{
  const char *name;
  IndexModes indexMode;
} INDEX_MODES[] =
{
  { "MANUAL", INDEX_MODE_MANUAL },
  { "AUTO",   INDEX_MODE_AUTO   },
  { "*",      INDEX_MODE_ALL    }
};

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL Thread initThread;

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define openIndex(...)  __openIndex(__FILE__,__LINE__,__VA_ARGS__)
  #define createIndex(...) __createIndex(__FILE__,__LINE__,__VA_ARGS__)
  #define closeIndex(...) __closeIndex(__FILE__,__LINE__,__VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : openIndex
* Purpose: open index database
* Input  : databaseFileName - database file name
* Output : indexHandle - index handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors openIndex(IndexHandle *indexHandle,
                         const char  *databaseFileName
                        )
#else /* not NDEBUG */
  LOCAL Errors __openIndex(const char  *__fileName__,
                           uint        __lineNb__,
                           IndexHandle *indexHandle,
                           const char  *databaseFileName
                          )
#endif /* NDEBUG */
{
  Errors error;

  assert(indexHandle != NULL);
  assert(databaseFileName != NULL);

  // open index database
  #ifdef NDEBUG
    error = Database_open(&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE);
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_READWRITE);
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }

  // disable synchronous mode and journal to increase transaction speed
  Database_setEnabledSync(&indexHandle->databaseHandle,FALSE);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : createIndex
* Purpose: create empty index database
* Input  : databaseFileName - database file name
* Output : indexHandle - index handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  LOCAL Errors createIndex(IndexHandle *indexHandle,
                           const char  *databaseFileName
                          )
#else /* not NDEBUG */
  LOCAL Errors __createIndex(const char  *__fileName__,
                             uint        __lineNb__,
                             IndexHandle *indexHandle,
                             const char  *databaseFileName
                            )
#endif /* NDEBUG */
{
  Errors error;

  assert(indexHandle != NULL);
  assert(databaseFileName != NULL);

  // open index database
  File_deleteCString(databaseFileName,FALSE);
  #ifdef NDEBUG
    error = Database_open(&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE);
  #else /* not NDEBUG */
    error = __Database_open(__fileName__,__lineNb__,&indexHandle->databaseHandle,databaseFileName,DATABASE_OPENMODE_CREATE);
  #endif /* NDEBUG */
  if (error != ERROR_NONE)
  {
    return error;
  }

  // disable synchronous mode and journal to increase transaction speed
  Database_execute(&indexHandle->databaseHandle,
                   CALLBACK(NULL,NULL),
                   "PRAGMA synchronous=OFF;"
                  );
  Database_execute(&indexHandle->databaseHandle,
                   CALLBACK(NULL,NULL),
                   "PRAGMA journal_mode=OFF;"
                  );

  // create tables
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           INDEX_TABLE_DEFINITION
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
                            uint        __lineNb__,
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
  error = openIndex(&indexHandle,databaseFileName);
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

  (void)closeIndex(&indexHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : setIndexVersion
* Purpose: set index version
* Input  : databaseFileName - database file name
*          indexVersion     - index version
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors setIndexVersion(const char *databaseFileName, int64 indexVersion)
{
  Errors      error;
  IndexHandle indexHandle;

  // open index database
  error = openIndex(&indexHandle,databaseFileName);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // get database version
  error = Database_setInteger64(&indexHandle.databaseHandle,
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

  (void)closeIndex(&indexHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : cleanUp
* Purpose: clean-up index database
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : clean-up steps:
*            - remove duplicates in meta-table
*            - set id in storage, files, images, directories, links,
*              hardlinks, special
\***********************************************************************/

LOCAL void cleanUp(IndexHandle *indexHandle)
{
  DatabaseQueryHandle databaseQueryHandle;
  String              name;

  // remove duplicate entries in meta table
  if (Database_prepare(&databaseQueryHandle,
                       &indexHandle->databaseHandle,
                       "SELECT name FROM meta GROUP BY name"
                      ) == ERROR_NONE
     )
  {
    name = String_new();
    while (Database_getNextRow(&databaseQueryHandle,
                               "%S",
                               name
                              )
          )
    {
      (void)Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM meta \
                              WHERE     name=%'S \
                                    AND rowid NOT IN (SELECT rowid FROM meta WHERE name=%'S ORDER BY rowId DESC LIMIT 0,1); \
                             ",
                             name,
                             name
                            );
    }
    String_delete(name);
    Database_finalize(&databaseQueryHandle);
  }

#if 0
  // fix ids
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK(NULL,NULL),
                         "UPDATE storage SET id=rowId WHERE id IS NULL;"
                        );
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK(NULL,NULL),
                         "UPDATE files SET id=rowId WHERE id IS NULL;"
                        );
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK(NULL,NULL),
                         "UPDATE images SET id=rowId WHERE id IS NULL;"
                        );
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK(NULL,NULL),
                         "UPDATE directories SET id=rowId WHERE id IS NULL;"
                        );
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK(NULL,NULL),
                         "UPDATE links SET id=rowId WHERE id IS NULL;"
                        );
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK(NULL,NULL),
                         "UPDATE hardlinks SET id=rowId WHERE id IS NULL;"
                        );
  (void)Database_execute(&indexHandle->databaseHandle,
                         CALLBACK(NULL,NULL),
                         "UPDATE special SET id=rowId WHERE id IS NULL;"
                        );
#endif
}

/***********************************************************************\
* Name   : upgradeToVersion2
* Purpose: upgrade index to version 2
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeToVersion2(const char *databaseFileName)
{
  String      newDatabaseFileName,backupDatabaseFileName;
  Errors      error;
  IndexHandle indexHandle,newIndexHandle;

  // open old index
  error = openIndex(&indexHandle,databaseFileName);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // create new empty index
  newDatabaseFileName    = String_new();
  backupDatabaseFileName = String_new();
  String_setCString(newDatabaseFileName,databaseFileName);
  String_appendCString(newDatabaseFileName,".new");
  String_setCString(backupDatabaseFileName,databaseFileName);
  String_appendCString(backupDatabaseFileName,".backup");
  error = createIndex(&newIndexHandle,String_cString(newDatabaseFileName));
  if (error != ERROR_NONE)
  {
    String_delete(backupDatabaseFileName);
    String_delete(newDatabaseFileName);
    return error;
  }

  // transfer data to new index
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "storage"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "files"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "images"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "directories"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "links"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "special"
                              );
  }
  if (error != ERROR_NONE)
  {
    (void)closeIndex(&newIndexHandle);
    (void)closeIndex(&indexHandle);
    (void)File_delete(newDatabaseFileName,FALSE);
    String_delete(backupDatabaseFileName);
    String_delete(newDatabaseFileName);
    return error;
  }

  // close new database
  (void)closeIndex(&newIndexHandle);
  (void)closeIndex(&indexHandle);

  // rename database files
  error = File_renameCString(String_cString(newDatabaseFileName),
                             databaseFileName,
                             String_cString(backupDatabaseFileName)
                            );
  if (error != ERROR_NONE)
  {
    (void)File_delete(newDatabaseFileName,FALSE);
    String_delete(backupDatabaseFileName);
    String_delete(newDatabaseFileName);
    return error;
  }

  // free resources
  String_delete(backupDatabaseFileName);
  String_delete(newDatabaseFileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : upgradeToVersion3
* Purpose: upgrade index to version 3
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeToVersion3(const char *databaseFileName)
{
#if 0
  Errors      error;
  IndexHandle indexHandle;

  // add uuid to storage
  error = Database_execute(&indexHandle.databaseHandle,
                           CALLBACK(NULL,NULL),
                           "ALTER TABLE storage ADD COLUMN uuid TEXT"
                          );
  if (error != ERROR_NONE) return error;

  return ERROR_NONE;
#endif

  String         newDatabaseFileName,backupDatabaseFileName;
  Errors         error;
  IndexHandle indexHandle,newIndexHandle;

  // open old index
  error = openIndex(&indexHandle,databaseFileName);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // create new empty index database
  newDatabaseFileName    = String_new();
  backupDatabaseFileName = String_new();
  String_setCString(newDatabaseFileName,databaseFileName);
  String_appendCString(newDatabaseFileName,".new");
  String_setCString(backupDatabaseFileName,databaseFileName);
  String_appendCString(backupDatabaseFileName,".backup");
  error = createIndex(&newIndexHandle,String_cString(newDatabaseFileName));
  if (error != ERROR_NONE)
  {
    String_delete(backupDatabaseFileName);
    String_delete(newDatabaseFileName);
    return error;
  }

  // transfer data to new index
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "storage"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "files"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "images"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "directories"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "links"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "hardlinks"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "special"
                              );
  }
  if (error != ERROR_NONE)
  {
    (void)closeIndex(&newIndexHandle);
    (void)closeIndex(&indexHandle);
    (void)File_delete(newDatabaseFileName,FALSE);
    String_delete(backupDatabaseFileName);
    String_delete(newDatabaseFileName);
    return error;
  }

  // close new database
  (void)closeIndex(&newIndexHandle);
  (void)closeIndex(&indexHandle);

  // rename database files
  error = File_renameCString(String_cString(newDatabaseFileName),
                             databaseFileName,
                             String_cString(backupDatabaseFileName)
                            );
  if (error != ERROR_NONE)
  {
    (void)File_delete(newDatabaseFileName,FALSE);
    String_delete(backupDatabaseFileName);
    String_delete(newDatabaseFileName);
    return error;
  }

  // free resources
  String_delete(backupDatabaseFileName);
  String_delete(newDatabaseFileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : upgradeToVersion4
* Purpose: upgrade index to version 4
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors upgradeToVersion4(const char *databaseFileName)
{
  String              newDatabaseFileName,backupDatabaseFileName;
  Errors              error;
  IndexHandle         indexHandle,newIndexHandle;
  String              name1,name2;
  DatabaseQueryHandle databaseQueryHandle1,databaseQueryHandle2;
  DatabaseId          storageId;
  StaticString        (uuid,INDEX_UUID_LENGTH);
  uint64              createdDateTime;
  DatabaseId          entityId;
  bool                equalsFlag;
  ulong               i;

  // open old index
  error = openIndex(&indexHandle,databaseFileName);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // create new empty index, temporary disable foreign key contrains
  newDatabaseFileName    = String_new();
  backupDatabaseFileName = String_new();
  String_setCString(newDatabaseFileName,databaseFileName);
  String_appendCString(newDatabaseFileName,".new");
  String_setCString(backupDatabaseFileName,databaseFileName);
  String_appendCString(backupDatabaseFileName,".backup");
  error = createIndex(&newIndexHandle,String_cString(newDatabaseFileName));
  if (error != ERROR_NONE)
  {
    String_delete(backupDatabaseFileName);
    String_delete(newDatabaseFileName);
    return error;
  }
  Database_setEnabledForeignKeys(&newIndexHandle.databaseHandle,FALSE);

  // transfer data to new database
#ifndef NDEBUG
Database_debugEnable(false);
//Database_debugEnable(true);
#endif
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "storage"
                              );
  }
#ifndef NDEBUG
//Database_debugEnable(false);
#endif
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "files"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "images"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "directories"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "links"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "hardlinks"
                              );
  }
  if (error == ERROR_NONE)
  {
    error = Database_copyTable(&indexHandle.databaseHandle,
                               &newIndexHandle.databaseHandle,
                               "special"
                              );
  }
#ifndef NDEBUG
//Database_debugEnable(true);
#endif
  if (error != ERROR_NONE)
  {
    (void)closeIndex(&newIndexHandle);
    (void)closeIndex(&indexHandle);
    (void)File_delete(newDatabaseFileName,FALSE);
    String_delete(backupDatabaseFileName);
    String_delete(newDatabaseFileName);
    return error;
  }

Database_debugEnable(true);
  // set entityId in storage entries
  name1 = String_new();
  name2 = String_new();
  error = Database_prepare(&databaseQueryHandle1,
                           &indexHandle.databaseHandle,
                           "SELECT uuid, \
                                   name, \
                                   STRFTIME('%%s',created) \
                            FROM storage \
                            GROUP BY uuid \
                            ORDER BY id,created ASC \
                           "
                          );
  if (error == ERROR_NONE)
  {
    while (Database_getNextRow(&databaseQueryHandle1,
                               "%S %S %lld",
                               uuid,
                               name1,
                               &createdDateTime
                              )
       )
    {
      // insert entity
      error = Database_execute(&newIndexHandle.databaseHandle,
                               CALLBACK(NULL,NULL),
                               "INSERT INTO entities \
                                  (\
                                   jobUUID,\
                                   scheduleUUID, \
                                   created,\
                                   type,\
                                   bidFlag\
                                  ) \
                                VALUES \
                                  (\
                                   %'S,\
                                   '',\
                                   DATETIME(%llu,'unixepoch'),\
                                   %d,\
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
        entityId = Database_getLastRowId(&newIndexHandle.databaseHandle);

Database_debugEnable(false);
#if 1
        // assign entity id for all storage entries with same uuid and matching name (equals except digits)
        error = Database_prepare(&databaseQueryHandle2,
                                 &indexHandle.databaseHandle,
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
              (void)Database_execute(&newIndexHandle.databaseHandle,
                                     CALLBACK(NULL,NULL),
                                     "UPDATE storage \
                                      SET entityId=%llu \
                                      WHERE id=%llu; \
                                     ",
                                     entityId,
                                     storageId
                                    );
            }
          }
          Database_finalize(&databaseQueryHandle2);
        }
#endif
Database_debugEnable(true);
      }
    }
    Database_finalize(&databaseQueryHandle1);
  }
  String_delete(name2);
  String_delete(name1);
  if (error != ERROR_NONE)
  {
    (void)closeIndex(&newIndexHandle);
    (void)closeIndex(&indexHandle);
    (void)File_delete(newDatabaseFileName,FALSE);
    String_delete(backupDatabaseFileName);
    String_delete(newDatabaseFileName);
    return error;
  }

  // close new database
  (void)closeIndex(&newIndexHandle);
  (void)closeIndex(&indexHandle);
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);

  // rename database files
  error = File_renameCString(String_cString(newDatabaseFileName),
                             databaseFileName,
                             String_cString(backupDatabaseFileName)
                            );
  if (error != ERROR_NONE)
  {
    (void)File_delete(newDatabaseFileName,FALSE);
    String_delete(backupDatabaseFileName);
    String_delete(newDatabaseFileName);
    return error;
  }

  // free resources
  String_delete(backupDatabaseFileName);
  String_delete(newDatabaseFileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : initThreadCode
* Purpose: init index thread
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initThreadCode(IndexHandle *indexHandle)
{
  Errors error;
  int64  indexVersion;

  assert(indexHandle != NULL);
  assert(!indexHandle->initFlag);

  // open/create database
  if (File_existsCString(indexHandle->databaseFileName))
  {
    do
    {
      // get index version
      error = getIndexVersion(indexHandle->databaseFileName,&indexVersion);
      if (error != ERROR_NONE)
      {
        return;
      }

      // upgrade index structure
      if (indexVersion < INDEX_VERSION)
      {
        switch (indexVersion)
        {
          case 1:
            error = upgradeToVersion2(indexHandle->databaseFileName);
            indexVersion = 2;
          case 2:
            error = upgradeToVersion3(indexHandle->databaseFileName);
            indexVersion = 3;
            break;
          case 3:
            error = upgradeToVersion4(indexHandle->databaseFileName);
            indexVersion = 4;
            break;
          default:
            // assume correct database version if index is unknown
            indexVersion = INDEX_VERSION;
            break;
        }
        if (error != ERROR_NONE)
        {
          return;
        }

        // update index version
        error = setIndexVersion(indexHandle->databaseFileName,indexVersion);
        if (error != ERROR_NONE)
        {
          return;
        }
      }
    }
    while (indexVersion < INDEX_VERSION);

    // open data base
    error = openIndex(indexHandle,indexHandle->databaseFileName);
    if (error != ERROR_NONE)
    {
      return;
    }

    // clean-up database
    cleanUp(indexHandle);
  }
  else
  {
    // create index database
    error = createIndex(indexHandle,indexHandle->databaseFileName);
    if (error != ERROR_NONE)
    {
      return;
    }
  }

  // init done
  indexHandle->initFlag = TRUE;
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
  assert(indexHandle->initFlag);

  indexQueryHandle->indexHandle                = indexHandle;
  indexQueryHandle->storage.type               = STORAGE_TYPE_NONE;
  indexQueryHandle->storage.storageNamePattern = NULL;
  indexQueryHandle->storage.hostNamePattern    = NULL;
  indexQueryHandle->storage.loginNamePattern   = NULL;
  indexQueryHandle->storage.deviceNamePattern  = NULL;
  indexQueryHandle->storage.fileNamePattern    = NULL;
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
  assert(indexQueryHandle->indexHandle->initFlag);

  if (indexQueryHandle->storage.fileNamePattern    != NULL) Pattern_delete(indexQueryHandle->storage.fileNamePattern);
  if (indexQueryHandle->storage.deviceNamePattern  != NULL) Pattern_delete(indexQueryHandle->storage.deviceNamePattern);
  if (indexQueryHandle->storage.loginNamePattern   != NULL) Pattern_delete(indexQueryHandle->storage.loginNamePattern);
  if (indexQueryHandle->storage.hostNamePattern    != NULL) Pattern_delete(indexQueryHandle->storage.hostNamePattern);
  if (indexQueryHandle->storage.storageNamePattern != NULL) Pattern_delete(indexQueryHandle->storage.storageNamePattern);
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
  uint i;

  String_clear(string);
  for (i = INDEX_STATE_OK; i < INDEX_STATE_UNKNOWN; i++)
  {
    if ((indexStateSet & (1 << i)) != 0)
    {
      if (!String_isEmpty(string)) String_appendCString(string,",");
      String_format(string,"%d",i);
    }
  }

  return string;
}

/***********************************************************************\
* Name   : getREGEXPString
* Purpose: get REGEXP filter string
* Input  : string      - string variable
*          columnName  - column name
*          patternText - pattern text
* Output : -
* Return : string for WHERE statement
* Notes  : -
\***********************************************************************/

LOCAL String getREGEXPString(String string, const char *columnName, const String patternText)
{
  StringTokenizer stringTokenizer;
  String          token;
  ulong           z;
  char            ch;

  String_setCString(string,"1");
  if (patternText != NULL)
  {
    String_initTokenizer(&stringTokenizer,
                         patternText,
                         STRING_BEGIN,
                         STRING_WHITE_SPACES,
                         STRING_QUOTES,
                         TRUE
                        );
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      String_appendCString(string," AND REGEXP('");
      z = 0;
      while (z < String_length(token))
      {
        ch = String_index(token,z);
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
            z++;
            break;
          case '*':
            String_appendCString(string,".*");
            z++;
            break;
          case '?':
            String_appendChar(string,'.');
            z++;
            break;
          case '\'':
            String_appendCString(string,"''");
            z++;
            break;
          default:
            String_appendChar(string,ch);
            z++;
            break;
        }
      }
      String_format(string,"',0,%s)",columnName);
    }
    String_doneTokenizer(&stringTokenizer);
  }

  return string;
}

/***********************************************************************\
* Name   : getOrderingString
* Purpose: get SQL ordering string
* Input  : ordering - database ordering
* Output : -
* Return : string
* Notes  : -
\***********************************************************************/

LOCAL const char *getOrderingString(DatabaseOrdering ordering)
{
  const char *s;

  switch (ordering)
  {
    case DATABASE_ORDERING_ASCENDING:  s = "ASC";  break;
    case DATABASE_ORDERING_DESCENDING: s = "DESC"; break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }

  return s;
}

/*---------------------------------------------------------------------*/

Errors Index_initAll(void)
{
  return ERROR_NONE;
}

void Index_doneAll(void)
{
}

const char *Index_stateToString(IndexStates indexState, const char *defaultValue)
{
  uint       z;
  const char *name;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_STATES))
         && (INDEX_STATES[z].indexState != indexState)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_STATES))
  {
    name = INDEX_STATES[z].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseState(const char *name, IndexStates *indexState)
{
  uint z;

  assert(name != NULL);
  assert(indexState != NULL);

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_STATES))
         && !stringEqualsIgnoreCase(INDEX_STATES[z].name,name)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_STATES))
  {
    (*indexState) = INDEX_STATES[z].indexState;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Index_modeToString(IndexModes indexMode, const char *defaultValue)
{
  uint       z;
  const char *name;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_MODES))
         && (INDEX_MODES[z].indexMode != indexMode)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_MODES))
  {
    name = INDEX_MODES[z].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseMode(const char *name, IndexModes *indexMode)
{
  uint z;

  assert(name != NULL);
  assert(indexMode != NULL);

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_MODES))
         && !stringEqualsIgnoreCase(INDEX_MODES[z].name,name)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_MODES))
  {
    (*indexMode) = INDEX_MODES[z].indexMode;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

Errors Index_init(IndexHandle *indexHandle,
                  const char  *databaseFileName
                 )
{
  assert(indexHandle != NULL);
  assert(databaseFileName != NULL);

  // init variables
  indexHandle->databaseFileName = databaseFileName;
  indexHandle->initFlag         = FALSE;

  if (!Thread_init(&initThread,"Index init",0,initThreadCode,indexHandle))
  {
    HALT_FATAL_ERROR("Cannot initialize index init thread!");
  }

  return ERROR_NONE;
}

#ifdef NDEBUG
  void Index_done(IndexHandle *indexHandle)
#else /* not NDEBUG */
  void __Index_done(const char  *__fileName__,
                    uint        __lineNb__,
                    IndexHandle *indexHandle
                   )
#endif /* NDEBUG */
{
  assert(indexHandle != NULL);

  #ifdef NDEBUG
    (void)closeIndex(indexHandle);
  #else /* not NDEBUG */
    (void)__closeIndex(__fileName__,__lineNb__,indexHandle);
  #endif /* NDEBUG */
}

bool Index_isReady(IndexHandle *indexHandle)
{
  assert(indexHandle != NULL);

  return indexHandle->initFlag;
}

bool Index_findById(IndexHandle *indexHandle,
                    DatabaseId  storageId,
                    String      jobUUID,
                    String      scheduleUUID,
                    String      storageName,
                    IndexStates *indexState,
                    uint64      *lastCheckedTimestamp
                   )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  bool                result;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   storage.name, \
                                   storage.state, \
                                   STRFTIME('%%s',storage.lastChecked) \
                            FROM storage \
                            LEFT JOIN entities ON storage.entityId=entities.id \
                            WHERE storage.id=%lld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return FALSE;
  }
  result = Database_getNextRow(&databaseQueryHandle,
                               "%S %S %S %d %llu",
                               jobUUID,
                               scheduleUUID,
                               storageName,
                               indexState,
                               lastCheckedTimestamp
                              );
  Database_finalize(&databaseQueryHandle);

  return result;
}

bool Index_findByName(IndexHandle  *indexHandle,
                      StorageTypes storageType,
                      const String hostName,
                      const String loginName,
                      const String deviceName,
                      const String fileName,
                      String       jobUUID,
                      String       scheduleUUID,
                      DatabaseId   *storageId,
                      IndexStates  *indexState,
                      uint64       *lastCheckedTimestamp
                     )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  String              storageName;
  StorageSpecifier    storageSpecifier;
  bool                foundFlag;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);
  assert(storageId != NULL);

  (*storageId) = DATABASE_ID_NONE;

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   storage.id, \
                                   storage.name, \
                                   storage.state, \
                                   STRFTIME('%%s',storagelastChecked) \
                            FROM storage \
                            LEFT JOIN entities ON storage.entityId=entities.id \
                           "
                          );
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  storageName = String_new();
  Storage_initSpecifier(&storageSpecifier);
  foundFlag   = FALSE;
  while (   Database_getNextRow(&databaseQueryHandle,
                                "%S %S %lld %S %d %llu",
                                jobUUID,
                                scheduleUUID,
                                storageId,
                                storageName,
                                indexState,
                                lastCheckedTimestamp
                               )
         && !foundFlag
        )
  {
    if (Storage_parseName(&storageSpecifier,storageName) == ERROR_NONE)
    {
      switch (storageSpecifier.type)
      {
        case STORAGE_TYPE_FILESYSTEM:
          foundFlag =     ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_FILESYSTEM))
                      && ((fileName == NULL) || String_equals(fileName,storageSpecifier.fileName));
          break;
        case STORAGE_TYPE_FTP:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_FTP))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName ))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageSpecifier.fileName ));
          break;
        case STORAGE_TYPE_SSH:
        case STORAGE_TYPE_SCP:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_SSH) || (storageType == STORAGE_TYPE_SCP))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName ))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageSpecifier.fileName ));
          break;
        case STORAGE_TYPE_SFTP:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_SFTP))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName ))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageSpecifier.fileName ));
          break;
        case STORAGE_TYPE_WEBDAV:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_WEBDAV))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName ))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageSpecifier.fileName ));
          break;
        case STORAGE_TYPE_CD:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_CD))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageSpecifier.fileName  ));
          break;
        case STORAGE_TYPE_DVD:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_DVD))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageSpecifier.fileName  ));
          break;
        case STORAGE_TYPE_BD:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_BD))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageSpecifier.fileName  ));
          break;
        case STORAGE_TYPE_DEVICE:
          foundFlag =    ((storageType == STORAGE_TYPE_ANY) || (storageType == STORAGE_TYPE_DEVICE))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageSpecifier.fileName  ));
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break;
      }
    }
  }
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(storageName);

  Database_finalize(&databaseQueryHandle);

  return foundFlag;
}

bool Index_findByState(IndexHandle   *indexHandle,
                       IndexStateSet indexStateSet,
                       String        jobUUID,
                       String        scheduleUUID,
                       DatabaseId    *storageId,
                       String        storageName,
                       uint64        *lastCheckedTimestamp
                      )
{
  String              indexStateSetString;
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  bool                result;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);
  assert(storageId != NULL);

  (*storageId) = DATABASE_ID_NONE;
  if (storageName != NULL) String_clear(storageName);
  if (lastCheckedTimestamp != NULL) (*lastCheckedTimestamp) = 0LL;

  indexStateSetString = String_new();
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   storage.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.lastChecked) \
                            FROM storage \
                            LEFT JOIN entities ON storage.entityId=entities.id \
                            WHERE storage.state IN (%S) \
                           ",
                           getIndexStateSetString(indexStateSetString,indexStateSet)
                          );
  if (error != ERROR_NONE)
  {
    String_delete(indexStateSetString);
    return FALSE;
  }
  String_delete(indexStateSetString);
  result = Database_getNextRow(&databaseQueryHandle,
                               "%S %S %lld %S %llu",
                               jobUUID,
                               scheduleUUID,
                               storageId,
                               storageName,
                               lastCheckedTimestamp
                              );
  Database_finalize(&databaseQueryHandle);

  return result;
}

Errors Index_clear(IndexHandle *indexHandle,
                   DatabaseId  storageId
                  )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  // Note: do in single steps to avoid long-time-locking of database!
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM files WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM images WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM directories WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM links WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM hardlinks WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM special WHERE storageId=%ld;",
                           storageId
                          );

  return ERROR_NONE;
}

Errors Index_update(IndexHandle  *indexHandle,
                    DatabaseId   storageId,
                    const String storageName,
                    uint64       size
                   )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  if (storageName != NULL)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "UPDATE storage \
                              SET name=%'S \
                              WHERE id=%ld;\
                             ",
                             storageName,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "UPDATE storage \
                            SET size=%ld \
                            WHERE id=%ld;\
                           ",
                           size,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_getState(IndexHandle *indexHandle,
                      DatabaseId  storageId,
                      IndexStates *indexState,
                      uint64      *lastCheckedTimestamp,
                      String      errorMessage
                     )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT state, \
                                   STRFTIME('%%s',lastChecked), \
                                   errorMessage \
                            FROM storage \
                            WHERE id=%ld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseQueryHandle,
                           "%d %llu %S",
                           indexState,
                           lastCheckedTimestamp,
                           errorMessage
                          )
     )
  {
    (*indexState) = INDEX_STATE_UNKNOWN;
    if (errorMessage != NULL) String_clear(errorMessage);
  }
  Database_finalize(&databaseQueryHandle);

  return ERROR_NONE;
}

Errors Index_setState(IndexHandle *indexHandle,
                      DatabaseId  storageId,
                      IndexStates indexState,
                      uint64      lastCheckedTimestamp,
                      const char  *errorMessage,
                      ...
                     )
{
  Errors  error;
  va_list arguments;
  String  s;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "UPDATE storage \
                            SET state=%d, \
                                errorMessage=NULL \
                            WHERE id=%ld; \
                           ",
                           indexState,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  if (lastCheckedTimestamp != 0LL)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "UPDATE storage \
                              SET lastChecked=DATETIME(%llu,'unixepoch') \
                              WHERE id=%ld; \
                             ",
                             lastCheckedTimestamp,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  if (errorMessage != NULL)
  {
    va_start(arguments,errorMessage);
    s = String_vformat(String_new(),errorMessage,arguments);
    va_end(arguments);

    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "UPDATE storage \
                              SET errorMessage=%'S \
                              WHERE id=%ld; \
                             ",
                             s,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      String_delete(s);
      return error;
    }

    String_delete(s);
  }
  else
  {
  }

  return ERROR_NONE;
}

long Index_countState(IndexHandle *indexHandle,
                      IndexStates indexState
                     )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  long                count;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

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
}

Errors Index_initListUUIDs(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle
                          )
{
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   STRFTIME('%%s',(SELECT created FROM storage WHERE storage.entityId=entities.id ORDER BY created DESC LIMIT 0,1)), \
                                   (SELECT SUM(size) FROM storage LEFT JOIN entities AS storageEntities ON storage.entityId=storageEntities.id WHERE storageEntities.jobUUID=entities.jobUUID), \
                                   (SELECT errorMessage FROM storage WHERE storage.entityId=entities.id ORDER BY created DESC LIMIT 0,1) \
                            FROM entities \
                            GROUP BY entities.jobUUID; \
                           "
                          );
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);
  DEBUG_ADD_RESOURCE_TRACE("indexQueryHandle",indexQueryHandle);

  return error;
}

bool Index_getNextUUID(IndexQueryHandle *indexQueryHandle,
                       String           jobUUID,
                       String           scheduleUUID,
                       uint64           *lastCreatedDateTime,
                       uint64           *totalSize,
                       String           lastErrorMessage
                      )
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  assert(indexQueryHandle->indexHandle->initFlag);

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%S %S %lld %lld %S",
                             jobUUID,
                             scheduleUUID,
                             lastCreatedDateTime,
                             totalSize,
                             lastErrorMessage
                            );
}

Errors Index_deleteUUID(IndexHandle  *indexHandle,
                        const String jobUUID
                       )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  DatabaseId          entityId;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  // delete entities of UUID
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT id \
                            FROM entities \
                            WHERE jobUUID=%'S; \
                           ",
                           jobUUID
                          );
  if (error == ERROR_NONE)
  {
    while (   Database_getNextRow(&databaseQueryHandle,
                                  "%lld",
                                  &entityId
                                 )
           && (error == ERROR_NONE)
          )
    {
      error = Index_deleteEntity(indexHandle,entityId);
    }
    Database_finalize(&databaseQueryHandle);
  }

  return error;
}

Errors Index_initListEntities(IndexQueryHandle *indexQueryHandle,
                              IndexHandle      *indexHandle,
                              const String     jobUUID,
                              const String     scheduleUUID,
                              DatabaseOrdering ordering,
                              ulong            offset
                             )
{
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT entities.id, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   STRFTIME('%%s',entities.created), \
                                   entities.type, \
                                   (SELECT SUM(size) FROM storage WHERE storage.entityId=entities.id), \
                                   (SELECT errorMessage FROM storage WHERE storage.entityId=entities.id ORDER BY created DESC LIMIT 0,1) \
                            FROM entities \
                            WHERE     (%d OR jobUUID=%'S) \
                                  AND (%d OR scheduleUUID=%'S) \
                            ORDER BY entities.created %s \
                            LIMIT %lu OFFSET %lu \
                           ",
                           String_isEmpty(jobUUID) ? 1 : 0,
                           jobUUID,
                           String_isEmpty(scheduleUUID) ? 1 : 0,
                           scheduleUUID,
                           getOrderingString(ordering),
                           MAX_UINT,
                           offset
                          );
//Database_debugPrintQueryInfo(&indexQueryHandle->databaseQueryHandle);
  DEBUG_ADD_RESOURCE_TRACE("indexQueryHandle",indexQueryHandle);

  return error;
}

bool Index_getNextEntity(IndexQueryHandle *indexQueryHandle,
                         DatabaseId       *databaseId,
                         String           jobUUID,
                         String           scheduleUUID,
                         uint64           *createdDateTime,
                         ArchiveTypes     *archiveType,
                         uint64           *totalSize,
                         String           lastErrorMessage
                        )
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  assert(indexQueryHandle->indexHandle->initFlag);

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %S %llu %u %lld %S",
                             databaseId,
                             jobUUID,
                             scheduleUUID,
                             createdDateTime,
                             archiveType,
                             totalSize,
                             lastErrorMessage
                            );
}

Errors Index_newEntity(IndexHandle  *indexHandle,
                       const String jobUUID,
                       const String scheduleUUID,
                       ArchiveTypes archiveType,
                       DatabaseId   *entityId
                      )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);
  assert(entityId != NULL);

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO entities \
                              (\
                               jobUUID,\
                               scheduleUUID,\
                               created,\
                               type,\
                               parentJobUUID,\
                               bidFlag\
                              ) \
                            VALUES \
                              (\
                               %'S,\
                               %'S,\
                               DATETIME('now'),\
                               %u,\
                               '',\
                               0\
                              ); \
                           ",
                           jobUUID,
                           scheduleUUID,
                           archiveType
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  (*entityId) = Database_getLastRowId(&indexHandle->databaseHandle);

  return ERROR_NONE;
}

Errors Index_deleteEntity(IndexHandle *indexHandle,
                          DatabaseId  entityId
                         )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  DatabaseId          storageId;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  // delete storage of entity
  error = Database_prepare(&databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT id \
                            FROM storage \
                            WHERE entityId=%ld; \
                           ",
                           entityId
                          );
  if (error == ERROR_NONE)
  {
    while (   Database_getNextRow(&databaseQueryHandle,
                                  "%lld",
                                  &storageId
                                 )
           && (error == ERROR_NONE)
          )
    {
      error = Index_deleteStorage(indexHandle,storageId);
    }
    Database_finalize(&databaseQueryHandle);
  }

  // delete entity
  if (error == ERROR_NONE)
  {
    error = Database_execute(&indexHandle->databaseHandle,
                             CALLBACK(NULL,NULL),
                             "DELETE FROM entities WHERE id=%ld;",
                             entityId
                            );
  }

  return error;
}

Errors Index_initListStorage(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const String     jobUUID,
                             DatabaseId       entityId,
                             StorageTypes     storageType,
                             const String     storageName,
                             const String     hostName,
                             const String     loginName,
                             const String     deviceName,
                             const String     fileName,
                             IndexStateSet    indexStateSet
                            )
{
  String indexStateSetString;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  initIndexQueryHandle(indexQueryHandle,indexHandle);
  indexQueryHandle->storage.type = storageType;
  if (storageName != NULL) indexQueryHandle->storage.storageNamePattern = Pattern_new(storageName,PATTERN_TYPE_GLOB,PATTERN_FLAG_IGNORE_CASE);
  if (hostName    != NULL) indexQueryHandle->storage.hostNamePattern    = Pattern_new(hostName,   PATTERN_TYPE_GLOB,PATTERN_FLAG_IGNORE_CASE);
  if (loginName   != NULL) indexQueryHandle->storage.loginNamePattern   = Pattern_new(loginName,  PATTERN_TYPE_GLOB,PATTERN_FLAG_IGNORE_CASE);
  if (deviceName  != NULL) indexQueryHandle->storage.deviceNamePattern  = Pattern_new(deviceName, PATTERN_TYPE_GLOB,PATTERN_FLAG_IGNORE_CASE);
  if (fileName    != NULL) indexQueryHandle->storage.fileNamePattern    = Pattern_new(fileName,   PATTERN_TYPE_GLOB,PATTERN_FLAG_IGNORE_CASE);

  indexStateSetString = String_new();
  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT storage.id, \
                                   storage.entityId, \
                                   entities.jobUUID, \
                                   entities.scheduleUUID, \
                                   entities.type, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   storage.size, \
                                   storage.state, \
                                   storage.mode, \
                                   STRFTIME('%%s',storage.lastChecked), \
                                   storage.errorMessage \
                            FROM storage \
                            LEFT JOIN entities ON entities.id=storage.entityId \
                            WHERE     (%d OR (entities.jobUUID='%S')) \
                                  AND (%d OR (storage.entityId=%ld)) \
                                  AND storage.state IN (%S) \
                            ORDER BY storage.created DESC \
                           ",
                           String_isEmpty(jobUUID) ? 1 : 0,
                           jobUUID,
                           (entityId == DATABASE_ID_ANY) ? 1 : 0,
                           entityId,
                           getIndexStateSetString(indexStateSetString,indexStateSet)
                          );
  DEBUG_ADD_RESOURCE_TRACE("indexQueryHandle",indexQueryHandle);

  String_delete(indexStateSetString);

  return error;
}

bool Index_getNextStorage(IndexQueryHandle *indexQueryHandle,
                          DatabaseId       *storageId,
                          DatabaseId       *entityId,
                          String           jobUUID,
                          String           scheduleUUID,
                          ArchiveTypes     *archiveType,
                          String           storageName,
                          uint64           *createdDateTime,
                          uint64           *size,
                          IndexStates      *indexState,
                          IndexModes       *indexMode,
                          uint64           *lastCheckedDateTime,
                          String           errorMessage
                         )
{
  StorageSpecifier storageSpecifier;
  bool             foundFlag;

  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  assert(indexQueryHandle->indexHandle->initFlag);

  Storage_initSpecifier(&storageSpecifier);
  foundFlag = FALSE;
  while (   !foundFlag
         && Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                                "%lld %lld %S %S %d %S %llu %llu %d %d %llu %S",
                                storageId,
                                entityId,
                                jobUUID,
                                scheduleUUID,
                                archiveType,
                                storageName,
                                createdDateTime,
                                size,
                                indexState,
                                indexMode,
                                lastCheckedDateTime,
                                errorMessage
                               )
        )
  {
    if (Storage_parseName(&storageSpecifier,storageName) == ERROR_NONE)
    {
      switch (storageSpecifier.type)
      {
        case STORAGE_TYPE_FILESYSTEM:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_FILESYSTEM))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName,           PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,storageSpecifier.fileName,PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_FTP:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_FTP))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName,               PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.hostNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.hostNamePattern,   storageSpecifier.hostName, PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.loginNamePattern   == NULL) || Pattern_match(indexQueryHandle->storage.loginNamePattern,  storageSpecifier.loginName,PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,   storageSpecifier.fileName, PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_SSH:
        case STORAGE_TYPE_SCP:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_SSH) || (indexQueryHandle->storage.type == STORAGE_TYPE_SCP))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName,             PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.hostNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.hostNamePattern, storageSpecifier.hostName, PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.loginNamePattern   == NULL) || Pattern_match(indexQueryHandle->storage.loginNamePattern,storageSpecifier.loginName,PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern, storageSpecifier.fileName, PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_SFTP:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_SFTP))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName,             PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.hostNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.hostNamePattern, storageSpecifier.hostName, PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.loginNamePattern   == NULL) || Pattern_match(indexQueryHandle->storage.loginNamePattern,storageSpecifier.loginName,PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern, storageSpecifier.fileName, PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_WEBDAV:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_WEBDAV))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName,             PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.hostNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.hostNamePattern, storageSpecifier.hostName, PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.loginNamePattern   == NULL) || Pattern_match(indexQueryHandle->storage.loginNamePattern,storageSpecifier.loginName,PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern, storageSpecifier.fileName, PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_CD:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_CD))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName,               PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.deviceNamePattern  == NULL) || Pattern_match(indexQueryHandle->storage.deviceNamePattern,storageSpecifier.deviceName,PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,  storageSpecifier.fileName,  PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_DVD:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_DVD))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName,               PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.deviceNamePattern  == NULL) || Pattern_match(indexQueryHandle->storage.deviceNamePattern,storageSpecifier.deviceName,PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,  storageSpecifier.fileName,  PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_BD:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_BD))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName,               PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.deviceNamePattern  == NULL) || Pattern_match(indexQueryHandle->storage.deviceNamePattern,storageSpecifier.deviceName,PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,  storageSpecifier.fileName,  PATTERN_MATCH_MODE_ANY));
          break;
        case STORAGE_TYPE_DEVICE:
          foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_DEVICE))
                      && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName,               PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.deviceNamePattern  == NULL) || Pattern_match(indexQueryHandle->storage.deviceNamePattern,storageSpecifier.deviceName,PATTERN_MATCH_MODE_ANY))
                      && ((indexQueryHandle->storage.fileNamePattern    == NULL) || Pattern_match(indexQueryHandle->storage.fileNamePattern,  storageSpecifier.fileName,  PATTERN_MATCH_MODE_ANY));
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break;
      }
    }
    else
    {
      foundFlag =    ((indexQueryHandle->storage.type == STORAGE_TYPE_ANY) || (indexQueryHandle->storage.type == STORAGE_TYPE_FILESYSTEM))
                  && ((indexQueryHandle->storage.storageNamePattern == NULL) || Pattern_match(indexQueryHandle->storage.storageNamePattern,storageName,PATTERN_MATCH_MODE_ANY));
    }
  }
  Storage_doneSpecifier(&storageSpecifier);

  return foundFlag;
}

Errors Index_newStorage(IndexHandle  *indexHandle,
                        DatabaseId   entityId,
                        const String storageName,
                        IndexStates  indexState,
                        IndexModes   indexMode,
                        DatabaseId   *storageId
                       )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);
  assert(storageId != NULL);

  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "INSERT INTO storage \
                              (\
                               entityId,\
                               name,\
                               created,\
                               size,\
                               state,\
                               mode,\
                               lastChecked\
                              ) \
                            VALUES \
                              (\
                               %d,\
                               %'S,\
                               DATETIME('now'),\
                               0,\
                               %d,\
                               %d,\
                               DATETIME('now')\
                              ); \
                           ",
                           entityId,
                           storageName,
                           indexState,
                           indexMode
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  (*storageId) = Database_getLastRowId(&indexHandle->databaseHandle);

  return ERROR_NONE;
}

Errors Index_deleteStorage(IndexHandle *indexHandle,
                           DatabaseId  storageId
                          )
{
  Errors error;

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  // Note: do in single steps to avoid long-time-locking of database!
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM files WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM images WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM directories WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM links WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM hardlinks WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM special WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(&indexHandle->databaseHandle,
                           CALLBACK(NULL,NULL),
                           "DELETE FROM storage WHERE id=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;

  return ERROR_NONE;
}

Errors Index_initListFiles(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const DatabaseId storageIds[],
                           uint             storageIdCount,
                           String           pattern
                          )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"files.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(storage.id IN (");
    for (z = 0; z < storageIdCount; z++)
    {
      if (z > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[z]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT files.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   files.name, \
                                   files.size, \
                                   files.timeModified, \
                                   files.userId, \
                                   files.groupId, \
                                   files.permission, \
                                   files.fragmentOffset, \
                                   files.fragmentSize\
                            FROM files\
                              LEFT JOIN storage ON storage.id=files.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );
  DEBUG_ADD_RESOURCE_TRACE("indexQueryHandle",indexQueryHandle);

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextFile(IndexQueryHandle *indexQueryHandle,
                       DatabaseId       *databaseId,
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
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  assert(indexQueryHandle->indexHandle->initFlag);

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %llu %llu %d %d %d %llu %llu",
                             databaseId,
                             storageName,
                             storageDateTime,
                             fileName,
                             size,
                             timeModified,
                             userId,
                             groupId,
                             permission,
                             fragmentOffset,
                             fragmentSize
                            );
}

Errors Index_deleteFile(IndexHandle *indexHandle,
                        DatabaseId  databaseId
                       )
{
  assert(indexHandle != NULL);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM files WHERE id=%ld;",
                          databaseId
                         );
}

Errors Index_initListImages(IndexQueryHandle *indexQueryHandle,
                            IndexHandle      *indexHandle,
                            const DatabaseId *storageIds,
                            uint             storageIdCount,
                            String           pattern
                           )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"images.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(storage.id IN (");
    for (z = 0; z < storageIdCount; z++)
    {
      if (z > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[z]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT images.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   images.name, \
                                   images.fileSystemType, \
                                   images.size, \
                                   images.blockOffset, \
                                   images.blockCount \
                            FROM images \
                              LEFT JOIN storage ON storage.id=images.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );
  DEBUG_ADD_RESOURCE_TRACE("indexQueryHandle",indexQueryHandle);

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextImage(IndexQueryHandle *indexQueryHandle,
                        DatabaseId       *databaseId,
                        String           storageName,
                        uint64           *storageDateTime,
                        String           imageName,
                        FileSystemTypes  *fileSystemType,
                        uint64           *size,
                        uint64           *blockOffset,
                        uint64           *blockCount
                       )
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  assert(indexQueryHandle->indexHandle->initFlag);

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %u %llu %llu %llu",
                             databaseId,
                             storageName,
                             storageDateTime,
                             imageName,
                             fileSystemType,
                             size,
                             blockOffset,
                             blockCount
                            );
}

Errors Index_deleteImage(IndexHandle *indexHandle,
                         DatabaseId  databaseId
                        )
{
  assert(indexHandle != NULL);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM files WHERE id=%ld;",
                          databaseId
                         );
}

Errors Index_initListDirectories(IndexQueryHandle *indexQueryHandle,
                                 IndexHandle      *indexHandle,
                                 const DatabaseId *storageIds,
                                 uint             storageIdCount,
                                 String           pattern
                                )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"directories.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(storage.id IN (");
    for (z = 0; z < storageIdCount; z++)
    {
      if (z > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[z]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT directories.id,\
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   directories.name, \
                                   directories.timeModified, \
                                   directories.userId, \
                                   directories.groupId, \
                                   directories.permission \
                            FROM directories \
                              LEFT JOIN storage ON storage.id=directories.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );
  DEBUG_ADD_RESOURCE_TRACE("indexQueryHandle",indexQueryHandle);

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextDirectory(IndexQueryHandle *indexQueryHandle,
                            DatabaseId       *databaseId,
                            String           storageName,
                            uint64           *storageDateTime,
                            String           directoryName,
                            uint64           *timeModified,
                            uint32           *userId,
                            uint32           *groupId,
                            uint32           *permission
                           )
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  assert(indexQueryHandle->indexHandle->initFlag);

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %llu %d %d %d",
                             databaseId,
                             storageName,
                             storageDateTime,
                             directoryName,
                             timeModified,
                             userId,
                             groupId,
                             permission
                            );
}

Errors Index_deleteDirectory(IndexHandle *indexHandle,
                             DatabaseId  databaseId
                            )
{
  assert(indexHandle != NULL);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM directories WHERE id=%ld;",
                          databaseId
                         );
}

Errors Index_initListLinks(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const DatabaseId *storageIds,
                           uint             storageIdCount,
                           String           pattern
                          )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"links.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(storage.id IN (");
    for (z = 0; z < storageIdCount; z++)
    {
      if (z > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[z]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT links.id,\
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   links.name, \
                                   links.destinationName, \
                                   links.timeModified, \
                                   links.userId, \
                                   links.groupId, \
                                   links.permission \
                            FROM links \
                              LEFT JOIN storage ON storage.id=links.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );
  DEBUG_ADD_RESOURCE_TRACE("indexQueryHandle",indexQueryHandle);

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextLink(IndexQueryHandle *indexQueryHandle,
                       DatabaseId       *databaseId,
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
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  assert(indexQueryHandle->indexHandle->initFlag);

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %S %llu %d %d %d",
                             databaseId,
                             storageName,
                             storageDateTime,
                             linkName,
                             destinationName,
                             timeModified,
                             userId,
                             groupId,
                             permission
                            );
}

Errors Index_deleteLink(IndexHandle *indexHandle,
                        DatabaseId  databaseId
                       )
{
  assert(indexHandle != NULL);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM links WHERE id=%ld;",
                          databaseId
                         );
}

Errors Index_initListHardLinks(IndexQueryHandle *indexQueryHandle,
                               IndexHandle      *indexHandle,
                               const DatabaseId *storageIds,
                               uint             storageIdCount,
                               String           pattern
                              )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"hardlinks.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(storage.id IN (");
    for (z = 0; z < storageIdCount; z++)
    {
      if (z > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[z]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT hardlinks.id,\
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   hardlinks.name, \
                                   hardlinks.size, \
                                   hardlinks.timeModified, \
                                   hardlinks.userId, \
                                   hardlinks.groupId, \
                                   hardlinks.permission, \
                                   hardlinks.fragmentOffset, \
                                   hardlinks.fragmentSize\
                            FROM hardlinks \
                              LEFT JOIN storage ON storage.id=hardlinks.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );
  DEBUG_ADD_RESOURCE_TRACE("indexQueryHandle",indexQueryHandle);

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextHardLink(IndexQueryHandle *indexQueryHandle,
                           DatabaseId       *databaseId,
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
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  assert(indexQueryHandle->indexHandle->initFlag);

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %llu %llu %d %d %d %llu %llu",
                             databaseId,
                             storageName,
                             storageDateTime,
                             fileName,
                             size,
                             timeModified,
                             userId,
                             groupId,
                             permission,
                             fragmentOffset,
                             fragmentSize
                            );
}

Errors Index_deleteHardLink(IndexHandle *indexHandle,
                            DatabaseId  databaseId
                           )
{
  assert(indexHandle != NULL);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM hardlinks WHERE id=%ld;",
                          databaseId
                         );
}

Errors Index_initListSpecial(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const DatabaseId *storageIds,
                             uint             storageIdCount,
                             String           pattern
                            )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(indexQueryHandle != NULL);
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  initIndexQueryHandle(indexQueryHandle,indexHandle);

  regexpString = getREGEXPString(String_new(),"special.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(storage.id IN (");
    for (z = 0; z < storageIdCount; z++)
    {
      if (z > 0) String_appendChar(storageIdsString,',');
      String_format(storageIdsString,"%d",storageIds[z]);
    }
    String_appendCString(storageIdsString,"))");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(&indexQueryHandle->databaseQueryHandle,
                           &indexHandle->databaseHandle,
                           "SELECT special.id,\
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   special.name, \
                                   special.timeModified, \
                                   special.userId, \
                                   special.groupId, \
                                   special.permission \
                            FROM special \
                              LEFT JOIN storage ON storage.id=special.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );
  DEBUG_ADD_RESOURCE_TRACE("indexQueryHandle",indexQueryHandle);

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextSpecial(IndexQueryHandle *indexQueryHandle,
                          DatabaseId       *databaseId,
                          String           storageName,
                          uint64           *storageDateTime,
                          String           name,
                          uint64           *timeModified,
                          uint32           *userId,
                          uint32           *groupId,
                          uint32           *permission
                         )
{
  assert(indexQueryHandle != NULL);
  assert(indexQueryHandle->indexHandle != NULL);
  assert(indexQueryHandle->indexHandle->initFlag);

  return Database_getNextRow(&indexQueryHandle->databaseQueryHandle,
                             "%lld %S %llu %S %llu %d %d %d",
                             databaseId,
                             storageName,
                             storageDateTime,
                             name,
                             timeModified,
                             userId,
                             groupId,
                             permission
                            );
}

Errors Index_deleteSpecial(IndexHandle *indexHandle,
                           DatabaseId  databaseId
                          )
{
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "DELETE FROM special WHERE id=%ld;",
                          databaseId
                         );
}

void Index_doneList(IndexQueryHandle *indexQueryHandle)
{
  assert(indexQueryHandle != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(indexQueryHandle);

  Database_finalize(&indexQueryHandle->databaseQueryHandle);
  doneIndexQueryHandle(indexQueryHandle);
}

Errors Index_addFile(IndexHandle  *indexHandle,
                     DatabaseId   storageId,
                     const String fileName,
                     uint64       size,
                     uint64       timeLastAccess,
                     uint64       timeModified,
                     uint64       timeLastChanged,
                     uint32       userId,
                     uint32       groupId,
                     uint32       permission,
                     uint64       fragmentOffset,
                     uint64       fragmentSize
                    )
{
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);
  assert(fileName != NULL);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "INSERT INTO files \
                             (\
                              storageId,\
                              name,\
                              size,\
                              timeLastAccess,\
                              timeModified,\
                              timeLastChanged,\
                              userId,\
                              groupId,\
                              permission,\
                              fragmentOffset,\
                              fragmentSize\
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %u,\
                              %u,\
                              %u,\
                              %lu,\
                              %lu\
                             ); \
                          ",
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

Errors Index_addImage(IndexHandle     *indexHandle,
                      DatabaseId      storageId,
                      const String    imageName,
                      FileSystemTypes fileSystemType,
                      int64           size,
                      ulong           blockSize,
                      uint64          blockOffset,
                      uint64          blockCount
                     )
{
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);
  assert(imageName != NULL);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "INSERT INTO images \
                             (\
                              storageId,\
                              name,\
                              fileSystemType,\
                              size,\
                              blockSize,\
                              blockOffset,\
                              blockCount\
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %d,\
                              %lu,\
                              %u,\
                              %lu,\
                              %lu\
                             );\
                          ",
                          storageId,
                          imageName,
                          fileSystemType,
                          size,
                          blockSize,
                          blockOffset,
                          blockCount
                         );
}

Errors Index_addDirectory(IndexHandle *indexHandle,
                          DatabaseId  storageId,
                          String      directoryName,
                          uint64      timeLastAccess,
                          uint64      timeModified,
                          uint64      timeLastChanged,
                          uint32      userId,
                          uint32      groupId,
                          uint32      permission
                         )
{
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);
  assert(directoryName != NULL);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "INSERT INTO directories \
                             (\
                              storageId,\
                              name,\
                              timeLastAccess,\
                              timeModified,\
                              timeLastChanged,\
                              userId,\
                              groupId,\
                              permission\
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %u,\
                              %u,\
                              %u \
                             );\
                          ",
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

Errors Index_addLink(IndexHandle  *indexHandle,
                     DatabaseId   storageId,
                     const String linkName,
                     const String destinationName,
                     uint64       timeLastAccess,
                     uint64       timeModified,
                     uint64       timeLastChanged,
                     uint32       userId,
                     uint32       groupId,
                     uint32       permission
                    )
{
  assert(indexHandle != NULL);
  assert(linkName != NULL);
  assert(destinationName != NULL);

  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "INSERT INTO links \
                             (\
                              storageId,\
                              name,\
                              destinationName,\
                              timeLastAccess,\
                              timeModified,\
                              timeLastChanged,\
                              userId,\
                              groupId,\
                              permission\
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %'S,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %u,\
                              %u,\
                              %u\
                             );\
                           ",
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

Errors Index_addHardLink(IndexHandle  *indexHandle,
                         DatabaseId   storageId,
                         const String fileName,
                         uint64       size,
                         uint64       timeLastAccess,
                         uint64       timeModified,
                         uint64       timeLastChanged,
                         uint32       userId,
                         uint32       groupId,
                         uint32       permission,
                         uint64       fragmentOffset,
                         uint64       fragmentSize
                        )
{
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);
  assert(fileName != NULL);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "INSERT INTO hardlinks \
                             (\
                              storageId,\
                              name,\
                              size,\
                              timeLastAccess,\
                              timeModified,\
                              timeLastChanged,\
                              userId,\
                              groupId,\
                              permission,\
                              fragmentOffset,\
                              fragmentSize\
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %u,\
                              %u,\
                              %u,\
                              %lu,\
                              %lu\
                             ); \
                          ",
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

Errors Index_addSpecial(IndexHandle      *indexHandle,
                        DatabaseId       storageId,
                        const String     name,
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
  assert(indexHandle != NULL);
  assert(indexHandle->initFlag);
  assert(name != NULL);

  return Database_execute(&indexHandle->databaseHandle,
                          CALLBACK(NULL,NULL),
                          "INSERT INTO special \
                             (\
                              storageId,\
                              name,\
                              specialType,\
                              timeLastAccess,\
                              timeModified,\
                              timeLastChanged,\
                              userId,\
                              groupId,\
                              permission,\
                              major,\
                              minor \
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %u,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %u,\
                              %u,\
                              %u,\
                              %d,\
                              %u\
                             );\
                          ",
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

#ifdef __cplusplus
  }
#endif

/* end of file */
