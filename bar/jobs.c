/***********************************************************************\
*
* Contents: Backup ARchiver job functions
* Systems: all
*
\***********************************************************************/

#define __JOBS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>
#include <locale.h>
#include <time.h>
#include <signal.h>
#include <assert.h>

#include "common/global.h"
#include "common/autofree.h"
#include "common/lists.h"
#include "common/strings.h"
#include "common/stringmaps.h"
#include "common/arrays.h"
#include "common/configvalues.h"
#include "common/threads.h"
#include "common/semaphores.h"
#include "common/msgqueues.h"
#include "common/stringlists.h"
#include "common/misc.h"
#include "common/network.h"
#include "common/files.h"
#include "common/devices.h"
#include "common/patterns.h"
#include "common/patternlists.h"
#include "common/passwords.h"

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "errors.h"
#include "configuration.h"
#include "entrylists.h"
#include "archives.h"
#include "storage.h"
#include "index/index.h"
#include "continuous.h"
#include "server_io.h"
#include "connector.h"

#include "jobs.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define LOCK_TIMEOUT                             (10L*60L*MS_PER_SECOND)  // general lock timeout [ms]

#define MAX_SCHEDULE_CATCH_TIME                  30       // max. schedule catch time [days]

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
SlaveList slaveList;
JobList   jobList;

#ifndef NDEBUG
uint64 jobListLockTimestamp;
#endif /* NDEBUG */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#if 0
//TODO: remove
/***********************************************************************\
* Name   : equalsScheduleNode
* Purpose: compare schedule nodes if equals
* Input  : scheduleNode1,scheduleNode2 - schedule nodes
* Output : -
* Return : TRUE iff scheduleNode1 = scheduleNode2
* Notes  : -
\***********************************************************************/

LOCAL bool equalsScheduleNode(const ScheduleNode *scheduleNode1, const ScheduleNode *scheduleNode2)
{
  assert(scheduleNode1 != NULL);
  assert(scheduleNode2 != NULL);

  if (   (scheduleNode1->date.year  != scheduleNode2->date.year )
      || (scheduleNode1->date.month != scheduleNode2->date.month)
      || (scheduleNode1->date.day   != scheduleNode2->date.day  )
     )
  {
    return 0;
  }

  if (scheduleNode1->weekDaySet != scheduleNode2->weekDaySet)
  {
    return 0;
  }

  if (   (scheduleNode1->time.hour   != scheduleNode2->time.hour )
      || (scheduleNode1->time.minute != scheduleNode2->time.minute)
     )
  {
    return 0;
  }

  if (scheduleNode1->archiveType != scheduleNode2->archiveType)
  {
    return 0;
  }

  if (scheduleNode1->interval != scheduleNode2->interval)
  {
    return 0;
  }

  if (!String_equals(scheduleNode1->customText,scheduleNode2->customText))
  {
    return 0;
  }

  if (scheduleNode1->minKeep != scheduleNode2->minKeep)
  {
    return 0;
  }

  if (scheduleNode1->maxKeep != scheduleNode2->maxKeep)
  {
    return 0;
  }

  if (scheduleNode1->maxAge != scheduleNode2->maxAge)
  {
    return 0;
  }

  if (scheduleNode1->noStorage != scheduleNode2->noStorage)
  {
    return 0;
  }

  return 1;
}
#endif

#ifndef NDEBUG
/***********************************************************************\
* Name   : debugIsPersistenceListSorted
* Purpose: check if persistence list is sorted ascending
* Input  : persistenceList - persistence list
* Output : -
* Return : TRUE iff sorted ascending
* Notes  : -
\***********************************************************************/

LOCAL bool debugIsPersistenceListSorted(const PersistenceList *persistenceList)
{
  const PersistenceNode *persistenceNode;
  LIST_ITERATE(persistenceList,persistenceNode)
  {
    if (   (persistenceNode->next != NULL)
        && (persistenceNode->next->maxAge != AGE_FOREVER)
        && (persistenceNode->next->maxAge < persistenceNode->maxAge)
       )
    {
      return FALSE;
    }
  }

  return TRUE;
}
#endif

ScheduleNode *Job_newScheduleNode(ConstString scheduleUUID)
{
  ScheduleNode *scheduleNode;

  scheduleNode = LIST_NEW_NODE(ScheduleNode);
  if (scheduleNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  scheduleNode->uuid                      = String_new();
  scheduleNode->parentUUID                = NULL;
  scheduleNode->date.year                 = DATE_ANY;
  scheduleNode->date.month                = DATE_ANY;
  scheduleNode->date.day                  = DATE_ANY;
  scheduleNode->weekDaySet                = WEEKDAY_SET_ANY;
  scheduleNode->time.hour                 = TIME_ANY;
  scheduleNode->time.minute               = TIME_ANY;
  scheduleNode->archiveType               = ARCHIVE_TYPE_NORMAL;
  scheduleNode->interval                  = 0;
  scheduleNode->customText                = String_new();
  scheduleNode->beginTime.hour            = TIME_ANY;
  scheduleNode->beginTime.minute          = TIME_ANY;
  scheduleNode->endTime.hour              = TIME_ANY;
  scheduleNode->endTime.minute            = TIME_ANY;
  scheduleNode->testCreatedArchives       = FALSE;
  scheduleNode->noStorage                 = FALSE;
  scheduleNode->enabled                   = FALSE;

  scheduleNode->deprecatedPersistenceFlag = FALSE;
  scheduleNode->minKeep                   = 0;
  scheduleNode->maxKeep                   = 0;
  scheduleNode->maxAge                    = AGE_FOREVER;

  scheduleNode->active                    = FALSE;
  scheduleNode->lastExecutedDateTime      = 0LL;

  scheduleNode->totalEntityCount          = 0L;
  scheduleNode->totalStorageCount         = 0L;
  scheduleNode->totalStorageSize          = 0LL;
  scheduleNode->totalEntryCount           = 0LL;
  scheduleNode->totalEntrySize            = 0LL;

  if (!String_isEmpty(scheduleUUID))
  {
    String_set(scheduleNode->uuid,scheduleUUID);
  }
  else
  {
    Misc_getUUID(scheduleNode->uuid);
  }

  return scheduleNode;
}

ScheduleNode *Job_duplicateScheduleNode(ScheduleNode *fromScheduleNode,
                                        void         *userData
                                       )
{
  assert(fromScheduleNode != NULL);

  UNUSED_VARIABLE(userData);

  ScheduleNode *scheduleNode = LIST_NEW_NODE(ScheduleNode);
  if (scheduleNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  scheduleNode->uuid                      = Misc_getUUID(String_new());
  scheduleNode->parentUUID                = String_duplicate(fromScheduleNode->parentUUID);
  scheduleNode->date.year                 = fromScheduleNode->date.year;
  scheduleNode->date.month                = fromScheduleNode->date.month;
  scheduleNode->date.day                  = fromScheduleNode->date.day;
  scheduleNode->weekDaySet                = fromScheduleNode->weekDaySet;
  scheduleNode->time.hour                 = fromScheduleNode->time.hour;
  scheduleNode->time.minute               = fromScheduleNode->time.minute;
  scheduleNode->archiveType               = fromScheduleNode->archiveType;
  scheduleNode->interval                  = fromScheduleNode->interval;
  scheduleNode->customText                = String_duplicate(fromScheduleNode->customText);
  scheduleNode->beginTime                 = fromScheduleNode->beginTime;
  scheduleNode->endTime                   = fromScheduleNode->endTime;
  scheduleNode->testCreatedArchives       = fromScheduleNode->testCreatedArchives;
  scheduleNode->noStorage                 = fromScheduleNode->noStorage;
  scheduleNode->enabled                   = fromScheduleNode->enabled;

  scheduleNode->active                    = fromScheduleNode->active;
  scheduleNode->lastExecutedDateTime      = fromScheduleNode->lastExecutedDateTime;

  scheduleNode->totalEntityCount          = 0L;
  scheduleNode->totalStorageCount         = 0L;
  scheduleNode->totalStorageSize          = 0LL;
  scheduleNode->totalEntryCount           = 0LL;
  scheduleNode->totalEntrySize            = 0LL;

// TODO: remove
  // deprecated
  scheduleNode->deprecatedPersistenceFlag = fromScheduleNode->deprecatedPersistenceFlag;
  scheduleNode->minKeep                   = fromScheduleNode->minKeep;
  scheduleNode->maxKeep                   = fromScheduleNode->maxKeep;
  scheduleNode->maxAge                    = fromScheduleNode->maxAge;

  return scheduleNode;
}

void Job_freeScheduleNode(ScheduleNode *scheduleNode, void *userData)
{
  assert(scheduleNode != NULL);
  assert(scheduleNode->uuid != NULL);
  assert(scheduleNode->customText != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(scheduleNode->customText);
  String_delete(scheduleNode->parentUUID);
  String_delete(scheduleNode->uuid);
}

void Job_deleteScheduleNode(ScheduleNode *scheduleNode)
{
  assert(scheduleNode != NULL);

  Job_freeScheduleNode(scheduleNode,NULL);
  LIST_DELETE_NODE(scheduleNode);
}

PersistenceNode *Job_newPersistenceNode(ArchiveTypes archiveType,
                                        int          minKeep,
                                        int          maxKeep,
                                        int          maxAge,
                                        ConstString  moveTo
                                       )
{
  PersistenceNode *persistenceNode = LIST_NEW_NODE(PersistenceNode);
  if (persistenceNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  #ifndef NDEBUG
    persistenceNode->id        = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  #else
    persistenceNode->id        = Misc_getId();
  #endif
  persistenceNode->archiveType = archiveType;
  persistenceNode->minKeep     = minKeep;
  persistenceNode->maxKeep     = maxKeep;
  persistenceNode->maxAge      = maxAge;
  persistenceNode->moveTo      = String_duplicate(moveTo);

  return persistenceNode;
}

PersistenceNode *Job_duplicatePersistenceNode(PersistenceNode *fromPersistenceNode,
                                              void            *userData
                                             )
{
  assert(fromPersistenceNode != NULL);

  UNUSED_VARIABLE(userData);

  PersistenceNode *persistenceNode = LIST_NEW_NODE(PersistenceNode);
  if (persistenceNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  #ifndef NDEBUG
    persistenceNode->id          = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  #else
    persistenceNode->id          = Misc_getId();
  #endif
  persistenceNode->archiveType = fromPersistenceNode->archiveType;
  persistenceNode->minKeep     = fromPersistenceNode->minKeep;
  persistenceNode->maxKeep     = fromPersistenceNode->maxKeep;
  persistenceNode->maxAge      = fromPersistenceNode->maxAge;
  persistenceNode->moveTo      = String_duplicate(fromPersistenceNode->moveTo);

  return persistenceNode;
}

void Job_insertPersistenceNode(PersistenceList *persistenceList,
                               PersistenceNode *persistenceNode
                              )
{
  assert(persistenceList != NULL);
  assert(persistenceNode != NULL);

  // find position in persistence list
  PersistenceNode *nextPersistenceNode = LIST_FIND_FIRST(persistenceList,
                                                         nextPersistenceNode,
                                                            (persistenceNode->maxAge != AGE_FOREVER)
                                                         && (   (nextPersistenceNode->maxAge == AGE_FOREVER)
                                                             || (nextPersistenceNode->maxAge > persistenceNode->maxAge)
                                                            )
                                                        );

  // insert into persistence list
  List_insert(persistenceList,persistenceNode,nextPersistenceNode);

  assert(debugIsPersistenceListSorted(persistenceList));
}

void Job_freePersistenceNode(PersistenceNode *persistenceNode, void *userData)
{
  assert(persistenceNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(persistenceNode->moveTo);
}

void Job_deletePersistenceNode(PersistenceNode *persistenceNode)
{
  assert(persistenceNode != NULL);

  Job_freePersistenceNode(persistenceNode,NULL);
  LIST_DELETE_NODE(persistenceNode);
}

/***********************************************************************\
* Name   : initOptionsFileServer
* Purpose: init file server
* Input  : fileServer - file server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsFileServer(FileServer *fileServer)
{
  assert(fileServer != NULL);

  UNUSED_VARIABLE(fileServer);
}

/***********************************************************************\
* Name   : duplicateOptionsFileServer
* Purpose: duplicate file server
* Input  : fileServer     - file server
*          fromFileServer - from file server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsFileServer(FileServer *fileServer, const FileServer *fromFileServer)
{
  assert(fileServer != NULL);
  assert(fromFileServer != NULL);

  UNUSED_VARIABLE(fileServer);
  UNUSED_VARIABLE(fromFileServer);
}

/***********************************************************************\
* Name   : clearOptionsFileServer
* Purpose: clear file server
* Input  : fileServer - file server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsFileServer(FileServer *fileServer)
{
  assert(fileServer != NULL);

  UNUSED_VARIABLE(fileServer);
}

/***********************************************************************\
* Name   : doneOptionsFileServer
* Purpose: done file server
* Input  : fileServer - file server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsFileServer(FileServer *fileServer)
{
  assert(fileServer != NULL);

  UNUSED_VARIABLE(fileServer);
}

/***********************************************************************\
* Name   : initOptionsFTPServer
* Purpose: init FTP server
* Input  : ftpServer - FTP server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsFTPServer(FTPServer *ftpServer)
{
  assert(ftpServer != NULL);
//  assert(globalOptions.defaultFTPServer != NULL);

  ftpServer->userName = String_new();
  Password_initDuplicate(&ftpServer->password,&globalOptions.defaultFTPServer.ftp.password);
}

/***********************************************************************\
* Name   : duplicateOptionsFTPServer
* Purpose: duplicate FTP server
* Input  : ftpServer     - FTP server
*          fromFTPServer - from FTP server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsFTPServer(FTPServer *ftpServer, const FTPServer *fromFTPServer)
{
  assert(ftpServer != NULL);
  assert(fromFTPServer != NULL);

  ftpServer->userName = String_duplicate(fromFTPServer->userName);
  Password_initDuplicate(&ftpServer->password,&fromFTPServer->password);
}

/***********************************************************************\
* Name   : clearOptionsFTPServer
* Purpose: clear FTP server
* Input  : ftpServer - FTP server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsFTPServer(FTPServer *ftpServer)
{
  assert(ftpServer != NULL);

  String_clear(ftpServer->userName);
  Password_clear(&ftpServer->password);
}

/***********************************************************************\
* Name   : doneOptionsFTPServer
* Purpose: done FTP server
* Input  : ftpServer - FTP server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsFTPServer(FTPServer *ftpServer)
{
  assert(ftpServer != NULL);

  Password_done(&ftpServer->password);
  String_delete(ftpServer->userName);
}

/***********************************************************************\
* Name   : initOptionsSSHServer
* Purpose: init SSH server
* Input  : sshServer - SSH server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsSSHServer(SSHServer *sshServer)
{
  assert(sshServer != NULL);
//  assert(globalOptions.defaultSSHServer != NULL);

  sshServer->port     = 0;
  sshServer->userName = String_new();
  Password_initDuplicate(&sshServer->password,&globalOptions.defaultSSHServer.ssh.password);
  Configuration_duplicateKey(&sshServer->publicKey,&globalOptions.defaultSSHServer.ssh.publicKey);
  Configuration_duplicateKey(&sshServer->privateKey,&globalOptions.defaultSSHServer.ssh.privateKey);
}

/***********************************************************************\
* Name   : duplicateOptionsSSHServer
* Purpose: duplicate SSH server
* Input  : sshServer     - SSH server
*          fromSSHServer - from SSH server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsSSHServer(SSHServer *sshServer, const SSHServer *fromSSHServer)
{
  assert(sshServer != NULL);
  assert(fromSSHServer != NULL);

  sshServer->port     = fromSSHServer->port;
  sshServer->userName = String_duplicate(fromSSHServer->userName);
  Password_initDuplicate(&sshServer->password,&fromSSHServer->password);
  Configuration_duplicateKey(&sshServer->publicKey,&fromSSHServer->publicKey);
  Configuration_duplicateKey(&sshServer->privateKey,&fromSSHServer->privateKey);
}

/***********************************************************************\
* Name   : clearOptionsSSHServer
* Purpose: clear SSH server
* Input  : sshServer - SSH server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsSSHServer(SSHServer *sshServer)
{
  assert(sshServer != NULL);

  sshServer->port = 0;
  String_clear(sshServer->userName);
  Password_clear(&sshServer->password);
  Configuration_clearKey(&sshServer->publicKey);
  Configuration_clearKey(&sshServer->privateKey);
}

/***********************************************************************\
* Name   : doneOptionsSSHServer
* Purpose: done SSH server
* Input  : sshServer - SSH server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsSSHServer(SSHServer *sshServer)
{
  assert(sshServer != NULL);

  Configuration_doneKey(&sshServer->privateKey);
  Configuration_doneKey(&sshServer->publicKey);
  Password_done(&sshServer->password);
  String_delete(sshServer->userName);
}

/***********************************************************************\
* Name   : initOptionsWebDAVServer
* Purpose: init webDAV server
* Input  : webDAVServer - webDAV server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsWebDAVServer(WebDAVServer *webDAVServer)
{
  assert(webDAVServer != NULL);

  webDAVServer->userName = String_new();
  Password_initDuplicate(&webDAVServer->password,&globalOptions.defaultWebDAVServer.webDAV.password);
  Configuration_duplicateKey(&webDAVServer->publicKey,&globalOptions.defaultWebDAVServer.webDAV.publicKey);
  Configuration_duplicateKey(&webDAVServer->privateKey,&globalOptions.defaultWebDAVServer.webDAV.privateKey);
}

/***********************************************************************\
* Name   : duplicateOptionsWebDAVServer
* Purpose: duplicate webDAV server
* Input  : webDAVServer     - webDAV server
*          fromWebDAVServer - from webDAV server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsWebDAVServer(WebDAVServer *webDAVServer, const WebDAVServer *fromWebDAVServer)
{
  assert(webDAVServer != NULL);
  assert(fromWebDAVServer != NULL);

  webDAVServer->userName = String_duplicate(fromWebDAVServer->userName);
  Password_initDuplicate(&webDAVServer->password,&fromWebDAVServer->password);
  Configuration_duplicateKey(&webDAVServer->publicKey,&fromWebDAVServer->publicKey);
  Configuration_duplicateKey(&webDAVServer->privateKey,&fromWebDAVServer->privateKey);
}

/***********************************************************************\
* Name   : clearOptionsWebDAVServer
* Purpose: clear webDAV server
* Input  : webDAVServer - webDAV server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsWebDAVServer(WebDAVServer *webDAVServer)
{
  assert(webDAVServer != NULL);

  String_clear(webDAVServer->userName);
  Password_clear(&webDAVServer->password);
  Configuration_clearKey(&webDAVServer->publicKey);
  Configuration_clearKey(&webDAVServer->privateKey);
}

/***********************************************************************\
* Name   : doneOptionsWebDAVServer
* Purpose: done webDAV server
* Input  : webDAVServer - webDAV server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsWebDAVServer(WebDAVServer *webDAVServer)
{
  assert(webDAVServer != NULL);

  Configuration_doneKey(&webDAVServer->privateKey);
  Configuration_doneKey(&webDAVServer->publicKey);
  Password_done(&webDAVServer->password);
  String_delete(webDAVServer->userName);
}

/***********************************************************************\
* Name   : initOptionsSMBServer
* Purpose: init SMB server
* Input  : smbServer - SMB server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsSMBServer(SMBServer *smbServer)
{
  assert(smbServer != NULL);

  smbServer->userName = String_new();
  Password_initDuplicate(&smbServer->password,&globalOptions.defaultSMBServer.smb.password);
}

/***********************************************************************\
* Name   : duplicateOptionsSMBServer
* Purpose: duplicate SMB server
* Input  : smbServer     - SMB server
*          fromSMBServer - from SMB server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsSMBServer(SMBServer *smbServer, const SMBServer *fromSMBServer)
{
  assert(smbServer != NULL);
  assert(fromSMBServer != NULL);

  smbServer->userName = String_duplicate(fromSMBServer->userName);
  Password_initDuplicate(&smbServer->password,&fromSMBServer->password);
}

/***********************************************************************\
* Name   : clearOptionsSMBServer
* Purpose: clear SMB server
* Input  : smbServer - SMB server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsSMBServer(SMBServer *smbServer)
{
  assert(smbServer != NULL);

  String_clear(smbServer->userName);
  Password_clear(&smbServer->password);
}

/***********************************************************************\
* Name   : doneOptionsSMBServer
* Purpose: done SMB server
* Input  : smbServer - SMB server
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsSMBServer(SMBServer *smbServer)
{
  assert(smbServer != NULL);

  Password_done(&smbServer->password);
  String_delete(smbServer->userName);
}

/***********************************************************************\
* Name   : initOptionsOpticalDisk
* Purpose: init options optical disk
* Input  : opticalDisk - optical disk options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsOpticalDisk(OpticalDisk *opticalDisk)
{
  assert(opticalDisk != NULL);

  opticalDisk->deviceName              = String_new();
  opticalDisk->requestVolumeCommand    = String_new();
  opticalDisk->unloadVolumeCommand     = String_new();
  opticalDisk->loadVolumeCommand       = String_new();
  opticalDisk->volumeSize              = 0LL;
  opticalDisk->imagePreProcessCommand  = String_new();
  opticalDisk->imagePostProcessCommand = String_new();
  opticalDisk->imageCommand            = String_new();
  opticalDisk->eccPreProcessCommand    = String_new();
  opticalDisk->eccPostProcessCommand   = String_new();
  opticalDisk->eccCommand              = String_new();
  opticalDisk->blankCommand            = String_new();
  opticalDisk->writePreProcessCommand  = String_new();
  opticalDisk->writePostProcessCommand = String_new();
  opticalDisk->writeCommand            = String_new();
  opticalDisk->writeImageCommand       = String_new();
}

/***********************************************************************\
* Name   : duplicateOptionsOpticalDisk
* Purpose: duplicate options optical disk
* Input  : opticalDisk     - optical disk options
*          fromOpticalDisk - from optical disk options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsOpticalDisk(OpticalDisk *opticalDisk, const OpticalDisk *fromOpticalDisk)
{
  assert(opticalDisk != NULL);
  assert(fromOpticalDisk != NULL);

  opticalDisk->deviceName              = String_duplicate(fromOpticalDisk->deviceName             );
  opticalDisk->requestVolumeCommand    = String_duplicate(fromOpticalDisk->requestVolumeCommand   );
  opticalDisk->unloadVolumeCommand     = String_duplicate(fromOpticalDisk->unloadVolumeCommand    );
  opticalDisk->loadVolumeCommand       = String_duplicate(fromOpticalDisk->loadVolumeCommand      );
  opticalDisk->volumeSize              = fromOpticalDisk->volumeSize;
  opticalDisk->imagePreProcessCommand  = String_duplicate(fromOpticalDisk->imagePreProcessCommand );
  opticalDisk->imagePostProcessCommand = String_duplicate(fromOpticalDisk->imagePostProcessCommand);
  opticalDisk->imageCommand            = String_duplicate(fromOpticalDisk->imageCommand           );
  opticalDisk->eccPreProcessCommand    = String_duplicate(fromOpticalDisk->eccPreProcessCommand   );
  opticalDisk->eccPostProcessCommand   = String_duplicate(fromOpticalDisk->eccPostProcessCommand  );
  opticalDisk->eccCommand              = String_duplicate(fromOpticalDisk->eccCommand             );
  opticalDisk->blankCommand            = String_duplicate(fromOpticalDisk->blankCommand           );
  opticalDisk->writePreProcessCommand  = String_duplicate(fromOpticalDisk->writePreProcessCommand );
  opticalDisk->writePostProcessCommand = String_duplicate(fromOpticalDisk->writePostProcessCommand);
  opticalDisk->writeCommand            = String_duplicate(fromOpticalDisk->writeCommand           );
  opticalDisk->writeImageCommand       = String_duplicate(fromOpticalDisk->writeImageCommand      );
}

/***********************************************************************\
* Name   : clearOptionsOpticalDisk
* Purpose: clear options optical disk
* Input  : opticalDisk - optical disc options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsOpticalDisk(OpticalDisk *opticalDisk)
{
  assert(opticalDisk != NULL);

  String_clear(opticalDisk->deviceName             );
  String_clear(opticalDisk->requestVolumeCommand   );
  String_clear(opticalDisk->unloadVolumeCommand    );
  String_clear(opticalDisk->loadVolumeCommand      );
  opticalDisk->volumeSize = 0LL;
  String_clear(opticalDisk->imagePreProcessCommand );
  String_clear(opticalDisk->imagePostProcessCommand);
  String_clear(opticalDisk->imageCommand           );
  String_clear(opticalDisk->eccPreProcessCommand   );
  String_clear(opticalDisk->eccPostProcessCommand  );
  String_clear(opticalDisk->eccCommand             );
  String_clear(opticalDisk->blankCommand           );
  String_clear(opticalDisk->writePreProcessCommand );
  String_clear(opticalDisk->writePostProcessCommand);
  String_clear(opticalDisk->writeCommand           );
  String_clear(opticalDisk->writeImageCommand      );
}

/***********************************************************************\
* Name   : doneOptionsOpticalDisk
* Purpose: done options optical disk
* Input  : opticalDisk - optical disc options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsOpticalDisk(OpticalDisk *opticalDisk)
{
  assert(opticalDisk != NULL);

  String_delete(opticalDisk->writeImageCommand      );
  String_delete(opticalDisk->writeCommand           );
  String_delete(opticalDisk->writePostProcessCommand);
  String_delete(opticalDisk->writePreProcessCommand );
  String_delete(opticalDisk->blankCommand           );
  String_delete(opticalDisk->eccCommand             );
  String_delete(opticalDisk->eccPostProcessCommand  );
  String_delete(opticalDisk->eccPreProcessCommand   );
  String_delete(opticalDisk->imageCommand           );
  String_delete(opticalDisk->imagePostProcessCommand);
  String_delete(opticalDisk->imagePreProcessCommand );
  String_delete(opticalDisk->loadVolumeCommand      );
  String_delete(opticalDisk->unloadVolumeCommand    );
  String_delete(opticalDisk->requestVolumeCommand   );
  String_delete(opticalDisk->deviceName             );
}

/***********************************************************************\
* Name   : initOptionsDevice
* Purpose: init options device
* Input  : device - device options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initOptionsDevice(Device *device)
{
  assert(device != NULL);

  device->name                    = String_new();
  device->requestVolumeCommand    = String_new();
  device->unloadVolumeCommand     = String_new();
  device->loadVolumeCommand       = String_new();
  device->volumeSize              = 0LL;
  device->imagePreProcessCommand  = String_new();
  device->imagePostProcessCommand = String_new();
  device->imageCommand            = String_new();
  device->eccPreProcessCommand    = String_new();
  device->eccPostProcessCommand   = String_new();
  device->eccCommand              = String_new();
  device->blankCommand            = String_new();
  device->writePreProcessCommand  = String_new();
  device->writePostProcessCommand = String_new();
  device->writeCommand            = String_new();
}

/***********************************************************************\
* Name   : duplicateOptionsDevice
* Purpose: duplicate options device
* Input  : device     - device options
*          fromDevice - from device options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void duplicateOptionsDevice(Device *device, const Device *fromDevice)
{
  assert(device != NULL);
  assert(fromDevice != NULL);

  device->name                    = String_duplicate(fromDevice->name                   );
  device->requestVolumeCommand    = String_duplicate(fromDevice->requestVolumeCommand   );
  device->unloadVolumeCommand     = String_duplicate(fromDevice->unloadVolumeCommand    );
  device->loadVolumeCommand       = String_duplicate(fromDevice->loadVolumeCommand      );
  device->volumeSize              = fromDevice->volumeSize;
  device->imagePreProcessCommand  = String_duplicate(fromDevice->imagePreProcessCommand );
  device->imagePostProcessCommand = String_duplicate(fromDevice->imagePostProcessCommand);
  device->imageCommand            = String_duplicate(fromDevice->imageCommand           );
  device->eccPreProcessCommand    = String_duplicate(fromDevice->eccPreProcessCommand   );
  device->eccPostProcessCommand   = String_duplicate(fromDevice->eccPostProcessCommand  );
  device->eccCommand              = String_duplicate(fromDevice->eccCommand             );
  device->blankCommand            = String_duplicate(fromDevice->blankCommand           );
  device->writePreProcessCommand  = String_duplicate(fromDevice->writePreProcessCommand );
  device->writePostProcessCommand = String_duplicate(fromDevice->writePostProcessCommand);
  device->writeCommand            = String_duplicate(fromDevice->writeCommand           );
}

/***********************************************************************\
* Name   : clearOptionsDevice
* Purpose: clear device
* Input  : device - device options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptionsDevice(Device *device)
{
  assert(device != NULL);

  String_clear(device->name                   );
  String_clear(device->requestVolumeCommand   );
  String_clear(device->unloadVolumeCommand    );
  String_clear(device->loadVolumeCommand      );
  device->volumeSize = 0LL;
  String_clear(device->imagePreProcessCommand );
  String_clear(device->imagePostProcessCommand);
  String_clear(device->imageCommand           );
  String_clear(device->eccPreProcessCommand   );
  String_clear(device->eccPostProcessCommand  );
  String_clear(device->eccCommand             );
  String_clear(device->blankCommand           );
  String_clear(device->writePreProcessCommand );
  String_clear(device->writePostProcessCommand);
  String_clear(device->writeCommand           );
}

/***********************************************************************\
* Name   : doneOptionsDevice
* Purpose: done device
* Input  : device - device options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneOptionsDevice(Device *device)
{
  assert(device != NULL);

  String_delete(device->writeCommand           );
  String_delete(device->writePostProcessCommand);
  String_delete(device->writePreProcessCommand );
  String_delete(device->blankCommand           );
  String_delete(device->eccCommand             );
  String_delete(device->eccPostProcessCommand  );
  String_delete(device->eccPreProcessCommand   );
  String_delete(device->imageCommand           );
  String_delete(device->imagePostProcessCommand);
  String_delete(device->imagePreProcessCommand );
  String_delete(device->loadVolumeCommand      );
  String_delete(device->unloadVolumeCommand    );
  String_delete(device->requestVolumeCommand   );
  String_delete(device->name                   );
}

/***********************************************************************\
* Name   : clearOptions
* Purpose: clear options
* Input  : jobOptions - job options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void clearOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  String_clear(jobOptions->uuid);

  String_clear(jobOptions->includeFileListFileName);
  String_clear(jobOptions->includeFileCommand);
  String_clear(jobOptions->includeImageListFileName);
  String_clear(jobOptions->includeImageCommand);
  String_clear(jobOptions->excludeListFileName);
  String_clear(jobOptions->excludeCommand);

  List_clear(&jobOptions->mountList);
  PatternList_clear(&jobOptions->compressExcludePatternList);
  DeltaSourceList_clear(&jobOptions->deltaSourceList);
  List_clear(&jobOptions->scheduleList);
  List_clear(&jobOptions->persistenceList);
  jobOptions->persistenceList.lastModificationDateTime = 0LL;

  jobOptions->archiveType                = ARCHIVE_TYPE_NORMAL;

  jobOptions->archivePartSize            = globalOptions.archivePartSize;
  String_clear(jobOptions->incrementalListFileName);
  jobOptions->directoryStripCount        = DIRECTORY_STRIP_NONE;
  String_clear(jobOptions->destination);
  jobOptions->owner                      = globalOptions.owner;
  jobOptions->permissions                = globalOptions.permissions;
  jobOptions->patternType                = globalOptions.patternType;

  jobOptions->compressAlgorithms.delta   = COMPRESS_ALGORITHM_NONE;
  jobOptions->compressAlgorithms.byte    = COMPRESS_ALGORITHM_NONE;

  #ifdef HAVE_GCRYPT
    jobOptions->cryptType                = CRYPT_TYPE_SYMMETRIC;
  #else /* not HAVE_GCRYPT */
    jobOptions->cryptType                = CRYPT_TYPE_NONE;
  #endif /* HAVE_GCRYPT */
  for (size_t i = 0; i < 4; i++)
  {
    jobOptions->cryptAlgorithms[i] = CRYPT_ALGORITHM_NONE;
  }
  jobOptions->cryptPasswordMode                 = PASSWORD_MODE_DEFAULT;
  Password_clear(&jobOptions->cryptPassword);
  Configuration_clearKey(&jobOptions->cryptPublicKey);
  Configuration_clearKey(&jobOptions->cryptPrivateKey);

  String_clear(jobOptions->preProcessScript );
  String_clear(jobOptions->postProcessScript);
  String_clear(jobOptions->slavePreProcessScript );
  String_clear(jobOptions->slavePostProcessScript);

#ifdef HAVE_PAR2
  String_clear(jobOptions->par2Directory );
  jobOptions->par2BlockSize  = DEFAULT_PAR2_BLOCK_SIZE;
  jobOptions->par2FileCount  = DEFAULT_PAR2_FILE_COUNT;
  jobOptions->par2BlockCount = DEFAULT_PAR2_BLOCK_COUNT;
#endif // HAVE_PAR2

  jobOptions->storageOnMasterFlag            = TRUE;
  clearOptionsFileServer(&jobOptions->fileServer);
  clearOptionsFTPServer(&jobOptions->ftpServer);
  clearOptionsSSHServer(&jobOptions->sshServer);
  clearOptionsWebDAVServer(&jobOptions->webDAVServer);
  clearOptionsSMBServer(&jobOptions->smbServer);
  clearOptionsOpticalDisk(&jobOptions->opticalDisk);
  clearOptionsDevice(&jobOptions->device);

  String_clear(jobOptions->comment);

  jobOptions->archiveFileMode            = ARCHIVE_FILE_MODE_STOP;
  jobOptions->restoreEntryMode           = RESTORE_ENTRY_MODE_STOP;
  jobOptions->sparseFilesFlag                 = FALSE;

  jobOptions->errorCorrectionCodesFlag   = FALSE;
  jobOptions->alwaysCreateImageFlag      = FALSE;
  jobOptions->blankFlag                  = FALSE;
  jobOptions->waitFirstVolumeFlag        = FALSE;
  jobOptions->rawImagesFlag              = FALSE;

  jobOptions->testCreatedArchivesFlag    = FALSE;
  jobOptions->skipUnreadableFlag         = FALSE;
  jobOptions->forceDeltaCompressionFlag  = FALSE;
  jobOptions->ignoreNoDumpAttributeFlag  = FALSE;
  jobOptions->noFragmentsCheckFlag       = FALSE;
//TODO: job option or better global option only?
  jobOptions->noIndexDatabaseFlag        = FALSE;
  jobOptions->forceVerifySignaturesFlag  = FALSE;
  jobOptions->skipVerifySignaturesFlag   = FALSE;
  jobOptions->noStorage                  = FALSE;
  jobOptions->noSignatureFlag            = FALSE;
//TODO: job option or better global option only?
  jobOptions->noBAROnMediumFlag          = FALSE;
  jobOptions->noStopOnErrorFlag          = FALSE;
  jobOptions->noStopOnOwnerErrorFlag     = FALSE;
  jobOptions->noStopOnAttributeErrorFlag = FALSE;
  jobOptions->dryRun                     = FALSE;
}

/***********************************************************************\
* Name   : freeJobNode
* Purpose: free job node
* Input  : jobNode  - job node
*          userData - user data (no used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeJobNode(JobNode *jobNode, void *userData)
{
  assert(jobNode != NULL);
  assert(jobNode->job.uuid != NULL);
  assert(jobNode->name != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(jobNode->abortedByInfo);

  String_delete(jobNode->customText);
  String_delete(jobNode->scheduleUUID);

  doneRunningInfo(&jobNode->runningInfo);

  String_delete(jobNode->byName);

  Job_done(&jobNode->job);
  String_delete(jobNode->name);
  String_delete(jobNode->fileName);
}

/***********************************************************************\
* Name   : freeSlaveNode
* Purpose: free slave node
* Input  : slaveNode - slave node
*          userData  - user data (no used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeSlaveNode(SlaveNode *slaveNode, void *userData)
{
  assert(slaveNode != NULL);

  UNUSED_VARIABLE(userData);

  Connector_done(&slaveNode->connectorInfo);
  String_delete(slaveNode->name);
}

/*---------------------------------------------------------------------*/

Errors Job_initAll(void)
{
  Semaphore_init(&slaveList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&slaveList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeSlaveNode,NULL));
  Semaphore_init(&jobList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&jobList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeJobNode,NULL));

  return ERROR_NONE;
}

void Job_doneAll(void)
{
  List_done(&jobList);
  Semaphore_done(&jobList.lock);
  List_done(&slaveList);
  Semaphore_done(&slaveList.lock);
}

void Job_init(Job *job)
{
  assert(job != NULL);

  job->uuid                    = String_new();
  job->slaveHost.name          = String_new();
  job->slaveHost.port          = 0;
  job->slaveHost.tlsMode       = TLS_MODE_NONE;
  job->storageName             = String_new();
  job->storageNameListStdin    = FALSE;
  job->storageNameListFileName = String_new();
  job->storageNameCommand      = String_new();
  EntryList_init(&job->includeEntryList);
  PatternList_init(&job->excludePatternList);
  Job_initOptions(&job->options);

  DEBUG_ADD_RESOURCE_TRACE(job,Job);
}

void Job_initDuplicate(Job *job, const Job *fromJob)
{
  assert(job != NULL);
  assert(fromJob != NULL);

  DEBUG_CHECK_RESOURCE_TRACE(fromJob);

  job->uuid                    = String_duplicate(fromJob->uuid);
  job->slaveHost.name          = String_duplicate(fromJob->slaveHost.name);
  job->slaveHost.port          = fromJob->slaveHost.port;
  job->slaveHost.tlsMode       = fromJob->slaveHost.tlsMode;

  job->storageName             = String_duplicate(fromJob->storageName);
  job->storageNameListStdin    = fromJob->storageNameListStdin;
  job->storageNameListFileName = String_duplicate(fromJob->storageNameListFileName);
  job->storageNameCommand      = String_duplicate(fromJob->storageNameCommand);

  EntryList_initDuplicate(&job->includeEntryList,
                          &fromJob->includeEntryList,
                          NULL,  // fromEntryListFromNode
                          NULL  // fromEntryListToNode
                         );
  PatternList_initDuplicate(&job->excludePatternList,
                            &fromJob->excludePatternList,
                            NULL,  // fromPatternListFromNode
                            NULL  // fromPatternListToNode
                           );
  Job_copyOptions(&job->options,&fromJob->options);

  DEBUG_ADD_RESOURCE_TRACE(job,Job);
}

void Job_done(Job *job)
{
  assert(job != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(job,Job);

  Job_doneOptions(&job->options);

  PatternList_done(&job->excludePatternList);
  EntryList_done(&job->includeEntryList);

  String_delete(job->storageNameCommand);
  String_delete(job->storageNameListFileName);
  String_delete(job->storageName);

  String_delete(job->slaveHost.name);
  String_delete(job->uuid);
}

JobNode *Job_new(JobTypes    jobType,
                 ConstString name,
                 ConstString jobUUID,
                 ConstString fileName
                )
{
  assert(name != NULL);

  // allocate job node
  JobNode *jobNode = LIST_NEW_NODE(JobNode);
  if (jobNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init job node
  Job_init(&jobNode->job);
  if (!String_isEmpty(jobUUID))
  {
    String_set(jobNode->job.uuid,jobUUID);
  }
  else
  {
    Misc_getUUID(jobNode->job.uuid);
  }
  jobNode->name                             = String_duplicate(name);
  jobNode->jobType                          = jobType;

  jobNode->modifiedFlag                     = FALSE;
  jobNode->includeExcludeModifiedFlag       = FALSE;
  jobNode->mountModifiedFlag                = FALSE;
  jobNode->scheduleModifiedFlag             = FALSE;
  jobNode->persistenceModifiedFlag          = FALSE;

  jobNode->lastScheduleCheckDateTime        = 0LL;

  jobNode->fileName                         = String_duplicate(fileName);
  jobNode->fileModified                     = 0LL;

  jobNode->masterIO                         = NULL;

  jobNode->jobState                         = JOB_STATE_NONE;
  jobNode->slaveState                       = SLAVE_STATE_OFFLINE;
  jobNode->slaveTLS                         = FALSE;
  jobNode->slaveInsecureTLS                 = FALSE;

  initRunningInfo(&jobNode->runningInfo);
  jobNode->volumeRequest                    = VOLUME_REQUEST_NONE;
  jobNode->volumeRequestNumber              = 0;

  jobNode->archiveType                      = ARCHIVE_TYPE_NORMAL;
  jobNode->scheduleUUID                     = String_new();
  jobNode->customText                       = String_new();
  jobNode->testCreatedArchives              = FALSE;
  jobNode->noStorage                        = FALSE;
  jobNode->dryRun                           = FALSE;
  jobNode->byName                           = String_new();

  jobNode->requestedAbortFlag               = FALSE;
  jobNode->abortedByInfo                    = String_new();
  jobNode->volumeNumber                     = 0;
  jobNode->volumeUnloadFlag                 = FALSE;

  jobNode->executionCount.normal            = 0L;
  jobNode->executionCount.full              = 0L;
  jobNode->executionCount.incremental       = 0L;
  jobNode->executionCount.differential      = 0L;
  jobNode->executionCount.continuous        = 0L;
  jobNode->averageDuration.normal           = 0LL;
  jobNode->averageDuration.full             = 0LL;
  jobNode->averageDuration.incremental      = 0LL;
  jobNode->averageDuration.differential     = 0LL;
  jobNode->averageDuration.continuous       = 0LL;
  jobNode->totalEntityCount                 = 0L;
  jobNode->totalStorageCount                = 0L;
  jobNode->totalStorageSize                 = 0L;
  jobNode->totalEntryCount                  = 0L;
  jobNode->totalEntrySize                   = 0L;

  return jobNode;
}

JobNode *Job_copy(const JobNode *jobNode,
                  ConstString   fileName
                 )
{
  // allocate job node
  JobNode *newJobNode = LIST_NEW_NODE(JobNode);
  if (newJobNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init job node
  Job_initDuplicate(&newJobNode->job,&jobNode->job);
  newJobNode->name                             = File_getBaseName(String_new(),fileName,TRUE);
  newJobNode->jobType                          = jobNode->jobType;

  newJobNode->modifiedFlag                     = TRUE;

  newJobNode->lastScheduleCheckDateTime        = 0LL;

  newJobNode->fileName                         = String_duplicate(fileName);
  newJobNode->fileModified                     = 0LL;

  newJobNode->masterIO                         = NULL;

  newJobNode->jobState                         = JOB_STATE_NONE;
  newJobNode->slaveState                       = SLAVE_STATE_OFFLINE;
  newJobNode->slaveTLS                         = FALSE;
  newJobNode->slaveInsecureTLS                 = FALSE;

  initRunningInfo(&newJobNode->runningInfo);
  newJobNode->volumeRequest                    = VOLUME_REQUEST_NONE;
  newJobNode->volumeRequestNumber              = 0;

  newJobNode->archiveType                      = ARCHIVE_TYPE_NORMAL;
  newJobNode->scheduleUUID                     = String_new();
  newJobNode->customText                       = String_new();
  newJobNode->testCreatedArchives              = FALSE;
  newJobNode->noStorage                        = FALSE;
  newJobNode->dryRun                           = FALSE;
  newJobNode->byName                           = String_new();

  newJobNode->requestedAbortFlag               = FALSE;
  newJobNode->abortedByInfo                    = String_new();
  newJobNode->volumeNumber                     = 0;
  newJobNode->volumeUnloadFlag                 = FALSE;

  resetRunningInfo(&newJobNode->runningInfo);

  newJobNode->executionCount.normal            = 0L;
  newJobNode->executionCount.full              = 0L;
  newJobNode->executionCount.incremental       = 0L;
  newJobNode->executionCount.differential      = 0L;
  newJobNode->executionCount.continuous        = 0L;
  newJobNode->averageDuration.normal           = 0LL;
  newJobNode->averageDuration.full             = 0LL;
  newJobNode->averageDuration.incremental      = 0LL;
  newJobNode->averageDuration.differential     = 0LL;
  newJobNode->averageDuration.continuous       = 0LL;
  newJobNode->totalEntityCount                 = 0L;
  newJobNode->totalStorageCount                = 0L;
  newJobNode->totalStorageSize                 = 0L;
  newJobNode->totalEntryCount                  = 0L;
  newJobNode->totalEntrySize                   = 0L;

  return newJobNode;
}

void Job_delete(JobNode *jobNode)
{
  assert(jobNode != NULL);

  freeJobNode(jobNode,NULL);
  LIST_DELETE_NODE(jobNode);
}

void Job_setListModified(void)
{
}

bool Job_isSomeRunning(void)
{
  bool runningFlag = FALSE;
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    const JobNode *jobNode;
    JOB_LIST_ITERATE(jobNode)
    {
      if (Job_isRunning(jobNode->jobState))
      {
        runningFlag = TRUE;
        break;
      }
    }
  }

  return runningFlag;
}

bool Job_parseState(const char *name, JobStates *jobState, bool *noStorage, bool *dryRun)
{
  assert(name != NULL);

  assert(name != NULL);
  assert(jobState != NULL);

  if (noStorage != NULL) (*noStorage) = FALSE;
  if (dryRun    != NULL) (*dryRun)    = FALSE;
  if      (stringEqualsIgnoreCase(name,"NONE"))
  {
    (*jobState) = JOB_STATE_NONE;
  }
  else if (stringEqualsIgnoreCase(name,"WAITING"))
  {
    (*jobState) = JOB_STATE_WAITING;
  }
  else if (stringEqualsIgnoreCase(name,"NO_STORAGE"))
  {
    (*jobState) = JOB_STATE_RUNNING;
    if (noStorage != NULL) (*noStorage) = TRUE;
  }
  else if (stringEqualsIgnoreCase(name,"DRY_RUNNING"))
  {
    (*jobState) = JOB_STATE_RUNNING;
    if (dryRun != NULL) (*dryRun) = TRUE;
  }
  else if (stringEqualsIgnoreCase(name,"RUNNING"))
  {
    (*jobState) = JOB_STATE_RUNNING;
  }
  else if (stringEqualsIgnoreCase(name,"DONE"))
  {
    (*jobState) = JOB_STATE_DONE;
  }
  else if (stringEqualsIgnoreCase(name,"ERROR"))
  {
    (*jobState) = JOB_STATE_ERROR;
  }
  else if (stringEqualsIgnoreCase(name,"ABORTED"))
  {
    (*jobState) = JOB_STATE_ABORTED;
  }
  else if (stringEqualsIgnoreCase(name,"DISCONNECTED"))
  {
    (*jobState) = JOB_STATE_DISCONNECTED;
  }
  else
  {
    return FALSE;
  }

  return TRUE;
}

const char *Job_getStateText(JobStates jobState, bool noStorage, bool dryRun)
{
  const char *stateText;

  stateText = "UNKNOWN";
  switch (jobState)
  {
    case JOB_STATE_NONE:
      stateText = "NONE";
      break;
    case JOB_STATE_WAITING:
      stateText = "WAITING";
      break;
    case JOB_STATE_RUNNING:
      if      (noStorage)
      {
        stateText = "NO_STORAGE";
      }
      else if (dryRun)
      {
        stateText = "DRY_RUNNING";
      }
      else
      {
        stateText = "RUNNING";
      }
      break;
    case JOB_STATE_DONE:
      stateText = "DONE";
      break;
    case JOB_STATE_ERROR:
      stateText = "ERROR";
      break;
    case JOB_STATE_ABORTED:
      stateText = "ABORTED";
      break;
    case JOB_STATE_DISCONNECTED:
      stateText = "DISCONNECTED";
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return stateText;
}

ScheduleNode *Job_findScheduleByUUID(const JobNode *jobNode, ConstString scheduleUUID)
{
  assert(scheduleUUID != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  ScheduleNode *scheduleNode;
  if (jobNode != NULL)
  {
    scheduleNode = LIST_FIND(&jobNode->job.options.scheduleList,scheduleNode,String_equals(scheduleNode->uuid,scheduleUUID));
  }
  else
  {
    scheduleNode = NULL;
    JOB_LIST_ITERATEX(jobNode,scheduleNode == NULL)
    {
      scheduleNode = LIST_FIND(&jobNode->job.options.scheduleList,scheduleNode,String_equals(scheduleNode->uuid,scheduleUUID));
    }
  }

  return scheduleNode;
}

void Job_setModified(JobNode *jobNode)
{
  ConnectorInfo *connectorInfo;

  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // force reconnect slave
  JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
  {
    if (connectorInfo != NULL)
    {
      if (Connector_isConnected(connectorInfo)) Connector_shutdown(connectorInfo);
    }
  }

  jobNode->modifiedFlag = TRUE;
}

void Job_setScheduleModified(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  Job_updateNotifies(jobNode);

  jobNode->scheduleModifiedFlag = TRUE;
}

void Job_flush(JobNode *jobNode)
{
  assert(Semaphore_isLocked(&jobList.lock));

  if (jobNode->scheduleModifiedFlag)
  {
    // update continuous notifies
    const ScheduleNode *scheduleNode;
    LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
    {
      if (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS)
      {
        if (scheduleNode->enabled)
        {
          Continuous_initNotify(jobNode->name,
                                String_cString(jobNode->job.uuid),
                                String_cString(scheduleNode->uuid),
                                scheduleNode->date,
                                scheduleNode->weekDaySet,
                                scheduleNode->beginTime,
                                scheduleNode->endTime,
                                &jobNode->job.includeEntryList
                               );
        }
        else
        {
          Continuous_doneNotify(jobNode->name,
                                String_cString(jobNode->job.uuid),
                                String_cString(scheduleNode->uuid)
                               );
        }
      }
    }
  }

  if (   (jobNode->modifiedFlag)
      || (jobNode->includeExcludeModifiedFlag)
      || (jobNode->mountModifiedFlag)
      || (jobNode->scheduleModifiedFlag)
      || (jobNode->persistenceModifiedFlag)
     )
  {
    Errors error = Job_write(jobNode);
    if (error != ERROR_NONE)
    {
      printWarning(_("cannot update job '%s' (error: %s)"),String_cString(jobNode->fileName),Error_getText(error));
    }
  }
}

void Job_flushAll()
{
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    JobNode *jobNode;
    JOB_LIST_ITERATE(jobNode)
    {
      Job_flush(jobNode);
    }
  }
}

Errors Job_readScheduleInfo(JobNode *jobNode)
{
  Errors error;

  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // init variables
  ScheduleNode *scheduleNode;
  LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
  {
    scheduleNode->lastExecutedDateTime = 0LL;
  }
  jobNode->runningInfo.lastExecutedDateTime = 0LL;

  // get filename
  String fileName = String_new();
  String baseName = String_new();
  File_splitFileName(jobNode->fileName,fileName,baseName,NULL);
  File_appendFileName(fileName,String_insertChar(baseName,0,'.'));
  String_delete(baseName);

  if (File_exists(fileName))
  {
    // open file .name
    FileHandle fileHandle;
    error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
    if (error != ERROR_NONE)
    {
      String_delete(fileName);
      return error;
    }

    // read file
    String line = String_new();
    if (File_getLine(&fileHandle,line,NULL,NULL))
    {
      /* first line: <last execution time stamp> <type> <state> <error code> <error data>
                     <last execution time stamp> <type>
                     <last execution time stamp>

      */
      uint64       n1;
      uint         n2;
      char         s1[64],s2[32],s3[256];
      ArchiveTypes archiveType;
      JobStates    jobState;
      if      (   String_parse(line,STRING_BEGIN,"%"PRIu64" %64s %32s %u % 256s",NULL,&n1,s1,s2,&n2,s3)
               && Archive_parseType(s1,&archiveType,NULL)
               && Job_parseState(s2,&jobState,NULL,NULL)
              )
      {
        ScheduleNode *scheduleNode;
        LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
        {
          scheduleNode->lastExecutedDateTime = n1;
        }
        if (n1 > jobNode->runningInfo.lastExecutedDateTime) jobNode->runningInfo.lastExecutedDateTime = n1;
        jobNode->jobState = jobState;
        jobNode->runningInfo.lastErrorCode   = n2;
        jobNode->runningInfo.lastErrorNumber = 0;
        String_setCString(jobNode->runningInfo.lastErrorData,s3);
      }
      else if (   String_parse(line,STRING_BEGIN,"%"PRIu64" %64s",NULL,&n1,s1)
               && Archive_parseType(s1,&archiveType,NULL)
              )
      {
        LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
        {
          if (scheduleNode->archiveType == archiveType)
          {
            scheduleNode->lastExecutedDateTime = n1;
          }
        }
        if (n1 > jobNode->runningInfo.lastExecutedDateTime) jobNode->runningInfo.lastExecutedDateTime = n1;
      }
      else if (String_parse(line,STRING_BEGIN,"%"PRIu64,NULL,&n1))
      {
        LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
        {
          scheduleNode->lastExecutedDateTime = n1;
        }
        if (n1 > jobNode->runningInfo.lastExecutedDateTime) jobNode->runningInfo.lastExecutedDateTime = n1;
      }

      // other lines: <last execution time stamp>+<type>
      while (File_getLine(&fileHandle,line,NULL,NULL))
      {
        if (String_parse(line,STRING_BEGIN,"%"PRIu64" %64s",NULL,&n1,s1))
        {
          if (Archive_parseType(s1,&archiveType,NULL))
          {
            LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
            {
              if (scheduleNode->archiveType == archiveType)
              {
                scheduleNode->lastExecutedDateTime = n1;
              }
            }
          }
          if (n1 > jobNode->runningInfo.lastExecutedDateTime) jobNode->runningInfo.lastExecutedDateTime = n1;
        }
      }
    }
    jobNode->lastScheduleCheckDateTime = jobNode->runningInfo.lastExecutedDateTime;

    String_delete(line);

    // close file
    File_close(&fileHandle);

    if (error != ERROR_NONE)
    {
      String_delete(fileName);
      return error;
    }
  }

  // set max. last schedule check date/time
  if (jobNode->lastScheduleCheckDateTime == 0LL)
  {
    jobNode->lastScheduleCheckDateTime = Misc_getCurrentDate()-MAX_SCHEDULE_CATCH_TIME*S_PER_DAY;
  }

  // convert deprecated schedule persistence -> persistence data
  LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
  {
    if (scheduleNode->deprecatedPersistenceFlag)
    {
      // map 0 -> all/forever
      int minKeep = scheduleNode->minKeep;
      int maxKeep = (scheduleNode->maxKeep != 0) ? scheduleNode->maxKeep : KEEP_ALL;
      int maxAge  = (scheduleNode->maxAge  != 0) ? scheduleNode->maxAge  : AGE_FOREVER;

      // find existing persistence node
      PersistenceNode *persistenceNode = LIST_FIND(&jobNode->job.options.persistenceList,
                                                   persistenceNode,
                                                      (persistenceNode->archiveType == scheduleNode->archiveType)
                                                   && (persistenceNode->minKeep     == minKeep                  )
                                                   && (persistenceNode->maxKeep     == maxKeep                  )
                                                   && (persistenceNode->maxAge      == maxAge                   )
                                                  );
      if (persistenceNode == NULL)
      {
        // create new persistence node
        persistenceNode = Job_newPersistenceNode(scheduleNode->archiveType,
                                                 minKeep,
                                                 maxKeep,
                                                 maxAge,
                                                 NULL
                                                );
        assert(persistenceNode != NULL);

        // insert into persistence list
        Job_insertPersistenceNode(&jobNode->job.options.persistenceList,persistenceNode);
      }
    }
  }

  // free resources
  String_delete(fileName);

  return ERROR_NONE;
}

Errors Job_writeScheduleInfo(JobNode *jobNode, ArchiveTypes archiveType, uint64 executeEndDateTime)
{
  Errors error;

  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // set last executed
  ScheduleNode *scheduleNode;
  LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
  {
    if (scheduleNode->archiveType == archiveType)
    {
      scheduleNode->lastExecutedDateTime = executeEndDateTime;
    }
  }

  if (!String_isEmpty(jobNode->fileName))
  {
    // get filename
    String fileName;
    fileName = String_new();
    String baseName = String_new();
    File_splitFileName(jobNode->fileName,fileName,baseName,NULL);
    File_appendFileName(fileName,String_insertChar(baseName,0,'.'));
    String_delete(baseName);

    // create file .name
    FileHandle fileHandle;
    error = File_open(&fileHandle,fileName,FILE_OPEN_CREATE);
    if (error != ERROR_NONE)
    {
      String_delete(fileName);
      return error;
    }

    // write last execution time stamp+state+error code+error data
    error = File_printLine(&fileHandle,
                           "%"PRIu64" %s %s %u %s",
                           executeEndDateTime,
                           Archive_archiveTypeToString(archiveType),
                           Job_getStateText(jobNode->jobState,jobNode->noStorage,jobNode->dryRun),
                           jobNode->runningInfo.lastErrorCode,
                           String_cString(jobNode->runningInfo.lastErrorData)
                          );
    if (error != ERROR_NONE)
    {
      File_close(&fileHandle);
      String_delete(fileName);
      return error;
    }

    // write for archive types: last execution time stamp+type+state+message
    for (archiveType = ARCHIVE_TYPE_MIN; archiveType <= ARCHIVE_TYPE_MAX; archiveType++)
    {
      // get last executed date/time
      uint64 lastExecutedDateTime = 0LL;
      LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
      {
        if ((scheduleNode->archiveType == archiveType) && (scheduleNode->lastExecutedDateTime > lastExecutedDateTime))
        {
          lastExecutedDateTime = scheduleNode->lastExecutedDateTime;
        }
      }

      // write <last execution time stamp>+<type>
      if (lastExecutedDateTime > 0LL)
      {
        error = File_printLine(&fileHandle,
                               "%"PRIu64" %s",
                               lastExecutedDateTime,
                               Archive_archiveTypeToString(archiveType)
                              );
        if (error != ERROR_NONE)
        {
          File_close(&fileHandle);
          String_delete(fileName);
          return error;
        }
      }
    }

    // close file
    File_close(&fileHandle);

    // free resources
    String_delete(fileName);
  }

  return ERROR_NONE;
}

bool Job_read(JobNode *jobNode)
{
  Errors error;

  assert(jobNode != NULL);
  assert(jobNode->fileName != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // reset job values
  String_clear(jobNode->job.uuid);
  String_clear(jobNode->job.slaveHost.name);
  jobNode->job.slaveHost.port    = 0;
  jobNode->job.slaveHost.tlsMode = TLS_MODE_NONE;
  String_clear(jobNode->job.storageName);
  EntryList_clear(&jobNode->job.includeEntryList);
  PatternList_clear(&jobNode->job.excludePatternList);
  clearOptions(&jobNode->job.options);

  // open file
  FileHandle fileHandle;
  error = File_open(&fileHandle,jobNode->fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printError(_("cannot open job file '%s' (error: %s)!"),
               String_cString(jobNode->fileName),
               Error_getText(error)
              );
    return FALSE;
  }

  // parse file
  bool   failed        = FALSE;
  bool   hasDeprecated = FALSE;
  String line          = String_new();
  size_t lineNb        = 0;
  StringList commentList;
  StringList_init(&commentList);
  String s             = String_new();
  String name          = String_new();
  String value         = String_new();
  while (File_getLine(&fileHandle,line,&lineNb,NULL) && !failed)
  {
    long nextIndex;

    // parse line
    if      (String_isEmpty(line) || String_startsWithCString(line,"# ---"))
    {
      // discard separator or empty line
      StringList_clear(&commentList);
    }
    else if (String_startsWithCString(line,"# "))
    {
      // store comment
      String_remove(line,STRING_BEGIN,2);
      StringList_append(&commentList,line);
    }
    else if (String_parse(line,STRING_BEGIN,"#%S=",&nextIndex,name))
    {
      uint i;

      // commented value -> only store comments
      if (!StringList_isEmpty(&commentList))
      {
        i = ConfigValue_find(BAR_CONFIG_VALUES,
                             CONFIG_VALUE_INDEX_NONE,
                             CONFIG_VALUE_INDEX_NONE,
                             String_cString(name)
                            );
        if (i != CONFIG_VALUE_INDEX_NONE)
        {
          ConfigValue_setComments(BAR_CONFIG_VALUES,&BAR_CONFIG_VALUES[i],&commentList);
          StringList_clear(&commentList);
        }
      }
    }
    else if (String_startsWithChar(line,'#'))
    {
      // ignore other commented lines
    }
    else if (String_parse(line,STRING_BEGIN,"[schedule %S]",NULL,s))
    {
      // find section
      uint firstValueIndex,lastValueIndex;
      uint i = ConfigValue_findSection(JOB_CONFIG_VALUES,
                                       "schedule",
                                       &firstValueIndex,
                                       &lastValueIndex
                                      );
      assertx(i != CONFIG_VALUE_INDEX_NONE,"unknown section 'schedule'");
      UNUSED_VARIABLE(i);

      // new schedule
      ScheduleNode *scheduleNode = Job_newScheduleNode(NULL);
      assert(scheduleNode != NULL);
      while (   File_getLine(&fileHandle,line,&lineNb,"#")
             && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
            )
      {
        if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
        {
          i = ConfigValue_find(JOB_CONFIG_VALUES,
                               firstValueIndex,
                               lastValueIndex,
                               String_cString(name)
                              );
          if (i != CONFIG_VALUE_INDEX_NONE)
          {
            ConfigValue_parse(JOB_CONFIG_VALUES,
                              &JOB_CONFIG_VALUES[i],
                              "schedule",
                              String_cString(value),
                              CALLBACK_INLINE(void,(const char *errorMessage, void *userData),
                              {
                                UNUSED_VARIABLE(userData);

                                printError(_("%s in %s, line %ld: '%s'"),errorMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                              },NULL),
                              CALLBACK_INLINE(void,(const char *warningMessage, void *userData),
                              {
                                UNUSED_VARIABLE(userData);

                                printWarning(_("%s in %s, line %ld: '%s'"),warningMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                              },NULL),
                              scheduleNode,
                              &commentList
                             );
          }
          else
          {
            printError(_("unknown value '%s' in %s, line %ld"),String_cString(name),String_cString(jobNode->fileName),lineNb);
            failed = TRUE;
          }
        }
        else
        {
          printError(_("syntax error in %s, line %ld: '%s' - skipped"),
                     String_cString(jobNode->fileName),
                     lineNb,
                     String_cString(line)
                    );
          failed = TRUE;
        }
      }
      File_ungetLine(&fileHandle,line,&lineNb);

      // init schedule uuid
      if (String_isEmpty(scheduleNode->uuid))
      {
        Misc_getUUID(scheduleNode->uuid);
      }

      // get schedule info (if possible)
      scheduleNode->lastExecutedDateTime = 0LL;
      scheduleNode->totalEntityCount     = 0L;
      scheduleNode->totalStorageCount    = 0L;
      scheduleNode->totalStorageSize     = 0LL;
      scheduleNode->totalEntryCount      = 0L;
      scheduleNode->totalEntrySize       = 0LL;

      // append to list (if not a duplicate)
      const ScheduleNode *existingScheduleNode;
      if (!LIST_CONTAINS(&jobNode->job.options.scheduleList,
                         existingScheduleNode,
                            (existingScheduleNode->date.year   == scheduleNode->date.year            )
                         && (existingScheduleNode->date.month  == scheduleNode->date.month           )
                         && (existingScheduleNode->date.day    == scheduleNode->date.day             )
                         && (existingScheduleNode->weekDaySet  == scheduleNode->weekDaySet           )
                         && (existingScheduleNode->time.hour   == scheduleNode->time.hour            )
                         && (existingScheduleNode->time.minute == scheduleNode->time.minute          )
                         && (existingScheduleNode->archiveType == scheduleNode->archiveType          )
                         && (existingScheduleNode->interval    == scheduleNode->interval             )
                         && (String_equals(existingScheduleNode->customText,scheduleNode->customText))
                         && (existingScheduleNode->minKeep     == scheduleNode->minKeep              )
                         && (existingScheduleNode->maxKeep     == scheduleNode->maxKeep              )
                         && (existingScheduleNode->maxAge      == scheduleNode->maxAge               )
                         && (existingScheduleNode->noStorage   == scheduleNode->noStorage            )
                        )
        )
      {
        // append to schedule list
        List_append(&jobNode->job.options.scheduleList,scheduleNode);
      }
      else
      {
        // duplicate -> discard
        Job_deleteScheduleNode(scheduleNode);
      }
    }
    else if (String_parse(line,STRING_BEGIN,"[persistence %S]",NULL,s))
    {
      // find section
      uint firstValueIndex,lastValueIndex;
      uint i = ConfigValue_findSection(JOB_CONFIG_VALUES,
                                       "persistence",
                                       &firstValueIndex,
                                       &lastValueIndex
                                      );
      assertx(i != CONFIG_VALUE_INDEX_NONE,"unknown section 'persistence'");
      UNUSED_VARIABLE(i);

      ArchiveTypes archiveType;
      if (Archive_parseType(String_cString(s),&archiveType,NULL))
      {
        // new persistence
        PersistenceNode *persistenceNode = Job_newPersistenceNode(archiveType,0,0,0,NULL);
        assert(persistenceNode != NULL);
        while (   File_getLine(&fileHandle,line,&lineNb,"#")
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
          {
            i = ConfigValue_find(JOB_CONFIG_VALUES,
                                 firstValueIndex,
                                 lastValueIndex,
                                 String_cString(name)
                                );
            if (i != CONFIG_VALUE_INDEX_NONE)
            {
              ConfigValue_parse(JOB_CONFIG_VALUES,
                                &JOB_CONFIG_VALUES[i],
                                "persistence",
                                String_cString(value),
                                CALLBACK_INLINE(void,(const char *errorMessage, void *userData),
                                {
                                  UNUSED_VARIABLE(userData);

                                  printError(_("%s in %s, line %ld: '%s'"),errorMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                                },NULL),
                                CALLBACK_INLINE(void,(const char *warningMessage, void *userData),
                                {
                                  UNUSED_VARIABLE(userData);

                                  printWarning(_("%s in %s, line %ld: '%s'"),warningMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                                },NULL),
                                persistenceNode,
                                &commentList
                               );
            }
            else
            {
              printError(_("unknown value '%s' in %s, line %ld"),String_cString(name),String_cString(jobNode->fileName),lineNb);
              failed = TRUE;
            }
          }
          else
          {
            printError(_("syntax error in %s, line %ld: '%s' - skipped"),
                       String_cString(jobNode->fileName),
                       lineNb,
                       String_cString(line)
                      );
            failed = TRUE;
          }
        }
        File_ungetLine(&fileHandle,line,&lineNb);

        // insert into persistence list (if not a duplicate)
        const PersistenceNode *existingPersistenceNode;
        if (!LIST_CONTAINS(&jobNode->job.options.persistenceList,
                           existingPersistenceNode,
                              (existingPersistenceNode->archiveType == persistenceNode->archiveType)
                           && (existingPersistenceNode->minKeep     == persistenceNode->minKeep    )
                           && (existingPersistenceNode->maxKeep     == persistenceNode->maxKeep    )
                           && (existingPersistenceNode->maxAge      == persistenceNode->maxAge     )
                          )
           )
        {
          // insert into persistence list
          Job_insertPersistenceNode(&jobNode->job.options.persistenceList,persistenceNode);
        }
        else
        {
          // duplicate -> discard
          Job_deletePersistenceNode(persistenceNode);
        }
      }
      else
      {
        printError(_("unknown archive type '%s' in section '%s' in %s, line %ld - skipped"),
                   String_cString(s),
                   "persistence",
                   String_cString(jobNode->fileName),
                   lineNb
                  );

        // skip rest of section
        while (   File_getLine(&fileHandle,line,&lineNb,"#")
               && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
              )
        {
          // ignored
        }
        File_ungetLine(&fileHandle,line,&lineNb);

        failed = TRUE;
      }
    }
    else if (String_parse(line,STRING_BEGIN,"[global]",NULL))
    {
      // nothing to do
    }
    else if (String_parse(line,STRING_BEGIN,"[end]",NULL))
    {
      // nothing to do
    }
    else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
    {
      uint i = ConfigValue_find(JOB_CONFIG_VALUES,
                                CONFIG_VALUE_INDEX_NONE,
                                CONFIG_VALUE_INDEX_NONE,
                                String_cString(name)
                               );
      if (i != CONFIG_VALUE_INDEX_NONE)
      {
        ConfigValue_parse(JOB_CONFIG_VALUES,
                          &JOB_CONFIG_VALUES[i],
                          NULL, // sectionName
                          String_cString(value),
                          CALLBACK_INLINE(void,(const char *errorMessage, void *userData),
                          {
                            UNUSED_VARIABLE(userData);

                            printError(_("%s in %s, line %ld: '%s'"),errorMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                          },NULL),
                          CALLBACK_INLINE(void,(const char *warningMessage, void *userData),
                          {
                            UNUSED_VARIABLE(userData);

                            printWarning(_("%s in %s, line %ld: '%s'"),warningMessage,String_cString(jobNode->fileName),lineNb,String_cString(line));
                          },NULL),
                          jobNode,
                          &commentList
                         );

        hasDeprecated = (JOB_CONFIG_VALUES[i].type == CONFIG_VALUE_TYPE_DEPRECATED);
      }
      else
      {
        printError(_("unknown value '%s' in %s, line %ld"),String_cString(name),String_cString(jobNode->fileName),lineNb);
        failed = TRUE;
      }
    }
    else
    {
      printError(_("syntax error in %s, line %ld: '%s' - skipped"),
                 String_cString(jobNode->fileName),
                 lineNb,
                 String_cString(line)
                );
      failed = TRUE;
    }
  }
  String_delete(value);
  String_delete(name);
  String_delete(s);
  StringList_done(&commentList);
  String_delete(line);

  // close file
  (void)File_close(&fileHandle);
  jobNode->fileModified = File_getFileTimeModified(jobNode->fileName);

  // check if failed
  if (failed)
  {
    return FALSE;
  }

  // init values from storage name
  StorageSpecifier storageSpecifier;
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,jobNode->job.storageName);
  if (error == ERROR_NONE)
  {
    switch (storageSpecifier.type)
    {
      case STORAGE_TYPE_NONE:
        break;
      case STORAGE_TYPE_FILESYSTEM:
        break;
      case STORAGE_TYPE_FTP:
        if (String_isEmpty(jobNode->job.options.ftpServer.userName)) String_set(jobNode->job.options.ftpServer.userName,storageSpecifier.userName);
        if (Password_isEmpty(&jobNode->job.options.ftpServer.password)) Password_set(&jobNode->job.options.ftpServer.password,&storageSpecifier.password);
        break;
      case STORAGE_TYPE_SCP:
      case STORAGE_TYPE_SFTP:
// TODO: add port in storage name
//        if (jobNode->job.options.sshServer.port == 0) jobNode->job.options.sshServer.port = storageSpecifier.port;
        if (String_isEmpty(jobNode->job.options.sshServer.userName)) String_set(jobNode->job.options.sshServer.userName,storageSpecifier.userName);
        if (Password_isEmpty(&jobNode->job.options.sshServer.password)) Password_set(&jobNode->job.options.sshServer.password,&storageSpecifier.password);
        break;
      case STORAGE_TYPE_WEBDAV:
      case STORAGE_TYPE_WEBDAVS:
// TODO: add port in storage name
//        if (jobNode->job.options.webDAVServer.port == 0) jobNode->job.options.webDAVServer.port = storageSpecifier.port;
        if (String_isEmpty(jobNode->job.options.webDAVServer.userName)) String_set(jobNode->job.options.webDAVServer.userName,storageSpecifier.userName);
        if (Password_isEmpty(&jobNode->job.options.webDAVServer.password)) Password_set(&jobNode->job.options.webDAVServer.password,&storageSpecifier.password);
        break;
      case STORAGE_TYPE_SMB:
        if (String_isEmpty(jobNode->job.options.smbServer.userName)) String_set(jobNode->job.options.smbServer.userName,storageSpecifier.userName);
        if (Password_isEmpty(&jobNode->job.options.smbServer.password)) Password_set(&jobNode->job.options.smbServer.password,&storageSpecifier.password);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        break;
      case STORAGE_TYPE_DEVICE:
        break;
      case STORAGE_TYPE_ANY:
        break;
      case STORAGE_TYPE_UNKNOWN:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break;
    }
  }
  Storage_doneSpecifier(&storageSpecifier);

  // read schedule info (ignore errors)
  (void)Job_readScheduleInfo(jobNode);

  // reset job modified
  jobNode->modifiedFlag               = hasDeprecated;
  jobNode->includeExcludeModifiedFlag = FALSE;
  jobNode->mountModifiedFlag          = FALSE;
  jobNode->scheduleModifiedFlag       = FALSE;
  jobNode->persistenceModifiedFlag    = FALSE;

  return TRUE;
}

Errors Job_rereadAll(ConstString jobsDirectory)
{
  Errors error;

  assert(jobsDirectory != NULL);

  // add new/update jobs
  String fileName = String_new();
  File_setFileName(fileName,jobsDirectory);
  DirectoryListHandle directoryListHandle;
  error = File_openDirectoryList(&directoryListHandle,fileName);
  if (error != ERROR_NONE)
  {
    String_delete(fileName);
    return error;
  }
  String baseName = String_new();
  while (!File_endOfDirectoryList(&directoryListHandle))
  {
    // read directory entry
    if (File_readDirectoryList(&directoryListHandle,fileName,NULL) != ERROR_NONE)
    {
      continue;
    }

    // get base name
    File_getBaseName(baseName,fileName,TRUE);

    // check if readable file and not ".*"
    if (File_isFile(fileName) && File_isReadable(fileName) && !String_startsWithChar(baseName,'.'))
    {
      JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
      {
        JobNode *jobNode;
        jobNode = Job_find(baseName);
        if (jobNode == NULL)
        {
          // create and read new job
          jobNode = Job_new(JOB_TYPE_CREATE,
                            baseName,
                            NULL, // jobUUID
                            fileName
                           );
          assert(jobNode != NULL);

          (void)Job_read(jobNode);
          List_append(&jobList,jobNode);

          // notify about changes
          Job_setIncludeExcludeModified(jobNode);
          Job_setMountModified(jobNode);
          Job_setScheduleModified(jobNode);
          Job_setPersistenceModified(jobNode);

          // notify about changes
          Job_setListModified();
        }
        else
        {
          // re-read existing job if not active and file is modified
          if (   !Job_isActive(jobNode->jobState)
              && (File_getFileTimeModified(fileName) > jobNode->fileModified)
             )
          {
            // read job
            (void)Job_read(jobNode);

            // notify about changes
            Job_setIncludeExcludeModified(jobNode);
            Job_setMountModified(jobNode);
            Job_setScheduleModified(jobNode);
            Job_setPersistenceModified(jobNode);
          }
        }

        // reset modified flag
        jobNode->includeExcludeModifiedFlag = FALSE;
        jobNode->mountModifiedFlag          = FALSE;
        jobNode->scheduleModifiedFlag       = FALSE;
        jobNode->persistenceModifiedFlag    = FALSE;
      }
    }
  }
  String_delete(baseName);
  File_closeDirectoryList(&directoryListHandle);

  // remove not existing jobs
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    JobNode *jobNode = jobList.head;
    while (jobNode != NULL)
    {
      if (jobNode->jobState == JOB_STATE_NONE)
      {
        File_setFileName(fileName,jobsDirectory);
        File_appendFileName(fileName,jobNode->name);
        if (File_isFile(fileName) && File_isReadable(fileName))
        {
          // exists => ok
          jobNode = jobNode->next;
        }
        else
        {
          // do not exists anymore => remove and delete job node
          jobNode = List_removeAndFree(&jobList,jobNode);

          // notify about changes
          Job_setListModified();
        }
      }
      else
      {
        jobNode = jobNode->next;
      }
    }
  }

  // create UUIDs if empty
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    JobNode *jobNode;
    JOB_LIST_ITERATE(jobNode)
    {
      if (String_isEmpty(jobNode->job.uuid))
      {
        Misc_getUUID(jobNode->job.uuid);
        jobNode->modifiedFlag = TRUE;
      }
    }
  }

  // check for duplicate UUIDs
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ_WRITE,LOCK_TIMEOUT)
  {
    const JobNode *jobNode1;
    jobNode1 = jobList.head;
    while (jobNode1 != NULL)
    {
      const JobNode *jobNode2 = jobNode1->next;
      while (jobNode2 != NULL)
      {
        if (String_equals(jobNode1->job.uuid,jobNode2->job.uuid))
        {
          printWarning(_("duplicate UUID in jobs '%s' and '%s'!"),String_cString(jobNode1->name),String_cString(jobNode2->name));
        }
        jobNode2 = jobNode2->next;
      }
      jobNode1 = jobNode1->next;
    }
  }

  // update jobs
  Job_flushAll();

  // free resources
  String_delete(fileName);

  return ERROR_NONE;
}

Errors Job_write(JobNode *jobNode)
{
  Errors error;

  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if (String_isSet(jobNode->fileName))
  {
    // correct config values
    switch (jobNode->job.options.cryptPasswordMode)
    {
      case PASSWORD_MODE_DEFAULT:
      case PASSWORD_MODE_ASK:
        Password_clear(&jobNode->job.options.cryptPassword);
        break;
      case PASSWORD_MODE_NONE:
      case PASSWORD_MODE_CONFIG:
        // nothing to do
        break;
    }

    // write job config
    error = ConfigValue_writeConfigFile(jobNode->fileName,JOB_CONFIG_VALUES,jobNode,TRUE);
    if (error != ERROR_NONE)
    {
      logMessage(NULL,  // logHandle
                 LOG_TYPE_ERROR,
                 "cannot write job '%s' (error: %s)\n",
                 String_cString(jobNode->fileName),
                 Error_getText(error)
                );
      return error;
    }
    error = File_setPermission(jobNode->fileName,FILE_PERMISSION_USER_READ|FILE_PERMISSION_USER_WRITE);
    if (error != ERROR_NONE)
    {
      logMessage(NULL,  // logHandle
                 LOG_TYPE_WARNING,
                 "cannot set file permissions of job '%s' (error: %s)\n",
                 String_cString(jobNode->fileName),
                 Error_getText(error)
                );
    }

    // save time modified
    jobNode->fileModified = File_getFileTimeModified(jobNode->fileName);

    // free resources
  }

  // reset modified flag
  jobNode->modifiedFlag               = FALSE;
  jobNode->includeExcludeModifiedFlag = FALSE;
  jobNode->mountModifiedFlag          = FALSE;
  jobNode->scheduleModifiedFlag       = FALSE;
  jobNode->persistenceModifiedFlag    = FALSE;

  return ERROR_NONE;
}

void Job_trigger(JobNode      *jobNode,
                 ConstString  scheduleUUID,
                 ArchiveTypes archiveType,
                 ConstString  customText,
                 bool         testCreatedArchives,
                 bool         noStorage,
                 bool         dryRun,
                 uint64       startDateTime,
                 const char   *byName
                )
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if (!Job_isActive(jobNode->jobState))
  {
    // set job state
    jobNode->jobState              = JOB_STATE_WAITING;
    String_set(jobNode->scheduleUUID,scheduleUUID);
    jobNode->archiveType           = archiveType;
    String_set(jobNode->customText,customText);
    jobNode->testCreatedArchives   = testCreatedArchives;
    jobNode->noStorage             = noStorage;
    jobNode->dryRun                = dryRun;
    jobNode->startDateTime         = startDateTime;
    String_setCString(jobNode->byName,byName);

    jobNode->requestedAbortFlag    = FALSE;
    String_clear(jobNode->abortedByInfo);
    jobNode->volumeNumber          = 0;
    jobNode->volumeUnloadFlag      = FALSE;
    Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

    // reset running info
    resetRunningInfo(&jobNode->runningInfo);

    Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);
  }
}

void Job_start(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if (!Job_isRunning(jobNode->jobState))
  {
    // set job state, reset running info
    jobNode->jobState = JOB_STATE_RUNNING;
    resetRunningInfo(&jobNode->runningInfo);

    // increment active counter
    jobList.activeCount++;

    Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);
  }
}

void Job_end(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  // clear job state
  String_clear(jobNode->scheduleUUID);
  String_clear(jobNode->customText);
  if      (jobNode->requestedAbortFlag)
  {
    jobNode->jobState = JOB_STATE_ABORTED;
  }
  else if (jobNode->runningInfo.error != ERROR_NONE)
  {
    jobNode->jobState = JOB_STATE_ERROR;
  }
  else
  {
    jobNode->jobState = JOB_STATE_DONE;
  }

  // decrement active counter
  assert(jobList.activeCount > 0);
  jobList.activeCount--;

  // signal modified
  Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);
}

void Job_abort(JobNode *jobNode, const char *abortedByInfo)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if      (Job_isRunning(jobNode->jobState))
  {
    // request abort job
    jobNode->requestedAbortFlag = TRUE;
    Semaphore_signalModified(&jobList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

    if (Job_isLocal(jobNode))
    {
      // wait until local job terminated
      while (Job_isRunning(jobNode->jobState))
      {
        Semaphore_waitModified(&jobList.lock,LOCK_TIMEOUT);
      }
    }
    else
    {
      // abort slave job
      ConnectorInfo *connectorInfo;
      JOB_CONNECTOR_LOCKED_DO(connectorInfo,jobNode,LOCK_TIMEOUT)
      {
        jobNode->runningInfo.error = Connector_jobAbort(connectorInfo,
                                                        jobNode->job.uuid
                                                       );
      }
    }

    // set abort-by info
    String_setCString(jobNode->abortedByInfo,abortedByInfo);
  }
  else if (Job_isActive(jobNode->jobState))
  {
    jobNode->jobState = JOB_STATE_NONE;
  }
}

void Job_reset(JobNode *jobNode)
{
  assert(jobNode != NULL);
  assert(Semaphore_isLocked(&jobList.lock));

  if (!Job_isActive(jobNode->jobState))
  {
    jobNode->jobState = JOB_STATE_NONE;
    resetRunningInfo(&jobNode->runningInfo);
  }
}

void Job_initOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  memClear(jobOptions,sizeof(JobOptions));

  jobOptions->uuid                                      = String_new();

  jobOptions->includeFileListFileName                   = String_duplicate(globalOptions.includeFileListFileName);
  jobOptions->includeFileCommand                        = String_duplicate(globalOptions.includeFileCommand);
  jobOptions->includeImageListFileName                  = String_duplicate(globalOptions.includeImageListFileName);
  jobOptions->includeImageCommand                       = String_duplicate(globalOptions.includeImageCommand);
  jobOptions->excludeListFileName                       = String_duplicate(globalOptions.excludeListFileName);
  jobOptions->excludeCommand                            = String_duplicate(globalOptions.excludeCommand);

  List_initDuplicate(&jobOptions->mountList,
                     &globalOptions.mountList,
                     NULL,  // fromListFromNode
                     NULL,  // fromListToNode
                     CALLBACK_((ListNodeDuplicateFunction)Configuration_duplicateMountNode,NULL),
                     CALLBACK_((ListNodeFreeFunction)Configuration_freeMountNode,NULL)
                    );
  PatternList_initDuplicate(&jobOptions->compressExcludePatternList,
                            &globalOptions.compressExcludePatternList,
                            NULL,  // fromPatternListFromNode
                            NULL  // fromPatternListToNode
                           );
  DeltaSourceList_initDuplicate(&jobOptions->deltaSourceList,
                                &globalOptions.deltaSourceList,
                                NULL,  // fromDeltaSourceListFromNode
                                NULL  // fromDeltaSourceListToNode
                               );
  List_init(&jobOptions->scheduleList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)Job_freeScheduleNode,NULL));
  List_init(&jobOptions->persistenceList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)Job_freePersistenceNode,NULL));
  jobOptions->persistenceList.lastModificationDateTime  = 0LL;

  jobOptions->archiveType                               = globalOptions.archiveType;

  jobOptions->archivePartSize                           = globalOptions.archivePartSize;
  jobOptions->incrementalListFileName                   = String_duplicate(globalOptions.incrementalListFileName);
  jobOptions->directoryStripCount                       = globalOptions.directoryStripCount;
  jobOptions->destination                               = String_duplicate(globalOptions.destination);
  jobOptions->owner                                     = globalOptions.owner;
  jobOptions->permissions                               = globalOptions.permissions;
  jobOptions->patternType                               = globalOptions.patternType;

  jobOptions->compressAlgorithms.delta                  = globalOptions.compressAlgorithms.delta;
  jobOptions->compressAlgorithms.byte                   = globalOptions.compressAlgorithms.byte;

  for (size_t i = 0; i < 4; i++)
  {
    jobOptions->cryptAlgorithms[i] = globalOptions.cryptAlgorithms[i];
  }
  jobOptions->cryptType                                 = globalOptions.cryptType;
  jobOptions->cryptPasswordMode                         = globalOptions.cryptPasswordMode;
  Password_initDuplicate(&jobOptions->cryptPassword,&globalOptions.cryptPassword);
  Configuration_duplicateKey(&jobOptions->cryptPublicKey,&globalOptions.cryptPublicKey);
  Configuration_duplicateKey(&jobOptions->cryptPrivateKey,&globalOptions.cryptPrivateKey);

  jobOptions->preProcessScript                          = String_new();
  jobOptions->postProcessScript                         = String_new();
  jobOptions->slavePreProcessScript                     = String_new();
  jobOptions->slavePostProcessScript                    = String_new();

#ifdef HAVE_PAR2
  jobOptions->par2Directory                             = String_new();
  jobOptions->par2BlockSize                             = DEFAULT_PAR2_BLOCK_SIZE;
  jobOptions->par2FileCount                             = DEFAULT_PAR2_FILE_COUNT;
  jobOptions->par2BlockCount                            = DEFAULT_PAR2_BLOCK_COUNT;
#endif // HAVE_PAR2

  jobOptions->storageOnMasterFlag                       = TRUE;
  initOptionsFileServer(&jobOptions->fileServer);
  initOptionsFTPServer(&jobOptions->ftpServer);
  initOptionsSSHServer(&jobOptions->sshServer);
  initOptionsWebDAVServer(&jobOptions->webDAVServer);
  initOptionsSMBServer(&jobOptions->smbServer);
  initOptionsOpticalDisk(&jobOptions->opticalDisk);
  initOptionsDevice(&jobOptions->device);

  jobOptions->fragmentSize                              = globalOptions.fragmentSize;
  jobOptions->maxStorageSize                            = globalOptions.maxStorageSize;
  jobOptions->skipUnreadableFlag                        = globalOptions.skipUnreadableFlag;

  jobOptions->testCreatedArchivesFlag                   = globalOptions.testCreatedArchivesFlag;

  jobOptions->volumeSize                                = globalOptions.volumeSize;

  jobOptions->comment                                   = String_duplicate(globalOptions.comment);

  jobOptions->skipUnreadableFlag                        = globalOptions.skipUnreadableFlag;
  jobOptions->forceDeltaCompressionFlag                 = globalOptions.forceDeltaCompressionFlag;
  jobOptions->ignoreNoDumpAttributeFlag                 = globalOptions.ignoreNoDumpAttributeFlag;
  jobOptions->archiveFileMode                           = globalOptions.archiveFileMode;
  jobOptions->restoreEntryMode                          = globalOptions.restoreEntryMode;
  jobOptions->sparseFilesFlag                           = globalOptions.sparseFilesFlag;
  jobOptions->errorCorrectionCodesFlag                  = globalOptions.errorCorrectionCodesFlag;
  jobOptions->alwaysCreateImageFlag                     = globalOptions.alwaysCreateImageFlag;
  jobOptions->blankFlag                                 = globalOptions.blankFlag;
  jobOptions->waitFirstVolumeFlag                       = globalOptions.waitFirstVolumeFlag;
  jobOptions->rawImagesFlag                             = globalOptions.rawImagesFlag;
  jobOptions->noFragmentsCheckFlag                      = globalOptions.noFragmentsCheckFlag;
//TODO: job option or better global option only?
  jobOptions->noIndexDatabaseFlag                       = globalOptions.noIndexDatabaseFlag;
  jobOptions->forceVerifySignaturesFlag                 = globalOptions.forceVerifySignaturesFlag;
  jobOptions->skipVerifySignaturesFlag                  = globalOptions.skipVerifySignaturesFlag;
  jobOptions->noStorage                                 = globalOptions.noStorage;
  jobOptions->noSignatureFlag                           = globalOptions.noSignatureFlag;
//TODO: job option or better global option only?
  jobOptions->noBAROnMediumFlag                         = globalOptions.noBAROnMediumFlag;
  jobOptions->noStopOnErrorFlag                         = globalOptions.noStopOnErrorFlag;
  jobOptions->noStopOnOwnerErrorFlag                    = globalOptions.noStopOnOwnerErrorFlag;
  jobOptions->noStopOnAttributeErrorFlag                = globalOptions.noStopOnAttributeErrorFlag;
  jobOptions->dryRun                                    = globalOptions.dryRun;

  DEBUG_ADD_RESOURCE_TRACE(jobOptions,JobOptions);
}

void Job_copyOptions(JobOptions *jobOptions, const JobOptions *fromJobOptions)
{
  assert(jobOptions != NULL);
  assert(fromJobOptions != NULL);

  DEBUG_CHECK_RESOURCE_TRACE(fromJobOptions);

  memClear(jobOptions,sizeof(JobOptions));

  jobOptions->uuid                                      = String_new();

  jobOptions->includeFileListFileName                   = String_duplicate(fromJobOptions->includeFileListFileName);
  jobOptions->includeFileCommand                        = String_duplicate(fromJobOptions->includeFileCommand);
  jobOptions->includeImageListFileName                  = String_duplicate(fromJobOptions->includeImageListFileName);
  jobOptions->includeImageCommand                       = String_duplicate(fromJobOptions->includeImageCommand);
  jobOptions->excludeListFileName                       = String_duplicate(fromJobOptions->excludeListFileName);
  jobOptions->excludeCommand                            = String_duplicate(fromJobOptions->excludeCommand);

  List_initDuplicate(&jobOptions->mountList,
                     &fromJobOptions->mountList,
                     NULL,  // fromListFromNode
                     NULL,  // fromListToNode
                     CALLBACK_((ListNodeDuplicateFunction)Configuration_duplicateMountNode,NULL),
                     CALLBACK_((ListNodeFreeFunction)Configuration_freeMountNode,NULL)
                    );
  PatternList_initDuplicate(&jobOptions->compressExcludePatternList,
                            &fromJobOptions->compressExcludePatternList,
                            NULL,  // fromPatternListFromNode
                            NULL  // fromPatternListToNode
                           );
  DeltaSourceList_initDuplicate(&jobOptions->deltaSourceList,
                                &fromJobOptions->deltaSourceList,
                                NULL,  // fromDeltaSourceListFromNode
                                NULL  // fromDeltaSourceListToNode
                               );
  List_initDuplicate(&jobOptions->scheduleList,
                     &fromJobOptions->scheduleList,
                     NULL,  // fromListFromNode
                     NULL,  // fromListToNode
                     CALLBACK_((ListNodeDuplicateFunction)Job_duplicateScheduleNode,NULL),
                     CALLBACK_((ListNodeFreeFunction)Job_freeScheduleNode,NULL)
                    );
  List_initDuplicate(&jobOptions->persistenceList,
                     &fromJobOptions->persistenceList,
                     NULL,  // fromListFromNode
                     NULL,  // fromListToNode
                     CALLBACK_((ListNodeDuplicateFunction)Job_duplicatePersistenceNode,NULL),
                     CALLBACK_((ListNodeFreeFunction)Job_freePersistenceNode,NULL)
                    );
  jobOptions->persistenceList.lastModificationDateTime  = 0LL;

  jobOptions->archiveType                               = fromJobOptions->archiveType;

  jobOptions->archivePartSize                           = fromJobOptions->archivePartSize;
  jobOptions->incrementalListFileName                   = String_duplicate(fromJobOptions->incrementalListFileName);
  jobOptions->directoryStripCount                       = fromJobOptions->directoryStripCount;
  jobOptions->destination                               = String_duplicate(fromJobOptions->destination);
  jobOptions->owner                                     = fromJobOptions->owner;
  jobOptions->permissions                               = fromJobOptions->permissions;
  jobOptions->patternType                               = fromJobOptions->patternType;

  jobOptions->compressAlgorithms.delta                  = fromJobOptions->compressAlgorithms.delta;
  jobOptions->compressAlgorithms.byte                   = fromJobOptions->compressAlgorithms.byte;

  for (size_t i = 0; i < 4; i++)
  {
    jobOptions->cryptAlgorithms[i] = fromJobOptions->cryptAlgorithms[i];
  }
  #ifdef HAVE_GCRYPT
    jobOptions->cryptType                               = fromJobOptions->cryptType;
  #else /* not HAVE_GCRYPT */
    jobOptions->cryptType                               = fromJobOptions->cryptType;
  #endif /* HAVE_GCRYPT */
  jobOptions->cryptPasswordMode                         = fromJobOptions->cryptPasswordMode;
  Password_initDuplicate(&jobOptions->cryptPassword,&fromJobOptions->cryptPassword);
  Configuration_duplicateKey(&jobOptions->cryptPublicKey,&fromJobOptions->cryptPublicKey);
  Configuration_duplicateKey(&jobOptions->cryptPrivateKey,&fromJobOptions->cryptPrivateKey);

  jobOptions->preProcessScript                          = String_duplicate(fromJobOptions->preProcessScript);
  jobOptions->postProcessScript                         = String_duplicate(fromJobOptions->postProcessScript);
  jobOptions->slavePreProcessScript                     = String_duplicate(fromJobOptions->slavePreProcessScript);
  jobOptions->slavePostProcessScript                    = String_duplicate(fromJobOptions->slavePostProcessScript);

#ifdef HAVE_PAR2
  jobOptions->par2Directory                             = String_duplicate(fromJobOptions->par2Directory);
  jobOptions->par2BlockSize                             = fromJobOptions->par2BlockSize;
  jobOptions->par2FileCount                             = fromJobOptions->par2FileCount;
  jobOptions->par2BlockCount                            = fromJobOptions->par2BlockCount;
#endif // HAVE_PAR2

  jobOptions->storageOnMasterFlag                       = fromJobOptions->storageOnMasterFlag;
  duplicateOptionsFileServer(&jobOptions->fileServer,&fromJobOptions->fileServer);
  duplicateOptionsFTPServer(&jobOptions->ftpServer,&fromJobOptions->ftpServer);
  duplicateOptionsSSHServer(&jobOptions->sshServer,&fromJobOptions->sshServer);
  duplicateOptionsWebDAVServer(&jobOptions->webDAVServer,&fromJobOptions->webDAVServer);
  duplicateOptionsSMBServer(&jobOptions->smbServer,&fromJobOptions->smbServer);
  duplicateOptionsOpticalDisk(&jobOptions->opticalDisk,&fromJobOptions->opticalDisk);
  duplicateOptionsDevice(&jobOptions->device,&fromJobOptions->device);

  jobOptions->fragmentSize                              = fromJobOptions->fragmentSize;
  jobOptions->maxStorageSize                            = fromJobOptions->maxStorageSize;

  jobOptions->testCreatedArchivesFlag                   = fromJobOptions->testCreatedArchivesFlag;

  jobOptions->volumeSize                                = fromJobOptions->volumeSize;

  jobOptions->comment                                   = String_duplicate(fromJobOptions->comment);

  jobOptions->skipUnreadableFlag                        = fromJobOptions->skipUnreadableFlag;
  jobOptions->forceDeltaCompressionFlag                 = fromJobOptions->forceDeltaCompressionFlag;
  jobOptions->ignoreNoDumpAttributeFlag                 = fromJobOptions->ignoreNoDumpAttributeFlag;
  jobOptions->archiveFileMode                           = fromJobOptions->archiveFileMode;
  jobOptions->restoreEntryMode                          = fromJobOptions->restoreEntryMode;
  jobOptions->sparseFilesFlag                           = fromJobOptions->sparseFilesFlag;
  jobOptions->errorCorrectionCodesFlag                  = fromJobOptions->errorCorrectionCodesFlag;
  jobOptions->alwaysCreateImageFlag                     = fromJobOptions->alwaysCreateImageFlag;
  jobOptions->blankFlag                                 = fromJobOptions->blankFlag;
  jobOptions->waitFirstVolumeFlag                       = fromJobOptions->waitFirstVolumeFlag;
  jobOptions->rawImagesFlag                             = fromJobOptions->rawImagesFlag;
  jobOptions->noFragmentsCheckFlag                      = fromJobOptions->noFragmentsCheckFlag;
//TODO: job option or better global option only?
  jobOptions->noIndexDatabaseFlag                       = fromJobOptions->noIndexDatabaseFlag;
  jobOptions->forceVerifySignaturesFlag                 = fromJobOptions->forceVerifySignaturesFlag;
  jobOptions->skipVerifySignaturesFlag                  = fromJobOptions->skipVerifySignaturesFlag;
  jobOptions->noStorage                                 = fromJobOptions->noStorage;
  jobOptions->noSignatureFlag                           = fromJobOptions->noSignatureFlag;
//TODO: job option or better global option only?
  jobOptions->noBAROnMediumFlag                         = fromJobOptions->noBAROnMediumFlag;
  jobOptions->noStopOnErrorFlag                         = fromJobOptions->noStopOnErrorFlag;
  jobOptions->noStopOnOwnerErrorFlag                    = fromJobOptions->noStopOnOwnerErrorFlag;
  jobOptions->noStopOnAttributeErrorFlag                = fromJobOptions->noStopOnAttributeErrorFlag;
  jobOptions->dryRun                                    = fromJobOptions->dryRun;

  DEBUG_ADD_RESOURCE_TRACE(jobOptions,JobOptions);
}

void Job_getOptions(String jobName, JobOptions *jobOptions, ConstString uuid)
{
  assert(jobOptions != NULL);

  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,LOCK_TIMEOUT)
  {
    const JobNode *jobNode = Job_findByUUID(uuid);
    if (jobNode != NULL)
    {
      String_set(jobName,jobNode->name);
      Job_copyOptions(jobOptions,&jobNode->job.options);
    }
    else
    {
      String_set(jobName,uuid);
      Job_initOptions(jobOptions);
    }
  }
}

void Job_doneOptions(JobOptions *jobOptions)
{
  assert(jobOptions != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(jobOptions,JobOptions);

  String_delete(jobOptions->comment);

  doneOptionsDevice(&jobOptions->device);
  doneOptionsOpticalDisk(&jobOptions->opticalDisk);

  doneOptionsSMBServer(&jobOptions->smbServer);
  doneOptionsWebDAVServer(&jobOptions->webDAVServer);
  doneOptionsSSHServer(&jobOptions->sshServer);
  doneOptionsFTPServer(&jobOptions->ftpServer);
  doneOptionsFileServer(&jobOptions->fileServer);

#ifdef HAVE_PAR2
  String_delete(jobOptions->par2Directory);
#endif // HAVE_PAR2

  String_delete(jobOptions->slavePostProcessScript);
  String_delete(jobOptions->slavePreProcessScript);
  String_delete(jobOptions->postProcessScript);
  String_delete(jobOptions->preProcessScript);

  Configuration_doneKey(&jobOptions->cryptPrivateKey);
  Configuration_doneKey(&jobOptions->cryptPublicKey);
  Password_done(&jobOptions->cryptPassword);

  String_delete(jobOptions->destination);
  String_delete(jobOptions->incrementalListFileName);

  List_done(&jobOptions->persistenceList);
  List_done(&jobOptions->scheduleList);
  DeltaSourceList_done(&jobOptions->deltaSourceList);
  PatternList_done(&jobOptions->compressExcludePatternList);
  List_done(&jobOptions->mountList);

  String_delete(jobOptions->excludeCommand);
  String_delete(jobOptions->excludeListFileName);
  String_delete(jobOptions->includeImageCommand);
  String_delete(jobOptions->includeImageListFileName);
  String_delete(jobOptions->includeFileCommand);
  String_delete(jobOptions->includeFileListFileName);

  String_delete(jobOptions->uuid);
}

void Job_duplicatePersistenceList(PersistenceList *persistenceList, const PersistenceList *fromPersistenceList)
{
  assert(persistenceList != NULL);
  assert(fromPersistenceList != NULL);
  assert(debugIsPersistenceListSorted(fromPersistenceList));

  List_initDuplicate(persistenceList,
                     fromPersistenceList,
                     NULL,  // fromListFromNode
                     NULL,  // fromListToNode
                     CALLBACK_((ListNodeDuplicateFunction)Job_duplicatePersistenceNode,NULL),
                     CALLBACK_((ListNodeFreeFunction)Job_freePersistenceNode,NULL)
                    );
}

void Job_donePersistenceList(PersistenceList *persistenceList)
{
  assert(persistenceList != NULL);
  assert(debugIsPersistenceListSorted(persistenceList));

  List_done(persistenceList);
}

SlaveNode *Job_addSlave(ConstString name, uint port, TLSModes tlsMode)
{
  assert(name != NULL);
  assert(Semaphore_isLocked(&slaveList.lock));

  SlaveNode *slaveNode = LIST_NEW_NODE(SlaveNode);
  if (slaveNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  slaveNode->name               = String_duplicate(name);
  slaveNode->port               = port;
  slaveNode->tlsMode            = tlsMode;
  Connector_init(&slaveNode->connectorInfo);
  slaveNode->lastOnlineDateTime = 0LL;
  slaveNode->authorizedFlag     = FALSE;
  slaveNode->lockCount          = 0;

  List_append(&slaveList,slaveNode);

  return slaveNode;
}

SlaveNode *Job_removeSlave(SlaveNode *slaveNode)
{
  assert(slaveNode != NULL);
  assert(Semaphore_isLocked(&slaveList.lock));
  assert(slaveNode->lockCount == 0);

  if (Connector_isConnected(&slaveNode->connectorInfo))
  {
    Connector_disconnect(&slaveNode->connectorInfo);
  }

  return List_removeAndFree(&slaveList,slaveNode);
}

ConnectorInfo *Job_connectorLock(const JobNode *jobNode, long timeout)
{
  assert(jobNode != NULL);

  ConnectorInfo *connectorInfo = NULL;
  JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,timeout)
  {
    SlaveNode *slaveNode = LIST_FIND(&slaveList,
                                     slaveNode,
                                        (slaveNode->port == jobNode->job.slaveHost.port)
                                     && String_equals(slaveNode->name,jobNode->job.slaveHost.name)
                                    );
    if (slaveNode != NULL)
    {
      connectorInfo = &slaveNode->connectorInfo;
      slaveNode->lockCount++;
    }
  }

  return connectorInfo;
}

void Job_connectorUnlock(ConnectorInfo *connectorInfo, long timeout)
{
  if (connectorInfo != NULL)
  {
    JOB_SLAVE_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,timeout)
    {
      SlaveNode *slaveNode = LIST_FIND(&slaveList,
                                       slaveNode,
                                       &slaveNode->connectorInfo == connectorInfo
                                      );
      if (slaveNode != NULL)
      {
        assert(slaveNode->lockCount > 0);
        slaveNode->lockCount--;
      }
    }
  }
}

void Job_updateNotifies(const JobNode *jobNode)
{
  assert(jobNode != NULL);

  const ScheduleNode *scheduleNode;
  LIST_ITERATE(&jobNode->job.options.scheduleList,scheduleNode)
  {
    if (scheduleNode->archiveType == ARCHIVE_TYPE_CONTINUOUS)
    {
      if (scheduleNode->enabled)
      {
        Continuous_initNotify(jobNode->name,
                              String_cString(jobNode->job.uuid),
                              String_cString(scheduleNode->uuid),
                              scheduleNode->date,
                              scheduleNode->weekDaySet,
                              scheduleNode->beginTime,
                              scheduleNode->endTime,
                              &jobNode->job.includeEntryList
                             );
      }
      else
      {
        Continuous_doneNotify(jobNode->name,
                              String_cString(jobNode->job.uuid),
                              String_cString(scheduleNode->uuid)
                             );
      }
    }
  }
}

void Job_updateAllNotifies(void)
{
  const JobNode *jobNode;

  JOB_LIST_ITERATE(jobNode)
  {
    Job_updateNotifies(jobNode);
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
