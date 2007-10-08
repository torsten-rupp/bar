/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.c,v $
* $Revision: 1.25 $
* $Author: torsten $
* Contents: Backup ARchiver archive create function
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"
#include "stringlists.h"
#include "msgqueues.h"
#include "semaphores.h"

#include "errors.h"
#include "patterns.h"
#include "files.h"
#include "archive.h"
#include "crypt.h"
#include "storage.h"

#include "commands_create.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_FILE_MSG_QUEUE_ENTRIES    256
#define MAX_STORAGE_MSG_QUEUE_ENTRIES 256

#define BUFFER_SIZE               (64*1024)

/***************************** Datatypes *******************************/

typedef struct
{
  String    fileName;
  FileTypes fileType;
} FileMsg;

typedef struct
{
  String fileName;
  uint64 fileSize;
  bool   completeFlag;
  String destinationFileName;
} StorageMsg;

typedef struct
{
  const char  *archiveFileName;
  PatternList *includePatternList;
  PatternList *excludePatternList;

  MsgQueue    fileMsgQueue;                         // queue with files to backup

  pthread_t   collectorSumThreadId;                 // files collector sum thread id
  pthread_t   collectorThreadId;                    // files collector thread id
  bool        collectorThreadExitFlag;

  MsgQueue    storageMsgQueue;                      // queue with storage files
  Semaphore   storageSemaphore;
  uint        storageCount;
  uint64      storageSize;
  pthread_t   storageThreadId;                      // storage thread id
  bool        storageThreadExitFlag;

  bool        failFlag;

  struct
  {
    ulong  includedCount;
    uint64 includedByteSum;
    ulong  excludedCount;
    ulong  skippedCount;
    ulong  errorCount;
  } statistics;

  CreateStatusInfoFunction statusInfoFunction;
  void                     *statusInfoUserData;
  CreateStatusInfo         statusInfo;              // status info
} CreateInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateStatusInfo(const CreateInfo *createInfo)
{
  assert(createInfo != NULL);

  if (createInfo->statusInfoFunction != NULL)
  {
    createInfo->statusInfoFunction(&createInfo->statusInfo,createInfo->statusInfoUserData);
  }
}

/***********************************************************************\
* Name   : checkIsIncluded
* Purpose: check if filename is included
* Input  : includePatternNode - include pattern node
*          fileName           - file name
* Output : -
* Return : TRUE if excluded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool checkIsIncluded(PatternNode *includePatternNode,
                           String      fileName
                          )
{
  assert(includePatternNode != NULL);
  assert(fileName != NULL);

  return Pattern_match(includePatternNode,fileName,PATTERN_MATCH_MODE_BEGIN);
}

/***********************************************************************\
* Name   : checkIsExcluded
* Purpose: check if filename is excluded
* Input  : excludePatternList - exclude pattern list
*          fileName           - file name
* Output : -
* Return : TRUE if excluded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool checkIsExcluded(PatternList *excludePatternList,
                           String      fileName
                          )
{
  assert(excludePatternList != NULL);
  assert(fileName != NULL);

  return Pattern_matchList(excludePatternList,fileName,PATTERN_MATCH_MODE_BEGIN);
}

/***********************************************************************\
* Name   : appendToFileList
* Purpose: append a filename to filename list
* Input  : fileMsgQueue - file message queue
*          fileName     - file name (will be copied!)
*          fileType     - file type
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendToFileList(MsgQueue  *fileMsgQueue,
                            String    fileName,
                            FileTypes fileType
                           )
{
  FileMsg fileMsg;

  assert(fileMsgQueue != NULL);
  assert(fileName != NULL);

  /* init */
  fileMsg.fileName = String_copy(fileName);
  fileMsg.fileType = fileType;

  /* put into message queue */
  MsgQueue_put(fileMsgQueue,&fileMsg,sizeof(fileMsg));
}

/***********************************************************************\
* Name   : freeFileMsg
* Purpose: free file msg
* Input  : fileMsg - file message
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeFileMsg(FileMsg *fileMsg, void *userData)
{
  assert(fileMsg != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(fileMsg->fileName);
}

/***********************************************************************\
* Name   : getNextFile
* Purpose: get next file from list of files to pack
* Input  : fileMsgQueue - file message queue
           fileName - file name variable
* Output : fileName - file name
*          fileType - file type
* Return : TRUE if file available, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool getNextFile(MsgQueue  *fileMsgQueue,
                       String    fileName,
                       FileTypes *fileType
                      )
{
  FileMsg fileMsg;

  assert(fileName != NULL);
  assert(fileType != NULL);

  if (MsgQueue_get(fileMsgQueue,&fileMsg,NULL,sizeof(fileMsg)))
  {
    String_set(fileName,fileMsg.fileName);
    (*fileType) = fileMsg.fileType;

    String_delete(fileMsg.fileName);

    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/***********************************************************************\
* Name   : collectorSumThread
* Purpose: file collector sum thread: only collect files and update
*          total files/bytes values
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void collectorSumThread(CreateInfo *createInfo)
{
  StringList      nameList;
  String          name;
  PatternNode     *includePatternNode;
  StringTokenizer fileNameTokenizer;
  String          basePath;
  String          s;
  Errors          error;
  String          fileName;
  FileInfo        fileInfo;
  DirectoryHandle directoryHandle;

  assert(createInfo != NULL);
  assert(createInfo->includePatternList != NULL);
  assert(createInfo->excludePatternList != NULL);

  StringList_init(&nameList);
  name = String_new();

  includePatternNode = createInfo->includePatternList->head;
  while (   !createInfo->collectorThreadExitFlag
         && !createInfo->failFlag
         && (includePatternNode != NULL)
        )
  {
    /* find base path */
    basePath = String_new();
    File_initSplitFileName(&fileNameTokenizer,includePatternNode->pattern);
    if (File_getNextSplitFileName(&fileNameTokenizer,&s) && !Pattern_checkIsPattern(name))
    {
      if (String_length(s) > 0)
      {
        File_setFileName(basePath,s);
      }
      else
      {
        File_setFileNameChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
      }
    }
    while (File_getNextSplitFileName(&fileNameTokenizer,&s) && !Pattern_checkIsPattern(name))
    {
      File_appendFileName(basePath,s);
    }
    File_doneSplitFileName(&fileNameTokenizer);

    /* find files */
    StringList_append(&nameList,basePath);
    while (   !createInfo->collectorThreadExitFlag
           && !createInfo->failFlag
           && !StringList_empty(&nameList)
          )
    {
      /* get next directory to process */
      name = StringList_getFirst(&nameList,name);
      if (   checkIsIncluded(includePatternNode,name)
          && !checkIsExcluded(createInfo->excludePatternList,name)
         )
      {
        switch (File_getType(name))
        {
          case FILETYPE_FILE:
            error = File_getFileInfo(name,&fileInfo);
            if (error == ERROR_NONE)
            {
              createInfo->statusInfo.totalFiles++;
              createInfo->statusInfo.totalBytes += fileInfo.size;
              updateStatusInfo(createInfo);
            }
            break;
          case FILETYPE_DIRECTORY:
            createInfo->statusInfo.totalFiles++;
            updateStatusInfo(createInfo);

            /* open directory contents */
            error = File_openDirectory(&directoryHandle,name);
            if (error == ERROR_NONE)
            {
              /* read directory contents */
              fileName = String_new();
              while (   !createInfo->collectorThreadExitFlag
                     && !createInfo->failFlag
                     && !File_endOfDirectory(&directoryHandle)
                    )
              {
                /* read next directory entry */
                error = File_readDirectory(&directoryHandle,fileName);
                if (error != ERROR_NONE)
                {
                  continue;
                }

                if (   checkIsIncluded(includePatternNode,fileName)
                    && !checkIsExcluded(createInfo->excludePatternList,fileName)
                   )
                {
                  /* detect file type */
                  switch (File_getType(fileName))
                  {
                    case FILETYPE_FILE:
                      error = File_getFileInfo(fileName,&fileInfo);
                      if (error == ERROR_NONE)
                      {
                        createInfo->statusInfo.totalFiles++;
                        createInfo->statusInfo.totalBytes += fileInfo.size;
                        updateStatusInfo(createInfo);
                      }
                      break;
                    case FILETYPE_DIRECTORY:
                      /* add to name list */
                      StringList_append(&nameList,fileName);
                      break;
                    case FILETYPE_LINK:
                      createInfo->statusInfo.totalFiles++;
                      updateStatusInfo(createInfo);
                      break;
                    default:
                      break;
                  }
                }
              }

              /* close directory, free resources */
              String_delete(fileName);
              File_closeDirectory(&directoryHandle);
            }
            break;
          case FILETYPE_LINK:
            createInfo->statusInfo.totalFiles++;
            updateStatusInfo(createInfo);
            break;
          default:
            break;
        }
      }
    }

    /* free resources */
    String_delete(basePath);

    /* next include pattern */
    includePatternNode = includePatternNode->next;
  }

  /* free resoures */
  String_delete(name);
  StringList_done(&nameList);
}

/***********************************************************************\
* Name   : collectorThread
* Purpose: file collector thread
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void collectorThread(CreateInfo *createInfo)
{
  StringList      nameList;
  String          name;
  PatternNode     *includePatternNode;
  StringTokenizer fileNameTokenizer;
  String          basePath;
  String          s;
  Errors          error;
  String          fileName;
  FileInfo        fileInfo;
  DirectoryHandle directoryHandle;

  assert(createInfo != NULL);
  assert(createInfo->includePatternList != NULL);
  assert(createInfo->excludePatternList != NULL);

  StringList_init(&nameList);
  name = String_new();

  includePatternNode = createInfo->includePatternList->head;
  while (   !createInfo->collectorThreadExitFlag
         && !createInfo->failFlag
         && (includePatternNode != NULL)
        )
  {
    /* find base path */
    basePath = String_new();
    File_initSplitFileName(&fileNameTokenizer,includePatternNode->pattern);
    if (File_getNextSplitFileName(&fileNameTokenizer,&s) && !Pattern_checkIsPattern(name))
    {
      if (String_length(s) > 0)
      {
        File_setFileName(basePath,s);
      }
      else
      {
        File_setFileNameChar(basePath,FILES_PATHNAME_SEPARATOR_CHAR);
      }
    }
    while (File_getNextSplitFileName(&fileNameTokenizer,&s) && !Pattern_checkIsPattern(name))
    {
      File_appendFileName(basePath,s);
    }
    File_doneSplitFileName(&fileNameTokenizer);

    /* find files */
    StringList_append(&nameList,basePath);
    while (   !createInfo->collectorThreadExitFlag
           && !createInfo->failFlag
           && !StringList_empty(&nameList)
          )
    {
      /* get next directory to process */
      name = StringList_getFirst(&nameList,name);
      if (   checkIsIncluded(includePatternNode,name)
          && !checkIsExcluded(createInfo->excludePatternList,name)
         )
      {
        switch (File_getType(name))
        {
          case FILETYPE_FILE:
            error = File_getFileInfo(name,&fileInfo);
            if (error == ERROR_NONE)
            {
              /* add to file list */
              appendToFileList(&createInfo->fileMsgQueue,name,FILETYPE_FILE);
            }
            else
            {
              createInfo->statistics.excludedCount++;
            }
            break;
          case FILETYPE_DIRECTORY:
            /* add to file list */
            appendToFileList(&createInfo->fileMsgQueue,name,FILETYPE_DIRECTORY);

            /* open directory contents */
            error = File_openDirectory(&directoryHandle,name);
            if (error == ERROR_NONE)
            {
              /* read directory contents */
              fileName = String_new();
              while (   !createInfo->collectorThreadExitFlag
                     && !createInfo->failFlag
                     && !File_endOfDirectory(&directoryHandle)
                    )
              {
                /* read next directory entry */
                error = File_readDirectory(&directoryHandle,fileName);
                if (error != ERROR_NONE)
                {
                  printError("Cannot read directory '%s' (error: %s)\n",
                             String_cString(name),
                             getErrorText(error)
                            );
                  createInfo->statistics.errorCount++;
                  continue;
                }

                if (   checkIsIncluded(includePatternNode,fileName)
                    && !checkIsExcluded(createInfo->excludePatternList,fileName)
                   )
                {
                  /* detect file type */
                  switch (File_getType(fileName))
                  {
                    case FILETYPE_FILE:
                      error = File_getFileInfo(fileName,&fileInfo);
                      if (error == ERROR_NONE)
                      {
                        /* add to file list */
                        appendToFileList(&createInfo->fileMsgQueue,fileName,FILETYPE_FILE);
                      }
                      else
                      {
                        createInfo->statistics.excludedCount++;
                      }
                      break;
                    case FILETYPE_DIRECTORY:
                      /* add to name list */
                      StringList_append(&nameList,fileName);
                      break;
                    case FILETYPE_LINK:
                      /* add to file list */
                      appendToFileList(&createInfo->fileMsgQueue,fileName,FILETYPE_LINK);
                      break;
                    default:
                      info(2,"Unknown type of file '%s' - skipped\n",String_cString(fileName));
                      createInfo->statistics.skippedCount++;
                      break;
                  }
                }
                else
                {
                  createInfo->statistics.excludedCount++;
                }
              }

              /* close directory, free resources */
              String_delete(fileName);
              File_closeDirectory(&directoryHandle);
            }
            else
            {
              printError("Cannot open directory '%s' (error: %s)\n",
                         String_cString(name),
                         getErrorText(error)
                        );
              createInfo->statistics.errorCount++;
            }
            break;
          case FILETYPE_LINK:
            /* add to file list */
            appendToFileList(&createInfo->fileMsgQueue,name,FILETYPE_LINK);
            break;
          default:
            info(2,"Unknown type of file '%s' - skipped\n",String_cString(name));
            createInfo->statistics.skippedCount++;
            break;
        }
      }
      else
      {
        createInfo->statistics.excludedCount++;
      }
    }

    /* free resources */
    String_delete(basePath);

    /* next include pattern */
    includePatternNode = includePatternNode->next;
  }
  MsgQueue_setEndOfMsg(&createInfo->fileMsgQueue);

  /* free resoures */
  String_delete(name);
  StringList_done(&nameList);

  createInfo->collectorThreadExitFlag = TRUE;
}

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : appendFileNameToList
* Purpose: append a filename to a filename list
* Input  : storageMsgQueue     - storage message queue
*          fileName            - file name (will be copied!)
*          fileSize            - file size (in bytes)
*          destinationFileName - destination file name (will be copied!)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void appendToStorageList(MsgQueue  *storageMsgQueue,
                               String    fileName,
                               uint64    fileSize,
                               String    destinationFileName
                              )
{
  StorageMsg storageMsg;

  assert(storageMsgQueue != NULL);
  assert(fileName != NULL);
  assert(destinationFileName != NULL);

  /* init */
  storageMsg.fileName            = String_copy(fileName);
  storageMsg.fileSize            = fileSize;
  storageMsg.destinationFileName = String_copy(destinationFileName);
  storageMsg.completeFlag        = TRUE;

  /* put into message queue */
  MsgQueue_put(storageMsgQueue,&storageMsg,sizeof(storageMsg));
}


/***********************************************************************\
* Name   : freeStorageMsg
* Purpose: free storage msg
* Input  : storageMsg - storage message
*          userData   - user data (ignored)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeStorageMsg(StorageMsg *storageMsg, void *userData)
{
  assert(storageMsg != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(storageMsg->fileName);
  String_delete(storageMsg->destinationFileName);
}

/***********************************************************************\
* Name   : storeArchiveFile
* Purpose: storage archive call back
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors storeArchiveFile(String fileName,
                              uint64 fileSize,
                              bool   completeFlag,
                              int    partNumber,
                              void   *userData
                             )
{
  CreateInfo *createInfo = (CreateInfo*)userData;
  const char *i0,*i1;
  ulong      divisor;
  ulong      n;
  int        d;
  String     destinationName;

  assert(createInfo != NULL);

  if (completeFlag)
  {
    /* get destination file name */
    if (partNumber >= 0)
    {
      i0 = strchr(createInfo->archiveFileName,'#');
      if (i0 != NULL)
      {
        /* find #...# and get max. divisor for part number */
        divisor = 1;
        i1 = i0+1;
        while ((*i1) == '#')
        {
          i1++;
          if (divisor < 1000000000) divisor*=10;
        }

        /* format destination file name */
        destinationName = String_newBuffer(createInfo->archiveFileName,(ulong)(i0-createInfo->archiveFileName));
        n = partNumber;
        while (divisor > 0)
        {
          d = n/divisor; n = n%divisor; divisor = divisor/10;
          String_appendChar(destinationName,'0'+d);
        }
        String_appendCString(destinationName,i1);
      }
      else
      {
        /* format destination file name */
        destinationName = String_format(String_new(),"%s.%06d",createInfo->archiveFileName,partNumber);
      }
    }
    else
    {
      destinationName = String_newCString(createInfo->archiveFileName);
    }

    /* send to storage controller */
    Semaphore_lock(&createInfo->storageSemaphore);
fprintf(stderr,"%s,%d: Semaphore locked by main\n",__FILE__,__LINE__);
    createInfo->storageCount += 1;
    createInfo->storageSize  += fileSize;
    appendToStorageList(&createInfo->storageMsgQueue,
                        fileName,
                        fileSize,
                        destinationName
                       );
    Semaphore_unlock(&createInfo->storageSemaphore);

    /* wait for space in temporary directory */
    if (globalOptions.maxTmpSize > 0)
    {
      Semaphore_lock(&createInfo->storageSemaphore);
fprintf(stderr,"%s,%d: Semaphore locked by main2\n",__FILE__,__LINE__);
      while ((createInfo->storageCount > 2) && (createInfo->storageSize > globalOptions.maxTmpSize))
      {
        Semaphore_wait(&createInfo->storageSemaphore);
      }
      Semaphore_unlock(&createInfo->storageSemaphore);
    }

    /* free resources */
    String_delete(destinationName);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : storage
* Purpose: archive storage thread
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void storageThread(CreateInfo *createInfo)
{
  byte        *buffer;
  StorageMsg  storageMsg;
  StorageInfo storageInfo;
  Errors      error;
  FileHandle  fileHandle;
  ulong       n;

  assert(createInfo != NULL);

  buffer = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  while (!createInfo->storageThreadExitFlag && MsgQueue_get(&createInfo->storageMsgQueue,&storageMsg,NULL,sizeof(storageMsg)))
  {
    if (!createInfo->failFlag)
    {
      info(0,"Store '%s'...",String_cString(storageMsg.destinationFileName));

      /* open storage */
      error = Storage_create(&storageInfo,storageMsg.destinationFileName,storageMsg.fileSize);
      if (error != ERROR_NONE)
      {
        info(0,"FAIL!\n");
        printError("Cannot store file '%s' (error: %s)\n",
                   String_cString(storageMsg.destinationFileName),
                   getErrorText(error)
                  );
        File_delete(storageMsg.fileName);
        String_delete(storageMsg.fileName);
        String_delete(storageMsg.destinationFileName);
        createInfo->failFlag = TRUE;
        continue;
      }
      String_set(createInfo->statusInfo.storageName,storageMsg.destinationFileName);
      updateStatusInfo(createInfo);

      /* store data */
      error = File_open(&fileHandle,storageMsg.fileName,FILE_OPENMODE_READ);
      if (error != ERROR_NONE)
      {
        info(0,"FAIL!\n");
        printError("Cannot open file '%s' (error: %s)!\n",
                   String_cString(storageMsg.fileName),
                   getErrorText(error)
                  );
        File_delete(storageMsg.fileName);
        String_delete(storageMsg.fileName);
        String_delete(storageMsg.destinationFileName);
        createInfo->failFlag = TRUE;
        continue;
      }
      do
      {
        error = File_read(&fileHandle,buffer,BUFFER_SIZE,&n);
        if (error != ERROR_NONE)
        {
          info(0,"FAIL!\n");
          printError("Cannot read file '%s' (error: %s)!\n",
                     String_cString(storageMsg.fileName),
                     getErrorText(error)
                    );
          createInfo->failFlag = TRUE;
          break;
        }        
        error = Storage_write(&storageInfo,buffer,n);
        if (error != ERROR_NONE)
        {
          info(0,"FAIL!\n");
          printError("Cannot write file '%s' (error: %s)!\n",
                     String_cString(storageMsg.destinationFileName),
                     getErrorText(error)
                    );
          createInfo->failFlag = TRUE;
          break;
        }        
      }  
      while (!createInfo->storageThreadExitFlag && !File_eof(&fileHandle));
      File_close(&fileHandle);

      /* close storage */
      Storage_close(&storageInfo);

      if (!createInfo->failFlag)
      {
        info(0,"ok\n");
      }
    }
else
{
fprintf(stderr,"%s,%d: FAIL - only delete files \n",__FILE__,__LINE__);
}
    /* delete source file */
    if (!File_delete(storageMsg.fileName))
    {
      warning("Cannot delete file '%s'!\n",
              String_cString(storageMsg.fileName)
             );
    }

    /* update storage info */
    Semaphore_lock(&createInfo->storageSemaphore);
fprintf(stderr,"%s,%d: Semaphore locked by storage\n",__FILE__,__LINE__);
    assert(createInfo->storageCount > 0);
    assert(createInfo->storageSize >= storageMsg.fileSize);
    createInfo->storageCount -= 1;
    createInfo->storageSize  -= storageMsg.fileSize;
    Semaphore_unlock(&createInfo->storageSemaphore);

    /* free resources */
    String_delete(storageMsg.fileName);
    String_delete(storageMsg.destinationFileName);
  }
fprintf(stderr,"%s,%d: storage end\n",__FILE__,__LINE__);

  /* free resoures */
  free(buffer);

  createInfo->storageThreadExitFlag = TRUE;
}

/*---------------------------------------------------------------------*/

bool Command_create(const char               *archiveFileName,
                    PatternList              *includePatternList,
                    PatternList              *excludePatternList,
                    ulong                    archivePartSize,
                    uint                     compressAlgorithm,
                    ulong                    compressMinFileSize,
                    CryptAlgorithms          cryptAlgorithm,
                    const char               *password,
                    CreateStatusInfoFunction createStatusInfoFunction,
                    void                     *createStatusInfoUserData
                   )
{
  CreateInfo      createInfo;
  ArchiveInfo     archiveInfo;
  byte            *buffer;
  Errors          error;
  String          fileName;
  FileTypes       fileType;
  ArchiveFileInfo archiveFileInfo;

  assert(archiveFileName != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);

  /* initialise variables */
  createInfo.archiveFileName             = archiveFileName;
  createInfo.includePatternList          = includePatternList;
  createInfo.excludePatternList          = excludePatternList;
  createInfo.failFlag                    = FALSE;
  createInfo.collectorThreadExitFlag     = FALSE;
  createInfo.storageCount                = 0;
  createInfo.storageSize                 = 0;
  createInfo.storageThreadExitFlag       = FALSE;
  createInfo.statistics.includedCount    = 0;
  createInfo.statistics.includedByteSum  = 0;
  createInfo.statistics.excludedCount    = 0;
  createInfo.statistics.skippedCount     = 0;
  createInfo.statusInfoFunction          = createStatusInfoFunction;
  createInfo.statusInfoUserData          = createStatusInfoUserData;
  createInfo.statusInfo.doneFiles        = 0;
  createInfo.statusInfo.doneBytes        = 0;
  createInfo.statusInfo.totalFiles       = 0;
  createInfo.statusInfo.totalBytes       = 0;
  createInfo.statusInfo.compressionRatio = 0.0;
  createInfo.statusInfo.fileName         = String_new();
  createInfo.statusInfo.storageName      = String_new();

  /* allocate resources */
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  fileName = String_new();

  /* init file name list, list locka and list signal */
  if (!MsgQueue_init(&createInfo.fileMsgQueue,MAX_FILE_MSG_QUEUE_ENTRIES))
  {
    HALT_FATAL_ERROR("Cannot initialise file message queue!");
  }
  if (!MsgQueue_init(&createInfo.storageMsgQueue,0))
  {
    HALT_FATAL_ERROR("Cannot initialise storage message queue!");
  }
  if (!Semaphore_init(&createInfo.storageSemaphore))
  {
    HALT_FATAL_ERROR("Cannot initialise storage semaphore!");
  }

  /* prepare storage */
  error = Storage_prepare(String_setCString(fileName,archiveFileName));
  if (error != ERROR_NONE)
  {
    printError("Cannot prepare storage of file '%s' (error: %s)\n",
               archiveFileName,
               getErrorText(error)
              );
    Semaphore_done(&createInfo.storageSemaphore);
    MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
    MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
    String_delete(fileName);
    free(buffer);
    String_delete(createInfo.statusInfo.fileName);
    String_delete(createInfo.statusInfo.storageName);

    return FALSE;
  }

  /* create new archive */
  error = Archive_create(&archiveInfo,
                         storeArchiveFile,
                         &createInfo,
                         globalOptions.tmpDirectory,
                         archivePartSize,
                         compressAlgorithm,
                         compressMinFileSize,
                         cryptAlgorithm,
                         password
                        );
  if (error != ERROR_NONE)
  {
    printError("Cannot create archive file '%s' (error: %s)\n",
               archiveFileName,
               getErrorText(error)
              );
    Semaphore_done(&createInfo.storageSemaphore);
    MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
    MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
    String_delete(fileName);
    free(buffer);
    String_delete(createInfo.statusInfo.fileName);
    String_delete(createInfo.statusInfo.storageName);

    return FALSE;
  }

  /* start threads */
  if (pthread_create(&createInfo.collectorSumThreadId,NULL,(void*(*)(void*))collectorSumThread,&createInfo) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise collector sum thread!");
  }
  if (pthread_create(&createInfo.collectorThreadId,NULL,(void*(*)(void*))collectorThread,&createInfo) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise collector thread!");
  }
  if (pthread_create(&createInfo.storageThreadId,NULL,(void*(*)(void*))storageThread,&createInfo) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise storage thread!");
  }

  /* store files */
  while (getNextFile(&createInfo.fileMsgQueue,fileName,&fileType))
  {
    if (!createInfo.failFlag)
    {
      info(1,"Add '%s'...",String_cString(fileName));

      switch (fileType)
      {
        case FILETYPE_FILE:
          {
            FileInfo   fileInfo;
            FileHandle fileHandle;
            ulong      n;
            double     ratio;

            /* get file info */
            error = File_getFileInfo(fileName,&fileInfo);
            if (error != ERROR_NONE)
            {
              info(1,"FAIL\n");
              printError("Cannot get info for file '%s' (error: %s)\n",
                         String_cString(fileName),
                         getErrorText(error)
                        );
              createInfo.failFlag = TRUE;
              continue;
            }

            /* open file */  
            error = File_open(&fileHandle,fileName,FILE_OPENMODE_READ);
            if (error != ERROR_NONE)
            {
              if (globalOptions.skipUnreadableFlag)
              {
                info(1,"skipped (reason: %s)\n",
                     getErrorText(error)
                    );
              }
              else
              {
                info(1,"FAIL\n");
                printError("Cannot open file '%s' (error: %s)\n",
                           String_cString(fileName),
                           getErrorText(error)
                          );
                createInfo.failFlag = TRUE;
              }
              continue;
            }

            /* new file */
            error = Archive_newFileEntry(&archiveInfo,
                                         &archiveFileInfo,
                                         fileName,
                                         &fileInfo
                                        );
            if (error != ERROR_NONE)
            {
              info(1,"FAIL\n");
              printError("Cannot create new archive entry '%s' (error: %s)\n",
                         String_cString(fileName),
                         getErrorText(error)
                        );
              createInfo.failFlag = TRUE;
              break;
            }
            String_set(createInfo.statusInfo.fileName,fileName);
            updateStatusInfo(&createInfo);

            /* write file content into archive */
            error = ERROR_NONE;
            do
            {
              File_read(&fileHandle,buffer,BUFFER_SIZE,&n);
              if (n > 0)
              {
                error = Archive_writeFileData(&archiveFileInfo,buffer,n);
                createInfo.statusInfo.doneBytes += n;
                updateStatusInfo(&createInfo);
              }
            }
            while (   (n > 0)
                   && !createInfo.failFlag
                   && (error == ERROR_NONE)
                  );            
            if (error != ERROR_NONE)
            {
              info(1,"FAIL\n");
              printError("Cannot create archive file (error: %s)!\n",
                         getErrorText(error)
                        );
              File_close(&fileHandle);
              Archive_closeEntry(&archiveFileInfo);
              createInfo.failFlag = TRUE;
              break;
            }

            /* close file */
            File_close(&fileHandle);
            createInfo.statusInfo.doneFiles++;
            updateStatusInfo(&createInfo);

            /* close archive entry */
            error = Archive_closeEntry(&archiveFileInfo);
            if (error != ERROR_NONE)
            {
              info(1,"FAIL\n");
              printError("Cannot close archive file (error: %s)!\n",
                         getErrorText(error)
                        );
              createInfo.failFlag = TRUE;
              break;
            }

            /* update statistics */
            createInfo.statistics.includedCount++;
            createInfo.statistics.includedByteSum += fileInfo.size;

            if ((archiveFileInfo.file.compressAlgorithm != COMPRESS_ALGORITHM_NONE) && (archiveFileInfo.file.chunkFileData.fragmentSize > 0))
            {
              ratio = 100.0-archiveFileInfo.file.chunkInfoFileData.size*100.0/archiveFileInfo.file.chunkFileData.fragmentSize;
            }
            else
            {
              ratio = 0;
            }
            info(1,"ok (%llu bytes, ratio %.1f%%)\n",fileInfo.size,ratio);
          }
          break;
        case FILETYPE_DIRECTORY:
          {
            FileInfo fileInfo;

            /* get directory info */
            error = File_getFileInfo(fileName,&fileInfo);
            if (error != ERROR_NONE)
            {
              info(1,"FAIL\n");
              printError("Cannot get info for directory '%s' (error: %s)\n",
                         String_cString(fileName),
                         getErrorText(error)
                        );
              createInfo.failFlag = TRUE;
              continue;
            }

            /* new directory */
            error = Archive_newDirectoryEntry(&archiveInfo,
                                              &archiveFileInfo,
                                              fileName,
                                              &fileInfo
                                             );
            if (error != ERROR_NONE)
            {
              info(1,"FAIL\n");
              printError("Cannot create new archive entry '%s' (error: %s)\n",
                         String_cString(fileName),
                         getErrorText(error)
                        );
              createInfo.failFlag = TRUE;
              break;
            }

            /* close archive entry */
            Archive_closeEntry(&archiveFileInfo);
            createInfo.statusInfo.doneFiles++;
            updateStatusInfo(&createInfo);

            /* free resources */

            /* update statistics */
            createInfo.statistics.includedCount++;

            info(1,"ok\n");
          }
          break;
        case FILETYPE_LINK:
          {
            FileInfo fileInfo;
            String   name;

            /* get file info */
            error = File_getFileInfo(fileName,&fileInfo);
            if (error != ERROR_NONE)
            {
              info(1,"FAIL\n");
              printError("Cannot get info for file '%s' (error: %s)\n",
                         String_cString(fileName),
                         getErrorText(error)
                        );
              createInfo.failFlag = TRUE;
              continue;
            }

            /* read link */
            name = String_new();
            error = File_readLink(fileName,name);
            if (error != ERROR_NONE)
            {
              info(1,"FAIL\n");
              printError("Cannot read link '%s' (error: %s)\n",
                         String_cString(fileName),
                         getErrorText(error)
                        );
              String_delete(name);
              createInfo.failFlag = TRUE;
              continue;
            }

            /* new link */
            error = Archive_newLinkEntry(&archiveInfo,
                                         &archiveFileInfo,
                                         fileName,
                                         name,
                                         &fileInfo
                                        );
            if (error != ERROR_NONE)
            {
              info(1,"FAIL\n");
              printError("Cannot create new archive entry '%s' (error: %s)\n",
                         String_cString(fileName),
                         getErrorText(error)
                        );
              String_delete(name);
              createInfo.failFlag = TRUE;
              break;
            }

            /* close archive entry */
            Archive_closeEntry(&archiveFileInfo);
            createInfo.statusInfo.doneFiles++;
            updateStatusInfo(&createInfo);

            /* free resources */
            String_delete(name);

            /* update statistics */
            createInfo.statistics.includedCount++;

            info(1,"ok\n");
          }
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }
  }

  /* close archive */
  Archive_close(&archiveInfo);
  MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue);

  /* wait for threads */
  pthread_join(createInfo.storageThreadId,NULL);
  pthread_join(createInfo.collectorThreadId,NULL);
  pthread_join(createInfo.collectorSumThreadId,NULL);

  /* output statics */
  info(0,"%lu file(s)/%llu bytes(s) included\n",createInfo.statistics.includedCount,createInfo.statistics.includedByteSum);
  info(2,"%lu file(s) excluded\n",createInfo.statistics.excludedCount);
  info(2,"%lu file(s) skipped\n",createInfo.statistics.skippedCount);
  info(2,"%lu error(s)\n",createInfo.statistics.errorCount);

  /* free resources */
  Semaphore_done(&createInfo.storageSemaphore);
  MsgQueue_done(&createInfo.storageMsgQueue,(MsgQueueMsgFreeFunction)freeStorageMsg,NULL);
  MsgQueue_done(&createInfo.fileMsgQueue,(MsgQueueMsgFreeFunction)freeFileMsg,NULL);
  String_delete(fileName);
  free(buffer);
  String_delete(createInfo.statusInfo.fileName);
  String_delete(createInfo.statusInfo.storageName);

  return !createInfo.failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
