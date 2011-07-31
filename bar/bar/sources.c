/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/compress.h,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: Backup ARchiver source functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "patternlists.h"
#include "files.h"

#include "errors.h"
#include "archive.h"

#include "sources.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : freeFTPServerNode
* Purpose: free FTP server node
* Input  : ftpServerNode - FTP server node
*          userData      - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeSourceNode(SourceNode *sourceNode, void *userData)
{
  assert(sourceNode != NULL);

  UNUSED_VARIABLE(userData);

  switch (sourceNode->type)
  {
    case SOURCE_ENTRY_TYPE_FILE:
      String_delete(sourceNode->file.name);
      break;
    case SOURCE_ENTRY_TYPE_IMAGE:
      String_delete(sourceNode->image.name);
      break;
    case SOURCE_ENTRY_TYPE_HARDLINK:
      StringList_done(&sourceNode->hardLink.nameList);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    #endif /* NDEBUG */
  }
  String_delete(sourceNode->storageName);
}

/*---------------------------------------------------------------------*/

Errors Source_init(SourceInfo        *sourceInfo,
                   const PatternList *sourcePatternList,
                   JobOptions        *jobOptions
                  )
{
  String      storageName;
  String      fileName;
  PatternNode *patternNode;
  Errors      error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  ArchiveInfo archiveInfo;
  ArchiveEntryTypes archiveEntryType;
  SourceNode  *sourceNode;

  assert(sourceInfo != NULL);
  assert(sourcePatternList != NULL);

  List_init(&sourceInfo->sourceList);

  storageName = String_new();
  fileName    = File_newFileName();
  PATTERNLIST_ITERATE(sourcePatternList,patternNode)
  {
    error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                      patternNode->string,
                                      jobOptions
                                     );
    if (error == ERROR_NONE)
    {
      while (!Storage_endOfDirectoryList(&storageDirectoryListHandle))
      {
        error = Storage_readDirectoryList(&storageDirectoryListHandle,
                                          fileName,
                                          NULL
                                         );
        if (   (error == ERROR_NONE)
            && Pattern_match(&patternNode->pattern,fileName,PATTERN_MATCH_MODE_EXACT)
           )
        {
          /* get storage name */
          String_set(storageName,fileName);

          /* open archive */
          error = Archive_open(&archiveInfo,
                               storageName,
                               jobOptions,
                               NULL,
                               NULL
                              );
          if (error == ERROR_NONE)
          {
            while (   !Archive_eof(&archiveInfo,TRUE)
//                   && ((requestedAbortFlag == NULL) || !(*requestedAbortFlag))
                   && (error == ERROR_NONE)
                  )
            {
#if 0
              /* pause */
              while ((pauseFlag != NULL) && (*pauseFlag))
              {
                Misc_udelay(5000*1000);
              }
#endif /* 0 */

              /* get next file type */
              error = Archive_getNextArchiveEntryType(&archiveInfo,
                                                      &archiveEntryType,
                                                      TRUE
                                                     );
              if (error == ERROR_NONE)
              {
                /* read entry */
                switch (archiveEntryType)
                {
                  case ARCHIVE_ENTRY_TYPE_FILE:
                    {
                      String             fileName;
                      ArchiveEntryInfo   archiveEntryInfo;
                      CompressAlgorithms compressAlgorithm;
                      CryptAlgorithms    cryptAlgorithm;
                      CryptTypes         cryptType;
                      FileInfo           fileInfo;
                      uint64             fragmentOffset,fragmentSize;

                      /* open archive file */
                      fileName = String_new();
                      error = Archive_readFileEntry(&archiveInfo,
                                                    &archiveEntryInfo,
                                                    &compressAlgorithm,
                                                    &cryptAlgorithm,
                                                    &cryptType,
                                                    fileName,
                                                    NULL,
                                                    &fragmentOffset,
                                                    &fragmentSize
                                                   );
                      if (error != ERROR_NONE)
                      {
                        String_delete(fileName);
                        break;
                      }

                      /* add to source list */
                      sourceNode = LIST_NEW_NODE(SourceNode);
                      if (sourceNode == NULL)
                      {
                        HALT_INSUFFICIENT_MEMORY();
                      }
                      sourceNode->type                = SOURCE_ENTRY_TYPE_FILE;
                      sourceNode->storageName         = String_duplicate(storageName);
                      sourceNode->file.name           = String_duplicate(fileName);
                      sourceNode->file.fragmentOffset = fragmentOffset;
                      sourceNode->file.fragmentSize   = fragmentSize;
                      List_append(&sourceInfo->sourceList,sourceNode);

                      /* close archive file, free resources */
                      Archive_closeEntry(&archiveEntryInfo);
                      String_delete(fileName);
                    }
                    break;
                  case ARCHIVE_ENTRY_TYPE_IMAGE:
                    {
                      String             deviceName;
                      ArchiveEntryInfo   archiveEntryInfo;
                      CompressAlgorithms compressAlgorithm;
                      CryptAlgorithms    cryptAlgorithm;
                      CryptTypes         cryptType;
                      DeviceInfo         deviceInfo;
                      uint64             blockOffset,blockCount;

                      /* open archive file */
                      deviceName = String_new();
                      error = Archive_readImageEntry(&archiveInfo,
                                                     &archiveEntryInfo,
                                                     &compressAlgorithm,
                                                     &cryptAlgorithm,
                                                     &cryptType,
                                                     deviceName,
                                                     &deviceInfo,
                                                     &blockOffset,
                                                     &blockCount
                                                    );
                      if (error != ERROR_NONE)
                      {
                        String_delete(deviceName);
                        break;
                      }

                      /* add to source list */

                      /* close archive file, free resources */
                      Archive_closeEntry(&archiveEntryInfo);
                      String_delete(deviceName);
                    }
                    break;
                  case ARCHIVE_ENTRY_TYPE_DIRECTORY:
                    break;
                  case ARCHIVE_ENTRY_TYPE_LINK:
                    break;
                  case ARCHIVE_ENTRY_TYPE_HARDLINK:
                    {
                      StringList         fileNameList;
                      ArchiveEntryInfo   archiveEntryInfo;
                      CompressAlgorithms compressAlgorithm;
                      CryptAlgorithms    cryptAlgorithm;
                      CryptTypes         cryptType;
                      FileInfo           fileInfo;
                      uint64             fragmentOffset,fragmentSize;
                      const StringNode   *stringNode;
                      String             fileName;

                      /* open archive file */
                      StringList_init(&fileNameList);
                      error = Archive_readHardLinkEntry(&archiveInfo,
                                                    &archiveEntryInfo,
                                                    &compressAlgorithm,
                                                    &cryptAlgorithm,
                                                    &cryptType,
                                                    &fileNameList,
                                                    &fileInfo,
                                                    &fragmentOffset,
                                                    &fragmentSize
                                                   );
                      if (error != ERROR_NONE)
                      {
                        StringList_done(&fileNameList);
                        break;
                      }
          //fprintf(stderr,"%s,%d: index update %s\n",__FILE__,__LINE__,String_cString(name));

                      /* add to source list */
                      STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
                      {
                      }

                      /* close archive file, free resources */
                      Archive_closeEntry(&archiveEntryInfo);
                      StringList_done(&fileNameList);
                    }
                    break;
                  case ARCHIVE_ENTRY_TYPE_SPECIAL:
                    break;
                  default:
                    #ifndef NDEBUG
                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                    #endif /* NDEBUG */
                    break; /* not reached */
                }
              }
            }

            /* close archive */
            Archive_close(&archiveInfo);
          }         
        }
      }
      Storage_closeDirectoryList(&storageDirectoryListHandle);
    }
  }
  File_deleteFileName(fileName);
  String_delete(storageName);

  return ERROR_NONE;
}

void Source_done(SourceInfo *sourceInfo)
{
  assert(sourceInfo != NULL);

  List_done(&sourceInfo->sourceList,(ListNodeFreeFunction)freeSourceNode,NULL);
}

Errors Source_openEntry(SourceEntryInfo *sourceEntryInfo,
                        SourceInfo      *sourceInfo,
                        const String    name
                       )
{
  SourceNode *sourceNode;
  StringNode *stringNode;

  assert(sourceEntryInfo != NULL);
  assert(sourceInfo != NULL);
  assert(name != NULL);

  sourceEntryInfo->sourceNode = NULL;
  LIST_ITERATE(&sourceInfo->sourceList,sourceNode)
  {
    switch (sourceNode->type)
    {
      case SOURCE_ENTRY_TYPE_FILE:
        if (String_equals(sourceNode->file.name,name))
        {
          sourceEntryInfo->sourceNode = sourceNode;
        }
        break;
      case SOURCE_ENTRY_TYPE_IMAGE:
        if (String_equals(sourceNode->image.name,name))
        {
          sourceEntryInfo->sourceNode = sourceNode;
        }
        break;
      case SOURCE_ENTRY_TYPE_HARDLINK:
        stringNode = StringList_find(&sourceNode->hardLink.nameList,name);
        if (stringNode != NULL)
        {
          sourceEntryInfo->sourceNode = sourceNode;
        }
        break;
    }
  }

  return ERROR_NONE;
}

void Source_closeEntry(SourceEntryInfo *sourceEntryInfo)
{
  assert(sourceEntryInfo != NULL);
}

Errors Source_getEntryDataBlock(SourceEntryInfo *sourceEntryInfo,
                                void            *buffer,
                                uint64          offset,
                                ulong           length
                               )
{
  assert(sourceEntryInfo != NULL);
  assert(buffer != NULL);
fprintf(stderr,"%s,%d: \n",__FILE__,__LINE__);

  return ERROR_STILL_NOT_IMPLEMENTED;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
