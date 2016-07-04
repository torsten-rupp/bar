/***********************************************************************\
*
* $Revision: 4012 $
* $Date: 2015-04-28 19:02:40 +0200 (Tue, 28 Apr 2015) $
* $Author: torsten $
* Contents: storage device functions
* Systems: all
*
\***********************************************************************/

#define __STORAGE_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "autofree.h"
#include "strings.h"
#include "stringlists.h"
#include "files.h"
#include "network.h"
#include "errors.h"

#include "errors.h"
#include "crypt.h"
#include "passwords.h"
#include "misc.h"
#include "archive.h"
#include "bar_global.h"
#include "bar.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
/* file data buffer size */
#define BUFFER_SIZE (64*1024)

// different timeouts [ms]
#define READ_TIMEOUT   (60*1000)

#define INITIAL_BUFFER_SIZE   (64*1024)
#define INCREMENT_BUFFER_SIZE ( 8*1024)
#define MAX_BUFFER_SIZE       (64*1024)
#define MAX_FILENAME_LENGTH   ( 8*1024)

#define UNLOAD_VOLUME_DELAY_TIME (10LL*MISC_US_PER_SECOND) /* [us] */
#define LOAD_VOLUME_DELAY_TIME   (10LL*MISC_US_PER_SECOND) /* [us] */

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : requestNewDeviceVolume
* Purpose: request new volume
* Input  : storageHandle - storage file handle
*          waitFlag          - TRUE to wait for new volume
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors requestNewDeviceVolume(StorageHandle *storageHandle, bool waitFlag)
{
  TextMacro             textMacros[2];
  bool                  volumeRequestedFlag;
  StorageRequestResults storageRequestResult;

  TEXT_MACRO_N_STRING (textMacros[0],"%device",storageHandle->storageSpecifier.deviceName,NULL);
  TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageHandle->requestedVolumeNumber,      NULL);

  if (   (storageHandle->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageHandle->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    // sleep a short time to give hardware time for finishing volume; unload current volume
    printInfo(0,"Unload volume #%d...",storageHandle->volumeNumber);
    Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
    Misc_executeCommand(String_cString(storageHandle->device.unloadVolumeCommand),
                        textMacros,SIZE_OF_ARRAY(textMacros),
                        CALLBACK(executeIOOutput,NULL),
                        CALLBACK(executeIOOutput,NULL)
                       );
    printInfo(0,"ok\n");

    storageHandle->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  // request new volume
  volumeRequestedFlag  = FALSE;
  storageRequestResult = STORAGE_REQUEST_VOLUME_UNKNOWN;
  if      (storageHandle->requestVolumeFunction != NULL)
  {
    volumeRequestedFlag = TRUE;

    // request new volume via call back, unload if requested
    do
    {
      storageRequestResult = storageHandle->requestVolumeFunction(storageHandle->requestedVolumeNumber,
                                                                  storageHandle->requestVolumeUserData
                                                                 );
      if (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD)
      {
        // sleep a short time to give hardware time for finishing volume, then unload current medium
        printInfo(0,"Unload volume...");
        Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
        Misc_executeCommand(String_cString(storageHandle->device.unloadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           );
        printInfo(0,"ok\n");
      }
    }
    while (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD);

    storageHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (storageHandle->device.requestVolumeCommand != NULL)
  {
    volumeRequestedFlag = TRUE;

    // request new volume via external command
    printInfo(0,"Request new volume #%d...",storageHandle->requestedVolumeNumber);
    if (Misc_executeCommand(String_cString(storageHandle->device.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           ) == ERROR_NONE
       )
    {
      printInfo(0,"ok\n");
      storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
    }
    else
    {
      printInfo(0,"FAIL\n");
      storageRequestResult = STORAGE_REQUEST_VOLUME_FAIL;
    }

    storageHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else
  {
    if (storageHandle->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
    {
      if (waitFlag)
      {
        volumeRequestedFlag = TRUE;

        printInfo(0,"Please insert volume #%d into drive '%s' and press ENTER to continue\n",storageHandle->requestedVolumeNumber,storageHandle->storageSpecifier.deviceName);
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
      else
      {
        printInfo(0,"Please insert volume #%d into drive '%s'\n",storageHandle->requestedVolumeNumber,storageHandle->storageSpecifier.deviceName);
      }
    }
    else
    {
      if (waitFlag)
      {
        volumeRequestedFlag = TRUE;

        printInfo(0,"Press ENTER to continue\n");
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
    }

    storageHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (volumeRequestedFlag)
  {
    switch (storageRequestResult)
    {
      case STORAGE_REQUEST_VOLUME_OK:
        // load volume; sleep a short time to give hardware time for reading volume information
        printInfo(0,"Load volume #%d...",storageHandle->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storageHandle->device.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           );
        Misc_udelay(LOAD_VOLUME_DELAY_TIME);
        printInfo(0,"ok\n");

        // store new volume number
        storageHandle->volumeNumber = storageHandle->requestedVolumeNumber;

        // update status info
        storageHandle->runningInfo.volumeNumber = storageHandle->volumeNumber;
        updateStorageStatusInfo(storageHandle);

        storageHandle->volumeState = STORAGE_VOLUME_STATE_LOADED;
        return ERROR_NONE;
        break;
      case STORAGE_REQUEST_VOLUME_ABORTED:
        return ERROR_NONE;
        break;
      default:
        return ERROR_LOAD_VOLUME_FAIL;
        break;
    }
  }
  else
  {
    return ERROR_NONE;
  }
}

/*---------------------------------------------------------------------*/

LOCAL Errors StorageDevice_initAll(void)
{
  Errors error;

  error = ERROR_NONE;

  return error;
}

LOCAL void StorageDevice_doneAll(void)
{
}

LOCAL bool StorageDevice_parseSpecifier(ConstString deviceSpecifier,
                                        ConstString defaultDeviceName,
                                        String      deviceName
                                       )
{
  bool result;

  assert(deviceSpecifier != NULL);
  assert(deviceName != NULL);

  String_clear(deviceName);

  if (String_matchCString(deviceSpecifier,STRING_BEGIN,"^([^:]*):$",NULL,STRING_NO_ASSIGN,deviceName,NULL))
  {
    // <device name>

    result = TRUE;
  }
  else
  {
    if (defaultDeviceName != NULL) String_set(deviceName,defaultDeviceName);

    result = TRUE;
  }

  return result;
}

LOCAL bool StorageDevice_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
                                         ConstString            archiveName1,
                                         const StorageSpecifier *storageSpecifier2,
                                         ConstString            archiveName2
                                        )
{
  assert(storageSpecifier1 != NULL);
  assert(storageSpecifier1->type == STORAGE_TYPE_DEVICE);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_DEVICE);

  if (archiveName1 == NULL) archiveName1 = storageSpecifier1->archiveName;
  if (archiveName2 == NULL) archiveName2 = storageSpecifier2->archiveName;

  return    String_equals(storageSpecifier1->deviceName,storageSpecifier2->deviceName)
         && String_equals(archiveName1,archiveName2);
}

LOCAL String StorageDevice_getName(StorageSpecifier *storageSpecifier,
                                   ConstString      archiveName
                                  )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);

  // get file to use
  if      (archiveName != NULL)
  {
    storageFileName = archiveName;
  }
  else if (storageSpecifier->archivePatternString != NULL)
  {
    storageFileName = storageSpecifier->archivePatternString;
  }
  else
  {
    storageFileName = storageSpecifier->archiveName;
  }

  String_appendCString(storageSpecifier->storageName,"device://");
  if (!String_isEmpty(storageSpecifier->deviceName))
  {
    String_append(storageSpecifier->storageName,storageSpecifier->deviceName);
    String_appendChar(storageSpecifier->storageName,':');
  }
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(storageSpecifier->storageName,'/');
    String_append(storageSpecifier->storageName,storageFileName);
  }

  return storageSpecifier->storageName;
}

LOCAL ConstString StorageDevice_getPrintableName(StorageSpecifier *storageSpecifier,
                                                 ConstString      archiveName
                                                )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);

  // get file to use
  if      (!String_isEmpty(archiveName))
  {
    storageFileName = archiveName;
  }
  else if (!String_isEmpty(storageSpecifier->archivePatternString))
  {
    storageFileName = storageSpecifier->archivePatternString;
  }
  else
  {
    storageFileName = storageSpecifier->archiveName;
  }

  String_appendCString(storageSpecifier->storageName,"device://");
  if (!String_isEmpty(storageSpecifier->deviceName))
  {
    String_append(storageSpecifier->storageName,storageSpecifier->deviceName);
    String_appendChar(storageSpecifier->storageName,':');
  }
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(storageSpecifier->storageName,'/');
    String_append(storageSpecifier->storageName,storageFileName);
  }

  return storageSpecifier->storageName;
}

LOCAL Errors StorageDevice_init(StorageHandle          *storageHandle,
                                const StorageSpecifier *storageSpecifier,
                                const JobOptions       *jobOptions
                               )
{
  Errors         error;
  Device         device;
  FileSystemInfo fileSystemInfo;

  assert(storageHandle != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_DEVICE);
  assert(storageSpecifier != NULL);
  assert(globalOptions.device != NULL);

  UNUSED_VARIABLE(storageSpecifier);

  // get device name
  if (String_isEmpty(storageHandle->storageSpecifier.deviceName))
  {
    String_set(storageHandle->storageSpecifier.deviceName,globalOptions.device->name);
  }

  // get device settings
  getDeviceSettings(storageHandle->storageSpecifier.deviceName,jobOptions,&device);

  // init variables
  storageHandle->device.requestVolumeCommand   = device.requestVolumeCommand;
  storageHandle->device.unloadVolumeCommand    = device.unloadVolumeCommand;
  storageHandle->device.loadVolumeCommand      = device.loadVolumeCommand;
  storageHandle->device.volumeSize             = device.volumeSize;
  storageHandle->device.imagePreProcessCommand = device.imagePreProcessCommand;
  storageHandle->device.imagePostProcessCommand= device.imagePostProcessCommand;
  storageHandle->device.imageCommand           = device.imageCommand;
  storageHandle->device.eccPreProcessCommand   = device.eccPreProcessCommand;
  storageHandle->device.eccPostProcessCommand  = device.eccPostProcessCommand;
  storageHandle->device.eccCommand             = device.eccCommand;
  storageHandle->device.writePreProcessCommand = device.writePreProcessCommand;
  storageHandle->device.writePostProcessCommand= device.writePostProcessCommand;
  storageHandle->device.writeCommand           = device.writeCommand;
  storageHandle->device.directory              = String_new();
  if ((jobOptions != NULL) && jobOptions->waitFirstVolumeFlag)
  {
    storageHandle->device.number        = 0;
    storageHandle->device.newVolumeFlag = TRUE;
  }
  else
  {
    storageHandle->device.number        = 1;
    storageHandle->device.newVolumeFlag = FALSE;
  }
  StringList_init(&storageHandle->device.fileNameList);
  storageHandle->device.totalSize              = 0LL;

  // get device settings
  getDeviceSettings(storageHandle->storageSpecifier.deviceName,jobOptions,&device);

  // check space in temporary directory: 2x volumeSize
  error = File_getFileSystemInfo(&fileSystemInfo,tmpDirectory);
  if (error != ERROR_NONE)
  {
    StringList_done(&storageHandle->device.fileNameList);
    return error;
  }
  if (fileSystemInfo.freeBytes < (device.volumeSize*2))
  {
    printWarning("Insufficient space in temporary directory '%s' (%.1lf%s free, %.1lf%s recommended)!\n",
                 String_cString(tmpDirectory),
                 BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                 BYTES_SHORT(device.volumeSize*2),BYTES_UNIT(device.volumeSize*2)
                );
  }

  // create temporary directory for device files
  error = File_getTmpDirectoryName(storageHandle->device.directory,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    StringList_done(&storageHandle->device.fileNameList);
    return error;
  }

  // request first volume for device
  storageHandle->requestedVolumeNumber = 1;

  return ERROR_NONE;
}

LOCAL Errors StorageDevice_done(StorageHandle *storageHandle)
{
  Errors error;
  String fileName;
  Errors tmpError;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  error = ERROR_NONE;

  // delete files
  fileName = String_new();
  while (!StringList_isEmpty(&storageHandle->device.fileNameList))
  {
    StringList_getFirst(&storageHandle->device.fileNameList,fileName);
    tmpError = File_delete(fileName,FALSE);
    if (tmpError != ERROR_NONE)
    {
      if (error == ERROR_NONE) error = tmpError;
    }
  }
  String_delete(fileName);

  // delete temporare directory
  File_delete(storageHandle->device.directory,FALSE);

  // free resources
  StringList_done(&storageHandle->device.fileNameList);
  String_delete(storageHandle->device.directory);

  return error;
}

LOCAL Errors StorageDevice_preProcess(StorageHandle *storageHandle,
                                      ConstString   archiveName,
                                      time_t        timestamp,
                                      bool          initialFlag
                                     )
{
  Errors    error;
  TextMacro textMacros[3];
  String    script;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(globalOptions.device != NULL);

  UNUSED_VARIABLE(initialFlag);

  error = ERROR_NONE;

  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
  {
    // request next volume
    if (storageHandle->device.newVolumeFlag)
    {
      storageHandle->device.number++;
      storageHandle->device.newVolumeFlag = FALSE;

      storageHandle->requestedVolumeNumber = storageHandle->device.number;
    }

    // check if new volume is required
    if (storageHandle->volumeNumber != storageHandle->requestedVolumeNumber)
    {
      error = requestNewDeviceVolume(storageHandle,FALSE);
    }

    // init macros
    TEXT_MACRO_N_STRING (textMacros[0],"%device",storageHandle->storageSpecifier.deviceName,NULL);
    TEXT_MACRO_N_STRING (textMacros[1],"%file",  archiveName,                               NULL);
    TEXT_MACRO_N_INTEGER(textMacros[2],"%number",storageHandle->requestedVolumeNumber,      NULL);

    // write pre-processing
    if ((globalOptions.device != NULL) && (globalOptions.device->writePreProcessCommand != NULL))
    {
      // get script
      script = expandTemplate(String_cString(storageHandle->device.writePreProcessCommand),
                              EXPAND_MACRO_MODE_STRING,
                              timestamp,
                              initialFlag,
                              textMacros,
                              SIZE_OF_ARRAY(textMacros)
                             );
      if (script != NULL)
      {
        // execute script
        error = Misc_executeScript(String_cString(script),
                                   CALLBACK(executeIOOutput,NULL),
                                   CALLBACK(executeIOOutput,NULL)
                                  );
        String_delete(script);
      }
      else
      {
        error = ERROR_EXPAND_TEMPLATE;
      }
    }
  }

  return error;
}

LOCAL Errors StorageDevice_postProcess(StorageHandle *storageHandle,
                                       ConstString   archiveName,
                                       time_t        timestamp,
                                       bool          finalFlag
                                      )
{
  Errors    error;
  String    imageFileName;
  TextMacro textMacros[5];
  String    fileName;
  String    script;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(globalOptions.device != NULL);

  error = ERROR_NONE;

  if (storageHandle->device.volumeSize == 0LL)
  {
    printWarning("Device volume size is 0 bytes!\n");
  }

  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
  {
    if (   (storageHandle->device.totalSize > storageHandle->device.volumeSize)
        || (finalFlag && storageHandle->device.totalSize > 0LL)
       )
    {
      // device size limit reached -> write to device volume and request new volume

      // check if new volume is required
      if (storageHandle->volumeNumber != storageHandle->requestedVolumeNumber)
      {
        error = requestNewDeviceVolume(storageHandle,TRUE);
        if (error != ERROR_NONE)
        {
          return error;
        }
        updateStorageStatusInfo(storageHandle);
      }

      // get temporary image file name
      imageFileName = String_new();
      error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
      if (error != ERROR_NONE)
      {
        return error;
      }

      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%device",   storageHandle->storageSpecifier.deviceName,NULL);
      TEXT_MACRO_N_STRING (textMacros[1],"%directory",storageHandle->device.directory,           NULL);
      TEXT_MACRO_N_STRING (textMacros[2],"%image",    imageFileName,                             NULL);
      TEXT_MACRO_N_STRING (textMacros[3],"%file",     archiveName,                               NULL);
      TEXT_MACRO_N_INTEGER(textMacros[4],"%number",   storageHandle->volumeNumber,               NULL);

      // create image
      if (error == ERROR_NONE)
      {
        printInfo(0,"Make image pre-processing of volume #%d...",storageHandle->volumeNumber);
        error = Misc_executeCommand(String_cString(storageHandle->device.imagePreProcessCommand ),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
      }
      if (error == ERROR_NONE)
      {
        printInfo(0,"Make image volume #%d...",storageHandle->volumeNumber);
        error = Misc_executeCommand(String_cString(storageHandle->device.imageCommand),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
      }
      if (error == ERROR_NONE)
      {
        printInfo(0,"Make image post-processing of volume #%d...",storageHandle->volumeNumber);
        error = Misc_executeCommand(String_cString(storageHandle->device.imagePostProcessCommand),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
      }

      // write to device
      if (error == ERROR_NONE)
      {
        printInfo(0,"Write device pre-processing of volume #%d...",storageHandle->volumeNumber);
        error = Misc_executeCommand(String_cString(storageHandle->device.writePreProcessCommand),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
      }
      if (error == ERROR_NONE)
      {
        printInfo(0,"Write device volume #%d...",storageHandle->volumeNumber);
        error = Misc_executeCommand(String_cString(storageHandle->device.writeCommand),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
      }
      if (error == ERROR_NONE)
      {
        printInfo(0,"Write device post-processing of volume #%d...",storageHandle->volumeNumber);
        error = Misc_executeCommand(String_cString(storageHandle->device.writePostProcessCommand),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
      }

      if (error != ERROR_NONE)
      {
        File_delete(imageFileName,FALSE);
        String_delete(imageFileName);
        return error;
      }
      File_delete(imageFileName,FALSE);
      String_delete(imageFileName);

      // delete stored files
      fileName = String_new();
      while (!StringList_isEmpty(&storageHandle->device.fileNameList))
      {
        StringList_getFirst(&storageHandle->device.fileNameList,fileName);
        error = File_delete(fileName,FALSE);
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      String_delete(fileName);
      if (error != ERROR_NONE)
      {
        return error;
      }

      // reset
      storageHandle->device.newVolumeFlag = TRUE;
      storageHandle->device.totalSize     = 0;
    }

    // write post-processing
    if ((globalOptions.device != NULL) && (globalOptions.device->writePostProcessCommand != NULL))
    {
      // get script
      script = expandTemplate(String_cString(storageHandle->device.writePostProcessCommand),
                              EXPAND_MACRO_MODE_STRING,
                              timestamp,
                              finalFlag,
                              textMacros,
                              SIZE_OF_ARRAY(textMacros)
                             );
      if (script != NULL)
      {
        // execute script
        error = Misc_executeScript(String_cString(script),
                                   CALLBACK(executeIOOutput,NULL),
                                   CALLBACK(executeIOOutput,NULL)
                                  );
        String_delete(script);
      }
      else
      {
        error = ERROR_EXPAND_TEMPLATE;
      }
    }
  }
  else
  {
    // update info
    storageHandle->runningInfo.volumeProgress = 1.0;
    updateStorageStatusInfo(storageHandle);
  }

  return error;
}

LOCAL Errors StorageDevice_unloadVolume(StorageHandle *storageHandle)
{
  Errors    error;
  TextMacro textMacros[1];

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  TEXT_MACRO_N_STRING(textMacros[0],"%device",storageHandle->storageSpecifier.deviceName,NULL);
  error = Misc_executeCommand(String_cString(storageHandle->device.unloadVolumeCommand),
                              textMacros,SIZE_OF_ARRAY(textMacros),
                              CALLBACK(executeIOOutput,NULL),
                              CALLBACK(executeIOOutput,NULL)
                             );

  return error;
}

LOCAL bool StorageDevice_exists(StorageHandle *storageHandle, ConstString archiveName)
{
  assert(storageHandle != NULL);
  assert(!String_isEmpty(archiveName));

HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
  UNUSED_VARIABLE(storageHandle);

  return File_exists(archiveName);
}

LOCAL Errors StorageDevice_create(StorageArchiveHandle *storageArchiveHandle,
                                  ConstString   archiveName,
                                  uint64        archiveSize
                                 )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(archiveSize);

  // create file name
  // init variables
  storageArchiveHandle->device.fileName = String_new();
  String_set(storageArchiveHandle->device.fileName,storageArchiveHandle->storageHandle->device.directory);
  File_appendFileName(storageArchiveHandle->device.fileName,archiveName);

  if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
  {
    // open file
    error = File_open(&storageArchiveHandle->device.fileHandle,
                      storageArchiveHandle->device.fileName,
                      FILE_OPEN_CREATE
                     );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  DEBUG_ADD_RESOURCE_TRACE(&storageArchiveHandle->device,sizeof(storageArchiveHandle->device));

  return ERROR_NONE;
}

LOCAL Errors StorageDevice_open(StorageArchiveHandle *storageArchiveHandle,
                                ConstString archiveName
                               )
{
  assert(storageArchiveHandle != NULL);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(!String_isEmpty(archiveName));

  // init variables
  storageArchiveHandle->device.fileName = String_new();

UNUSED_VARIABLE(storageArchiveHandle);
UNUSED_VARIABLE(archiveName);

  // open file
#ifndef WERROR
#warning TODO still not implemented
#endif
#if 0
  error = File_open(&storageArchiveHandle->fileSystem.fileHandle,
                    archiveName,
                    FILE_OPEN_READ
                   );
  if (error != ERROR_NONE)
  {
    String_delete(storageArchiveHandle->device.fileName);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(&storageArchiveHandle->device,sizeof(storageArchiveHandle->device));
#endif /* 0 */
String_delete(storageArchiveHandle->device.fileName);
  return ERROR_FUNCTION_NOT_SUPPORTED;

  return ERROR_NONE;
}

LOCAL void StorageDevice_close(StorageArchiveHandle *storageArchiveHandle)
{
  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->device);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->device,sizeof(storageArchiveHandle->device));

  switch (storageArchiveHandle->mode)
  {
    case STORAGE_MODE_READ:
      storageArchiveHandle->storageHandle->device.totalSize += File_getSize(&storageArchiveHandle->device.fileHandle);
      File_close(&storageArchiveHandle->device.fileHandle);
      String_delete(storageArchiveHandle->device.fileName);
      break;
    case STORAGE_MODE_WRITE:
      if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
      {
        storageArchiveHandle->storageHandle->device.totalSize += File_getSize(&storageArchiveHandle->device.fileHandle);
        File_close(&storageArchiveHandle->device.fileHandle);
      }
      String_delete(storageArchiveHandle->device.fileName);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  StringList_append(&storageArchiveHandle->storageHandle->device.fileNameList,storageArchiveHandle->device.fileName);
}

LOCAL bool StorageDevice_eof(StorageArchiveHandle *storageArchiveHandle)
{
  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->device);
  assert(storageArchiveHandle->mode == STORAGE_MODE_READ);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
  {
    return File_eof(&storageArchiveHandle->device.fileHandle);
  }
  else
  {
    return TRUE;
  }
}

LOCAL Errors StorageDevice_read(StorageArchiveHandle *storageArchiveHandle,
                                void          *buffer,
                                ulong         size,
                                ulong         *bytesRead
                               )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->device);
  assert(storageArchiveHandle->mode == STORAGE_MODE_READ);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(buffer != NULL);

//fprintf(stderr,"%s,%d: size=%lu\n",__FILE__,__LINE__,size);
  if (bytesRead != NULL) (*bytesRead) = 0L;
  error = ERROR_NONE;
  if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
  {
    error = File_read(&storageArchiveHandle->device.fileHandle,buffer,size,bytesRead);
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageDevice_write(StorageArchiveHandle *storageArchiveHandle,
                                 const void    *buffer,
                                 ulong         size
                                )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->device);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->mode == STORAGE_MODE_WRITE);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(buffer != NULL);

  error = ERROR_NONE;
  if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
  {
    error = File_write(&storageArchiveHandle->device.fileHandle,buffer,size);
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageDevice_tell(StorageArchiveHandle *storageArchiveHandle,
                                uint64        *offset
                               )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->device);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
  {
    error = File_tell(&storageArchiveHandle->device.fileHandle,offset);
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageDevice_seek(StorageArchiveHandle *storageArchiveHandle,
                                uint64        offset
                               )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->device);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  error = ERROR_NONE;
  if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
  {
    error = File_seek(&storageArchiveHandle->device.fileHandle,offset);
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL uint64 StorageDevice_getSize(StorageArchiveHandle *storageArchiveHandle)
{
  uint64 size;

  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->device);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  size = 0LL;
  if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
  {
    size = File_getSize(&storageArchiveHandle->device.fileHandle);
  }

  return size;
}

LOCAL Errors StorageDevice_delete(StorageHandle *storageHandle,
                                  ConstString   archiveName
                                 )
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageHandle);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

#if 0
still not complete
LOCAL Errors StorageDevice_getFileInfo(StorageHandle *storageHandle,
                                       ConstString   fileName,
                                       FileInfo      *fileInfo
                                      )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_DEVICE);
  assert(fileInfo != NULL);

  UNUSED_VARIABLE(storageHandle);
  UNUSED_VARIABLE(fileName);
  UNUSED_VARIABLE(fileInfo);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageDevice_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                             const StorageSpecifier     *storageSpecifier,
                                             const JobOptions           *jobOptions,
                                             ServerConnectionPriorities serverConnectionPriority,
                                             ConstString                archiveName
                                            )
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);

  UNUSED_VARIABLE(storageDirectoryListHandle);
  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

LOCAL void StorageDevice_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);

  UNUSED_VARIABLE(storageDirectoryListHandle);
}

LOCAL bool StorageDevice_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);

  UNUSED_VARIABLE(storageDirectoryListHandle);

  return TRUE;
}

LOCAL Errors StorageDevice_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                             String                     fileName,
                                             FileInfo                   *fileInfo
                                            )
{
  assert(storageDirectoryListHandle != NULL);

  UNUSED_VARIABLE(storageDirectoryListHandle);
  UNUSED_VARIABLE(fileName);
  UNUSED_VARIABLE(fileInfo);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
