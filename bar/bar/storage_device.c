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

#define UNLOAD_VOLUME_DELAY_TIME (10LL*US_PER_SECOND) /* [us] */
#define LOAD_VOLUME_DELAY_TIME   (10LL*US_PER_SECOND) /* [us] */

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
* Input  : storage  - storage
*          waitFlag - TRUE to wait for new volume
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors requestNewDeviceVolume(Storage *storage, bool waitFlag)
{
  TextMacro                   textMacros[2];
  bool                        volumeRequestedFlag;
  StorageRequestVolumeResults storageRequestVolumeResult;

  TEXT_MACRO_N_STRING (textMacros[0],"%device",storage->storageSpecifier.deviceName,NULL);
  TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storage->requestedVolumeNumber,      NULL);

  if (   (storage->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storage->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    // sleep a short time to give hardware time for finishing volume; unload current volume
    printInfo(1,"Unload volume #%d...",storage->volumeNumber);
    Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
    Misc_executeCommand(String_cString(storage->device.unloadVolumeCommand),
                        textMacros,SIZE_OF_ARRAY(textMacros),
                        CALLBACK(executeIOOutput,NULL),
                        CALLBACK(executeIOOutput,NULL)
                       );
    printInfo(1,"OK\n");

    storage->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  // request new volume
  volumeRequestedFlag  = FALSE;
  storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_UNKNOWN;
  if      (storage->requestVolumeFunction != NULL)
  {
    volumeRequestedFlag = TRUE;

    // request new volume via call back, unload if requested
    do
    {
      storageRequestVolumeResult = storage->requestVolumeFunction(STORAGE_REQUEST_VOLUME_TYPE_NEW,
                                                                  storage->requestedVolumeNumber,
                                                                  NULL,  // message
                                                                  storage->requestVolumeUserData
                                                                 );
      if (storageRequestVolumeResult == STORAGE_REQUEST_VOLUME_RESULT_UNLOAD)
      {
        // sleep a short time to give hardware time for finishing volume, then unload current medium
        printInfo(1,"Unload volume...");
        Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
        Misc_executeCommand(String_cString(storage->device.unloadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           );
        printInfo(1,"OK\n");
      }
    }
    while (storageRequestVolumeResult == STORAGE_REQUEST_VOLUME_RESULT_UNLOAD);

    storage->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (storage->device.requestVolumeCommand != NULL)
  {
    volumeRequestedFlag = TRUE;

    // request new volume via external command
    printInfo(1,"Request new volume #%d...",storage->requestedVolumeNumber);
    if (Misc_executeCommand(String_cString(storage->device.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           ) == ERROR_NONE
       )
    {
      printInfo(1,"OK\n");
      storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_OK;
    }
    else
    {
      printInfo(1,"FAIL\n");
      storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_FAIL;
    }

    storage->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else
  {
    if (storage->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
    {
      if (waitFlag)
      {
        volumeRequestedFlag = TRUE;

        printInfo(0,"Please insert volume #%d into drive '%s' and press ENTER to continue\n",storage->requestedVolumeNumber,storage->storageSpecifier.deviceName);
        Misc_waitEnter();

        storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_OK;
      }
      else
      {
        printInfo(0,"Please insert volume #%d into drive '%s'\n",storage->requestedVolumeNumber,storage->storageSpecifier.deviceName);
      }
    }
    else
    {
      if (waitFlag)
      {
        volumeRequestedFlag = TRUE;

        printInfo(0,"Press ENTER to continue\n");
        Misc_waitEnter();

        storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_OK;
      }
    }

    storage->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (volumeRequestedFlag)
  {
    switch (storageRequestVolumeResult)
    {
      case STORAGE_REQUEST_VOLUME_RESULT_OK:
        // load volume; sleep a short time to give hardware time for reading volume information
        printInfo(1,"Load volume #%d...",storage->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storage->device.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           );
        Misc_udelay(LOAD_VOLUME_DELAY_TIME);
        printInfo(1,"OK\n");

        // store new volume number
        storage->volumeNumber = storage->requestedVolumeNumber;

        // update status info
        storage->runningInfo.volumeNumber = storage->volumeNumber;
        updateStorageStatusInfo(storage);

        storage->volumeState = STORAGE_VOLUME_STATE_LOADED;
        return ERROR_NONE;
        break;
      case STORAGE_REQUEST_VOLUME_RESULT_ABORTED:
        // load volume
        printInfo(1,"Load volume #%d...",storage->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storage->device.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           );
        printInfo(1,"OK\n");

        return ERROR_ABORTED;
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

LOCAL Errors StorageDevice_init(Storage                *storage,
                                const StorageSpecifier *storageSpecifier,
                                const JobOptions       *jobOptions
                               )
{
  Errors         error;
  Device         device;
  FileSystemInfo fileSystemInfo;

  assert(storage != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_DEVICE);
  assert(storageSpecifier != NULL);
  assert(globalOptions.device != NULL);

  UNUSED_VARIABLE(storageSpecifier);

  // get device name
  if (String_isEmpty(storage->storageSpecifier.deviceName))
  {
    String_set(storage->storageSpecifier.deviceName,globalOptions.device->name);
  }

  // get device settings
  getDeviceSettings(storage->storageSpecifier.deviceName,jobOptions,&device);

  // init variables
  storage->device.requestVolumeCommand   = device.requestVolumeCommand;
  storage->device.unloadVolumeCommand    = device.unloadVolumeCommand;
  storage->device.loadVolumeCommand      = device.loadVolumeCommand;
  storage->device.volumeSize             = device.volumeSize;
  storage->device.imagePreProcessCommand = device.imagePreProcessCommand;
  storage->device.imagePostProcessCommand= device.imagePostProcessCommand;
  storage->device.imageCommand           = device.imageCommand;
  storage->device.eccPreProcessCommand   = device.eccPreProcessCommand;
  storage->device.eccPostProcessCommand  = device.eccPostProcessCommand;
  storage->device.eccCommand             = device.eccCommand;
  storage->device.writePreProcessCommand = device.writePreProcessCommand;
  storage->device.writePostProcessCommand= device.writePostProcessCommand;
  storage->device.writeCommand           = device.writeCommand;
  storage->device.directory              = String_new();
  if ((jobOptions != NULL) && jobOptions->waitFirstVolumeFlag)
  {
    storage->device.number        = 0;
    storage->device.newVolumeFlag = TRUE;
  }
  else
  {
    storage->device.number        = 1;
    storage->device.newVolumeFlag = FALSE;
  }
  StringList_init(&storage->device.fileNameList);
  storage->device.totalSize              = 0LL;

  // get device settings
  getDeviceSettings(storage->storageSpecifier.deviceName,jobOptions,&device);

  // check space in temporary directory: 2x volumeSize
  error = File_getFileSystemInfo(&fileSystemInfo,tmpDirectory);
  if (error != ERROR_NONE)
  {
    StringList_done(&storage->device.fileNameList);
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
  error = File_getTmpDirectoryName(storage->device.directory,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    StringList_done(&storage->device.fileNameList);
    return error;
  }

  // request first volume for device
  storage->requestedVolumeNumber = 1;

  return ERROR_NONE;
}

LOCAL Errors StorageDevice_done(Storage *storage)
{
  Errors error;
  String fileName;
  Errors tmpError;

  assert(storage != NULL);
  assert(storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  error = ERROR_NONE;

  // delete files
  fileName = String_new();
  while (!StringList_isEmpty(&storage->device.fileNameList))
  {
    StringList_removeFirst(&storage->device.fileNameList,fileName);
    tmpError = File_delete(fileName,FALSE);
    if (tmpError != ERROR_NONE)
    {
      if (error == ERROR_NONE) error = tmpError;
    }
  }
  String_delete(fileName);

  // delete temporare directory
  File_delete(storage->device.directory,FALSE);

  // free resources
  StringList_done(&storage->device.fileNameList);
  String_delete(storage->device.directory);

  return error;
}

LOCAL Errors StorageDevice_preProcess(Storage     *storage,
                                      ConstString archiveName,
                                      time_t      timestamp,
                                      bool        initialFlag
                                     )
{
  Errors    error;
  TextMacro textMacros[3];
  String    script;

  assert(storage != NULL);
  assert(storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(globalOptions.device != NULL);

  UNUSED_VARIABLE(initialFlag);

  error = ERROR_NONE;

  if ((storage->jobOptions == NULL) || !storage->jobOptions->dryRunFlag)
  {
    // request next volume
    if (storage->device.newVolumeFlag)
    {
      storage->device.number++;
      storage->device.newVolumeFlag = FALSE;

      storage->requestedVolumeNumber = storage->device.number;
    }

    // check if new volume is required
    if (storage->volumeNumber != storage->requestedVolumeNumber)
    {
      error = requestNewDeviceVolume(storage,FALSE);
    }

    // init macros
    TEXT_MACRO_N_STRING (textMacros[0],"%device",storage->storageSpecifier.deviceName,NULL);
    TEXT_MACRO_N_STRING (textMacros[1],"%file",  archiveName,                         NULL);
    TEXT_MACRO_N_INTEGER(textMacros[2],"%number",storage->requestedVolumeNumber,      NULL);

    // write pre-processing
    if ((globalOptions.device != NULL) && (globalOptions.device->writePreProcessCommand != NULL))
    {
      // get script
      script = expandTemplate(String_cString(storage->device.writePreProcessCommand),
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

LOCAL Errors StorageDevice_postProcess(Storage     *storage,
                                       ConstString archiveName,
                                       time_t      timestamp,
                                       bool        finalFlag
                                      )
{
  Errors    error;
  String    imageFileName;
  TextMacro textMacros[5];
  String    fileName;
  String    script;

  assert(storage != NULL);
  assert(storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(globalOptions.device != NULL);

  error = ERROR_NONE;

  if (storage->device.volumeSize == 0LL)
  {
    printWarning("Device volume size is 0 bytes!\n");
  }

  if ((storage->jobOptions == NULL) || !storage->jobOptions->dryRunFlag)
  {
    if (   (storage->device.totalSize > storage->device.volumeSize)
        || (finalFlag && storage->device.totalSize > 0LL)
       )
    {
      // device size limit reached -> write to device volume and request new volume

      // check if new volume is required
      if (storage->volumeNumber != storage->requestedVolumeNumber)
      {
        error = requestNewDeviceVolume(storage,TRUE);
        if (error != ERROR_NONE)
        {
          return error;
        }
        updateStorageStatusInfo(storage);
      }

      // get temporary image file name
      imageFileName = String_new();
      error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
      if (error != ERROR_NONE)
      {
        return error;
      }

      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%device",   storage->storageSpecifier.deviceName,NULL);
      TEXT_MACRO_N_STRING (textMacros[1],"%directory",storage->device.directory,           NULL);
      TEXT_MACRO_N_STRING (textMacros[2],"%image",    imageFileName,                       NULL);
      TEXT_MACRO_N_STRING (textMacros[3],"%file",     archiveName,                         NULL);
      TEXT_MACRO_N_INTEGER(textMacros[4],"%number",   storage->volumeNumber,               NULL);

      // create image
      if (error == ERROR_NONE)
      {
        printInfo(1,"Make image pre-processing of volume #%d...",storage->volumeNumber);
        error = Misc_executeCommand(String_cString(storage->device.imagePreProcessCommand ),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
      if (error == ERROR_NONE)
      {
        printInfo(1,"Make image volume #%d...",storage->volumeNumber);
        error = Misc_executeCommand(String_cString(storage->device.imageCommand),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
      if (error == ERROR_NONE)
      {
        printInfo(1,"Make image post-processing of volume #%d...",storage->volumeNumber);
        error = Misc_executeCommand(String_cString(storage->device.imagePostProcessCommand),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }

      // write to device
      if (error == ERROR_NONE)
      {
        printInfo(1,"Write device pre-processing of volume #%d...",storage->volumeNumber);
        error = Misc_executeCommand(String_cString(storage->device.writePreProcessCommand),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
      if (error == ERROR_NONE)
      {
        printInfo(1,"Write device volume #%d...",storage->volumeNumber);
        error = Misc_executeCommand(String_cString(storage->device.writeCommand),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
      if (error == ERROR_NONE)
      {
        printInfo(1,"Write device post-processing of volume #%d...",storage->volumeNumber);
        error = Misc_executeCommand(String_cString(storage->device.writePostProcessCommand),
                                    textMacros,
                                    SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOOutput,NULL),
                                    CALLBACK(executeIOOutput,NULL)
                                   );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
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
      while (!StringList_isEmpty(&storage->device.fileNameList))
      {
        StringList_removeFirst(&storage->device.fileNameList,fileName);
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
      storage->device.newVolumeFlag = TRUE;
      storage->device.totalSize     = 0;
    }

    // write post-processing
    if ((globalOptions.device != NULL) && (globalOptions.device->writePostProcessCommand != NULL))
    {
      // get script
      script = expandTemplate(String_cString(storage->device.writePostProcessCommand),
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
    storage->runningInfo.volumeProgress = 1.0;
    updateStorageStatusInfo(storage);
  }

  return error;
}

LOCAL Errors StorageDevice_unloadVolume(Storage *storage)
{
  Errors    error;
  TextMacro textMacros[1];

  assert(storage != NULL);
  assert(storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  TEXT_MACRO_N_STRING(textMacros[0],"%device",storage->storageSpecifier.deviceName,NULL);
  error = Misc_executeCommand(String_cString(storage->device.unloadVolumeCommand),
                              textMacros,SIZE_OF_ARRAY(textMacros),
                              CALLBACK(executeIOOutput,NULL),
                              CALLBACK(executeIOOutput,NULL)
                             );

  return error;
}

LOCAL bool StorageDevice_exists(Storage *storage, ConstString archiveName)
{
  assert(storage != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storage);
  UNUSED_VARIABLE(archiveName);

  return File_exists(archiveName);
}

LOCAL Errors StorageDevice_create(StorageHandle *storageHandle,
                                  ConstString   archiveName,
                                  uint64        archiveSize
                                 )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(archiveSize);

  // create file name
  // init variables
  storageHandle->device.fileName = String_new();
  String_set(storageHandle->device.fileName,storageHandle->storage->device.directory);
  File_appendFileName(storageHandle->device.fileName,archiveName);

  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    // open file
    error = File_open(&storageHandle->device.fileHandle,
                      storageHandle->device.fileName,
                      FILE_OPEN_CREATE
                     );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->device,sizeof(storageHandle->device));

  return ERROR_NONE;
}

LOCAL Errors StorageDevice_open(StorageHandle *storageHandle,
                                ConstString   archiveName
                               )
{
  assert(storageHandle != NULL);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(!String_isEmpty(archiveName));

  // init variables
  storageHandle->device.fileName = String_new();

UNUSED_VARIABLE(storageHandle);
UNUSED_VARIABLE(archiveName);

  // open file
#ifndef WERROR
#warning TODO still not implemented
#endif
#if 0
  error = File_open(&storageHandle->fileSystem.fileHandle,
                    archiveName,
                    FILE_OPEN_READ
                   );
  if (error != ERROR_NONE)
  {
    String_delete(storageHandle->device.fileName);
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->device,sizeof(storageHandle->device));
#endif /* 0 */
String_delete(storageHandle->device.fileName);
  return ERROR_FUNCTION_NOT_SUPPORTED;

  return ERROR_NONE;
}

LOCAL void StorageDevice_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->device,sizeof(storageHandle->device));

  switch (storageHandle->mode)
  {
    case STORAGE_MODE_READ:
      storageHandle->storage->device.totalSize += File_getSize(&storageHandle->device.fileHandle);
      File_close(&storageHandle->device.fileHandle);
      String_delete(storageHandle->device.fileName);
      break;
    case STORAGE_MODE_WRITE:
      if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
      {
        storageHandle->storage->device.totalSize += File_getSize(&storageHandle->device.fileHandle);
        File_close(&storageHandle->device.fileHandle);
      }
      String_delete(storageHandle->device.fileName);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  StringList_append(&storageHandle->storage->device.fileNameList,storageHandle->device.fileName);
}

LOCAL bool StorageDevice_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    return File_eof(&storageHandle->device.fileHandle);
  }
  else
  {
    return TRUE;
  }
}

LOCAL Errors StorageDevice_read(StorageHandle *storageHandle,
                                void          *buffer,
                                ulong         size,
                                ulong         *bytesRead
                               )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(buffer != NULL);

//fprintf(stderr,"%s,%d: size=%lu\n",__FILE__,__LINE__,size);
  if (bytesRead != NULL) (*bytesRead) = 0L;
  error = ERROR_NONE;
  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    error = File_read(&storageHandle->device.fileHandle,buffer,size,bytesRead);
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageDevice_write(StorageHandle *storageHandle,
                                 const void    *buffer,
                                 ulong         size
                                )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(buffer != NULL);

  error = ERROR_NONE;
  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    error = File_write(&storageHandle->device.fileHandle,buffer,size);
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageDevice_tell(StorageHandle *storageHandle,
                                uint64        *offset
                               )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    error = File_tell(&storageHandle->device.fileHandle,offset);
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageDevice_seek(StorageHandle *storageHandle,
                                uint64        offset
                               )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  error = ERROR_NONE;
  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    error = File_seek(&storageHandle->device.fileHandle,offset);
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL uint64 StorageDevice_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storage != NULL);
  assert(storageHandle->storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  size = 0LL;
  if ((storageHandle->storage->jobOptions == NULL) || !storageHandle->storage->jobOptions->dryRunFlag)
  {
    size = File_getSize(&storageHandle->device.fileHandle);
  }

  return size;
}

LOCAL Errors StorageDevice_delete(Storage     *storage,
                                  ConstString archiveName
                                 )
{
  assert(storage != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storage);
  assert(storage->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storage);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

#if 0
still not complete
LOCAL Errors StorageDevice_getFileInfo(Storage     *storage,
                                       ConstString fileName,
                                       FileInfo    *fileInfo
                                      )
{
  Errors error;

  assert(storage != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_DEVICE);
  assert(fileInfo != NULL);

  UNUSED_VARIABLE(storage);
  UNUSED_VARIABLE(fileName);
  UNUSED_VARIABLE(fileInfo);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageDevice_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                             const StorageSpecifier     *storageSpecifier,
                                             ConstString                archiveName,
                                             const JobOptions           *jobOptions,
                                             ServerConnectionPriorities serverConnectionPriority
                                            )
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageDirectoryListHandle);
  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(archiveName);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);

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
