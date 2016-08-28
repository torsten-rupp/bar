/***********************************************************************\
*
* $Revision: 4012 $
* $Date: 2015-04-28 19:02:40 +0200 (Tue, 28 Apr 2015) $
* $Author: torsten $
* Contents: storage optical functions
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
#ifdef HAVE_ISO9660
  #include <cdio/cdio.h>
  #include <cdio/iso9660.h>
  #include <cdio/logging.h>
#endif /* HAVE_ISO9660 */
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

#define MAX_CD_SIZE  (900LL*1024LL*1024LL)     // 900M
#define MAX_DVD_SIZE (2LL*4613734LL*1024LL)    // 9G (dual layer)
#define MAX_BD_SIZE  (2LL*25LL*1024LL*1024LL)  // 50G (dual layer)

#define CD_VOLUME_SIZE      (700LL*1024LL*1024LL)
#define CD_VOLUME_ECC_SIZE  (560LL*1024LL*1024LL)
#define DVD_VOLUME_SIZE     (4482LL*1024LL*1024LL)
#define DVD_VOLUME_ECC_SIZE (3600LL*1024LL*1024LL)
#define BD_VOLUME_SIZE      (25LL*1024LL*1024LL*1024LL)
#define BD_VOLUME_ECC_SIZE  (20LL*1024LL*1024LL*1024LL)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifdef HAVE_ISO9660
/***********************************************************************\
* Name   : libcdioLogCallback
* Purpose: callback for libcdio log messages
* Input  : buffer   - buffer for data
*          size     - size of element
*          n        - number of elements
*          userData - user data
* Output : -
* Return : always size*n
* Notes  : -
\***********************************************************************/

LOCAL void libcdioLogCallback(cdio_log_level_t level, const char *message)
{
  UNUSED_VARIABLE(level);

  printInfo(5,"libcdio: %s\n",message);
}
#endif /* HAVE_ISO9660 */

/***********************************************************************\
* Name   : requestNewOpticalMedium
* Purpose: request new cd/dvd/bd medium
* Input  : storageHandle - storage file handle
*          waitFlag          - TRUE to wait for new medium
* Output : -
* Return : TRUE if new medium loaded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors requestNewOpticalMedium(StorageHandle *storageHandle, bool waitFlag)
{
  TextMacro             textMacros[2];
  bool                  mediumRequestedFlag;
  StorageRequestResults storageRequestResult;

  TEXT_MACRO_N_STRING (textMacros[0],"%device",storageHandle->storageSpecifier.deviceName,NULL);
  TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageHandle->requestedVolumeNumber,      NULL);

  if (   (storageHandle->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageHandle->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    // sleep a short time to give hardware time for finishing volume, then unload current volume
    printInfo(1,"Unload medium #%d...",storageHandle->volumeNumber);
    Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
    Misc_executeCommand(String_cString(storageHandle->opticalDisk.write.unloadVolumeCommand),
                        textMacros,SIZE_OF_ARRAY(textMacros),
                        CALLBACK(executeIOOutput,NULL),
                        CALLBACK(executeIOOutput,NULL)
                       );
    printInfo(1,"ok\n");

    storageHandle->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  // request new medium
  mediumRequestedFlag  = FALSE;
  storageRequestResult = STORAGE_REQUEST_VOLUME_UNKNOWN;
  if      (storageHandle->requestVolumeFunction != NULL)
  {
    mediumRequestedFlag = TRUE;

    // request new medium via call back, unload if requested
    do
    {
      storageRequestResult = storageHandle->requestVolumeFunction(storageHandle->requestedVolumeNumber,
                                                                  storageHandle->requestVolumeUserData
                                                                 );
      if (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD)
      {
        // sleep a short time to give hardware time for finishing volume, then unload current medium
        printInfo(1,"Unload medium...");
        Misc_executeCommand(String_cString(storageHandle->opticalDisk.write.unloadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           );
        printInfo(1,"ok\n");
      }
    }
    while (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD);

    storageHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (storageHandle->opticalDisk.write.requestVolumeCommand != NULL)
  {
    mediumRequestedFlag = TRUE;

    // request new volume via external command
    printInfo(1,"Request new medium #%d...",storageHandle->requestedVolumeNumber);
    if (Misc_executeCommand(String_cString(storageHandle->opticalDisk.write.requestVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           ) == ERROR_NONE
       )
    {
      printInfo(1,"ok\n");
      storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
    }
    else
    {
      printInfo(1,"FAIL\n");
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
        mediumRequestedFlag = TRUE;

        printInfo(0,"Please insert medium #%d into drive '%s' and press ENTER to continue\n",storageHandle->requestedVolumeNumber,String_cString(storageHandle->storageSpecifier.deviceName));
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
      else
      {
        printInfo(0,"Please insert medium #%d into drive '%s'\n",storageHandle->requestedVolumeNumber,String_cString(storageHandle->storageSpecifier.deviceName));
      }
    }
    else
    {
      if (waitFlag)
      {
        mediumRequestedFlag = TRUE;

        printInfo(0,"Press ENTER to continue\n");
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
    }

    storageHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (mediumRequestedFlag)
  {
    switch (storageRequestResult)
    {
      case STORAGE_REQUEST_VOLUME_OK:
        // load medium, then sleep a short time to give hardware time for reading medium information
        printInfo(1,"Load medium #%d...",storageHandle->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storageHandle->opticalDisk.write.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           );
        Misc_udelay(LOAD_VOLUME_DELAY_TIME);
        printInfo(1,"ok\n");

        // store new medium number
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

/***********************************************************************\
* Name   : executeIOmkisofs
* Purpose: process mkisofs output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOmkisofs(ConstString line,
                            void        *userData
                           )
{
  StorageHandle *storageHandle = (StorageHandle*)userData;
  String        s;
  double        p;

  assert(storageHandle != NULL);
  assert((storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

//fprintf(stderr,"%s,%d: line=%s\n",__FILE__,__LINE__,String_cString(line));
  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".* ([0-9\\.]+)% done.*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
//fprintf(stderr,"%s,%d: mkisofs: %s\n",__FILE__,__LINE__,String_cString(line));
    p = String_toDouble(s,0,NULL,NULL,0);
    storageHandle->runningInfo.volumeProgress = ((double)storageHandle->opticalDisk.write.step*100.0+p)/(double)(storageHandle->opticalDisk.write.steps*100);
    updateStorageStatusInfo(storageHandle);
  }
  String_delete(s);

  executeIOOutput(line,NULL);
}

/***********************************************************************\
* Name   : executeIODVDisaster
* Purpose: process dvdisaster output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOdvdisaster(ConstString line,
                               void        *userData
                              )
{
  StorageHandle *storageHandle = (StorageHandle*)userData;
  String        s;
  double        p;

  assert(storageHandle != NULL);
  assert((storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*adding space\\): +([0-9\\.]+)%",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    storageHandle->runningInfo.volumeProgress = ((double)(storageHandle->opticalDisk.write.step+0)*100.0+p)/(double)(storageHandle->opticalDisk.write.steps*100);
    updateStorageStatusInfo(storageHandle);
  }
  if (String_matchCString(line,STRING_BEGIN,".*generation: +([0-9\\.]+)%",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    storageHandle->runningInfo.volumeProgress = ((double)(storageHandle->opticalDisk.write.step+1)*100.0+p)/(double)(storageHandle->opticalDisk.write.steps*100);
    updateStorageStatusInfo(storageHandle);
  }
  String_delete(s);

  executeIOOutput(line,NULL);
}

/***********************************************************************\
* Name   : executeIOgrowisofs
* Purpose: process growisofs output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOgrowisofs(ConstString line,
                              void        *userData
                             )
{
  StorageHandle *storageHandle = (StorageHandle*)userData;
  String        s;
  double        p;

  assert(storageHandle != NULL);
  assert((storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".* \\(([0-9\\.]+)%\\) .*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    storageHandle->runningInfo.volumeProgress = ((double)storageHandle->opticalDisk.write.step*100.0+p)/(double)(storageHandle->opticalDisk.write.steps*100);
    updateStorageStatusInfo(storageHandle);
  }
  String_delete(s);

  executeIOOutput(line,NULL);
}

/*---------------------------------------------------------------------*/

LOCAL Errors StorageOptical_initAll(void)
{
  Errors error;

  error = ERROR_NONE;

  #ifdef HAVE_ISO9660
    (void)cdio_log_set_handler(libcdioLogCallback);
  #endif /* HAVE_ISO9660 */

  return error;
}

LOCAL void StorageOptical_doneAll(void)
{
  #ifdef HAVE_ISO9660
    (void)cdio_log_set_handler(NULL);
  #endif /* HAVE_ISO9660 */
}

LOCAL bool StorageOptical_parseSpecifier(ConstString deviceSpecifier,
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

LOCAL bool StorageOptical_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
                                          ConstString            archiveName1,
                                          const StorageSpecifier *storageSpecifier2,
                                          ConstString            archiveName2
                                         )
{
  assert(storageSpecifier1 != NULL);
  assert((storageSpecifier1->type == STORAGE_TYPE_CD) || (storageSpecifier1->type == STORAGE_TYPE_DVD) || (storageSpecifier1->type == STORAGE_TYPE_BD));
  assert(storageSpecifier2 != NULL);
  assert((storageSpecifier2->type == STORAGE_TYPE_CD) || (storageSpecifier2->type == STORAGE_TYPE_DVD) || (storageSpecifier2->type == STORAGE_TYPE_BD));

  if (archiveName1 == NULL) archiveName1 = storageSpecifier1->archiveName;
  if (archiveName2 == NULL) archiveName2 = storageSpecifier2->archiveName;

  return    String_equals(storageSpecifier1->deviceName,storageSpecifier2->deviceName)
         && String_equals(archiveName1,archiveName2);
}

LOCAL String StorageOptical_getName(StorageSpecifier *storageSpecifier,
                                    ConstString      archiveName
                                   )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));

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

  String_clear(storageSpecifier->storageName);
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_CD:
      String_appendCString(storageSpecifier->storageName,"cd://");
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
      break;
    case STORAGE_TYPE_DVD:
      String_appendCString(storageSpecifier->storageName,"dvd://");
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
      break;
    case STORAGE_TYPE_BD:
      String_appendCString(storageSpecifier->storageName,"bd://");
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
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return storageSpecifier->storageName;
}

LOCAL ConstString StorageOptical_getPrintableName(StorageSpecifier *storageSpecifier,
                                                  ConstString      archiveName
                                                 )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));

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

  String_clear(storageSpecifier->storageName);
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_CD:
      String_appendCString(storageSpecifier->storageName,"cd://");
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
      break;
    case STORAGE_TYPE_DVD:
      String_appendCString(storageSpecifier->storageName,"dvd://");
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
      break;
    case STORAGE_TYPE_BD:
      String_appendCString(storageSpecifier->storageName,"bd://");
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
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return storageSpecifier->storageName;
}

LOCAL Errors StorageOptical_init(StorageHandle          *storageHandle,
                                 const StorageSpecifier *storageSpecifier,
                                 const JobOptions       *jobOptions
                                )
{
  Errors         error;
  OpticalDisk    opticalDisk;
  uint64         volumeSize,maxMediumSize;
  FileSystemInfo fileSystemInfo;
  String         sourceFileName,fileBaseName,destinationFileName;

  assert(storageHandle != NULL);
  assert(storageSpecifier != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));

  UNUSED_VARIABLE(storageSpecifier);

  // get device name
  if (String_isEmpty(storageHandle->storageSpecifier.deviceName))
  {
    switch (storageHandle->storageSpecifier.type)
    {
      case STORAGE_TYPE_CD:
        String_set(storageHandle->storageSpecifier.deviceName,globalOptions.cd.deviceName);
        break;
      case STORAGE_TYPE_DVD:
        String_set(storageHandle->storageSpecifier.deviceName,globalOptions.dvd.deviceName);
        break;
      case STORAGE_TYPE_BD:
        String_set(storageHandle->storageSpecifier.deviceName,globalOptions.bd.deviceName);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
         #endif /* NDEBUG */
        break;
    }
  }

  // get cd/dvd/bd settings
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_CD:
      getCDSettings(jobOptions,&opticalDisk);
      break;
    case STORAGE_TYPE_DVD:
      getDVDSettings(jobOptions,&opticalDisk);
      break;
    case STORAGE_TYPE_BD:
      getBDSettings(jobOptions,&opticalDisk);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  /* check space in temporary directory: should be enough to hold
     raw cd/dvd/bd data (volumeSize) and cd/dvd image (4G, single layer)
     including error correction codes (2x)
  */
  error = File_getFileSystemInfo(&fileSystemInfo,tmpDirectory);
  if (error != ERROR_NONE)
  {
    return error;
  }
  volumeSize    = 0LL;
  maxMediumSize = 0LL;
  switch (storageHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_CD:
      volumeSize = CD_VOLUME_SIZE;
      if      ((jobOptions != NULL) && (jobOptions->volumeSize > 0LL)       ) volumeSize = jobOptions->volumeSize;
      else if (globalOptions.cd.volumeSize > 0LL                            ) volumeSize = globalOptions.cd.volumeSize;
      else if ((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag ) volumeSize = CD_VOLUME_ECC_SIZE;
      else                                                                    volumeSize = CD_VOLUME_SIZE;
      maxMediumSize = MAX_CD_SIZE;
      break;
    case STORAGE_TYPE_DVD:
      if      ((jobOptions != NULL) && (jobOptions->volumeSize > 0LL)       ) volumeSize = jobOptions->volumeSize;
      else if (globalOptions.dvd.volumeSize > 0LL                           ) volumeSize = globalOptions.dvd.volumeSize;
      else if ((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag ) volumeSize = DVD_VOLUME_ECC_SIZE;
      else                                                                    volumeSize = DVD_VOLUME_SIZE;
      maxMediumSize = MAX_DVD_SIZE;
      break;
    case STORAGE_TYPE_BD:
      if      ((jobOptions != NULL) && (jobOptions->volumeSize > 0LL)       ) volumeSize = jobOptions->volumeSize;
      else if (globalOptions.bd.volumeSize > 0LL                            ) volumeSize = globalOptions.bd.volumeSize;
      else if ((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag ) volumeSize = BD_VOLUME_ECC_SIZE;
      else                                                                    volumeSize = BD_VOLUME_SIZE;
      maxMediumSize = MAX_BD_SIZE;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  if (fileSystemInfo.freeBytes < (volumeSize+maxMediumSize*(((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag) ? 2 : 1)))
  {
    printWarning("Insufficient space in temporary directory '%s' for medium (%.1lf%s free, %.1lf%s recommended)!\n",
                 String_cString(tmpDirectory),
                 BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                 BYTES_SHORT((volumeSize+maxMediumSize*(((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag ? 2 : 1)))),BYTES_UNIT((volumeSize+maxMediumSize*(((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag ? 2 : 1))))
                );
  }

  storageHandle->opticalDisk.write.requestVolumeCommand   = opticalDisk.requestVolumeCommand;
  storageHandle->opticalDisk.write.unloadVolumeCommand    = opticalDisk.unloadVolumeCommand;
  storageHandle->opticalDisk.write.loadVolumeCommand      = opticalDisk.loadVolumeCommand;
  storageHandle->opticalDisk.write.volumeSize             = volumeSize;
  storageHandle->opticalDisk.write.imagePreProcessCommand = opticalDisk.imagePreProcessCommand;
  storageHandle->opticalDisk.write.imagePostProcessCommand= opticalDisk.imagePostProcessCommand;
  storageHandle->opticalDisk.write.imageCommand           = opticalDisk.imageCommand;
  storageHandle->opticalDisk.write.eccPreProcessCommand   = opticalDisk.eccPreProcessCommand;
  storageHandle->opticalDisk.write.eccPostProcessCommand  = opticalDisk.eccPostProcessCommand;
  storageHandle->opticalDisk.write.eccCommand             = opticalDisk.eccCommand;
  storageHandle->opticalDisk.write.writePreProcessCommand = opticalDisk.writePreProcessCommand;
  storageHandle->opticalDisk.write.writePostProcessCommand= opticalDisk.writePostProcessCommand;
  storageHandle->opticalDisk.write.writeCommand           = opticalDisk.writeCommand;
  storageHandle->opticalDisk.write.writeImageCommand      = opticalDisk.writeImageCommand;
  storageHandle->opticalDisk.write.steps                  = ((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag) ? 4 : 1;
  storageHandle->opticalDisk.write.directory              = String_new();
  storageHandle->opticalDisk.write.step                   = 0;
  if ((jobOptions != NULL) && jobOptions->waitFirstVolumeFlag)
  {
    storageHandle->opticalDisk.write.number        = 0;
    storageHandle->opticalDisk.write.newVolumeFlag = TRUE;
  }
  else
  {
    storageHandle->opticalDisk.write.number        = 1;
    storageHandle->opticalDisk.write.newVolumeFlag = FALSE;
  }
  StringList_init(&storageHandle->opticalDisk.write.fileNameList);
  storageHandle->opticalDisk.write.totalSize              = 0LL;

  // create temporary directory for medium files
  error = File_getTmpDirectoryName(storageHandle->opticalDisk.write.directory,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    StringList_done(&storageHandle->opticalDisk.write.fileNameList);
    String_delete(storageHandle->opticalDisk.write.directory);
    return error;
  }

  if ((jobOptions != NULL) && !jobOptions->noBAROnMediumFlag)
  {
    // store a copy of BAR executable on medium (ignore errors)
    sourceFileName = String_newCString(globalOptions.barExecutable);
    fileBaseName = File_getFileBaseName(String_new(),sourceFileName);
    destinationFileName = File_appendFileName(String_duplicate(storageHandle->opticalDisk.write.directory),fileBaseName);
    File_copy(sourceFileName,destinationFileName);
    StringList_append(&storageHandle->opticalDisk.write.fileNameList,destinationFileName);
    String_delete(destinationFileName);
    String_delete(fileBaseName);
    String_delete(sourceFileName);
  }

  return ERROR_NONE;
}

LOCAL Errors StorageOptical_done(StorageHandle *storageHandle)
{
  Errors error;
  String fileName;
  Errors tmpError;

  assert(storageHandle != NULL);
  assert((storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;

  // delete files
  fileName = String_new();
  while (!StringList_isEmpty(&storageHandle->opticalDisk.write.fileNameList))
  {
    StringList_removeFirst(&storageHandle->opticalDisk.write.fileNameList,fileName);
    tmpError = File_delete(fileName,FALSE);
    if (tmpError != ERROR_NONE)
    {
      if (error == ERROR_NONE) error = tmpError;
    }
  }
  String_delete(fileName);

  // delete temporare directory
  File_delete(storageHandle->opticalDisk.write.directory,FALSE);

  // free resources
  StringList_done(&storageHandle->opticalDisk.write.fileNameList);
  String_delete(storageHandle->opticalDisk.write.directory);

  return error;
}

LOCAL Errors StorageOptical_preProcess(StorageHandle *storageHandle,
                                       ConstString   archiveName,
                                       time_t        timestamp,
                                       bool          initialFlag
                                      )
{
  TextMacro   textMacros[3];
  Errors      error;
  ConstString template;
  String      script;

  assert(storageHandle != NULL);
  assert((storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  UNUSED_VARIABLE(initialFlag);

  error = ERROR_NONE;
  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
  {
    // request next medium
    if (storageHandle->opticalDisk.write.newVolumeFlag)
    {
      storageHandle->opticalDisk.write.number++;
      storageHandle->opticalDisk.write.newVolumeFlag = FALSE;

      storageHandle->requestedVolumeNumber = storageHandle->opticalDisk.write.number;
    }

    // check if new medium is required
    if (storageHandle->volumeNumber != storageHandle->requestedVolumeNumber)
    {
      // request load new medium
      error = requestNewOpticalMedium(storageHandle,FALSE);
    }

    // init macros
    TEXT_MACRO_N_STRING (textMacros[0],"%device",storageHandle->storageSpecifier.deviceName,NULL);
    TEXT_MACRO_N_STRING (textMacros[1],"%file",  archiveName,                               NULL);
    TEXT_MACRO_N_INTEGER(textMacros[2],"%number",storageHandle->requestedVolumeNumber,      NULL);

    // write pre-processing
    template = NULL;
    switch (storageHandle->storageSpecifier.type)
    {
      case STORAGE_TYPE_CD:  template = globalOptions.cd.writePreProcessCommand;  break;
      case STORAGE_TYPE_DVD: template = globalOptions.dvd.writePreProcessCommand; break;
      case STORAGE_TYPE_BD:  template = globalOptions.bd.writePreProcessCommand;  break;
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
    if (template != NULL)
    {
      // get script
      script = expandTemplate(String_cString(template),
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

LOCAL Errors StorageOptical_postProcess(StorageHandle *storageHandle,
                                        ConstString   archiveName,
                                        time_t        timestamp,
                                        bool          finalFlag
                                       )
{
  Errors      error;
  StringList  stderrList;
  String      imageFileName;
  TextMacro   textMacros[6];
  String      fileName;
  FileInfo    fileInfo;
  bool        retryFlag;
  ConstString template;
  String      script;

  assert(storageHandle != NULL);
  assert((storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;

  if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
  {
    if (   (storageHandle->opticalDisk.write.totalSize > storageHandle->opticalDisk.write.volumeSize)
        || (finalFlag && (storageHandle->opticalDisk.write.totalSize > 0LL))
       )
    {
      // medium size limit reached or final medium -> create medium and request new volume

      // init variables
      StringList_init(&stderrList);

      // update info
      storageHandle->runningInfo.volumeProgress = 0.0;
      updateStorageStatusInfo(storageHandle);

      // get temporary image file name
      imageFileName = String_new();
      error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
      if (error != ERROR_NONE)
      {
        StringList_done(&stderrList);
        return error;
      }

      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%device",   storageHandle->storageSpecifier.deviceName,NULL);
      TEXT_MACRO_N_STRING (textMacros[1],"%directory",storageHandle->opticalDisk.write.directory,NULL);
      TEXT_MACRO_N_STRING (textMacros[2],"%image",    imageFileName,                             NULL);
      TEXT_MACRO_N_INTEGER(textMacros[3],"%sectors",  0,                                         NULL);
      TEXT_MACRO_N_STRING (textMacros[4],"%file",     archiveName,                               NULL);
      TEXT_MACRO_N_INTEGER(textMacros[5],"%number",   storageHandle->volumeNumber,               NULL);

      if ((storageHandle->jobOptions != NULL) && (storageHandle->jobOptions->alwaysCreateImageFlag || storageHandle->jobOptions->errorCorrectionCodesFlag))
      {
        // create medium image
        printInfo(1,"Make medium image #%d with %d part(s)...",storageHandle->opticalDisk.write.number,StringList_count(&storageHandle->opticalDisk.write.fileNameList));
        storageHandle->opticalDisk.write.step = 0;
        StringList_clear(&stderrList);
        error = Misc_executeCommand(String_cString(storageHandle->opticalDisk.write.imageCommand),
                                    textMacros,SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOmkisofs,storageHandle),
                                    CALLBACK(executeIOOutput,&stderrList)
                                   );
        if (error != ERROR_NONE)
        {
          printInfo(1,"FAIL\n");
          File_delete(imageFileName,FALSE);
          String_delete(imageFileName);
          StringList_done(&stderrList);
          return error;
        }
        File_getFileInfo(imageFileName,&fileInfo);
        printInfo(1,"ok (%llu bytes)\n",fileInfo.size);

        if (storageHandle->jobOptions->errorCorrectionCodesFlag)
        {
          // add error-correction codes to medium image
          printInfo(1,"Add ECC to image #%d...",storageHandle->opticalDisk.write.number);
          storageHandle->opticalDisk.write.step = 1;
          StringList_clear(&stderrList);
          error = Misc_executeCommand(String_cString(storageHandle->opticalDisk.write.eccCommand),
                                      textMacros,SIZE_OF_ARRAY(textMacros),
                                      CALLBACK(executeIOdvdisaster,storageHandle),
                                      CALLBACK(executeIOOutput,&stderrList)
                                     );
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL\n");
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);
            StringList_done(&stderrList);
            return error;
          }
          File_getFileInfo(imageFileName,&fileInfo);
          printInfo(1,"ok (%llu bytes)\n",fileInfo.size);
        }

        // get number of image sectors
        if (File_getFileInfo(imageFileName,&fileInfo) == ERROR_NONE)
        {
          TEXT_MACRO_N_INTEGER(textMacros[3],"%sectors",(ulong)(fileInfo.size/2048LL),NULL);
        }

        // check if new medium is required
        if (storageHandle->volumeNumber != storageHandle->requestedVolumeNumber)
        {
          // request load new medium
          error = requestNewOpticalMedium(storageHandle,TRUE);
          if (error != ERROR_NONE)
          {
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);
            StringList_done(&stderrList);
            return error;
          }
          updateStorageStatusInfo(storageHandle);
        }

        retryFlag = TRUE;
        do
        {
          retryFlag = FALSE;

          // write image to medium
          printInfo(1,"Write image to medium #%d...",storageHandle->opticalDisk.write.number);
          storageHandle->opticalDisk.write.step = 3;
          StringList_clear(&stderrList);
          error = Misc_executeCommand(String_cString(storageHandle->opticalDisk.write.writeImageCommand),
                                      textMacros,SIZE_OF_ARRAY(textMacros),
                                      CALLBACK(executeIOgrowisofs,storageHandle),
                                      CALLBACK(executeIOOutput,&stderrList)
                                     );
          if (error == ERROR_NONE)
          {
            printInfo(1,"ok\n");
            retryFlag = FALSE;
          }
          else
          {
            printInfo(1,"FAIL\n");
            if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
            {
              retryFlag = Misc_getYesNo("Retry write image to medium?");
            }
          }
        }
        while ((error != ERROR_NONE) && retryFlag);
        if (error != ERROR_NONE)
        {
          File_delete(imageFileName,FALSE);
          String_delete(imageFileName);
          StringList_done(&stderrList);
          return error;
        }
      }
      else
      {
        // check if new medium is required
        if (storageHandle->volumeNumber != storageHandle->requestedVolumeNumber)
        {
          // request load new medium
          error = requestNewOpticalMedium(storageHandle,TRUE);
          if (error != ERROR_NONE)
          {
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);
            StringList_done(&stderrList);
            return error;
          }
          updateStorageStatusInfo(storageHandle);
        }

        retryFlag = TRUE;
        do
        {
          retryFlag = FALSE;

          // write to medium
          printInfo(1,"Write medium #%d with %d part(s)...",storageHandle->opticalDisk.write.number,StringList_count(&storageHandle->opticalDisk.write.fileNameList));
          storageHandle->opticalDisk.write.step = 0;
          StringList_clear(&stderrList);
          error = Misc_executeCommand(String_cString(storageHandle->opticalDisk.write.writeCommand),
                                      textMacros,SIZE_OF_ARRAY(textMacros),
                                      CALLBACK(executeIOgrowisofs,storageHandle),
                                      CALLBACK(executeIOOutput,&stderrList)
                                     );
          if (error == ERROR_NONE)
          {
            printInfo(1,"ok\n");
          }
          else
          {
            printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
            if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
            {
              retryFlag = Misc_getYesNo("Retry write image to medium?");
            }
          }
        }
        while ((error != ERROR_NONE) && retryFlag);
        if (error != ERROR_NONE)
        {
          File_delete(imageFileName,FALSE);
          String_delete(imageFileName);
          StringList_done(&stderrList);
          return error;
        }
      }

      // delete image
      File_delete(imageFileName,FALSE);
      String_delete(imageFileName);

      // update info
      storageHandle->runningInfo.volumeProgress = 1.0;
      updateStorageStatusInfo(storageHandle);

      // delete stored files
      fileName = String_new();
      while (!StringList_isEmpty(&storageHandle->opticalDisk.write.fileNameList))
      {
        StringList_removeFirst(&storageHandle->opticalDisk.write.fileNameList,fileName);
        error = File_delete(fileName,FALSE);
        if (error != ERROR_NONE)
        {
          break;
        }
      }
      String_delete(fileName);
      if (error != ERROR_NONE)
      {
        return error;
      }

      // reset
      storageHandle->opticalDisk.write.newVolumeFlag = TRUE;
      storageHandle->opticalDisk.write.totalSize     = 0;

      // free resources
      StringList_done(&stderrList);
    }

    // write post-processing
    template = NULL;
    switch (storageHandle->storageSpecifier.type)
    {
      case STORAGE_TYPE_CD:  template = globalOptions.cd.writePostProcessCommand;  break;
      case STORAGE_TYPE_DVD: template = globalOptions.dvd.writePostProcessCommand; break;
      case STORAGE_TYPE_BD:  template = globalOptions.bd.writePostProcessCommand;  break;
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
    if (template != NULL)
    {
      // get script
      script = expandTemplate(String_cString(template),
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
    storageHandle->opticalDisk.write.step     = 3;
    storageHandle->runningInfo.volumeProgress = 1.0;
    updateStorageStatusInfo(storageHandle);
  }

  return error;
}

LOCAL Errors StorageOptical_unloadVolume(StorageHandle *storageHandle)
{
  Errors    error;
  TextMacro textMacros[1];

  assert(storageHandle != NULL);
  assert((storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_UNKNOWN;

  TEXT_MACRO_N_STRING(textMacros[0],"%device",storageHandle->storageSpecifier.deviceName,NULL);
  error = Misc_executeCommand(String_cString(storageHandle->opticalDisk.write.unloadVolumeCommand),
                              textMacros,SIZE_OF_ARRAY(textMacros),
                              CALLBACK(executeIOOutput,NULL),
                              CALLBACK(executeIOOutput,NULL)
                             );
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL bool StorageOptical_exists(StorageHandle *storageHandle, ConstString archiveName)
{
  assert(storageHandle != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageHandle);
  UNUSED_VARIABLE(archiveName);

  return FALSE;
}

LOCAL Errors StorageOptical_create(StorageArchiveHandle *storageArchiveHandle,
                                   ConstString   archiveName,
                                   uint64        archiveSize
                                  )
{
  Errors error;
  String directoryName;

  assert(storageArchiveHandle != NULL);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert((storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(archiveSize);

  // init variables
  #ifdef HAVE_ISO9660
    storageArchiveHandle->opticalDisk.read.iso9660Handle     = NULL;
    storageArchiveHandle->opticalDisk.read.iso9660Stat       = NULL;
    storageArchiveHandle->opticalDisk.read.index             = 0LL;
    storageArchiveHandle->opticalDisk.read.buffer.blockIndex = 0LL;
    storageArchiveHandle->opticalDisk.read.buffer.length     = 0L;
  #endif /* HAVE_ISO9660 */
  storageArchiveHandle->opticalDisk.write.fileName               = String_new();

  // create file name
  String_set(storageArchiveHandle->opticalDisk.write.fileName,storageArchiveHandle->storageHandle->opticalDisk.write.directory);
  File_appendFileName(storageArchiveHandle->opticalDisk.write.fileName,archiveName);

  if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
  {
    // create directory if not existing
    directoryName = File_getFilePathName(String_new(),storageArchiveHandle->opticalDisk.write.fileName);
    if (!String_isEmpty(directoryName) && !File_exists(directoryName))
    {
      error = File_makeDirectory(directoryName,
                                 FILE_DEFAULT_USER_ID,
                                 FILE_DEFAULT_GROUP_ID,
                                 FILE_DEFAULT_PERMISSION
                                );
      if (error != ERROR_NONE)
      {
        String_delete(directoryName);
        return error;
      }
    }
    String_delete(directoryName);

    // create file
    error = File_open(&storageArchiveHandle->opticalDisk.write.fileHandle,
                      storageArchiveHandle->opticalDisk.write.fileName,
                      FILE_OPEN_CREATE
                     );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  DEBUG_ADD_RESOURCE_TRACE(&storageArchiveHandle->opticalDisk,sizeof(storageArchiveHandle->opticalDisk));

  return ERROR_NONE;
}

LOCAL Errors StorageOptical_open(StorageArchiveHandle *storageArchiveHandle,
                                 ConstString          archiveName
                                )
{
  assert(storageArchiveHandle != NULL);
  assert((storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(!String_isEmpty(archiveName));

  #ifdef HAVE_ISO9660
    {
      // initialize variables
      storageArchiveHandle->opticalDisk.read.iso9660Handle     = NULL;
      storageArchiveHandle->opticalDisk.read.iso9660Stat       = NULL;
      storageArchiveHandle->opticalDisk.read.index             = 0LL;
      storageArchiveHandle->opticalDisk.read.buffer.blockIndex = 0LL;
      storageArchiveHandle->opticalDisk.read.buffer.length     = 0L;

      storageArchiveHandle->opticalDisk.read.buffer.data = (byte*)malloc(ISO_BLOCKSIZE);
      if (storageArchiveHandle->opticalDisk.read.buffer.data == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }

      // check if device exists
      if (String_isEmpty(storageArchiveHandle->storageHandle->storageSpecifier.deviceName))
      {
        free(storageArchiveHandle->opticalDisk.read.buffer.data);
        return ERROR_NO_DEVICE_NAME;
      }
      if (!File_exists(storageArchiveHandle->storageHandle->storageSpecifier.deviceName))
      {
        free(storageArchiveHandle->opticalDisk.read.buffer.data);
        return ERRORX_(OPTICAL_DISK_NOT_FOUND,0,"%s",String_cString(storageArchiveHandle->storageHandle->storageSpecifier.deviceName));
      }

      // open optical disk/ISO 9660 file
      storageArchiveHandle->opticalDisk.read.iso9660Handle = iso9660_open_ext(String_cString(storageArchiveHandle->storageHandle->storageSpecifier.deviceName),ISO_EXTENSION_ROCK_RIDGE);//ISO_EXTENSION_ALL);
      if (storageArchiveHandle->opticalDisk.read.iso9660Handle == NULL)
      {
        if (File_isFile(storageArchiveHandle->storageHandle->storageSpecifier.deviceName))
        {
          free(storageArchiveHandle->opticalDisk.read.buffer.data);
          return ERRORX_(OPEN_ISO9660_FILE,errno,"%s",String_cString(storageArchiveHandle->storageHandle->storageSpecifier.deviceName));
        }
        else
        {
          free(storageArchiveHandle->opticalDisk.read.buffer.data);
          return ERRORX_(OPEN_OPTICAL_DISK,errno,"%s",String_cString(storageArchiveHandle->storageHandle->storageSpecifier.deviceName));
        }
      }

      // prepare file for reading
      storageArchiveHandle->opticalDisk.read.iso9660Stat = iso9660_ifs_stat_translate(storageArchiveHandle->opticalDisk.read.iso9660Handle,
                                                                               String_cString(archiveName)
                                                                              );
      if (storageArchiveHandle->opticalDisk.read.iso9660Stat == NULL)
      {
        iso9660_close(storageArchiveHandle->opticalDisk.read.iso9660Handle);
        free(storageArchiveHandle->opticalDisk.read.buffer.data);
        return ERRORX_(FILE_NOT_FOUND_,errno,"%s",String_cString(archiveName));
      }

      DEBUG_ADD_RESOURCE_TRACE(&storageArchiveHandle->opticalDisk,sizeof(storageArchiveHandle->opticalDisk));
    }

    return ERROR_NONE;
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_ISO9660 */
}

LOCAL void StorageOptical_close(StorageArchiveHandle *storageArchiveHandle)
{
  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->opticalDisk);
  assert((storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  DEBUG_REMOVE_RESOURCE_TRACE(&storageArchiveHandle->opticalDisk,sizeof(storageArchiveHandle->opticalDisk));

  switch (storageArchiveHandle->mode)
  {
    case STORAGE_MODE_READ:
      #ifdef HAVE_ISO9660
        assert(storageArchiveHandle->opticalDisk.read.iso9660Handle != NULL);
        assert(storageArchiveHandle->opticalDisk.read.iso9660Stat != NULL);

        free(storageArchiveHandle->opticalDisk.read.iso9660Stat);
        iso9660_close(storageArchiveHandle->opticalDisk.read.iso9660Handle);
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_MODE_WRITE:
      if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
      {
        storageArchiveHandle->storageHandle->opticalDisk.write.totalSize += File_getSize(&storageArchiveHandle->opticalDisk.write.fileHandle);
        File_close(&storageArchiveHandle->opticalDisk.write.fileHandle);
      }
      StringList_append(&storageArchiveHandle->storageHandle->opticalDisk.write.fileNameList,storageArchiveHandle->opticalDisk.write.fileName);
      String_delete(storageArchiveHandle->opticalDisk.write.fileName);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

LOCAL bool StorageOptical_eof(StorageArchiveHandle *storageArchiveHandle)
{
  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->opticalDisk);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->mode == STORAGE_MODE_READ);
  assert((storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  #ifdef HAVE_ISO9660
    assert(storageArchiveHandle->opticalDisk.read.iso9660Handle != NULL);
    assert(storageArchiveHandle->opticalDisk.read.iso9660Stat != NULL);

    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      return storageArchiveHandle->opticalDisk.read.index >= storageArchiveHandle->opticalDisk.read.iso9660Stat->size;
    }
    else
    {
      return TRUE;
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageArchiveHandle);

    return TRUE;
  #endif /* HAVE_ISO9660 */
}

LOCAL Errors StorageOptical_read(StorageArchiveHandle *storageArchiveHandle,
                                 void          *buffer,
                                 ulong         size,
                                 ulong         *bytesRead
                                )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->opticalDisk);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->mode == STORAGE_MODE_READ);
  assert((storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(buffer != NULL);

//fprintf(stderr,"%s,%d: size=%lu\n",__FILE__,__LINE__,size);
  if (bytesRead != NULL) (*bytesRead) = 0L;
  error = ERROR_NONE;
  #ifdef HAVE_ISO9660
    {
      uint64   blockIndex;
      uint     blockOffset;
      long int n;
      ulong    bytesAvail;

      assert(storageArchiveHandle->opticalDisk.read.iso9660Handle != NULL);
      assert(storageArchiveHandle->opticalDisk.read.iso9660Stat != NULL);

      if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
      {
        assert(storageArchiveHandle->opticalDisk.read.buffer.data != NULL);

        while (   (size > 0L)
               && (storageArchiveHandle->opticalDisk.read.index < (uint64)storageArchiveHandle->opticalDisk.read.iso9660Stat->size)
              )
        {
          // get ISO9660 block index, offset
          blockIndex  = (int64)(storageArchiveHandle->opticalDisk.read.index/ISO_BLOCKSIZE);
          blockOffset = (uint)(storageArchiveHandle->opticalDisk.read.index%ISO_BLOCKSIZE);

          if (   (blockIndex != storageArchiveHandle->opticalDisk.read.buffer.blockIndex)
              || (blockOffset >= storageArchiveHandle->opticalDisk.read.buffer.length)
             )
          {
            // read ISO9660 block
            n = iso9660_iso_seek_read(storageArchiveHandle->opticalDisk.read.iso9660Handle,
                                      storageArchiveHandle->opticalDisk.read.buffer.data,
                                      storageArchiveHandle->opticalDisk.read.iso9660Stat->lsn+(lsn_t)blockIndex,
                                      1 // read 1 block
                                     );
            if (n < ISO_BLOCKSIZE)
            {
              error = ERROR_(IO_ERROR,errno);
              break;
            }
            storageArchiveHandle->opticalDisk.read.buffer.blockIndex = blockIndex;
            storageArchiveHandle->opticalDisk.read.buffer.length     = (((blockIndex+1)*ISO_BLOCKSIZE) <= (uint64)storageArchiveHandle->opticalDisk.read.iso9660Stat->size)
                                                                  ? ISO_BLOCKSIZE
                                                                  : (ulong)(storageArchiveHandle->opticalDisk.read.iso9660Stat->size%ISO_BLOCKSIZE);
          }

          // copy data
          bytesAvail = MIN(size,storageArchiveHandle->opticalDisk.read.buffer.length-blockOffset);
          memcpy(buffer,storageArchiveHandle->opticalDisk.read.buffer.data+blockOffset,bytesAvail);

          // adjust buffer, size, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          size -= bytesAvail;
          if (bytesRead != NULL) (*bytesRead) += bytesAvail;
          storageArchiveHandle->opticalDisk.read.index += (uint64)bytesAvail;
        }
      }
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(size);
    UNUSED_VARIABLE(bytesRead);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_ISO9660 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageOptical_write(StorageArchiveHandle *storageArchiveHandle,
                                  const void    *buffer,
                                  ulong         size
                                 )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->opticalDisk);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert(storageArchiveHandle->mode == STORAGE_MODE_WRITE);
  assert((storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(buffer != NULL);

  error = ERROR_NONE;
  if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
  {
    error = File_write(&storageArchiveHandle->opticalDisk.write.fileHandle,buffer,size);
  }

  return error;
}

LOCAL Errors StorageOptical_tell(StorageArchiveHandle *storageArchiveHandle,
                                 uint64        *offset
                                )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->opticalDisk);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert((storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  #ifdef HAVE_ISO9660
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      switch (storageArchiveHandle->mode)
      {
        case STORAGE_MODE_READ:
          (*offset) = storageArchiveHandle->opticalDisk.read.index;
          error     = ERROR_NONE;
          break;
        case STORAGE_MODE_WRITE:
          error = File_tell(&storageArchiveHandle->opticalDisk.write.fileHandle,offset);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(offset);
  #endif /* HAVE_ISO9660 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageOptical_seek(StorageArchiveHandle *storageArchiveHandle,
                                 uint64        offset
                                )
{
  Errors error;

  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->opticalDisk);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert((storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;
  #ifdef HAVE_ISO9660
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      switch (storageArchiveHandle->mode)
      {
        case STORAGE_MODE_READ:
          storageArchiveHandle->opticalDisk.read.index = offset;
          error = ERROR_NONE;
          break;
        case STORAGE_MODE_WRITE:
          error = File_seek(&storageArchiveHandle->opticalDisk.write.fileHandle,offset);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageArchiveHandle);
    UNUSED_VARIABLE(offset);
  #endif /* HAVE_ISO9660 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL uint64 StorageOptical_getSize(StorageArchiveHandle *storageArchiveHandle)
{
  uint64 size;

  assert(storageArchiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageArchiveHandle->opticalDisk);
  assert(storageArchiveHandle->storageHandle != NULL);
  assert((storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageArchiveHandle->storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  size = 0LL;
  #ifdef HAVE_ISO9660
    if ((storageArchiveHandle->storageHandle->jobOptions == NULL) || !storageArchiveHandle->storageHandle->jobOptions->dryRunFlag)
    {
      switch (storageArchiveHandle->mode)
      {
        case STORAGE_MODE_READ:
          assert(storageArchiveHandle->opticalDisk.read.iso9660Stat);
          size = (uint64)storageArchiveHandle->opticalDisk.read.iso9660Stat->size;
          break;
        case STORAGE_MODE_WRITE:
          size = File_getSize(&storageArchiveHandle->opticalDisk.write.fileHandle);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageArchiveHandle);
  #endif /* HAVE_ISO9660 */

  return size;
}

LOCAL Errors StorageOptical_delete(StorageHandle *storageHandle,
                                   ConstString   archiveName
                                  )
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert((storageHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageHandle);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

#if 0
still not complete
LOCAL Errors StorageOptical_getFileInfo(StorageHandle *storageHandle,
                                        ConstString   fileName,
                                        FileInfo      *fileInfo
                                       )
{
  String infoFileName;
  Errors error;

  assert(storageHandle != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));
  assert(fileInfo != NULL);

  infoFileName = (fileName != NULL) ? fileName : storageHandle->storageSpecifier.archiveName;
  memset(fileInfo,0,sizeof(fileInfo));

  error = ERROR_FUNCTION_NOT_SUPPORTED;
  assert(error != ERROR_UNKNOWN);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageOptical_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                              const StorageSpecifier     *storageSpecifier,
                                              const JobOptions           *jobOptions,
                                              ServerConnectionPriorities serverConnectionPriority,
                                              ConstString                archiveName
                                             )
{
  AutoFreeList autoFreeList;
  Errors       error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));

  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);

  // initialize variables
  AutoFree_init(&autoFreeList);

  // init variables
  error = ERROR_UNKNOWN;
  switch (storageDirectoryListHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_CD : storageDirectoryListHandle->type = STORAGE_TYPE_CD;  break;
    case STORAGE_TYPE_DVD: storageDirectoryListHandle->type = STORAGE_TYPE_DVD; break;
    case STORAGE_TYPE_BD : storageDirectoryListHandle->type = STORAGE_TYPE_BD;  break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  // open directory listing
  #ifdef HAVE_ISO9660
    // init variables
    storageDirectoryListHandle->opticalDisk.pathName = String_duplicate(archiveName);
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->opticalDisk.pathName,{ String_delete(storageDirectoryListHandle->opticalDisk.pathName); });

    // get device name
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.deviceName))
    {
      switch (storageDirectoryListHandle->storageSpecifier.type)
      {
        case STORAGE_TYPE_CD:
          String_set(storageDirectoryListHandle->storageSpecifier.deviceName,globalOptions.cd.deviceName);
          break;
        case STORAGE_TYPE_DVD:
          String_set(storageDirectoryListHandle->storageSpecifier.deviceName,globalOptions.dvd.deviceName);
          break;
        case STORAGE_TYPE_BD:
          String_set(storageDirectoryListHandle->storageSpecifier.deviceName,globalOptions.bd.deviceName);
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
           #endif /* NDEBUG */
          break;
      }
    }

    // check if device exists
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.deviceName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_DEVICE_NAME;
    }
    if (!File_exists(storageDirectoryListHandle->storageSpecifier.deviceName))
    {
      error = ERRORX_(OPTICAL_DISK_NOT_FOUND,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.deviceName));
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // open optical disk/ISO 9660 file
    storageDirectoryListHandle->opticalDisk.iso9660Handle = iso9660_open_ext(String_cString(storageDirectoryListHandle->storageSpecifier.deviceName),ISO_EXTENSION_ALL);
    if (storageDirectoryListHandle->opticalDisk.iso9660Handle == NULL)
    {
      if (File_isFile(storageDirectoryListHandle->storageSpecifier.deviceName))
      {
        error = ERRORX_(OPEN_ISO9660_FILE,errno,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.deviceName));
      }
      else
      {
        error = ERRORX_(OPEN_OPTICAL_DISK,errno,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.deviceName));
      }
      if (error != ERROR_NONE)
      {
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->opticalDisk.iso9660Handle,{ iso9660_close(storageDirectoryListHandle->opticalDisk.iso9660Handle); });

    // open directory for reading
    storageDirectoryListHandle->opticalDisk.cdioList = iso9660_ifs_readdir(storageDirectoryListHandle->opticalDisk.iso9660Handle,
                                                                           String_cString(archiveName)
                                                                          );
    if (storageDirectoryListHandle->opticalDisk.cdioList == NULL)
    {
      error = ERRORX_(FILE_NOT_FOUND_,errno,"%s",String_cString(archiveName));
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    storageDirectoryListHandle->opticalDisk.cdioNextNode = _cdio_list_begin(storageDirectoryListHandle->opticalDisk.cdioList);
  #else /* not HAVE_ISO9660 */
    // open directory
    error = File_openDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle,
                                   archiveName
                                  );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->opticalDisk.directoryListHandle,{ File_closeDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle); });
  #endif /* HAVE_ISO9660 */

  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

LOCAL void StorageOptical_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert((storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  #ifdef HAVE_ISO9660
    _cdio_list_free(storageDirectoryListHandle->opticalDisk.cdioList,true);
    iso9660_close(storageDirectoryListHandle->opticalDisk.iso9660Handle);
    String_delete(storageDirectoryListHandle->opticalDisk.pathName);
  #else /* not HAVE_ISO9660 */
    File_closeDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle);
  #endif /* HAVE_ISO9660 */
}

LOCAL bool StorageOptical_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  bool endOfDirectoryFlag;

  assert(storageDirectoryListHandle != NULL);
  assert((storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  endOfDirectoryFlag = TRUE;
  #ifdef HAVE_ISO9660
    endOfDirectoryFlag = (storageDirectoryListHandle->opticalDisk.cdioNextNode == NULL);
  #else /* not HAVE_ISO9660 */
    endOfDirectoryFlag = File_endOfDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle);
  #endif /* HAVE_ISO9660 */

  return endOfDirectoryFlag;
}

LOCAL Errors StorageOptical_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                              String                     fileName,
                                              FileInfo                   *fileInfo
                                             )
{
  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert((storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;
  #ifdef HAVE_ISO9660
    {
      iso9660_stat_t *iso9660Stat;
      char           *s;

      if (storageDirectoryListHandle->opticalDisk.cdioNextNode != NULL)
      {
        iso9660Stat = (iso9660_stat_t*)_cdio_list_node_data(storageDirectoryListHandle->opticalDisk.cdioNextNode);
        assert(iso9660Stat != NULL);

        s = (char*)malloc(strlen(iso9660Stat->filename)+1);
        if (s != NULL)
        {
          // Note: enable Joliet extension to avoid conversion to lower case
          iso9660_name_translate_ext(iso9660Stat->filename,s,ISO_EXTENSION_JOLIET_LEVEL1);
          String_set(fileName,storageDirectoryListHandle->opticalDisk.pathName);
          File_appendFileNameCString(fileName,s);
          free(s);

          if (fileInfo != NULL)
          {
            if      (iso9660Stat->type == _STAT_FILE)
            {
              fileInfo->type = FILE_TYPE_FILE;
            }
            else if (iso9660Stat->type == _STAT_DIR)
            {
              fileInfo->type = FILE_TYPE_DIRECTORY;
            }
            else
            {
              fileInfo->type = FILE_TYPE_UNKNOWN;
            }
            fileInfo->size            = iso9660Stat->size;
            fileInfo->timeLastAccess  = (uint64)mktime(&iso9660Stat->tm);
            fileInfo->timeModified    = (uint64)mktime(&iso9660Stat->tm);
            fileInfo->timeLastChanged = 0LL;
            fileInfo->userId          = iso9660Stat->xa.user_id;
            fileInfo->groupId         = iso9660Stat->xa.group_id;
            fileInfo->permission      = iso9660Stat->xa.attributes;
            fileInfo->major           = 0;
            fileInfo->minor           = 0;
            memset(&fileInfo->cast,0,sizeof(FileCast));
          }

          storageDirectoryListHandle->opticalDisk.cdioNextNode = _cdio_list_node_next(storageDirectoryListHandle->opticalDisk.cdioNextNode);
        }
        else
        {
          error = ERROR_INSUFFICIENT_MEMORY;
        }
      }
      else
      {
        error = ERROR_END_OF_DIRECTORY;
      }
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileInfo);

    error = File_readDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle,fileName);
  #endif /* HAVE_ISO9660 */

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
