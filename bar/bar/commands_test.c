/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive test function
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

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

#include "errors.h"
#include "patterns.h"
#include "entrylists.h"
#include "patternlists.h"
#include "files.h"
#include "archive.h"
#include "fragmentlists.h"

#include "commands_test.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// file data buffer size
#define BUFFER_SIZE (64*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/*---------------------------------------------------------------------*/

Errors Command_test(const StringList                *storageNameList,
                    const EntryList                 *includeEntryList,
                    const PatternList               *excludePatternList,
                    JobOptions                      *jobOptions,
                    ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                    void                            *archiveGetCryptPasswordUserData
                   )
{
  byte              *archiveBuffer,*fileBuffer;
  FragmentList      fragmentList;
  StorageSpecifier  storageSpecifier;
  StorageHandle     storageHandle;
  StringNode        *stringNode;
  String            storageName;
  Errors            failError;
  Errors            error;
  ArchiveInfo       archiveInfo;
  ArchiveEntryInfo  archiveEntryInfo;
  ArchiveEntryTypes archiveEntryType;
  FragmentNode      *fragmentNode;

  assert(storageNameList != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

  // allocate resources
  archiveBuffer = (byte*)malloc(BUFFER_SIZE);
  if (archiveBuffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  fileBuffer = malloc(BUFFER_SIZE);
  if (fileBuffer == NULL)
  {
    free(archiveBuffer);
    HALT_INSUFFICIENT_MEMORY();
  }
  FragmentList_init(&fragmentList);
  Storage_initSpecifier(&storageSpecifier);

  failError = ERROR_NONE;
  STRINGLIST_ITERATE(storageNameList,stringNode,storageName)
  {
    // parse storage name, get printable name
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error != ERROR_NONE)
    {
      printError("Invalid storage '%s' (error: %s)!\n",
                 String_cString(storageName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }

    // init storage
    error = Storage_init(&storageHandle,
                         &storageSpecifier,
                         jobOptions,
                         &globalOptions.maxBandWidthList,
                         SERVER_CONNECTION_PRIORITY_HIGH,
                         CALLBACK(NULL,NULL),
                         CALLBACK(NULL,NULL)
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot initialize storage '%s' (error: %s)!\n",
                 String_cString(storageName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }

    printInfo(0,"Testing archive '%s':\n",Storage_getPrintableNameCString(&storageSpecifier,NULL));

    // open archive
    error = Archive_open(&archiveInfo,
                         &storageHandle,
                         &storageSpecifier,
                         jobOptions,
                         archiveGetCryptPasswordFunction,
                         archiveGetCryptPasswordUserData
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot open storage '%s' (error: %s)!\n",
                 Storage_getPrintableNameCString(&storageSpecifier,NULL),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      (void)Storage_done(&storageHandle);
      continue;
    }

    // read archive
    while (!Archive_eof(&archiveInfo,FALSE))
    {
      // get next archive entry type
      error = Archive_getNextArchiveEntryType(&archiveInfo,
                                              &archiveEntryType,
                                              FALSE
                                             );
      if (error != ERROR_NONE)
      {
        printError("Cannot read next entry in archive '%s' (error: %s)!\n",
                   Storage_getPrintableNameCString(&storageSpecifier,NULL),
                   Error_getText(error)
                  );
        if (failError == ERROR_NONE) failError = error;
        break;
      }

      switch (archiveEntryType)
      {
        case ARCHIVE_ENTRY_TYPE_FILE:
          {
            String       fileName;
            FileInfo     fileInfo;
            uint64       fragmentOffset,fragmentSize;
            FragmentNode *fragmentNode;
            uint64       length;
            ulong        n;

            // read file
            fileName = String_new();
            error = Archive_readFileEntry(&archiveEntryInfo,
                                          &archiveInfo,
                                          NULL,  // deltaCompressAlgorithm
                                          NULL,  // byteCompressAlgorithm
                                          NULL,  // cryptAlgorithm
                                          NULL,  // cryptType
                                          fileName,
                                          &fileInfo,
                                          NULL,  // fileExtendedAttributeList
                                          NULL,  // deltaSourceName
                                          NULL,  // deltaSourceSize
                                          &fragmentOffset,
                                          &fragmentSize
                                         );
            if (error != ERROR_NONE)
            {
              printError("Cannot read 'file' content of archive '%s' (error: %s)!\n",
                         Storage_getPrintableNameCString(&storageSpecifier,NULL),
                         Error_getText(error)
                        );
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Test file '%s'...",String_cString(fileName));

              if (!jobOptions->noFragmentsCheckFlag)
              {
                // get file fragment list
                fragmentNode = FragmentList_find(&fragmentList,fileName);
                if (fragmentNode == NULL)
                {
                  fragmentNode = FragmentList_add(&fragmentList,fileName,fileInfo.size,NULL,0);
                }
                assert(fragmentNode != NULL);
              }
              else
              {
                fragmentNode = NULL;
              }
//FragmentList_print(fragmentNode,String_cString(fileName));

              // test read file content
              length = 0LL;
              while (length < fragmentSize)
              {
                n = MIN(fragmentSize-length,BUFFER_SIZE);

                // read archive file
                error = Archive_readData(&archiveEntryInfo,archiveBuffer,n);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot read content of archive '%s' (error: %s)!\n",
                             Storage_getPrintableNameCString(&storageSpecifier,NULL),
                             Error_getText(error)
                            );
                  break;
                }

                length += (uint64)n;

                printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
              }
              if (error != ERROR_NONE)
              {
                if (failError == ERROR_NONE) failError = error;
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                continue;
              }
              printInfo(2,"    \b\b\b\b");

              if (fragmentNode != NULL)
              {
                // add fragment to file fragment list
                FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

                // discard fragment list if file is complete
                if (FragmentList_isEntryComplete(fragmentNode))
                {
                  FragmentList_discard(&fragmentList,fragmentNode);
                }
              }

              /* check if all data read.
                 Note: it is not possible to check if all data is read when
                 compression is used. The decompressor may not be at the end
                 of a compressed data chunk even compressed data is _not_
                 corrupt.
              */
              if (   !Compress_isCompressed(archiveEntryInfo.file.deltaCompressAlgorithm)
                  && !Compress_isCompressed(archiveEntryInfo.file.byteCompressAlgorithm)
                  && !Archive_eofData(&archiveEntryInfo)
                 )
              {
                printInfo(1,"FAIL!\n");
                printError("unexpected data at end of file entry '%S'!\n",fileName);
                if (failError == ERROR_NONE) failError = ERROR_CORRUPT_DATA;
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                break;
              }

              printInfo(1,"ok\n");
            }
            else
            {
              // skip
              printInfo(2,"  Test '%s'...skipped\n",String_cString(fileName));
            }

            // close archive file, free resources
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printError("closing 'file' entry fail (error: %s)!\n",
                         Error_getText(error)
                        );
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // free resources
            String_delete(fileName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_IMAGE:
          {
            String       deviceName;
            DeviceInfo   deviceInfo;
            uint64       blockOffset,blockCount;
            FragmentNode *fragmentNode;
            uint64       block;
            ulong        bufferBlockCount;

            // read image
            deviceName = String_new();
            error = Archive_readImageEntry(&archiveEntryInfo,
                                           &archiveInfo,
                                           NULL,  // deltaCompressAlgorithm
                                           NULL,  // byteCompressAlgorithm
                                           NULL,  // cryptAlgorithm
                                           NULL,  // cryptType
                                           deviceName,
                                           &deviceInfo,
                                           NULL,  // deltaSourceName
                                           NULL,  // deltaSourceSize
                                           &blockOffset,
                                           &blockCount
                                          );
            if (error != ERROR_NONE)
            {
              printError("Cannot read 'image' content of archive '%s' (error: %s)!\n",
                         Storage_getPrintableNameCString(&storageSpecifier,NULL),
                         Error_getText(error)
                        );
              String_delete(deviceName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }
            if (deviceInfo.blockSize > BUFFER_SIZE)
            {
              printError("Device block size %llu on '%s' is too big (max: %llu)\n",
                         deviceInfo.blockSize,
                         String_cString(deviceName),
                         BUFFER_SIZE
                        );
              String_delete(deviceName);
              if (failError == ERROR_NONE)
              {
                failError = ERROR_INVALID_DEVICE_BLOCK_SIZE;
              }
              break;
            }
            assert(deviceInfo.blockSize > 0);

            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,deviceName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Test image '%s'...",String_cString(deviceName));

              if (!jobOptions->noFragmentsCheckFlag)
              {
                // get file fragment node
                fragmentNode = FragmentList_find(&fragmentList,deviceName);
                if (fragmentNode == NULL)
                {
                  fragmentNode = FragmentList_add(&fragmentList,deviceName,deviceInfo.size,NULL,0);
                }
//FragmentList_print(fragmentNode,String_cString(deviceName));
                assert(fragmentNode != NULL);
              }
              else
              {
                fragmentNode = NULL;
              }

              // test read image content
              block = 0LL;
              while (block < blockCount)
              {
                bufferBlockCount = MIN(blockCount-block,BUFFER_SIZE/deviceInfo.blockSize);

                // read archive file
                error = Archive_readData(&archiveEntryInfo,archiveBuffer,bufferBlockCount*deviceInfo.blockSize);
                if (error != ERROR_NONE)
                {
                  printInfo(1,"FAIL!\n");
                  printError("Cannot read content of archive '%s' (error: %s)!\n",
                             Storage_getPrintableNameCString(&storageSpecifier,NULL),
                             Error_getText(error)
                            );
                  break;
                }

                block += (uint64)bufferBlockCount;

                printInfo(2,"%3d%%\b\b\b\b",(uint)((block*100LL)/blockCount));
              }
              if (error != ERROR_NONE)
              {
                if (failError == ERROR_NONE) failError = error;
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(deviceName);
                continue;
              }
              printInfo(2,"    \b\b\b\b");

              if (fragmentNode != NULL)
              {
                // add fragment to file fragment list
                FragmentList_addEntry(fragmentNode,blockOffset*(uint64)deviceInfo.blockSize,blockCount*(uint64)deviceInfo.blockSize);

                // discard fragment list if file is complete
                if (FragmentList_isEntryComplete(fragmentNode))
                {
                  FragmentList_discard(&fragmentList,fragmentNode);
                }
              }

              /* check if all data read.
                 Note: it is not possible to check if all data is read when
                 compression is used. The decompressor may not be at the end
                 of a compressed data chunk even compressed data is _not_
                 corrupt.
              */
              if (   !Compress_isCompressed(archiveEntryInfo.image.deltaCompressAlgorithm)
                  && !Compress_isCompressed(archiveEntryInfo.image.byteCompressAlgorithm)
                  && !Archive_eofData(&archiveEntryInfo))
              {
                printInfo(1,"FAIL!\n");
                printError("unexpected data at end of image entry '%S'!\n",deviceName);
                if (failError == ERROR_NONE) failError = ERROR_CORRUPT_DATA;
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(deviceName);
                break;
              }

              printInfo(1,"ok\n");
            }
            else
            {
              // skip
              printInfo(2,"  Test '%s'...skipped\n",String_cString(deviceName));
            }

            // close archive file, free resources
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printError("closing 'image' entry fail (error: %s)!\n",
                         Error_getText(error)
                        );
              String_delete(deviceName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // free resources
            String_delete(deviceName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_DIRECTORY:
          {
            String   directoryName;
            FileInfo fileInfo;

            // read directory
            directoryName = String_new();
            error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                               &archiveInfo,
                                               NULL,  // cryptAlgorithm
                                               NULL,  // cryptType
                                               directoryName,
                                               &fileInfo,
                                               NULL   // fileExtendedAttributeList
                                              );
            if (error != ERROR_NONE)
            {
              printError("Cannot read 'directory' content of archive '%s' (error: %s)!\n",
                         Storage_getPrintableNameCString(&storageSpecifier,NULL),
                         Error_getText(error)
                        );
              String_delete(directoryName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Test directory '%s'...",String_cString(directoryName));

              // check if all data read
              if (!Archive_eofData(&archiveEntryInfo))
              {
                printInfo(1,"FAIL!\n");
                printError("unexpected data at end of directory entry '%S'!\n",directoryName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(directoryName);
                if (failError == ERROR_NONE) failError = ERROR_CORRUPT_DATA;
                break;
              }

              printInfo(1,"ok\n");

              // free resources
            }
            else
            {
              // skip
              printInfo(2,"Test '%s'...skipped\n",String_cString(directoryName));
            }

            // close archive file
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printError("closing 'directory' entry fail (error: %s)!\n",
                         Error_getText(error)
                        );
              String_delete(directoryName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // free resources
            String_delete(directoryName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_LINK:
          {
            String   linkName;
            String   fileName;
            FileInfo fileInfo;

            // read link
            linkName = String_new();
            fileName = String_new();
            error = Archive_readLinkEntry(&archiveEntryInfo,
                                          &archiveInfo,
                                          NULL,  // cryptAlgorithm
                                          NULL,  // cryptType
                                          linkName,
                                          fileName,
                                          &fileInfo,
                                          NULL   // fileExtendedAttributeList
                                         );
            if (error != ERROR_NONE)
            {
              printError("Cannot read 'link' content of archive '%s' (error: %s)!\n",
                         Storage_getPrintableNameCString(&storageSpecifier,NULL),
                         Error_getText(error)
                        );
              String_delete(fileName);
              String_delete(linkName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Test link '%s'...",String_cString(linkName));

              // check if all data read
              if (!Archive_eofData(&archiveEntryInfo))
              {
                printInfo(1,"FAIL!\n");
                printError("unexpected data at end of link entry '%S'!\n",linkName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                String_delete(linkName);
                if (failError == ERROR_NONE) failError = ERROR_CORRUPT_DATA;
                break;
              }

              printInfo(1,"ok\n");

              // free resources
            }
            else
            {
              // skip
              printInfo(2,"  Test '%s'...skipped\n",String_cString(linkName));
            }

            // close archive file
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printError("closing 'link' entry fail (error: %s)!\n",
                         Error_getText(error)
                        );
              String_delete(fileName);
              String_delete(linkName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // free resources
            String_delete(fileName);
            String_delete(linkName);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_HARDLINK:
          {
            StringList       fileNameList;
            FileInfo         fileInfo;
            uint64           fragmentOffset,fragmentSize;
            bool             testedDataFlag;
            const StringNode *stringNode;
            String           fileName;
            FragmentNode     *fragmentNode;
            uint64           length;
            ulong            n;

            // read hard linke
            StringList_init(&fileNameList);
            error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                              &archiveInfo,
                                              NULL,  // deltaCompressAlgorithm
                                              NULL,  // byteCompressAlgorithm
                                              NULL,  // cryptAlgorithm
                                              NULL,  // cryptType
                                              &fileNameList,
                                              &fileInfo,
                                              NULL,  // fileExtendedAttributeList
                                              NULL,  // deltaSourceName
                                              NULL,  // deltaSourceSize
                                              &fragmentOffset,
                                              &fragmentSize
                                             );
            if (error != ERROR_NONE)
            {
              printError("Cannot read 'hard link' content of archive '%s' (error: %s)!\n",
                         Storage_getPrintableNameCString(&storageSpecifier,NULL),
                         Error_getText(error)
                        );
              StringList_done(&fileNameList);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            testedDataFlag = FALSE;
            STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
            {
              if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                  && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                 )
              {
                printInfo(1,"  Test hard link '%s'...",String_cString(fileName));

                if (!testedDataFlag && (failError == ERROR_NONE))
                {
                  // test hard link data

                  if (!jobOptions->noFragmentsCheckFlag)
                  {
                    // get file fragment list
                    fragmentNode = FragmentList_find(&fragmentList,fileName);
                    if (fragmentNode == NULL)
                    {
                      fragmentNode = FragmentList_add(&fragmentList,fileName,fileInfo.size,NULL,0);
                    }
                    assert(fragmentNode != NULL);
//FragmentList_print(fragmentNode,String_cString(fileName));
                  }
                  else
                  {
                    fragmentNode = NULL;
                  }

                  // test read hard link content
                  length = 0LL;
                  while (length < fragmentSize)
                  {
                    n = MIN(fragmentSize-length,BUFFER_SIZE);

                    // read archive file
                    error = Archive_readData(&archiveEntryInfo,archiveBuffer,n);
                    if (error != ERROR_NONE)
                    {
                      printInfo(1,"FAIL!\n");
                      printError("Cannot read content of archive '%s' (error: %s)!\n",
                                 Storage_getPrintableNameCString(&storageSpecifier,NULL),
                                 Error_getText(error)
                                );
                      break;
                    }

                    length += (uint64)n;

                    printInfo(2,"%3d%%\b\b\b\b",(uint)((length*100LL)/fragmentSize));
                  }
                  if (error != ERROR_NONE)
                  {
                    if (failError == ERROR_NONE) failError = error;
                    break;
                  }
                  printInfo(2,"    \b\b\b\b");

                  if (fragmentNode != NULL)
                  {
                    // add fragment to file fragment list
                    FragmentList_addEntry(fragmentNode,fragmentOffset,fragmentSize);

                    // discard fragment list if file is complete
                    if (FragmentList_isEntryComplete(fragmentNode))
                    {
                      FragmentList_discard(&fragmentList,fragmentNode);
                    }
                  }

                  /* check if all data read.
                     Note: it is not possible to check if all data is read when
                     compression is used. The decompressor may not be at the end
                     of a compressed data chunk even compressed data is _not_
                     corrupt.
                  */
                  if (   !Compress_isCompressed(archiveEntryInfo.hardLink.deltaCompressAlgorithm)
                      && !Compress_isCompressed(archiveEntryInfo.hardLink.byteCompressAlgorithm)
                      && !Archive_eofData(&archiveEntryInfo))
                  {
                    printError("unexpected data at end of hard link entry '%S'!\n",fileName);
                    if (failError == ERROR_NONE) failError = ERROR_CORRUPT_DATA;
                    break;
                  }

                  printInfo(1,"ok\n");

                  testedDataFlag = TRUE;
                }
                else
                {
                  // test hard link data already done
                  if (failError == ERROR_NONE)
                  {
                    printInfo(1,"ok\n");
                  }
                  else
                  {
                    printInfo(1,"FAIL!\n");
                  }
                }
              }
              else
              {
                // skip
                printInfo(2,"  Test '%s'...skipped\n",String_cString(fileName));
              }
            }
            if (failError != ERROR_NONE)
            {
              (void)Archive_closeEntry(&archiveEntryInfo);
              StringList_done(&fileNameList);
              break;
            }

            // close archive file, free resources
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printError("closing 'hard link' entry fail (error: %s)!\n",
                         Error_getText(error)
                        );
              StringList_done(&fileNameList);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // free resources
            StringList_done(&fileNameList);
          }
          break;
        case ARCHIVE_ENTRY_TYPE_SPECIAL:
          {
            String   fileName;
            FileInfo fileInfo;

            // read special
            fileName = String_new();
            error = Archive_readSpecialEntry(&archiveEntryInfo,
                                             &archiveInfo,
                                             NULL,  // cryptAlgorithm
                                             NULL,  // cryptType
                                             fileName,
                                             &fileInfo,
                                             NULL   // fileExtendedAttributeList
                                            );
            if (error != ERROR_NONE)
            {
              printError("Cannot read 'special' content of archive '%s' (error: %s)!\n",
                         Storage_getPrintableNameCString(&storageSpecifier,NULL),
                         Error_getText(error)
                        );
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
               )
            {
              printInfo(1,"  Test special device '%s'...",String_cString(fileName));

              // check if all data read
              if (!Archive_eofData(&archiveEntryInfo))
              {
                printInfo(1,"FAIL!\n");
                printError("unexpected data at end of special entry '%S'!\n",fileName);
                (void)Archive_closeEntry(&archiveEntryInfo);
                String_delete(fileName);
                if (failError == ERROR_NONE) failError = ERROR_CORRUPT_DATA;
                break;
              }

              printInfo(1,"ok\n");

              // free resources
            }
            else
            {
              // skip
              printInfo(2,"  Test '%s'...skipped\n",String_cString(fileName));
            }

            // close archive file
            error = Archive_closeEntry(&archiveEntryInfo);
            if (error != ERROR_NONE)
            {
              printError("closing 'special' entry fail (error: %s)!\n",
                         Error_getText(error)
                        );
              String_delete(fileName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            // free resources
            String_delete(fileName);
          }
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
    }

    // close archive
    Archive_close(&archiveInfo);

    // done storage
    (void)Storage_done(&storageHandle);
  }

  if (   (failError == ERROR_NONE)
      && !jobOptions->noFragmentsCheckFlag
     )
  {
    // check fragment lists
    FRAGMENTLIST_ITERATE(&fragmentList,fragmentNode)
    {
      if (!FragmentList_isEntryComplete(fragmentNode))
      {
        printInfo(0,"Warning: incomplete file '%s'\n",String_cString(fragmentNode->name));
        if (isPrintInfo(2))
        {
          printInfo(2,"  Fragments:\n");
          FragmentList_print(stdout,4,fragmentNode);
        }
        if (failError == ERROR_NONE) failError = ERROR_ENTRY_INCOMPLETE;
      }
    }
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  FragmentList_done(&fragmentList);
  free(fileBuffer);
  free(archiveBuffer);

  return failError;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
