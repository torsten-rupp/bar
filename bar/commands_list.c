/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/commands_list.c,v $
* $Revision: 1.14 $
* $Author: torsten $
* Contents: Backup ARchiver archive list function
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
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "stringlists.h"

#include "bar.h"
#include "errors.h"
#include "patterns.h"
#include "files.h"
#include "archive.h"

#include "commands_list.h"

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

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : printFileInfo
* Purpose: print file information
* Input  : 
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printFileInfo(const String       fileName,
                         const FileInfo     *fileInfo,
                         CompressAlgorithms compressAlgorithm,
                         CryptAlgorithms    cryptAlgorithm,
                         uint64             fragmentOffset,
                         uint64             fragmentSize,
                         uint64             archiveFileSize
                        )
{
  double ratio;

  assert(fileName != NULL);
  assert(fileInfo != NULL);

  if ((compressAlgorithm != COMPRESS_ALGORITHM_NONE) && (fragmentSize > 0))
  {
    ratio = 100.0-archiveFileSize*100.0/fragmentSize;
  }
  else
  {
    ratio = 0;
  }

  printf("FILE %10llu %10llu..%10llu %-10s %6.1f%% %-10s %s\n",
         fileInfo->size,
         fragmentOffset,
         (fragmentSize > 0)?fragmentOffset+fragmentSize-1:fragmentOffset,
         Compress_getAlgorithmName(compressAlgorithm),
         ratio,
         Crypt_getAlgorithmName(cryptAlgorithm),
         String_cString(fileName)
        );
}

/***********************************************************************\
* Name   : printLinkInfo
* Purpose: print link information
* Input  : 
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printDirectoryInfo(const String    directoryName,
                              const FileInfo  *fileInfo,
                              CryptAlgorithms cryptAlgorithm
                             )
{
  assert(directoryName != NULL);
  assert(fileInfo != NULL);

  printf("DIR                                                       %-10s %s\n",
         Crypt_getAlgorithmName(cryptAlgorithm),
         String_cString(directoryName)
        );
}

/***********************************************************************\
* Name   : printLinkInfo
* Purpose: print link information
* Input  : 
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printLinkInfo(const String    linkName,
                         const String    fileName,
                         const FileInfo  *fileInfo,
                         CryptAlgorithms cryptAlgorithm
                        )
{
  assert(linkName != NULL);
  assert(fileName != NULL);
  assert(fileInfo != NULL);

  printf("LINK                                                      %-10s %s -> %s\n",
         Crypt_getAlgorithmName(cryptAlgorithm),
         String_cString(linkName),
         String_cString(fileName)
        );
}

/*---------------------------------------------------------------------*/

bool Command_list(StringList  *archiveFileNameList,
                  PatternList *includePatternList,
                  PatternList *excludePatternList,
                  const char  *password
                 )
{
  String             archiveFileName;
  bool               failFlag;
  ulong              fileCount;
  Errors             error;
  ArchiveInfo        archiveInfo;
  ArchiveFileInfo    archiveFileInfo;
  FileTypes          fileType;
  CryptAlgorithms    cryptAlgorithm;

  assert(archiveFileNameList != NULL);
  assert(includePatternList != NULL);
  assert(excludePatternList != NULL);

  archiveFileName = String_new();

  failFlag  = FALSE;
  fileCount = 0;
  info(0,
       "%4s %-10s %-22s %-10s %-7s %-10s %s\n",
       "Type",
       "Size",
       "Part",
       "Compress",
       "Ratio %",
       "Crypt",
       "Name"
      );
  info(0,"--------------------------------------------------------------------------------------------------------------\n");
  while (!StringList_empty(archiveFileNameList))
  {
    StringList_getFirst(archiveFileNameList,archiveFileName);

    /* open archive */
    error = Archive_open(&archiveInfo,
                         archiveFileName,
                         password
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open file '%s' (error: %s)!\n",
                 String_cString(archiveFileName),
                 getErrorText(error)
                );
      failFlag = TRUE;
      continue;
    }

    /* list contents */
    while (!Archive_eof(&archiveInfo))
    {
      /* get next file type */
      error = Archive_getNextFileType(&archiveInfo,
                                      &archiveFileInfo,
                                      &fileType
                                     );
      if (error != ERROR_NONE)
      {
        printError("Cannot not read content of archive '%s' (error: %s)!\n",
                   String_cString(archiveFileName),
                   getErrorText(error)
                  );
        failFlag = TRUE;
        break;
      }

      switch (fileType)
      {
        case FILETYPE_FILE:
          {
            ArchiveFileInfo    archiveFileInfo;
            CompressAlgorithms compressAlgorithm;
            String             fileName;
            FileInfo           fileInfo;
            uint64             fragmentOffset,fragmentSize;

            /* open archive file */
            fileName = String_new();
            error = Archive_readFileEntry(&archiveInfo,
                                          &archiveFileInfo,
                                          &compressAlgorithm,
                                          &cryptAlgorithm,
                                          fileName,
                                          &fileInfo,
                                          &fragmentOffset,
                                          &fragmentSize
                                         );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         getErrorText(error)
                        );
              String_delete(fileName);
              failFlag = TRUE;
              break;
            }

            if (   (List_empty(includePatternList) || Patterns_matchList(includePatternList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !Patterns_matchList(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              /* output file info */
              printFileInfo(fileName,
                            &fileInfo,
                            compressAlgorithm,
                            cryptAlgorithm,
                            fragmentOffset,
                            fragmentSize,
                            archiveFileInfo.file.chunkInfoFileData.size
                           );
              fileCount++;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(fileName);
          }
          break;
        case FILETYPE_DIRECTORY:
          {
            String   directoryName;
            FileInfo fileInfo;

            /* open archive lin */
            directoryName = String_new();
            error = Archive_readDirectoryEntry(&archiveInfo,
                                               &archiveFileInfo,
                                               &cryptAlgorithm,
                                               directoryName,
                                               &fileInfo
                                              );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         getErrorText(error)
                        );
              String_delete(directoryName);
              failFlag = TRUE;
              break;
            }

            if (   (List_empty(includePatternList) || Patterns_matchList(includePatternList,directoryName,PATTERN_MATCH_MODE_EXACT))
                && !Patterns_matchList(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              /* output file info */
              printDirectoryInfo(directoryName,
                                 &fileInfo,
                                 cryptAlgorithm
                                );
              fileCount++;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(directoryName);
          }
          break;
        case FILETYPE_LINK:
          {
            String   linkName;
            String   fileName;
            FileInfo fileInfo;

            /* open archive lin */
            linkName = String_new();
            fileName = String_new();
            error = Archive_readLinkEntry(&archiveInfo,
                                          &archiveFileInfo,
                                          &cryptAlgorithm,
                                          linkName,
                                          fileName,
                                          &fileInfo
                                         );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read content of archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         getErrorText(error)
                        );
              String_delete(fileName);
              String_delete(linkName);
              failFlag = TRUE;
              break;
            }

            if (   (List_empty(includePatternList) || Patterns_matchList(includePatternList,linkName,PATTERN_MATCH_MODE_EXACT))
                && !Patterns_matchList(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              /* output file info */
              printLinkInfo(linkName,
                            fileName,
                            &fileInfo,
                            cryptAlgorithm
                           );
              fileCount++;
            }

            /* close archive file, free resources */
            Archive_closeEntry(&archiveFileInfo);
            String_delete(fileName);
            String_delete(linkName);
          }
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }

    /* close archive */
    Archive_close(&archiveInfo);
  }
  info(0,"--------------------------------------------------------------------------------------------------------------\n");
  info(0,"%lu file(s)\n",fileCount);

  /* free resources */
  String_delete(archiveFileName);

  return !failFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
