/***********************************************************************\
*
* Contents: Backup ARchiver PAR2 wrapper functions
* Systems: all
*
\***********************************************************************/

#define __PAR2_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include <fstream>
#include <iostream>
#include <vector>

#ifdef HAVE_LIBPAR2_H
// Note: define required to get correct data types in libpar2.h
#define HAVE_CONFIG_H 1
#include <libpar2.h>
#endif // HAVE_LIBPAR2_H

#include "common/global.h"
#include "common/strings.h"
#include "common/files.h"

#include "configuration.h"
#include "errors.h"

#include "par2.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MAX_PAR2_BLOCKS  32768
#define PAR2_MEMORY_SIZE (64*MB)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : resultToText
* Purpose: convert result to text
* Input  : result - result
* Output : -
* Return : text
* Notes  : -
\***********************************************************************/

#ifdef HAVE_PAR2
LOCAL const char* resultToText(Result result)
{
  switch (result)
  {
    case eRepairNotPossible:           return "repair not possible";
    case eInvalidCommandLineArguments: return "invalid argument";
    case eInsufficientCriticalData:    return "insufficient critical data";
    case eRepairFailed:                return "repair failed";
    case eFileIOError:                 return "file i/o error";
    case eLogicError:                  return "internal error";
    case eMemoryError:                 return "insufficient memory";
    default:                           return "";
  }
}
#endif // HAVE_PAR2

Errors PAR2_create(ConstString      dataFileName,
                   uint64           dataFileSize,
                   ConstString      sourceFileName,
                   const char       *checkSumFilesDirectory,
                   ArchiveFileModes archiveFileMode
                  )
{
#ifdef HAVE_PAR2
  Errors error = ERROR_NONE;

  // check blocks size/block count
  if (   (globalOptions.par2BlockSize <= 0)
      || ((globalOptions.par2BlockSize % 4) != 0)
     )
  {
    return ERROR_PAR2_INVALID_BLOCK_SIZE;
  }
  if (globalOptions.par2BlockCount > MAX_PAR2_BLOCKS)
  {
    return ERROR_PAR2_TOO_MANY_BLOCKS;
  }
  if ((ALIGN(dataFileSize,globalOptions.par2BlockSize)/globalOptions.par2BlockSize) > MAX_PAR2_BLOCKS)
  {
    return ERRORX_(PAR2_BLOCK_SIZE_TOO_SMALL,0,"%" PRIu64,ALIGN(dataFileSize/MAX_PAR2_BLOCKS,4));
  }

  String dataDirectoryName = File_getDirectoryName(String_new(),dataFileName);
  String sourceLinkName    = File_getBaseName(String_new(),sourceFileName,TRUE);
  String sourceBaseName    = File_getBaseName(String_new(),sourceFileName,FALSE);
  String sourceExtension   = File_getExtension(String_new(),sourceFileName);

  // check/delete existing PAR2 files
  DirectoryListHandle directoryListHandle;
  error = File_openDirectoryListCString(&directoryListHandle,checkSumFilesDirectory);
  if (error != ERROR_NONE)
  {
    String_delete(sourceLinkName);
    String_delete(sourceExtension);
    String_delete(sourceBaseName);
    String_delete(dataDirectoryName);
    return error;
  }
  bool   par2ExistsFlag = FALSE;
  String fileName       = String_new();
  String baseName       = String_new();
  while (   (File_readDirectoryList(&directoryListHandle,fileName) == ERROR_NONE)
         && !par2ExistsFlag
        )
  {
    if (File_isFile(fileName))
    {
      File_getBaseName(baseName,fileName,TRUE);
      if (   String_startsWith(baseName,sourceBaseName)
          && String_endsWithCString(baseName,".par2")
         )
      {
        if (archiveFileMode == ARCHIVE_FILE_MODE_OVERWRITE)
        {
          (void)File_delete(fileName,FALSE);
        }
        else
        {
          par2ExistsFlag = TRUE;
        }
      }
    }
  }
  String_delete(baseName);
  String_delete(fileName);
  File_closeDirectoryList(&directoryListHandle);
  if (par2ExistsFlag)
  {
    String_delete(sourceLinkName);
    String_delete(sourceExtension);
    String_delete(sourceBaseName);
    String_delete(dataDirectoryName);
    return ERROR_FILE_EXISTS_;
  }

  /* Note: PAR2 reject absolute paths for security reasons - why?
   *       You should not be able to write into /... without root
   *       priviledges.
  */
  String currentDirectoryName = File_getCurrentDirectory(String_new());
  error = File_changeDirectory(dataDirectoryName);
  if (error != ERROR_NONE)
  {
    String_delete(sourceLinkName);
    String_delete(sourceExtension);
    String_delete(sourceBaseName);
    String_delete(dataDirectoryName);
    return error;
  }

  /* create temporary symbolic link to data file
     Note: required for PAR2 which expect the real source file name
  */
  error = File_makeLink(sourceLinkName,dataFileName);
  if (error != ERROR_NONE)
  {
    (void)File_changeDirectory(currentDirectoryName);
    String_delete(sourceLinkName);
    String_delete(sourceExtension);
    String_delete(sourceBaseName);
    String_delete(dataDirectoryName);
    String_delete(currentDirectoryName);
    return error;
  }

  // create PAR2 checksum files
  try
  {
    std::ofstream null("/dev/null",std::ofstream::out|std::ofstream::app);
    NoiseLevel noiseLevel;
    switch (globalOptions.verboseLevel)
    {
      case 3:
        noiseLevel = NoiseLevel::nlNormal;
        break;
      case 4:
        noiseLevel = NoiseLevel::nlNoisy;
        break;
      case 5:
      case 6:
        noiseLevel = NoiseLevel::nlDebug;
        break;
      default:
        noiseLevel = NoiseLevel::nlSilent;
        break;
    }
    Result result = par2create((globalOptions.verboseLevel >= 3) ? std::cout : null,
                               (globalOptions.verboseLevel >= 3) ? std::cerr : null,
                               noiseLevel,
                               PAR2_MEMORY_SIZE,
                               "",  // basePath,
                               std::string(checkSumFilesDirectory)+FILE_PATH_SEPARATOR_CHAR+std::string(String_cString(sourceBaseName)),
                               std::vector<std::string>{std::string(String_cString(sourceBaseName))+std::string(String_cString(sourceExtension))},
                               (u64)globalOptions.par2BlockSize,
                               (u32)0,  // firstBlock
                               Scheme::scVariable,
                               (u32)globalOptions.par2FileCount,
                               (u32)globalOptions.par2BlockCount
                              );
    switch (result)
    {
      case eSuccess:
        error = ERROR_NONE;
        break;
      case eFileIOError:
        error = ERROR_IO;
        break;
      default:
        error = ERRORX_(PAR2,(uint)result,"%s",resultToText(result));
        break;
    }
    null.close();
  }
  catch (...)
  {
    error = ERROR_PAR2;
  }

  // delete link
  (void)File_delete(sourceLinkName,FALSE);

  // restore current directory
  (void)File_changeDirectory(currentDirectoryName);

  // free resources
  String_delete(sourceLinkName);
  String_delete(sourceExtension);
  String_delete(sourceBaseName);
  String_delete(dataDirectoryName);
  String_delete(currentDirectoryName);

  return error;
#else // not HAVE_PAR2
  UNUSED_VARIABLE(dataFileName);
  UNUSED_VARIABLE(dataFileSize);
  UNUSED_VARIABLE(sourceFileName);
  UNUSED_VARIABLE(checkSumFilesDirectory);
  UNUSED_VARIABLE(archiveFileMode);

  return ERROR_FUNCTION_NOT_SUPPORTED;
#endif // HAVE_PAR2
}

#ifdef __cplusplus
  }
#endif

/* end of file */
