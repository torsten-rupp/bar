/***********************************************************************\
*
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
#if   defined(HAVE_BURN)
  #pragma GCC push_options  // Note: some prototypes have the syntax void foo() instead of void foo(void)
  #pragma GCC diagnostic ignored "-Wstrict-prototypes"
  #include "libisofs/libisofs.h"
  #pragma GCC pop_options
  #include "libburn/libburn.h"
#elif defined(HAVE_ISOFS)
  #define LIBISOFS_WITHOUT_LIBBURN 1
  #pragma GCC push_options  // Note: some prototypes have the syntax void foo() instead of void foo(void)
  #pragma GCC diagnostic ignored "-Wstrict-prototypes"
  #include "libisofs/libisofs.h"
  #pragma GCC pop_options
#endif /* HAVE_... */
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
#include "archives.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define OPTICAL_UNLOAD_VOLUME_DELAY_TIME (30LL*MS_PER_SECOND) // [ms]
#define OPTICAL_LOAD_VOLUME_DELAY_TIME   (30LL*MS_PER_SECOND) // [ms]

#define MAX_CD_SIZE  (        900LL*MB)  // 900M
#define MAX_DVD_SIZE (2LL*4613734LL*KB)  // 9G (dual layer)
#define MAX_BD_SIZE  (     3LL*25LL*GB)  // 50G (triple layer)

#define CD_VOLUME_SIZE      ( 700LL*MB)
#define CD_VOLUME_ECC_SIZE  ( 560LL*MB)
#define DVD_VOLUME_SIZE     (4482LL*MB)
#define DVD_VOLUME_ECC_SIZE (3600LL*MB)
#define BD_VOLUME_SIZE      (  25LL*GB)
#define BD_VOLUME_ECC_SIZE  (  20LL*GB)

#define ISO_SECTOR_SIZE     2048
#define ISO_FIFO_SIZE       2048      // 2048*2048 = 4MB
#define ISO_PADDING         (300*KB)  // 300KB padding

/***************************** Datatypes *******************************/

// burn source handle
typedef struct
{
  int                fileHandle;
  IsoImage           *isoImage;
  IsoWriteOpts       *isoWriteOpts;
  struct burn_source *burnSource;
} ISOSourceHandle;

// execute I/O info
typedef struct
{
  StorageInfo *storageInfo;
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
* Name   : setMessageSeveritity
* Purpose: set debug verbose level in libisofs+libburn
* Input  : verboseLevel - verbose level
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void setMessageSeveritity(long verboseLevel)
{
  if      (verboseLevel >= 5)
  {
    #ifdef HAVE_ISOFS
      iso_set_msgs_severities("ALL", "NEVER", "");
    #endif // HAVE_ISOFS
  #ifdef HAVE_BURN
    burn_msgs_set_severities("ALL", "NEVER", "");
  #endif // HAVE_BURN
  }
  else if (verboseLevel >= 4)
  {
    #ifdef HAVE_ISOFS
      iso_set_msgs_severities("DEBUG", "NEVER", "");
    #endif // HAVE_ISOFS
  #ifdef HAVE_BURN
    burn_msgs_set_severities("DEBUG", "NEVER", "");
  #endif // HAVE_BURN
  }
  else
  {
    #ifdef HAVE_ISOFS
      iso_set_msgs_severities("NEVER", "NEVER", "");
    #endif // HAVE_ISOFS
  #ifdef HAVE_BURN
    burn_msgs_set_severities("NEVER", "NEVER", "");
  #endif // HAVE_BURN
  }
}

/***********************************************************************\
* Name   : libisofsPollMessages
* Purpose: poll and output debug messages from libisofs
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void libisofsPollMessages(void)
{
  int  errorCode;
  int  imageId;
  char messageText[ISO_MSGS_MESSAGE_LEN];
  char severity[80];
  while (iso_obtain_msgs("ALL",&errorCode,&imageId,messageText,severity) == 1)
  {
    UNUSED_VARIABLE(imageId);
    UNUSED_VARIABLE(severity);
    printInfo(0, "DEBUG libisofs: %s (error %d)\n", messageText, errorCode);
  }
}

/***********************************************************************\
* Name   : libburnPollMessages
* Purpose: poll and output debug messages from libburn
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void libburnPollMessages(void)
{
  int  errorCode;
  char messageText[BURN_MSGS_MESSAGE_LEN];
  int  osError;
  char severity[80];
  while (burn_msgs_obtain("ALL",&errorCode,messageText,&osError,severity) == 1)
  {
    UNUSED_VARIABLE(osError);
    UNUSED_VARIABLE(severity);
    printInfo(0, "DEBUG liburn: %s (error %d)\n", messageText, errorCode);
  }
}

/***********************************************************************\
* Name   : initOpticalExecuteIOInfo
* Purpose: initialize execute I/O info
* Input  : executeIOInfo - execute I/O info variable
*          storageInfo   - storag einfo
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOpticalExecuteIOInfo(ExecuteIOInfo *executeIOInfo, StorageInfo *storageInfo)
{
  assert(executeIOInfo != NULL);

  executeIOInfo->storageInfo = storageInfo;
  StringList_init(&executeIOInfo->stderrList);
}

/***********************************************************************\
* Name   : doneOpticalExecuteIOInfo
* Purpose: deinitialize execute I/O info
* Input  : executeIOInfo - execute I/O info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOpticalExecuteIOInfo(ExecuteIOInfo *executeIOInfo)
{
  assert(executeIOInfo != NULL);

  StringList_done(&executeIOInfo->stderrList);
}

/***********************************************************************\
* Name   : getOpticalDeviceName
* Purpose: get device name from storage specifider or global device name
* Input  : storageSpecifier - storage specifier
* Output : -
* Return : device name
* Notes  : -
\***********************************************************************/

LOCAL const char *getOpticalDeviceName(const StorageSpecifier *storageSpecifier)
{
  const char *deviceName = NULL;
  if (!String_isEmpty(storageSpecifier->deviceName))
  {
    deviceName = String_cString(storageSpecifier->deviceName);
  }
  else
  {
    switch (storageSpecifier->type)
    {
      case STORAGE_TYPE_CD:
        deviceName = String_cString(globalOptions.cd.deviceName);
        break;
      case STORAGE_TYPE_DVD:
        deviceName = String_cString(globalOptions.dvd.deviceName);
        break;
      case STORAGE_TYPE_BD:
        deviceName = String_cString(globalOptions.bd.deviceName);
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

/***********************************************************************\
* Name   : libcdioLogCallback
* Purpose: callback for libcdio log messages
* Input  : level   - level (not used)
*          message - message
* Output : -
* Return : *
* Notes  : -
\***********************************************************************/

#ifdef HAVE_ISOFS
LOCAL void libcdioLogCallback(cdio_log_level_t level, const char *message)
{
  UNUSED_VARIABLE(level);

  printInfo(5,"libcdio: %s\n",message);
}
#endif /* HAVE_ISOFS */

/***********************************************************************\
* Name   : executeIOOpticalMkIsofsOutput
* Purpose: process mkisofs output
* Input  : storageInfo - storage info
*          line        - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalMkIsofsOutput(StorageInfo *storageInfo,
                                         ConstString line
                                        )
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);

  String s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*?([0-9\\.]+)%.*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    double percentageDone = String_toDouble(s,0,NULL,NULL,0);
    updateVolumeDone(storageInfo,0,percentageDone,0.0);
    updateStorageRunningInfo(storageInfo);
  }
  String_delete(s);
}

/***********************************************************************\
* Name   : executeIOOpticalMkIsofsStdout
* Purpose: process mkisofs stdout
* Input  : line     - line
*          userData - execute I/O info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalMkIsofsStdout(ConstString line,
                                         void        *userData
                                        )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;
  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);
  executeIOOpticalMkIsofsOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,NULL);
}

/***********************************************************************\
* Name   : executeIOOpticalMkisofsStderr
* Purpose: process mkisofs stderr
* Input  : line     - line
*          userData - execute I/O info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalMkIsofsStderr(ConstString line,
                                         void        *userData
                                        )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;
  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);
  executeIOOpticalMkIsofsOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,&executeIOInfo->stderrList);
}

/***********************************************************************\
* Name   : executeIOOpticalDVDDisasterOutput
* Purpose: process dvdisaster output
* Input  : storageInfo - storage info
*          line        - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalDVDDisasterOutput(StorageInfo *storageInfo,
                                             ConstString line
                                            )
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);

  String s = String_new();
  if      (String_matchCString(line,STRING_BEGIN,".*adding space\\): +([0-9\\.]+)%",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    double percentageDone = String_toDouble(s,0,NULL,NULL,0);
    updateVolumeDone(storageInfo,0, 0+percentageDone/2,0.0);
    updateStorageRunningInfo(storageInfo);
  }
  else if (String_matchCString(line,STRING_BEGIN,".*generation: +([0-9\\.]+)%",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    double percentageDone = String_toDouble(s,0,NULL,NULL,0);
    updateVolumeDone(storageInfo,0,50+percentageDone/2,0.0);
    updateStorageRunningInfo(storageInfo);
  }
  String_delete(s);
}

/***********************************************************************\
* Name   : executeIOOpticalDVDDisasterStdout
* Purpose: process dvdisaster stdout
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalDVDDisasterStdout(ConstString line,
                                             void        *userData
                                            )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;
  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);
  executeIOOpticalDVDDisasterOutput(executeIOInfo->storageInfo,line);
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

LOCAL void executeIOOpticalDVDDisasterStderr(ConstString line,
                                             void        *userData
                                            )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;
  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);
  executeIOOpticalDVDDisasterOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,&executeIOInfo->stderrList);
}

/***********************************************************************\
* Name   : executeIOOpticalFormatOutput
* Purpose: process format output
* Input  : storageInfo - storage info
*          line        - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalFormatOutput(StorageInfo *storageInfo,
                                        ConstString line
                                       )
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);

  String s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*?([0-9\\.]+)%.*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    double percentageDone = String_toDouble(s,0,NULL,NULL,0);
    updateVolumeDone(storageInfo,0,percentageDone,0.0);
    updateStorageRunningInfo(storageInfo);
  }
  String_delete(s);
}

/***********************************************************************\
* Name   : executeIOformatStdout
* Purpose: process format stdout output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalFormatStdout(ConstString line,
                                        void        *userData
                                       )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;
  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);
  executeIOOpticalFormatOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,NULL);
}

/***********************************************************************\
* Name   : executeIOOpticalFormatStderr
* Purpose: process format stderr output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalFormatStderr(ConstString line,
                                        void        *userData
                                       )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;
  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);
  executeIOOpticalFormatOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,&executeIOInfo->stderrList);
}

/***********************************************************************\
* Name   : executeIOOpticalBlankOutput
* Purpose: process blank output
* Input  : storageInfo - storage info
*          line        - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalBlankOutput(StorageInfo *storageInfo,
                                       ConstString line
                                      )
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);

  String s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*?([0-9\\.]+)%.*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    double percentageDone = String_toDouble(s,0,NULL,NULL,0);
    updateVolumeDone(storageInfo,0,percentageDone,0.0);
    updateStorageRunningInfo(storageInfo);
  }
  String_delete(s);
}

/***********************************************************************\
* Name   : executeIOOpticalBlankStdout
* Purpose: process blank stdout output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalBlankStdout(ConstString line,
                                       void        *userData
                                      )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;
  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);
  executeIOOpticalBlankOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,NULL);
}

/***********************************************************************\
* Name   : executeIOOpticalBlankStderr
* Purpose: process blank stderr output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalBlankStderr(ConstString line,
                                       void        *userData
                                      )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;
  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);
  executeIOOpticalBlankOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,&executeIOInfo->stderrList);
}

/***********************************************************************\
* Name   : executeIOOpticalGrowisofsOutput
* Purpose: process growisofs output
* Input  : storageInfo - storage info
*          line        - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalGrowIsofsOutput(StorageInfo *storageInfo,
                                           ConstString line
                                          )
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(line != NULL);

  String s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*?([0-9\\.]+)%.*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    double percentageDone = String_toDouble(s,0,NULL,NULL,0);
    updateVolumeDone(storageInfo,0,percentageDone,0.0);
    updateStorageRunningInfo(storageInfo);
  }
  String_delete(s);
}

/***********************************************************************\
* Name   : executeIOOpticalGrowisofsStdout
* Purpose: process growisofs stdout output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalGrowIsofsStdout(ConstString line,
                                           void        *userData
                                          )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;
  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);
  executeIOOpticalGrowIsofsOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,NULL);
}

/***********************************************************************\
* Name   : executeIOOpticalGrowIsofsStderr
* Purpose: process growisofs stderr output
* Input  : line     - line
*          userData - storage file handle variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOpticalGrowIsofsStderr(ConstString line,
                                           void        *userData
                                          )
{
  ExecuteIOInfo *executeIOInfo = (ExecuteIOInfo*)userData;
  assert(executeIOInfo != NULL);
  assert(executeIOInfo->storageInfo != NULL);
  executeIOOpticalGrowIsofsOutput(executeIOInfo->storageInfo,line);
  executeIOOutput(line,&executeIOInfo->stderrList);
}

/*---------------------------------------------------------------------*/

LOCAL Errors StorageOptical_initAll(void)
{
  #ifdef HAVE_ISOFS
    (void)cdio_log_set_handler(libcdioLogCallback);

    int result = iso_init();
    if (result != ISO_SUCCESS)
    {
      (void)cdio_log_set_handler(NULL);
      return ERRORX_(INIT,result,"%s",iso_error_to_msg(result));
    }
  #endif // HAVE_ISOFS

  #ifdef HAVE_BURN
    if (burn_initialize() == 0)
    {
      #ifdef HAVE_ISOFS
        iso_finish();
      #endif // HAVE_ISOFS
      (void)cdio_log_set_handler(NULL);
      return ERROR_INIT;
    }
  #endif // HAVE_BURN

  return ERROR_NONE;
}

LOCAL void StorageOptical_doneAll(void)
{
  #ifdef HAVE_BURN
    burn_finish();
  #endif /* HAVE_ISOFS */
  #ifdef HAVE_ISOFS
    iso_finish();
  #endif /* HAVE_ISOFS */

  #ifdef HAVE_ISO9660
    (void)cdio_log_set_handler(NULL);
  #endif /* HAVE_ISO9660 */
}

LOCAL bool StorageOptical_parseSpecifier(ConstString deviceSpecifier,
                                         ConstString defaultDeviceName,
                                         String      deviceName
                                        )
{
  assert(deviceSpecifier != NULL);
  assert(deviceName != NULL);

  String_clear(deviceName);

  bool result;
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
  assert(storageSpecifier != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));

  // get file to use
  ConstString storageFileName;
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

/***********************************************************************\
* Name   : StorageOptical_getPrintableName
* Purpose: get printable storage name (without password)
* Input  : string           - name variable (can be NULL)
*          storageSpecifier - storage specifier string
*          archiveName      - archive name (can be NULL)
* Output : -
* Return : printable storage name
* Notes  : if archiveName is NULL file name from storageSpecifier is used
\***********************************************************************/

LOCAL void StorageOptical_getPrintableName(String                 string,
                                           const StorageSpecifier *storageSpecifier,
                                           ConstString            archiveName
                                          )
{
  assert(string != NULL);
  assert(storageSpecifier != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));

  // get file to use
  ConstString storageFileName;
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

/***********************************************************************\
* Name   : StorageOptical_init
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

LOCAL Errors StorageOptical_init(StorageInfo      *storageInfo,
                                 const JobOptions *jobOptions
                                )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_CD)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD)
        );

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
  OpticalDisk opticalDisk;
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
     raw cd/dvd/bd data (volumeSize) and cd/dvd/bd image (up to 25G)
     including error correction codes (2x)
  */
  FileSystemInfo fileSystemInfo;
  error = File_getFileSystemInfo(&fileSystemInfo,tmpDirectory);
  if (error != ERROR_NONE)
  {
    Configuration_doneOpticalDiskSettings(&opticalDisk);
    return error;
  }
  uint64 volumeSize    = 0LL;
  uint64 maxMediumSize = 0LL;
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
    printWarning(_("insufficient space in temporary directory '%s' for medium (%.1lf%s free, %.1lf%s recommended)!"),
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
  storageInfo->opticalDisk.write.formatCommand           = String_duplicate(opticalDisk.formatCommand);
  storageInfo->opticalDisk.write.blankCommand            = String_duplicate(opticalDisk.blankCommand);
  storageInfo->opticalDisk.write.writePreProcessCommand  = String_duplicate(opticalDisk.writePreProcessCommand);
  storageInfo->opticalDisk.write.writePostProcessCommand = String_duplicate(opticalDisk.writePostProcessCommand);
  storageInfo->opticalDisk.write.writeCommand            = String_duplicate(opticalDisk.writeCommand);
  storageInfo->opticalDisk.write.writeImageCommand       = String_duplicate(opticalDisk.writeImageCommand);
  storageInfo->opticalDisk.write.steps                   = 0;
  if ((jobOptions != NULL) && jobOptions->formatFlag) storageInfo->opticalDisk.write.steps += 1;  // format
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
  storageInfo->opticalDisk.write.totalSize               = 0LL;

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
    String fileBaseName = File_getBaseName(String_new(),globalOptions.barExecutable,TRUE);
    String destinationFileName = File_appendFileName(String_duplicate(storageInfo->opticalDisk.write.directory),fileBaseName);
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

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;

  // delete files
  String fileName = String_new();
  while (!StringList_isEmpty(&storageInfo->opticalDisk.write.fileNameList))
  {
    StringList_removeFirst(&storageInfo->opticalDisk.write.fileNameList,fileName);
    Errors tmpError = File_delete(fileName,FALSE);
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
  String_delete(storageInfo->opticalDisk.write.formatCommand);
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
* Name   : loadOpticalVolume
* Purpose: load optical volume (close tray)
* Input  : storageInfo - storage info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors loadOpticalVolume(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  Errors error = ERROR_NONE;

  const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
  #ifndef NDEBUG
    const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
    if (debugEmulateBlockDevice != NULL)
    {
      deviceName = debugEmulateBlockDevice;
    }
  #endif

  if (String_isEmpty(storageInfo->opticalDisk.write.loadVolumeCommand))
  {
    #ifdef HAVE_BURN
      #ifndef NDEBUG
      if (debugEmulateBlockDevice == NULL)
      {
      #endif
        uint                  retries = 3;
        int                   profileNumber;
        char                  profileName[80];
        bool                  profileOK;
        enum burn_disc_status burnDiscStatus;
        uint64                writableDiscSectors;
        do
        {
          struct burn_drive_info *burnDriveInfo;
          if (burn_drive_scan_and_grab(&burnDriveInfo,(char*)deviceName,1) == 1)  // Note: close tray+lock
          {
            // wait for drive
            while (burn_drive_get_status(burnDriveInfo->drive,NULL) != BURN_DRIVE_IDLE)
            {
              Misc_mdelay(100);
            }

            // get media info
            profileOK           = (burn_disc_get_profile(burnDriveInfo->drive,&profileNumber,profileName) == 1);
            burnDiscStatus      = burn_disc_get_status(burnDriveInfo->drive);
            writableDiscSectors = (uint64)burn_disc_available_space(burnDriveInfo->drive, NULL) / ISO_SECTOR_SIZE;

            burn_drive_release(burnDriveInfo->drive,0);  // Note: keep tray closed, just unlock

            burn_drive_info_free(burnDriveInfo);

            if (   (burnDiscStatus != BURN_DISC_FULL)
                && (burnDiscStatus != BURN_DISC_APPENDABLE)
                && (burnDiscStatus != BURN_DISC_BLANK)
               )
            {
              Misc_mdelay(2000);
              retries--;
            }
          }
          else
          {
            error = ERRORX_(OPEN_OPTICAL_DRIVE,errno,"%s",strerror(errno));
          }
        }
        while (   (error == ERROR_NONE)
               && (burnDiscStatus != BURN_DISC_FULL)
               && (burnDiscStatus != BURN_DISC_APPENDABLE)
               && (burnDiscStatus != BURN_DISC_BLANK)
               && (retries > 0)
              );
        if (error == ERROR_NONE)
        {
          switch (burnDiscStatus)
          {
            case BURN_DISC_FULL:
              if (profileOK && !stringIsEmpty(profileName))
              {
                printInfo(1,
                          "Loaded medium '%s', full, %"PRIu64" sectors\n",
                          profileName,
                          writableDiscSectors
                         );
              }
              else
              {
                printInfo(1,
                          "Loaded medium 0x%04X, full, %"PRIu64" sectors\n",
                          profileNumber,
                          writableDiscSectors
                         );
              }
              break;
            case BURN_DISC_APPENDABLE:
              if (profileOK && !stringIsEmpty(profileName))
              {
                printInfo(1,
                          "Loaded medium '%s', appendable, %"PRIu64" sectors\n",
                          profileName,
                          writableDiscSectors
                         );
              }
              else
              {
                printInfo(1,
                          "Loaded medium 0x%04X, appendable, %"PRIu64" sectors\n",
                          profileNumber,
                          writableDiscSectors
                         );
              }
              break;
            case BURN_DISC_BLANK:
              if (profileOK && !stringIsEmpty(profileName))
              {
                printInfo(1,
                          "Loaded medium '%s', blank, %"PRIu64" sectors\n",
                          profileName,
                          writableDiscSectors
                         );
              }
              else
              {
                printInfo(1,
                          "Loaded medium 0x%04X, blank, %"PRIu64" sectors\n",
                          profileNumber,
                          writableDiscSectors
                         );
              }
              break;
            case BURN_DISC_EMPTY:
            default:
              printError("no medium present or not recognized");
              error = ERROR_OPTICAL_DISK_NOT_FOUND;
              break;
          }
        }
      #ifndef NDEBUG
      }
      #endif
    #else
      error = ERROR_FUNCTION_NOT_SUPPORTED;
    #endif
  }
  else
  {
    TextMacros (textMacros,2);
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_CSTRING("device",deviceName,                      NULL);
      TEXT_MACRO_X_UINT   ("number",storageInfo->volumeRequestNumber,NULL);
    }
    StringList stderrList;
    StringList_init(&stderrList);
    String     commandLine = String_new();
    error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.loadVolumeCommand),
                                textMacros.data,
                                textMacros.count,
                                commandLine,
                                CALLBACK_(executeIOOutput,&stderrList),
                                CALLBACK_(executeIOOutput,&stderrList),
                                WAIT_FOREVER
                               );
    if (error == ERROR_NONE)
    {
      Misc_mdelay(OPTICAL_LOAD_VOLUME_DELAY_TIME);

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

/***********************************************************************\
* Name   : unloadOpticalVolume
* Purpose: unload optical volume (open tray)
* Input  : storageInfo - storageInfo
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors unloadOpticalVolume(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  Errors error = ERROR_NONE;

  const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
  #ifndef NDEBUG
    const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
    if (debugEmulateBlockDevice != NULL)
    {
      deviceName = debugEmulateBlockDevice;
    }
  #endif

  if (String_isEmpty(storageInfo->opticalDisk.write.unloadVolumeCommand))
  {
    #ifdef HAVE_BURN
      #ifndef NDEBUG
      if (debugEmulateBlockDevice == NULL)
      {
      #endif
        struct burn_drive_info *burnDriveInfo;
        if (burn_drive_scan_and_grab(&burnDriveInfo,(char*)deviceName,0) == 1)  // Note: just lock
        {
          // wait for drive
          while (burn_drive_get_status(burnDriveInfo->drive,NULL) != BURN_DRIVE_IDLE)
          {
            Misc_mdelay(100);
          }

          burn_drive_release(burnDriveInfo->drive,1);  // Note: 1 = unlock+eject tray

          burn_drive_info_free(burnDriveInfo);
        }
        else
        {
          error = ERRORX_(OPEN_OPTICAL_DRIVE,errno,"%s",strerror(errno));
        }
      #ifndef NDEBUG
      }
      #endif
    #else
      error = ERROR_FUNCTION_NOT_SUPPORTED;
    #endif
  }
  else
  {
    Misc_mdelay(OPTICAL_UNLOAD_VOLUME_DELAY_TIME);

    TextMacros (textMacros,2);
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_CSTRING("device",deviceName,                      NULL);
      TEXT_MACRO_X_UINT   ("number",storageInfo->volumeRequestNumber,NULL);
    }
    StringList stderrList;
    StringList_init(&stderrList);
    String     commandLine = String_new();
    error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.unloadVolumeCommand),
                                textMacros.data,
                                textMacros.count,
                                commandLine,
                                CALLBACK_(executeIOOutput,&stderrList),
                                CALLBACK_(executeIOOutput,&stderrList),
                                WAIT_FOREVER
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
  }

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
  Errors error = ERROR_NONE;

  const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
  #ifndef NDEBUG
    const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
    if (debugEmulateBlockDevice != NULL)
    {
      deviceName = debugEmulateBlockDevice;
    }
  #endif

  // unload medium
  if (error == ERROR_NONE)
  {
    if (   (storageInfo->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
        || (storageInfo->volumeState == STORAGE_VOLUME_STATE_LOADED)
       )
    {
      error = unloadOpticalVolume(storageInfo);
      if (error == ERROR_NONE)
      {
        printInfo(1,"Unloaded medium #%d\n",storageInfo->volumeNumber);
      }
      else
      {
        printInfo(1,"Unload medium #%d failed (error: %s)\n",storageInfo->volumeNumber, Error_getText(error));
      }

      storageInfo->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
    }
  }

  // request new medium
  bool                        volumeRequestedFlag        = FALSE;
  StorageVolumeRequestResults storageRequestVolumeResult = STORAGE_VOLUME_REQUEST_RESULT_UNKNOWN;
  if (error == ERROR_NONE)
  {
    if      (storageInfo->volumeRequestFunction != NULL)
    {
      // request volume via callback
      volumeRequestedFlag = TRUE;

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
          error = unloadOpticalVolume(storageInfo);
          if (error == ERROR_NONE)
          {
            printInfo(1,"Unloaded medium #%d\n",storageInfo->volumeNumber);
          }
          else
          {
            printInfo(1,"Unload medium #%d failed (error: %s)\n",storageInfo->volumeNumber, Error_getText(error));
          }
        }
      }
      while (storageRequestVolumeResult == STORAGE_VOLUME_REQUEST_RESULT_UNLOAD);

      storageInfo->volumeState = STORAGE_VOLUME_STATE_WAIT;
    }
    else if (storageInfo->opticalDisk.write.requestVolumeCommand != NULL)
    {
      // request volume via external command
      volumeRequestedFlag = TRUE;

      TextMacros (textMacros,2);
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING("device",deviceName,                      NULL);
        TEXT_MACRO_X_UINT  ("number",storageInfo->volumeRequestNumber,NULL);
      }

      // request new volume via external command
      printInfo(1,"Request new medium #%d...",storageInfo->volumeRequestNumber);
      if (Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.requestVolumeCommand),
                              textMacros.data,
                              textMacros.count,
                              NULL, // commandLine
                              CALLBACK_(executeIOOutput,NULL),
                              CALLBACK_(executeIOOutput,NULL),
                              (globalOptions.commandTimeout > 0) ? (long)globalOptions.commandTimeout : WAIT_FOREVER
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
      // request volume via console
      if (storageInfo->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
      {
        if (waitFlag)
        {
          volumeRequestedFlag = TRUE;

          printInfo(0,
                    "Please insert medium #%d into drive '%s' and press ENTER to continue\n",
                    storageInfo->volumeRequestNumber,
                    deviceName
                   );
          Misc_waitEnter();

          storageRequestVolumeResult = STORAGE_VOLUME_REQUEST_RESULT_LOAD;
        }
        else
        {
          printInfo(0,
                    "Please insert medium #%d into drive '%s'\n",
                    storageInfo->volumeRequestNumber,
                    deviceName
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
  }

  if (error == ERROR_NONE)
  {
    if (volumeRequestedFlag)
    {
      switch (storageRequestVolumeResult)
      {
        case STORAGE_VOLUME_REQUEST_RESULT_LOAD:
          // load medium, then sleep a short time to give hardware time for reading medium information
          error = loadOpticalVolume(storageInfo);
          if (error != ERROR_NONE)
          {
            printInfo(1,"Load medium failed (error: %s)\n",Error_getText(error));
          }

          // store new medium number
          storageInfo->volumeNumber = storageInfo->volumeRequestNumber;

          // update running info
          storageInfo->progress.volumeNumber = storageInfo->volumeNumber;
          updateStorageRunningInfo(storageInfo);

          storageInfo->volumeState = STORAGE_VOLUME_STATE_LOADED;
          error = ERROR_NONE;
          break;
        case STORAGE_VOLUME_REQUEST_RESULT_ABORTED:
          error = ERROR_ABORTED;
          break;
        default:
          error = ERROR_LOAD_VOLUME_FAIL;
          break;
      }
    }
  }

  return error;
}

/***********************************************************************\
* Name   : isFormatted
* Purpose: check if disc is formated
* Input  : storageInfo - storage info
* Output : -
* Return : TRUE iff disc is formatted
* Notes  : -
\***********************************************************************/

LOCAL bool isFormatted(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);

  bool result = FALSE;

  const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
  #ifndef NDEBUG
    const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
    if (debugEmulateBlockDevice != NULL)
    {
      deviceName = debugEmulateBlockDevice;
    }
  #endif

  #ifdef HAVE_BURN
    // aquire drive
    #ifndef NDEBUG
    if (debugEmulateBlockDevice == NULL)
    {
    #endif
      struct burn_drive_info *burnDriveInfo;
      if (burn_drive_scan_and_grab(&burnDriveInfo,(char*)deviceName,1) == 1)
      {
        // wait for drive
        while (burn_drive_get_status(burnDriveInfo->drive,NULL) != BURN_DRIVE_IDLE)
        {
          Misc_mdelay(100);
        }

        enum burn_disc_status burnDiscStatus;
        burnDiscStatus = burn_disc_get_status(burnDriveInfo->drive);
        if (burnDiscStatus == BURN_DISC_BLANK)
        {
          int      status;
          off_t    size;
          unsigned blockLengthSpareAreaSize;
          int      formatCount;
          if (burn_disc_get_formats(burnDriveInfo->drive,&status,&size,&blockLengthSpareAreaSize,&formatCount) == 1)
          {
            result = (status == BURN_FORMAT_IS_FORMATTED);
          }
        }

        burn_drive_release(burnDriveInfo->drive,0);  // Note: keep tray closed, just unlock

        burn_drive_info_free(burnDriveInfo);
      }
    #ifndef NDEBUG
    }
    #endif
  #endif

  return result;
}

/***********************************************************************\
* Name   : getWritebaleDiscSectors
* Purpose: get number of writable sectors (2048 bytes) on medium
* Input  : storageInfo - storage info
* Output : writableDiscSectors - writeable disc sectors (2028 bytes)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getWritebaleDiscSectors(uint64 *writableDiscSectors, const StorageInfo *storageInfo)
{
  assert(writableDiscSectors != NULL);
  assert(storageInfo != NULL);

  Errors error = ERROR_NONE;

  const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
  #ifndef NDEBUG
    const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
    if (debugEmulateBlockDevice != NULL)
    {
      deviceName = debugEmulateBlockDevice;
    }
  #endif

  #ifdef HAVE_BURN
    // aquire drive
    #ifndef NDEBUG
    if (debugEmulateBlockDevice == NULL)
    {
    #endif
      struct burn_drive_info *burnDriveInfo;
      if (burn_drive_scan_and_grab(&burnDriveInfo,(char*)deviceName,1) == 1)
      {
        // wait for drive
        while (burn_drive_get_status(burnDriveInfo->drive,NULL) != BURN_DRIVE_IDLE)
        {
          Misc_mdelay(100);
        }

        // get writable disc sectors
        (*writableDiscSectors) = (uint64)burn_disc_available_space(burnDriveInfo->drive, NULL) / ISO_SECTOR_SIZE;

        burn_drive_release(burnDriveInfo->drive,0);  // Note: keep tray closed, just unlock

        burn_drive_info_free(burnDriveInfo);
      }
      else
      {
        error = ERRORX_(OPEN_OPTICAL_DRIVE,errno,"%s",strerror(errno));
      }
    #ifndef NDEBUG
    }
    #endif
  #else
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif

  return error;
}

/***********************************************************************\
* Name   : formatVolume
* Purpose: format volume
* Input  : storageInfo          - storage info
*          writeableDiscSectors - number of writable sectors on disc
*                                 (2048 bytes)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors formatVolume(StorageInfo *storageInfo, uint64 writeableDiscSectors)
{
  assert(storageInfo != NULL);

  Errors error = ERROR_NONE;

  if (storageInfo->jobOptions->formatFlag)
  {
    messageSet(&storageInfo->progress.message,MESSAGE_CODE_FORMAT_VOLUME,NULL);
    updateStorageRunningInfo(storageInfo);

    const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
    #ifndef NDEBUG
      const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
      if (debugEmulateBlockDevice != NULL)
      {
        deviceName = debugEmulateBlockDevice;
      }
    #endif

    printInfo(1,"Format volume #%u...",storageInfo->opticalDisk.write.number);
    if (String_isEmpty(storageInfo->opticalDisk.write.formatCommand))
    {
      #ifdef HAVE_BURN
        #ifndef NDEBUG
        if (debugEmulateBlockDevice == NULL)
        {
        #endif
          // aquire drive
          struct burn_drive_info *burnDriveInfo;
          if (burn_drive_scan_and_grab(&burnDriveInfo,(char*)deviceName,1) == 1)
          {
            setMessageSeveritity(globalOptions.verboseLevel);

            // wait for drive
            while (burn_drive_get_status(burnDriveInfo->drive,NULL) != BURN_DRIVE_IDLE)
            {
              libisofsPollMessages();
              libburnPollMessages();
              Misc_mdelay(100);
            }

            // format
            burn_set_signal_handling(NULL,NULL,0x30);
            burn_disc_format(burnDriveInfo->drive,0,0);
            Misc_mdelay(1*MS_PER_S);
            struct burn_progress burnProgress;
//while (burn_drive_get_status(drive, &progress) == BURN_DRIVE_FORMATTING) {
            while (burn_drive_get_status(burnDriveInfo->drive,&burnProgress) != BURN_DRIVE_IDLE)
            {
              libisofsPollMessages();
              libburnPollMessages();
              Misc_mdelay(100);
            }
            if (burn_is_aborting(0) > 0)
            {
              error = ERROR_ABORTED;
            }
            burn_set_signal_handling(NULL,NULL,0x00);

            burn_drive_release(burnDriveInfo->drive,0);  // Note: keep tray closed, just unlock

            burn_drive_info_free(burnDriveInfo);
          }
          else
          {
            error = ERRORX_(OPEN_OPTICAL_DRIVE,errno,"%s",strerror(errno));
          }
        #ifndef NDEBUG
        }
        #endif

        if (error == ERROR_NONE)
        {
          printInfo(1,"OK\n");
          logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Formatted volume #%u",storageInfo->volumeNumber);
        }
        else
        {
          printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
          logMessage(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Format volume #%u fail: %s",
                     storageInfo->volumeNumber,
                     Error_getText(error)
                    );
        }
      #else
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif
    }
    else
    {
      // init macros
      uint j = Thread_getNumberOfCores();
      TextMacros (textMacros,8);
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_CSTRING("device",       deviceName,                                                                         NULL);
        TEXT_MACRO_X_UINT64 ("size",         storageInfo->opticalDisk.write.volumeSize,                                          NULL);
        TEXT_MACRO_X_UINT64 ("sectors",      (storageInfo->opticalDisk.write.volumeSize + ISO_SECTOR_SIZE - 1) / ISO_SECTOR_SIZE,NULL);
        TEXT_MACRO_X_UINT64 ("discSectors",  writeableDiscSectors,                                                               NULL);
        TEXT_MACRO_X_UINT64 ("discSectors97",(writeableDiscSectors * 97) / 100,                                                  NULL);
        TEXT_MACRO_X_UINT   ("number",       storageInfo->volumeNumber,                                                          NULL);
        TEXT_MACRO_X_UINT   ("j",            j,                                                                                  NULL);
        TEXT_MACRO_X_UINT   ("j1",           (j > 1) ? j-1 : 1,                                                                  NULL);
      }

      // init variables
      String commandLine = String_new();
      ExecuteIOInfo executeIOInfo;
      initOpticalExecuteIOInfo(&executeIOInfo,storageInfo);

      // execute external blank command
      error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.formatCommand),
                                  textMacros.data,
                                  textMacros.count,
                                  commandLine,
                                  CALLBACK_(executeIOOpticalFormatStdout,&executeIOInfo),
                                  CALLBACK_(executeIOOpticalFormatStderr,&executeIOInfo),
                                  WAIT_FOREVER
                                 );
      if (error == ERROR_NONE)
      {
        printInfo(1,"OK\n");
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Formatted volume #%u",storageInfo->volumeNumber);
      }
      else
      {
        printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
        logMessage(storageInfo->logHandle,
                   LOG_TYPE_ERROR,
                   "Format volume #%u fail: %s",
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

      // free resources
      doneOpticalExecuteIOInfo(&executeIOInfo);
      String_delete(commandLine);
    }

    updateVolumeDone(storageInfo,1,0.0,0);
    updateStorageRunningInfo(storageInfo);
  }

  return error;
}

/***********************************************************************\
* Name   : blankVolume
* Purpose: blank volume
* Input  : storageInfo          - storage info
*          writeableDiscSectors - number of writable sectors on disc
*                                 (2048 bytes)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors blankVolume(StorageInfo *storageInfo, uint64 writeableDiscSectors)
{
  assert(storageInfo != NULL);

  Errors error = ERROR_NONE;

  if (storageInfo->jobOptions->blankFlag)
  {
    messageSet(&storageInfo->progress.message,MESSAGE_CODE_BLANK_VOLUME,NULL);
    updateStorageRunningInfo(storageInfo);

    const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
    #ifndef NDEBUG
      const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
      if (debugEmulateBlockDevice != NULL)
      {
        deviceName = debugEmulateBlockDevice;
      }
    #endif

    printInfo(1,"Blank volume #%u...",storageInfo->opticalDisk.write.number);
    if (String_isEmpty(storageInfo->opticalDisk.write.blankCommand))
    {
      #ifdef HAVE_BURN
        #ifndef NDEBUG
        if (debugEmulateBlockDevice == NULL)
        {
        #endif
          // aquire drive
          struct burn_drive_info *burnDriveInfo;
          if (burn_drive_scan_and_grab(&burnDriveInfo,(char*)deviceName,1) == 1)
          {
            setMessageSeveritity(globalOptions.verboseLevel);

            // wait for drive
            while (burn_drive_get_status(burnDriveInfo->drive,NULL) != BURN_DRIVE_IDLE)
            {
              libisofsPollMessages();
              libburnPollMessages();
              Misc_mdelay(100);
            }

            // blank
            burn_set_signal_handling(NULL,NULL,0x30);
            burn_disc_erase(burnDriveInfo->drive, 1);
            Misc_mdelay(1*MS_PER_S);
            struct burn_progress burnProgress;
            while (burn_drive_get_status(burnDriveInfo->drive,&burnProgress) != BURN_DRIVE_IDLE)
            {
              if (burnProgress.sectors > 0)
              {
                double percentageDone = ((double)burnProgress.sector*100.0)/(double)burnProgress.sectors;
                switch (globalOptions.runMode)
                {
                  case RUN_MODE_INTERACTIVE:
                    printInfo(1,"%3u%%\b\b\b\b",(uint)percentageDone);
                    break;
                  case RUN_MODE_BATCH:
                  case RUN_MODE_SERVER:
                    updateVolumeDone(storageInfo,0,percentageDone,0.0);
                    updateStorageRunningInfo(storageInfo);
                    break;
                }
              }

              libisofsPollMessages();
              libburnPollMessages();
              Misc_mdelay(100);
            }
            switch (globalOptions.runMode)
            {
              case RUN_MODE_INTERACTIVE:
                printInfo(1,"    \b\b\b\b");
                break;
              case RUN_MODE_BATCH:
              case RUN_MODE_SERVER:
                break;
            }
            if (burn_is_aborting(0) > 0)
            {
              error = ERROR_ABORTED;
            }
            burn_set_signal_handling(NULL,NULL,0x00);

            enum burn_disc_status burnDiscStatus = burn_disc_get_status(burnDriveInfo->drive);
            if (   (burnDiscStatus != BURN_DISC_BLANK)
                && (burnDiscStatus != BURN_DISC_APPENDABLE)
               )
            {
              error = ERRORX_(WRITE_OPTICAL_DISK,burnDiscStatus,"disc not writable");
            }

            burn_drive_release(burnDriveInfo->drive,0);  // Note: keep tray closed, just unlock

            burn_drive_info_free(burnDriveInfo);
          }
          else
          {
            error = ERRORX_(OPEN_OPTICAL_DRIVE,errno,"%s",strerror(errno));
          }
        #ifndef NDEBUG
        }
        #endif

        if (error == ERROR_NONE)
        {
          printInfo(1,"OK\n");
          logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Blanked volume #%u",storageInfo->volumeNumber);
        }
        else
        {
          printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
          logMessage(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Blank volume #%u fail: %s",
                     storageInfo->volumeNumber,
                     Error_getText(error)
                    );
        }
      #else
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif
    }
    else
    {
      // init macros
      uint j = Thread_getNumberOfCores();
      TextMacros (textMacros,8);
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_CSTRING("device",       deviceName,                                                                         NULL);
        TEXT_MACRO_X_UINT64 ("size",         storageInfo->opticalDisk.write.volumeSize,                                          NULL);
        TEXT_MACRO_X_UINT64 ("sectors",      (storageInfo->opticalDisk.write.volumeSize + ISO_SECTOR_SIZE - 1) / ISO_SECTOR_SIZE,NULL);
        TEXT_MACRO_X_UINT64 ("discSectors",  writeableDiscSectors,                                                               NULL);
        TEXT_MACRO_X_UINT64 ("discSectors97",(writeableDiscSectors * 97) / 100,                                                  NULL);
        TEXT_MACRO_X_UINT   ("number",       storageInfo->volumeNumber,                                                          NULL);
        TEXT_MACRO_X_UINT   ("j",            j,                                                                                  NULL);
        TEXT_MACRO_X_UINT   ("j1",           (j > 1) ? j-1 : 1,                                                                  NULL);
      }

      // init variables
      String commandLine = String_new();
      ExecuteIOInfo executeIOInfo;
      initOpticalExecuteIOInfo(&executeIOInfo,storageInfo);

      // execute external blank command
      error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.blankCommand),
                                  textMacros.data,
                                  textMacros.count,
                                  commandLine,
                                  CALLBACK_(executeIOOpticalBlankStdout,&executeIOInfo),
                                  CALLBACK_(executeIOOpticalBlankStderr,&executeIOInfo),
                                  WAIT_FOREVER
                                 );
      if (error == ERROR_NONE)
      {
        printInfo(1,"OK\n");
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Blanked volume #%u",storageInfo->volumeNumber);
      }
      else
      {
        printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
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

      // free resources
      doneOpticalExecuteIOInfo(&executeIOInfo);
      String_delete(commandLine);
    }

    updateVolumeDone(storageInfo,1,0.0,0);
    updateStorageRunningInfo(storageInfo);
  }

  return error;
}

/***********************************************************************\
* Name   : createISOSourceFromDirectory
* Purpose: create ISO from directory
* Input  : isoSourceHandle - burn source handle variable
*          directory       - directory with files for ISO
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createISOSourceFromDirectory(ISOSourceHandle *isoSourceHandle, ConstString directory)
{
  assert(isoSourceHandle != NULL);
  assert(directory != NULL);

  Errors error = ERROR_NONE;

  isoSourceHandle->fileHandle = -1;

  int result;

  // create new ISO image
  result = iso_image_new("Backup", &isoSourceHandle->isoImage);
  if (result == ISO_SUCCESS)
  {
    assert(isoSourceHandle->isoImage != NULL);

    // read ISO image content from dirctory
    iso_tree_set_follow_symlinks(isoSourceHandle->isoImage,1);
    iso_tree_set_ignore_hidden(isoSourceHandle->isoImage,1);
    iso_tree_set_ignore_special(isoSourceHandle->isoImage,1);
    result = iso_tree_add_dir_rec(isoSourceHandle->isoImage,
                                  iso_image_get_root(isoSourceHandle->isoImage),
                                  String_cString(directory)
                                 );
    if (result == ISO_SUCCESS)
    {
      // set ISO options
      result = iso_write_opts_new(&isoSourceHandle->isoWriteOpts, 0);
      if (result == ISO_SUCCESS)
      {
        assert(isoSourceHandle->isoWriteOpts != NULL);
        iso_write_opts_set_iso_level(isoSourceHandle->isoWriteOpts,2);
        iso_write_opts_set_rockridge(isoSourceHandle->isoWriteOpts,1);
        iso_write_opts_set_joliet(isoSourceHandle->isoWriteOpts,0);
        iso_write_opts_set_iso1999(isoSourceHandle->isoWriteOpts,0);

        // create burn source from ISO image
        result = iso_image_create_burn_source(isoSourceHandle->isoImage,
                                              isoSourceHandle->isoWriteOpts,
                                              &isoSourceHandle->burnSource
                                             );
        if (result == ISO_SUCCESS)
        {
          assert(isoSourceHandle->burnSource != NULL);
          assert(isoSourceHandle->burnSource->read == NULL);
          assert(isoSourceHandle->burnSource->version >= 1);
        }
        else
        {
          iso_write_opts_free(isoSourceHandle->isoWriteOpts);
        }
      }
      else
      {
        error = ERRORX_(CREATE_ISO9660,result,"%s",iso_error_to_msg(result));
      }
    }
    else
    {
      iso_image_unref(isoSourceHandle->isoImage);
      error = ERRORX_(CREATE_ISO9660,result,"%s",iso_error_to_msg(result));
    }
  }
  else
  {
    error = ERRORX_(CREATE_ISO9660,result,"%s",iso_error_to_msg(result));
  }

  return error;
}

/***********************************************************************\
* Name   : createISOSourceFromDirectory
* Purpose: create ISO from image
* Input  : isoSourceHandle - burn source handle variable
*          imageFileName   - image file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createISOSourceFromImage(ISOSourceHandle *isoSourceHandle, ConstString imageFileName)
{
  assert(isoSourceHandle != NULL);
  assert(imageFileName != NULL);

  Errors error = ERROR_NONE;

  isoSourceHandle->isoImage     = NULL;
  isoSourceHandle->isoWriteOpts = NULL;

  // get image size
  FileInfo fileInfo;
  File_getInfo(&fileInfo,imageFileName);

  // open image file
  isoSourceHandle->fileHandle = open(String_cString(imageFileName),O_RDONLY);
  if (isoSourceHandle->fileHandle != -1)
  {
    // create burn source from image file
    isoSourceHandle->burnSource = burn_fd_source_new(isoSourceHandle->fileHandle,-1,fileInfo.size);
    if (isoSourceHandle->burnSource != NULL)
    {
      assert(isoSourceHandle->burnSource != NULL);
      assert(isoSourceHandle->burnSource->read != NULL);
      assert(isoSourceHandle->burnSource->data != NULL);
    }
    else
    {
      close(isoSourceHandle->fileHandle);
      error = ERRORX_(WRITE_OPTICAL_DISK,0,"initialize write source");
    }
  }
  else
  {
    error = ERRORX_(OPEN_FILE,errno,"%s",strerror(errno));
  }

  return error;
}

/***********************************************************************\
* Name   : deleteBurnSource
* Purpose: delete burn source
* Input  : isoSourceHandle - burn source to delete
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deleteISOSource(ISOSourceHandle *isoSourceHandle)
{
  assert(isoSourceHandle != NULL);

  burn_source_free(isoSourceHandle->burnSource);
  if (isoSourceHandle->isoWriteOpts != NULL)
  {
    iso_write_opts_free(isoSourceHandle->isoWriteOpts);
  }
  if (isoSourceHandle->isoImage != NULL)
  {
    iso_image_unref(isoSourceHandle->isoImage);
  }
  if (isoSourceHandle->fileHandle != -1)
  {
    close(isoSourceHandle->fileHandle);
  }
}

#ifdef HAVE_BURN
/***********************************************************************\
* Name   : writeISOSource
* Purpose: write ISO source handler with FIFO
* Input  : storageInfo     - storage info
*          burnDriveInfo   - burn drive info
*          isoSourceHandle - ISO source handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeISOSource(StorageInfo            *storageInfo,
                            struct burn_drive_info *burnDriveInfo,
                            ISOSourceHandle        *isoSourceHandle
                           )
{
  assert(storageInfo != NULL);
  assert(burnDriveInfo != NULL);
  assert(isoSourceHandle != NULL);

  Errors error = ERROR_NONE;

  struct burn_disc *burnDisc = burn_disc_create();
  assert(burnDisc != NULL);

  struct burn_session *burnSession = burn_session_create();
  assert(burnSession != NULL);
  burn_disc_add_session(burnDisc,burnSession,BURN_POS_END);

  struct burn_track   *burnTrack   = burn_track_create();
  assert(burnTrack != NULL);
  burn_track_define_data(burnTrack,0,ISO_PADDING,1,BURN_MODE1);

  struct burn_source *burnFifoSource = burn_fifo_source_new(isoSourceHandle->burnSource,
                                                            ISO_SECTOR_SIZE,
                                                            ISO_FIFO_SIZE,
                                                            0
                                                           );
  if (burnFifoSource != NULL)
  {
    enum burn_source_status burnSourceStatus;
    burnSourceStatus = burn_track_set_source(burnTrack,
                                             burnFifoSource
                                            );
    if (burnSourceStatus == BURN_SOURCE_OK)
    {
      if (burn_session_add_track(burnSession,
                                 burnTrack,
                                 BURN_POS_END
                                ) == 1
         )
      {

        // set burn options
        struct burn_write_opts *burnWriteOptions = burn_write_opts_new(burnDriveInfo->drive);
        if (burnWriteOptions != NULL)
        {
          burn_write_opts_set_perform_opc(burnWriteOptions, 0);
// TODO: command line options
          burn_write_opts_set_multi(burnWriteOptions, 0);
//if (simulate_burn) printf("\n*** Will TRY to SIMULATE burning ***\n\n");
          burn_write_opts_set_simulate(burnWriteOptions,0);
// TODO: command line options
          burn_drive_set_speed(burnDriveInfo->drive,0,0);
          burn_write_opts_set_underrun_proof(burnWriteOptions,1);
          char burnReasons[BURN_REASONS_LEN];
          if (burn_write_opts_auto_write_type(burnWriteOptions,burnDisc,burnReasons,0) != BURN_WRITE_NONE)
          {
            setMessageSeveritity(globalOptions.verboseLevel);

            // start write image
            burn_set_signal_handling(NULL,NULL,0x30);
            burn_disc_write(burnWriteOptions,burnDisc);

            // wait until drive spawned up
            while (   !Storage_isAborted(storageInfo)
                   && (burn_drive_get_status(burnDriveInfo->drive,NULL) == BURN_DRIVE_SPAWNING)
                  )
            {
              libisofsPollMessages();
              libburnPollMessages();

              Misc_mdelay(250);
            }

            // write image
            TimeoutInfo          progressInfoTimeout;
            Misc_initTimeout(&progressInfoTimeout, 5*MS_PER_SECOND);
            uint64               lastSector          = 0;
            uint64               lastSectorTimestamp = Misc_getTimestamp();
            struct burn_progress burnProgress;
            while (   !Storage_isAborted(storageInfo)
                   && (burn_drive_get_status(burnDriveInfo->drive,&burnProgress) != BURN_DRIVE_IDLE)
                  )
            {
              // calculate percentage done
              double percentageDone = 0.0;
              if (burnProgress.sectors > 0)
              {
                percentageDone = ((double)burnProgress.sector*100.0)/(double)burnProgress.sectors;
              }

              // get buffer fifo level
              size_t fifoLevel = 0;
              if (burnProgress.track >= 0 && burnProgress.track < 1)
              {
                int  size;
                int  bytesFree;
                char *statusText;
                if (burn_fifo_inquire_status(burnFifoSource,
                                             &size,
                                             &bytesFree,
                                             &statusText
                                            ) >= 0
                   )
                {
                  fifoLevel = (size_t)(100-(bytesFree*100)/size);
                }
              }

              // calculate writing speed
              uint64 timestamp = Misc_getTimestamp();
              size_t bytesPerSecond = (timestamp > lastSectorTimestamp)
                                        ? (size_t)(((burnProgress.sector - lastSector) * ISO_SECTOR_SIZE * US_PER_S) / (timestamp - lastSectorTimestamp))
                                        : 0;
              lastSector          = burnProgress.sector;
              lastSectorTimestamp = timestamp;

              // output progress
              switch (globalOptions.runMode)
              {
                case RUN_MODE_INTERACTIVE:
                  if (Misc_isTimeout(&progressInfoTimeout))
                  {
                    printInfo(1,"Burn %5.1lfMiB/s %5.1lf%% (buffer: %3u%%)\n",bytesPerSecond/(double)MB,percentageDone,fifoLevel);
                    Misc_restartTimeout(&progressInfoTimeout,0);
                  }
                  break;
                case RUN_MODE_BATCH:
                case RUN_MODE_SERVER:
                  updateVolumeDone(storageInfo,0,percentageDone,bytesPerSecond);
                  updateStorageRunningInfo(storageInfo);
                  break;
              }

              libisofsPollMessages();
              libburnPollMessages();

              Misc_mdelay(250);
            }

            if      (Storage_isAborted(storageInfo) || (burn_is_aborting(0) < 0))
            {
              burn_drive_cancel(burnDriveInfo->drive);
              error = ERROR_ABORTED;
            }
            else if (burn_drive_wrote_well(burnDriveInfo->drive) != 1)
            {
              error = ERROR_WRITE_OPTICAL_DISK;
            }
            burn_set_signal_handling(NULL,NULL,0x00);

            Misc_doneTimeout(&progressInfoTimeout);
          }
          else
          {
            error = ERRORX_(WRITE_OPTICAL_DISK,0,"%s",burnReasons);
          }
          burn_write_opts_free(burnWriteOptions);
        }
        else
        {
          error = ERRORX_(WRITE_OPTICAL_DISK,0,"initialize write options");
        }
      }
      else
      {
        error = ERRORX_(WRITE_OPTICAL_DISK,0,"add track");
      }
    }
    else
    {
      error = ERRORX_(WRITE_OPTICAL_DISK,burnSourceStatus,"initialize write track");
    }
    burn_source_free(burnFifoSource);
  }
  else
  {
    error = ERRORX_(WRITE_OPTICAL_DISK,0,"initialize write FIFO");
  }

  burn_track_free(burnTrack);
  burn_session_free(burnSession);
  burn_disc_free(burnDisc);

  return error;
}
#endif

/***********************************************************************\
* Name   : createISOImage
* Purpose: create ISO9660 image file
* Input  : storageInfo          - storage info
*          imageFileName        - ISO9660 image file name
*          writeableDiscSectors - number of writable sectors on disc
*                                 (2048 bytes)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createISOImage(StorageInfo *storageInfo,
                            ConstString imageFileName,
                            uint64      writeableDiscSectors
                           )
{
  assert(storageInfo != NULL);
  assert(imageFileName != NULL);

  Errors error = ERROR_NONE;

  const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
  #ifndef NDEBUG
    const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
    if (debugEmulateBlockDevice != NULL)
    {
      deviceName = debugEmulateBlockDevice;
    }
  #endif

  ExecuteIOInfo executeIOInfo;
  initOpticalExecuteIOInfo(&executeIOInfo,storageInfo);

  // pre-processing
  if (error == ERROR_NONE)
  {
    messageSet(&storageInfo->progress.message,MESSAGE_CODE_CREATE_IMAGE,NULL);
    updateStorageRunningInfo(storageInfo);

    if (!String_isEmpty(storageInfo->opticalDisk.write.imagePreProcessCommand))
    {
      printInfo(1,"Image pre-processing of volume #%u...",storageInfo->volumeNumber);

      // init macros
      uint j = Thread_getNumberOfCores();
      TextMacros (textMacros,10);
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_CSTRING("device",       deviceName,                              NULL);
        TEXT_MACRO_X_STRING ("directory",    storageInfo->opticalDisk.write.directory,NULL);
        TEXT_MACRO_X_STRING ("image",        imageFileName,                           NULL);
        TEXT_MACRO_X_UINT64 ("size",         0,                                       NULL);
        TEXT_MACRO_X_UINT64 ("sectors",      0,                                       NULL);
        TEXT_MACRO_X_UINT64 ("discSectors",  writeableDiscSectors,                    NULL);
        TEXT_MACRO_X_UINT64 ("discSectors97",(writeableDiscSectors * 97) / 100,       NULL);
        TEXT_MACRO_X_UINT   ("number",       storageInfo->volumeNumber,               NULL);
        TEXT_MACRO_X_UINT   ("j",            j,                                       NULL);
        TEXT_MACRO_X_UINT   ("j1",           (j > 1) ? j-1 : 1,                       NULL);
      }

      // execute command
      String commandLine = String_new();
      StringList_clear(&executeIOInfo.stderrList);
      error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.imagePreProcessCommand),
                                  textMacros.data,
                                  textMacros.count,
                                  commandLine,
                                  CALLBACK_(executeIOOutput,NULL),
                                  CALLBACK_(executeIOOutput,NULL),
                                  (globalOptions.commandTimeout > 0) ? (long)globalOptions.commandTimeout : WAIT_FOREVER
                                 );
      if (error == ERROR_NONE)
      {
        printInfo(1,"OK\n");
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Image pre-processed for volume #%u",storageInfo->volumeNumber);
      }
      else
      {
        printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
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
    updateVolumeDone(storageInfo,1,0.0,0);

    messageClear(&storageInfo->progress.message);
    updateStorageRunningInfo(storageInfo);
  }

  // create image
  if (error == ERROR_NONE)
  {
    messageSet(&storageInfo->progress.message,MESSAGE_CODE_CREATE_IMAGE,NULL);
    updateStorageRunningInfo(storageInfo);

    if (String_isEmpty(storageInfo->opticalDisk.write.imageCommand))
    {
      #ifdef HAVE_ISOFS
        #define BLOCK_COUNT 32

        // allocate buffer
        byte *buffer = (byte*)malloc(BLOCK_COUNT*ISO_SECTOR_SIZE);
        if (buffer == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }

        printInfo(1,"Create image for volume #%u with %d part(s)...",storageInfo->opticalDisk.write.number,StringList_count(&storageInfo->opticalDisk.write.fileNameList));
        FileHandle fileHandle;
        error = File_open(&fileHandle,imageFileName,FILE_OPEN_CREATE);
        if (error == ERROR_NONE)
        {
          ISOSourceHandle isoSourceHandle;
          error = createISOSourceFromDirectory(&isoSourceHandle,storageInfo->opticalDisk.write.directory);
          if (error == ERROR_NONE)
          {
            switch (globalOptions.runMode)
            {
              case RUN_MODE_INTERACTIVE:
                printInfo(1,"%3u%%\b\b\b\b",0);
                break;
              case RUN_MODE_BATCH:
              case RUN_MODE_SERVER:
                break;
            }

            TimeoutInfo progressInfoTimeout;
            Misc_initTimeout(&progressInfoTimeout, 5*MS_PER_SECOND);
            off_t  size         = isoSourceHandle.burnSource->get_size(isoSourceHandle.burnSource);
            int    readBytes;
            off_t  writtenBytes = 0;
            do
            {
              readBytes = isoSourceHandle.burnSource->read_xt(isoSourceHandle.burnSource,buffer,BLOCK_COUNT*ISO_SECTOR_SIZE);
              if (readBytes > 0)
              {
                error = File_write(&fileHandle,buffer,(ulong)readBytes);
                if (error == ERROR_NONE)
                {
                  writtenBytes += (uint64)readBytes;

                  // output progress
                  double percentageDone = (size > 0)
                                            ? (writtenBytes*100)/size
                                            : 0.0;
                  switch (globalOptions.runMode)
                  {
                    case RUN_MODE_INTERACTIVE:
                      if (Misc_isTimeout(&progressInfoTimeout))
                      {
                        printInfo(1,"%3u%%\b\b\b\b",(uint)percentageDone);
                        Misc_restartTimeout(&progressInfoTimeout,0);
                      }
                      break;
                    case RUN_MODE_BATCH:
                    case RUN_MODE_SERVER:
                      updateVolumeDone(storageInfo,0,percentageDone,0);
                      updateStorageRunningInfo(storageInfo);
                      break;
                  }
                }
              }
              else if (readBytes < 0)
              {
                error = ERROR_READ_FILE;
              }
            }
            while (   (error == ERROR_NONE)
                   && (readBytes > 0)
                  );
            Misc_doneTimeout(&progressInfoTimeout);

            switch (globalOptions.runMode)
            {
              case RUN_MODE_INTERACTIVE:
                printInfo(1,"    \b\b\b\b");
                break;
              case RUN_MODE_BATCH:
              case RUN_MODE_SERVER:
                break;
            }

            deleteISOSource(&isoSourceHandle);
          }

          (void)File_close(&fileHandle);
        }
        if (error == ERROR_NONE)
        {
          printInfo(1,"OK\n");
          logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Created image for volume #%u",storageInfo->volumeNumber);
        }
        else
        {
          printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
          logMessage(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Create image volume #%u fail: %s",
                     storageInfo->volumeNumber,
                     Error_getText(error)
                    );
        }

        // free resources
        free(buffer);

        #undef BLOCK_COUNT
      #else // not HAVE_ISOFS
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif // HAVE_ISOFS
    }
    else
    {
      printInfo(1,"Create image for volume #%u with %d part(s)...",storageInfo->opticalDisk.write.number,StringList_count(&storageInfo->opticalDisk.write.fileNameList));

      // init macros
      uint j = Thread_getNumberOfCores();
      TextMacros (textMacros,10);
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_CSTRING("device",       deviceName,                              NULL);
        TEXT_MACRO_X_STRING ("directory",    storageInfo->opticalDisk.write.directory,NULL);
        TEXT_MACRO_X_STRING ("image",        imageFileName,                           NULL);
        TEXT_MACRO_X_UINT64 ("size",         0,                                       NULL);
        TEXT_MACRO_X_UINT64 ("sectors",      0,                                       NULL);
        TEXT_MACRO_X_UINT64 ("discSectors",  writeableDiscSectors,                    NULL);
        TEXT_MACRO_X_UINT64 ("discSectors97",(writeableDiscSectors * 97) / 100,       NULL);
        TEXT_MACRO_X_UINT   ("number",       storageInfo->volumeNumber,               NULL);
        TEXT_MACRO_X_UINT   ("j",            j,                                       NULL);
        TEXT_MACRO_X_UINT   ("j1",           (j > 1) ? j-1 : 1,                       NULL);
      }

      // execute command
      String commandLine = String_new();
      StringList_clear(&executeIOInfo.stderrList);
      error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.imageCommand),
                                  textMacros.data,
                                  textMacros.count,
                                  commandLine,
                                  CALLBACK_(executeIOOpticalMkIsofsStdout,&executeIOInfo),
                                  CALLBACK_(executeIOOpticalMkIsofsStderr,&executeIOInfo),
                                  WAIT_FOREVER
                                 );
      if (error == ERROR_NONE)
      {
        printInfo(1,"OK\n");
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Created image for volume #%u",storageInfo->volumeNumber);
      }
      else
      {
        printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
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
    updateVolumeDone(storageInfo,1,0.0,0);

    messageClear(&storageInfo->progress.message);
    updateStorageRunningInfo(storageInfo);
  }

  // get image size
  FileInfo fileInfo;
  File_getInfo(&fileInfo,imageFileName);

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
        printInfo(1,"Add error correction codes pre-processing to image of volume #%u...",storageInfo->volumeNumber);

        // init macros
        uint j = Thread_getNumberOfCores();
        TextMacros (textMacros,10);
        TEXT_MACROS_INIT(textMacros)
        {
          TEXT_MACRO_X_CSTRING("device",       deviceName,                              NULL);
          TEXT_MACRO_X_STRING ("directory",    storageInfo->opticalDisk.write.directory,NULL);
          TEXT_MACRO_X_STRING ("image",        imageFileName,                           NULL);
          TEXT_MACRO_X_UINT64 ("size",         fileInfo.size,                           NULL);
          TEXT_MACRO_X_UINT64 ("sectors",      (uint64)(fileInfo.size/ISO_SECTOR_SIZE), NULL);
          TEXT_MACRO_X_UINT64 ("discSectors",  writeableDiscSectors,                    NULL);
          TEXT_MACRO_X_UINT64 ("discSectors97",(writeableDiscSectors * 97) / 100,       NULL);
          TEXT_MACRO_X_UINT   ("number",       storageInfo->volumeNumber,               NULL);
          TEXT_MACRO_X_UINT   ("j",            j,                                       NULL);
          TEXT_MACRO_X_UINT   ("j1",           (j > 1) ? j-1 : 1,                       NULL);
        }

        // execute command
        String commandLine = String_new();
        StringList_clear(&executeIOInfo.stderrList);
        error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.eccPreProcessCommand),
                                    textMacros.data,
                                    textMacros.count,
                                    commandLine,
                                    CALLBACK_(executeIOOutput,NULL),
                                    CALLBACK_(executeIOOutput,NULL),
                                    (globalOptions.commandTimeout > 0) ? (long)globalOptions.commandTimeout : WAIT_FOREVER
                                   );
        if (error == ERROR_NONE)
        {
          printInfo(1,"OK\n");
          logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
          logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"ECC pre-processed image for volume #%u",storageInfo->volumeNumber);
        }
        else
        {
          printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
          logMessage(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Add error correction codes pre-processing to image of volume #%u fail: %s",
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
    updateVolumeDone(storageInfo,1,0.0,0);
    updateStorageRunningInfo(storageInfo);

    // add error-correction codes to image
    if (error == ERROR_NONE)
    {
      if (!String_isEmpty(storageInfo->opticalDisk.write.eccCommand))
      {
        printInfo(1,"Add error correction codes to image of volume #%u...",storageInfo->volumeNumber);

        // init macros
        uint j = Thread_getNumberOfCores();
        TextMacros (textMacros,10);
        TEXT_MACROS_INIT(textMacros)
        {
          TEXT_MACRO_X_CSTRING("device",       deviceName,                              NULL);
          TEXT_MACRO_X_STRING ("directory",    storageInfo->opticalDisk.write.directory,NULL);
          TEXT_MACRO_X_STRING ("image",        imageFileName,                           NULL);
          TEXT_MACRO_X_UINT64 ("size",         fileInfo.size,                           NULL);
          TEXT_MACRO_X_UINT64 ("sectors",      (uint64)(fileInfo.size/ISO_SECTOR_SIZE), NULL);
          TEXT_MACRO_X_UINT64 ("discSectors",  writeableDiscSectors,                    NULL);
          TEXT_MACRO_X_UINT64 ("discSectors97",(writeableDiscSectors * 97) / 100,       NULL);
          TEXT_MACRO_X_UINT   ("number",       storageInfo->volumeNumber,               NULL);
          TEXT_MACRO_X_UINT   ("j",            j,                                       NULL);
          TEXT_MACRO_X_UINT   ("j1",           (j > 1) ? j-1 : 1,                       NULL);
        }

        // execute command
        String commandLine = String_new();
        StringList_clear(&executeIOInfo.stderrList);
        error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.eccCommand),
                                    textMacros.data,
                                    textMacros.count,
                                    commandLine,
                                    CALLBACK_(executeIOOpticalDVDDisasterStdout,&executeIOInfo),
                                    CALLBACK_(executeIOOpticalDVDDisasterStderr,&executeIOInfo),
                                    WAIT_FOREVER
                                   );
        if (error == ERROR_NONE)
        {
          printInfo(1,"OK\n");
          logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
          logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Added ECC to image for volume #%u",storageInfo->volumeNumber);
        }
        else
        {
          printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
          logMessage(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Add error correction codes to image of volume #%u fail: %s",
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
    updateVolumeDone(storageInfo,1,0.0,0);
    updateStorageRunningInfo(storageInfo);

    // error-correction codes post-processing
    if (error == ERROR_NONE)
    {
      if (!String_isEmpty(storageInfo->opticalDisk.write.eccPostProcessCommand))
      {
        printInfo(1,"Add error correction codes post-processing to image of volume #%u...",storageInfo->volumeNumber);

        // init macros
        uint j = Thread_getNumberOfCores();
        TextMacros (textMacros,10);
        TEXT_MACROS_INIT(textMacros)
        {
          TEXT_MACRO_X_CSTRING("device",       deviceName,                              NULL);
          TEXT_MACRO_X_STRING ("directory",    storageInfo->opticalDisk.write.directory,NULL);
          TEXT_MACRO_X_STRING ("image",        imageFileName,                           NULL);
          TEXT_MACRO_X_UINT64 ("size",         fileInfo.size,                           NULL);
          TEXT_MACRO_X_UINT64 ("sectors",      (uint64)(fileInfo.size/ISO_SECTOR_SIZE), NULL);
          TEXT_MACRO_X_UINT64 ("discSectors",  writeableDiscSectors,                    NULL);
          TEXT_MACRO_X_UINT64 ("discSectors97",(writeableDiscSectors * 97) / 100,       NULL);
          TEXT_MACRO_X_UINT   ("number",       storageInfo->volumeNumber,               NULL);
          TEXT_MACRO_X_UINT   ("j",            j,                                       NULL);
          TEXT_MACRO_X_UINT   ("j1",           (j > 1) ? j-1 : 1,                       NULL);
        }

        // execute command
        String commandLine = String_new();
        StringList_clear(&executeIOInfo.stderrList);
        error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.eccPostProcessCommand ),
                                    textMacros.data,
                                    textMacros.count,
                                    commandLine,
                                    CALLBACK_(executeIOOutput,NULL),
                                    CALLBACK_(executeIOOutput,NULL),
                                    (globalOptions.commandTimeout > 0) ? (long)globalOptions.commandTimeout : WAIT_FOREVER
                                   );
        if (error == ERROR_NONE)
        {
          printInfo(1,"OK\n");
          logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
          logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"ECC post-processed image for volume #%u",storageInfo->volumeNumber);
        }
        else
        {
          printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
          logMessage(storageInfo->logHandle,
                     LOG_TYPE_ERROR,
                     "Add error correction codes post-processing to image of volume #%u fail: %s",
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
    updateVolumeDone(storageInfo,1,0.0,0);
    updateStorageRunningInfo(storageInfo);

    messageClear(&storageInfo->progress.message);
    updateStorageRunningInfo(storageInfo);
  }

  // update image size
  File_getInfo(&fileInfo,imageFileName);

  // post-processing
  if (error == ERROR_NONE)
  {
    messageSet(&storageInfo->progress.message,MESSAGE_CODE_CREATE_IMAGE,NULL);
    updateStorageRunningInfo(storageInfo);

    if (!String_isEmpty(storageInfo->opticalDisk.write.imagePostProcessCommand))
    {
      printInfo(1,"Image post-processing of volume #%u...",storageInfo->volumeNumber);

      // init macros
      uint j = Thread_getNumberOfCores();
      TextMacros (textMacros,10);
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_CSTRING("device",       deviceName,                              NULL);
        TEXT_MACRO_X_STRING ("directory",    storageInfo->opticalDisk.write.directory,NULL);
        TEXT_MACRO_X_STRING ("image",        imageFileName,                           NULL);
        TEXT_MACRO_X_UINT64 ("size",         fileInfo.size,                           NULL);
        TEXT_MACRO_X_UINT64 ("sectors",      (uint64)(fileInfo.size/ISO_SECTOR_SIZE), NULL);
        TEXT_MACRO_X_UINT64 ("discSectors",  writeableDiscSectors,                    NULL);
        TEXT_MACRO_X_UINT64 ("discSectors97",(writeableDiscSectors * 97) / 100,       NULL);
        TEXT_MACRO_X_UINT   ("number",       storageInfo->volumeNumber,               NULL);
        TEXT_MACRO_X_UINT   ("j",            j,                                       NULL);
        TEXT_MACRO_X_UINT   ("j1",           (j > 1) ? j-1 : 1,                       NULL);
      }

      // execute command
      String commandLine = String_new();
      StringList_clear(&executeIOInfo.stderrList);
      error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.imagePostProcessCommand ),
                                  textMacros.data,
                                  textMacros.count,
                                  commandLine,
                                  CALLBACK_(executeIOOutput,NULL),
                                  CALLBACK_(executeIOOutput,NULL),
                                  (globalOptions.commandTimeout > 0) ? (long)globalOptions.commandTimeout : WAIT_FOREVER
                                 );
      if (error == ERROR_NONE)
      {
        printInfo(1,"OK\n");
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Image post-processed for volume #%u",storageInfo->volumeNumber);
      }
      else
      {
        printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
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

    updateVolumeDone(storageInfo,1,0.0,0);
    messageClear(&storageInfo->progress.message);
    updateStorageRunningInfo(storageInfo);
  }

  // free resources
  doneOpticalExecuteIOInfo(&executeIOInfo);

  return error;
}

/***********************************************************************\
* Name   : writeISOImage
* Purpose: write ISO9660 image to volume
* Input  : storageInfo   - storage info
*          imageFileName - ISO9660 image file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeISOImage(StorageInfo *storageInfo, ConstString imageFileName)
{
  assert(storageInfo != NULL);
  assert(imageFileName != NULL);

  Errors error = ERROR_NONE;

  messageSet(&storageInfo->progress.message,MESSAGE_CODE_WRITE_VOLUME,NULL);
  updateStorageRunningInfo(storageInfo);

  const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
  #ifndef NDEBUG
    const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
    if (debugEmulateBlockDevice != NULL)
    {
      deviceName = debugEmulateBlockDevice;
    }
  #endif

  if (String_isEmpty(storageInfo->opticalDisk.write.writeImageCommand))
  {
    #ifdef HAVE_BURN
      // aquire drive
      #ifndef NDEBUG
      if (debugEmulateBlockDevice == NULL)
      {
      #endif
        struct burn_drive_info *burnDriveInfo;
        if (burn_drive_scan_and_grab(&burnDriveInfo,(char*)deviceName,1) == 1)
        {
          // wait for drive
          while (burn_drive_get_status(burnDriveInfo->drive,NULL) != BURN_DRIVE_IDLE)
          {
            Misc_mdelay(100);
          }

          // get media status
          enum burn_disc_status burnDiscStatus = burn_disc_get_status(burnDriveInfo->drive);
          if (   (burnDiscStatus == BURN_DISC_BLANK)
              || (burnDiscStatus == BURN_DISC_APPENDABLE)
             )
          {
            // create ISO source form image file
            ISOSourceHandle isoSourceHandle;
            error = createISOSourceFromImage(&isoSourceHandle,imageFileName);
            if (error == ERROR_NONE)
            {
              error = writeISOSource(storageInfo,burnDriveInfo,&isoSourceHandle);
              deleteISOSource(&isoSourceHandle);
            }
          }
          else
          {
            error = ERRORX_(WRITE_OPTICAL_DISK,burnDiscStatus,"disc not writable");
          }

          burn_drive_release(burnDriveInfo->drive,0);  // Note: keep tray closed, just unlock

          burn_drive_info_free(burnDriveInfo);
        }
        else
        {
          error = ERRORX_(OPEN_OPTICAL_DRIVE,errno,"%s",strerror(errno));
        }
      #ifndef NDEBUG
      }
      else
      {
        error = File_copyCString(String_cString(imageFileName),deviceName);
      }
      #endif

      if (error == ERROR_NONE)
      {
        printInfo(1,"Written image to volume #%u\n",storageInfo->opticalDisk.write.number);
        logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Written image to volume #%u",storageInfo->volumeNumber);
      }
      else
      {
        printInfo(1,"Write image to volume #%u failed (error: %s)\n",storageInfo->opticalDisk.write.number,Error_getText(error));
        logMessage(storageInfo->logHandle,
                   LOG_TYPE_ERROR,
                   "Write image to volume #%u fail: %s",
                   storageInfo->volumeNumber,
                   Error_getText(error)
                  );
      }
    #else
      error = ERROR_FUNCTION_NOT_SUPPORTED;
    #endif
  }
  else
  {
    // get image size
    FileInfo fileInfo;
    File_getInfo(&fileInfo,imageFileName);

    // init macros
    uint j = Thread_getNumberOfCores();
    TextMacros (textMacros,8);
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_CSTRING("device",   deviceName,                              NULL);
      TEXT_MACRO_X_STRING ("directory",storageInfo->opticalDisk.write.directory,NULL);
      TEXT_MACRO_X_STRING ("image",    imageFileName,                           NULL);
      TEXT_MACRO_X_UINT64 ("size",     fileInfo.size,                           NULL);
      TEXT_MACRO_X_UINT64 ("sectors",  (uint64)(fileInfo.size/ISO_SECTOR_SIZE), NULL);
      TEXT_MACRO_X_UINT   ("number",   storageInfo->volumeNumber,               NULL);
      TEXT_MACRO_X_UINT   ("j",        j,                                       NULL);
      TEXT_MACRO_X_UINT   ("j1",       (j > 1) ? j-1 : 1,                       NULL);
    }

    // init variables
    String commandLine = String_new();
    ExecuteIOInfo executeIOInfo;
    initOpticalExecuteIOInfo(&executeIOInfo,storageInfo);

    StringList_clear(&executeIOInfo.stderrList);
    error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.writeImageCommand),
                                textMacros.data,
                                textMacros.count,
                                commandLine,
                                CALLBACK_(executeIOOpticalGrowIsofsStdout,&executeIOInfo),
                                CALLBACK_(executeIOOpticalGrowIsofsStderr,&executeIOInfo),
                                WAIT_FOREVER
                               );
    if (error == ERROR_NONE)
    {
      printInfo(1,"Written image to volume #%u OK\n",storageInfo->opticalDisk.write.number);
      logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
      logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Written image to volume #%u",storageInfo->volumeNumber);
    }
    else
    {
      printInfo(1,"Write image to volume #%u failed (error: %s)\n",storageInfo->opticalDisk.write.number,Error_getText(error));
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
    }

    // free resources
    doneOpticalExecuteIOInfo(&executeIOInfo);
    String_delete(commandLine);
  }

  updateVolumeDone(storageInfo,1,0.0,0);
  messageClear(&storageInfo->progress.message);
  updateStorageRunningInfo(storageInfo);

  return error;
}

/***********************************************************************\
* Name   : writeDirectory
* Purpose: write ISO to volume
* Input  : storageInfo          - storage info
*          writeableDiscSectors - number of writable sectors on disc
*                                 (2048 bytes)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors writeDirectory(StorageInfo *storageInfo, uint64 writeableDiscSectors)
{
  assert(storageInfo != NULL);

  Errors error = ERROR_NONE;

  messageSet(&storageInfo->progress.message,MESSAGE_CODE_WRITE_VOLUME,NULL);
  updateStorageRunningInfo(storageInfo);

  const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
  #ifndef NDEBUG
    const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
    if (debugEmulateBlockDevice != NULL)
    {
      deviceName = debugEmulateBlockDevice;
    }
  #endif

  printInfo(1,"Write volume #%u with %d part(s)...",storageInfo->opticalDisk.write.number,StringList_count(&storageInfo->opticalDisk.write.fileNameList));
  if (String_isEmpty(storageInfo->opticalDisk.write.writeCommand))
  {
    #ifdef HAVE_BURN
      #ifndef NDEBUG
      if (debugEmulateBlockDevice == NULL)
      {
      #endif
        // aquire drive
        struct burn_drive_info *burnDriveInfo;
        if (burn_drive_scan_and_grab(&burnDriveInfo,(char*)deviceName,1) == 1)
        {
          // wait for drive
          while (burn_drive_get_status(burnDriveInfo->drive,NULL) != BURN_DRIVE_IDLE)
          {
            Misc_mdelay(100);
          }

          // get media status
          enum burn_disc_status burnDiscStatus = burn_disc_get_status(burnDriveInfo->drive);
          if (   (burnDiscStatus == BURN_DISC_BLANK)
              || (burnDiscStatus == BURN_DISC_APPENDABLE)
             )
          {
            ISOSourceHandle isoSourceHandle;
            error = createISOSourceFromDirectory(&isoSourceHandle,storageInfo->opticalDisk.write.directory);
            if (error == ERROR_NONE)
            {
              error = writeISOSource(storageInfo,burnDriveInfo,&isoSourceHandle);
              deleteISOSource(&isoSourceHandle);
            }
          }
          else
          {
            error = ERRORX_(WRITE_OPTICAL_DISK,burnDiscStatus,"disc not writable");
          }

          burn_drive_release(burnDriveInfo->drive,0);  // Note: keep tray closed, just unlock

          burn_drive_info_free(burnDriveInfo);
        }
        else
        {
          error = ERRORX_(OPEN_OPTICAL_DRIVE,errno,"%s",strerror(errno));
        }
      #ifndef NDEBUG
      }
      else
      {
        #ifdef HAVE_ISOFS
          FileHandle fileHandle;
          error = File_openCString(&fileHandle,deviceName,FILE_OPEN_CREATE);
          if (error == ERROR_NONE)
          {
            ISOSourceHandle isoSourceHandle;
            error = createISOSourceFromDirectory(&isoSourceHandle,storageInfo->opticalDisk.write.directory);
            if (error == ERROR_NONE)
            {
              #define BLOCK_COUNT 32

              byte *buffer = (byte*)malloc(BLOCK_COUNT*ISO_SECTOR_SIZE);
              if (buffer == NULL)
              {
                HALT_INSUFFICIENT_MEMORY();
              }

              int readBytes;
              do
              {
                readBytes = isoSourceHandle.burnSource->read_xt(isoSourceHandle.burnSource,buffer,BLOCK_COUNT*ISO_SECTOR_SIZE);
                if (readBytes > 0)
                {
                  error = File_write(&fileHandle,buffer,(ulong)readBytes);
                }
                else if (readBytes < 0)
                {
                  error = ERROR_READ_FILE;
                }
              }
              while (   (error == ERROR_NONE)
                     && (readBytes > 0)
                    );

              free(buffer);

              deleteISOSource(&isoSourceHandle);
              if (error == ERROR_NONE)
              {
                error = File_close(&fileHandle);
              }
              else
              {
                (void)File_close(&fileHandle);
              }

              #undef BLOCK_COUNT
            }
            else
            {
              (void)File_close(&fileHandle);
            }
          }
        #else // not HAVE_ISOFS
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        #endif // HAVE_ISOFS
      }
      #endif
    #else
      error = ERROR_FUNCTION_NOT_SUPPORTED;
    #endif
  }
  else
  {
    // init macros
    uint j = Thread_getNumberOfCores();
    TextMacros (textMacros,9);
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_CSTRING("device",       deviceName,                                                                         NULL);
      TEXT_MACRO_X_STRING ("directory",    storageInfo->opticalDisk.write.directory,                                           NULL);
      TEXT_MACRO_X_UINT64 ("size",         storageInfo->opticalDisk.write.volumeSize,                                          NULL);
      TEXT_MACRO_X_UINT64 ("sectors",      (storageInfo->opticalDisk.write.volumeSize + ISO_SECTOR_SIZE - 1) / ISO_SECTOR_SIZE,NULL);
      TEXT_MACRO_X_UINT64 ("discSectors",  writeableDiscSectors,                                                               NULL);
      TEXT_MACRO_X_UINT64 ("discSectors97",(writeableDiscSectors * 97) / 100,                                                  NULL);
      TEXT_MACRO_X_UINT   ("number",       storageInfo->volumeNumber,                                                          NULL);
      TEXT_MACRO_X_UINT   ("j",            j,                                                                                  NULL);
      TEXT_MACRO_X_UINT   ("j1",           (j > 1) ? j-1 : 1,                                                                  NULL);
    }

    // init variables
    String commandLine = String_new();
    ExecuteIOInfo executeIOInfo;
    initOpticalExecuteIOInfo(&executeIOInfo,storageInfo);

    StringList_clear(&executeIOInfo.stderrList);
    error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.writeCommand),
                                textMacros.data,
                                textMacros.count,
                                commandLine,
                                CALLBACK_(executeIOOpticalGrowIsofsStdout,&executeIOInfo),
                                CALLBACK_(executeIOOpticalGrowIsofsStderr,&executeIOInfo),
                                WAIT_FOREVER
                               );
    if (error == ERROR_NONE)
    {
      printInfo(1,"OK\n");
      logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Command '%s'",String_cString(commandLine));
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
      logMessage(storageInfo->logHandle,LOG_TYPE_ERROR,"Command '%s'",String_cString(commandLine));
      logLines(storageInfo->logHandle,
               LOG_TYPE_ERROR,
               "  ",
               &executeIOInfo.stderrList
              );
    }

    // free resources
    doneOpticalExecuteIOInfo(&executeIOInfo);
    String_delete(commandLine);
  }

  updateVolumeDone(storageInfo,1,0.0,0);
  messageClear(&storageInfo->progress.message);
  updateStorageRunningInfo(storageInfo);

  return error;
}

/***********************************************************************\
* Name   : verifyVolume
* Purpose: verify content of cd/dvd/bd medium
* Input  : storageInfo - storage info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors verifyVolume(StorageInfo *storageInfo)
{
  #define BLOCK_COUNT 32

// TODO: remove
return ERROR_NONE;
  assert(storageInfo != NULL);

  Errors error = ERROR_NONE;

  messageSet(&storageInfo->progress.message,MESSAGE_CODE_VERIFY_VOLUME,NULL);
  updateStorageRunningInfo(storageInfo);

  #ifdef HAVE_ISO9660
    // allocate buffers
    byte *buffer0 = (byte*)malloc(BLOCK_COUNT*ISO_BLOCKSIZE);
    if (buffer0 == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    byte *buffer1 = (byte*)malloc(BLOCK_COUNT*ISO_BLOCKSIZE);
    if (buffer1 == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    // unload+load volume to clear caches
    (void)unloadOpticalVolume(storageInfo);
    (void)loadOpticalVolume(storageInfo);

    printInfo(1,"Verify volume #%u...",storageInfo->opticalDisk.write.number);

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
          error = File_readDirectoryList(&directoryListHandle,fileName,NULL);
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
      const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
      #ifndef NDEBUG
        const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
        if (debugEmulateBlockDevice != NULL)
        {
          deviceName = debugEmulateBlockDevice;
        }
      #endif

      TimeoutInfo timeoutInfo;
      Misc_initTimeout(&timeoutInfo,2*S_PER_MINUTE*MS_PER_SECOND);
      iso9660_t   *iso9660Handle;
      do
      {
        iso9660Handle = iso9660_open_ext(deviceName,
                                         ISO_EXTENSION_ALL
                                        );
        if (iso9660Handle == NULL) Misc_mdelay(5*MS_PER_SECOND);
      }
      while ((iso9660Handle == NULL) && !Misc_isTimeout(&timeoutInfo));
      Misc_doneTimeout(&timeoutInfo);
      if (iso9660Handle != NULL)
      {
        switch (globalOptions.runMode)
        {
          case RUN_MODE_INTERACTIVE:
            printInfo(1,"%3u%%\b\b\b\b",0);
            break;
          case RUN_MODE_BATCH:
          case RUN_MODE_SERVER:
            break;
        }

        uint        fileCount = 0;
        ConstString fileName;
        STRINGLIST_ITERATEX(&fileNameList,fileName,error == ERROR_NONE)
        {
          uint64 t0 = Misc_getTimestamp();

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
            String baseName = File_getBaseName(String_new(),fileName,TRUE);
            iso9660Stat = iso9660_ifs_stat_translate(iso9660Handle,
                                                     String_cString(baseName)
                                                    );
            if (iso9660Stat == NULL)
            {
              error = ERRORX_(FILE_NOT_FOUND_,errno,"%s",String_cString(baseName));
              File_close(&fileHandle);
              String_delete(baseName);
              break;
            }
            String_delete(baseName);
          }

          // read and compare content
          TimeoutInfo progressInfoTimeout;
          Misc_initTimeout(&progressInfoTimeout, 5*MS_PER_SECOND);
          uint64      readBytes  = 0;
          size_t      blockIndex = 0;
          while ((error == ERROR_NONE) && !File_eof(&fileHandle))
          {
            ulong n0;
            error = File_read(&fileHandle,buffer0,BLOCK_COUNT*ISO_BLOCKSIZE,&n0);
            if (error == ERROR_NONE)
            {
              long int n1 = iso9660_iso_seek_read(iso9660Handle,
                                                  buffer1,
                                                  iso9660Stat->lsn+(lsn_t)blockIndex,
                                                  BLOCK_COUNT
                                                 );
              if ((n1 >= 0) && (n0 <= (ulong)n1))
              {
                if (!memEquals(buffer0,n0,buffer1,n0))
                {
                  error = ERRORX_(VERIFY_OPTICAL_DISK,0,"'%s' at offset %"PRIu64,String_cString(fileName),(uint64)blockIndex*ISO_BLOCKSIZE);
                }
              }
              else
              {
                error = ERRORX_(READ_OPTICAL_DISK,0,"'%s' at offset %"PRIu64", %ubytes",String_cString(fileName),(uint64)blockIndex*ISO_BLOCKSIZE,n0);
              }
              readBytes += (uint64)n0;
            }
            blockIndex += BLOCK_COUNT;

            uint64 t1 = Misc_getTimestamp();

            // output progress
            double percentageDone = ((double)fileCount*100.0)/(double)(size_t)StringList_count(&fileNameList);
            size_t bytesPerSecond = (t1 > t0)
                                      ? (size_t)((readBytes * US_PER_S) / (t1 - t0))
                                      : 0;
            switch (globalOptions.runMode)
            {
              case RUN_MODE_INTERACTIVE:
                if (Misc_isTimeout(&progressInfoTimeout))
                {
                  printInfo(1,"%3u%%\b\b\b\b",(uint)percentageDone);
                  Misc_restartTimeout(&progressInfoTimeout,0);
                }
                break;
              case RUN_MODE_BATCH:
              case RUN_MODE_SERVER:
                updateVolumeDone(storageInfo,0,percentageDone,bytesPerSecond);
                updateStorageRunningInfo(storageInfo);
                break;
            }
          }

          // free resources
          iso9660_stat_free(iso9660Stat);
          File_close(&fileHandle);

          uint64 t1 = Misc_getTimestamp();
          fileCount++;

          // output progress
          double percentageDone = ((double)fileCount*100.0)/(double)(size_t)StringList_count(&fileNameList);
          size_t bytesPerSecond = (t1 > t0)
                                    ? (size_t)((readBytes * US_PER_S) / (t1 - t0))
                                    : 0;
          switch (globalOptions.runMode)
          {
            case RUN_MODE_INTERACTIVE:
              printInfo(1,"%3u%%\b\b\b\b",(uint)percentageDone);
              break;
            case RUN_MODE_BATCH:
            case RUN_MODE_SERVER:
              updateVolumeDone(storageInfo,0,percentageDone,bytesPerSecond);
              updateStorageRunningInfo(storageInfo);
              break;
          }
        }

        switch (globalOptions.runMode)
        {
          case RUN_MODE_INTERACTIVE:
            printInfo(1,"    \b\b\b\b");
            break;
          case RUN_MODE_BATCH:
          case RUN_MODE_SERVER:
            break;
        }

        iso9660_close(iso9660Handle);
      }
      else
      {
        if (File_isFileCString(deviceName))
        {
          error = ERRORX_(OPEN_ISO9660,errno,"%s",deviceName);
        }
        else
        {
          error = ERRORX_(OPEN_OPTICAL_DISK,errno,"%s",deviceName);
        }
      }
    }

    if (error == ERROR_NONE)
    {
      printInfo(1,"OK\n");
      logMessage(storageInfo->logHandle,LOG_TYPE_INFO,"Verified volume #%u",storageInfo->volumeNumber);
    }
    else
    {
      printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
      logMessage(storageInfo->logHandle,
                 LOG_TYPE_ERROR,
                 "Verify volume #%u fail: %s",
                 storageInfo->volumeNumber,
                 Error_getText(error)
                );
    }

    // free resources
    StringList_done(&fileNameList);
    free(buffer1);
    free(buffer0);
  #else
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif

  updateVolumeDone(storageInfo,1,0.0,0);
  messageClear(&storageInfo->progress.message);
  updateStorageRunningInfo(storageInfo);

  return error;

  #undef BLOCK_COUNT
}

LOCAL Errors StorageOptical_preProcess(StorageInfo *storageInfo,
                                       ConstString archiveName,
                                       time_t      timestamp,
                                       bool        initialFlag
                                      )
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  UNUSED_VARIABLE(initialFlag);

  Errors error = ERROR_NONE;

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

  // write pre-processing
  ConstString template = NULL;
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

    const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
    #ifndef NDEBUG
      const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
      if (debugEmulateBlockDevice != NULL)
      {
        deviceName = debugEmulateBlockDevice;
      }
    #endif

    // init macros
    uint j = Thread_getNumberOfCores();
    TextMacros (textMacros,5);
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_CSTRING("device",deviceName,                      NULL);
      TEXT_MACRO_X_STRING ("file",  archiveName,                     NULL);
      TEXT_MACRO_X_UINT   ("number",storageInfo->volumeRequestNumber,NULL);
      TEXT_MACRO_X_UINT   ("j",     j,                               NULL);
      TEXT_MACRO_X_UINT   ("j1",    (j > 1) ? j-1 : 1,               NULL);
    }

    // write pre-processing
    error = executeTemplate(String_cString(template),
                            timestamp,
                            textMacros.data,
                            textMacros.count,
                            CALLBACK_(executeIOOutput,NULL),
                            globalOptions.commandTimeout
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
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(storageInfo->jobOptions != NULL);

  Errors error = ERROR_NONE;

  if (   (storageInfo->opticalDisk.write.totalSize >= storageInfo->opticalDisk.write.volumeSize)
      || (finalFlag && (storageInfo->opticalDisk.write.totalSize > 0LL))
     )
  {
    // volume size limit reached or final volume -> request new volume and write volume

    // update running info
    resetVolumeDone(storageInfo);
    updateStorageRunningInfo(storageInfo);

    // get temporary image file name
    String imageFileName = String_new();
    error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
    if (error != ERROR_NONE)
    {
      String_delete(imageFileName);
      return error;
    }

    // check if new medium is required
    if (storageInfo->volumeNumber != storageInfo->volumeRequestNumber)
    {
      error = requestNewOpticalMedium(storageInfo,NULL,TRUE);
      if (error != ERROR_NONE)
      {
        (void)File_delete(imageFileName,FALSE);
        String_delete(imageFileName);
        return error;
      }
      updateStorageRunningInfo(storageInfo);
    }

    // load medium
    if (error == ERROR_NONE)
    {
      error = loadOpticalVolume(storageInfo);
      if (error != ERROR_NONE)
      {
        printInfo(1,"Load medium failed (error: %s)\n",Error_getText(error));
        (void)File_delete(imageFileName,FALSE);
        String_delete(imageFileName);
        return error;
      }
    }

    // get number of writable disc sectors
    uint64 writeableDiscSectors = 0;
    if (error == ERROR_NONE)
    {
      (void)getWritebaleDiscSectors(&writeableDiscSectors,storageInfo);
    }

    if (   (storageInfo->jobOptions != NULL)
        && (   storageInfo->jobOptions->alwaysCreateImageFlag
            || storageInfo->jobOptions->errorCorrectionCodesFlag
           )
       )
    {
      // create and write image

      // create ISO image
      if (error == ERROR_NONE)
      {
        error = createISOImage(storageInfo,imageFileName,writeableDiscSectors);
      }

      // format+blank+write image to medium+verify
      if (error == ERROR_NONE)
      {
        uint saveStep = storageInfo->opticalDisk.write.step;
        bool retryFlag;
        do
        {
          storageInfo->opticalDisk.write.step = saveStep;
          retryFlag = FALSE;

          if (storageInfo->jobOptions->formatFlag || !isFormatted(storageInfo))
          {
            // format medium (ignore errors)
            if (error == ERROR_NONE)
            {
              Errors tmpError = formatVolume(storageInfo,writeableDiscSectors);
              if (tmpError != ERROR_NONE)
              {
                printWarning("cannot format medium (error: %s)",Error_getText(tmpError));
              }
            }
          }

          // blank volume (ignore errors)
          if (!storageInfo->jobOptions->formatFlag && storageInfo->jobOptions->blankFlag)
          {
            if (error == ERROR_NONE)
            {
              Errors tmpError = blankVolume(storageInfo,writeableDiscSectors);
              if (tmpError != ERROR_NONE)
              {
                printWarning("cannot blank medium (error: %s)",Error_getText(tmpError));
              }
            }
          }

          // write image
          if (error == ERROR_NONE)
          {
            error = writeISOImage(storageInfo,imageFileName);
          }

          // verify volume
          if (error == ERROR_NONE)
          {
            error = verifyVolume(storageInfo);
          }

          if (error != ERROR_NONE)
          {
            logMessage(storageInfo->logHandle,
                       LOG_TYPE_ERROR,
                       "%s",
                       Error_getText(error)
                      );
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
            if (retryFlag) error = ERROR_NONE;
          }
        }
        while (retryFlag);
      }
    }
    else
    {
      // directly write files

      // format+blank+write to medium+verify
      if (error == ERROR_NONE)
      {
        uint saveStep = storageInfo->opticalDisk.write.step;
        bool retryFlag;
        do
        {
          storageInfo->opticalDisk.write.step = saveStep;
          retryFlag = FALSE;

          // format medium (ignore errors)
          if (storageInfo->jobOptions->formatFlag || !isFormatted(storageInfo))
          {
            if (error == ERROR_NONE)
            {
              Errors tmpError = formatVolume(storageInfo,writeableDiscSectors);
              if (tmpError != ERROR_NONE)
              {
                printWarning("cannot format medium (error: %s)",Error_getText(tmpError));
              }
            }
          }

          // blank volume (ignore errors)
          if (!storageInfo->jobOptions->formatFlag && storageInfo->jobOptions->blankFlag)
          {
            if (error == ERROR_NONE)
            {
              Errors tmpError = blankVolume(storageInfo,writeableDiscSectors);
              if (tmpError != ERROR_NONE)
              {
                printWarning("cannot blank medium (error: %s)",Error_getText(tmpError));
              }
            }
          }

          // write medium
          if (error == ERROR_NONE)
          {
            error = writeDirectory(storageInfo,writeableDiscSectors);
          }

          // verify volume
          if (error == ERROR_NONE)
          {
            error = verifyVolume(storageInfo);
          }

          if (error != ERROR_NONE)
          {
            switch (globalOptions.runMode)
            {
              case RUN_MODE_INTERACTIVE:
                retryFlag = Misc_getYesNo("Retry write volume?");
                break;
              case RUN_MODE_BATCH:
              case RUN_MODE_SERVER:
                retryFlag = (requestNewOpticalMedium(storageInfo,Error_getText(error),TRUE) == ERROR_NONE);
                break;
            }
            if (retryFlag) error = ERROR_NONE;
          }
        }
        while (retryFlag);
      }
    }

    // delete image file
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
    String fileName = String_new();
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
    updateVolumeDone(storageInfo,0,100.0,0);
    updateStorageRunningInfo(storageInfo);

    // handle error
    if (error != ERROR_NONE)
    {
      return error;
    }

    // reset
    storageInfo->opticalDisk.write.newVolumeFlag = TRUE;
    storageInfo->opticalDisk.write.totalSize     = 0;

    // free resources
  }

  ConstString template = NULL;
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

    const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
    #ifndef NDEBUG
      const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
      if (debugEmulateBlockDevice != NULL)
      {
        deviceName = debugEmulateBlockDevice;
      }
    #endif

    // init macros
    uint j = Thread_getNumberOfCores();
    TextMacros (textMacros,5);
    TEXT_MACROS_INIT(textMacros)
    {
      TEXT_MACRO_X_CSTRING("device",deviceName,                      NULL);
      TEXT_MACRO_X_STRING ("file",  archiveName,                     NULL);
      TEXT_MACRO_X_UINT   ("number",storageInfo->volumeRequestNumber,NULL);
      TEXT_MACRO_X_UINT   ("j",     j,                               NULL);
      TEXT_MACRO_X_UINT   ("j1",    (j > 1) ? j-1 : 1,               NULL);
    }

    // write post-processing
    error = executeTemplate(String_cString(template),
                            timestamp,
                            textMacros.data,
                            textMacros.count,
                            CALLBACK_(executeIOOutput,NULL),
                            globalOptions.commandTimeout
                           );

    printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
  }

  return error;
}

LOCAL bool StorageOptical_exists(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  bool existsFlag = FALSE;

  #ifdef HAVE_ISO9660
    iso9660_t *iso9660Handle = iso9660_open_ext(String_cString(storageInfo->storageSpecifier.deviceName),
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
      String pathName = File_getDirectoryName(String_new(),archiveName);
      String fileName = File_getBaseName(String_new(),archiveName,TRUE);
      CdioList_t *cdioList = iso9660_ifs_readdir(iso9660Handle,String_cString(pathName));
      if (cdioList != NULL)
      {
        CdioListNode_t *cdioNextNode = _cdio_list_begin(cdioList);
        while ((cdioNextNode != NULL) && !existsFlag)
        {
          iso9660_stat_t *iso9660Stat = (iso9660_stat_t*)_cdio_list_node_data(cdioNextNode);

          char *s = (char*)malloc(strlen(iso9660Stat->filename)+1);
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
      String_delete(fileName);
      String_delete(pathName);
#endif
      (void)iso9660_close(iso9660Handle);
    }
  #else /* not HAVE_ISO9660 */
    String fileName = String_duplicate(storageInfo->storageSpecifier.deviceName);
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
    const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
    #ifndef NDEBUG
      const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
      if (debugEmulateBlockDevice != NULL)
      {
        deviceName = debugEmulateBlockDevice;
      }
    #endif

    // check if device exists
    if (stringIsEmpty(deviceName))
    {
      return FALSE;
    }
    if (!File_existsCString(deviceName))
    {
      return FALSE;
    }

    iso9660_t *iso9660Handle = iso9660_open_ext(deviceName,ISO_EXTENSION_ALL);
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
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

    return FALSE;
  #endif /* HAVE_ISO9660 */
}

LOCAL bool StorageOptical_isDirectory(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(archiveName != NULL);

  #ifdef HAVE_ISO9660
    const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);
    #ifndef NDEBUG
      const char *debugEmulateBlockDevice = debugGetEmulateBlockDevice();
      if (debugEmulateBlockDevice != NULL)
      {
        deviceName = debugEmulateBlockDevice;
      }
    #endif

    // check if device exists
    if (stringIsEmpty(deviceName))
    {
      return FALSE;
    }
    if (!File_existsCString(deviceName))
    {
      return FALSE;
    }

    iso9660_t *iso9660Handle = iso9660_open_ext(deviceName,ISO_EXTENSION_ALL);
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
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

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
    const char *deviceName = getOpticalDeviceName(&storageInfo->storageSpecifier);

    // check if device exists
    if (stringIsEmpty(deviceName))
    {
      return FALSE;
    }
    if (!File_existsCString(deviceName))
    {
      return FALSE;
    }

    iso9660_t *iso9660Handle = iso9660_open_ext(deviceName,ISO_EXTENSION_ALL);
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
  String directoryName = File_getDirectoryName(String_new(),storageHandle->opticalDisk.write.fileName);
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
      return ERRORX_(OPEN_OPTICAL_DRIVE,0,"%s",String_cString(storageHandle->storageInfo->storageSpecifier.deviceName));
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
        return ERRORX_(OPEN_ISO9660,errno,"%s",String_cString(storageHandle->storageInfo->storageSpecifier.deviceName));
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
                                 ulong         *readBytes
                                )
{
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

  Errors error = ERROR_NONE;

  if (readBytes != NULL) (*readBytes) = 0L;
  #ifdef HAVE_ISO9660
    while (   (bufferSize > 0L)
           && (storageHandle->opticalDisk.read.index < (uint64)storageHandle->opticalDisk.read.iso9660Stat->size)
          )
    {
      // get ISO9660 block index, offset
      uint64 blockIndex  = (int64)(storageHandle->opticalDisk.read.index/ISO_BLOCKSIZE);
      uint   blockOffset = (uint)(storageHandle->opticalDisk.read.index%ISO_BLOCKSIZE);

      if (   (blockIndex != storageHandle->opticalDisk.read.buffer.blockIndex)
          || (blockOffset >= storageHandle->opticalDisk.read.buffer.length)
         )
      {
        // read ISO9660 block
        long int n = iso9660_iso_seek_read(storageHandle->opticalDisk.read.iso9660Handle,
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
      ulong bytesAvail = MIN(bufferSize,storageHandle->opticalDisk.read.buffer.length-blockOffset);
      memcpy(buffer,storageHandle->opticalDisk.read.buffer.data+blockOffset,bytesAvail);

      // adjust buffer, bufferSize, bytes read, index
      buffer = (byte*)buffer+bytesAvail;
      bufferSize -= bytesAvail;
      if (readBytes != NULL) (*readBytes) += bytesAvail;
      storageHandle->opticalDisk.read.index += (uint64)bytesAvail;
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferSize);
    UNUSED_VARIABLE(readBytes);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_ISO9660 */

  return error;
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
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(offset != NULL);

  Errors error = ERROR_UNKNOWN;

  (*offset) = 0LL;

  #ifdef HAVE_ISO9660
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
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_ISO9660 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageOptical_seek(StorageHandle *storageHandle,
                                 uint64        offset
                                )
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  Errors error = ERROR_UNKNOWN;

  #ifdef HAVE_ISO9660
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
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_ISO9660 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL uint64 StorageOptical_getSize(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  uint64 size = 0LL;
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

/***********************************************************************\
* Name   : StorageOptical_getFileInfo
* Purpose: get storage file info
* Input  : fileInfo    - file info variable
*          storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : fileInfo - file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageOptical_getFileInfo(FileInfo          *fileInfo,
                                        const StorageInfo *storageInfo,
                                        ConstString       archiveName
                                       )
{
  assert(fileInfo != NULL);
  assert(storageInfo != NULL);
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_CD)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD)
        );
  assert(archiveName != NULL);

  memClear(fileInfo,sizeof(fileInfo));
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

/*---------------------------------------------------------------------*/

LOCAL Errors StorageOptical_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                              const StorageSpecifier     *storageSpecifier,
                                              ConstString                pathName,
                                              const JobOptions           *jobOptions,
                                              ServerConnectionPriorities serverConnectionPriority
                                             )
{
  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));
  assert(pathName != NULL);
  assert(jobOptions != NULL);

  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);

  #ifdef HAVE_ISO9660
    AutoFreeList autoFreeList;
    AutoFree_init(&autoFreeList);

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
      error = ERRORX_(OPEN_OPTICAL_DRIVE,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.deviceName));
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
        error = ERRORX_(OPEN_ISO9660,errno,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.deviceName));
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
    AutoFreeList autoFreeList;
    AutoFree_init(&autoFreeList);

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
  assert(storageDirectoryListHandle != NULL);
  assert((storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  bool endOfDirectoryFlag = TRUE;

  #ifdef HAVE_ISO9660
    if (storageDirectoryListHandle->opticalDisk.cdioNextNode != NULL)
    {
      // skip directory "." and ".."
      const iso9660_stat_t* iso9660Stat = (iso9660_stat_t*)_cdio_list_node_data(storageDirectoryListHandle->opticalDisk.cdioNextNode);
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
  assert(storageDirectoryListHandle != NULL);
  assert((storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_CD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_BD));

  Errors error = ERROR_NONE;

  #ifdef HAVE_ISO9660
    {
      if (storageDirectoryListHandle->opticalDisk.cdioNextNode != NULL)
      {
        // skip directory "." and ".."
        iso9660_stat_t *iso9660Stat = (iso9660_stat_t*)_cdio_list_node_data(storageDirectoryListHandle->opticalDisk.cdioNextNode);
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

          char *s = (char*)malloc(strlen(iso9660Stat->filename)+1);
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

    error = File_readDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle,fileName,fileInfo);
  #endif /* HAVE_ISO9660 */

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
