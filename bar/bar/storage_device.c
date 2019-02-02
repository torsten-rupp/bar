/***********************************************************************\
*
* $Revision: 4012 $
* $Date: 2015-04-28 19:02:40 +0200 (Tue, 28 Apr 2015) $
* $Author: torsten $
* Contents: storage device functions
* Systems: all
*
\***********************************************************************/

#define __STORAGE_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/autofree.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/files.h"
#include "common/network.h"
#include "common/passwords.h"
#include "common/misc.h"

#include "errors.h"
#include "crypt.h"
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

LOCAL Errors requestNewDeviceVolume(StorageInfo *storageInfo, bool waitFlag)
{
  TextMacro                   textMacros[2];
  bool                        volumeRequestedFlag;
  StorageRequestVolumeResults storageRequestVolumeResult;

  TEXT_MACRO_N_STRING (textMacros[0],"%device",storageInfo->storageSpecifier.deviceName,NULL);
  TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageInfo->requestedVolumeNumber,      NULL);

  if (   (storageInfo->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageInfo->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    // sleep a short time to give hardware time for finishing volume; unload current volume
    printInfo(1,"Unload volume #%d...",storageInfo->volumeNumber);
    Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
    Misc_executeCommand(String_cString(storageInfo->device.unloadVolumeCommand),
                        textMacros,SIZE_OF_ARRAY(textMacros),
                        CALLBACK(executeIOOutput,NULL),
                        CALLBACK(executeIOOutput,NULL)
                       );
    printInfo(1,"OK\n");

    storageInfo->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  // request new volume
  volumeRequestedFlag  = FALSE;
  storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_UNKNOWN;
  if      (storageInfo->requestVolumeFunction != NULL)
  {
    volumeRequestedFlag = TRUE;

    // request new volume via call back, unload if requested
    do
    {
      storageRequestVolumeResult = storageInfo->requestVolumeFunction(STORAGE_REQUEST_VOLUME_TYPE_NEW,
                                                                  storageInfo->requestedVolumeNumber,
                                                                  NULL,  // message
                                                                  storageInfo->requestVolumeUserData
                                                                 );
      if (storageRequestVolumeResult == STORAGE_REQUEST_VOLUME_RESULT_UNLOAD)
      {
        // sleep a short time to give hardware time for finishing volume, then unload current medium
        printInfo(1,"Unload volume...");
        Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
        Misc_executeCommand(String_cString(storageInfo->device.unloadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           );
        printInfo(1,"OK\n");
      }
    }
    while (storageRequestVolumeResult == STORAGE_REQUEST_VOLUME_RESULT_UNLOAD);

    storageInfo->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (storageInfo->device.requestVolumeCommand != NULL)
  {
    volumeRequestedFlag = TRUE;

    // request new volume via external command
    printInfo(1,"Request new volume #%d...",storageInfo->requestedVolumeNumber);
    if (Misc_executeCommand(String_cString(storageInfo->device.loadVolumeCommand),
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

    storageInfo->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else
  {
    if (storageInfo->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
    {
      if (waitFlag)
      {
        volumeRequestedFlag = TRUE;

        printInfo(0,"Please insert volume #%d into drive '%s' and press ENTER to continue\n",storageInfo->requestedVolumeNumber,storageInfo->storageSpecifier.deviceName);
        Misc_waitEnter();

        storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_OK;
      }
      else
      {
        printInfo(0,"Please insert volume #%d into drive '%s'\n",storageInfo->requestedVolumeNumber,storageInfo->storageSpecifier.deviceName);
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

    storageInfo->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (volumeRequestedFlag)
  {
    switch (storageRequestVolumeResult)
    {
      case STORAGE_REQUEST_VOLUME_RESULT_OK:
        // load volume; sleep a short time to give hardware time for reading volume information
        printInfo(1,"Load volume #%d...",storageInfo->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storageInfo->device.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           );
        Misc_udelay(LOAD_VOLUME_DELAY_TIME);
        printInfo(1,"OK\n");

        // store new volume number
        storageInfo->volumeNumber = storageInfo->requestedVolumeNumber;

        // update volume info
        storageInfo->runningInfo.volumeNumber = storageInfo->volumeNumber;
        updateStorageStatusInfo(storageInfo);

        storageInfo->volumeState = STORAGE_VOLUME_STATE_LOADED;
        return ERROR_NONE;
        break;
      case STORAGE_REQUEST_VOLUME_RESULT_ABORTED:
        // load volume
        printInfo(1,"Load volume #%d...",storageInfo->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storageInfo->device.loadVolumeCommand),
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

LOCAL String StorageDevice_getName(String                 string,
                                   const StorageSpecifier *storageSpecifier,
                                   ConstString            archiveName
                                  )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);

  // get file to use
  if      (!String_isEmpty(archiveName))
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

  String_appendCString(string,"device://");
  if (!String_isEmpty(storageSpecifier->deviceName))
  {
    String_append(string,storageSpecifier->deviceName);
    String_appendChar(string,':');
  }
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(string,'/');
    String_append(string,storageFileName);
  }

  return string;
}

LOCAL void StorageDevice_getPrintableName(String                 string,
                                          const StorageSpecifier *storageSpecifier,
                                          ConstString            archiveName
                                         )
{
  ConstString storageFileName;

  assert(string != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_DEVICE);

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

  String_appendCString(string,"device://");
  if (!String_isEmpty(storageSpecifier->deviceName))
  {
    String_append(string,storageSpecifier->deviceName);
    String_appendChar(string,':');
  }
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(string,'/');
    String_append(string,storageFileName);
  }
}

LOCAL Errors StorageDevice_init(StorageInfo            *storageInfo,
                                const StorageSpecifier *storageSpecifier,
                                const JobOptions       *jobOptions
                               )
{
  Errors         error;
  Device         device;
  FileSystemInfo fileSystemInfo;

  assert(storageInfo != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_DEVICE);
  assert(storageSpecifier != NULL);
  assert(globalOptions.device != NULL);

  UNUSED_VARIABLE(storageSpecifier);

  // get device name
  if (String_isEmpty(storageInfo->storageSpecifier.deviceName))
  {
    String_set(storageInfo->storageSpecifier.deviceName,globalOptions.device->name);
  }

  // get device settings
  initDeviceSettings(&device,storageInfo->storageSpecifier.deviceName,jobOptions);

  // init variables
  storageInfo->device.requestVolumeCommand   = device.requestVolumeCommand;
  storageInfo->device.unloadVolumeCommand    = device.unloadVolumeCommand;
  storageInfo->device.loadVolumeCommand      = device.loadVolumeCommand;
  storageInfo->device.volumeSize             = device.volumeSize;
  storageInfo->device.imagePreProcessCommand = device.imagePreProcessCommand;
  storageInfo->device.imagePostProcessCommand= device.imagePostProcessCommand;
  storageInfo->device.imageCommand           = device.imageCommand;
  storageInfo->device.eccPreProcessCommand   = device.eccPreProcessCommand;
  storageInfo->device.eccPostProcessCommand  = device.eccPostProcessCommand;
  storageInfo->device.eccCommand             = device.eccCommand;
  storageInfo->device.writePreProcessCommand = device.writePreProcessCommand;
  storageInfo->device.writePostProcessCommand= device.writePostProcessCommand;
  storageInfo->device.writeCommand           = device.writeCommand;
  storageInfo->device.directory              = String_new();
  if ((jobOptions != NULL) && jobOptions->waitFirstVolumeFlag)
  {
    storageInfo->device.number        = 0;
    storageInfo->device.newVolumeFlag = TRUE;
  }
  else
  {
    storageInfo->device.number        = 1;
    storageInfo->device.newVolumeFlag = FALSE;
  }
  StringList_init(&storageInfo->device.fileNameList);
  storageInfo->device.totalSize              = 0LL;

  // get device settings
  initDeviceSettings(&device,storageInfo->storageSpecifier.deviceName,jobOptions);

  // check space in temporary directory: 2x volumeSize
  error = File_getFileSystemInfo(&fileSystemInfo,tmpDirectory);
  if (error != ERROR_NONE)
  {
    StringList_done(&storageInfo->device.fileNameList);
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
  error = File_getTmpDirectoryName(storageInfo->device.directory,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    StringList_done(&storageInfo->device.fileNameList);
    doneDeviceSettings(&device);
    return error;
  }

  // request first volume for device
  storageInfo->requestedVolumeNumber = 1;

  // free resources
  doneDeviceSettings(&device);

  return ERROR_NONE;
}

LOCAL Errors StorageDevice_done(StorageInfo *storageInfo)
{
  Errors error;
  String fileName;
  Errors tmpError;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_DEVICE);

  error = ERROR_NONE;

  // delete files
  fileName = String_new();
  while (!StringList_isEmpty(&storageInfo->device.fileNameList))
  {
    StringList_removeFirst(&storageInfo->device.fileNameList,fileName);
    tmpError = File_delete(fileName,FALSE);
    if (tmpError != ERROR_NONE)
    {
      if (error == ERROR_NONE) error = tmpError;
    }
  }
  String_delete(fileName);

  // delete temporare directory
  File_delete(storageInfo->device.directory,FALSE);

  // free resources
  StringList_done(&storageInfo->device.fileNameList);
  String_delete(storageInfo->device.directory);

  return error;
}

LOCAL Errors StorageDevice_preProcess(StorageInfo *storageInfo,
                                      ConstString archiveName,
                                      time_t      timestamp,
                                      bool        initialFlag
                                     )
{
  Errors    error;
  TextMacro textMacros[3];

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_DEVICE);
  assert(globalOptions.device != NULL);

  UNUSED_VARIABLE(initialFlag);

  error = ERROR_NONE;

  // request next volume
  if (storageInfo->device.newVolumeFlag)
  {
    storageInfo->device.number++;
    storageInfo->device.newVolumeFlag = FALSE;

    storageInfo->requestedVolumeNumber = storageInfo->device.number;
  }

  // check if new volume is required
  if (storageInfo->volumeNumber != storageInfo->requestedVolumeNumber)
  {
    error = requestNewDeviceVolume(storageInfo,FALSE);
  }

  // init macros
  TEXT_MACRO_N_STRING (textMacros[0],"%device",storageInfo->storageSpecifier.deviceName,NULL);
  TEXT_MACRO_N_STRING (textMacros[1],"%file",  archiveName,                             NULL);
  TEXT_MACRO_N_INTEGER(textMacros[2],"%number",storageInfo->requestedVolumeNumber,      NULL);

  // write pre-processing
  if (!String_isEmpty(globalOptions.device->writePreProcessCommand))
  {
    printInfo(1,"Write pre-processing...");
    error = executeTemplate(String_cString(storageInfo->device.writePreProcessCommand),
                            timestamp,
                            textMacros,
                            SIZE_OF_ARRAY(textMacros)
                           );
    printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
  }

  return error;
}

LOCAL Errors StorageDevice_postProcess(StorageInfo *storageInfo,
                                       ConstString archiveName,
                                       time_t      timestamp,
                                       bool        finalFlag
                                      )
{
  Errors    error;
  String    imageFileName;
  TextMacro textMacros[5];
  String    fileName;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_DEVICE);
  assert(globalOptions.device != NULL);

  error = ERROR_NONE;

  if (storageInfo->device.volumeSize == 0LL)
  {
    printWarning("Device volume size is 0 bytes!\n");
  }

  if (   (storageInfo->device.totalSize > storageInfo->device.volumeSize)
      || (finalFlag && storageInfo->device.totalSize > 0LL)
     )
  {
    // device size limit reached -> write to device volume and request new volume

    // check if new volume is required
    if (storageInfo->volumeNumber != storageInfo->requestedVolumeNumber)
    {
      error = requestNewDeviceVolume(storageInfo,TRUE);
      if (error != ERROR_NONE)
      {
        return error;
      }
      updateStorageStatusInfo(storageInfo);
    }

    // get temporary image file name
    imageFileName = String_new();
    error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // init macros
    TEXT_MACRO_N_STRING (textMacros[0],"%device",   storageInfo->storageSpecifier.deviceName,NULL);
    TEXT_MACRO_N_STRING (textMacros[1],"%directory",storageInfo->device.directory,           NULL);
    TEXT_MACRO_N_STRING (textMacros[2],"%image",    imageFileName,                           NULL);
    TEXT_MACRO_N_STRING (textMacros[3],"%file",     archiveName,                             NULL);
    TEXT_MACRO_N_INTEGER(textMacros[4],"%number",   storageInfo->volumeNumber,               NULL);

    // create image
    if (error == ERROR_NONE)
    {
      printInfo(1,"Make image pre-processing of volume #%d...",storageInfo->volumeNumber);
      error = Misc_executeCommand(String_cString(storageInfo->device.imagePreProcessCommand ),
                                  textMacros,
                                  SIZE_OF_ARRAY(textMacros),
                                  CALLBACK(executeIOOutput,NULL),
                                  CALLBACK(executeIOOutput,NULL)
                                 );
      printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
    }
    if (error == ERROR_NONE)
    {
      printInfo(1,"Make image volume #%d...",storageInfo->volumeNumber);
      error = Misc_executeCommand(String_cString(storageInfo->device.imageCommand),
                                  textMacros,
                                  SIZE_OF_ARRAY(textMacros),
                                  CALLBACK(executeIOOutput,NULL),
                                  CALLBACK(executeIOOutput,NULL)
                                 );
      printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
    }
    if (error == ERROR_NONE)
    {
      printInfo(1,"Make image post-processing of volume #%d...",storageInfo->volumeNumber);
      error = Misc_executeCommand(String_cString(storageInfo->device.imagePostProcessCommand),
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
      if (!String_isEmpty(storageInfo->device.writePreProcessCommand))
      {
        printInfo(1,"Write device pre-processing of volume #%d...",storageInfo->volumeNumber);
        error = executeTemplate(String_cString(storageInfo->device.writePreProcessCommand),
                                timestamp,
                                textMacros,
                                SIZE_OF_ARRAY(textMacros)
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
    }
    if (error == ERROR_NONE)
    {
      printInfo(1,"Write device volume #%d...",storageInfo->volumeNumber);
      error = Misc_executeCommand(String_cString(storageInfo->device.writeCommand),
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
    while (!StringList_isEmpty(&storageInfo->device.fileNameList))
    {
      StringList_removeFirst(&storageInfo->device.fileNameList,fileName);
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
    storageInfo->device.newVolumeFlag = TRUE;
    storageInfo->device.totalSize     = 0;

    // write post-processing
    if (!String_isEmpty(storageInfo->device.writePostProcessCommand))
    {
      // write post-processing
      if (!String_isEmpty(storageInfo->device.writePostProcessCommand))
      {
        // get script
        printInfo(1,"Write device post-processing of volume #%d...",storageInfo->volumeNumber);
        error = executeTemplate(String_cString(storageInfo->device.writePostProcessCommand),
                                timestamp,
                                textMacros,
                                SIZE_OF_ARRAY(textMacros)
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
    }
  }

  return error;
}

LOCAL Errors StorageDevice_unloadVolume(const StorageInfo *storageInfo)
{
  Errors    error;
  TextMacro textMacros[1];

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_DEVICE);

  TEXT_MACRO_N_STRING(textMacros[0],"%device",storageInfo->storageSpecifier.deviceName,NULL);
  error = Misc_executeCommand(String_cString(storageInfo->device.unloadVolumeCommand),
                              textMacros,SIZE_OF_ARRAY(textMacros),
                              CALLBACK(executeIOOutput,NULL),
                              CALLBACK(executeIOOutput,NULL)
                             );

  return error;
}

LOCAL bool StorageDevice_exists(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL bool StorageDevice_isFile(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL bool StorageDevice_isDirectory(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL bool StorageDevice_isReadable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL bool StorageDevice_isWritable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL Errors StorageDevice_getTmpName(String archiveName, const StorageInfo *storageInfo)
{
  assert(archiveName != NULL);
  assert(!String_isEmpty(archiveName));
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(archiveName);
  UNUSED_VARIABLE(storageInfo);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

LOCAL Errors StorageDevice_create(StorageHandle *storageHandle,
                                  ConstString   fileName,
                                  uint64        fileSize
                                 )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_DEVICE);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);

  // check if file exists
  if (   (storageHandle->storageInfo->jobOptions != NULL)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_APPEND)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_OVERWRITE)
      && StorageDevice_exists(storageHandle->storageInfo,fileName)
     )
  {
    return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(fileName));
  }

  // init variables
  storageHandle->device.fileName = String_new();
  String_set(storageHandle->device.fileName,storageHandle->storageInfo->device.directory);
  File_appendFileName(storageHandle->device.fileName,fileName);

  // open file
  error = File_open(&storageHandle->device.fileHandle,
                    storageHandle->device.fileName,
                    FILE_OPEN_CREATE
                   );
  if (error != ERROR_NONE)
  {
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->device,StorageHandleDevice);

  return ERROR_NONE;
}

LOCAL Errors StorageDevice_open(StorageHandle *storageHandle,
                                ConstString   archiveName
                               )
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_DEVICE);
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

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->device,StorageHandleDevice);
#endif /* 0 */
String_delete(storageHandle->device.fileName);
  return ERROR_FUNCTION_NOT_SUPPORTED;

  return ERROR_NONE;
}

LOCAL void StorageDevice_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_DEVICE);

  DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->device,StorageHandleDevice);

  switch (storageHandle->mode)
  {
    case STORAGE_MODE_READ:
      File_close(&storageHandle->device.fileHandle);
      String_delete(storageHandle->device.fileName);
      break;
    case STORAGE_MODE_WRITE:
      SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        storageHandle->storageInfo->device.totalSize += File_getSize(&storageHandle->device.fileHandle);
      }
      File_close(&storageHandle->device.fileHandle);
      String_delete(storageHandle->device.fileName);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  StringList_append(&storageHandle->storageInfo->device.fileNameList,storageHandle->device.fileName);
}

LOCAL bool StorageDevice_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_DEVICE);

  return File_eof(&storageHandle->device.fileHandle);
}

LOCAL Errors StorageDevice_read(StorageHandle *storageHandle,
                                void          *buffer,
                                ulong         bufferSize,
                                ulong         *bytesRead
                               )
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_DEVICE);
  assert(buffer != NULL);

  if (bytesRead != NULL) (*bytesRead) = 0L;

  return File_read(&storageHandle->device.fileHandle,buffer,bufferSize,bytesRead);
}

LOCAL Errors StorageDevice_write(StorageHandle *storageHandle,
                                 const void    *buffer,
                                 ulong         bufferLength
                                )
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_DEVICE);
  assert(buffer != NULL);

  return File_write(&storageHandle->device.fileHandle,buffer,bufferLength);
}

LOCAL Errors StorageDevice_tell(StorageHandle *storageHandle,
                                uint64        *offset
                               )
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_DEVICE);
  assert(offset != NULL);

  (*offset) = 0LL;

  return File_tell(&storageHandle->device.fileHandle,offset);
}

LOCAL Errors StorageDevice_seek(StorageHandle *storageHandle,
                                uint64        offset
                               )
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_DEVICE);

  return File_seek(&storageHandle->device.fileHandle,offset);
}

LOCAL uint64 StorageDevice_getSize(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_DEVICE);

  return File_getSize(&storageHandle->device.fileHandle);
}

LOCAL Errors StorageDevice_rename(const StorageInfo *storageInfo,
                                  ConstString       fromArchiveName,
                                  ConstString       toArchiveName
                                 )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_FILESYSTEM);

UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(fromArchiveName);
UNUSED_VARIABLE(toArchiveName);
error = ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

LOCAL Errors StorageDevice_delete(const StorageInfo *storageInfo,
                                  ConstString       archiveName
                                 )
{
  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);
  assert(storageInfo->type == STORAGE_TYPE_DEVICE);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

#if 0
still not complete
LOCAL Errors StorageDevice_getInfo(const StorageInfo *storageInfo,
                                   ConstString       fileName,
                                   FileInfo          *fileInfo
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
                                             ConstString                pathName,
                                             const JobOptions           *jobOptions,
                                             ServerConnectionPriorities serverConnectionPriority
                                            )
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(pathName != NULL);
  assert(jobOptions != NULL);

  UNUSED_VARIABLE(storageDirectoryListHandle);
  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(pathName);
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
