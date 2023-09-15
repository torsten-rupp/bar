/***********************************************************************\
*
* $Revision: 4036 $
* $Date: 2015-05-30 01:48:57 +0200 (Sat, 30 May 2015) $
* $Author: torsten $
* Contents: storage SMB/CIFS functions
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
#include <fcntl.h>
#ifdef HAVE_SMB2
  #include <smb2/smb2.h>
  #include <smb2/libsmb2.h>
#endif /* HAVE_SMB2 */
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/autofree.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/files.h"
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

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL Password defaultSMBPassword;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

// ----------------------------------------------------------------------

#ifdef HAVE_SMB2
#endif /* HAVE_SMB2 */
/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : StorageSMB_initAll
* Purpose: initialize SMB/CIFS storage
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_initAll(void)
{
  Password_init(&defaultSMBPassword);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : StorageSMB_doneAll
* Purpose: deinitialize SMB/CIFS storage
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void StorageSMB_doneAll(void)
{
  Password_done(&defaultSMBPassword);
}

/***********************************************************************\
* Name   : StorageSMB_parseSpecifier
* Purpose: parse SMB/CIFS specifier
* Input  : smbSpecifier - SMB/CIFS specifier
* Output : hostName      - host name
*          hostPort      - host port
*          loginName     - login name
*          loginPassword - login password
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSMB_parseSpecifier(ConstString smbSpecifier,
                                     String      hostName,
                                     uint        *hostPort,
                                     String      loginName,
                                     Password    *loginPassword
                                    )
{
  const char* LOGINNAME_MAP_FROM[] = {"\\@"};
  const char* LOGINNAME_MAP_TO[]   = {"@"};

  bool   result;
  String s,t;

  assert(smbSpecifier != NULL);
  assert(hostName != NULL);
  assert(loginName != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(loginName);
  if (loginPassword != NULL) Password_clear(loginPassword);

  s = String_new();
  t = String_new();
  if      (String_matchCString(smbSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,STRING_NO_ASSIGN,loginName,s,STRING_NO_ASSIGN,hostName,t,NULL))
  {
    // <login name>:<login password>@<host name>:<host port>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (loginPassword != NULL) Password_setString(loginPassword,s);
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(t,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(smbSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,loginName,s,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>:<login password>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (loginPassword != NULL) Password_setString(loginPassword,s);

    result = TRUE;
  }
  else if (String_matchCString(smbSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^:]+?):(\\d*)/{0,1}$",NULL,STRING_NO_ASSIGN,loginName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    if (loginName != NULL) String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (String_matchCString(smbSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^/]+)/{0,1}$",NULL,STRING_NO_ASSIGN,loginName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    if (loginName != NULL) String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);

    result = TRUE;
  }
  else if (String_matchCString(smbSpecifier,STRING_BEGIN,"^([^@:/]*?):(\\d*)/{0,1}$",NULL,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <host name>:<host port>
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (!String_isEmpty(smbSpecifier))
  {
    // <host name>
    String_set(hostName,smbSpecifier);

    result = TRUE;
  }
  else
  {
    result = FALSE;
  }
  String_delete(t);
  String_delete(s);

  return result;
}

/***********************************************************************\
* Name   : StorageSMB_equalSpecifiers
* Purpose: compare specifiers if equals
* Input  : storageSpecifier1,storageSpecifier2 - specifiers
*          archiveName1,archiveName2           - archive names (can be
*                                                NULL)
* Output : -
* Return : TRUE iff equals
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSMB_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
                                      ConstString            archiveName1,
                                      const StorageSpecifier *storageSpecifier2,
                                      ConstString            archiveName2
                                     )
{
  assert(storageSpecifier1 != NULL);
  assert(storageSpecifier1->type == STORAGE_TYPE_SMB);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_SMB);

  if (archiveName1 == NULL) archiveName1 = storageSpecifier1->archiveName;
  if (archiveName2 == NULL) archiveName2 = storageSpecifier2->archiveName;

  return    String_equals(storageSpecifier1->hostName,storageSpecifier2->hostName)
         && String_equals(storageSpecifier1->share,storageSpecifier2->share)
         && String_equals(archiveName1,archiveName2);
}

/***********************************************************************\
* Name   : StorageSMB_getName
* Purpose: get storage name
* Input  : string           - name variable (can be NULL)
*          storageSpecifier - storage specifier string
*          archiveName      - archive name (can be NULL)
* Output : -
* Return : storage name
* Notes  : if archiveName is NULL file name from storageSpecifier is used
\***********************************************************************/

LOCAL String StorageSMB_getName(String                 string,
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

  String_appendCString(string,"smb://");
  if (!String_isEmpty(storageSpecifier->loginName))
  {
    String_append(string,storageSpecifier->loginName);
    if (!Password_isEmpty(storageSpecifier->loginPassword))
    {
      String_appendChar(string,':');
      PASSWORD_DEPLOY_DO(plainPassword,storageSpecifier->loginPassword)
      {
        String_appendCString(string,plainPassword);
      }
    }
    String_appendChar(string,'@');
  }
  String_append(string,storageSpecifier->hostName);
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(string,'/');
    String_append(string,storageFileName);
  }

  return string;
}

/***********************************************************************\
* Name   : StorageSMB_getPrintableName
* Purpose: get printable storage name (without password)
* Input  : string           - name variable (can be NULL)
*          storageSpecifier - storage specifier string
*          archiveName      - archive name (can be NULL)
* Output : -
* Return : printable storage name
* Notes  : if archiveName is NULL file name from storageSpecifier is used
\***********************************************************************/

LOCAL void StorageSMB_getPrintableName(String                 string,
                                       const StorageSpecifier *storageSpecifier,
                                       ConstString            archiveName
                                      )
{
  ConstString storageFileName;

  assert(string != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SMB);

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

  String_appendCString(string,"smb://");
  String_append(string,storageSpecifier->hostName);
  if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 22))
  {
    String_appendFormat(string,":%d",storageSpecifier->hostPort);
  }
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(string,'/');
    String_append(string,storageFileName);
  }
}

/***********************************************************************\
* Name   : initSMBLogin
* Purpose: init SMB/CIFS login
* Input  : hostName                - host name
*          loginName               - login name
*          loginPassword           - login password
*          jobOptions              - job options
*          getNamePasswordFunction - get password call-back (can be
*                                    NULL)
*          getNamePasswordUserData - user data for get password call-back
* Output : -
* Return : TRUE if SMB password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initSMBLogin(ConstString             hostName,
                        String                  loginName,
                        Password                *loginPassword,
                        const JobOptions        *jobOptions,
                        GetNamePasswordFunction getNamePasswordFunction,
                        void                    *getNamePasswordUserData
                       )
{
  String s;
  bool   initFlag;

  assert(!String_isEmpty(hostName));
  assert(loginName != NULL);
  assert(loginPassword != NULL);

  initFlag = FALSE;

  if (jobOptions != NULL)
  {
    SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if (Password_isEmpty(&jobOptions->smbServer.password))
      {
        switch (globalOptions.runMode)
        {
          case RUN_MODE_INTERACTIVE:
            if (Password_isEmpty(&defaultSMBPassword))
            {
              s = !String_isEmpty(loginName)
                    ? String_format(String_new(),"SMB login password for %S@%S",loginName,hostName)
                    : String_format(String_new(),"SMB login password for %S",hostName);
              if (Password_input(loginPassword,String_cString(s),PASSWORD_INPUT_MODE_ANY))
              {
                initFlag = TRUE;
              }
              String_delete(s);
            }
            else
            {
              Password_set(loginPassword,&defaultSMBPassword);
              initFlag = TRUE;
            }
            break;
          case RUN_MODE_BATCH:
          case RUN_MODE_SERVER:
            if (getNamePasswordFunction != NULL)
            {
              s = !String_isEmpty(loginName)
                    ? String_format(String_new(),"%S@%S",loginName,hostName)
                    : String_format(String_new(),"%S",hostName);
              if (getNamePasswordFunction(loginName,
                                          loginPassword,
                                          PASSWORD_TYPE_SMB,
                                          String_cString(s),
                                          TRUE,
                                          TRUE,
                                          getNamePasswordUserData
                                         ) == ERROR_NONE
                 )
              {
                initFlag = TRUE;
              }
              String_delete(s);
            }
            break;
        }
      }
      else
      {
        initFlag = TRUE;
      }
    }
  }

  return initFlag;
}

#ifdef HAVE_SMB2

/***********************************************************************\
* Name   : smb2InitSharePath
* Purpose: initialize SMB/CIFS share and path
* Input  : share            - share variable
*          path             - path variable
*          storageSpecifier - storage specifier
*          archiveName      - archive name
* Output : share - SMB/CIFS share name
*          path  - path
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void smb2InitSharePath(String                 *share,
                             String                 *path,
                             const StorageSpecifier *storageSpecifier,
                             ConstString            archiveName
                            )
{
  assert(share != NULL);
  assert(path != NULL);
  assert(archiveName != NULL);

  (*share) = String_new();
  (*path ) = String_new();
  if      (!String_isEmpty(storageSpecifier->share))
  {
    String_set(*share,storageSpecifier->share);
    String_set(*path,archiveName);
  }
  else if (!String_matchCString(archiveName,STRING_BEGIN,"([^/]*)/(.*)",NULL,STRING_NO_ASSIGN,*share,*path,NULL))
  {
    String_set(*share,archiveName);
    String_clear(*path);
  }
}

/***********************************************************************\
* Name   : smb2DoneSharePath
* Purpose: deinitialize SMB/CIFS share and path
* Input  : share - share
*          path  - path
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void smb2DoneSharePath(String share,
                             String path
                            )
{
  assert(share != NULL);
  assert(path != NULL);

  String_delete(path);
  String_delete(share);
}
/***********************************************************************\
* Name   : smb2ConnectShare
* Purpose: connect SMB/CIFS share
* Input  : hostName      - host name
*          loginName     - login name
*          loginPassword - login password
*          share         - SMB/CIFS share name
* Output : smbContext - SMB context
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors smb2ConnectShare(struct smb2_context **smbContext,
                              ConstString         hostName,
                              ConstString         loginName,
                              const Password      *loginPassword,
                              ConstString         share
                             )
{
  int    smbErrorCode;
  Errors error;

  assert(smbContext != NULL);

  // create context
  (*smbContext) = smb2_init_context();
  if ((*smbContext) == NULL)
  {
    return ERROR_INSUFFICIENT_MEMORY;
  }

  // set authentication
  smb2_set_security_mode(*smbContext, SMB2_NEGOTIATE_SIGNING_ENABLED);
// TODO:
  smb2_set_authentication(*smbContext, 1);
  PASSWORD_DEPLOY_DO(plainPassword,loginPassword)
  {
    smb2_set_password(*smbContext,plainPassword);
  }

  // connect share
  smbErrorCode = smb2_connect_share(*smbContext,
                                    String_cString(hostName),
                                    String_cString(share),
                                    String_cString(loginName)
                                   );
  assert(smbErrorCode <= 0);
  if (smbErrorCode != 0)
  {
    error = ERRORX_(SMB,(uint)(-smbErrorCode),"%s",nterror_to_str(smbErrorCode));
    smb2_destroy_context(*smbContext);
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : smb2DisconnectShare
* Purpose: disconnect SMB/CIFS share
* Input  : smbContext - SMB context
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void smb2DisconnectShare(struct smb2_context *smbContext)
{
  assert(smbContext != NULL);

  smb2_disconnect_share(smbContext);
  smb2_destroy_context(smbContext);
}

/***********************************************************************\
* Name   : smb2stat
* Purpose: get SMB/CIFS file status
* Input  : hostName      - host name
*          loginName     - login name
*          loginPassword - login password
*          share         - SMB/CIFS share name
*          path          - file/directory path
* Output : smbStatus - SMB/CIFS status
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors smb2stat(struct smb2_stat_64 *smbStatus,
                      ConstString         hostName,
                      ConstString         loginName,
                      const Password      *loginPassword,
                      ConstString         share,
                      ConstString         path
                     )
{
  Errors              error;
  struct smb2_context *smbContext;
  int                 smbErrorCode;

  error = smb2ConnectShare(&smbContext,
                           hostName,
                           loginName,
                           loginPassword,
                           share
                          );
  if (error == ERROR_NONE)
  {
    smbErrorCode = smb2_stat(smbContext,
                             String_cString(path),
                             smbStatus
                            );
    if (smbErrorCode != 0)
    {
      error = ERRORX_(SMB,(uint)(-smbErrorCode),"%s",nterror_to_str(smbErrorCode));
    }

    smb2DisconnectShare(smbContext);
  }

  return error;
}

/***********************************************************************\
* Name   : checkSMBLogin
* Purpose: check if SMB/CIFS login is possible
* Input  : hostName      - host name
*          loginName     - login name
*          loginPassword - login password
*          share         - SMB/CIFS share name
* Output : -
* Return : ERROR_NONE if login is possible, error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors checkSMBLogin(ConstString hostName,
                           ConstString loginName,
                           Password    *loginPassword,
                           ConstString share
                          )
{
  struct smb2_context *smbContext;
  Errors              error;

  assert(loginName != NULL);

  printInfo(5,"SMB: host %s\n",String_cString(hostName));

  error = smb2ConnectShare(&smbContext,hostName,loginName,loginPassword,share);
  if (error == ERROR_NONE)
  {
    smb2DisconnectShare(smbContext);
  }

  return error;
}

#endif /* HAVE_SMB2 */

/***********************************************************************\
* Name   : StorageSMB_init
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

LOCAL Errors StorageSMB_init(StorageInfo                *storageInfo,
// TODO: unused
const StorageSpecifier *storageSpecifier,
                             const JobOptions           *jobOptions,
                             BandWidthList              *maxBandWidthList,
                             ServerConnectionPriorities serverConnectionPriority
                            )
{
  #ifdef HAVE_SMB2
    AutoFreeList autoFreeList;
    String       share,path;
    Errors       error;
    SMBServer    smbServer;
    uint         retries;
  #endif /* HAVE_SMB2 */

  assert(storageInfo != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SMB);

UNUSED_VARIABLE(storageSpecifier);

  #ifdef HAVE_SMB2
    // init variables
    AutoFree_init(&autoFreeList);
    initBandWidthLimiter(&storageInfo->smb.bandWidthLimiter,maxBandWidthList);
    AUTOFREE_ADD(&autoFreeList,&storageInfo->smb.bandWidthLimiter,{ doneBandWidthLimiter(&storageInfo->smb.bandWidthLimiter); });

    // get SMB/CIFS server settings
    storageInfo->smb.serverId = Configuration_initSMBServerSettings(&smbServer,storageInfo->storageSpecifier.hostName,jobOptions);
    AUTOFREE_ADD(&autoFreeList,&smbServer,{ Configuration_doneSMBServerSettings(&smbServer); });
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_set(storageInfo->storageSpecifier.loginName,smbServer.loginName);
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("USER"));
    if (String_isEmpty(storageInfo->storageSpecifier.share)) String_set(storageInfo->storageSpecifier.share,smbServer.share);
    if (String_isEmpty(storageInfo->storageSpecifier.share)) String_setCString(storageInfo->storageSpecifier.share,getenv("LOGNAME"));
    if (String_isEmpty(storageInfo->storageSpecifier.share)) String_setCString(storageInfo->storageSpecifier.share,getenv("USER"));
    if (String_isEmpty(storageInfo->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate SMB/CIFS server
    if (!allocateServer(storageInfo->smb.serverId,serverConnectionPriority,ALLOCATE_SERVER_TIMEOUT))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageInfo->smb.serverId,{ freeServer(storageInfo->smb.serverId); });

    // get share, path
    smb2InitSharePath(&share,&path,&storageInfo->storageSpecifier,storageInfo->storageSpecifier.archiveName);

    // check if SMB login is possible
    error = ERROR_SMB_AUTHENTICATION;
    if ((Error_getCode(error) == ERROR_CODE_SMB_AUTHENTICATION) && !Password_isEmpty(storageInfo->storageSpecifier.loginPassword))
    {
      error = checkSMBLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.loginName,
                            storageInfo->storageSpecifier.loginPassword,
                            share
                           );
    }
    if ((Error_getCode(error) == ERROR_CODE_SMB_AUTHENTICATION) && !Password_isEmpty(&smbServer.password))
    {
      error = checkSMBLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.loginName,
                            &smbServer.password,
                            share
                           );
      if (error == ERROR_NONE)
      {
        Password_set(storageInfo->storageSpecifier.loginPassword,&smbServer.password);
      }
    }
    if ((Error_getCode(error) == ERROR_CODE_SMB_AUTHENTICATION) && !Password_isEmpty(&smbServer.password))
    {
      error = checkSMBLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.loginName,
                            &defaultSMBPassword,
                            share
                           );
      if (error == ERROR_NONE)
      {
        Password_set(storageInfo->storageSpecifier.loginPassword,&defaultSMBPassword);
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SMB_AUTHENTICATION)
    {
      // initialize interactive/default password
      retries = 0;
      while ((Error_getCode(error) == ERROR_CODE_SMB_AUTHENTICATION) && (retries < MAX_PASSWORD_REQUESTS))
      {
        if (initSMBLogin(storageInfo->storageSpecifier.hostName,
                         storageInfo->storageSpecifier.loginName,
                         storageInfo->storageSpecifier.loginPassword,
                         jobOptions,
                         CALLBACK_(storageInfo->getNamePasswordFunction,storageInfo->getNamePasswordUserData)
                        )
           )
        {
          error = checkSMBLogin(storageInfo->storageSpecifier.hostName,
                                storageInfo->storageSpecifier.loginName,
                                storageInfo->storageSpecifier.loginPassword,
                                share
                               );
        }
        retries++;
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SMB_AUTHENTICATION)
    {
      error = (   !Password_isEmpty(storageInfo->storageSpecifier.loginPassword)
               || !Password_isEmpty(&smbServer.password)
               || !Password_isEmpty(&defaultSMBPassword)
              )
                ? ERRORX_(INVALID_SMB_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName))
                : ERRORX_(NO_SMB_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
    }

    // store password as default SMB/CIFS password
    if (error == ERROR_NONE)
    {
      Password_set(&defaultSMBPassword,storageInfo->storageSpecifier.loginPassword);
    }

    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // free resources
    smb2DoneSharePath(share,path);
    Configuration_doneSMBServerSettings(&smbServer);
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(maxBandWidthList);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_done
* Purpose: deinit storage
* Input  : storageInfo - storage info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_done(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

  #ifdef HAVE_SMB2
    freeServer(storageInfo->smb.serverId);
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageInfo);
  #endif /* HAVE_SMB2 */

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : StorageSMB_isServerAllocationPending
* Purpose: check if server allocation is pending
* Input  : storageInfo - storage info
* Output : -
* Return : TRUE iff server allocation pending
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSMB_isServerAllocationPending(const StorageInfo *storageInfo)
{
  bool serverAllocationPending;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

  serverAllocationPending = FALSE;
  #if defined(HAVE_SMB2)
    serverAllocationPending = isServerAllocationPending(storageInfo->smb.serverId);
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageInfo);

    serverAllocationPending = FALSE;
  #endif /* HAVE_SMB2 */

  return serverAllocationPending;
}

/***********************************************************************\
* Name   : StorageSMB_preProcess
* Purpose: pre-process storage
* Input  : storageInfo - storage info
*          archiveName - archive name
*          time        - time
*          initialFlag - TRUE iff initial call, FALSE otherwise
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_preProcess(const StorageInfo *storageInfo,
                                   ConstString       archiveName,
                                   time_t            time,
                                   bool              initialFlag
                                  )
{
  Errors error;
  #ifdef HAVE_SMB2
    TextMacros (textMacros,2);
  #endif /* HAVE_SMB2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

  error = ERROR_NONE;

  #ifdef HAVE_SMB2
    if (!initialFlag)
    {
      // init macros
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING ("%file",  archiveName,              NULL);
        TEXT_MACRO_X_INTEGER("%number",storageInfo->volumeNumber,NULL);
      }

      // write pre-processing
      if (!String_isEmpty(globalOptions.smb.writePreProcessCommand))
      {
        printInfo(1,"Write pre-processing...");
        error = executeTemplate(String_cString(globalOptions.smb.writePreProcessCommand),
                                time,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL)
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
    }
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(time);
    UNUSED_VARIABLE(initialFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */

  return error;
}

/***********************************************************************\
* Name   : StorageSMB_postProcess
* Purpose: post-process storage
* Input  : storageInfo - storage info
*          archiveName - archive name
*          time        - time
*          finalFlag   - TRUE iff final call, FALSE otherwise
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_postProcess(const StorageInfo *storageInfo,
                                    ConstString       archiveName,
                                    time_t            time,
                                    bool              finalFlag
                                   )
{
  Errors error;
  #ifdef HAVE_SMB2
    TextMacros (textMacros,2);
  #endif /* HAVE_SMB2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

  error = ERROR_NONE;

  #ifdef HAVE_SMB2
    if (!finalFlag)
    {
      // init macros
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING ("%file",  archiveName,                NULL);
        TEXT_MACRO_X_INTEGER("%number",storageInfo->volumeNumber,NULL);
      }

      // write post-process
      if (!String_isEmpty(globalOptions.smb.writePostProcessCommand))
      {
        printInfo(1,"Write post-processing...");
        error = executeTemplate(String_cString(globalOptions.smb.writePostProcessCommand),
                                time,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL)
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
    }
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(time);
    UNUSED_VARIABLE(finalFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */

  return error;
}

/***********************************************************************\
* Name   : Storage_exists
* Purpose: check if storage file exists
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE iff storage file exists
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSMB_exists(const StorageInfo *storageInfo, ConstString archiveName)
{
  #ifdef HAVE_SMB2
    String              share,path;
    bool                existsFlag;
    struct smb2_stat_64 smbStatus;
  #endif /* HAVE_SMB2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

  #ifdef HAVE_SMB2
    smb2InitSharePath(&share,&path,&storageInfo->storageSpecifier,archiveName);

    existsFlag = (smb2stat(&smbStatus,
                           storageInfo->storageSpecifier.hostName,
                           storageInfo->storageSpecifier.loginName,
                           storageInfo->storageSpecifier.loginPassword,
                           share,
                           path
                          ) == ERROR_NONE
                 );

    smb2DoneSharePath(share,path);

    return existsFlag;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

    return FALSE;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_isFile
* Purpose: check if storage file
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE if storage file, FALSE otherweise
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSMB_isFile(const StorageInfo *storageInfo, ConstString archiveName)
{
  #ifdef HAVE_SMB2
    String              share,path;
    bool                isFileFlag;
    struct smb2_stat_64 smbStatus;
  #endif /* HAVE_SMB2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

  #ifdef HAVE_SMB2
    smb2InitSharePath(&share,&path,&storageInfo->storageSpecifier,archiveName);

    isFileFlag =    (smb2stat(&smbStatus,
                              storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.loginName,
                              storageInfo->storageSpecifier.loginPassword,
                              share,
                              path
                             ) == ERROR_NONE
                    )
                 && (smbStatus.smb2_type == SMB2_TYPE_FILE);

    smb2DoneSharePath(share,path);

    return isFileFlag;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

    return FALSE;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_isDirectory
* Purpose: check if storage directory
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE if storage directory, FALSE otherweise
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSMB_isDirectory(const StorageInfo *storageInfo, ConstString archiveName)
{
  #ifdef HAVE_SMB2
    String              share,path;
    bool                isDirectoryFlag;
    struct smb2_stat_64 smbStatus;
  #endif /* HAVE_SMB2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

  #ifdef HAVE_SMB2
    smb2InitSharePath(&share,&path,&storageInfo->storageSpecifier,archiveName);

    isDirectoryFlag =    (smb2stat(&smbStatus,
                                   storageInfo->storageSpecifier.hostName,
                                   storageInfo->storageSpecifier.loginName,
                                   storageInfo->storageSpecifier.loginPassword,
                                   share,
                                   path
                                  ) == ERROR_NONE
                         )
                      && (smbStatus.smb2_type == SMB2_TYPE_DIRECTORY);

    smb2DoneSharePath(share,path);

    return isDirectoryFlag;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

    return FALSE;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_isReadable
* Purpose: check if storage file exists and is readable
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE if storage file/directory exists and is readable, FALSE
*          otherweise
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSMB_isReadable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

return ERROR_STILL_NOT_IMPLEMENTED;
}

/***********************************************************************\
* Name   : Storage_isWritable
* Purpose: check if storage file exists and is writable
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE if storage file/directory exists and is writable, FALSE
*          otherweise
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSMB_isWritable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

/***********************************************************************\
* Name   : StorageSMB_getTmpName
* Purpose: get temporary archive name
* Input  : archiveName - archive name variable
*          storageInfo - storage info
* Output : archiveName - temporary archive name
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_getTmpName(String archiveName, const StorageInfo *storageInfo)
{
  assert(archiveName != NULL);
  assert(!String_isEmpty(archiveName));
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(archiveName);
  UNUSED_VARIABLE(storageInfo);

//TODO
  return ERROR_STILL_NOT_IMPLEMENTED;
}

/***********************************************************************\
* Name   : StorageSMB_create
* Purpose: create new/append to storage
* Input  : storageHandle - storage handle variable
*          archiveName   - archive name (can be NULL)
*          archiveSize   - archive size [bytes]
*          forceFlag     - TRUE to force overwrite existing storage
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_create(StorageHandle *storageHandle,
                               ConstString   fileName,
                               uint64        fileSize,
                               bool          forceFlag
                              )
{
  #ifdef HAVE_SMB2
    String              share,path;
    Errors              error;
    String              directoryName;
    struct smb2_stat_64 smbStatus;
    int                 smbErrorCode;
  #endif /* HAVE_SMB2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);

  // check if file exists
  if (   !forceFlag
      && (storageHandle->storageInfo->jobOptions != NULL)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_APPEND)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_OVERWRITE)
      && StorageSMB_exists(storageHandle->storageInfo,fileName)
     )
  {
    return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(fileName));
  }

  #ifdef HAVE_SMB2
    // init variables
    storageHandle->smb.totalSentBytes     = 0LL;
    storageHandle->smb.totalReceivedBytes = 0LL;
    storageHandle->smb.context            = NULL;
    storageHandle->smb.fileHandle         = NULL;
    storageHandle->smb.index              = 0LL;
    storageHandle->smb.size               = 0LL;
// TODO: remove
//    storageHandle->smb.readAheadBuffer.offset = 0LL;
//    storageHandle->smb.readAheadBuffer.length = 0L;

    smb2InitSharePath(&share,&path,&storageHandle->storageInfo->storageSpecifier,fileName);

    // connect share
    error = smb2ConnectShare(&storageHandle->smb.context,
                             storageHandle->storageInfo->storageSpecifier.hostName,
                             storageHandle->storageInfo->storageSpecifier.loginName,
                             storageHandle->storageInfo->storageSpecifier.loginPassword,
                             share
                            );
    if (error != ERROR_NONE)
    {
      smb2DoneSharePath(share,path);
      return error;
    }

    // get max. number of bytes to write
    storageHandle->smb.maxReadWriteBytes = smb2_get_max_write_size(storageHandle->smb.context);

    // create directory if not existing
    directoryName = File_getDirectoryName(String_new(),path);
    if (!String_isEmpty(directoryName))
    {
      if (smb2_stat(storageHandle->smb.context,
                      String_cString(directoryName),
                      &smbStatus
                     ) == 0
         )
      {
        // check if directory
        if (smbStatus.smb2_type != SMB2_TYPE_DIRECTORY)
        {
          error = ERRORX_(NOT_A_DIRECTORY,0,"%s",String_cString(directoryName));
          String_delete(directoryName);
          smb2DisconnectShare(storageHandle->smb.context);
          smb2DoneSharePath(share,path);
          return error;
        }
      }
      else
      {
        // create directory
        smbErrorCode = smb2_mkdir(storageHandle->smb.context,String_cString(directoryName));
        if (smbErrorCode != 0)
        {
          error = ERRORX_(SMB,(uint)(-smbErrorCode),"create '%s' fail: %s",String_cString(directoryName),nterror_to_str(smbErrorCode));
          String_delete(directoryName);
          smb2DisconnectShare(storageHandle->smb.context);
          smb2DoneSharePath(share,path);
          return error;
        }
      }
    }
    String_delete(directoryName);

    // create file
    storageHandle->smb.fileHandle = smb2_open(storageHandle->smb.context,
                                              String_cString(path),
                                              O_WRONLY|O_CREAT
                                             );
    if (storageHandle->smb.fileHandle == NULL)
    {
      // TODO: use smb2_get_nterror(storageHandle->smb.context) instead of 0 (still no availabvle in v4.0.0)
      error = ERRORX_(SMB,0,"%s",smb2_get_error(storageHandle->smb.context));
      smb2DisconnectShare(storageHandle->smb.context);
      smb2DoneSharePath(share,path);
      return error;
    }

    // free resources
    smb2DoneSharePath(share,path);

    return ERROR_NONE;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileSize);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_open
* Purpose: open storage for reading
* Input  : storageHandle - storage handle variable
*          archiveName   - archive name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_open(StorageHandle *storageHandle,
                             ConstString   archiveName
                            )
{
  #ifdef HAVE_SMB2
    String              share,path;
    Errors              error;
    int                 smbErrorCode;
    struct smb2_stat_64 status;
  #endif /* HAVE_SMB2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);
  assert(!String_isEmpty(archiveName));

  #ifdef HAVE_SMB2
    // init variables
    storageHandle->smb.totalSentBytes     = 0LL;
    storageHandle->smb.totalReceivedBytes = 0LL;
    storageHandle->smb.fileHandle         = NULL;
    storageHandle->smb.index              = 0LL;
    storageHandle->smb.size               = 0LL;

    smb2InitSharePath(&share,&path,&storageHandle->storageInfo->storageSpecifier,archiveName);

    // connect share
    error = smb2ConnectShare(&storageHandle->smb.context,
                             storageHandle->storageInfo->storageSpecifier.hostName,
                             storageHandle->storageInfo->storageSpecifier.loginName,
                             storageHandle->storageInfo->storageSpecifier.loginPassword,
                             share
                            );
    if (error != ERROR_NONE)
    {
      smb2DoneSharePath(share,path);
      return error;
    }

    // get max. number of bytes to read
    storageHandle->smb.maxReadWriteBytes = smb2_get_max_read_size(storageHandle->smb.context);

    // open file
    storageHandle->smb.fileHandle = smb2_open(storageHandle->smb.context,
                                              String_cString(path),
                                              O_RDONLY
                                             );
    if (storageHandle->smb.fileHandle == NULL)
    {
      // TODO: use smb2_get_nterror(storageHandle->smb.context) instead of 0 (still no availabvle in v4.0.0)
      error = ERRORX_(SMB,0,"%s",smb2_get_error(storageHandle->smb.context));
      smb2DisconnectShare(storageHandle->smb.context);
      smb2DoneSharePath(share,path);
      return error;
    }

    // get file size
    smbErrorCode = smb2_stat(storageHandle->smb.context,
                             String_cString(path),
                             &status
                            );
    assert(smbErrorCode <= 0);
    if (smbErrorCode != 0)
    {
      error = ERRORX_(SMB,(uint)(-smbErrorCode),"%s",nterror_to_str(smbErrorCode));
      (void)smb2_close(storageHandle->smb.context,storageHandle->smb.fileHandle);
      smb2DisconnectShare(storageHandle->smb.context);
      smb2DoneSharePath(share,path);
      return error;
    }
    storageHandle->smb.size = status.smb2_size;

    // free resources
    smb2DoneSharePath(share,path);

    return ERROR_NONE;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_close
* Purpose: close storage file
* Input  : storageHandle - storage handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void StorageSMB_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

  #ifdef HAVE_SMB2
    switch (storageHandle->mode)
    {
      case STORAGE_MODE_READ:
        (void)smb2_close(storageHandle->smb.context,storageHandle->smb.fileHandle);
        break;
      case STORAGE_MODE_WRITE:
        (void)smb2_close(storageHandle->smb.context,storageHandle->smb.fileHandle);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    smb2DisconnectShare(storageHandle->smb.context);
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_eof
* Purpose: check if end-of-file in storage
* Input  : storageHandle - storage handle
* Output : -
* Return : TRUE if end-of-file, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSMB_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

  #ifdef HAVE_SMB2
    return storageHandle->smb.index >= storageHandle->smb.size;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageHandle);

    return TRUE;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_read
* Purpose: read from storage file
* Input  : storageHandle - storage handle
*          buffer        - buffer with data to write
*          size          - data size
*          bytesRead     - number of bytes read or NULL
* Output : bytesRead - number of bytes read
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_read(StorageHandle *storageHandle,
                             void          *buffer,
                             ulong         bufferSize,
                             ulong         *bytesRead
                            )
{
  #ifdef HAVE_SMB2
    Errors  error;
//    ulong   index;
    ulong   length;
    uint64  startTimestamp,endTimestamp;
    int     n;
    uint64  startTotalReceivedBytes,endTotalReceivedBytes;
  #endif /* HAVE_SMB2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);
  #ifdef HAVE_SMB2
    assert(storageHandle->smb.context != NULL);
    assert(storageHandle->smb.fileHandle != NULL);
//      assert(storageHandle->smb.readAheadBuffer.data != NULL);
  #endif /* HAVE_SMB2 */
  assert(buffer != NULL);

  #ifdef HAVE_SMB2
    if (bytesRead != NULL) (*bytesRead) = 0L;

    error = ERROR_NONE;
    while (   (bufferSize > 0)
           && (error == ERROR_NONE)
          )
    {
// TODO: remove
#if 0
      // copy as much data as available from read-ahead buffer
      if (   (storageHandle->smb.index >= storageHandle->smb.readAheadBuffer.offset)
          && (storageHandle->smb.index < (storageHandle->smb.readAheadBuffer.offset+storageHandle->smb.readAheadBuffer.length))
         )
      {
        // copy data from read-ahead buffer
        index      = (ulong)(storageHandle->smb.index-storageHandle->smb.readAheadBuffer.offset);
        bytesAvail = MIN(bufferSize,storageHandle->smb.readAheadBuffer.length-index);
        memCopyFast(buffer,bytesAvail,storageHandle->smb.readAheadBuffer.data+index,bytesAvail);

        // adjust buffer, bufferSize, bytes read, index
        buffer = (byte*)buffer+bytesAvail;
        bufferSize -= bytesAvail;
        if (bytesRead != NULL) (*bytesRead) += bytesAvail;
        storageHandle->smb.index += (uint64)bytesAvail;
      }
#endif

      // read rest of data
      if (bufferSize > 0)
      {
// TODO:
//          assert(storageHandle->smb.index >= (storageHandle->smb.readAheadBuffer.offset+storageHandle->smb.readAheadBuffer.length));

        // get max. number of bytes to receive in one step
        if (storageHandle->storageInfo->smb.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(MIN(storageHandle->storageInfo->smb.bandWidthLimiter.blockSize,bufferSize),storageHandle->smb.maxReadWriteBytes);
        }
        else
        {
          length = MIN(bufferSize,storageHandle->smb.maxReadWriteBytes);
        }
        assert(length > 0L);

        // get start time, start received bytes
        startTimestamp          = Misc_getTimestamp();
        startTotalReceivedBytes = storageHandle->smb.totalReceivedBytes;

#if 0
        if (length <= MAX_BUFFER_SIZE)
        {
          // read into read-ahead buffer
          error = smb2_read(storageHandle->smb.context,
                            storageHandle->smb.fileHandle,
                            storageHandle->smb.readAheadBuffer.data,
                            MIN((size_t)(storageHandle->smb.size-storageHandle->smb.index),MAX_BUFFER_SIZE),
                            &bytesAvail
                           );
          if (error != ERROR_NONE)
          {
            break;
          }
          storageHandle->smb.readAheadBuffer.offset = storageHandle->smb.index;
          storageHandle->smb.readAheadBuffer.length = bytesAvail;

          // copy data from read-ahead buffer
          bytesAvail = MIN(length,storageHandle->smb.readAheadBuffer.length);
          memcpy(buffer,storageHandle->smb.readAheadBuffer.data,bytesAvail);

          // adjust buffer, bufferSize, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          bufferSize -= bytesAvail;
          if (bytesRead != NULL) (*bytesRead) += bytesAvail;
          storageHandle->smb.index += (uint64)bytesAvail;
        }
        else
        {
#endif
          // read direct
          n = smb2_read(storageHandle->smb.context,
                        storageHandle->smb.fileHandle,
                        buffer,
                        length
                       );
          if (n <= 0)
          {
            error = ERRORX_(SMB,(uint)(-n),"%s",smb2_get_error(storageHandle->smb.context));
            break;
          }

          // adjust buffer, bufferSize, bytes read, index
          buffer = (byte*)buffer+(ulong)n;
          bufferSize -= (ulong)n;
          if (bytesRead != NULL) (*bytesRead) += (ulong)n;
          storageHandle->smb.index += (uint64)n;
//          }

        // get end time, end received bytes
        endTimestamp          = Misc_getTimestamp();
        endTotalReceivedBytes = storageHandle->smb.totalReceivedBytes;
        assert(endTotalReceivedBytes >= startTotalReceivedBytes);

        /* limit used band width if requested (note: when the system time is
           changing endTimestamp may become smaller than startTimestamp;
           thus do not check this with an assert())
        */
        if (endTimestamp >= startTimestamp)
        {
          SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            limitBandWidth(&storageHandle->storageInfo->smb.bandWidthLimiter,
                           endTotalReceivedBytes-startTotalReceivedBytes,
                           endTimestamp-startTimestamp
                          );
          }
        }
      }
    }

    return error;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferSize);
    UNUSED_VARIABLE(bytesRead);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_write
* Purpose: write into storage file
* Input  : storageHandle - storage handle
*          buffer        - buffer with data to write
*          size          - data size
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_write(StorageHandle *storageHandle,
                              const void    *buffer,
                              ulong         bufferLength
                             )
{
  #ifdef HAVE_SMB2
    Errors  error;
    ulong   bytesWritten;
    ulong   length;
    uint64  startTimestamp,endTimestamp;
    uint64  startTotalSentBytes,endTotalSentBytes;
    int     n;
  #endif /* HAVE_SMB2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);
  #ifdef HAVE_SMB2
    assert(storageHandle->smb.context != NULL);
    assert(storageHandle->smb.fileHandle != NULL);
  #endif /* HAVE_SMB2 */
  assert(buffer != NULL);

  #ifdef HAVE_SMB2
    bytesWritten = 0L;
    error        = ERROR_NONE;
    while (bytesWritten < bufferLength)
    {
      // get max. number of bytes to send in one step
      if (storageHandle->storageInfo->smb.bandWidthLimiter.maxBandWidthList != NULL)
      {
        length = MIN(MIN(storageHandle->storageInfo->smb.bandWidthLimiter.blockSize,bufferLength-bytesWritten),storageHandle->smb.maxReadWriteBytes);
      }
      else
      {
        length = MIN(bufferLength-bytesWritten,storageHandle->smb.maxReadWriteBytes);
      }
      assert(length > 0L);

      // get start time, start received bytes
      startTimestamp      = Misc_getTimestamp();
      startTotalSentBytes = storageHandle->smb.totalSentBytes;

      // send data
      n = smb2_write(storageHandle->smb.context,
                     storageHandle->smb.fileHandle,
                     buffer,
                     length
                    );
      if (n <= 0)
      {
        error = ERRORX_(SMB,(uint)(-n),"%s",smb2_get_error(storageHandle->smb.context));
        break;
      }
      buffer = (byte*)buffer+n;
      bytesWritten += n;

      // get end time, end received bytes
      endTimestamp      = Misc_getTimestamp();
      endTotalSentBytes = storageHandle->smb.totalSentBytes;
      assert(endTotalSentBytes >= startTotalSentBytes);

      /* limit used band width if requested (note: when the system time is
         changing endTimestamp may become smaller than startTimestamp;
         thus do not check this with an assert())
      */
      if (endTimestamp >= startTimestamp)
      {
        SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          limitBandWidth(&storageHandle->storageInfo->smb.bandWidthLimiter,
                         endTotalSentBytes-startTotalSentBytes,
                         endTimestamp-startTimestamp
                        );
        }
      }
    }
    storageHandle->smb.size += bytesWritten;

    return error;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferLength);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_getSize
* Purpose: get storage file size
* Input  : storageHandle - storage handle
* Output : -
* Return : size of storage
* Notes  : -
\***********************************************************************/

LOCAL uint64 StorageSMB_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

  size = 0LL;
  #ifdef HAVE_SMB2
    size = storageHandle->smb.size;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_SMB2 */

  return size;
}

/***********************************************************************\
* Name   : StorageSMB_tell
* Purpose: get current position in storage file
* Input  : storageHandle - storage handle
* Output : offset - offset (0..n-1)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_tell(StorageHandle *storageHandle,
                             uint64        *offset
                            )
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);
  assert(offset != NULL);

  #ifdef HAVE_SMB2
    (*offset) = storageHandle->smb.index;
    return ERROR_NONE;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_seek
* Purpose: seek in storage file
* Input  : storageHandle - storage handle
*          offset        - offset (0..n-1)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_seek(StorageHandle *storageHandle,
                             uint64        offset
                            )
{
  Errors error;
  #ifdef HAVE_SMB2
//    uint64 skip;
//    uint64 i;
    int64 n;
  #endif /* HAVE_SMB2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

  error = ERROR_UNKNOWN;
  #ifdef HAVE_SMB2
// TODO:
    assert(storageHandle->smb.context != NULL);
    assert(storageHandle->smb.fileHandle != NULL);
//    assert(storageHandle->smb.readAheadBuffer.data != NULL);

// TODO: remove
#if 0
    if      (offset > storageHandle->smb.index)
    {
      skip = offset-storageHandle->smb.index;
      if (skip > 0LL)
      {
        // skip data in read-ahead buffer
        if (   (storageHandle->smb.index >= storageHandle->smb.readAheadBuffer.offset)
            && (storageHandle->smb.index < (storageHandle->smb.readAheadBuffer.offset+storageHandle->smb.readAheadBuffer.length))
           )
        {
          i = storageHandle->smb.index-storageHandle->smb.readAheadBuffer.offset;
          n = MIN(skip,storageHandle->smb.readAheadBuffer.length-i);
          skip -= n;
          storageHandle->smb.index += (uint64)n;
        }

        if (skip > 0LL)
        {
          (void)smb2_lseek(storageHandle->smb.context,
                           storageHandle->smb.fileHandle,
                           offset,
                           SEEK_SET,
                           NULL
                          );
          storageHandle->smb.readAheadBuffer.offset = offset;
//          storageHandle->smb.readAheadBuffer.length = 0L;

          storageHandle->smb.index = offset;
        }
      }
    }
    else if (offset < storageHandle->smb.index)
    {

      skip = storageHandle->smb.index-offset;
      if (skip > 0LL)
      {
        // skip data in read-ahead buffer
        if (   (storageHandle->smb.index >= storageHandle->smb.readAheadBuffer.offset)
            && (storageHandle->smb.index < (storageHandle->smb.readAheadBuffer.offset+storageHandle->smb.readAheadBuffer.length))
           )
        {
          i = storageHandle->smb.index-storageHandle->smb.readAheadBuffer.offset;
          n = MIN(skip,i);
          skip -= n;
          storageHandle->smb.index -= (uint64)n;
        }

        if (skip > 0LL)
        {
          #if   defined(HAVE_SMB2_SFTP_SEEK64)
            libssh2_smb_seek64(storageHandle->smb.smbHandle,offset);
          #elif defined(HAVE_SMB2_SFTP_SEEK2)
            libssh2_smb_seek2(storageHandle->smb.smbHandle,offset);
          #else /* not HAVE_SMB2_SFTP_SEEK64 || HAVE_SMB2_SFTP_SEEK2 */
            libssh2_smb_seek(storageHandle->smb.smbHandle,(size_t)offset);
          #endif /* HAVE_SMB2_SFTP_SEEK64 || HAVE_SMB2_SFTP_SEEK2 */
          storageHandle->smb.readAheadBuffer.offset = offset;
          storageHandle->smb.readAheadBuffer.length = 0L;

          storageHandle->smb.index = offset;
        }
      }
    }
#else
    n = smb2_lseek(storageHandle->smb.context,
                   storageHandle->smb.fileHandle,
                   (int64)offset,
                   SEEK_SET,
                   NULL
                  );
    if (n >= 0)
    {
      storageHandle->smb.index = (uint64)n;
      error = ERROR_NONE;
    }
    else
    {
      error = ERRORX_(SMB,(uint)(-n),"%s",smb2_get_error(storageHandle->smb.context));
    }
#endif
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

/***********************************************************************\
* Name   : StorageSMB_rename
* Purpose: rename storage file
* Input  : storageInfo    - storage
*          oldArchiveName - archive names (can be NULL)
*          newArchiveName - new archive name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_rename(const StorageInfo *storageInfo,
                               ConstString       fromArchiveName,
                               ConstString       toArchiveName
                              )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);

UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(fromArchiveName);
UNUSED_VARIABLE(toArchiveName);
error = ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

/***********************************************************************\
* Name   : StorageSMB_makeDirectory
* Purpose: create directories
* Input  : storageInfo - storage info
*          pathName    - path name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_makeDirectory(const StorageInfo *storageInfo,
                                      ConstString       directoryName
                                     )
{
  #ifdef HAVE_SMB2
    String              share,path;
    Errors              error;
    struct smb2_context *smbContext;
    int                 smbErrorCode;
  #endif /* HAVE_SMB2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);
  assert(!String_isEmpty(directoryName));

  #ifdef HAVE_SMB2
    smb2InitSharePath(&share,&path,&storageInfo->storageSpecifier,directoryName);

    error = smb2ConnectShare(&smbContext,
                             storageInfo->storageSpecifier.hostName,
                             storageInfo->storageSpecifier.loginName,
                             storageInfo->storageSpecifier.loginPassword,
                             share
                            );
    if (error == ERROR_NONE)
    {
      smbErrorCode = smb2_mkdir(smbContext,String_cString(path));
      if (smbErrorCode != 0)
      {
        error = ERRORX_(SMB,(uint)(-smbErrorCode),"%s",nterror_to_str(smbErrorCode));
      }
    }

    smb2DoneSharePath(share,path);

    return error;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(directoryName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_delete
* Purpose: delete storage file/directory
* Input  : storageInfo - storage
*          archiveName - archive name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_delete(const StorageInfo *storageInfo,
                               ConstString       archiveName
                              )
{
  #ifdef HAVE_SMB2
    String              share,path;
    Errors              error;
    struct smb2_context *smbContext;
    int                 smbErrorCode;
  #endif /* HAVE_SMB2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);
  assert(!String_isEmpty(archiveName));

  #ifdef HAVE_SMB2
    smb2InitSharePath(&share,&path,&storageInfo->storageSpecifier,archiveName);

    error = smb2ConnectShare(&smbContext,
                             storageInfo->storageSpecifier.hostName,
                             storageInfo->storageSpecifier.loginName,
                             storageInfo->storageSpecifier.loginPassword,
                             share
                            );
    if (error == ERROR_NONE)
    {
      smbErrorCode = smb2_unlink(smbContext,String_cString(path));
      if (smbErrorCode != 0)
      {
        error = ERRORX_(SMB,(uint)(-smbErrorCode),"%s",nterror_to_str(smbErrorCode));
      }
    }

    smb2DoneSharePath(share,path);

    return error;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
}

#if 0
//TODO
Errors StorageSMB_getInfo(FileInfo          *fileInfo,
                          const StorageInfo *storageInfo,
                          ConstString       archiveName
                         )
{
  Errors       error;
  #ifdef HAVE_SMB2
    SocketHandle socketHandle;
  #endif /* HAVE_SMB2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SMB);
  assert(fileInfo != NULL);

  memClear(fileInfo,sizeof(FileInfo));

  error = ERROR_UNKNOWN;
  #ifdef HAVE_SMB2
    {
      LIBSSH2_SFTP_ATTRIBUTES smbAttributes;

      error = Network_connect(&socketHandle,
                              SOCKET_TYPE_SSH,
                              storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.loginName,
                              storageInfo->storageSpecifier.loginPassword,
                              NULL,  // caData
                              0,     // caLength
                              NULL,  // certData
                              0,     // certLength
                              storageInfo->smb.publicKey.data,
                              storageInfo->smb.publicKey.length,
                              storageInfo->smb.privateKey.data,
                              storageInfo->smb.privateKey.length,
                                SOCKET_FLAG_NONE
                              | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                              | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                              30*MS_PER_SECOND
                             );
      if (error == ERROR_NONE)
      {
        libssh2_session_set_timeout(Network_getSSHSession(&socketHandle),READ_TIMEOUT);

        error = smb2ConnectShare(&socketHandle,
                                 archiveName,
                                 fileInfo
                                );

//        Network_disconnect(&socketHandle);
      }
    }
  #else /* not HAVE_SMB2 */
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}
#endif

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : StorageSMB_openDirectoryList
* Purpose: open storage directory list for reading directory entries
* Input  : storageDirectoryListHandle - storage directory list handle
*                                       variable
*          storageSpecifier           - storage specifier
*          pathName                   - path name
*          jobOptions                 - job options
*          serverConnectionPriority   - server connection priority
* Output : storageDirectoryListHandle - initialized storage directory
*                                       list handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                          const StorageSpecifier     *storageSpecifier,
                                          ConstString                pathName,
                                          const JobOptions           *jobOptions,
                                          ServerConnectionPriorities serverConnectionPriority
                                         )
{
  #ifdef HAVE_SMB2
    AutoFreeList autoFreeList;
    String       share,path;
    Errors       error;
    SMBServer    smbServer;
    uint         retries;
  #endif /* HAVE_SMB2 */

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SMB);
  assert(pathName != NULL);
  assert(jobOptions != NULL);

  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(serverConnectionPriority);

  #ifdef HAVE_SMB2
// TODO:
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
    // init variables
    AutoFree_init(&autoFreeList);

    // get SMB/CIFS server settings
    storageDirectoryListHandle->smb.serverId = Configuration_initSMBServerSettings(&smbServer,storageDirectoryListHandle->storageSpecifier.hostName,jobOptions);
    AUTOFREE_ADD(&autoFreeList,&smbServer,{ Configuration_doneSMBServerSettings(&smbServer); });
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_set(storageDirectoryListHandle->storageSpecifier.loginName,smbServer.loginName);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("USER"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.share)) String_set(storageDirectoryListHandle->storageSpecifier.share,smbServer.share);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.share)) String_setCString(storageDirectoryListHandle->storageSpecifier.share,getenv("LOGNAME"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.share)) String_setCString(storageDirectoryListHandle->storageSpecifier.share,getenv("USER"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate SMB/CIFS server
    if (!allocateServer(storageDirectoryListHandle->smb.serverId,serverConnectionPriority,ALLOCATE_SERVER_TIMEOUT))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->smb.serverId,{ freeServer(storageDirectoryListHandle->smb.serverId); });

    // get share, path
    smb2InitSharePath(&share,&path,&storageDirectoryListHandle->storageSpecifier,pathName);
    AUTOFREE_ADD(&autoFreeList,&share,{ smb2DoneSharePath(share,path); });

    // check if SMB/CIFS login is possible
    error = ERROR_SMB_AUTHENTICATION;
    if ((Error_getCode(error) == ERROR_CODE_SMB_AUTHENTICATION) && !Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword))
    {
      error = checkSMBLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.loginName,
                            storageDirectoryListHandle->storageSpecifier.loginPassword,
                            share
                           );
    }
    if ((Error_getCode(error) == ERROR_CODE_SMB_AUTHENTICATION) && !Password_isEmpty(&smbServer.password))
    {
      error = checkSMBLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.loginName,
                            &smbServer.password,
                            share
                           );
      if (error == ERROR_NONE)
      {
        Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,&smbServer.password);
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SMB_AUTHENTICATION)
    {
      // initialize interactive/default password
      retries = 0;
      while ((Error_getCode(error) == ERROR_CODE_SMB_AUTHENTICATION) && (retries < MAX_PASSWORD_REQUESTS))
      {
        if (initSMBLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                         storageDirectoryListHandle->storageSpecifier.loginName,
                         storageDirectoryListHandle->storageSpecifier.loginPassword,
                         jobOptions,
// TODO:
CALLBACK_(NULL,NULL)//                         CALLBACK_(storageDirectoryListHandle->getNamePasswordFunction,storageDirectoryListHandle->getNamePasswordUserData)
                        )
           )
        {
          error = checkSMBLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                                storageDirectoryListHandle->storageSpecifier.loginName,
                                storageDirectoryListHandle->storageSpecifier.loginPassword,
                                share
                               );
        }
        retries++;
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SMB_AUTHENTICATION)
    {
      error = (   !Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword)
               || !Password_isEmpty(&smbServer.password)
               || !Password_isEmpty(&defaultSMBPassword)
              )
                ? ERRORX_(INVALID_SMB_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName))
                : ERRORX_(NO_SMB_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName));
    }
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // store password as default SMB/CIFS password
    Password_set(&defaultSMBPassword,storageDirectoryListHandle->storageSpecifier.loginPassword);

    // connect share
    error = smb2ConnectShare(&storageDirectoryListHandle->smb.context,
                             storageDirectoryListHandle->storageSpecifier.hostName,
                             storageDirectoryListHandle->storageSpecifier.loginName,
                             storageDirectoryListHandle->storageSpecifier.loginPassword,
                             share
                            );
    if (error == ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->smb.context,{ smb2DisconnectShare(storageDirectoryListHandle->smb.context); });

    // open directory for reading
	  storageDirectoryListHandle->smb.directory = smb2_opendir(storageDirectoryListHandle->smb.context,
                                                             String_cString(path)
                                                            );
    if (storageDirectoryListHandle->smb.directory == NULL)
    {
      error = ERRORX_(SMB,0,"%s",smb2_get_error(storageDirectoryListHandle->smb.context));
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    storageDirectoryListHandle->smb.directoryEntry = NULL;

    // free resources
    smb2DoneSharePath(share,path);
    Configuration_doneSMBServerSettings(&smbServer);
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(pathName);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_closeDirectoryList
* Purpose: close storage directory list
* Input  : storageDirectoryListHandle - storage directory list handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void StorageSMB_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SMB);

  #ifdef HAVE_SMB2
    smb2_closedir(storageDirectoryListHandle->smb.context, storageDirectoryListHandle->smb.directory);
    (void)smb2_disconnect_share(storageDirectoryListHandle->smb.context);
    smb2_destroy_context(storageDirectoryListHandle->smb.context);
    freeServer(storageDirectoryListHandle->smb.serverId);
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* HAVE_SMB2 */
}

/***********************************************************************\
* Name   : StorageSMB_endOfDirectoryList
* Purpose: check if end of storage directory list reached
* Input  : storageDirectoryListHandle - storage directory list handle
* Output : -
* Return : TRUE if not more diretory entries to read, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSMB_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  bool endOfDirectoryFlag;

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SMB);

  endOfDirectoryFlag = TRUE;
  #ifdef HAVE_SMB2
    {
      // read entry iff not already read
      if (storageDirectoryListHandle->smb.directoryEntry == NULL)
      {
        storageDirectoryListHandle->smb.directoryEntry = smb2_readdir(storageDirectoryListHandle->smb.context,
                                                                      storageDirectoryListHandle->smb.directory
                                                                     );
      }

      endOfDirectoryFlag = storageDirectoryListHandle->smb.directoryEntry == NULL;
    }
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* HAVE_SMB2 */

  return endOfDirectoryFlag;
}

/***********************************************************************\
* Name   : StorageSMB_readDirectoryList
* Purpose: read next storage directory list entry in storage
* Input  : storageDirectoryListHandle - storage directory list handle
*          fileName                   - file name variable
*          fileInfo                   - file info (can be NULL)
* Output : fileName - next file name (including path)
*          fileInfo - next file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSMB_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                          String                     fileName,
                                          FileInfo                   *fileInfo
                                         )
{
  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SMB);

  error = ERROR_NONE;
  #ifdef HAVE_SMB2
    {
      // read entry iff not already read
      if (storageDirectoryListHandle->smb.directoryEntry == NULL)
      {
        storageDirectoryListHandle->smb.directoryEntry = smb2_readdir(storageDirectoryListHandle->smb.context,
                                                                      storageDirectoryListHandle->smb.directory
                                                                     );
      }

      if (storageDirectoryListHandle->smb.directoryEntry != NULL)
      {
        String_setCString(fileName,storageDirectoryListHandle->smb.directoryEntry->name);

        if (fileInfo != NULL)
        {
          switch (storageDirectoryListHandle->smb.directoryEntry->st.smb2_type)
          {
            case SMB2_TYPE_FILE:      fileInfo->type = FILE_TYPE_FILE;      break;
            case SMB2_TYPE_DIRECTORY: fileInfo->type = FILE_TYPE_DIRECTORY; break;
            case SMB2_TYPE_LINK:      fileInfo->type = FILE_TYPE_LINK;      break;
            default:                  fileInfo->type = FILE_TYPE_UNKNOWN;   break;
          }
          fileInfo->size            = storageDirectoryListHandle->smb.directoryEntry->st.smb2_size;
          fileInfo->timeLastAccess  = storageDirectoryListHandle->smb.directoryEntry->st.smb2_atime;
          fileInfo->timeModified    = storageDirectoryListHandle->smb.directoryEntry->st.smb2_mtime;
          fileInfo->timeLastChanged = 0LL;
          fileInfo->userId          = 0;
          fileInfo->groupId         = 0;
          fileInfo->permissions     = 0;
          fileInfo->major           = 0;
          fileInfo->minor           = 0;
          memClear(&fileInfo->cast,sizeof(FileCast));

          storageDirectoryListHandle->smb.directoryEntry = NULL;
        }
      }
      else
      {
        error = ERROR_NONE;
      }
    }
  #else /* not HAVE_SMB2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileInfo);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SMB2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
