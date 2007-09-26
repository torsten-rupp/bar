/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_create.c,v $
* $Revision: 1.16 $
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

#include "errors.h"
#include "patterns.h"
#include "files.h"
#include "archive.h"
#include "crypt.h"
#include "storage.h"

#include "command_create.h"

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

  MsgQueue    fileMsgQueue;

  bool        collectorThreadExitFlag;
  pthread_t   collectorThread;

  MsgQueue    storageMsgQueue;
  bool        storageThreadExitFlag;
  pthread_t   storageThread;

  bool        failFlag;

  struct
  {
    ulong  includedCount;
    uint64 includedByteSum;
    ulong  excludedCount;
    ulong  skippedCount;
    ulong  errorCount;
  } statistics;
} CreateInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

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

  return Patterns_match(includePatternNode,fileName,PATTERN_MATCH_MODE_BEGIN);
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

  return Patterns_matchList(excludePatternList,fileName,PATTERN_MATCH_MODE_BEGIN);
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
* Name   : collector
* Purpose: file collector thread
* Input  : createInfo - create info block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void collector(CreateInfo *createInfo)
{
  StringList      nameList;
  String          name;
  PatternNode     *includePatternNode;
  StringTokenizer fileNameTokenizer;
  String          basePath;
  String          s;
  Errors          error;
  String          fileName;
  DirectoryHandle directoryHandle;

  assert(createInfo != NULL);
  assert(createInfo->includePatternList != NULL);
  assert(createInfo->excludePatternList != NULL);

  StringList_init(&nameList);
  name = String_new();

  includePatternNode = createInfo->includePatternList->head;
  while (!createInfo->collectorThreadExitFlag && (includePatternNode != NULL))
  {
    /* find base path */
    basePath = String_new();
    File_initSplitFileName(&fileNameTokenizer,includePatternNode->pattern);
    if (File_getNextSplitFileName(&fileNameTokenizer,&s) && !Patterns_checkIsPattern(name))
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
    while (File_getNextSplitFileName(&fileNameTokenizer,&s) && !Patterns_checkIsPattern(name))
    {
      File_appendFileName(basePath,s);
    }
    File_doneSplitFileName(&fileNameTokenizer);

    /* find files */
    StringList_append(&nameList,basePath);
    while (!createInfo->collectorThreadExitFlag && !StringList_empty(&nameList))
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
            /* add to file list */
            appendToFileList(&createInfo->fileMsgQueue,name,FILETYPE_FILE);
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
              while (!File_endOfDirectory(&directoryHandle))
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
                      /* add to file list */
                      appendToFileList(&createInfo->fileMsgQueue,fileName,FILETYPE_FILE);
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
  StringList_done(&nameList,NULL);

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
      /* get destination file name */
      destinationName = String_newCString(createInfo->archiveFileName);
    }

    appendToStorageList(&createInfo->storageMsgQueue,
                        fileName,
                        fileSize,
                        destinationName
                       );

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

LOCAL void storage(CreateInfo *createInfo)
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
//fprintf(stderr,"%s,%d: XXXXXXX storage %s -> %s\n",__FILE__,__LINE__,String_cString(storageMsg.fileName),
//String_cString(storageMsg.destinationFileName));
    /* open storage */
    error = Storage_create(&storageInfo,storageMsg.destinationFileName,storageMsg.fileSize);
    if (error != ERROR_NONE)
    {
      printError("Cannot store file '%s'\n",
                 String_cString(storageMsg.destinationFileName),
                 getErrorText(error)
                );
      File_delete(storageMsg.fileName);
      String_delete(storageMsg.fileName);
      String_delete(storageMsg.destinationFileName);
      createInfo->failFlag = TRUE;
      continue;
    }

    /* store data */
    error = File_open(&fileHandle,storageMsg.fileName,FILE_OPENMODE_READ);
    if (error != ERROR_NONE)
    {
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

    /* delete source file */
    if (!File_delete(storageMsg.fileName))
    {
      warning("Cannot delete file '%s'!\n",
              String_cString(storageMsg.fileName)
             );
    }

    /* free resources */
    String_delete(storageMsg.fileName);
    String_delete(storageMsg.destinationFileName);
  }

  /* free resoures */
  free(buffer);

  createInfo->storageThreadExitFlag = TRUE;
}

/*---------------------------------------------------------------------*/

bool command_create(const char      *archiveFileName,
                    PatternList     *includePatternList,
                    PatternList     *excludePatternList,
                    const char      *tmpDirectory,
                    ulong           partSize,
                    uint            compressAlgorithm,
                    ulong           compressMinFileSize,
                    CryptAlgorithms cryptAlgorithm,
                    const char      *password
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
  createInfo.archiveFileName            = archiveFileName;
  createInfo.includePatternList         = includePatternList;
  createInfo.excludePatternList         = excludePatternList;
  createInfo.failFlag                   = FALSE;
  createInfo.collectorThreadExitFlag    = FALSE;
  createInfo.storageThreadExitFlag      = FALSE;
  createInfo.statistics.includedCount   = 0;
  createInfo.statistics.includedByteSum = 0;
  createInfo.statistics.excludedCount   = 0;
  createInfo.statistics.skippedCount    = 0;

  /* allocate resources */
  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init file name list, list locka and list signal */
  if (!MsgQueue_init(&createInfo.fileMsgQueue,MAX_FILE_MSG_QUEUE_ENTRIES))
  {
    HALT_FATAL_ERROR("Cannot initialise file message queue!");
  }
  if (!MsgQueue_init(&createInfo.storageMsgQueue,MAX_STORAGE_MSG_QUEUE_ENTRIES))
  {
    HALT_FATAL_ERROR("Cannot initialise storage message queue!");
  }

  /* create new archive */
  error = Archive_create(&archiveInfo,
                         storeArchiveFile,
                         &createInfo,
                         tmpDirectory,
                         partSize,
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
    MsgQueue_done(&createInfo.storageMsgQueue,NULL,NULL);
    MsgQueue_done(&createInfo.fileMsgQueue,NULL,NULL);
    free(buffer);

    return FALSE;
  }

  /* start threads */
  if (pthread_create(&createInfo.collectorThread,NULL,(void*(*)(void*))collector,&createInfo) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise collector thread!");
  }
  if (pthread_create(&createInfo.storageThread,NULL,(void*(*)(void*))storage,&createInfo) != 0)
  {
    HALT_FATAL_ERROR("Cannot initialise storage thread!");
  }

  /* store files */
  fileName = String_new();
  while (getNextFile(&createInfo.fileMsgQueue,fileName,&fileType))
  {
    info(0,"Store '%s'...",String_cString(fileName));

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
            info(0,"fail\n");
            printError("Cannot get info for file '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            createInfo.failFlag = TRUE;
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
            info(0,"fail\n");
            printError("Cannot create new archive entry '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            createInfo.failFlag = TRUE;
            break;
          }

          /* write file content into archive */  
          error = File_open(&fileHandle,fileName,FILE_OPENMODE_READ);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printError("Cannot open file '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            createInfo.failFlag = TRUE;
            continue;
          }
          error = ERROR_NONE;
          do
          {
            File_read(&fileHandle,buffer,BUFFER_SIZE,&n);
            if (n > 0)
            {
              error = Archive_writeFileData(&archiveFileInfo,buffer,n);
            }
          }
          while ((n > 0) && (error == ERROR_NONE));
          File_close(&fileHandle);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
            printError("Cannot create archive file!\n");
            Archive_closeEntry(&archiveFileInfo);
            createInfo.failFlag = TRUE;
            break;
          }

          /* close archive entry */
          Archive_closeEntry(&archiveFileInfo);

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
          info(0,"ok (ratio %.1f%%)\n",ratio);
        }
        break;
      case FILETYPE_DIRECTORY:
        {
          FileInfo fileInfo;

          /* get directory info */
          error = File_getFileInfo(fileName,&fileInfo);
          if (error != ERROR_NONE)
          {
            info(0,"fail\n");
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
            info(0,"fail\n");
            printError("Cannot create new archive entry '%s' (error: %s)\n",
                       String_cString(fileName),
                       getErrorText(error)
                      );
            createInfo.failFlag = TRUE;
            break;
          }

          /* close archive entry */
          Archive_closeEntry(&archiveFileInfo);

          /* free resources */

          /* update statistics */
          createInfo.statistics.includedCount++;

          info(0,"ok\n");
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
            info(0,"fail\n");
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
            info(0,"fail\n");
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
            info(0,"fail\n");
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

          /* free resources */
          String_delete(name);

          /* update statistics */
          createInfo.statistics.includedCount++;

          info(0,"ok\n");
        }
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
  }
  String_delete(fileName);

  /* close archive */
  Archive_close(&archiveInfo);
  MsgQueue_setEndOfMsg(&createInfo.storageMsgQueue);

  /* wait for threads */
  pthread_join(createInfo.collectorThread,NULL);
  pthread_join(createInfo.storageThread,NULL);

  /* free resources */
  MsgQueue_done(&createInfo.storageMsgQueue,(MsgQueueMsgFreeFunction)freeStorageMsg,NULL);
  MsgQueue_done(&createInfo.fileMsgQueue,(MsgQueueMsgFreeFunction)freeFileMsg,NULL);
  free(buffer);

  /* output statics */
  info(0,"%lu file(s)/%llu bytes(s) included\n",createInfo.statistics.includedCount,createInfo.statistics.includedByteSum);
  info(1,"%lu file(s) excluded\n",createInfo.statistics.excludedCount);
  info(1,"%lu file(s) skipped\n",createInfo.statistics.skippedCount);
  info(1,"%lu error(s)\n",createInfo.statistics.errorCount);

  return !createInfo.failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
