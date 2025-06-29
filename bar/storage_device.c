/***********************************************************************\
*
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

#include "bar.h"
#include "bar_common.h"
#include "errors.h"
#include "crypt.h"
#include "archive.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
/* file data buffer size */
#define BUFFER_SIZE (64*1024)

#define INITIAL_BUFFER_SIZE   (64*1024)
#define INCREMENT_BUFFER_SIZE ( 8*1024)
#define MAX_BUFFER_SIZE       (64*1024)
#define MAX_FILENAME_LENGTH   ( 8*1024)

#define DEVICE_UNLOAD_VOLUME_DELAY_TIME (10LL*MS_PER_SECOND) /* [ms] */
#define DEVICE_LOAD_VOLUME_DELAY_TIME   (10LL*MS_PER_SECOND) /* [ms] */

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : loadDeviceVolume
* Purpose: load device volume
* Input  : storageInfo - storage info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors loadDeviceVolume(const StorageInfo *storageInfo)
{
  TextMacros (textMacros,2);
  StringList stderrList;
  String     commandLine;
  Errors     error;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;

  const char *deviceName = getDeviceName(&storageInfo->storageSpecifier);
#ifndef NDEBUG
  const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
  if (debugEmulateBlockDevice != NULL)
  {
    deviceName = debugEmulateBlockDevice;
  }
#endif

  // init variables
  StringList_init(&stderrList);
  commandLine = String_new();

  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_CSTRING("%device",deviceName,                      NULL);
    TEXT_MACRO_X_INT    ("%number",storageInfo->volumeRequestNumber,NULL);
  }
  error = Misc_executeCommand(String_cString(storageInfo->device.write.loadVolumeCommand),
                              textMacros.data,
                              textMacros.count,
                              commandLine,
                              CALLBACK_(executeIOOutput,NULL),
                              CALLBACK_(executeIOOutput,NULL)
                             );
  if (error == ERROR_NONE)
  {
    Misc_mdelay(DEVICE_LOAD_VOLUME_DELAY_TIME);

    logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
    logMessage(storageInfo->logHandle,
               LOG_TYPE_ERROR,
               "Loaded medium"
              );
  }
  else
  {
    logMessage(storageInfo->logHandle,
               LOG_TYPE_ERROR,
               "Load medium fail: %s",
               Error_getText(error)
              );
    logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(commandLine));
    logLines(storageInfo->logHandle,
             LOG_TYPE_ERROR,
             "  ",
             &stderrList
            );
  }

  // free resources
  String_delete(commandLine);
  StringList_done(&stderrList);

  return error;
}

/***********************************************************************\
* Name   : unloadDeviceVolume
* Purpose: unload device volume
* Input  : storageInfo - storageInfo
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors unloadDeviceVolume(const StorageInfo *storageInfo)
{
  TextMacros (textMacros,2);
  StringList stderrList;
  String     commandLine;
  Errors     error;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;

  const char *deviceName = getDeviceName(&storageInfo->storageSpecifier);
#ifndef NDEBUG
  const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
  if (debugEmulateBlockDevice != NULL)
  {
    deviceName = debugEmulateBlockDevice;
  }
#endif

  // init variables
  StringList_init(&stderrList);
  commandLine = String_new();

  Misc_mdelay(DEVICE_UNLOAD_VOLUME_DELAY_TIME);

  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_CSTRING("%device",deviceName,                      NULL);
    TEXT_MACRO_X_INT    ("%number",storageInfo->volumeRequestNumber,NULL);
  }
  error = Misc_executeCommand(String_cString(storageInfo->device.write.unloadVolumeCommand),
                              textMacros.data,
                              textMacros.count,
                              commandLine,
                              CALLBACK_(executeIOOutput,NULL),
                              CALLBACK_(executeIOOutput,NULL)
                             );
  if (error == ERROR_NONE)
  {
    logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
    logMessage(storageInfo->logHandle,
               LOG_TYPE_ERROR,
               "Unloaded medium"
              );
  }
  else
  {
    logMessage(storageInfo->logHandle,
               LOG_TYPE_ERROR,
               "Unload medium fail: %s",
               Error_getText(error)
              );
    logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(commandLine));
    logLines(storageInfo->logHandle,
             LOG_TYPE_ERROR,
             "  ",
             &stderrList
            );
  }

  // free resources
  String_delete(commandLine);
  StringList_done(&stderrList);

  return error;
}

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
  if (   (storageInfo->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageInfo->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    // sleep a short time to give hardware time for finishing volume; unload current volume
    printInfo(1,"Unload volume #%d...",storageInfo->volumeNumber);
    (void)unloadDeviceVolume(storageInfo);
    printInfo(1,"OK\n");

    storageInfo->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  // request new volume
  bool                        volumeRequestedFlag        = FALSE;
  StorageVolumeRequestResults storageRequestVolumeResult = STORAGE_VOLUME_REQUEST_RESULT_UNKNOWN;
  if      (storageInfo->volumeRequestFunction != NULL)
  {
    volumeRequestedFlag = TRUE;

    // request new volume via call back, unload if requested
    do
    {
      storageRequestVolumeResult = storageInfo->volumeRequestFunction(STORAGE_REQUEST_VOLUME_TYPE_NEW,
                                                                      storageInfo->volumeRequestNumber,
                                                                      NULL,  // message
                                                                      storageInfo->volumeRequestUserData
                                                                     );
      if (storageRequestVolumeResult == STORAGE_VOLUME_REQUEST_RESULT_UNLOAD)
      {
        // sleep a short time to give hardware time for finishing volume, then unload current medium
        printInfo(1,"Unload volume...");
        (void)unloadDeviceVolume(storageInfo);
        printInfo(1,"OK\n");
      }
    }
    while (storageRequestVolumeResult == STORAGE_VOLUME_REQUEST_RESULT_UNLOAD);

    storageInfo->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (storageInfo->device.write.requestVolumeCommand != NULL)
  {
    volumeRequestedFlag = TRUE;

    TextMacros (textMacros,2);
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_STRING("%device",storageInfo->storageSpecifier.deviceName,NULL);
      TEXT_MACRO_X_INT   ("%number",storageInfo->volumeRequestNumber,        NULL);
    }

    // request new volume via external command
    printInfo(1,"Request new volume #%d...",storageInfo->volumeRequestNumber);
    if (Misc_executeCommand(String_cString(storageInfo->device.write.requestVolumeCommand),
                            textMacros.data,
                            textMacros.count,
                            NULL, // commandLine
                            CALLBACK_(executeIOOutput,NULL),
                            CALLBACK_(executeIOOutput,NULL)
                           ) == ERROR_NONE
       )
    {
      printInfo(1,"OK\n");
      storageRequestVolumeResult = STORAGE_VOLUME_REQUEST_RESULT_LOAD;
    }
    else
    {
      printInfo(1,"FAIL\n");
      storageRequestVolumeResult = STORAGE_VOLUME_REQUEST_RESULT_FAIL;
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

        printInfo(0,
                  "Please insert volume #%d into drive '%s' and press ENTER to continue\n",
                  storageInfo->volumeRequestNumber,
                  storageInfo->storageSpecifier.deviceName
                 );
        Misc_waitEnter();

        storageRequestVolumeResult = STORAGE_VOLUME_REQUEST_RESULT_LOAD;
      }
      else
      {
        printInfo(0,
                  "Please insert volume #%d into drive '%s'\n",
                  storageInfo->volumeRequestNumber,
                  storageInfo->storageSpecifier.deviceName
                 );
      }
    }
    else
    {
      if (waitFlag)
      {
        volumeRequestedFlag = TRUE;

        printInfo(0,"Press ENTER to continue\n");
        Misc_waitEnter();

        storageRequestVolumeResult = STORAGE_VOLUME_REQUEST_RESULT_LOAD;
      }
    }

    storageInfo->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (volumeRequestedFlag)
  {
    switch (storageRequestVolumeResult)
    {
      case STORAGE_VOLUME_REQUEST_RESULT_LOAD:
        // load volume; sleep a short time to give hardware time for reading volume information
        printInfo(1,"Load volume #%d...",storageInfo->volumeRequestNumber);
        (void)loadDeviceVolume(storageInfo);
        printInfo(1,"OK\n");

        // store new volume number
        storageInfo->volumeNumber = storageInfo->volumeRequestNumber;

        // update volume info
        storageInfo->progress.volumeNumber = storageInfo->volumeNumber;
        updateStorageRunningInfo(storageInfo);

        storageInfo->volumeState = STORAGE_VOLUME_STATE_LOADED;
        return ERROR_NONE;
        break;
      case STORAGE_VOLUME_REQUEST_RESULT_ABORTED:
        // load volume
        printInfo(1,"Load volume #%d...",storageInfo->volumeRequestNumber);
        (void)loadDeviceVolume(storageInfo);
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

/***********************************************************************\
* Name   : StorageDevice_getPrintableName
* Purpose: get printable storage name (without password)
* Input  : string           - name variable (can be NULL)
*          storageSpecifier - storage specifier string
*          archiveName      - archive name (can be NULL)
* Output : -
* Return : printable storage name
* Notes  : if archiveName is NULL file name from storageSpecifier is used
\***********************************************************************/

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

/***********************************************************************\
* Name   : StorageDevice_init
* Purpose: init new storage
* Input  : storageInfo                     - storage info variable
*          jobOptions                      - job options or NULL
*          maxBandWidthList                - list with max. band width
*                                            to use [bits/s] or NULL
*          serverConnectionPriority        - server connection priority
* Output : storageInfo - initialized storage info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageDevice_init(StorageInfo      *storageInfo,
                                const JobOptions *jobOptions
                               )
{
  Errors         error;
  Device         device;
  FileSystemInfo fileSystemInfo;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(globalOptions.device != NULL);

  // get device name
  if (String_isEmpty(storageInfo->storageSpecifier.deviceName))
  {
    String_set(storageInfo->storageSpecifier.deviceName,globalOptions.device->name);
  }

  // get device settings
  Configuration_initDeviceSettings(&device,storageInfo->storageSpecifier.deviceName,jobOptions);

  // init variables
  storageInfo->device.write.requestVolumeCommand   = device.requestVolumeCommand;
  storageInfo->device.write.unloadVolumeCommand    = device.unloadVolumeCommand;
  storageInfo->device.write.loadVolumeCommand      = device.loadVolumeCommand;
  storageInfo->device.write.volumeSize             = device.volumeSize;
  storageInfo->device.write.imagePreProcessCommand = device.imagePreProcessCommand;
  storageInfo->device.write.imagePostProcessCommand= device.imagePostProcessCommand;
  storageInfo->device.write.imageCommand           = device.imageCommand;
  storageInfo->device.write.eccPreProcessCommand   = device.eccPreProcessCommand;
  storageInfo->device.write.eccPostProcessCommand  = device.eccPostProcessCommand;
  storageInfo->device.write.eccCommand             = device.eccCommand;
  storageInfo->device.write.writePreProcessCommand = device.writePreProcessCommand;
  storageInfo->device.write.writePostProcessCommand= device.writePostProcessCommand;
  storageInfo->device.write.writeCommand           = device.writeCommand;
  storageInfo->device.write.directory              = String_new();
  if ((jobOptions != NULL) && jobOptions->waitFirstVolumeFlag)
  {
    storageInfo->device.write.number        = 0;
    storageInfo->device.write.newVolumeFlag = TRUE;
  }
  else
  {
    storageInfo->device.write.number        = 1;
    storageInfo->device.write.newVolumeFlag = FALSE;
  }
  StringList_init(&storageInfo->device.write.fileNameList);
  storageInfo->device.write.totalSize              = 0LL;

  // check space in temporary directory: 2x volumeSize
  error = File_getFileSystemInfo(&fileSystemInfo,tmpDirectory);
  if (error != ERROR_NONE)
  {
    StringList_done(&storageInfo->device.write.fileNameList);
    String_delete(storageInfo->device.write.directory);
    Configuration_doneDeviceSettings(&device);
    return error;
  }
  if (fileSystemInfo.freeBytes < (device.volumeSize*2))
  {
    printWarning(_("insufficient space in temporary directory '%s' (%.1lf%s free, %.1lf%s recommended)!"),
                 String_cString(tmpDirectory),
                 BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                 BYTES_SHORT(device.volumeSize*2),BYTES_UNIT(device.volumeSize*2)
                );
  }

  // create temporary directory for device files
  error = File_getTmpDirectoryName(storageInfo->device.write.directory,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    StringList_done(&storageInfo->device.write.fileNameList);
    String_delete(storageInfo->device.write.directory);
    Configuration_doneDeviceSettings(&device);
    return error;
  }

  // request first volume for device
  storageInfo->volumeRequestNumber = 1;

  // free resources
  Configuration_doneDeviceSettings(&device);

  return ERROR_NONE;
}

LOCAL Errors StorageDevice_done(StorageInfo *storageInfo)
{
  Errors error;
  String fileName;
  Errors tmpError;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  error = ERROR_NONE;

  // delete files
  fileName = String_new();
  while (!StringList_isEmpty(&storageInfo->device.write.fileNameList))
  {
    StringList_removeFirst(&storageInfo->device.write.fileNameList,fileName);
    tmpError = File_delete(fileName,FALSE);
    if (tmpError != ERROR_NONE)
    {
      if (error == ERROR_NONE) error = tmpError;
    }
  }
  String_delete(fileName);

  // delete temporare directory
  File_delete(storageInfo->device.write.directory,FALSE);

  // free resources
  StringList_done(&storageInfo->device.write.fileNameList);
  String_delete(storageInfo->device.write.directory);

  return error;
}

LOCAL Errors StorageDevice_preProcess(StorageInfo *storageInfo,
                                      ConstString archiveName,
                                      time_t      timestamp,
                                      bool        initialFlag
                                     )
{
  Errors     error;
  uint       j;
  TextMacros (textMacros,5);

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(globalOptions.device != NULL);

  UNUSED_VARIABLE(initialFlag);

  error = ERROR_NONE;

  // request next volume
  if (storageInfo->device.write.newVolumeFlag)
  {
    storageInfo->device.write.number++;
    storageInfo->device.write.newVolumeFlag = FALSE;

    storageInfo->volumeRequestNumber = storageInfo->device.write.number;
  }

  // check if new volume is required
  if (storageInfo->volumeNumber != storageInfo->volumeRequestNumber)
  {
    error = requestNewDeviceVolume(storageInfo,FALSE);
  }

  // init macros
  j = Thread_getNumberOfCores();
  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING("%device",storageInfo->storageSpecifier.deviceName,NULL);
    TEXT_MACRO_X_STRING("%file",  archiveName,                             NULL);
    TEXT_MACRO_X_INT   ("%number",storageInfo->volumeRequestNumber,        NULL);
    TEXT_MACRO_X_INT   ("%j",     j,                                       NULL);
    TEXT_MACRO_X_INT   ("%j1",    (j > 1) ? j-1 : 1,                       NULL);
  }

  // write pre-processing
  if (!String_isEmpty(globalOptions.device->writePreProcessCommand))
  {
    printInfo(1,"Write pre-processing...");
    error = executeTemplate(String_cString(storageInfo->device.write.writePreProcessCommand),
                            timestamp,
                            textMacros.data,
                            textMacros.count,
                            CALLBACK_(executeIOOutput,NULL)
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
  Errors        error;
  ExecuteIOInfo executeIOInfo;
  String        imageFileName;
  uint          j;
  TextMacros    (textMacros,7);
  String        fileName;
  FileInfo      fileInfo;
  uint          retryCount;
  bool          retryFlag;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(globalOptions.device != NULL);

  error = ERROR_NONE;

  if (storageInfo->device.write.volumeSize == 0LL)
  {
    printWarning(_("device volume size is 0 bytes!"));
  }

  error = ERROR_NONE;

  if (   (storageInfo->device.write.totalSize > storageInfo->device.write.volumeSize)
      || (finalFlag && storageInfo->device.write.totalSize > 0LL)
     )
  {
    // device size limit reached -> write to device volume and request new volume

    // init variables
// TODO:
//    storageInfo->device.write.step = 0;
    executeIOInfo.storageInfo      = storageInfo;
    StringList_init(&executeIOInfo.stderrList);

    // update running info
    storageInfo->progress.volumeDone = 0.0;
    updateStorageRunningInfo(storageInfo);

    // check if new volume is required
    if (storageInfo->volumeNumber != storageInfo->volumeRequestNumber)
    {
      if (error == ERROR_NONE)
      {
        error = requestNewDeviceVolume(storageInfo,TRUE);
        if (error != ERROR_NONE)
        {
          return error;
        }
        updateStorageRunningInfo(storageInfo);
      }
    }

    // get temporary image file name
    imageFileName = String_new();
    error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
    if (error != ERROR_NONE)
    {
      return error;
    }

    // init macros
    j = Thread_getNumberOfCores();
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_STRING ("%device",   storageInfo->storageSpecifier.deviceName,NULL);
      TEXT_MACRO_X_STRING ("%directory",storageInfo->device.write.directory,     NULL);
      TEXT_MACRO_X_STRING ("%image",    imageFileName,                           NULL);
      TEXT_MACRO_X_STRING ("%file",     archiveName,                             NULL);
      TEXT_MACRO_X_INT    ("%number",   storageInfo->volumeNumber,               NULL);
      TEXT_MACRO_X_INT    ("%j",        j,                                       NULL);
      TEXT_MACRO_X_INT    ("%j1",       (j > 1) ? j-1 : 1,                       NULL);
    }

    // create image pre-processing
    if (error == ERROR_NONE)
    {
      if (!String_isEmpty(storageInfo->device.write.imagePreProcessCommand))
      {
        printInfo(1,"Image pre-processing of volume #%u...",storageInfo->volumeNumber);
        String commandLine = String_new();
        error = Misc_executeCommand(String_cString(storageInfo->device.write.imagePreProcessCommand ),
                                    textMacros.data,
                                    textMacros.count,
                                    NULL, // commandLine
                                    CALLBACK_(executeIOOutput,NULL),
                                    CALLBACK_(executeIOOutput,NULL)
                                   );
        if (error == ERROR_NONE)
        {
          printInfo(1,"OK\n");
        }
        else
        {
          printInfo(1,"FAIL\n");
          logMessage(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Image pre-processing of volume #%u fail: %s",
                     storageInfo->volumeNumber,
                     Error_getText(error)
                    );
          logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(commandLine));
          logLines(storageInfo->logHandle,
                   LOG_TYPE_ERROR,
                   "  ",
                   &executeIOInfo.stderrList
                  );
        }
        String_delete(commandLine);
      }
    }

    // create image
    if (error == ERROR_NONE)
    {
      if (!String_isEmpty(storageInfo->device.write.imageCommand))
      {
        printInfo(1,"Create image volume #%u...",storageInfo->volumeNumber);

        String commandLine = String_new();
        error = Misc_executeCommand(String_cString(storageInfo->device.write.imageCommand),
                                    textMacros.data,
                                    textMacros.count,
                                    commandLine,
                                    CALLBACK_(executeIOOutput,NULL),
                                    CALLBACK_(executeIOOutput,NULL)
                                   );
        if (error == ERROR_NONE)
        {
          printInfo(1,"OK\n");

          File_getInfo(&fileInfo,imageFileName);
        }
        else
        {
          printInfo(1,"FAIL\n");

          memClear(&fileInfo,sizeof(fileInfo));

          logMessage(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Create image volume #%u fail: %s",
                     storageInfo->volumeNumber,
                     Error_getText(error)
                    );
          logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(commandLine));
          logLines(storageInfo->logHandle,
                   LOG_TYPE_ERROR,
                   "  ",
                   &executeIOInfo.stderrList
                  );
        }
        String_delete(commandLine);
      }
    }

    // error-correction codes
    if (storageInfo->jobOptions->errorCorrectionCodesFlag)
    {
      // error-correction codes pre-processing
      if (error == ERROR_NONE)
      {
        if (!String_isEmpty(storageInfo->device.write.eccPreProcessCommand))
        {
          printInfo(1,"Add ECC pre-processing to image of volume #%u...",storageInfo->volumeNumber);

          String commandLine = String_new();
          error = Misc_executeCommand(String_cString(storageInfo->device.write.eccPreProcessCommand ),
                                      textMacros.data,
                                      textMacros.count,
                                      commandLine,
                                      CALLBACK_(executeIOOutput,NULL),
                                      CALLBACK_(executeIOOutput,NULL)
                                     );
          if (error == ERROR_NONE)
          {
            printInfo(1,"OK\n");
          }
          else
          {
            printInfo(1,"FAIL\n");
            logMessage(storageInfo->logHandle,
                       LOG_TYPE_ERROR,
                       "Add ECC pre-processing to image of volume #%u fail: %s",
                       storageInfo->volumeNumber,
                       Error_getText(error)
                      );
            logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(commandLine));
            logLines(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "  ",
                     &executeIOInfo.stderrList
                    );
          }
          String_delete(commandLine);
        }
      }

      // add error-correction codes to medium image
      if (error == ERROR_NONE)
      {
        if (!String_isEmpty(storageInfo->device.write.eccCommand))
        {
          printInfo(1,"Add ECC to image of volume #%u...",storageInfo->volumeNumber);

          String commandLine = String_new();
          StringList_clear(&executeIOInfo.stderrList);
          error = Misc_executeCommand(String_cString(storageInfo->device.write.eccCommand),
                                      textMacros.data,
                                      textMacros.count,
                                      commandLine,
                                      CALLBACK_(executeIOdvdisasterStdout,&executeIOInfo),
                                      CALLBACK_(executeIOdvdisasterStderr,&executeIOInfo)
                                     );
          if (error == ERROR_NONE)
          {
            printInfo(1,"OK\n");
          }
          else
          {
            printInfo(1,"FAIL\n");
            logMessage(storageInfo->logHandle,
                       LOG_TYPE_ERROR,
                       "Add ECC to image of volume #%u fail: %s",
                       storageInfo->volumeNumber,
                       Error_getText(error)
                      );
            logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(commandLine));
            logLines(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "  ",
                     &executeIOInfo.stderrList
                    );
          }
          String_delete(commandLine);
        }
      }

      // error-correction codes post-processing
      if (error == ERROR_NONE)
      {
        if (!String_isEmpty(storageInfo->device.write.eccPostProcessCommand))
        {
          printInfo(1,"Add ECC post-processing to image of volume #%u...",storageInfo->volumeNumber);

          String commandLine = String_new();
          error = Misc_executeCommand(String_cString(storageInfo->device.write.eccPostProcessCommand ),
                                      textMacros.data,
                                      textMacros.count,
                                      commandLine,
                                      CALLBACK_(executeIOOutput,NULL),
                                      CALLBACK_(executeIOOutput,NULL)
                                     );
          if (error == ERROR_NONE)
          {
            printInfo(1,"OK\n");
          }
          else
          {
            printInfo(1,"FAIL\n");

            logMessage(storageInfo->logHandle,
                       LOG_TYPE_ERROR,
                       "Add ECC post-processing to image of volume #%u fail: %s",
                       storageInfo->volumeNumber,
                       Error_getText(error)
                      );
            logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(commandLine));
            logLines(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "  ",
                     &executeIOInfo.stderrList
                    );
          }
          String_delete(commandLine);
        }
      }
    }

    // create image post-processing
    if (error == ERROR_NONE)
    {
      if (!String_isEmpty(storageInfo->device.write.imagePostProcessCommand))
      {
        printInfo(1,"Image post-processing of volume #%u...",storageInfo->volumeNumber);

        String commandLine = String_new();
        error = Misc_executeCommand(String_cString(storageInfo->device.write.imagePostProcessCommand),
                                    textMacros.data,
                                    textMacros.count,
                                    commandLine,
                                    CALLBACK_(executeIOOutput,NULL),
                                    CALLBACK_(executeIOOutput,NULL)
                                   );
        if (error == ERROR_NONE)
        {
          printInfo(1,"OK\n");
        }
        else
        {
          printInfo(1,"FAIL\n");
          logMessage(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Image post-processing of volume #%u fail: %s",
                     storageInfo->volumeNumber,
                     Error_getText(error)
                    );
          logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(commandLine));
          logLines(storageInfo->logHandle,
                   LOG_TYPE_ERROR,
                   "  ",
                   &executeIOInfo.stderrList
                  );
        }
        String_delete(commandLine);
      }
    }

    // blank mediuam
    if (storageInfo->jobOptions->blankFlag)
    {
      if (error == ERROR_NONE)
      {
        if (!String_isEmpty(storageInfo->device.write.blankCommand))
        {
          printInfo(1,"Blank volume #%u...",storageInfo->device.write.number);

          String commandLine = String_new();
          StringList_clear(&executeIOInfo.stderrList);
          error = Misc_executeCommand(String_cString(storageInfo->device.write.blankCommand),
                                      textMacros.data,
                                      textMacros.count,
                                      commandLine,
                                      CALLBACK_(executeIOblankStdout,&executeIOInfo),
                                      CALLBACK_(executeIOblankStderr,&executeIOInfo)
                                     );
          if (error == ERROR_NONE)
          {
            printInfo(1,"OK\n");
            logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Blanked volume #%u",storageInfo->volumeNumber);
            logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
            updateVolumeDone(storageInfo,1,0);
            updateStorageRunningInfo(storageInfo);
          }
          else
          {
            printInfo(1,"FAIL\n");

            logMessage(storageInfo->logHandle,
                       LOG_TYPE_ERROR,
                       "Blank volume #%u fail: %s",
                       storageInfo->volumeNumber,
                       Error_getText(error)
                      );
            logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(commandLine));
            logLines(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "  ",
                     &executeIOInfo.stderrList
                    );
          }
          String_delete(commandLine);
        }
      }
    }

    // write to device
    if (error == ERROR_NONE)
    {
      String commandLine = String_new();
      retryCount = 3;
      retryFlag  = TRUE;
      do
      {
        retryFlag = FALSE;

        printInfo(1,"Write device volume #%u...",storageInfo->volumeNumber);

        error = Misc_executeCommand(String_cString(storageInfo->device.write.writeCommand),
                                    textMacros.data,
                                    textMacros.count,
                                    commandLine,
                                    CALLBACK_(executeIOOutput,NULL),
                                    CALLBACK_(executeIOOutput,NULL)
                                   );
        if (error == ERROR_NONE)
        {
          printInfo(1,"OK\n");
          retryFlag = FALSE;
        }
        else
        {
          printInfo(1,"FAIL\n");
          logMessage(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Write image to volume #%u fail: %s",
                     storageInfo->volumeNumber,
                     Error_getText(error)
                    );
          logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(commandLine));
          logLines(storageInfo->logHandle,
                   LOG_TYPE_ERROR,
                   "  ",
                   &executeIOInfo.stderrList
                  );

          retryCount--;
          switch (globalOptions.runMode)
          {
            case RUN_MODE_INTERACTIVE:
              retryFlag = Misc_getYesNo("Retry write image to volume?");
              break;
            case RUN_MODE_BATCH:
            case RUN_MODE_SERVER:
              retryFlag = (requestNewOpticalMedium(storageInfo,Error_getText(error),TRUE) == ERROR_NONE);
              break;
          }
        }
      }
      while ((error != ERROR_NONE) && (retryCount > 0) && retryFlag);
      if (error == ERROR_NONE)
      {
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Written image volume #%u",storageInfo->volumeNumber);
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
        updateVolumeDone(storageInfo,1,0);
        updateStorageRunningInfo(storageInfo);
      }
      String_delete(commandLine);
    }

    // delete image
    File_delete(imageFileName,FALSE);
    String_delete(imageFileName);

    // update running info
    updateVolumeDone(storageInfo,0,100.0);
    updateStorageRunningInfo(storageInfo);

    // delete stored files
    fileName = String_new();
    while (!StringList_isEmpty(&storageInfo->device.write.fileNameList))
    {
      StringList_removeFirst(&storageInfo->device.write.fileNameList,fileName);
      error = File_delete(fileName,FALSE);
      if (error != ERROR_NONE)
      {
        return error;
      }
    }
    String_delete(fileName);

    // handle error
    if (error != ERROR_NONE)
    {
      StringList_done(&executeIOInfo.stderrList);
      return error;
    }

    // reset
    storageInfo->device.write.newVolumeFlag = TRUE;
    storageInfo->device.write.totalSize     = 0;

    // free resources
    StringList_done(&executeIOInfo.stderrList);

    // write post-processing
    if (!String_isEmpty(storageInfo->device.write.writePostProcessCommand))
    {
      // write post-processing
      if (!String_isEmpty(storageInfo->device.write.writePostProcessCommand))
      {
        // get script
        printInfo(1,"Write device post-processing of volume #%u...",storageInfo->volumeNumber);
        error = executeTemplate(String_cString(storageInfo->device.write.writePostProcessCommand),
                                timestamp,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL)
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
    }
  }

  return error;
}

LOCAL bool StorageDevice_exists(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  //TODO: still not implemented
  return FALSE;
}

LOCAL bool StorageDevice_isFile(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  //TODO: still not implemented
  return FALSE;
}

LOCAL bool StorageDevice_isDirectory(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  //TODO: still not implemented
  return FALSE;
}

LOCAL bool StorageDevice_isReadable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  //TODO: still not implemented
  return FALSE;
}

LOCAL bool StorageDevice_isWritable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  //TODO: still not implemented
  return FALSE;
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
                                  uint64        fileSize,
                                  bool          forceFlag
                                 )
{
  Errors error;
  String directoryName;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);


  // check if file exists
  if (   !forceFlag
      && (storageHandle->storageInfo->jobOptions != NULL)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_APPEND)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_OVERWRITE)
      && StorageDevice_exists(storageHandle->storageInfo,fileName)
     )
  {
    return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(fileName));
  }

  // init variables
  storageHandle->device.write.fileName = String_new();

  // create file name
  String_set(storageHandle->device.write.fileName,storageHandle->storageInfo->device.write.directory);
  File_appendFileName(storageHandle->device.write.fileName,fileName);

  // create directory if not existing
  directoryName = File_getDirectoryName(String_new(),storageHandle->device.write.fileName);
  if (!String_isEmpty(directoryName) && !File_exists(directoryName))
  {
    error = File_makeDirectory(directoryName,
                               FILE_DEFAULT_USER_ID,
                               FILE_DEFAULT_GROUP_ID,
                               FILE_DEFAULT_PERMISSIONS,
                               FALSE
                              );
    if (error != ERROR_NONE)
    {
      String_delete(directoryName);
      String_delete(storageHandle->device.write.fileName);
      return error;
    }
  }
  String_delete(directoryName);

  // create file
  error = File_open(&storageHandle->device.write.fileHandle,
                    storageHandle->device.write.fileName,
                    FILE_OPEN_CREATE
                   );
  if (error != ERROR_NONE)
  {
    String_delete(storageHandle->device.write.fileName);
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
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(!String_isEmpty(archiveName));

  // init variables
//TODO: NYI
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
    return error;
  }

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->device,StorageHandleDevice);
#endif /* 0 */

  return ERROR_FUNCTION_NOT_SUPPORTED;

  return ERROR_NONE;
}

LOCAL void StorageDevice_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->device,StorageHandleDevice);

  switch (storageHandle->mode)
  {
    case STORAGE_MODE_READ:
      File_close(&storageHandle->device.write.fileHandle);
      break;
    case STORAGE_MODE_WRITE:
      SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        storageHandle->storageInfo->device.write.totalSize += File_getSize(&storageHandle->device.write.fileHandle);
        StringList_append(&storageHandle->storageInfo->device.write.fileNameList,storageHandle->device.write.fileName);
      }
      File_close(&storageHandle->device.write.fileHandle);
      String_delete(storageHandle->device.write.fileName);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

LOCAL bool StorageDevice_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  return File_eof(&storageHandle->device.write.fileHandle);
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
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(buffer != NULL);

  if (bytesRead != NULL) (*bytesRead) = 0L;

  return File_read(&storageHandle->device.write.fileHandle,buffer,bufferSize,bytesRead);
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
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(buffer != NULL);

  return File_write(&storageHandle->device.write.fileHandle,buffer,bufferLength);
}

LOCAL Errors StorageDevice_tell(StorageHandle *storageHandle,
                                uint64        *offset
                               )
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(offset != NULL);

  (*offset) = 0LL;

  return File_tell(&storageHandle->device.write.fileHandle,offset);
}

LOCAL Errors StorageDevice_seek(StorageHandle *storageHandle,
                                uint64        offset
                               )
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  return File_seek(&storageHandle->device.write.fileHandle,offset);
}

LOCAL uint64 StorageDevice_getSize(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->device);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);

  return File_getSize(&storageHandle->device.write.fileHandle);
}

LOCAL Errors StorageDevice_rename(const StorageInfo *storageInfo,
                                  ConstString       fromArchiveName,
                                  ConstString       toArchiveName
                                 )
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(fromArchiveName);
  UNUSED_VARIABLE(toArchiveName);

  //TODO
#ifndef WERROR
#warning TODO still not implemented
#endif
return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL Errors StorageDevice_makeDirectory(const StorageInfo *storageInfo,
                                         ConstString       directoryName
                                        )
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(directoryName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(directoryName);

// TODO:
#ifndef WERROR
#warning TODO still not implemented
#endif
return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL Errors StorageDevice_delete(const StorageInfo *storageInfo,
                                  ConstString       archiveName
                                 )
{
  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

LOCAL Errors StorageDevice_getFileInfo(FileInfo          *fileInfo,
                                       const StorageInfo *storageInfo,
                                       ConstString       archiveName
                                      )
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_DEVICE);
  assert(fileInfo != NULL);

  UNUSED_VARIABLE(fileInfo);
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

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
