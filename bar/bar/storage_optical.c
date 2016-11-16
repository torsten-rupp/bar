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

// execute I/IO info
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
  TextMacro                   textMacros[2];
  bool                        mediumRequestedFlag;
  StorageRequestVolumeResults storageRequestVolumeResult;

  TEXT_MACRO_N_STRING (textMacros[0],"%device",storageInfo->storageSpecifier.deviceName,NULL);
  TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageInfo->requestedVolumeNumber,      NULL);

  if (   (storageInfo->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageInfo->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    // sleep a short time to give hardware time for finishing volume, then unload current volume
    printInfo(1,"Unload medium #%d...",storageInfo->volumeNumber);
    Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
    Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.unloadVolumeCommand),
                        textMacros,SIZE_OF_ARRAY(textMacros),
                        CALLBACK(executeIOOutput,NULL),
                        CALLBACK(executeIOOutput,NULL)
                       );
    printInfo(1,"OK\n");

    storageInfo->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  // request new medium
  mediumRequestedFlag  = FALSE;
  storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_UNKNOWN;
  if      (storageInfo->requestVolumeFunction != NULL)
  {
    // request volume via callback
    mediumRequestedFlag = TRUE;

    // request new medium via call back, unload if requested
    do
    {
      storageRequestVolumeResult = storageInfo->requestVolumeFunction(STORAGE_REQUEST_VOLUME_TYPE_NEW,
                                                                        storageInfo->requestedVolumeNumber,
                                                                        message,
                                                                        storageInfo->requestVolumeUserData
                                                                       );
      if (storageRequestVolumeResult == STORAGE_REQUEST_VOLUME_RESULT_UNLOAD)
      {
        // sleep a short time to give hardware time for finishing volume, then unload current medium
        printInfo(1,"Unload medium...");
        Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.unloadVolumeCommand),
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
  else if (storageInfo->opticalDisk.write.requestVolumeCommand != NULL)
  {
    // request volume via external command
    mediumRequestedFlag = TRUE;

    // request new volume via external command
    printInfo(1,"Request new medium #%d...",storageInfo->requestedVolumeNumber);
    if (Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.requestVolumeCommand),
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
    // request volume via console
    if (storageInfo->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
    {
      if (waitFlag)
      {
        mediumRequestedFlag = TRUE;

        printInfo(0,"Please insert medium #%d into drive '%s' and press ENTER to continue\n",storageInfo->requestedVolumeNumber,String_cString(storageInfo->storageSpecifier.deviceName));
        Misc_waitEnter();

        storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_OK;
      }
      else
      {
        printInfo(0,"Please insert medium #%d into drive '%s'\n",storageInfo->requestedVolumeNumber,String_cString(storageInfo->storageSpecifier.deviceName));
      }
    }
    else
    {
      if (waitFlag)
      {
        mediumRequestedFlag = TRUE;

        printInfo(0,"Press ENTER to continue\n");
        Misc_waitEnter();

        storageRequestVolumeResult = STORAGE_REQUEST_VOLUME_RESULT_OK;
      }
    }

    storageInfo->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (mediumRequestedFlag)
  {
    switch (storageRequestVolumeResult)
    {
      case STORAGE_REQUEST_VOLUME_RESULT_OK:
        // load medium, then sleep a short time to give hardware time for reading medium information
        printInfo(1,"Load medium #%d...",storageInfo->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            CALLBACK(executeIOOutput,NULL),
                            CALLBACK(executeIOOutput,NULL)
                           );
        Misc_udelay(LOAD_VOLUME_DELAY_TIME);
        printInfo(1,"OK\n");

        // store new medium number
        storageInfo->volumeNumber = storageInfo->requestedVolumeNumber;

        // update status info
        storageInfo->runningInfo.volumeNumber = storageInfo->volumeNumber;
        updateStorageStatusInfo(storageInfo);

        storageInfo->volumeState = STORAGE_VOLUME_STATE_LOADED;
        return ERROR_NONE;
        break;
      case STORAGE_REQUEST_VOLUME_RESULT_ABORTED:
        // load medium
        printInfo(1,"Load medium #%d...",storageInfo->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.loadVolumeCommand),
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
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

//fprintf(stderr,"%s,%d: mkisofs line=%s\n",__FILE__,__LINE__,String_cString(line));
  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*?([0-9\\.]+)%.*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
//fprintf(stderr,"%s,%d: mkisofs: %s -> %lf\n",__FILE__,__LINE__,String_cString(line),p);
    storageInfo->runningInfo.volumeProgress = ((double)storageInfo->opticalDisk.write.step*100.0+p)/(double)(storageInfo->opticalDisk.write.steps*100);
    updateStorageStatusInfo(storageInfo);
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
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

//fprintf(stderr,"%s,%d: dvdisaster line=%s\n",__FILE__,__LINE__,String_cString(line));
  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*adding space\\): +([0-9\\.]+)%",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
//fprintf(stderr,"%s,%d: dvdisaster add space: %s -> %lf\n",__FILE__,__LINE__,String_cString(line),p);
    storageInfo->runningInfo.volumeProgress = ((double)(storageInfo->opticalDisk.write.step+0)*100.0+p)/(double)(storageInfo->opticalDisk.write.steps*100);
    updateStorageStatusInfo(storageInfo);
  }
  if (String_matchCString(line,STRING_BEGIN,".*generation: +([0-9\\.]+)%",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
//fprintf(stderr,"%s,%d: dvdisaster codes: %s -> %lf\n",__FILE__,__LINE__,String_cString(line),p);
    storageInfo->runningInfo.volumeProgress = ((double)(storageInfo->opticalDisk.write.step+1)*100.0+p)/(double)(storageInfo->opticalDisk.write.steps*100);
    updateStorageStatusInfo(storageInfo);
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
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

//fprintf(stderr,"%s,%d: blank line=%s\n",__FILE__,__LINE__,String_cString(line));
  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*?([0-9\\.]+)%.*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
//fprintf(stderr,"%s,%d: blank: %s -> %lf\n",__FILE__,__LINE__,String_cString(line),p);
    storageInfo->runningInfo.volumeProgress = ((double)storageInfo->opticalDisk.write.step*100.0+p)/(double)(storageInfo->opticalDisk.write.steps*100);
    updateStorageStatusInfo(storageInfo);
  }
  String_delete(s);
}

/***********************************************************************\
* Name   : executeIOblankStdout
* Purpose: process blank stdout
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
* Purpose: process blank stderr
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
* Name   : executeIOgrowisofs
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
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

//fprintf(stderr,"%s,%d: growisofs line=%s\n",__FILE__,__LINE__,String_cString(line));
  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*?([0-9\\.]+)%.*",NULL,STRING_NO_ASSIGN,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
//fprintf(stderr,"%s,%d: growisofs: %s -> %lf\n",__FILE__,__LINE__,String_cString(line),p);
    storageInfo->runningInfo.volumeProgress = ((double)storageInfo->opticalDisk.write.step*100.0+p)/(double)(storageInfo->opticalDisk.write.steps*100);
    updateStorageStatusInfo(storageInfo);
  }
  String_delete(s);
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
* Name   : executeIOgrowisofs
* Purpose: process growisofs output
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

LOCAL Errors StorageOptical_init(StorageInfo            *storageInfo,
                                 const StorageSpecifier *storageSpecifier,
                                 const JobOptions       *jobOptions
                                )
{
  Errors         error;
  OpticalDisk    opticalDisk;
  uint64         volumeSize,maxMediumSize;
  FileSystemInfo fileSystemInfo;
  String         sourceFileName,fileBaseName,destinationFileName;

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
  switch (storageInfo->storageSpecifier.type)
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

  storageInfo->opticalDisk.write.requestVolumeCommand   = opticalDisk.requestVolumeCommand;
  storageInfo->opticalDisk.write.unloadVolumeCommand    = opticalDisk.unloadVolumeCommand;
  storageInfo->opticalDisk.write.loadVolumeCommand      = opticalDisk.loadVolumeCommand;
  storageInfo->opticalDisk.write.volumeSize             = volumeSize;
  storageInfo->opticalDisk.write.imagePreProcessCommand = opticalDisk.imagePreProcessCommand;
  storageInfo->opticalDisk.write.imagePostProcessCommand= opticalDisk.imagePostProcessCommand;
  storageInfo->opticalDisk.write.imageCommand           = opticalDisk.imageCommand;
  storageInfo->opticalDisk.write.eccPreProcessCommand   = opticalDisk.eccPreProcessCommand;
  storageInfo->opticalDisk.write.eccPostProcessCommand  = opticalDisk.eccPostProcessCommand;
  storageInfo->opticalDisk.write.eccCommand             = opticalDisk.eccCommand;
  storageInfo->opticalDisk.write.blankCommand           = opticalDisk.blankCommand;
  storageInfo->opticalDisk.write.writePreProcessCommand = opticalDisk.writePreProcessCommand;
  storageInfo->opticalDisk.write.writePostProcessCommand= opticalDisk.writePostProcessCommand;
  storageInfo->opticalDisk.write.writeCommand           = opticalDisk.writeCommand;
  storageInfo->opticalDisk.write.writeImageCommand      = opticalDisk.writeImageCommand;
  storageInfo->opticalDisk.write.steps                  = 1;
  if ((jobOptions != NULL) && jobOptions->errorCorrectionCodesFlag) storageInfo->opticalDisk.write.steps += 3;
  if ((jobOptions != NULL) && jobOptions->blankFlag) storageInfo->opticalDisk.write.steps += 1;
  storageInfo->opticalDisk.write.directory              = String_new();
  storageInfo->opticalDisk.write.step                   = 0;
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
    return error;
  }

  if ((jobOptions != NULL) && !jobOptions->noBAROnMediumFlag)
  {
    // store a copy of BAR executable on medium (ignore errors)
    sourceFileName = String_newCString(globalOptions.barExecutable);
    fileBaseName = File_getFileBaseName(String_new(),sourceFileName);
    destinationFileName = File_appendFileName(String_duplicate(storageInfo->opticalDisk.write.directory),fileBaseName);
    File_copy(sourceFileName,destinationFileName);
    StringList_append(&storageInfo->opticalDisk.write.fileNameList,destinationFileName);
    String_delete(destinationFileName);
    String_delete(fileBaseName);
    String_delete(sourceFileName);
  }

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

  return error;
}

LOCAL Errors StorageOptical_preProcess(StorageInfo *storageInfo,
                                       ConstString archiveName,
                                       time_t      timestamp,
                                       bool        initialFlag
                                      )
{
  TextMacro   textMacros[3];
  Errors      error;
  ConstString template;
  String      script;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  UNUSED_VARIABLE(initialFlag);

  error = ERROR_NONE;
  if ((storageInfo->jobOptions == NULL) || !storageInfo->jobOptions->dryRunFlag)
  {
    // request next medium
    if (storageInfo->opticalDisk.write.newVolumeFlag)
    {
      storageInfo->opticalDisk.write.number++;
      storageInfo->opticalDisk.write.newVolumeFlag = FALSE;

      storageInfo->requestedVolumeNumber = storageInfo->opticalDisk.write.number;
    }

    // check if new medium is required
    if (storageInfo->volumeNumber != storageInfo->requestedVolumeNumber)
    {
      // request load new medium
      error = requestNewOpticalMedium(storageInfo,NULL,FALSE);
    }

    // init macros
    TEXT_MACRO_N_STRING (textMacros[0],"%device",storageInfo->storageSpecifier.deviceName,NULL);
    TEXT_MACRO_N_STRING (textMacros[1],"%file",  archiveName,                               NULL);
    TEXT_MACRO_N_INTEGER(textMacros[2],"%number",storageInfo->requestedVolumeNumber,      NULL);

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

LOCAL Errors StorageOptical_postProcess(StorageInfo *storageInfo,
                                        ConstString archiveName,
                                        time_t      timestamp,
                                        bool        finalFlag
                                       )
{
  Errors        error;
  ExecuteIOInfo executeIOInfo;
  String        imageFileName;
  TextMacro     textMacros[6];
  String        fileName;
  FileInfo      fileInfo;
  bool          retryFlag;
  ConstString   template;
  String        script;

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;

  if ((storageInfo->jobOptions == NULL) || !storageInfo->jobOptions->dryRunFlag)
  {
    if (   (storageInfo->opticalDisk.write.totalSize > storageInfo->opticalDisk.write.volumeSize)
        || (finalFlag && (storageInfo->opticalDisk.write.totalSize > 0LL))
       )
    {
      // medium size limit reached or final medium -> create medium and request new volume

      // init variables
      storageInfo->opticalDisk.write.step = 0;
      executeIOInfo.storageInfo           = storageInfo;
      StringList_init(&executeIOInfo.stderrList);

      // update info
      storageInfo->runningInfo.volumeProgress = 0.0;
      updateStorageStatusInfo(storageInfo);

      // get temporary image file name
      imageFileName = String_new();
      error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
      if (error != ERROR_NONE)
      {
        StringList_done(&executeIOInfo.stderrList);
        return error;
      }

      // init macros
      TEXT_MACRO_N_STRING (textMacros[0],"%device",   storageInfo->storageSpecifier.deviceName,NULL);
      TEXT_MACRO_N_STRING (textMacros[1],"%directory",storageInfo->opticalDisk.write.directory,NULL);
      TEXT_MACRO_N_STRING (textMacros[2],"%image",    imageFileName,                             NULL);
      TEXT_MACRO_N_INTEGER(textMacros[3],"%sectors",  0,                                         NULL);
      TEXT_MACRO_N_STRING (textMacros[4],"%file",     archiveName,                               NULL);
      TEXT_MACRO_N_INTEGER(textMacros[5],"%number",   storageInfo->volumeNumber,               NULL);

      if ((storageInfo->jobOptions != NULL) && (storageInfo->jobOptions->alwaysCreateImageFlag || storageInfo->jobOptions->errorCorrectionCodesFlag))
      {
        // create medium image
        printInfo(1,"Make medium image #%d with %d part(s)...",storageInfo->opticalDisk.write.number,StringList_count(&storageInfo->opticalDisk.write.fileNameList));
        StringList_clear(&executeIOInfo.stderrList);
        error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.imageCommand),
                                    textMacros,SIZE_OF_ARRAY(textMacros),
                                    CALLBACK(executeIOmkisofsStdout,&executeIOInfo),
                                    CALLBACK(executeIOmkisofsStderr,&executeIOInfo)
                                   );
        if (error != ERROR_NONE)
        {
          printInfo(1,"FAIL\n");
          File_delete(imageFileName,FALSE);
          String_delete(imageFileName);
          StringList_done(&executeIOInfo.stderrList);
          return error;
        }
        File_getFileInfo(imageFileName,&fileInfo);
        printInfo(1,"OK (%llu bytes)\n",fileInfo.size);
        storageInfo->opticalDisk.write.step++;

        if (storageInfo->jobOptions->errorCorrectionCodesFlag)
        {
          // add error-correction codes to medium image
          printInfo(1,"Add ECC to image #%d...",storageInfo->opticalDisk.write.number);
          StringList_clear(&executeIOInfo.stderrList);
          error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.eccCommand),
                                      textMacros,SIZE_OF_ARRAY(textMacros),
                                      CALLBACK(executeIOdvdisasterStdout,&executeIOInfo),
                                      CALLBACK(executeIOdvdisasterStderr,&executeIOInfo)
                                     );
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL\n");
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);
            StringList_done(&executeIOInfo.stderrList);
            return error;
          }
          File_getFileInfo(imageFileName,&fileInfo);
          printInfo(1,"OK (%llu bytes)\n",fileInfo.size);
          storageInfo->opticalDisk.write.step++;
        }

        // get number of image sectors
        if (File_getFileInfo(imageFileName,&fileInfo) == ERROR_NONE)
        {
          TEXT_MACRO_N_INTEGER(textMacros[3],"%sectors",(ulong)(fileInfo.size/2048LL),NULL);
        }

        // check if new medium is required
        if (storageInfo->volumeNumber != storageInfo->requestedVolumeNumber)
        {
          // request load new medium
          error = requestNewOpticalMedium(storageInfo,NULL,TRUE);
          if (error != ERROR_NONE)
          {
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);
            StringList_done(&executeIOInfo.stderrList);
            return error;
          }
          updateStorageStatusInfo(storageInfo);
        }

        // blank mediuam
        if (storageInfo->jobOptions->blankFlag)
        {
          // add error-correction codes to medium image
          printInfo(1,"Blank medium #%d...",storageInfo->opticalDisk.write.number);
          StringList_clear(&executeIOInfo.stderrList);
          error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.blankCommand),
                                      textMacros,SIZE_OF_ARRAY(textMacros),
                                      CALLBACK(executeIOblankStdout,&executeIOInfo),
                                      CALLBACK(executeIOblankStderr,&executeIOInfo)
                                     );
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL\n");
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);
            StringList_done(&executeIOInfo.stderrList);
            return error;
          }
          printInfo(1,"OK\n");
          storageInfo->opticalDisk.write.step++;
        }

        retryFlag = TRUE;
        do
        {
          retryFlag = FALSE;

          // write image to medium
          printInfo(1,"Write image to medium #%d...",storageInfo->opticalDisk.write.number);
          StringList_clear(&executeIOInfo.stderrList);
          error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.writeImageCommand),
                                      textMacros,SIZE_OF_ARRAY(textMacros),
                                      CALLBACK(executeIOgrowisofsStdout,&executeIOInfo),
                                      CALLBACK(executeIOgrowisofsStderr,&executeIOInfo)
                                     );
          if (error == ERROR_NONE)
          {
            printInfo(1,"OK\n");
            retryFlag = FALSE;
          }
          else
          {
            printInfo(1,"FAIL\n");
            if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
            {
              retryFlag = Misc_getYesNo("Retry write image to medium?");
            }
            else
            {
              retryFlag = (requestNewOpticalMedium(storageInfo,Error_getText(error),TRUE) == ERROR_NONE);
            }
          }
        }
        while ((error != ERROR_NONE) && retryFlag);
        if (error != ERROR_NONE)
        {
          File_delete(imageFileName,FALSE);
          String_delete(imageFileName);
          StringList_done(&executeIOInfo.stderrList);
          return error;
        }
        storageInfo->opticalDisk.write.step++;
      }
      else
      {
        // check if new medium is required
        if (storageInfo->volumeNumber != storageInfo->requestedVolumeNumber)
        {
          // request load new medium
          error = requestNewOpticalMedium(storageInfo,NULL,TRUE);
          if (error != ERROR_NONE)
          {
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);
            StringList_done(&executeIOInfo.stderrList);
            return error;
          }
          updateStorageStatusInfo(storageInfo);
        }

        // blank mediuam
        if (storageInfo->jobOptions->blankFlag)
        {
          // add error-correction codes to medium image
          printInfo(1,"Blank medium #%d...",storageInfo->opticalDisk.write.number);
          StringList_clear(&executeIOInfo.stderrList);
          error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.blankCommand),
                                      textMacros,SIZE_OF_ARRAY(textMacros),
                                      CALLBACK(executeIOblankStdout,&executeIOInfo),
                                      CALLBACK(executeIOblankStderr,&executeIOInfo)
                                     );
          if (error != ERROR_NONE)
          {
            printInfo(1,"FAIL\n");
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);
            StringList_done(&executeIOInfo.stderrList);
            return error;
          }
          printInfo(1,"OK\n");
          storageInfo->opticalDisk.write.step++;
        }

        retryFlag = TRUE;
        do
        {
          retryFlag = FALSE;

          // write to medium
          printInfo(1,"Write medium #%d with %d part(s)...",storageInfo->opticalDisk.write.number,StringList_count(&storageInfo->opticalDisk.write.fileNameList));
          StringList_clear(&executeIOInfo.stderrList);
          error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.writeCommand),
                                      textMacros,SIZE_OF_ARRAY(textMacros),
                                      CALLBACK(executeIOgrowisofsStdout,&executeIOInfo),
                                      CALLBACK(executeIOgrowisofsStderr,&executeIOInfo)
                                     );
          if (error == ERROR_NONE)
          {
            printInfo(1,"OK\n");
          }
          else
          {
            printInfo(1,"FAIL (error: %s)\n",Error_getText(error));
            if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
            {
              retryFlag = Misc_getYesNo("Retry write image to medium?");
            }
            else
            {
              retryFlag = (requestNewOpticalMedium(storageInfo,Error_getText(error),TRUE) == ERROR_NONE);
            }
          }
        }
        while ((error != ERROR_NONE) && retryFlag);
        if (error != ERROR_NONE)
        {
          File_delete(imageFileName,FALSE);
          String_delete(imageFileName);
          StringList_done(&executeIOInfo.stderrList);
          return error;
        }
        storageInfo->opticalDisk.write.step++;
      }

      // delete image
      File_delete(imageFileName,FALSE);
      String_delete(imageFileName);

      // update info
      storageInfo->runningInfo.volumeProgress = 1.0;
      updateStorageStatusInfo(storageInfo);

      // delete stored files
      fileName = String_new();
      while (!StringList_isEmpty(&storageInfo->opticalDisk.write.fileNameList))
      {
        StringList_removeFirst(&storageInfo->opticalDisk.write.fileNameList,fileName);
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
      storageInfo->opticalDisk.write.newVolumeFlag = TRUE;
      storageInfo->opticalDisk.write.totalSize     = 0;

      // free resources
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
    storageInfo->opticalDisk.write.step     = storageInfo->opticalDisk.write.steps;
    storageInfo->runningInfo.volumeProgress = 1.0;
    updateStorageStatusInfo(storageInfo);
  }

  return error;
}

LOCAL Errors StorageOptical_unloadVolume(StorageInfo *storageInfo)
{
  Errors    error;
  TextMacro textMacros[1];

  assert(storageInfo != NULL);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_UNKNOWN;

  TEXT_MACRO_N_STRING(textMacros[0],"%device",storageInfo->storageSpecifier.deviceName,NULL);
  error = Misc_executeCommand(String_cString(storageInfo->opticalDisk.write.unloadVolumeCommand),
                              textMacros,SIZE_OF_ARRAY(textMacros),
                              CALLBACK(executeIOOutput,NULL),
                              CALLBACK(executeIOOutput,NULL)
                             );
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL bool StorageOptical_exists(StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return FALSE;
}

LOCAL Errors StorageOptical_create(StorageHandle *storageHandle,
                                   ConstString   archiveName,
                                   uint64        archiveSize
                                  )
{
  Errors error;
  String directoryName;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(archiveSize);

  // init variables
  #ifdef HAVE_ISO9660
    storageHandle->opticalDisk.read.iso9660Handle     = NULL;
    storageHandle->opticalDisk.read.iso9660Stat       = NULL;
    storageHandle->opticalDisk.read.index             = 0LL;
    storageHandle->opticalDisk.read.buffer.blockIndex = 0LL;
    storageHandle->opticalDisk.read.buffer.length     = 0L;
  #endif /* HAVE_ISO9660 */
  storageHandle->opticalDisk.write.fileName               = String_new();

  // create file name
  String_set(storageHandle->opticalDisk.write.fileName,storageHandle->storageInfo->opticalDisk.write.directory);
  File_appendFileName(storageHandle->opticalDisk.write.fileName,archiveName);

  if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
  {
    // create directory if not existing
    directoryName = File_getFilePathName(String_new(),storageHandle->opticalDisk.write.fileName);
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
    error = File_open(&storageHandle->opticalDisk.write.fileHandle,
                      storageHandle->opticalDisk.write.fileName,
                      FILE_OPEN_CREATE
                     );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  DEBUG_ADD_RESOURCE_TRACE(&storageHandle->opticalDisk,sizeof(storageHandle->opticalDisk));

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
    {
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

      // check if device exists
      if (String_isEmpty(storageHandle->storageInfo->storageSpecifier.deviceName))
      {
        free(storageHandle->opticalDisk.read.buffer.data);
        return ERROR_NO_DEVICE_NAME;
      }
      if (!File_exists(storageHandle->storageInfo->storageSpecifier.deviceName))
      {
        free(storageHandle->opticalDisk.read.buffer.data);
        return ERRORX_(OPTICAL_DISK_NOT_FOUND,0,"%s",String_cString(storageHandle->storageInfo->storageSpecifier.deviceName));
      }

      // open optical disk/ISO 9660 file
      storageHandle->opticalDisk.read.iso9660Handle = iso9660_open_ext(String_cString(storageHandle->storageInfo->storageSpecifier.deviceName),ISO_EXTENSION_ROCK_RIDGE);//ISO_EXTENSION_ALL);
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

      DEBUG_ADD_RESOURCE_TRACE(&storageHandle->opticalDisk,sizeof(storageHandle->opticalDisk));
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
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->opticalDisk);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->opticalDisk,sizeof(storageHandle->opticalDisk));

  switch (storageHandle->mode)
  {
    case STORAGE_MODE_READ:
      #ifdef HAVE_ISO9660
        assert(storageHandle->opticalDisk.read.iso9660Handle != NULL);
        assert(storageHandle->opticalDisk.read.iso9660Stat != NULL);

        free(storageHandle->opticalDisk.read.iso9660Stat);
        iso9660_close(storageHandle->opticalDisk.read.iso9660Handle);
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_MODE_WRITE:
      if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
      {
        storageHandle->storageInfo->opticalDisk.write.totalSize += File_getSize(&storageHandle->opticalDisk.write.fileHandle);
        File_close(&storageHandle->opticalDisk.write.fileHandle);
      }
      StringList_append(&storageHandle->storageInfo->opticalDisk.write.fileNameList,storageHandle->opticalDisk.write.fileName);
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
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->opticalDisk);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  #ifdef HAVE_ISO9660
    assert(storageHandle->opticalDisk.read.iso9660Handle != NULL);
    assert(storageHandle->opticalDisk.read.iso9660Stat != NULL);

    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
      return storageHandle->opticalDisk.read.index >= storageHandle->opticalDisk.read.iso9660Stat->size;
    }
    else
    {
      return TRUE;
    }
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
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->opticalDisk);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(buffer != NULL);

  if (bytesRead != NULL) (*bytesRead) = 0L;
  error = ERROR_NONE;
  #ifdef HAVE_ISO9660
    {
      uint64   blockIndex;
      uint     blockOffset;
      long int n;
      ulong    bytesAvail;

      assert(storageHandle->opticalDisk.read.iso9660Handle != NULL);
      assert(storageHandle->opticalDisk.read.iso9660Stat != NULL);

      if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
      {
        assert(storageHandle->opticalDisk.read.buffer.data != NULL);

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
              error = ERROR_(IO_ERROR,errno);
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
      }
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferSize);
    UNUSED_VARIABLE(bytesRead);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_ISO9660 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageOptical_write(StorageHandle *storageHandle,
                                  const void    *buffer,
                                  ulong         bufferLength
                                 )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->opticalDisk);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(buffer != NULL);

  error = ERROR_NONE;
  if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
  {
    error = File_write(&storageHandle->opticalDisk.write.fileHandle,buffer,bufferLength);
  }

  return error;
}

LOCAL Errors StorageOptical_tell(StorageHandle *storageHandle,
                                 uint64        *offset
                                )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->opticalDisk);
  assert(storageHandle->storageInfo != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  #ifdef HAVE_ISO9660
    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
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
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);
  #endif /* HAVE_ISO9660 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageOptical_seek(StorageHandle *storageHandle,
                                 uint64        offset
                                )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->opticalDisk);
  assert(storageHandle->storageInfo != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  error = ERROR_NONE;
  #ifdef HAVE_ISO9660
    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
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
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);
  #endif /* HAVE_ISO9660 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL uint64 StorageOptical_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->opticalDisk);
  assert(storageHandle->storageInfo != NULL);
  assert((storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));

  size = 0LL;
  #ifdef HAVE_ISO9660
    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
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
    }
  #else /* not HAVE_ISO9660 */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_ISO9660 */

  return size;
}

LOCAL Errors StorageOptical_delete(StorageInfo *storageInfo,
                                   ConstString archiveName
                                  )
{
  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);
  assert((storageInfo->storageSpecifier.type == STORAGE_TYPE_CD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_DVD) || (storageInfo->storageSpecifier.type == STORAGE_TYPE_BD));
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return ERROR_FUNCTION_NOT_SUPPORTED;
}

#if 0
still not complete
LOCAL Errors StorageOptical_getFileInfo(StorageInfo *storageInfo,
                                        ConstString fileName,
                                        FileInfo    *fileInfo
                                       )
{
  String infoFileName;
  Errors error;

  assert(storageInfo != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));
  assert(fileInfo != NULL);

  infoFileName = (fileName != NULL) ? fileName : storageInfo->storageSpecifier.archiveName;
  memset(fileInfo,0,sizeof(fileInfo));

  error = ERROR_FUNCTION_NOT_SUPPORTED;
  assert(error != ERROR_UNKNOWN);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageOptical_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                              const StorageSpecifier     *storageSpecifier,
                                              ConstString                archiveName,
                                              const JobOptions           *jobOptions,
                                              ServerConnectionPriorities serverConnectionPriority
                                             )
{
  AutoFreeList autoFreeList;
  Errors       error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert((storageSpecifier->type == STORAGE_TYPE_CD) || (storageSpecifier->type == STORAGE_TYPE_DVD) || (storageSpecifier->type == STORAGE_TYPE_BD));
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(jobOptions);
  UNUSED_VARIABLE(serverConnectionPriority);

  // initialize variables
  AutoFree_init(&autoFreeList);
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
