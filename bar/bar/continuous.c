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
#ifdef HAVE_SYS_INOTIFY_H
  #include <sys/inotify.h>
#endif
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "threads.h"
#include "strings.h"
#include "files.h"
#include "database.h"
#include "arrays.h"
#include "dictionaries.h"
#include "msgqueues.h"
#include "misc.h"
#include "errors.h"

#include "entrylists.h"
#include "patternlists.h"
#include "bar_global.h"
#include "bar.h"

#include "continuous.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

//#define NOTIFY_EVENTS IN_ALL_EVENTS
#define NOTIFY_EVENTS (IN_CLOSE_WRITE|IN_DELETE|IN_DELETE_SELF|IN_MODIFY|IN_MOVE_SELF|IN_MOVED_FROM|IN_MOVED_TO)

// sleep time [s]
//#define SLEEP_TIME_CONTINUOUS_CLEANUP_THREAD (4*60*60)

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

typedef struct
{
  int    watchHandle;
  char   jobUUID[MISC_UUID_STRING_LENGTH+1];
  String path;
} FileNotifyInfo;

// init notify message
typedef struct
{
  String      jobUUID;
  EntryList   includeEntryList;
  PatternList excludePatternList;
  JobOptions  jobOptions;
} InitNotifyMsg;

/***************************** Variables *******************************/
LOCAL Semaphore      fileNotifyLock;
LOCAL Array          fileNotifyHandles;
LOCAL Dictionary     fileNotifyDictionary;
LOCAL DatabaseHandle continuousDatabaseHandle;
LOCAL int            inotifyHandle;
LOCAL MsgQueue       initNotifyMsgQueue;
LOCAL Thread         continuousInitThread;
LOCAL Thread         continuousThread;
LOCAL Thread         continuousCleanupThread;
LOCAL bool           quitFlag;

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
UNUSED_VARIABLE(databaseHandle);
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
UNUSED_VARIABLE(databaseHandle);
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
* Name   : freeFileNotifyInfo
* Purpose: free file notify info
* Input  : fileNotifyInfo - file notify info
*          userData       - not used
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeFileNotifyInfo(FileNotifyInfo *fileNotifyInfo, void *userData)
{
  assert(fileNotifyInfo != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(fileNotifyInfo->path);
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
UNUSED_VARIABLE(databaseHandle);
#if 0
  String              absoluteFileName;
  String              pathName;
  Errors              error;
  DirectoryListHandle directoryListHandle;
  String              prefixFileName;
  String              oldDatabaseFileName;
  uint                sleepTime;

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

/***********************************************************************\
* Name   : freeInitNotifyMsg
* Purpose: free init notify msg
* Input  : initNotifyMsg - init notify message
*          userData      - user data (ignored)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeInitNotifyMsg(InitNotifyMsg *initNotifyMsg, void *userData)
{
  assert(initNotifyMsg != NULL);

  UNUSED_VARIABLE(userData);

  PatternList_done(&initNotifyMsg->excludePatternList);
  EntryList_done(&initNotifyMsg->includeEntryList);
  String_delete(initNotifyMsg->jobUUID);
}

/***********************************************************************\
* Name   : continuousInitThreadCode
* Purpose: continuous file thread code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void continuousInitThreadCode(void)
{
  InitNotifyMsg       initNotifyMsg;
  StringList          nameList;
  String              basePath;
  String              name;

  EntryNode           *includeEntryNode;
  StringTokenizer     fileNameTokenizer;
  ConstString         token;
  Errors              error;
  FileInfo            fileInfo;
  SemaphoreLock       semaphoreLock;
  DirectoryListHandle directoryListHandle;
  int                 watchHandle;
  FileNotifyInfo      *fileNotifyInfo;

  // init variables
  StringList_init(&nameList);
  basePath = String_new();
  name     = String_new();

  while (   !quitFlag
         && MsgQueue_get(&initNotifyMsgQueue,&initNotifyMsg,NULL,sizeof(initNotifyMsg),WAIT_FOREVER)
        )
  {
fprintf(stderr,"%s, %d: i %d\n",__FILE__,__LINE__,List_count(&initNotifyMsg.includeEntryList));
    LIST_ITERATEX(&initNotifyMsg.includeEntryList,includeEntryNode,!quitFlag)
    {
      // find base path
      File_initSplitFileName(&fileNameTokenizer,includeEntryNode->string);
      if (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
      {
        if (!String_isEmpty(token))
        {
          File_setFileName(basePath,token);
        }
        else
        {
          File_setFileNameChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
        }
      }
      while (File_getNextSplitFileName(&fileNameTokenizer,&token) && !Pattern_checkIsPattern(token))
      {
        File_appendFileName(basePath,token);
      }
      File_doneSplitFileName(&fileNameTokenizer);

      // find directories and create notify entries
      StringList_append(&nameList,basePath);
      while (   !StringList_isEmpty(&nameList)
             && !quitFlag
            )
      {
        // get next entry to process
        name = StringList_getLast(&nameList,name);

        // read file info
        error = File_getFileInfo(name,&fileInfo);
        if (error != ERROR_NONE)
        {
          continue;
        }

        if (!isNoDumpAttribute(&fileInfo,&initNotifyMsg.jobOptions) && !isNoBackup(name))
        {
#if 1
          // create notify
          watchHandle = inotify_add_watch(inotifyHandle,String_cString(name),NOTIFY_EVENTS);
          if (watchHandle == -1)
          {
            printWarning(_("Cannot create file notify for '%s' (error: %s)\n"),String_cString(name),strerror(errno));
            continue;
          }

          // init file notify info
          fileNotifyInfo = (FileNotifyInfo*)malloc(sizeof(FileNotifyInfo));
          if (fileNotifyInfo == NULL)
          {
            continue;
          }
          fileNotifyInfo->watchHandle = watchHandle;
          strncpy(fileNotifyInfo->jobUUID,String_cString(initNotifyMsg.jobUUID),sizeof(fileNotifyInfo->jobUUID));
          fileNotifyInfo->path = String_duplicate(name);

          // add to file notify dictionary
          SEMAPHORE_LOCKED_DO(semaphoreLock,&fileNotifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
          {
            Array_put(&fileNotifyHandles,(ulong)watchHandle,fileNotifyInfo);//,CALLBACK(NULL,NULL));

            Dictionary_add(&fileNotifyDictionary,
                           &fileNotifyInfo->watchHandle,sizeof(fileNotifyInfo->watchHandle),
                           fileNotifyInfo,sizeof(FileNotifyInfo),
                           CALLBACK(NULL,NULL)
                          );
          }
fprintf(stderr,"%s, %d: create notify %s: wd=%d\n",__FILE__,__LINE__,String_cString(name),watchHandle);
#endif

          // scan sub-directories
          if (fileInfo.type == FILE_TYPE_DIRECTORY)
          {
            // open directory contents
            error = File_openDirectoryList(&directoryListHandle,name);
            if (error == ERROR_NONE)
            {
              // read directory content
              while (   !File_endOfDirectoryList(&directoryListHandle)
                     && !quitFlag
                    )
              {
                // read next directory entry
                error = File_readDirectoryList(&directoryListHandle,name);
                if (error != ERROR_NONE)
                {
                  continue;
                }
//fprintf(stderr,"%s, %d: %s included=%d excluded=%d dictionary=%d\n",__FILE__,__LINE__,String_cString(fileName),isIncluded(includeEntryNode,fileName),isExcluded(createInfo->excludePatternList,fileName),Dictionary_contains(&duplicateNamesDictionary,String_cString(fileName),String_length(fileName)));

                if (   isIncluded(includeEntryNode,name)
                    && !isExcluded(&initNotifyMsg.excludePatternList,name)
                    )
                {
                  // read file info
                  error = File_getFileInfo(name,&fileInfo);
                  if (error != ERROR_NONE)
                  {
                    continue;
                  }

                  // add to name list
                  if (!isNoDumpAttribute(&fileInfo,&initNotifyMsg.jobOptions) && !isNoBackup(name))
                  {
                    if (fileInfo.type == FILE_TYPE_DIRECTORY)
                    {
                      StringList_append(&nameList,name);
                    }
                  }
                }
              }

              // close directory
              File_closeDirectoryList(&directoryListHandle);
            }
          }
        }
      }
    }

    // free init notify message
    freeInitNotifyMsg(&initNotifyMsg,NULL);
  }

  // free resources
}

/***********************************************************************\
* Name   : continuousThreadCode
* Purpose: continuous file thread code
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void continuousThreadCode(void)
{
  #define MAX_ENTRIES 128
  #define BUFFER_SIZE (MAX_ENTRIES*(sizeof(struct inotify_event)+NAME_MAX+1))

  void                       *buffer;
  StaticString               (jobUUID,MISC_UUID_STRING_LENGTH);
  String                     name;
  ssize_t                    n;
  const struct inotify_event *inotifyEvent;
  SemaphoreLock              semaphoreLock;
  FileNotifyInfo             *fileNotifyInfo;

  // init variables
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  name = String_new();

  while (!quitFlag)
  {
    // read inotify events
    do
    {
      n = read(inotifyHandle,buffer,BUFFER_SIZE);
//fprintf(stderr,"%s, %d: xxxxxx inotifyHandle=%d n=%d: %d %s\n",__FILE__,__LINE__,inotifyHandle,n,errno,strerror(errno));
    }
    while ((n == -1) && ((errno == EAGAIN) || (errno == EINTR)));

    // process inotify events
    inotifyEvent = (const struct inotify_event*)buffer;
    while (n > 0)
    {
//fprintf(stderr,"%s, %d: n=%d\n",__FILE__,__LINE__,n);
      if (inotifyEvent->len > 0)
      {
fprintf(stderr,"%s, %d: inotify event wd=%d mask=%08x: name=%s\n",__FILE__,__LINE__,inotifyEvent->wd,inotifyEvent->mask,inotifyEvent->name);
   if (inotifyEvent->mask & IN_ACCESS)        fprintf(stderr," IN_ACCESS"       );
   if (inotifyEvent->mask & IN_ATTRIB)        fprintf(stderr," IN_ATTRIB"       );
   if (inotifyEvent->mask & IN_CLOSE_NOWRITE) fprintf(stderr," IN_CLOSE_NOWRITE");
   if (inotifyEvent->mask & IN_CLOSE_WRITE)   fprintf(stderr," IN_CLOSE_WRITE"  );
   if (inotifyEvent->mask & IN_CREATE)        fprintf(stderr," IN_CREATE"       );
   if (inotifyEvent->mask & IN_DELETE)        fprintf(stderr," IN_DELETE"       );
   if (inotifyEvent->mask & IN_DELETE_SELF)   fprintf(stderr," IN_DELETE_SELF"  );
   if (inotifyEvent->mask & IN_IGNORED)       fprintf(stderr," IN_IGNORED"      );
   if (inotifyEvent->mask & IN_ISDIR)         fprintf(stderr," IN_ISDIR"        );
   if (inotifyEvent->mask & IN_MODIFY)        fprintf(stderr," IN_MODIFY"       );
   if (inotifyEvent->mask & IN_MOVE_SELF)     fprintf(stderr," IN_MOVE_SELF"    );
   if (inotifyEvent->mask & IN_MOVED_FROM)    fprintf(stderr," IN_MOVED_FROM"   );
   if (inotifyEvent->mask & IN_MOVED_TO)      fprintf(stderr," IN_MOVED_TO"     );
   if (inotifyEvent->mask & IN_OPEN)          fprintf(stderr," IN_OPEN"         );
   if (inotifyEvent->mask & IN_Q_OVERFLOW)    fprintf(stderr," IN_Q_OVERFLOW"   );
   if (inotifyEvent->mask & IN_UNMOUNT)       fprintf(stderr," IN_UNMOUNT"      );
fprintf(stderr,"\n");


        // get job UUID, file name
        String_clear(jobUUID);
        SEMAPHORE_LOCKED_DO(semaphoreLock,&fileNotifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
        {
          if (Dictionary_find(&fileNotifyDictionary,
                              &inotifyEvent->wd,
                              sizeof(inotifyEvent->wd),
                              (void**)&fileNotifyInfo,
                              NULL
                             )
             )
          {
            String_setCString(jobUUID,fileNotifyInfo->jobUUID);
            File_appendFileNameCString(String_set(name,fileNotifyInfo->path),inotifyEvent->name);
fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(name));
          }
        }

        // store into notify database
        if (!String_isEmpty(jobUUID))
        {
          Continuous_add(jobUUID,name);
        }
      }

      // next event
      inotifyEvent = (const struct inotify_event*)((byte*)inotifyEvent+sizeof(struct inotify_event)+inotifyEvent->len);
      n -= sizeof(struct inotify_event)+inotifyEvent->len;
    }
    assert(n == 0);
  }

  // free resources
  String_delete(name);
  free(buffer);
}

/*---------------------------------------------------------------------*/

Errors Continuous_initAll(void)
{
  Semaphore_init(&fileNotifyLock);
  Array_init(&fileNotifyHandles,sizeof(FileNotifyInfo),0,CALLBACK((ArrayElementFreeFunction)freeFileNotifyInfo,NULL),CALLBACK_NULL);
  Dictionary_init(&fileNotifyDictionary,CALLBACK((DictionaryFreeFunction)freeFileNotifyInfo,NULL),CALLBACK_NULL);

  inotifyHandle = inotify_init();
  if (inotifyHandle == -1)
  {
    return ERRORX_(OPEN_FILE,errno,"inotify");
  }

  if (!MsgQueue_init(&initNotifyMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialize init notify message queue!");
  }

  return ERROR_NONE;
}

void Continuous_doneAll(void)
{
  MsgQueue_done(&initNotifyMsgQueue,CALLBACK((MsgQueueMsgFreeFunction)freeInitNotifyMsg,NULL));
  close(inotifyHandle);
  Dictionary_done(&fileNotifyDictionary);
  Semaphore_done(&fileNotifyLock);
}

Errors Continuous_init(const char *databaseFileName)
{
  Errors error;
  int64  continuousVersion;

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

  // start threads
  if (!Thread_init(&continuousInitThread,"BAR continuous init",globalOptions.niceLevel,continuousInitThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize continuous init thread!");
  }
  if (!Thread_init(&continuousThread,"BAR continuous",globalOptions.niceLevel,continuousThreadCode,NULL))
  {
    HALT_FATAL_ERROR("Cannot initialize continuous thread!");
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

void Continuous_done(void)
{
  quitFlag = TRUE;
  MsgQueue_setEndOfMsg(&initNotifyMsgQueue);
//  Thread_join(&cleanupContinuousThread);
  Thread_join(&continuousThread);
  Thread_join(&continuousInitThread);

  Thread_done(&continuousThread);
  Thread_done(&continuousInitThread);

  (void)closeContinuous(&continuousDatabaseHandle);
}

Errors Continuous_initNotify(ConstString       jobUUID,
                             const EntryList   *includeEntryList,
                             const PatternList *excludePatternList,
                             const JobOptions  *jobOptions
                            )
{
  InitNotifyMsg initNotifyMsg;

  initNotifyMsg.jobUUID = String_duplicate(jobUUID);
  EntryList_init(&initNotifyMsg.includeEntryList); EntryList_copy(includeEntryList,&initNotifyMsg.includeEntryList,NULL,NULL);
  PatternList_init(&initNotifyMsg.excludePatternList); PatternList_copy(excludePatternList,&initNotifyMsg.excludePatternList,NULL,NULL);
  copyJobOptions(jobOptions,&initNotifyMsg.jobOptions);

  (void)MsgQueue_put(&initNotifyMsgQueue,&initNotifyMsg,sizeof(initNotifyMsg));

  return ERROR_NONE;
}

Errors Continuous_doneNotify(ConstString jobUUID)
{
  SemaphoreLock semaphoreLock;

  SEMAPHORE_LOCKED_DO(semaphoreLock,&fileNotifyLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {

  }
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

bool Continuous_isAvailable(ConstString jobUUID)
{
  bool                isAvailable;
  DatabaseQueryHandle databaseQueryHandle;

  if (Database_prepare(&databaseQueryHandle,
                       &continuousDatabaseHandle,
                       "SELECT id FROM names WHERE jobUUID=%'S LIMIT 0,1",
                       jobUUID
                      ) != ERROR_NONE
     )
  {
    return FALSE;
  }

  isAvailable = Database_getNextRow(&databaseQueryHandle,
                                    "%lld",
                                    NULL
                                   );

  Database_finalize(&databaseQueryHandle);

  return isAvailable;
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
