/***********************************************************************\
*
* $Revision: 4012 $
* $Date: 2015-04-28 19:02:40 +0200 (Tue, 28 Apr 2015) $
* $Author: torsten $
* Contents: storage optical functions
* Systems: all
*
\***********************************************************************/

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

#include "common/global.h"
#include "common/autofree.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/files.h"
#include "common/network.h"
#include "semaphore.h"
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

#define OPTICAL_UNLOAD_VOLUME_DELAY_TIME (30LL*MS_PER_SECOND) /* [ms] */
#define OPTICAL_LOAD_VOLUME_DELAY_TIME   (30LL*MS_PER_SECOND) /* [ms] */

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

// execute I/IO info
typedef struct
{
  StorageInfo *storageInfo;
  String      commandLine;
  StringList  stderrList;
} ExecuteIOInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getDeviceName
* Purpose: get device name from storage specifider or global device name
* Input  : storageSpecifier - storage specifier
* Output : -
* Return : device name
* Notes  : -
\***********************************************************************/

#ifdef HAVE_ISO9660
LOCAL ConstString getDeviceName(const StorageSpecifier *storageSpecifier)
{
  ConstString deviceName = NULL;

  if (!String_isEmpty(storageSpecifier->deviceName))
  {
    deviceName = storageSpecifier->deviceName;
  }
  else
  {
    switch (storageSpecifier->type)
    {
      case STORAGE_TYPE_CD:
        deviceName = globalOptions.cd.deviceName;
        break;
      case STORAGE_TYPE_DVD:
        deviceName = globalOptions.dvd.deviceName;
        break;
      case STORAGE_TYPE_BD:
        deviceName = globalOptions.bd.deviceName;
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
         #endif /* NDEBUG */
        break;
    }
  }

  return deviceName;
}
#endif /* HAVE_ISO9660 */

/***********************************************************************\
* Name   : isVolumeLoaded
* Purpose: check if volume is loaded in drive
* Input  : storageSpecifier - storage specifier
* Output : -
* Return : TRUE iff volume loaded
* Notes  : -
\***********************************************************************/

LOCAL bool isVolumeLoaded(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  #ifdef HAVE_ISO9660
    ConstString deviceName = getDeviceName(&storageInfo->storageSpecifier);

    // check if device exists
    if (String_isEmpty(deviceName))
    {
      return FALSE;
    }
    if (!File_exists(deviceName))
    {
      return FALSE;
    }

// TODO: how to check if media is available?
    return FALSE;
  #else /* not HAVE_ISO9660 */
    return TRUE;
  #endif /* HAVE_ISO9660 */
}

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

#ifdef HAVE_ISO9660
LOCAL void libcdioLogCallback(cdio_log_level_t level, const char *message)
{
  UNUSED_VARIABLE(level);

  printInfo(5,"libcdio: %s\n",message);
}
#endif /* HAVE_ISO9660 */

/***********************************************************************\
* Name   : executeIOmkisofsOutput
* Purpose: process mkisofs output
* Input  : storageInfo - storage info
*          line        - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOmkisofsOutput(StorageInfo *storageInfo,
                                  ConstString line
                                 )
{
  String s;
  double p;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);

  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*?([0-9\\.]+)%.*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    updateVolumeDone(storageInfo,0,p);
    updateStorageRunningInfo(storageInfo);
  }
  String_delete(s);
}

/***********************************************************************\
* Name   : executeIOmkisofsStdout
* Purpose: process mkisofs stdout
* Input  : line     - line
*          userData - execute I/O info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOmkisofsStdout(ConstString line,
                                  void        *userData
                                 )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;

  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);

  executeIOmkisofsOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,NULL);
}

/***********************************************************************\
* Name   : executeIOmkisofsStderr
* Purpose: process mkisofs stderr
* Input  : line     - line
*          userData - execute I/O info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOmkisofsStderr(ConstString line,
                                  void        *userData
                                 )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;

  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);

  executeIOmkisofsOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,&executeIOInfo->stderrList);
}

/***********************************************************************\
* Name   : executeIOdvdisasterOutput
* Purpose: process dvdisaster output
* Input  : storageInfo - storage info
*          line        - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOdvdisasterOutput(StorageInfo *storageInfo,
                                     ConstString line
                                    )
{
  String s;
  double p;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);

  s = String_new();
  if      (String_matchCString(line,STRING_BEGIN,".*adding space\\): +([0-9\\.]+)%",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    updateVolumeDone(storageInfo,0, 0+p/2);
    updateStorageRunningInfo(storageInfo);
  }
  else if (String_matchCString(line,STRING_BEGIN,".*generation: +([0-9\\.]+)%",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    updateVolumeDone(storageInfo,0,50+p/2);
    updateStorageRunningInfo(storageInfo);
  }
  String_delete(s);
}

/***********************************************************************\
* Name   : executeIOdvdisasterStdout
* Purpose: process dvdisaster stdout
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOdvdisasterStdout(ConstString line,
                                     void        *userData
                                    )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;

  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);

  executeIOdvdisasterOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,NULL);
}

/***********************************************************************\
* Name   : executeIOdvdisasterStderr
* Purpose: process dvdisaster stderr
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOdvdisasterStderr(ConstString line,
                                     void        *userData
                                    )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;

  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);

  executeIOdvdisasterOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,&executeIOInfo->stderrList);
}

/***********************************************************************\
* Name   : executeIOblankOutput
* Purpose: process blank output
* Input  : storageInfo - storage info
*          line        - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOblankOutput(StorageInfo *storageInfo,
                                ConstString line
                               )
{
  String s;
  double p;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);

  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*?([0-9\\.]+)%.*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    updateVolumeDone(storageInfo,0,p);
    updateStorageRunningInfo(storageInfo);
  }
  String_delete(s);
}

/***********************************************************************\
* Name   : executeIOblankStdout
* Purpose: process blank stdout output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOblankStdout(ConstString line,
                                void        *userData
                               )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;

  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);

  executeIOblankOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,NULL);
}

/***********************************************************************\
* Name   : executeIOblankStderr
* Purpose: process blank stderr output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOblankStderr(ConstString line,
                                void        *userData
                               )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;

  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);

  executeIOblankOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,&executeIOInfo->stderrList);
}

/***********************************************************************\
* Name   : executeIOgrowisofsOutput
* Purpose: process growisofs output
* Input  : storageInfo - storage info
*          line        - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOgrowisofsOutput(StorageInfo *storageInfo,
                                    ConstString line
                                   )
{
  String s;
  double p;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);

  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*?([0-9\\.]+)%.*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    updateVolumeDone(storageInfo,0,p);
    updateStorageRunningInfo(storageInfo);
  }
  String_delete(s);
}

/***********************************************************************\
* Name   : executeIOgrowisofsStdout
* Purpose: process growisofs stdout output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOgrowisofsStdout(ConstString line,
                                    void        *userData
                                   )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;

  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);

  executeIOgrowisofsOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,NULL);
}

/***********************************************************************\
* Name   : executeIOgrowisofsStderr
* Purpose: process growisofs stderr output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOgrowisofsStderr(ConstString line,
                                    void        *userData
                                   )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;

  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);

  executeIOgrowisofsOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,&executeIOInfo->stderrList);
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

LOCAL String StorageOptical_getName(String                 string,
                                    const StorageSpecifier *storageSpecifier,
                                    ConstString            archiveName
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

  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_CD:
      String_appendCString(string,"cd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(string,storageSpecifier->deviceName);
        String_appendChar(string,':');
      }
      if (!String_isEmpty(storageFileName))
      {
        String_append(string,storageFileName);
      }
      break;
    case STORAGE_TYPE_DVD:
      String_appendCString(string,"dvd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(string,storageSpecifier->deviceName);
        String_appendChar(string,':');
      }
      if (!String_isEmpty(storageFileName))
      {
        String_append(string,storageFileName);
      }
      break;
    case STORAGE_TYPE_BD:
      String_appendCString(string,"bd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(string,storageSpecifier->deviceName);
        String_appendChar(string,':');
      }
      if (!String_isEmpty(storageFileName))
      {
        String_append(string,storageFileName);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return string;
}

LOCAL void StorageOptical_getPrintableName(String                 string,
                                           const StorageSpecifier *storageSpecifier,
                                           ConstString            archiveName
                                          )
{
  ConstString storageFileName;

  assert(string != NULL);
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

  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_CD:
      String_appendCString(string,"cd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(string,storageSpecifier->deviceName);
        String_appendChar(string,':');
      }
      if (!String_isEmpty(storageFileName))
      {
        String_append(string,storageFileName);
      }
      break;
    case STORAGE_TYPE_DVD:
      String_appendCString(string,"dvd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(string,storageSpecifier->deviceName);
        String_appendChar(string,':');
      }
      if (!String_isEmpty(storageFileName))
      {
        String_append(string,storageFileName);
      }
      break;
    case STORAGE_TYPE_BD:
      String_appendCString(string,"bd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(string,storageSpecifier->deviceName);
        String_appendChar(string,':');
      }
      if (!String_isEmpty(storageFileName))
      {
        String_append(string,storageFileName);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
}

LOCAL Errors StorageOptical_init(StorageInfo            *storageInfo,
                                 const StorageSpecifier *storageSpecifier,
                                 const JobOptions       *jobOptions
                                )
{
  Errors         error;
  OpticalDisk    opticalDisk;
  uint64         volumeSize,maxMediumSize;
  FileSystemInfo fileSystemInfo;
  String         fileBaseName,destinationFileName;

  assert(storageInfo != NULL);
  assert(storageSpecifier != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));

  UNUSED_VARIABLE(storageSpecifier);

  // get device name
  if (String_isEmpty(storageInfo->storageSpecifier.deviceName))
  {
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_CD:
        String_set(storageInfo->storageSpecifier.deviceName,globalOptions.cd.deviceName);
        break;
      case STORAGE_TYPE_DVD:
        String_set(storageInfo->storageSpecifier.deviceName,globalOptions.dvd.deviceName);
        break;
      case STORAGE_TYPE_BD:
        String_set(storageInfo->storageSpecifier.deviceName,globalOptions.bd.deviceName);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
         #endif /* NDEBUG */
        break;
    }
  }

  // get cd/dvd/bd settings
  switch (storageInfo->storageSpecifier.type)
  {
    case STORAGE_TYPE_CD:
      Configuration_initCDSettings(&opticalDisk,jobOptions);
      break;
    case STORAGE_TYPE_DVD:
      Configuration_initDVDSettings(&opticalDisk,jobOptions);
      break;
    case STORAGE_TYPE_BD:
      Configuration_initBDSettings(&opticalDisk,jobOptions);
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
    Configuration_doneOpticalDiskSettings(&opticalDisk);
    return error;
  }
  volumeSize    = 0LL;
  maxMediumSize = 0LL;
  switch (storageInfo->storageSpecifier.type)
  {
    case STORAGE_TYPE_CD:
      volumeSize = CD_VOLUME_SIZE;
      if      ((jobOptions != NULL) && (jobOptions->opticalDisk.volumeSize > 0LL)) volumeSize = jobOptions->opticalDisk.volumeSize;
      else if ((jobOptions != NULL) && (jobOptions->volumeSize > 0LL)            ) volumeSize = jobOptions->volumeSize;
      else if (globalOptions.cd.volumeSize > 0LL                                 ) volumeSize = globalOptions.cd.volumeSize;
      else if ((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag      ) volumeSize = CD_VOLUME_ECC_SIZE;
      else                                                                         volumeSize = CD_VOLUME_SIZE;
      maxMediumSize = MAX_CD_SIZE;
      break;
    case STORAGE_TYPE_DVD:
      if      ((jobOptions != NULL) && (jobOptions->opticalDisk.volumeSize > 0LL)) volumeSize = jobOptions->opticalDisk.volumeSize;
      else if ((jobOptions != NULL) && (jobOptions->volumeSize > 0LL)            ) volumeSize = jobOptions->volumeSize;
      else if (globalOptions.dvd.volumeSize > 0LL                                ) volumeSize = globalOptions.dvd.volumeSize;
      else if ((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag      ) volumeSize = DVD_VOLUME_ECC_SIZE;
      else                                                                         volumeSize = DVD_VOLUME_SIZE;
      maxMediumSize = MAX_DVD_SIZE;
      break;
    case STORAGE_TYPE_BD:
      if      ((jobOptions != NULL) && (jobOptions->opticalDisk.volumeSize > 0LL)) volumeSize = jobOptions->opticalDisk.volumeSize;
      else if ((jobOptions != NULL) && (jobOptions->volumeSize > 0LL)            ) volumeSize = jobOptions->volumeSize;
      else if (globalOptions.bd.volumeSize > 0LL                                 ) volumeSize = globalOptions.bd.volumeSize;
      else if ((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag      ) volumeSize = BD_VOLUME_ECC_SIZE;
      else                                                                         volumeSize = BD_VOLUME_SIZE;
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
    printWarning("insufficient space in temporary directory '%s' for medium (%.1lf%s free, %.1lf%s recommended)!",
                 String_cString(tmpDirectory),
                 BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                 BYTES_SHORT((volumeSize+maxMediumSize*(((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag ? 2 : 1)))),BYTES_UNIT((volumeSize+maxMediumSize*(((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag ? 2 : 1))))
                );
  }

  storageInfo->opticalDisk.write.requestVolumeCommand    = String_duplicate(opticalDisk.requestVolumeCommand);
  storageInfo->opticalDisk.write.unloadVolumeCommand     = String_duplicate(opticalDisk.unloadVolumeCommand);
  storageInfo->opticalDisk.write.loadVolumeCommand       = String_duplicate(opticalDisk.loadVolumeCommand);
  storageInfo->opticalDisk.write.volumeSize              = volumeSize;
  storageInfo->opticalDisk.write.imagePreProcessCommand  = String_duplicate(opticalDisk.imagePreProcessCommand);
  storageInfo->opticalDisk.write.imagePostProcessCommand = String_duplicate(opticalDisk.imagePostProcessCommand);
  storageInfo->opticalDisk.write.imageCommand            = String_duplicate(opticalDisk.imageCommand);
  storageInfo->opticalDisk.write.eccPreProcessCommand    = String_duplicate(opticalDisk.eccPreProcessCommand);
  storageInfo->opticalDisk.write.eccPostProcessCommand   = String_duplicate(opticalDisk.eccPostProcessCommand);
  storageInfo->opticalDisk.write.eccCommand              = String_duplicate(opticalDisk.eccCommand);
  storageInfo->opticalDisk.write.blankCommand            = String_duplicate(opticalDisk.blankCommand);
  storageInfo->opticalDisk.write.writePreProcessCommand  = String_duplicate(opticalDisk.writePreProcessCommand);
  storageInfo->opticalDisk.write.writePostProcessCommand = String_duplicate(opticalDisk.writePostProcessCommand);
  storageInfo->opticalDisk.write.writeCommand            = String_duplicate(opticalDisk.writeCommand);
  storageInfo->opticalDisk.write.writeImageCommand       = String_duplicate(opticalDisk.writeImageCommand);
  storageInfo->opticalDisk.write.steps                   = 0;
  if ((jobOptions != NULL) && jobOptions->blankFlag) storageInfo->opticalDisk.write.steps += 1;  // blank
  if (   (jobOptions != NULL)
      && (   jobOptions->alwaysCreateImageFlag
          || jobOptions->errorCorrectionCodesFlag
         )
     )
  {
    storageInfo->opticalDisk.write.steps += 3;  // pre/create/post image
    if (jobOptions->errorCorrectionCodesFlag) storageInfo->opticalDisk.write.steps += 3;  // pre/add/post ECC
  }
  storageInfo->opticalDisk.write.steps += 1;  // write
  storageInfo->opticalDisk.write.steps += 1;  // verify
  storageInfo->opticalDisk.write.directory               = String_new();
  storageInfo->opticalDisk.write.step                    = 0;
  if ((jobOptions != NULL) && jobOptions->waitFirstVolumeFlag)
  {
    storageInfo->opticalDisk.write.number        = 0;
    storageInfo->opticalDisk.write.newVolumeFlag = TRUE;
  }
  else
  {
    storageInfo->opticalDisk.write.number        = 1;
    storageInfo->opticalDisk.write.newVolumeFlag = FALSE;
  }
  StringList_init(&storageInfo->opticalDisk.write.fileNameList);
  storageInfo->opticalDisk.write.totalSize              = 0LL;

  // create temporary directory for medium files
  error = File_getTmpDirectoryName(storageInfo->opticalDisk.write.directory,NULL,tmpDirectory);
  if (error != ERROR_NONE)
  {
    StringList_done(&storageInfo->opticalDisk.write.fileNameList);
    String_delete(storageInfo->opticalDisk.write.directory);
    Configuration_doneOpticalDiskSettings(&opticalDisk);
    return error;
  }

  if ((jobOptions != NULL) && !jobOptions->noBAROnMediumFlag)
  {
    // store a copy of BAR executable on medium (ignore errors)
    fileBaseName = File_getBaseName(String_new(),globalOptions.barExecutable,TRUE);
    destinationFileName = File_appendFileName(String_duplicate(storageInfo->opticalDisk.write.directory),fileBaseName);
    error = File_copy(globalOptions.barExecutable,destinationFileName);
    if (error == ERROR_NONE)
    {
      StringList_append(&storageInfo->opticalDisk.write.fileNameList,destinationFileName);
    }
    String_delete(destinationFileName);
    String_delete(fileBaseName);
  }

  // free resources
  Configuration_doneOpticalDiskSettings(&opticalDisk);

  return ERROR_NONE;
}

LOCAL Errors StorageOptical_done(StorageInfo *storageInfo)
{
  Errors error;
  String fileName;
  Errors tmpError;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;

  // delete files
  fileName = String_new();
  while (!StringList_isEmpty(&storageInfo->opticalDisk.write.fileNameList))
  {
    StringList_removeFirst(&storageInfo->opticalDisk.write.fileNameList,fileName);
    tmpError = File_delete(fileName,FALSE);
    if (tmpError != ERROR_NONE)
    {
      if (error == ERROR_NONE) error = tmpError;
    }
  }
  String_delete(fileName);

  // delete temporare directory
  File_delete(storageInfo->opticalDisk.write.directory,FALSE);

  // free resources
  StringList_done(&storageInfo->opticalDisk.write.fileNameList);
  String_delete(storageInfo->opticalDisk.write.directory);
  String_delete(storageInfo->opticalDisk.write.writeImageCommand);
  String_delete(storageInfo->opticalDisk.write.writeCommand);
  String_delete(storageInfo->opticalDisk.write.writePostProcessCommand);
  String_delete(storageInfo->opticalDisk.write.writePreProcessCommand);
  String_delete(storageInfo->opticalDisk.write.blankCommand);
  String_delete(storageInfo->opticalDisk.write.eccCommand);
  String_delete(storageInfo->opticalDisk.write.eccPostProcessCommand);
  String_delete(storageInfo->opticalDisk.write.eccPreProcessCommand);
  String_delete(storageInfo->opticalDisk.write.imageCommand);
  String_delete(storageInfo->opticalDisk.write.imagePostProcessCommand);
  String_delete(storageInfo->opticalDisk.write.imagePreProcessCommand);
  String_delete(storageInfo->opticalDisk.write.loadVolumeCommand);
  String_delete(storageInfo->opticalDisk.write.unloadVolumeCommand);
  String_delete(storageInfo->opticalDisk.write.requestVolumeCommand);

  return error;
}

/***********************************************************************\
* Name   : loadVolume
* Purpose: load volume
* Input  : storageInfo - storage info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors loadVolume(const StorageInfo *storageInfo)
{
  TextMacros (textMacros,2);
  StringList stderrList;
  String     commandLine;
  Errors     error;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;

  if (!isVolumeLoaded(storageInfo))
  {
    // init variables
    StringList_init(&stderrList);
    commandLine = String_new();

    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_STRING("%device",storageInfo->storageSpecifier.deviceName,NULL);
      TEXT_MACRO_X_INT   ("%number",storageInfo->volumeRequestNumber,        NULL);
    }
    error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.loadVolumeCommand),
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
  }

  return error;
}

LOCAL Errors StorageOptical_unloadVolume(const StorageInfo *storageInfo)
{
  TextMacros (textMacros,2);
  StringList stderrList;
  String     commandLine;
  Errors     error;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  // init variables
  StringList_init(&stderrList);
  commandLine = String_new();

  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING("%device",storageInfo->storageSpecifier.deviceName,NULL);
    TEXT_MACRO_X_INT   ("%number",storageInfo->volumeRequestNumber,        NULL);
  }
  error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.unloadVolumeCommand),
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
* Name   : requestNewOpticalMedium
* Purpose: request new cd/dvd/bd medium
* Input  : storageInfo - storage info
           message     - message to show (or NULL)
*          waitFlag    - TRUE to wait for new medium
* Output : -
* Return : TRUE if new medium loaded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors requestNewOpticalMedium(StorageInfo *storageInfo,
                                     const char  *message,
                                     bool        waitFlag
                                    )
{
  TextMacros                  (textMacros,2);
  bool                        mediumRequestedFlag;
  StorageVolumeRequestResults storageRequestVolumeResult;

  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING("%device",storageInfo->storageSpecifier.deviceName,NULL);
    TEXT_MACRO_X_INT   ("%number",storageInfo->volumeRequestNumber,        NULL);
  }

  if (   (storageInfo->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageInfo->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    // sleep a short time to give hardware time for finishing volume, then unload current volume
    printInfo(1,"Unload medium #%d...",storageInfo->volumeNumber);
    Misc_mdelay(OPTICAL_UNLOAD_VOLUME_DELAY_TIME);
    (void)StorageOptical_unloadVolume(storageInfo);
    printInfo(1,"OK\n");

    storageInfo->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  // request new medium
  mediumRequestedFlag  = FALSE;
  storageRequestVolumeResult = STORAGE_VOLUME_REQUEST_RESULT_UNKNOWN;
  if      (storageInfo->volumeRequestFunction != NULL)
  {
    // request volume via callback
    mediumRequestedFlag = TRUE;

    // request new medium via call back, unload if requested
    do
    {
      storageRequestVolumeResult = storageInfo->volumeRequestFunction(STORAGE_REQUEST_VOLUME_TYPE_NEW,
                                                                      storageInfo->volumeRequestNumber,
                                                                      message,
                                                                      storageInfo->volumeRequestUserData
                                                                     );
      if (storageRequestVolumeResult == STORAGE_VOLUME_REQUEST_RESULT_UNLOAD)
      {
        // sleep a short time to give hardware time for finishing volume, then unload current medium
        printInfo(1,"Unload medium...");
//TODO: check if medium is ready
        Misc_mdelay(OPTICAL_UNLOAD_VOLUME_DELAY_TIME);
        (void)StorageOptical_unloadVolume(storageInfo);
        printInfo(1,"OK\n");
      }
    }
    while (storageRequestVolumeResult == STORAGE_VOLUME_REQUEST_RESULT_UNLOAD);

    storageInfo->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (storageInfo->opticalDisk.write.requestVolumeCommand != NULL)
  {
    // request volume via external command
    mediumRequestedFlag = TRUE;

    // request new volume via external command
    printInfo(1,"Request new medium #%d...",storageInfo->volumeRequestNumber);
    if (Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.requestVolumeCommand),
                            textMacros.data,
                            textMacros.count,
                            NULL, // commandLine
                            CALLBACK_(executeIOOutput,NULL),
                            CALLBACK_(executeIOOutput,NULL)
                           ) == ERROR_NONE
       )
    {
      printInfo(1,"OK\n");
      storageRequestVolumeResult = STORAGE_VOLUME_REQUEST_RESULT_OK;
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
    // request volume via console
    if (storageInfo->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
    {
      if (waitFlag)
      {
        mediumRequestedFlag = TRUE;

        printInfo(0,
                  "Please insert medium #%d into drive '%s' and press ENTER to continue\n",
                  storageInfo->volumeRequestNumber,
                  String_cString(storageInfo->storageSpecifier.deviceName)
                 );
        Misc_waitEnter();

        storageRequestVolumeResult = STORAGE_VOLUME_REQUEST_RESULT_OK;
      }
      else
      {
        printInfo(0,
                  "Please insert medium #%d into drive '%s'\n",
                  storageInfo->volumeRequestNumber,
                  String_cString(storageInfo->storageSpecifier.deviceName)
                 );
      }
    }
    else
    {
      if (waitFlag)
      {
        mediumRequestedFlag = TRUE;

        printInfo(0,"Press ENTER to continue\n");
        Misc_waitEnter();

        storageRequestVolumeResult = STORAGE_VOLUME_REQUEST_RESULT_OK;
      }
    }

    storageInfo->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (mediumRequestedFlag)
  {
    switch (storageRequestVolumeResult)
    {
      case STORAGE_VOLUME_REQUEST_RESULT_OK:
        // load medium, then sleep a short time to give hardware time for reading medium information
        printInfo(1,"Load medium #%d...",storageInfo->volumeRequestNumber);
        (void)loadVolume(storageInfo);
        Misc_mdelay(OPTICAL_LOAD_VOLUME_DELAY_TIME);
        printInfo(1,"OK\n");

        // store new medium number
        storageInfo->volumeNumber = storageInfo->volumeRequestNumber;

        // update running info
        storageInfo->progress.volumeNumber = storageInfo->volumeNumber;
        updateStorageRunningInfo(storageInfo);

        storageInfo->volumeState = STORAGE_VOLUME_STATE_LOADED;
        return ERROR_NONE;
        break;
      case STORAGE_VOLUME_REQUEST_RESULT_ABORTED:
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

/***********************************************************************\
* Name   : verifyVolume
* Purpose: verify content of cd/dvd/bd medium
* Input  : storageInfo - storage info
* Output : -
* Return : TRUE if medium verified, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors verifyVolume(StorageInfo *storageInfo)
{
  Errors error;

  #ifdef HAVE_ISO9660
    // allocate buffers
    byte *buffer0 = (byte*)malloc(ISO_BLOCKSIZE);
    if (buffer0 == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    byte *buffer1 = (byte*)malloc(ISO_BLOCKSIZE);
    if (buffer1 == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    error = ERROR_NONE;

    // get file names
    StringList fileNameList;
    StringList_init(&fileNameList);
    if (error == ERROR_NONE)
    {
      DirectoryListHandle directoryListHandle;
      error = File_openDirectoryList(&directoryListHandle,
                                     storageInfo->opticalDisk.write.directory
                                    );
      if (error == ERROR_NONE)
      {
        String fileName = String_new();
        while (!File_endOfDirectoryList(&directoryListHandle) && (error == ERROR_NONE))
        {
          error = File_readDirectoryList(&directoryListHandle,fileName);
          if (error == ERROR_NONE)
          {
            StringList_append(&fileNameList,fileName);
          }
        }
        String_delete(fileName);

        File_closeDirectoryList(&directoryListHandle);
      }
    }

    // compare files
    if (error == ERROR_NONE)
    {
      ConstString deviceName = getDeviceName(&storageInfo->storageSpecifier);
      TimeoutInfo timeoutInfo;
      iso9660_t   *iso9660Handle;
      Misc_initTimeout(&timeoutInfo,60*MS_PER_SECOND);
      do
      {
        iso9660Handle = iso9660_open_ext(String_cString(deviceName),
                                         ISO_EXTENSION_ALL
                                        );
        if (iso9660Handle == NULL) Misc_mdelay(5*MS_PER_SECOND);
      }
      while ((iso9660Handle == NULL) && !Misc_isTimeout(&timeoutInfo));
      Misc_doneTimeout(&timeoutInfo);
      if (iso9660Handle != NULL)
      {
        StringListIterator stringListIterator;
        String             fileName = String_new();
        uint               n = 0;
        STRINGLIST_ITERATEX(&fileNameList,stringListIterator,fileName,error == ERROR_NONE)
        {
          // open file
          FileHandle fileHandle;
          if (error == ERROR_NONE)
          {
            error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
            if (error != ERROR_NONE) break;
          }

          // open file on optical medium
          iso9660_stat_t *iso9660Stat;
          if (error == ERROR_NONE)
          {
            File_getBaseName(fileName,fileName,TRUE);
            iso9660Stat = iso9660_ifs_stat_translate(iso9660Handle,
                                                     String_cString(fileName)
                                                    );
            if (iso9660Stat == NULL)
            {
              error = ERRORX_(FILE_NOT_FOUND_,errno,"%s",String_cString(fileName));
              File_close(&fileHandle);
              break;
            }
          }

          // read and compare content
          size_t blockIndex = 0;
          while ((error == ERROR_NONE) && !File_eof(&fileHandle))
          {
            ulong readBytes;
            error = File_read(&fileHandle,buffer0,ISO_BLOCKSIZE,&readBytes);
            if (error == ERROR_NONE)
            {
              long int n = iso9660_iso_seek_read(iso9660Handle,
                                                 buffer1,
                                                 iso9660Stat->lsn+(lsn_t)blockIndex,
                                                 1 // read 1 block
                                                );
              if (n == ISO_BLOCKSIZE)
              {
                if (memcmp(buffer0,buffer1,readBytes) != 0)
                {
                  error = ERRORX_(ENTRIES_DIFFER,0,"%s",String_cString(fileName));
                }
              }
              else
              {
                error = ERRORX_(READ_OPTICAL_DISK,0,"%s",String_cString(fileName));
              }
            }
            blockIndex++;
          }

          // free resources
          iso9660_stat_free(iso9660Stat);
          File_close(&fileHandle);

          n++;
          updateVolumeDone(storageInfo,0,((double)n*100.0)/(double)(size_t)StringList_count(&fileNameList));
          updateStorageRunningInfo(storageInfo);
        }
        String_delete(fileName);

        iso9660_close(iso9660Handle);
      }
      else
      {
        if (File_isFile(deviceName))
        {
          error = ERRORX_(OPEN_ISO9660_FILE,errno,"%s",String_cString(deviceName));
        }
        else
        {
          error = ERRORX_(OPEN_OPTICAL_DISK,errno,"%s",String_cString(deviceName));
        }
      }
    }

    // free resources
    StringList_done(&fileNameList);
    free(buffer1);
    free(buffer0);
  #else
    error == ERROR_FUNCTION_NOT_SUPPORTED;
  #endif

  return error;
}

LOCAL Errors StorageOptical_preProcess(StorageInfo *storageInfo,
                                       ConstString archiveName,
                                       time_t      timestamp,
                                       bool        initialFlag
                                      )
{
  uint        j;
  TextMacros  (textMacros,5);
  Errors      error;
  ConstString template;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  UNUSED_VARIABLE(initialFlag);

  error = ERROR_NONE;

  // request next medium
  if (storageInfo->opticalDisk.write.newVolumeFlag)
  {
    storageInfo->opticalDisk.write.number++;
    storageInfo->opticalDisk.write.newVolumeFlag = FALSE;

    storageInfo->volumeRequestNumber = storageInfo->opticalDisk.write.number;
  }

  // check if new medium is required
  if (storageInfo->volumeNumber != storageInfo->volumeRequestNumber)
  {
    // request load new medium
    error = requestNewOpticalMedium(storageInfo,NULL,FALSE);
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
  template = NULL;
  switch (storageInfo->storageSpecifier.type)
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
  if (!String_isEmpty(template))
  {
    printInfo(1,"Write pre-processing...");
    error = executeTemplate(String_cString(template),
                            timestamp,
                            textMacros.data,
                            textMacros.count,
                            CALLBACK_(executeIOOutput,NULL)
                           );
    printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
  }

  return error;
}

LOCAL Errors StorageOptical_postProcess(StorageInfo *storageInfo,
                                        ConstString archiveName,
                                        time_t      timestamp,
                                        bool        finalFlag
                                       )
{
  Errors        error;
  ExecuteIOInfo executeIOInfo;
  String        imageFileName;
  uint          j;
  TextMacros    (textMacros,8);
  String        fileName;
  FileInfo      fileInfo;
  ConstString   template;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;

  if (   (storageInfo->opticalDisk.write.totalSize >= storageInfo->opticalDisk.write.volumeSize)
      || (finalFlag && (storageInfo->opticalDisk.write.totalSize > 0LL))
     )
  {
    // medium size limit reached or final medium -> request new medium and writecreate medium

    // init variables
    executeIOInfo.storageInfo = storageInfo;
    executeIOInfo.commandLine = String_new();
    StringList_init(&executeIOInfo.stderrList);

    // update running info
    resetVolumeDone(storageInfo);
    updateStorageRunningInfo(storageInfo);

    // get temporary image file name
    imageFileName = String_new();
    error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
    if (error != ERROR_NONE)
    {
      String_delete(imageFileName);
      StringList_done(&executeIOInfo.stderrList);
      String_delete(executeIOInfo.commandLine);
      return error;
    }

    // init macros
    j = Thread_getNumberOfCores();
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_STRING ("%device",   storageInfo->storageSpecifier.deviceName,NULL);
      TEXT_MACRO_X_STRING ("%directory",storageInfo->opticalDisk.write.directory,NULL);
      TEXT_MACRO_X_STRING ("%image",    imageFileName,                           NULL);
      TEXT_MACRO_X_INT    ("%sectors",  0,                                       NULL);
      TEXT_MACRO_X_STRING ("%file",     archiveName,                             NULL);
      TEXT_MACRO_X_INT    ("%number",   storageInfo->volumeNumber,               NULL);
      TEXT_MACRO_X_INT    ("%j",        j,                                       NULL);
      TEXT_MACRO_X_INT    ("%j1",       (j > 1) ? j-1 : 1,                       NULL);
    }

    // check if new medium is required
    if (error == ERROR_NONE)
    {
      if (storageInfo->volumeNumber != storageInfo->volumeRequestNumber)
      {
        error = requestNewOpticalMedium(storageInfo,NULL,TRUE);
        if (error != ERROR_NONE)
        {
          (void)File_delete(imageFileName,FALSE);
          String_delete(imageFileName);
          StringList_done(&executeIOInfo.stderrList);
          String_delete(executeIOInfo.commandLine);
          return error;
        }
        updateStorageRunningInfo(storageInfo);
      }
    }

    if (   (storageInfo->jobOptions != NULL)
        && (   storageInfo->jobOptions->alwaysCreateImageFlag
            || storageInfo->jobOptions->errorCorrectionCodesFlag
           )
       )
    {
      // create and write image

      // create image pre-processing
      if (error == ERROR_NONE)
      {
        messageSet(&storageInfo->progress.message,MESSAGE_CODE_CREATE_IMAGE,NULL);
        updateStorageRunningInfo(storageInfo);

        if (!String_isEmpty(storageInfo->opticalDisk.write.imagePreProcessCommand))
        {
          printInfo(1,"Image pre-processing of volume #%u...",storageInfo->volumeNumber);
          StringList_clear(&executeIOInfo.stderrList);
          error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.imagePreProcessCommand),
                                      textMacros.data,
                                      textMacros.count,
                                      executeIOInfo.commandLine,
                                      CALLBACK_(executeIOOutput,NULL),
                                      CALLBACK_(executeIOOutput,NULL)
                                     );
          if (error == ERROR_NONE)
          {
            printInfo(1,"OK\n");
            logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(executeIOInfo.commandLine));
            logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Image pre-processed for volume #%u",storageInfo->volumeNumber);
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
            logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(executeIOInfo.commandLine));
            logLines(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "  ",
                     &executeIOInfo.stderrList
                    );
          }
        }
        updateVolumeDone(storageInfo,1,0.0);

        messageClear(&storageInfo->progress.message);
        updateStorageRunningInfo(storageInfo);
      }

      // create image
      if (error == ERROR_NONE)
      {
        messageSet(&storageInfo->progress.message,MESSAGE_CODE_CREATE_IMAGE,NULL);
        updateStorageRunningInfo(storageInfo);

        if (!String_isEmpty(storageInfo->opticalDisk.write.imageCommand))
        {
          printInfo(1,"Create image volume #%u with %d part(s)...",storageInfo->opticalDisk.write.number,StringList_count(&storageInfo->opticalDisk.write.fileNameList));
          StringList_clear(&executeIOInfo.stderrList);
          error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.imageCommand),
                                      textMacros.data,
                                      textMacros.count,
                                      executeIOInfo.commandLine,
                                      CALLBACK_(executeIOmkisofsStdout,&executeIOInfo),
                                      CALLBACK_(executeIOmkisofsStderr,&executeIOInfo)
                                     );
          if (error == ERROR_NONE)
          {
            printInfo(1,"OK\n");
            logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(executeIOInfo.commandLine));
            logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Created image for volume #%u",storageInfo->volumeNumber);

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
            logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(executeIOInfo.commandLine));
            logLines(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "  ",
                     &executeIOInfo.stderrList
                    );
          }
        }
        updateVolumeDone(storageInfo,1,0.0);

        messageClear(&storageInfo->progress.message);
        updateStorageRunningInfo(storageInfo);
      }

      // error-correction codes
      if (storageInfo->jobOptions->errorCorrectionCodesFlag)
      {
        messageSet(&storageInfo->progress.message,MESSAGE_CODE_ADD_ERROR_CORRECTION_CODES,NULL);
        updateStorageRunningInfo(storageInfo);

        // error-correction codes pre-processing
        if (error == ERROR_NONE)
        {
          if (!String_isEmpty(storageInfo->opticalDisk.write.eccPreProcessCommand))
          {
            printInfo(1,"Add ECC pre-processing to image of volume #%u...",storageInfo->volumeNumber);
            StringList_clear(&executeIOInfo.stderrList);
            error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.eccPreProcessCommand ),
                                        textMacros.data,
                                        textMacros.count,
                                        executeIOInfo.commandLine,
                                        CALLBACK_(executeIOOutput,NULL),
                                        CALLBACK_(executeIOOutput,NULL)
                                       );
            if (error == ERROR_NONE)
            {
              printInfo(1,"OK\n");
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(executeIOInfo.commandLine));
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"ECC pre-processed image for volume #%u",storageInfo->volumeNumber);
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
              logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(executeIOInfo.commandLine));
              logLines(storageInfo->logHandle,
                       LOG_TYPE_ERROR,
                       "  ",
                       &executeIOInfo.stderrList
                      );
            }
          }
        }
        updateVolumeDone(storageInfo,1,0.0);
        updateStorageRunningInfo(storageInfo);

        // add error-correction codes to image
        if (error == ERROR_NONE)
        {
          if (!String_isEmpty(storageInfo->opticalDisk.write.eccCommand))
          {
            printInfo(1,"Add ECC to image of volume #%u...",storageInfo->volumeNumber);
            StringList_clear(&executeIOInfo.stderrList);
            error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.eccCommand),
                                        textMacros.data,
                                        textMacros.count,
                                        executeIOInfo.commandLine,
                                        CALLBACK_(executeIOdvdisasterStdout,&executeIOInfo),
                                        CALLBACK_(executeIOdvdisasterStderr,&executeIOInfo)
                                       );
            if (error == ERROR_NONE)
            {
              printInfo(1,"OK\n");
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(executeIOInfo.commandLine));
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Added ECC to image for volume #%u",storageInfo->volumeNumber);
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
              logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(executeIOInfo.commandLine));
              logLines(storageInfo->logHandle,
                       LOG_TYPE_ERROR,
                       "  ",
                       &executeIOInfo.stderrList
                      );
            }
          }
        }
        updateVolumeDone(storageInfo,1,0.0);
        updateStorageRunningInfo(storageInfo);

        // error-correction codes post-processing
        if (error == ERROR_NONE)
        {
          if (!String_isEmpty(storageInfo->opticalDisk.write.eccPostProcessCommand))
          {
            printInfo(1,"Add ECC post-processing to image of volume #%u...",storageInfo->volumeNumber);
            StringList_clear(&executeIOInfo.stderrList);
            error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.eccPostProcessCommand ),
                                        textMacros.data,
                                        textMacros.count,
                                        executeIOInfo.commandLine,
                                        CALLBACK_(executeIOOutput,NULL),
                                        CALLBACK_(executeIOOutput,NULL)
                                       );
            if (error == ERROR_NONE)
            {
              printInfo(1,"OK\n");
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(executeIOInfo.commandLine));
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"ECC post-processed image for volume #%u",storageInfo->volumeNumber);
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
              logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(executeIOInfo.commandLine));
              logLines(storageInfo->logHandle,
                       LOG_TYPE_ERROR,
                       "  ",
                       &executeIOInfo.stderrList
                      );
            }
          }
        }
        updateVolumeDone(storageInfo,1,0.0);
        updateStorageRunningInfo(storageInfo);

        messageClear(&storageInfo->progress.message);
        updateStorageRunningInfo(storageInfo);
      }

      // create image post-processing
      if (error == ERROR_NONE)
      {
        messageSet(&storageInfo->progress.message,MESSAGE_CODE_CREATE_IMAGE,NULL);
        updateStorageRunningInfo(storageInfo);

        if (!String_isEmpty(storageInfo->opticalDisk.write.imagePostProcessCommand))
        {
          printInfo(1,"Image post-processing of volume #%u...",storageInfo->volumeNumber);
          StringList_clear(&executeIOInfo.stderrList);
          error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.imagePostProcessCommand ),
                                      textMacros.data,
                                      textMacros.count,
                                      executeIOInfo.commandLine,
                                      CALLBACK_(executeIOOutput,NULL),
                                      CALLBACK_(executeIOOutput,NULL)
                                     );
          if (error == ERROR_NONE)
          {
            printInfo(1,"OK\n");
            logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(executeIOInfo.commandLine));
            logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Image post-processed for volume #%u",storageInfo->volumeNumber);
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
            logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(executeIOInfo.commandLine));
            logLines(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "  ",
                     &executeIOInfo.stderrList
                    );
          }
        }

        updateVolumeDone(storageInfo,1,0.0);
        messageClear(&storageInfo->progress.message);
        updateStorageRunningInfo(storageInfo);
      }

      // update macros
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING ("%device",   storageInfo->storageSpecifier.deviceName,NULL);
        TEXT_MACRO_X_STRING ("%directory",storageInfo->opticalDisk.write.directory,NULL);
        TEXT_MACRO_X_STRING ("%image",    imageFileName,                           NULL);
        TEXT_MACRO_X_INT    ("%sectors",  (ulong)(fileInfo.size/2048LL),           NULL);
        TEXT_MACRO_X_STRING ("%file",     archiveName,                             NULL);
        TEXT_MACRO_X_INT    ("%number",   storageInfo->volumeNumber,               NULL);
        TEXT_MACRO_X_INT    ("%j",        j,                                       NULL);
        TEXT_MACRO_X_INT    ("%j1",       (j > 1) ? j-1 : 1,                       NULL);
      }

      // blank+write+verify image to medium
      if (error == ERROR_NONE)
      {
        uint saveStep   = storageInfo->opticalDisk.write.step;
        uint retryCount = 3;
        bool retryFlag;
        do
        {
          storageInfo->opticalDisk.write.step = saveStep;
          retryFlag = FALSE;

          // blank volume
          if (error == ERROR_NONE)
          {
            if (storageInfo->jobOptions->blankFlag)
            {
              if (!String_isEmpty(storageInfo->opticalDisk.write.blankCommand))
              {
                messageSet(&storageInfo->progress.message,MESSAGE_CODE_BLANK_VOLUME,NULL);
                updateStorageRunningInfo(storageInfo);

                printInfo(1,"Blank volume #%u...",storageInfo->opticalDisk.write.number);
                loadVolume(storageInfo);
                //TODO: check if medium is ready
                Misc_mdelay(OPTICAL_LOAD_VOLUME_DELAY_TIME);
                StringList_clear(&executeIOInfo.stderrList);
                error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.blankCommand),
                                            textMacros.data,
                                            textMacros.count,
                                            executeIOInfo.commandLine,
                                            CALLBACK_(executeIOblankStdout,&executeIOInfo),
                                            CALLBACK_(executeIOblankStderr,&executeIOInfo)
                                           );
                if (error == ERROR_NONE)
                {
                  printInfo(1,"OK\n");
                  logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(executeIOInfo.commandLine));
                  logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Blanked volume #%u",storageInfo->volumeNumber);
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
                  logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(executeIOInfo.commandLine));
                  logLines(storageInfo->logHandle,
                           LOG_TYPE_ERROR,
                           "  ",
                           &executeIOInfo.stderrList
                          );
                }

                updateVolumeDone(storageInfo,1,0.0);
                updateStorageRunningInfo(storageInfo);
              }
            }
          }

          // write image
          if (error == ERROR_NONE)
          {
            messageSet(&storageInfo->progress.message,MESSAGE_CODE_WRITE_VOLUME,NULL);
            updateStorageRunningInfo(storageInfo);

            printInfo(1,"Write image to volume #%u...",storageInfo->opticalDisk.write.number);
            loadVolume(storageInfo);
            //TODO: check if medium is ready
            Misc_mdelay(OPTICAL_LOAD_VOLUME_DELAY_TIME);
            StringList_clear(&executeIOInfo.stderrList);
            error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.writeImageCommand),
                                        textMacros.data,
                                        textMacros.count,
                                        executeIOInfo.commandLine,
                                        CALLBACK_(executeIOgrowisofsStdout,&executeIOInfo),
                                        CALLBACK_(executeIOgrowisofsStderr,&executeIOInfo)
                                       );
            if (error == ERROR_NONE)
            {
              printInfo(1,"OK\n");
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(executeIOInfo.commandLine));
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Written image to volume #%u",storageInfo->volumeNumber);
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
              logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(executeIOInfo.commandLine));
              logLines(storageInfo->logHandle,
                       LOG_TYPE_ERROR,
                       "  ",
                       &executeIOInfo.stderrList
                      );
            }

            updateVolumeDone(storageInfo,1,0.0);
            messageClear(&storageInfo->progress.message);
            updateStorageRunningInfo(storageInfo);
          }

          // verify volume
          if (error == ERROR_NONE)
          {
            messageSet(&storageInfo->progress.message,MESSAGE_CODE_VERIFY_VOLUME,NULL);
            updateStorageRunningInfo(storageInfo);

            printInfo(1,"Verify volume #%u...",storageInfo->opticalDisk.write.number);
            loadVolume(storageInfo);
            error = verifyVolume(storageInfo);
            if (error == ERROR_NONE)
            {
              printInfo(1,"OK\n");
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Verified volume #%u",storageInfo->volumeNumber);
            }
            else
            {
              printInfo(1,"FAIL\n");
              logMessage(storageInfo->logHandle,
                         LOG_TYPE_ERROR,
                         "Verify volume #%u fail: %s",
                         storageInfo->volumeNumber,
                         Error_getText(error)
                        );
            }

            updateVolumeDone(storageInfo,1,0.0);
            messageClear(&storageInfo->progress.message);
            updateStorageRunningInfo(storageInfo);
          }

          if (error != ERROR_NONE)
          {
            retryCount--;
            if (retryCount > 0)
            {
              if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
              {
                retryFlag = Misc_getYesNo("Retry write image to volume?");
              }
              else
              {
                retryFlag = (requestNewOpticalMedium(storageInfo,Error_getText(error),TRUE) == ERROR_NONE);
              }
            }
          }
        }
        while ((error != ERROR_NONE) && (retryCount > 0) && retryFlag);
      }
    }
    else
    {
      // directly write files

      // blank+write+verify to medium
      if (error == ERROR_NONE)
      {
        uint saveStep   = storageInfo->opticalDisk.write.step;
        uint retryCount = 3;
        bool retryFlag;
        do
        {
          storageInfo->opticalDisk.write.step = saveStep;
          retryFlag = FALSE;

          // blank volume
          if (error == ERROR_NONE)
          {
            if (storageInfo->jobOptions->blankFlag)
            {
              if (!String_isEmpty(storageInfo->opticalDisk.write.blankCommand))
              {
                messageSet(&storageInfo->progress.message,MESSAGE_CODE_BLANK_VOLUME,NULL);
                updateStorageRunningInfo(storageInfo);

                printInfo(1,"Blank volume #%u...",storageInfo->opticalDisk.write.number);
                loadVolume(storageInfo);
                //TODO: check if medium is ready
                Misc_mdelay(OPTICAL_LOAD_VOLUME_DELAY_TIME);
                StringList_clear(&executeIOInfo.stderrList);
                error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.blankCommand),
                                            textMacros.data,
                                            textMacros.count,
                                            executeIOInfo.commandLine,
                                            CALLBACK_(executeIOblankStdout,&executeIOInfo),
                                            CALLBACK_(executeIOblankStderr,&executeIOInfo)
                                           );
                if (error == ERROR_NONE)
                {
                  printInfo(1,"OK\n");
                  logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(executeIOInfo.commandLine));
                  logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Blanked volume #%u",storageInfo->volumeNumber);
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
                  logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(executeIOInfo.commandLine));
                  logLines(storageInfo->logHandle,
                           LOG_TYPE_ERROR,
                           "  ",
                           &executeIOInfo.stderrList
                          );
                }

                updateVolumeDone(storageInfo,1,0.0);
                messageClear(&storageInfo->progress.message);
                updateStorageRunningInfo(storageInfo);
              }
            }
          }

          // write medium
          if (error == ERROR_NONE)
          {
            messageSet(&storageInfo->progress.message,MESSAGE_CODE_WRITE_VOLUME,NULL);
            updateStorageRunningInfo(storageInfo);

            printInfo(1,"Write volume #%u with %d part(s)...",storageInfo->opticalDisk.write.number,StringList_count(&storageInfo->opticalDisk.write.fileNameList));
            loadVolume(storageInfo);
            //TODO: check if medium is ready
            Misc_mdelay(OPTICAL_LOAD_VOLUME_DELAY_TIME);
            StringList_clear(&executeIOInfo.stderrList);
            error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.writeCommand),
                                        textMacros.data,
                                        textMacros.count,
                                        executeIOInfo.commandLine,
                                        CALLBACK_(executeIOgrowisofsStdout,&executeIOInfo),
                                        CALLBACK_(executeIOgrowisofsStderr,&executeIOInfo)
                                       );
            if (error == ERROR_NONE)
            {
              printInfo(1,"OK\n");
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(executeIOInfo.commandLine));
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Written volume #%u",storageInfo->volumeNumber);
            }
            else
            {
              printInfo(1,"FAIL (error: %s)\n",Error_getText(error));

              logMessage(storageInfo->logHandle,
                         LOG_TYPE_ERROR,
                         "Write volume #%u fail: %s",
                         storageInfo->volumeNumber,
                         Error_getText(error)
                        );
              logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(executeIOInfo.commandLine));
              logLines(storageInfo->logHandle,
                       LOG_TYPE_ERROR,
                       "  ",
                       &executeIOInfo.stderrList
                      );
            }

            updateVolumeDone(storageInfo,1,0.0);
            messageClear(&storageInfo->progress.message);
            updateStorageRunningInfo(storageInfo);
          }

          // verify volume
          if (error == ERROR_NONE)
          {
            messageSet(&storageInfo->progress.message,MESSAGE_CODE_VERIFY_VOLUME,NULL);
            updateStorageRunningInfo(storageInfo);

            printInfo(1,"Verify volume #%u...",storageInfo->opticalDisk.write.number);
            loadVolume(storageInfo);
            error = verifyVolume(storageInfo);
            if (error == ERROR_NONE)
            {
              printInfo(1,"OK\n");
              logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Verified volume #%u",storageInfo->volumeNumber);
            }
            else
            {
              printInfo(1,"FAIL\n");
              logMessage(storageInfo->logHandle,
                         LOG_TYPE_ERROR,
                         "Verify volume #%u fail: %s",
                         storageInfo->volumeNumber,
                         Error_getText(error)
                        );
            }

            updateVolumeDone(storageInfo,1,0.0);
            messageClear(&storageInfo->progress.message);
            updateStorageRunningInfo(storageInfo);
          }

          if (error != ERROR_NONE)
          {
            retryCount--;
            if (retryCount > 0)
            {
              if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
              {
                retryFlag = Misc_getYesNo("Retry write volume?");
              }
              else
              {
                retryFlag = (requestNewOpticalMedium(storageInfo,Error_getText(error),TRUE) == ERROR_NONE);
              }
            }
          }
        }
        while ((error != ERROR_NONE) && (retryCount > 0) && retryFlag);
      }
    }

    // delete image
    if (error == ERROR_NONE)
    {
      error = File_delete(imageFileName,FALSE);
    }
    else
    {
      (void)File_delete(imageFileName,FALSE);
    }
    String_delete(imageFileName);

    // delete stored files
    fileName = String_new();
    while (!StringList_isEmpty(&storageInfo->opticalDisk.write.fileNameList))
    {
      StringList_removeFirst(&storageInfo->opticalDisk.write.fileNameList,fileName);
      if (error == ERROR_NONE)
      {
        error = File_delete(fileName,FALSE);
      }
      else
      {
        (void)File_delete(fileName,FALSE);
      }
    }
    String_delete(fileName);

    // update running info
    updateVolumeDone(storageInfo,0,100.0);
    updateStorageRunningInfo(storageInfo);

    // handle error
    if (error != ERROR_NONE)
    {
      StringList_done(&executeIOInfo.stderrList);
      String_delete(executeIOInfo.commandLine);
      return error;
    }

    // reset
    storageInfo->opticalDisk.write.newVolumeFlag = TRUE;
    storageInfo->opticalDisk.write.totalSize     = 0;

    // free resources
    String_delete(executeIOInfo.commandLine);
    StringList_done(&executeIOInfo.stderrList);
  }

  // write post-processing
  template = NULL;
  switch (storageInfo->storageSpecifier.type)
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
  if (!String_isEmpty(template))
  {
    printInfo(1,"Write post-processing...");
    error = executeTemplate(String_cString(template),
                            timestamp,
                            textMacros.data,
                            textMacros.count,
                            CALLBACK_(executeIOOutput,NULL)
                           );
    printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
  }

  return error;
}

LOCAL bool StorageOptical_exists(const StorageInfo *storageInfo, ConstString archiveName)
{
  bool existsFlag;
  #ifdef HAVE_ISO9660
    String         pathName,fileName;
    iso9660_t      *iso9660Handle;
    iso9660_stat_t *iso9660Stat;
    CdioList_t     *cdioList;
    CdioListNode_t *cdioNextNode;
    char           *s;
  #else /* not HAVE_ISO9660 */
    String fileName;
  #endif /* HAVE_ISO9660 */

  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  existsFlag = FALSE;

  #ifdef HAVE_ISO9660
    pathName = File_getDirectoryName(String_new(),archiveName);
    fileName = File_getBaseName(String_new(),archiveName,TRUE);

    iso9660Handle = iso9660_open_ext(String_cString(storageInfo->storageSpecifier.deviceName),
                                     ISO_EXTENSION_ALL
                                    );
    if (iso9660Handle != NULL)
    {
// TODO: use
#if 0
      iso9660_stat_t *iso9660Stat = iso9660_ifs_stat_translate(iso9660Handle,String_cString(archiveName));
      if (iso9660Stat != NULL)
      {
        existsFlag = TRUE;
        iso9660_stat_free(iso9660Stat);
      }
#else
      cdioList = iso9660_ifs_readdir(iso9660Handle,String_cString(pathName));
      if (cdioList != NULL)
      {
        cdioNextNode = _cdio_list_begin(cdioList);
        while ((cdioNextNode != NULL) && !existsFlag)
        {
          iso9660Stat = (iso9660_stat_t*)_cdio_list_node_data(cdioNextNode);

          s = (char*)malloc(strlen(iso9660Stat->filename)+1);
          if (s != NULL)
          {
            iso9660_name_translate_ext(iso9660Stat->filename,s,ISO_EXTENSION_JOLIET_LEVEL1);
            existsFlag = String_equalsCString(fileName,s);
            free(s);
          }
          cdioNextNode = _cdio_list_node_next(cdioNextNode);
        }
        _cdio_list_free(cdioList,TRUE,free);
      }
#endif
      (void)iso9660_close(iso9660Handle);
    }

    String_delete(fileName);
    String_delete(pathName);
  #else /* not HAVE_ISO9660 */
    fileName = String_duplicate(storageInfo->storageSpecifier.deviceName);
    File_appendFileName(fileName,archiveName);
    existsFlag = File_exists(fileName);
    String_delete(fileName);
  #endif /* HAVE_ISO9660 */

  return existsFlag;
}

LOCAL bool StorageOptical_isFile(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(archiveName != NULL);

  #ifdef HAVE_ISO9660
    ConstString deviceName = getDeviceName(&storageInfo->storageSpecifier);

    // check if device exists
    if (String_isEmpty(deviceName))
    {
      return FALSE;
    }
    if (!File_exists(deviceName))
    {
      return FALSE;
    }

    iso9660_t *iso9660Handle = iso9660_open_ext(String_cString(deviceName),ISO_EXTENSION_ALL);
    if (iso9660Handle == NULL)
    {
      return FALSE;
    }

    iso9660_stat_t *iso9660Stat = iso9660_ifs_stat_translate(iso9660Handle,String_cString(archiveName));
    if (iso9660Stat == NULL)
    {
      (void)iso9660_close(iso9660Handle);
      return FALSE;
    }

    bool isFile = iso9660Stat->type == _STAT_FILE;

    iso9660_stat_free(iso9660Stat);
    (void)iso9660_close(iso9660Handle);

    return isFile;
  #else /* not HAVE_ISO9660 */
    return FALSE;
  #endif /* HAVE_ISO9660 */
}

LOCAL bool StorageOptical_isDirectory(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(archiveName != NULL);

  #ifdef HAVE_ISO9660
    ConstString deviceName = getDeviceName(&storageInfo->storageSpecifier);

    // check if device exists
    if (String_isEmpty(deviceName))
    {
      return FALSE;
    }
    if (!File_exists(deviceName))
    {
      return FALSE;
    }

    iso9660_t *iso9660Handle = iso9660_open_ext(String_cString(deviceName),ISO_EXTENSION_ALL);
    if (iso9660Handle == NULL)
    {
      return FALSE;
    }

    iso9660_stat_t *iso9660Stat = iso9660_ifs_stat_translate(iso9660Handle,String_cString(archiveName));
    if (iso9660Stat == NULL)
    {
      (void)iso9660_close(iso9660Handle);
      return FALSE;
    }

    bool isFile = iso9660Stat->type == _STAT_DIR;

    iso9660_stat_free(iso9660Stat);
    (void)iso9660_close(iso9660Handle);

    return isFile;
  #else /* not HAVE_ISO9660 */
    return FALSE;
  #endif /* HAVE_ISO9660 */
}

LOCAL bool StorageOptical_isReadable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  #ifdef HAVE_ISO9660
    ConstString deviceName = getDeviceName(&storageInfo->storageSpecifier);

    // check if device exists
    if (String_isEmpty(deviceName))
    {
      return FALSE;
    }
    if (!File_exists(deviceName))
    {
      return FALSE;
    }

    iso9660_t *iso9660Handle = iso9660_open_ext(String_cString(deviceName),ISO_EXTENSION_ALL);
    if (iso9660Handle == NULL)
    {
      return FALSE;
    }

    iso9660_stat_t *iso9660Stat = iso9660_ifs_stat_translate(iso9660Handle,String_cString(archiveName));
    bool isReadable = (iso9660Stat != NULL);
    if (iso9660Stat != NULL)
    {
      iso9660_stat_free(iso9660Stat);
    }

    (void)iso9660_close(iso9660Handle);

    return isReadable;
  #else /* not HAVE_ISO9660 */
    return FALSE;
  #endif /* HAVE_ISO9660 */
}

LOCAL bool StorageOptical_isWritable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return FALSE;
}

LOCAL Errors StorageOptical_getTmpName(String archiveName, const StorageInfo *storageInfo)
{
  assert(archiveName != NULL);
  assert(!String_isEmpty(archiveName));
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(archiveName);
  UNUSED_VARIABLE(storageInfo);

//TODO
  return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL Errors StorageOptical_create(StorageHandle *storageHandle,
                                   ConstString   fileName,
                                   uint64        fileSize,
                                   bool          forceFlag
                                  )
{
  Errors error;
  String directoryName;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);

  // init variables
  #ifdef HAVE_ISO9660
    storageHandle->opticalDisk.read.iso9660Handle     = NULL;
    storageHandle->opticalDisk.read.iso9660Stat       = NULL;
    storageHandle->opticalDisk.read.index             = 0LL;
    storageHandle->opticalDisk.read.buffer.blockIndex = 0LL;
    storageHandle->opticalDisk.read.buffer.length     = 0L;
  #endif /* HAVE_ISO9660 */
  storageHandle->opticalDisk.write.fileName           = String_new();

  // check if file exists
  if (   !forceFlag
      && (storageHandle->storageInfo->jobOptions != NULL)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_APPEND)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_OVERWRITE)
      && !storageHandle->storageInfo->jobOptions->blankFlag
      && StorageOptical_exists(storageHandle->storageInfo,fileName)
     )
  {
    String_delete(storageHandle->opticalDisk.write.fileName);
    return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(fileName));
  }

  // create file name
  String_set(storageHandle->opticalDisk.write.fileName,storageHandle->storageInfo->opticalDisk.write.directory);
  File_appendFileName(storageHandle->opticalDisk.write.fileName,fileName);

  // create directory if not existing
  directoryName = File_getDirectoryName(String_new(),storageHandle->opticalDisk.write.fileName);
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
      String_delete(storageHandle->opticalDisk.write.fileName);
      return error;
    }
  }
  String_delete(directoryName);

  // create file
  error = File_open(&storageHandle->opticalDisk.write.fileHandle,
                    storageHandle->opticalDisk.write.fileName,
                    FILE_OPEN_CREATE
                   );
  if (error != ERROR_NONE)
  {
    String_delete(storageHandle->opticalDisk.write.fileName);
    return error;
  }

  return ERROR_NONE;
}

LOCAL Errors StorageOptical_open(StorageHandle *storageHandle,
                                 ConstString   archiveName
                                )
{
  assert(storageHandle != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(!String_isEmpty(archiveName));

  #ifdef HAVE_ISO9660
    // initialize variables
    storageHandle->opticalDisk.read.iso9660Handle     = NULL;
    storageHandle->opticalDisk.read.iso9660Stat       = NULL;
    storageHandle->opticalDisk.read.index             = 0LL;
    storageHandle->opticalDisk.read.buffer.blockIndex = 0LL;
    storageHandle->opticalDisk.read.buffer.length     = 0L;

    storageHandle->opticalDisk.read.buffer.data = (byte*)malloc(ISO_BLOCKSIZE);
    if (storageHandle->opticalDisk.read.buffer.data == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    // check if device name exists
    if (String_isEmpty(storageHandle->storageInfo->storageSpecifier.deviceName))
    {
      free(storageHandle->opticalDisk.read.buffer.data);
      return ERROR_NO_DEVICE_NAME;
    }
    if (!Device_exists(storageHandle->storageInfo->storageSpecifier.deviceName))
    {
      free(storageHandle->opticalDisk.read.buffer.data);
      return ERRORX_(OPTICAL_DISK_NOT_FOUND,0,"%s",String_cString(storageHandle->storageInfo->storageSpecifier.deviceName));
    }

    // open optical disk/ISO 9660 file
    storageHandle->opticalDisk.read.iso9660Handle = iso9660_open_ext(String_cString(storageHandle->storageInfo->storageSpecifier.deviceName),
                                                                     ISO_EXTENSION_ALL
                                                                    );
    if (storageHandle->opticalDisk.read.iso9660Handle == NULL)
    {
      if (File_isFile(storageHandle->storageInfo->storageSpecifier.deviceName))
      {
        free(storageHandle->opticalDisk.read.buffer.data);
        return ERRORX_(OPEN_ISO9660_FILE,errno,"%s",String_cString(storageHandle->storageInfo->storageSpecifier.deviceName));
      }
      else
      {
        free(storageHandle->opticalDisk.read.buffer.data);
        return ERRORX_(OPEN_OPTICAL_DISK,errno,"%s",String_cString(storageHandle->storageInfo->storageSpecifier.deviceName));
      }
    }

    // prepare file for reading
    storageHandle->opticalDisk.read.iso9660Stat = iso9660_ifs_stat_translate(storageHandle->opticalDisk.read.iso9660Handle,
                                                                             String_cString(archiveName)
                                                                            );
    if (storageHandle->opticalDisk.read.iso9660Stat == NULL)
    {
      iso9660_close(storageHandle->opticalDisk.read.iso9660Handle);
      free(storageHandle->opticalDisk.read.buffer.data);
      return ERRORX_(FILE_NOT_FOUND_,errno,"%s",String_cString(archiveName));
    }

    return ERROR_NONE;
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_ISO9660 */
}

LOCAL void StorageOptical_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  switch (storageHandle->mode)
  {
    case STORAGE_MODE_READ:
      #ifdef HAVE_ISO9660
        assert(storageHandle->opticalDisk.read.iso9660Handle != NULL);
        assert(storageHandle->opticalDisk.read.iso9660Stat != NULL);

        iso9660_stat_free(storageHandle->opticalDisk.read.iso9660Stat);
        iso9660_close(storageHandle->opticalDisk.read.iso9660Handle);
        free(storageHandle->opticalDisk.read.buffer.data);
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_MODE_WRITE:
      SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
      {
        storageHandle->storageInfo->opticalDisk.write.totalSize += File_getSize(&storageHandle->opticalDisk.write.fileHandle);
        StringList_append(&storageHandle->storageInfo->opticalDisk.write.fileNameList,storageHandle->opticalDisk.write.fileName);
      }
      (void)File_close(&storageHandle->opticalDisk.write.fileHandle);
      String_delete(storageHandle->opticalDisk.write.fileName);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
}

LOCAL bool StorageOptical_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  #ifdef HAVE_ISO9660
    assert(storageHandle->opticalDisk.read.iso9660Handle != NULL);
    assert(storageHandle->opticalDisk.read.iso9660Stat != NULL);

    return storageHandle->opticalDisk.read.index >= storageHandle->opticalDisk.read.iso9660Stat->size;
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);

    return TRUE;
  #endif /* HAVE_ISO9660 */
}

LOCAL Errors StorageOptical_read(StorageHandle *storageHandle,
                                 void          *buffer,
                                 ulong         bufferSize,
                                 ulong         *bytesRead
                                )
{
  #ifdef HAVE_ISO9660
    Errors   error;
    uint64   blockIndex;
    uint     blockOffset;
    long int n;
    ulong    bytesAvail;
  #endif

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(buffer != NULL);
  #ifdef HAVE_ISO9660
    assert(storageHandle->opticalDisk.read.iso9660Handle != NULL);
    assert(storageHandle->opticalDisk.read.iso9660Stat != NULL);
    assert(storageHandle->opticalDisk.read.buffer.data != NULL);
  #endif

  if (bytesRead != NULL) (*bytesRead) = 0L;
  #ifdef HAVE_ISO9660
    error = ERROR_NONE;
    while (   (bufferSize > 0L)
           && (storageHandle->opticalDisk.read.index < (uint64)storageHandle->opticalDisk.read.iso9660Stat->size)
          )
    {
      // get ISO9660 block index, offset
      blockIndex  = (int64)(storageHandle->opticalDisk.read.index/ISO_BLOCKSIZE);
      blockOffset = (uint)(storageHandle->opticalDisk.read.index%ISO_BLOCKSIZE);

      if (   (blockIndex != storageHandle->opticalDisk.read.buffer.blockIndex)
          || (blockOffset >= storageHandle->opticalDisk.read.buffer.length)
         )
      {
        // read ISO9660 block
        n = iso9660_iso_seek_read(storageHandle->opticalDisk.read.iso9660Handle,
                                  storageHandle->opticalDisk.read.buffer.data,
                                  storageHandle->opticalDisk.read.iso9660Stat->lsn+(lsn_t)blockIndex,
                                  1 // read 1 block
                                 );
        if (n < ISO_BLOCKSIZE)
        {
          error = ERROR_(IO,errno);
          break;
        }
        storageHandle->opticalDisk.read.buffer.blockIndex = blockIndex;
        storageHandle->opticalDisk.read.buffer.length     = (((blockIndex+1)*ISO_BLOCKSIZE) <= (uint64)storageHandle->opticalDisk.read.iso9660Stat->size)
                                                              ? ISO_BLOCKSIZE
                                                              : (ulong)(storageHandle->opticalDisk.read.iso9660Stat->size%ISO_BLOCKSIZE);
      }

      // copy data
      bytesAvail = MIN(bufferSize,storageHandle->opticalDisk.read.buffer.length-blockOffset);
      memcpy(buffer,storageHandle->opticalDisk.read.buffer.data+blockOffset,bytesAvail);

      // adjust buffer, bufferSize, bytes read, index
      buffer = (byte*)buffer+bytesAvail;
      bufferSize -= bytesAvail;
      if (bytesRead != NULL) (*bytesRead) += bytesAvail;
      storageHandle->opticalDisk.read.index += (uint64)bytesAvail;
    }

    return error;
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferSize);
    UNUSED_VARIABLE(bytesRead);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_ISO9660 */
}

LOCAL Errors StorageOptical_write(StorageHandle *storageHandle,
                                  const void    *buffer,
                                  ulong         bufferLength
                                 )
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(buffer != NULL);

  return File_write(&storageHandle->opticalDisk.write.fileHandle,buffer,bufferLength);
}

LOCAL Errors StorageOptical_tell(StorageHandle *storageHandle,
                                 uint64        *offset
                                )
{
  #ifdef HAVE_ISO9660
    Errors error;
  #endif

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(offset != NULL);

  (*offset) = 0LL;

  #ifdef HAVE_ISO9660
    error = ERROR_UNKNOWN;
    switch (storageHandle->mode)
    {
      case STORAGE_MODE_READ:
        (*offset) = storageHandle->opticalDisk.read.index;
        error     = ERROR_NONE;
        break;
      case STORAGE_MODE_WRITE:
        error = File_tell(&storageHandle->opticalDisk.write.fileHandle,offset);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    assert(error != ERROR_UNKNOWN);

    return error;
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_ISO9660 */
}

LOCAL Errors StorageOptical_seek(StorageHandle *storageHandle,
                                 uint64        offset
                                )
{
  #ifdef HAVE_ISO9660
    Errors error;
  #endif

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  #ifdef HAVE_ISO9660
    error = ERROR_UNKNOWN;
    switch (storageHandle->mode)
    {
      case STORAGE_MODE_READ:
        storageHandle->opticalDisk.read.index = offset;
        error = ERROR_NONE;
        break;
      case STORAGE_MODE_WRITE:
        error = File_seek(&storageHandle->opticalDisk.write.fileHandle,offset);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    assert(error != ERROR_UNKNOWN);

    return error;
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_ISO9660 */
}

LOCAL uint64 StorageOptical_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  size = 0LL;
  #ifdef HAVE_ISO9660
    switch (storageHandle->mode)
    {
      case STORAGE_MODE_READ:
        assert(storageHandle->opticalDisk.read.iso9660Stat);
        size = (uint64)storageHandle->opticalDisk.read.iso9660Stat->size;
        break;
      case STORAGE_MODE_WRITE:
        size = File_getSize(&storageHandle->opticalDisk.write.fileHandle);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_ISO9660 */

  return size;
}

LOCAL Errors StorageOptical_rename(const StorageInfo *storageInfo,
                                   ConstString       fromArchiveName,
                                   ConstString       toArchiveName
                                  )
{
  Errors error;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

// TODO:
UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(fromArchiveName);
UNUSED_VARIABLE(toArchiveName);
#ifndef WERROR
#warning TODO still not implemented
#endif
error = ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

LOCAL Errors StorageOptical_makeDirectory(const StorageInfo *storageInfo,
                                          ConstString       directoryName
                                         )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(directoryName));

// TODO:
UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(directoryName);
#ifndef WERROR
#warning TODO still not implemented
#endif
error = ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

LOCAL Errors StorageOptical_delete(const StorageInfo *storageInfo,
                                   ConstString       archiveName
                                  )
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

#if 0
still not complete
LOCAL Errors StorageOptical_getInfo(const StorageInfo *storageInfo,
                                    ConstString       fileName,
                                    FileInfo          *fileInfo
                                   )
{
  String infoFileName;
  Errors error;

  assert(storageInfo != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));
  assert(fileInfo != NULL);

  infoFileName = (fileName != NULL) ? fileName : storageInfo->storageSpecifier.archiveName;
  memClear(fileInfo,sizeof(fileInfo));

  return ERROR_FUNCTION_NOT_SUPPORTED;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageOptical_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                              const StorageSpecifier     *storageSpecifier,
                                              ConstString                pathName,
                                              const JobOptions           *jobOptions,
                                              ServerConnectionPriorities serverConnectionPriority
                                             )
{
  AutoFreeList autoFreeList;
  Errors       error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));
  assert(pathName != NULL);
  assert(jobOptions != NULL);

  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);

  // init variables
  AutoFree_init(&autoFreeList);

  #ifdef HAVE_ISO9660
    storageDirectoryListHandle->opticalDisk.pathName = String_duplicate(pathName);
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

    // open optical disk/ISO 9660 device
    storageDirectoryListHandle->opticalDisk.iso9660Handle = iso9660_open_ext(String_cString(storageDirectoryListHandle->storageSpecifier.deviceName),
                                                                             ISO_EXTENSION_ALL
                                                                            );
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
                                                                           String_cString(storageDirectoryListHandle->opticalDisk.pathName)
                                                                          );
    if (storageDirectoryListHandle->opticalDisk.cdioList == NULL)
    {
      error = ERRORX_(FILE_NOT_FOUND_,errno,"%s",String_cString(storageDirectoryListHandle->opticalDisk.pathName));
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    storageDirectoryListHandle->opticalDisk.cdioNextNode = _cdio_list_begin(storageDirectoryListHandle->opticalDisk.cdioList);

    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_ISO9660 */
    storageDirectoryListHandle->opticalDisk.pathName = String_duplicate(pathName);
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->opticalDisk.pathName,{ String_delete(storageDirectoryListHandle->opticalDisk.pathName); });

    // open directory
    error = File_openDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle,
                                   storageDirectoryListHandle->opticalDisk.pathName
                                  );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->opticalDisk.directoryListHandle,{ File_closeDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle); });

    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #endif /* HAVE_ISO9660 */
}

LOCAL void StorageOptical_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert((storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  #ifdef HAVE_ISO9660
    _cdio_list_free(storageDirectoryListHandle->opticalDisk.cdioList,TRUE,free);
    (void)iso9660_close(storageDirectoryListHandle->opticalDisk.iso9660Handle);
    String_delete(storageDirectoryListHandle->opticalDisk.pathName);
  #else /* not HAVE_ISO9660 */
    File_closeDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle);
  #endif /* HAVE_ISO9660 */
}

LOCAL bool StorageOptical_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  #ifdef HAVE_ISO9660
    const iso9660_stat_t* iso9660Stat;
  #endif /* HAVE_ISO9660 */
  bool endOfDirectoryFlag;

  assert(storageDirectoryListHandle != NULL);
  assert((storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  endOfDirectoryFlag = TRUE;
  #ifdef HAVE_ISO9660
    if (storageDirectoryListHandle->opticalDisk.cdioNextNode != NULL)
    {
      // skip directory "." and ".."
      iso9660Stat = (iso9660_stat_t*)_cdio_list_node_data(storageDirectoryListHandle->opticalDisk.cdioNextNode);
      assert(iso9660Stat != NULL);
      while (   (storageDirectoryListHandle->opticalDisk.cdioNextNode != NULL)
             && (iso9660Stat->type == _STAT_DIR)
             && ((stringEquals(iso9660Stat->filename,".") || stringEquals(iso9660Stat->filename,"..")))
            )
      {
        storageDirectoryListHandle->opticalDisk.cdioNextNode = _cdio_list_node_next(storageDirectoryListHandle->opticalDisk.cdioNextNode);
        if (storageDirectoryListHandle->opticalDisk.cdioNextNode != NULL)
        {
          iso9660Stat = (iso9660_stat_t*)_cdio_list_node_data(storageDirectoryListHandle->opticalDisk.cdioNextNode);
          assert(iso9660Stat != NULL);
        }
      }
    }
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
        // skip directory "." and ".."
        iso9660Stat = (iso9660_stat_t*)_cdio_list_node_data(storageDirectoryListHandle->opticalDisk.cdioNextNode);
        while (   (storageDirectoryListHandle->opticalDisk.cdioNextNode != NULL)
               && (iso9660Stat->type == _STAT_DIR)
               && ((stringEquals(iso9660Stat->filename,".") || stringEquals(iso9660Stat->filename,"..")))
              )
        {
          storageDirectoryListHandle->opticalDisk.cdioNextNode = _cdio_list_node_next(storageDirectoryListHandle->opticalDisk.cdioNextNode);
          if (storageDirectoryListHandle->opticalDisk.cdioNextNode != NULL)
          {
            iso9660Stat = (iso9660_stat_t*)_cdio_list_node_data(storageDirectoryListHandle->opticalDisk.cdioNextNode);
          }
        }
        if (storageDirectoryListHandle->opticalDisk.cdioNextNode != NULL)
        {
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
              fileInfo->permissions     = iso9660Stat->xa.attributes;
              fileInfo->major           = 0;
              fileInfo->minor           = 0;
              memClear(&fileInfo->cast,sizeof(FileCast));
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
