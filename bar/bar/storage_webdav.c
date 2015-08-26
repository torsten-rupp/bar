/***********************************************************************\
*
* $Revision: 4012 $
* $Date: 2015-04-28 19:02:40 +0200 (Tue, 28 Apr 2015) $
* $Author: torsten $
* Contents: storage WebDAV functions
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
#ifdef HAVE_CURL
  #include <curl/curl.h>
#endif /* HAVE_CURL */
#ifdef HAVE_MXML
  #include <mxml.h>
#endif /* HAVE_MXML */
#include <signal.h>
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
// file data buffer size
#define BUFFER_SIZE (64*1024)

// different timeouts [ms]
#define WEBDAV_TIMEOUT (30*1000)
#define READ_TIMEOUT   (60*1000)

#define INITIAL_BUFFER_SIZE   (64*1024)
#define INCREMENT_BUFFER_SIZE ( 8*1024)
#define MAX_BUFFER_SIZE       (64*1024)
#define MAX_FILENAME_LENGTH   ( 8*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#ifdef HAVE_CURL
  LOCAL Password *defaultWebDAVPassword;
#endif /* HAVE_CURL */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifdef HAVE_CURL
/***********************************************************************\
* Name   : initDefaultWebDAVPassword
* Purpose: init default WebDAV password
* Input  : hostName   - host name
*          loginName  - login name
*          jobOptions - job options
* Output : -
* Return : TRUE if WebDAV password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initDefaultWebDAVPassword(ConstString hostName, ConstString loginName, const JobOptions *jobOptions)
{
  SemaphoreLock semaphoreLock;
  String        s;
  bool          initFlag;

  initFlag = FALSE;

  if (jobOptions != NULL)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
    {
      if (jobOptions->webDAVServer.password == NULL)
      {
        if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
        {
          if (defaultWebDAVPassword == NULL)
          {
            Password *password = Password_new();
            s = !String_isEmpty(loginName)
                  ? String_format(String_new(),"WebDAV login password for %S@%S",loginName,hostName)
                  : String_format(String_new(),"WebDAV login password for %S",hostName);
            if (Password_input(password,String_cString(s),PASSWORD_INPUT_MODE_ANY))
            {
              defaultWebDAVPassword = password;
              initFlag = TRUE;
            }
            else
            {
              Password_delete(password);
            }
            String_delete(s);
          }
          else
          {
            initFlag = TRUE;
          }
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

/***********************************************************************\
* Name   : initWebDAVHandle
* Purpose: init WebDAV handle
* Input  : curlHandle    - CURL handle
*          url           - URL
*          loginName     - login name
*          loginPassword - login password
*          timeout       - timeout [ms]
* Output : -
* Return : CURLE_OK if no error, CURL error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL CURLcode initWebDAVHandle(CURL *curlHandle, ConstString url, ConstString loginName, Password *loginPassword, long timeout)
{
  CURLcode    curlCode;
  const char *plainLoginPassword;

  // reset
  curl_easy_reset(curlHandle);

  curlCode = CURLE_OK;

  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_FAILONERROR,1L);
  }
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_CONNECTTIMEOUT_MS,timeout);
  }
  if (globalOptions.verboseLevel >= 6)
  {
    // enable debug mode
    (void)curl_easy_setopt(curlHandle,CURLOPT_VERBOSE,1L);
  }

  /* Note: curl trigger from time to time a SIGALRM. The curl option
           CURLOPT_NOSIGNAL should stop this. But it seems there is
           a bug in curl which cause random crashes when
           CURLOPT_NOSIGNAL is enabled. Thus: do not use it!
           Instead install a signal handler to catch the not wanted
           signal.
  (void)curl_easy_setopt(storageHandle->webdav.curlMultiHandle,CURLOPT_NOSIGNAL,1L);
  (void)curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_NOSIGNAL,1L);
  */

  // set URL
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
  }

  // set login
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_USERNAME,String_cString(loginName));
  }
  if (curlCode == CURLE_OK)
  {
    plainLoginPassword = Password_deploy(loginPassword);
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_PASSWORD,plainLoginPassword);
    Password_undeploy(loginPassword);
  }

  // set nop-handlers
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_HEADERFUNCTION,curlNopDataCallback);
  }
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_HEADERDATA,0L);
  }
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_WRITEFUNCTION,curlNopDataCallback);
  }
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_WRITEDATA,0L);
  }

  return curlCode;
}

/***********************************************************************\
* Name   : checkWebDAVLogin
* Purpose: check if WebDAV login is possible
* Input  : hostName      - host name
*          loginName     - login name
*          loginPassword - login password
* Output : -
* Return : ERROR_NONE if login is possible, error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors checkWebDAVLogin(ConstString hostName,
                              ConstString loginName,
                              Password    *loginPassword
                             )
{
  CURL     *curlHandle;
  String   url;
  CURLcode curlCode;

  // init handle
  curlHandle = curl_easy_init();
  if (curlHandle == NULL)
  {
    return ERROR_WEBDAV_SESSION_FAIL;
  }

  // set connect
  url = String_format(String_new(),"http://%S",hostName);
  curlCode = initWebDAVHandle(curlHandle,url,loginName,loginPassword,WEBDAV_TIMEOUT);
  String_delete(url);
  if (curlCode != CURLE_OK)
  {
    (void)curl_easy_cleanup(curlHandle);
    return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_easy_strerror(curlCode));
  }

  // login
  curlCode = curl_easy_perform(curlHandle);
  if (curlCode != CURLE_OK)
  {
    (void)curl_easy_cleanup(curlHandle);
    return ERRORX_(WEBDAV_AUTHENTICATION,0,curl_easy_strerror(curlCode));
  }

  // free resources
  (void)curl_easy_cleanup(curlHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : curlWebDAVReadDataCallback
* Purpose: curl WebDAV read data callback: read data from buffer and
*          send to remote
* Input  : buffer   - buffer for data
*          size     - size of an element
*          n        - number of elements
*          userData - user data
* Output : -
* Return : number of read bytes or 0
* Notes  : -
\***********************************************************************/

LOCAL size_t curlWebDAVReadDataCallback(void   *buffer,
                                        size_t size,
                                        size_t n,
                                        void   *userData
                                       )
{
  StorageHandle *storageHandle = (StorageHandle*)userData;
  size_t        bytesSent;

  assert(buffer != NULL);
  assert(size > 0);
  assert(storageHandle != NULL);
  assert(storageHandle->webdav.sendBuffer.data != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  if (storageHandle->webdav.sendBuffer.index < storageHandle->webdav.sendBuffer.length)
  {
    bytesSent = MIN(n,(size_t)(storageHandle->webdav.sendBuffer.length-storageHandle->webdav.sendBuffer.index)/size)*size;

    memcpy(buffer,storageHandle->webdav.sendBuffer.data+storageHandle->webdav.sendBuffer.index,bytesSent);
    storageHandle->webdav.sendBuffer.index += (ulong)bytesSent;
  }
  else
  {
    bytesSent = 0;
  }
//fprintf(stderr,"%s, %d: bytesSent=%d\n",__FILE__,__LINE__,bytesSent);

  return bytesSent;
}

/***********************************************************************\
* Name   : curlWebDAVWriteDataCallback
* Purpose: curl WebDAV write data callback: receive data from remote
*          and store into buffer
* Input  : buffer   - buffer with data
*          size     - size of an element
*          n        - number of elements
*          userData - user data
* Output : -
* Return : number of stored bytes or 0
* Notes  : -
\***********************************************************************/

LOCAL size_t curlWebDAVWriteDataCallback(const void *buffer,
                                         size_t     size,
                                         size_t     n,
                                         void       *userData
                                        )
{
  StorageHandle *storageHandle = (StorageHandle*)userData;
  ulong         bytesReceived;
  ulong         newSize;
  byte          *newData;

  assert(buffer != NULL);
  assert(size > 0);
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);

  // calculate number of received bytes
  bytesReceived = n*size;

  // increase buffer size if required
  if ((storageHandle->webdav.receiveBuffer.length+bytesReceived) > storageHandle->webdav.receiveBuffer.size)
  {
    newSize = ((storageHandle->webdav.receiveBuffer.length+bytesReceived+INCREMENT_BUFFER_SIZE-1)/INCREMENT_BUFFER_SIZE)*INCREMENT_BUFFER_SIZE;
    newData = (byte*)realloc(storageHandle->webdav.receiveBuffer.data,newSize);
    if (newData == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    storageHandle->webdav.receiveBuffer.data = newData;
    storageHandle->webdav.receiveBuffer.size = newSize;
  }

  // append data to buffer
  memcpy(storageHandle->webdav.receiveBuffer.data+storageHandle->webdav.receiveBuffer.length,buffer,bytesReceived);
  storageHandle->webdav.receiveBuffer.length += bytesReceived;
//static size_t totalReceived = 0;
//totalReceived+=bytesReceived;
//fprintf(stderr,"%s, %d: storageHandle->webdav.receiveBuffer.length=%d bytesReceived=%d %d\n",__FILE__,__LINE__,storageHandle->webdav.receiveBuffer.length,bytesReceived,totalReceived);

  return bytesReceived;
}

/***********************************************************************\
* Name   : curlWebDAVReadDirectoryDataCallback
* Purpose: curl WebDAV parse directory list callback
* Input  : buffer   - buffer with data: receive data from remote
*          size     - size of an element
*          n        - number of elements
*          userData - user data
* Output : -
* Return : number of processed bytes or 0
* Notes  : -
\***********************************************************************/

LOCAL size_t curlWebDAVReadDirectoryDataCallback(const void *buffer,
                                                 size_t     size,
                                                 size_t     n,
                                                 void       *userData
                                                )
{
  String directoryData = (String)userData;

  assert(buffer != NULL);
  assert(size > 0);
  assert(directoryData != NULL);

  String_appendBuffer(directoryData,buffer,size*n);

  return size*n;
}
#endif /* HAVE_CURL */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageWebDAV_initAll(void)
{
  Errors error;

  error = ERROR_NONE;

  #ifdef HAVE_CURL
    defaultWebDAVPassword = NULL;
  #endif /* HAVE_CURL */

  return error;
}

LOCAL void StorageWebDAV_doneAll(void)
{
  #ifdef HAVE_CURL
    Password_delete(defaultWebDAVPassword);
  #endif /* HAVE_CURL */
}

LOCAL bool StorageWebDAV_parseSpecifier(ConstString webdavSpecifier,
                                        String      hostName,
                                        String      loginName,
                                        Password    *loginPassword
                                       )
{
  const char* LOGINNAME_MAP_FROM[] = {"\\@"};
  const char* LOGINNAME_MAP_TO[]   = {"@"};

  bool   result;
  String s;

  assert(webdavSpecifier != NULL);
  assert(hostName != NULL);
  assert(loginName != NULL);

  String_clear(hostName);
  String_clear(loginName);
  if (loginPassword != NULL) Password_clear(loginPassword);

  s = String_new();
  if      (String_matchCString(webdavSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)$",NULL,NULL,loginName,s,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>:<login password>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (loginPassword != NULL) Password_setString(loginPassword,s);

    result = TRUE;
  }
  else if (String_matchCString(webdavSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@/]*?)$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));

    result = TRUE;
  }
  else if (!String_isEmpty(webdavSpecifier))
  {
    // <host name>
    String_set(hostName,webdavSpecifier);

    result = TRUE;
  }
  else
  {
    result = FALSE;
  }
  String_delete(s);

  return result;
}

LOCAL bool StorageWebDAV_equalNames(const StorageSpecifier *storageSpecifier1,
                                    const StorageSpecifier *storageSpecifier2
                                   )
{
  assert(storageSpecifier1 != NULL);
  assert(storageSpecifier1->type == STORAGE_TYPE_WEBDAV);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_WEBDAV);

  return    String_equals(storageSpecifier1->hostName,storageSpecifier2->hostName)
         && String_equals(storageSpecifier1->loginName,storageSpecifier2->loginName)
         && String_equals(storageSpecifier1->archiveName,storageSpecifier2->archiveName);
}

LOCAL String StorageWebDAV_getName(StorageSpecifier *storageSpecifier,
                                   ConstString      archiveName
                                  )
{
  ConstString storageFileName;
  const char  *plainLoginPassword;

  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_WEBDAV);

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

  String_appendCString(storageSpecifier->storageName,"webdav://");
  if (!String_isEmpty(storageSpecifier->loginName))
  {
    String_append(storageSpecifier->storageName,storageSpecifier->loginName);
    if (!Password_isEmpty(storageSpecifier->loginPassword))
    {
      String_appendChar(storageSpecifier->storageName,':');
      plainLoginPassword = Password_deploy(storageSpecifier->loginPassword);
      String_appendCString(storageSpecifier->storageName,plainLoginPassword);
      Password_undeploy(storageSpecifier->loginPassword);
    }
    String_appendChar(storageSpecifier->storageName,'@');
  }
  String_append(storageSpecifier->storageName,storageSpecifier->hostName);
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(storageSpecifier->storageName,'/');
    String_append(storageSpecifier->storageName,storageFileName);
  }

  return storageSpecifier->storageName;
}

LOCAL ConstString StorageWebDAV_getPrintableName(StorageSpecifier *storageSpecifier,
                                                 ConstString      archiveName
                                                )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_WEBDAV);

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

      String_appendCString(storageSpecifier->storageName,"webdav://");
      if (!String_isEmpty(storageSpecifier->loginName))
      {
        String_append(storageSpecifier->storageName,storageSpecifier->loginName);
        String_appendChar(storageSpecifier->storageName,'@');
      }
      String_append(storageSpecifier->storageName,storageSpecifier->hostName);
      if (!String_isEmpty(storageFileName))
      {
        String_appendChar(storageSpecifier->storageName,'/');
        String_append(storageSpecifier->storageName,storageFileName);
      }

  return storageSpecifier->storageName;
}

LOCAL Errors StorageWebDAV_init(StorageHandle              *storageHandle,
                                const StorageSpecifier     *storageSpecifier,
                                const JobOptions           *jobOptions,
                                BandWidthList              *maxBandWidthList,
                                ServerConnectionPriorities serverConnectionPriority
                               )
{
  #ifdef HAVE_CURL
    AutoFreeList autoFreeList;
    Errors       error;
    WebDAVServer webDAVServer;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_WEBDAV);

  #ifdef HAVE_CURL
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif /* HAVE_CURL */

#warning TODO remove
  UNUSED_VARIABLE(storageSpecifier);

  #ifdef HAVE_CURL
    // init variables
    AutoFree_init(&autoFreeList);
    storageHandle->webdav.curlMultiHandle      = NULL;
    storageHandle->webdav.curlHandle           = NULL;
    storageHandle->webdav.index                = 0LL;
    storageHandle->webdav.size                 = 0LL;
    storageHandle->webdav.receiveBuffer.data   = NULL;
    storageHandle->webdav.receiveBuffer.size   = 0L;
    storageHandle->webdav.receiveBuffer.offset = 0LL;
    storageHandle->webdav.receiveBuffer.length = 0L;
    storageHandle->webdav.sendBuffer.data      = NULL;
    storageHandle->webdav.sendBuffer.index     = 0L;
    storageHandle->webdav.sendBuffer.length    = 0L;
    initBandWidthLimiter(&storageHandle->webdav.bandWidthLimiter,maxBandWidthList);

    // get WebDAV server settings
    storageHandle->webdav.server = getWebDAVServerSettings(storageHandle->storageSpecifier.hostName,jobOptions,&webDAVServer);
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_set(storageHandle->storageSpecifier.loginName,webDAVServer.loginName);
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_setCString(storageHandle->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_setCString(storageHandle->storageSpecifier.loginName,getenv("USER"));
    if (String_isEmpty(storageHandle->storageSpecifier.hostName))
    {
      doneBandWidthLimiter(&storageHandle->webdav.bandWidthLimiter);
      return ERROR_NO_HOST_NAME;
    }

    // allocate WebDAV server
    if (!allocateServer(storageHandle->webdav.server,serverConnectionPriority,60*1000L))
    {
      doneBandWidthLimiter(&storageHandle->webdav.bandWidthLimiter);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,storageHandle->webdav.server,{ freeServer(storageHandle->webdav.server); });

    // check WebDAV login, get correct password
    error = ERROR_WEBDAV_SESSION_FAIL;
    if ((error != ERROR_NONE) && !Password_isEmpty(storageHandle->storageSpecifier.loginPassword))
    {
      error = checkWebDAVLogin(storageHandle->storageSpecifier.hostName,
                               storageHandle->storageSpecifier.loginName,
                               storageHandle->storageSpecifier.loginPassword
                              );
    }
    if ((error != ERROR_NONE) && !Password_isEmpty(webDAVServer.password))
    {
      error = checkWebDAVLogin(storageHandle->storageSpecifier.hostName,
                               storageHandle->storageSpecifier.loginName,
                               webDAVServer.password
                              );
      if (error == ERROR_NONE)
      {
        Password_set(storageHandle->storageSpecifier.loginPassword,webDAVServer.password);
      }
    }
    if ((error != ERROR_NONE) && !Password_isEmpty(defaultWebDAVPassword))
    {
      error = checkWebDAVLogin(storageHandle->storageSpecifier.hostName,
                               storageHandle->storageSpecifier.loginName,
                               defaultWebDAVPassword
                              );
      if (error == ERROR_NONE)
      {
        Password_set(storageHandle->storageSpecifier.loginPassword,defaultWebDAVPassword);
      }
    }
    if (error != ERROR_NONE)
    {
      // initialize default password
      if (initDefaultWebDAVPassword(storageHandle->storageSpecifier.hostName,storageHandle->storageSpecifier.loginName,jobOptions))
      {
        error = checkWebDAVLogin(storageHandle->storageSpecifier.hostName,
                                 storageHandle->storageSpecifier.loginName,
                                 defaultWebDAVPassword
                                );
        if (error == ERROR_NONE)
        {
          Password_set(storageHandle->storageSpecifier.loginPassword,defaultWebDAVPassword);
        }
      }
      else
      {
        error = !Password_isEmpty(defaultWebDAVPassword) ? ERROR_INVALID_WEBDAV_PASSWORD : ERROR_NO_WEBDAV_PASSWORD;
      }
    }
    if (error != ERROR_NONE)
    {
      doneBandWidthLimiter(&storageHandle->webdav.bandWidthLimiter);
      return error;
    }

    // free resources
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(maxBandWidthList);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
}

LOCAL Errors StorageWebDAV_done(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  // free WebDAV server connection
  #ifdef HAVE_CURL
    freeServer(storageHandle->webdav.server);
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_CURL || HAVE_FTP */

  return ERROR_NONE;
}

LOCAL bool StorageWebDAV_isServerAllocationPending(StorageHandle *storageHandle)
{
  bool serverAllocationPending;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  serverAllocationPending = FALSE;
  #ifdef HAVE_CURL
    serverAllocationPending = isServerAllocationPending(storageHandle->webdav.server);
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);

    serverAllocationPending = FALSE;
  #endif /* HAVE_CURL */

  return serverAllocationPending;
}

LOCAL Errors StorageWebDAV_preProcess(StorageHandle *storageHandle,
                                      bool          initialFlag
                                     )
{
  Errors error;
  #ifdef HAVE_CURL
    TextMacro textMacros[1];
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  error = ERROR_NONE;
  #ifdef HAVE_CURL
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      if (!initialFlag)
      {
        // init macros
        TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageHandle->volumeNumber              );

        if (globalOptions.ftp.writePreProcessCommand != NULL)
        {
          // write pre-processing
          if (error == ERROR_NONE)
          {
            printInfo(0,"Write pre-processing...");
            error = Misc_executeCommand(String_cString(globalOptions.webdav.writePreProcessCommand),
                                        textMacros,
                                        SIZE_OF_ARRAY(textMacros),
                                        CALLBACK(executeIOOutput,NULL),
                                        CALLBACK(executeIOOutput,NULL)
                                       );
            printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
          }
        }
      }
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(initialFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */

  return error;
}

LOCAL Errors StorageWebDAV_postProcess(StorageHandle *storageHandle,
                                       bool          finalFlag
                                      )
{
  Errors error;
  #ifdef HAVE_CURL
    TextMacro textMacros[1];
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  error = ERROR_NONE;
  #ifdef HAVE_CURL
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      if (!finalFlag)
      {
        // init macros
        TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageHandle->volumeNumber);

        if (globalOptions.ftp.writePostProcessCommand != NULL)
        {
          // write post-process
          if (error == ERROR_NONE)
          {
            printInfo(0,"Write post-processing...");
            error = Misc_executeCommand(String_cString(globalOptions.webdav.writePostProcessCommand),
                                        textMacros,
                                        SIZE_OF_ARRAY(textMacros),
                                        CALLBACK(executeIOOutput,NULL),
                                        CALLBACK(executeIOOutput,NULL)
                                       );
            printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
          }
        }
      }
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(finalFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */

  return error;
}

LOCAL Errors StorageWebDAV_create(StorageHandle *storageHandle,
                                  ConstString   archiveName,
                                  uint64        archiveSize
                                 )
{
  #ifdef HAVE_CURL
    String          baseURL;
    CURLcode        curlCode;
    CURLMcode       curlMCode;
    String          url;
    String          pathName,baseName;
    StringTokenizer nameTokenizer;
    ConstString     token;
    int             runningHandles;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(!String_isEmpty(storageHandle->storageSpecifier.archiveName));
  assert(archiveName != NULL);

  #ifdef HAVE_CURL
    // initialize variables
    storageHandle->webdav.index             = 0LL;
    storageHandle->webdav.size              = archiveSize;
    storageHandle->webdav.sendBuffer.index  = 0L;
    storageHandle->webdav.sendBuffer.length = 0L;

    // open Curl handles
    storageHandle->webdav.curlMultiHandle = curl_multi_init();
    if (storageHandle->webdav.curlMultiHandle == NULL)
    {
      return ERROR_WEBDAV_SESSION_FAIL;
    }
    storageHandle->webdav.curlHandle = curl_easy_init();
    if (storageHandle->webdav.curlHandle == NULL)
    {
      curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      return ERROR_WEBDAV_SESSION_FAIL;
    }

    // get base URL
    baseURL = String_format(String_new(),"http://%S",storageHandle->storageSpecifier.hostName);
    if (storageHandle->storageSpecifier.hostPort != 0) String_format(baseURL,":d",storageHandle->storageSpecifier.hostPort);

    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      // get pathname, basename
      pathName = File_getFilePathName(String_new(),archiveName);
      baseName = File_getFileBaseName(String_new(),archiveName);

      // create directories if necessary
      if (!String_isEmpty(pathName))
      {
        curlCode = CURLE_OK;
        url = String_format(String_duplicate(baseURL),"/");
        File_initSplitFileName(&nameTokenizer,pathName);
        while (File_getNextSplitFileName(&nameTokenizer,&token))
        {
          String_append(url,token);
          String_appendChar(url,'/');

          // check if sub-directory exists
          curlCode = initWebDAVHandle(storageHandle->webdav.curlHandle,
                                      url,
                                      storageHandle->storageSpecifier.loginName,
                                      storageHandle->storageSpecifier.loginPassword,
                                      WEBDAV_TIMEOUT
                                    );
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_NOBODY,1L);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_perform(storageHandle->webdav.curlHandle);
          }
          if (curlCode != CURLE_OK)
          {
            // create sub-directory
            curlCode = initWebDAVHandle(storageHandle->webdav.curlHandle,
                                        url,
                                        storageHandle->storageSpecifier.loginName,
                                        storageHandle->storageSpecifier.loginPassword,
                                        WEBDAV_TIMEOUT
                                      );
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_NOBODY,1L);
            }
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_CUSTOMREQUEST,"MKCOL");
            }
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_perform(storageHandle->webdav.curlHandle);
            }
          }
        }
        File_doneSplitFileName(&nameTokenizer);
        if (curlCode != CURLE_OK)
        {
          String_delete(url);
          String_delete(baseName);
          String_delete(pathName);
          String_delete(baseURL);
          (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
          (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
          return ERRORX_(CREATE_DIRECTORY,0,curl_easy_strerror(curlCode));
        }
        String_delete(url);
      }

      // first delete file if overwrite requested
      if ((storageHandle->jobOptions != NULL) && storageHandle->jobOptions->overwriteArchiveFilesFlag)
      {
        // get URL
        url = String_format(String_duplicate(baseURL),"/");
        File_initSplitFileName(&nameTokenizer,pathName);
        while (File_getNextSplitFileName(&nameTokenizer,&token))
        {
          String_append(url,token);
          String_appendChar(url,'/');
        }
        File_doneSplitFileName(&nameTokenizer);
        String_append(url,baseName);

        // check if file exists
        curlCode = initWebDAVHandle(storageHandle->webdav.curlHandle,
                                    url,
                                    storageHandle->storageSpecifier.loginName,
                                    storageHandle->storageSpecifier.loginPassword,
                                    WEBDAV_TIMEOUT
                                   );
        if (curlCode == CURLE_OK)
        {
          curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_NOBODY,1L);
        }
        if (curlCode == CURLE_OK)
        {
          curlCode = curl_easy_perform(storageHandle->webdav.curlHandle);
        }
        if (curlCode == CURLE_OK)
        {
          // delete file
          curlCode = initWebDAVHandle(storageHandle->webdav.curlHandle,
                                      url,
                                      storageHandle->storageSpecifier.loginName,
                                      storageHandle->storageSpecifier.loginPassword,
                                      WEBDAV_TIMEOUT
                                    );
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_NOBODY,1L);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_CUSTOMREQUEST,"DELETE");
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_perform(storageHandle->webdav.curlHandle);
          }
          if (curlCode != CURLE_OK)
          {
            String_delete(url);
            String_delete(baseName);
            String_delete(pathName);
            String_delete(baseURL);
            (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
            (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
            return ERRORX_(DELETE_FILE,0,curl_easy_strerror(curlCode));
          }
        }

        // free resources
        String_delete(url);
      }

      // init WebDAV upload
      url = String_format(String_duplicate(baseURL),"/");
      File_initSplitFileName(&nameTokenizer,pathName);
      while (File_getNextSplitFileName(&nameTokenizer,&token))
      {
        String_append(url,token);
        String_appendChar(url,'/');
      }
      File_doneSplitFileName(&nameTokenizer);
      String_append(url,baseName);

      curlCode = initWebDAVHandle(storageHandle->webdav.curlHandle,
                                  url,
                                  storageHandle->storageSpecifier.loginName,
                                  storageHandle->storageSpecifier.loginPassword,
                                  WEBDAV_TIMEOUT
                                 );
      if (curlCode == CURLE_OK)
      {
        curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_NOBODY,0L);
      }
      if (curlCode == CURLE_OK)
      {
        curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_CUSTOMREQUEST,"PUT");
      }
      if (curlCode == CURLE_OK)
      {
        curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_UPLOAD,1L);
      }
      if (curlCode == CURLE_OK)
      {
        curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_READFUNCTION,curlWebDAVReadDataCallback);
      }
      if (curlCode == CURLE_OK)
      {
        curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_READDATA,storageHandle);
      }
      if (curlCode == CURLE_OK)
      {
        curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_INFILESIZE_LARGE,(curl_off_t)storageHandle->webdav.size);
      }
      if (curlCode != CURLE_OK)
      {
        String_delete(url);
        String_delete(baseName);
        String_delete(pathName);
        String_delete(baseURL);
        (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
        (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
        return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_easy_strerror(curlCode));
      }
      curlMCode = curl_multi_add_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
      if (curlMCode != CURLM_OK)
      {
        String_delete(url);
        String_delete(baseName);
        String_delete(pathName);
        String_delete(baseURL);
        (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
        (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
        return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
      }
      String_delete(url);

      // start WebDAV upload
      do
      {
        curlMCode = curl_multi_perform(storageHandle->webdav.curlMultiHandle,&runningHandles);
      }
      while (   (curlMCode == CURLM_CALL_MULTI_PERFORM)
             && (runningHandles > 0)
            );
      if (curlMCode != CURLM_OK)
      {
        String_delete(baseName);
        String_delete(pathName);
        String_delete(baseURL);
        (void)curl_multi_remove_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
        (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
        (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
        return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
      }

      // free resources
      String_delete(baseName);
      String_delete(pathName);
    }

    // free resources
    String_delete(baseURL);

    DEBUG_ADD_RESOURCE_TRACE("storage create curl",&storageHandle->webdav);

    return ERROR_NONE;
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(archiveSize);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
}

LOCAL Errors StorageWebDAV_open(StorageHandle *storageHandle, ConstString archiveName)
{
  #ifdef HAVE_CURL
    String          baseURL;
    CURLcode        curlCode;
    CURLMcode       curlMCode;
    String          url;
    String          pathName,baseName;
    StringTokenizer nameTokenizer;
    ConstString     token;
    double          fileSize;
    int             runningHandles;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  #ifdef HAVE_CURL
    // initialize variables
    storageHandle->webdav.index                = 0LL;
    storageHandle->webdav.receiveBuffer.size   = INITIAL_BUFFER_SIZE;
    storageHandle->webdav.receiveBuffer.offset = 0LL;
    storageHandle->webdav.receiveBuffer.length = 0L;

    // allocate transfer buffer
    storageHandle->webdav.receiveBuffer.data = (byte*)malloc(storageHandle->webdav.receiveBuffer.size);
    if (storageHandle->webdav.receiveBuffer.data == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    // open Curl handles
    storageHandle->webdav.curlMultiHandle = curl_multi_init();
    if (storageHandle->webdav.curlMultiHandle == NULL)
    {
      free(storageHandle->webdav.receiveBuffer.data);
      return ERROR_WEBDAV_SESSION_FAIL;
    }
    storageHandle->webdav.curlHandle = curl_easy_init();
    if (storageHandle->webdav.curlHandle == NULL)
    {
      curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return ERROR_WEBDAV_SESSION_FAIL;
    }

    // get base URL
    baseURL = String_format(String_new(),"http://%S",storageHandle->storageSpecifier.hostName);
    if (storageHandle->storageSpecifier.hostPort != 0) String_format(baseURL,":d",storageHandle->storageSpecifier.hostPort);

    // get pathname, basename
    pathName = File_getFilePathName(String_new(),archiveName);
    baseName = File_getFileBaseName(String_new(),archiveName);

    // get url
    url = String_format(String_duplicate(baseURL),"/");
    File_initSplitFileName(&nameTokenizer,pathName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_append(url,token);
      String_appendChar(url,'/');
    }
    File_doneSplitFileName(&nameTokenizer);
    String_append(url,baseName);

    // check if file exists
    curlCode = initWebDAVHandle(storageHandle->webdav.curlHandle,
                                url,
                                storageHandle->storageSpecifier.loginName,
                                storageHandle->storageSpecifier.loginPassword,
                                WEBDAV_TIMEOUT
                              );
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_NOBODY,1L);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_perform(storageHandle->webdav.curlHandle);
    }
    if (curlCode != CURLE_OK)
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(pathName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return ERROR_FILE_NOT_FOUND_;
    }
    String_delete(url);

    // get file size
    curlCode = curl_easy_getinfo(storageHandle->webdav.curlHandle,CURLINFO_CONTENT_LENGTH_DOWNLOAD,&fileSize);
    if (   (curlCode != CURLE_OK)
        || (fileSize < 0.0)
       )
    {
      String_delete(baseName);
      String_delete(pathName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return ERROR_WEBDAV_GET_SIZE;
    }
    storageHandle->webdav.size = (uint64)fileSize;

    // init WebDAV download
    url = String_format(String_duplicate(baseURL),"/");
    File_initSplitFileName(&nameTokenizer,pathName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_append(url,token);
      String_appendChar(url,'/');
    }
    File_doneSplitFileName(&nameTokenizer);
    String_append(url,baseName);

    curlCode = initWebDAVHandle(storageHandle->webdav.curlHandle,
                                url,
                                storageHandle->storageSpecifier.loginName,
                                storageHandle->storageSpecifier.loginPassword,
                                WEBDAV_TIMEOUT
                              );
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_CUSTOMREQUEST,"GET");
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_NOBODY,0L);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_WRITEFUNCTION,curlWebDAVWriteDataCallback);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_WRITEDATA,storageHandle);
    }
    if (curlCode != CURLE_OK)
    {
      String_delete(baseName);
      String_delete(pathName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_easy_strerror(curlCode));
    }
    curlMCode = curl_multi_add_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
    if (curlMCode != CURLM_OK)
    {
      String_delete(baseName);
      String_delete(pathName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
    }
    String_delete(url);

    // start WebDAV download
    do
    {
      curlMCode = curl_multi_perform(storageHandle->webdav.curlMultiHandle,&runningHandles);
    }
    while (   (curlMCode == CURLM_CALL_MULTI_PERFORM)
           && (runningHandles > 0)
          );
    if (curlMCode != CURLM_OK)
    {
      String_delete(baseName);
      String_delete(pathName);
      String_delete(baseURL);
      (void)curl_multi_remove_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
    }

    // free resources
    String_delete(baseName);
    String_delete(pathName);
    String_delete(baseURL);

    DEBUG_ADD_RESOURCE_TRACE("storage open webdav",&storageHandle->webdav);

    return ERROR_NONE;
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
}

LOCAL void StorageWebDAV_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  #ifdef HAVE_CURL
    assert(storageHandle->webdav.curlHandle != NULL);
    assert(storageHandle->webdav.curlMultiHandle != NULL);

    DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->webdav);

    (void)curl_multi_remove_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
    (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
    (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
    if (storageHandle->webdav.receiveBuffer.data != NULL) free(storageHandle->webdav.receiveBuffer.data);
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_CURL */
}

LOCAL bool StorageWebDAV_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  #ifdef HAVE_CURL
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      return storageHandle->webdav.index >= storageHandle->webdav.size;
    }
    else
    {
      return TRUE;
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    return TRUE;
  #endif /* HAVE_CURL */
}

LOCAL Errors StorageWebDAV_read(StorageHandle *storageHandle,
                                void          *buffer,
                                ulong         size,
                                ulong         *bytesRead
                               )
{
  Errors error;
  #ifdef HAVE_CURL
    ulong     index;
    ulong     bytesAvail;
    ulong     length;
    uint64    startTimestamp,endTimestamp;
    CURLMcode curlmCode;
    int       runningHandles;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(buffer != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

//fprintf(stderr,"%s,%d: size=%lu\n",__FILE__,__LINE__,size);
  if (bytesRead != NULL) (*bytesRead) = 0L;
  error = ERROR_NONE;
  #ifdef HAVE_CURL
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      assert(storageHandle->webdav.curlMultiHandle != NULL);
      assert(storageHandle->webdav.receiveBuffer.data != NULL);

      while (   (size > 0L)
             && (storageHandle->webdav.index < storageHandle->webdav.size)
             && (error == ERROR_NONE)
            )
      {
        // copy as much data as available from receive buffer
        if (   (storageHandle->webdav.index >= storageHandle->webdav.receiveBuffer.offset)
            && (storageHandle->webdav.index < (storageHandle->webdav.receiveBuffer.offset+storageHandle->webdav.receiveBuffer.length))
           )
        {
          // copy data from receive buffer
          index      = (ulong)(storageHandle->webdav.index-storageHandle->webdav.receiveBuffer.offset);
          bytesAvail = MIN(size,storageHandle->webdav.receiveBuffer.length-index);
          memcpy(buffer,storageHandle->webdav.receiveBuffer.data+index,bytesAvail);

          // adjust buffer, size, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          size -= bytesAvail;
          if (bytesRead != NULL) (*bytesRead) += bytesAvail;
          storageHandle->webdav.index += (uint64)bytesAvail;
        }

        // read rest of data
        if (   (size > 0L)
            && (storageHandle->webdav.index < storageHandle->webdav.size)
           )
        {
          assert(storageHandle->webdav.index >= (storageHandle->webdav.receiveBuffer.offset+storageHandle->webdav.receiveBuffer.length));

          // get max. number of bytes to receive in one step
          if (storageHandle->webdav.bandWidthLimiter.maxBandWidthList != NULL)
          {
            length = MIN(storageHandle->webdav.bandWidthLimiter.blockSize,size);
          }
          else
          {
            length = size;
          }
          assert(length > 0L);

          // get start time
          startTimestamp = Misc_getTimestamp();

          // receive data
          storageHandle->webdav.receiveBuffer.length = 0L;
          runningHandles = 1;
          while (   (storageHandle->webdav.receiveBuffer.length < length)
                 && (runningHandles > 0)
                 && (error == ERROR_NONE)
                )
          {
            // wait for socket
            error = waitCurlSocket(storageHandle->webdav.curlMultiHandle);

            // perform curl action
            if (error == ERROR_NONE)
            {
              do
              {
                curlmCode = curl_multi_perform(storageHandle->webdav.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: curlmCode=%d %ld receive=%ld length=%ld runningHandles=%d\n",__FILE__,__LINE__,curlmCode,storageHandle->webdav.sendBuffer.index,storageHandle->webdav.receiveBuffer.length,length,runningHandles);
              }
              while (   (curlmCode == CURLM_CALL_MULTI_PERFORM)
                     && (runningHandles > 0)
                    );
              if (curlmCode != CURLM_OK)
              {
                error = ERRORX_(NETWORK_RECEIVE,0,curl_multi_strerror(curlmCode));
              }
            }
          }
          if      (error != ERROR_NONE)
          {
            break;
          }
//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__,length);
          else if ((storageHandle->webdav.receiveBuffer.length < length) && (runningHandles <= 0))
          {
            const CURLMsg *curlMsg;
            int           n,i;

            error = ERROR_NETWORK_RECEIVE;
            curlMsg = curl_multi_info_read(storageHandle->webdav.curlMultiHandle,&n);
            for (i = 0; i < n; i++)
            {
              if ((curlMsg[i].easy_handle == storageHandle->webdav.curlHandle) && (curlMsg[i].msg == CURLMSG_DONE))
              {
                error = ERRORX_(NETWORK_RECEIVE,0,curl_easy_strerror(curlMsg[i].data.result));
                break;
              }
              curlMsg++;
            }
            break;
          }
          storageHandle->webdav.receiveBuffer.offset = storageHandle->webdav.index;

          // copy data from receive buffer
          bytesAvail = MIN(length,storageHandle->webdav.receiveBuffer.length);
          memcpy(buffer,storageHandle->webdav.receiveBuffer.data,bytesAvail);

          // adjust buffer, size, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          size -= bytesAvail;
          if (bytesRead != NULL) (*bytesRead) += bytesAvail;
          storageHandle->webdav.index += (uint64)bytesAvail;

          // get end time
          endTimestamp = Misc_getTimestamp();

          /* limit used band width if requested (note: when the system time is
             changing endTimestamp may become smaller than startTimestamp;
             thus do not check this with an assert())
          */
          if (endTimestamp >= startTimestamp)
          {
            limitBandWidth(&storageHandle->webdav.bandWidthLimiter,
                           bytesAvail,
                           endTimestamp-startTimestamp
                          );
          }
        }
      }
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(size);
    UNUSED_VARIABLE(bytesRead);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageWebDAV_write(StorageHandle *storageHandle,
                                 const void    *buffer,
                                 ulong         size
                                )
{
  Errors error;
  #ifdef HAVE_CURL
    ulong     writtenBytes;
    ulong     length;
    uint64    startTimestamp,endTimestamp;
    CURLMcode curlmCode;
    int       runningHandles;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(buffer != NULL);

  error = ERROR_NONE;
  #ifdef HAVE_CURL
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      assert(storageHandle->webdav.curlMultiHandle != NULL);

      writtenBytes = 0L;
      while (writtenBytes < size)
      {
        // get max. number of bytes to send in one step
        if (storageHandle->webdav.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(storageHandle->webdav.bandWidthLimiter.blockSize,size-writtenBytes);
        }
        else
        {
          length = size-writtenBytes;
        }
        assert(length > 0L);

        // get start time
        startTimestamp = Misc_getTimestamp();

        // send data
        storageHandle->webdav.sendBuffer.data   = buffer;
        storageHandle->webdav.sendBuffer.index  = 0L;
        storageHandle->webdav.sendBuffer.length = length;
        runningHandles = 1;
        while (   (storageHandle->webdav.sendBuffer.index < storageHandle->webdav.sendBuffer.length)
               && (error == ERROR_NONE)
               && (runningHandles > 0)
              )
        {
          // wait for socket
          error = waitCurlSocket(storageHandle->webdav.curlMultiHandle);

          // perform curl action
          if (error == ERROR_NONE)
          {
            do
            {
              curlmCode = curl_multi_perform(storageHandle->webdav.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: curlmCode=%d %ld %ld runningHandles=%d\n",__FILE__,__LINE__,curlmCode,storageHandle->webdav.sendBuffer.index,storageHandle->webdav.sendBuffer.length,runningHandles);
            }
            while (   (curlmCode == CURLM_CALL_MULTI_PERFORM)
                   && (runningHandles > 0)
                  );
            if (curlmCode != CURLM_OK)
            {
              error = ERRORX_(NETWORK_SEND,0,curl_multi_strerror(curlmCode));
            }
          }
        }
        if (error != ERROR_NONE)
        {
          break;
        }
        if (storageHandle->webdav.sendBuffer.index < storageHandle->webdav.sendBuffer.length)
        {
          error = ERROR_NETWORK_SEND;
          break;
        }
        buffer = (byte*)buffer+storageHandle->webdav.sendBuffer.length;
        writtenBytes += storageHandle->webdav.sendBuffer.length;

        // get end time
        endTimestamp = Misc_getTimestamp();

        /* limit used band width if requested (note: when the system time is
           changing endTimestamp may become smaller than startTimestamp;
           thus do not check this with an assert())
        */
        if (endTimestamp >= startTimestamp)
        {
          limitBandWidth(&storageHandle->webdav.bandWidthLimiter,
                         storageHandle->webdav.sendBuffer.length,
                         endTimestamp-startTimestamp
                        );
        }
      }
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(size);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL uint64 StorageWebDAV_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  size = 0LL;
  #ifdef HAVE_CURL
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      size = storageHandle->webdav.size;
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);

    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_CURL */

  return size;
}

LOCAL Errors StorageWebDAV_tell(StorageHandle *storageHandle,
                                uint64        *offset
                               )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  #ifdef HAVE_CURL
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      (*offset) = storageHandle->webdav.index;
      error     = ERROR_NONE;
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageWebDAV_seek(StorageHandle *storageHandle,
                                uint64        offset
                               )
{
  Errors error;
  #ifdef HAVE_CURL
    uint64    skip;
    ulong     i;
    ulong     n;
    ulong     length;
    uint64    startTimestamp,endTimestamp;
    CURLMcode curlmCode;
    int       runningHandles;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  error = ERROR_NONE;
  #ifdef HAVE_CURL
    if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
    {
      assert(storageHandle->webdav.receiveBuffer.data != NULL);

      if      (offset > storageHandle->webdav.index)
      {
        // calculate number of bytes to skip
        skip = offset-storageHandle->webdav.index;

        while (skip > 0LL)
        {
          // skip data in receive buffer
          if (   (storageHandle->webdav.index >= storageHandle->webdav.receiveBuffer.offset)
              && (storageHandle->webdav.index < (storageHandle->webdav.receiveBuffer.offset+storageHandle->webdav.receiveBuffer.length))
             )
          {
            i = (ulong)storageHandle->webdav.index-storageHandle->webdav.receiveBuffer.offset;
            n = MIN(skip,storageHandle->webdav.receiveBuffer.length-i);

            skip -= (uint64)n;
            storageHandle->webdav.index += (uint64)n;
          }

          if (skip > 0LL)
          {
            assert(storageHandle->webdav.index >= (storageHandle->webdav.receiveBuffer.offset+storageHandle->webdav.receiveBuffer.length));

            // get max. number of bytes to receive in one step
            if (storageHandle->webdav.bandWidthLimiter.maxBandWidthList != NULL)
            {
              length = MIN(storageHandle->webdav.bandWidthLimiter.blockSize,MIN(skip,MAX_BUFFER_SIZE));
            }
            else
            {
              length = MIN(skip,MAX_BUFFER_SIZE);
            }
            assert(length > 0L);

            // get start time
            startTimestamp = Misc_getTimestamp();

            // receive data
            storageHandle->webdav.receiveBuffer.length = 0L;
            runningHandles = 1;
            while (   (storageHandle->webdav.receiveBuffer.length < length)
                   && (error == ERROR_NONE)
                   && (runningHandles > 0)
                  )
            {
              // wait for socket
              error = waitCurlSocket(storageHandle->webdav.curlMultiHandle);

              // perform curl action
              if (error == ERROR_NONE)
              {
                do
                {
                  curlmCode = curl_multi_perform(storageHandle->webdav.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: curlmCode=%d %ld %ld runningHandles=%d\n",__FILE__,__LINE__,curlmCode,storageHandle->webdav.receiveBuffer.length,length,runningHandles);
                }
                while (   (curlmCode == CURLM_CALL_MULTI_PERFORM)
                       && (runningHandles > 0)
                      );
                if (curlmCode != CURLM_OK)
                {
                  error = ERRORX_(NETWORK_RECEIVE,0,curl_multi_strerror(curlmCode));
                }
              }
            }
            if (error != ERROR_NONE)
            {
              break;
            }
            if (storageHandle->webdav.receiveBuffer.length < length)
            {
              error = ERROR_IO_ERROR;
              break;
            }
            storageHandle->webdav.receiveBuffer.offset = storageHandle->webdav.index;

            // get end time
            endTimestamp = Misc_getTimestamp();

            /* limit used band width if requested (note: when the system time is
               changing endTimestamp may become smaller than startTimestamp;
               thus do not check this with an assert())
            */
            if (endTimestamp >= startTimestamp)
            {
              limitBandWidth(&storageHandle->webdav.bandWidthLimiter,
                             length,
                             endTimestamp-startTimestamp
                            );
            }
          }
        }
      }
      else if (   (offset >= storageHandle->webdav.receiveBuffer.offset)
               && (offset < storageHandle->webdav.receiveBuffer.offset+(uint64)storageHandle->webdav.receiveBuffer.length)
              )
      {
        storageHandle->webdav.index = offset;
      }
      else
      {
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      }
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageWebDAV_delete(StorageHandle *storageHandle,
                                  ConstString   storageFileName
                                 )
{
  ConstString deleteFileName;
  Errors      error;
  #ifdef HAVE_CURL
    CURL            *curlHandle;
    String          baseURL;
    CURLcode        curlCode;
    String          pathName,baseName;
    String          url;
    StringTokenizer nameTokenizer;
    ConstString     token;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  deleteFileName = (storageFileName != NULL) ? storageFileName : storageHandle->storageSpecifier.archiveName;

  error = ERROR_UNKNOWN;
  #ifdef HAVE_CURL
    // initialize variables
    curlHandle = curl_easy_init();
    if (curlHandle != NULL)
    {
      // get base URL
      baseURL = String_format(String_new(),"http://%S",storageHandle->storageSpecifier.hostName);
      if (storageHandle->storageSpecifier.hostPort != 0) String_format(baseURL,":d",storageHandle->storageSpecifier.hostPort);

      // get pathname, basename
      pathName = File_getFilePathName(String_new(),deleteFileName);
      baseName = File_getFileBaseName(String_new(),deleteFileName);

      // get URL
      url = String_format(String_duplicate(baseURL),"/");
      File_initSplitFileName(&nameTokenizer,pathName);
      while (File_getNextSplitFileName(&nameTokenizer,&token))
      {
        String_append(url,token);
        String_appendChar(url,'/');
      }
      File_doneSplitFileName(&nameTokenizer);
      String_append(url,baseName);

      if ((storageHandle->jobOptions == NULL) || !storageHandle->jobOptions->dryRunFlag)
      {
        // delete file
        curlCode = initWebDAVHandle(curlHandle,
                                    url,
                                    storageHandle->storageSpecifier.loginName,
                                    storageHandle->storageSpecifier.loginPassword,
                                    WEBDAV_TIMEOUT
                                   );
        if (curlCode == CURLE_OK)
        {
          curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
        }
        if (curlCode == CURLE_OK)
        {
          curlCode = curl_easy_setopt(curlHandle,CURLOPT_CUSTOMREQUEST,"DELETE");
        }
        if (curlCode == CURLE_OK)
        {
          curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
        }
        if (curlCode == CURLE_OK)
        {
          curlCode = curl_easy_perform(curlHandle);
        }
        if (curlCode != CURLE_OK)
        {
          error = ERRORX_(DELETE_FILE,0,curl_easy_strerror(curlCode));
        }

        // check if file deleted
        if (curlCode == CURLE_OK)
        {
          curlCode = initWebDAVHandle(curlHandle,
                                      url,
                                      storageHandle->storageSpecifier.loginName,
                                      storageHandle->storageSpecifier.loginPassword,
                                      WEBDAV_TIMEOUT
                                     );
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(curlHandle,CURLOPT_CUSTOMREQUEST,"HEAD");
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_perform(curlHandle);
          }
          if (curlCode != CURLE_OK)
          {
            error = ERROR_NONE;
          }
          else
          {
            error = ERRORX_(DELETE_FILE,0,curl_easy_strerror(curlCode));
          }
        }
      }
      else
      {
        error = ERROR_NONE;
      }

      // free resources
      String_delete(url);
      String_delete(baseName);
      String_delete(pathName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(curlHandle);
    }
    else
    {
      error = ERROR_WEBDAV_SESSION_FAIL;
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(storageFileName);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
  assert(error != ERROR_UNKNOWN);

  return error;
}

#if 0
still not complete
LOCAL Errors StorageWebDAV_getFileInfo(StorageHandle *storageHandle,
                                       ConstString   fileName,
                                       FileInfo      *fileInfo
                                      )
{
  String infoFileName;
  Errors error;
  #ifdef HAVE_CURL
    Server            server;
    CURL              *curlHandle;
    String            baseURL;
    const char        *plainLoginPassword;
    CURLcode          curlCode;
    String            pathName,baseName;
    String            url;
    StringTokenizer   nameTokenizer;
    ConstString       token;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(fileInfo != NULL);

  infoFileName = (fileName != NULL) ? fileName : storageHandle->storageSpecifier.archiveName;
  memset(fileInfo,0,sizeof(fileInfo));

  error = ERROR_UNKNOWN;
  #ifdef HAVE_CURL
    // get WebDAV server settings
    getFTPServerSettings(storageHandle->storageSpecifier.hostName,storageHandle->jobOptions,&server);
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_set(storageHandle->storageSpecifier.loginName,ftpServer.loginName);
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_setCString(storageHandle->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageHandle->storageSpecifier.loginName)) String_setCString(storageHandle->storageSpecifier.loginName,getenv("USER"));
    if (String_isEmpty(storageHandle->storageSpecifier.hostName))
    {
      return ERROR_NO_HOST_NAME;
    }

    // allocate FTP server
    if (!allocateServer(&server,SERVER_CONNECTION_PRIORITY_LOW,60*1000L))
    {
      return ERROR_TOO_MANY_CONNECTIONS;
    }

    // open Curl handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      error = ERROR_WEBDAV_SESSION_FAIL;
    }

    // get base URL
    baseURL = String_format(String_new(),"http://%S",storageHandle->storageSpecifier.hostName);
    if (storageHandle->storageSpecifier.hostPort != 0) String_format(baseURL,":d",storageHandle->storageSpecifier.hostPort);

    // get pathname, basename
    pathName = File_getFilePathName(String_new(),infoFileName);
    baseName = File_getFileBaseName(String_new(),infoFileName);

    // get URL
    url = String_format(String_duplicate(baseURL),"/");
    File_initSplitFileName(&nameTokenizer,pathName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_append(url,token);
      String_appendChar(url,'/');
    }
    File_doneSplitFileName(&nameTokenizer);
    String_append(url,baseName);

    // get file info
    curlCode = initWebDAVHandle(curlHandle,
                                url,
                                storageHandle->storageSpecifier.loginName,
                                storageHandle->storageSpecifier.loginPassword,
                                WEBDAV_TIMEOUT
                              );
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
    }
    if (curlCode == CURLE_OK)
    {
#warning INFO, HEAD?
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_CUSTOMREQUEST,"INFO");
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_perform(curlHandle);
    }
    if (curlCode == CURLE_OK)
    {
      error = ERROR_NONE;
    }
    else
    {
      error = ERRORX_(FILE_NOT_FOUND_,0,curl_easy_strerror(curlCode));
    }

    // free resources
    String_delete(url);
    String_delete(baseName);
    String_delete(pathName);
    String_delete(baseURL);
    (void)curl_easy_cleanup(curlHandle);
    freeServer(&server);
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileInfo);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
  assert(error != ERROR_UNKNOWN);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageWebDAV_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                             const StorageSpecifier     *storageSpecifier,
                                             const JobOptions           *jobOptions,
                                             ServerConnectionPriorities serverConnectionPriority
                                            )
{
  Errors       error;
  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    AutoFreeList autoFreeList;
    WebDAVServer      webDAVServer;
    CURL              *curlHandle;
    String            url;
    CURLcode          curlCode;
    StringTokenizer   nameTokenizer;
    ConstString       token;
    String            directoryData;
    struct curl_slist *curlSList;
  #endif /* defined(HAVE_CURL) && defined(HAVE_MXML) */

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_WEBDAV);
  assert(jobOptions != NULL);

  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif /* defined(HAVE_CURL) && defined(HAVE_MXML) */

#warning TODO remove
UNUSED_VARIABLE(storageSpecifier);

  // open directory listing
  error = ERROR_UNKNOWN;
  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    // init variables
    AutoFree_init(&autoFreeList);
    storageDirectoryListHandle->type               = STORAGE_TYPE_WEBDAV;
    storageDirectoryListHandle->webdav.rootNode    = NULL;
    storageDirectoryListHandle->webdav.lastNode    = NULL;
    storageDirectoryListHandle->webdav.currentNode = NULL;

    // get WebDAV server settings
    storageDirectoryListHandle->webdav.server = getWebDAVServerSettings(storageDirectoryListHandle->storageSpecifier.hostName,jobOptions,&webDAVServer);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_set(storageDirectoryListHandle->storageSpecifier.loginName,webDAVServer.loginName);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("USER"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate WebDAV server
    if (!allocateServer(storageDirectoryListHandle->webdav.server,serverConnectionPriority,60*1000L))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,storageDirectoryListHandle->webdav.server,{ freeServer(storageDirectoryListHandle->webdav.server); });

    // check WebDAV login, get correct password
    error = ERROR_UNKNOWN;
    if ((error != ERROR_NONE) && !Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword))
    {
      error = checkWebDAVLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                               storageDirectoryListHandle->storageSpecifier.loginName,
                               storageDirectoryListHandle->storageSpecifier.loginPassword
                              );
    }
    if ((error != ERROR_NONE) && !Password_isEmpty(webDAVServer.password))
    {
      error = checkWebDAVLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                               storageDirectoryListHandle->storageSpecifier.loginName,
                               webDAVServer.password
                              );
      if (error == ERROR_NONE)
      {
        Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,webDAVServer.password);
      }
    }
    if ((error != ERROR_NONE) && !Password_isEmpty(defaultWebDAVPassword))
    {
      error = checkWebDAVLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                               storageDirectoryListHandle->storageSpecifier.loginName,
                               defaultWebDAVPassword
                              );
      if (error == ERROR_NONE)
      {
        Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,defaultWebDAVPassword);
      }
    }
    if (error != ERROR_NONE)
    {
      // initialize default password
      if (initDefaultWebDAVPassword(storageDirectoryListHandle->storageSpecifier.hostName,storageDirectoryListHandle->storageSpecifier.loginName,jobOptions))
      {
        error = checkWebDAVLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                                 storageDirectoryListHandle->storageSpecifier.loginName,
                                 defaultWebDAVPassword
                                );
        if (error == ERROR_NONE)
        {
          Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,defaultWebDAVPassword);
        }
      }
      else
      {
        error = !Password_isEmpty(defaultWebDAVPassword) ? ERROR_INVALID_WEBDAV_PASSWORD : ERROR_NO_WEBDAV_PASSWORD;
      }
    }
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // init handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_WEBDAV_SESSION_FAIL;
    }

    // get URL
    url = String_format(String_new(),"http://%S",storageDirectoryListHandle->storageSpecifier.hostName);
    if (storageDirectoryListHandle->storageSpecifier.hostPort != 0) String_format(url,":d",storageDirectoryListHandle->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,storageDirectoryListHandle->storageSpecifier.archiveName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    File_doneSplitFileName(&nameTokenizer);
    String_appendChar(url,'/');

    // read directory data
    directoryData = String_new();
    curlSList = curl_slist_append(NULL,"Depth: 1");
    curlCode = initWebDAVHandle(curlHandle,
                                url,
                                storageDirectoryListHandle->storageSpecifier.loginName,
                                storageDirectoryListHandle->storageSpecifier.loginPassword,
                                WEBDAV_TIMEOUT
                              );
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_CUSTOMREQUEST,"PROPFIND");
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_HTTPHEADER,curlSList);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_WRITEFUNCTION,curlWebDAVReadDirectoryDataCallback);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_WRITEDATA,directoryData);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_perform(curlHandle);
    }
    (void)curl_easy_setopt(curlHandle,CURLOPT_HTTPHEADER,NULL);
    curl_slist_free_all(curlSList);
    if (curlCode != CURLE_OK)
    {
      error = ERROR_READ_DIRECTORY;
      String_delete(directoryData);
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // parse directory entries
    storageDirectoryListHandle->webdav.rootNode = mxmlLoadString(NULL,
                                                                 String_cString(directoryData),
                                                                 MXML_OPAQUE_CALLBACK
                                                                );
    if (storageDirectoryListHandle->webdav.rootNode == NULL)
    {
      error = ERROR_READ_DIRECTORY;
      String_delete(directoryData);
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    storageDirectoryListHandle->webdav.lastNode = storageDirectoryListHandle->webdav.rootNode;

    // discard first entry: directory
    storageDirectoryListHandle->webdav.lastNode = mxmlFindElement(storageDirectoryListHandle->webdav.lastNode,
                                                                  storageDirectoryListHandle->webdav.rootNode,
                                                                  "D:href",
                                                                  NULL,
                                                                  NULL,
                                                                  MXML_DESCEND
                                                                 );

    // free resources
    String_delete(directoryData);
    String_delete(url);
    (void)curl_easy_cleanup(curlHandle);
    AutoFree_done(&autoFreeList);
  #else /* not defined(HAVE_CURL) && defined(HAVE_MXML) */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(jobOptions);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* defined(HAVE_CURL) && defined(HAVE_MXML) */

  return ERROR_NONE;
}

LOCAL void StorageWebDAV_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->type == STORAGE_TYPE_WEBDAV);

  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    mxmlDelete(storageDirectoryListHandle->webdav.rootNode);
  #else /* not defined(HAVE_CURL) && defined(HAVE_MXML) */
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* defined(HAVE_CURL) && defined(HAVE_MXML) */
}

LOCAL bool StorageWebDAV_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  bool endOfDirectoryFlag;

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->type == STORAGE_TYPE_WEBDAV);

  endOfDirectoryFlag = TRUE;
  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    if (storageDirectoryListHandle->webdav.currentNode == NULL)
    {
      storageDirectoryListHandle->webdav.currentNode = mxmlFindElement(storageDirectoryListHandle->webdav.lastNode,
                                                                       storageDirectoryListHandle->webdav.rootNode,
                                                                       "D:href",
                                                                       NULL,
                                                                       NULL,
                                                                       MXML_DESCEND
                                                                      );
    }
    endOfDirectoryFlag = (storageDirectoryListHandle->webdav.currentNode == NULL);
  #else /* not defined(HAVE_CURL) && defined(HAVE_MXML) */
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* defined(HAVE_CURL) && defined(HAVE_MXML) */

  return endOfDirectoryFlag;
}

LOCAL Errors StorageWebDAV_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                             String                     fileName,
                                             FileInfo                   *fileInfo
                                            )
{
  Errors error;
  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    mxml_node_t *node;
  #endif /* HAVE_CURL */

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->type == STORAGE_TYPE_WEBDAV);

  error = ERROR_NONE;
  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    if (storageDirectoryListHandle->webdav.currentNode == NULL)
    {
      storageDirectoryListHandle->webdav.currentNode = mxmlFindElement(storageDirectoryListHandle->webdav.lastNode,
                                                                       storageDirectoryListHandle->webdav.rootNode,
                                                                       "D:href",
                                                                       NULL,
                                                                       NULL,
                                                                       MXML_DESCEND
                                                                      );
    }

    if (   (storageDirectoryListHandle->webdav.currentNode != NULL)
        && (storageDirectoryListHandle->webdav.currentNode->type == MXML_ELEMENT)
        && (storageDirectoryListHandle->webdav.currentNode->child != NULL)
        && (storageDirectoryListHandle->webdav.currentNode->child->type == MXML_OPAQUE)
        && (storageDirectoryListHandle->webdav.currentNode->child->value.opaque != NULL)
       )
    {
      // get file name
      String_setCString(fileName,storageDirectoryListHandle->webdav.currentNode->child->value.opaque);

      // get file info
      if (fileInfo != NULL)
      {
        fileInfo->type            = FILE_TYPE_FILE;
        fileInfo->size            = 0LL;
        fileInfo->timeLastAccess  = 0LL;
        fileInfo->timeModified    = 0LL;
        fileInfo->timeLastChanged = 0LL;
        fileInfo->userId          = FILE_DEFAULT_USER_ID;
        fileInfo->groupId         = FILE_DEFAULT_GROUP_ID;
        fileInfo->permission      = 0;
        fileInfo->major           = 0;
        fileInfo->minor           = 0;

        node = mxmlFindElement(storageDirectoryListHandle->webdav.currentNode,
                               storageDirectoryListHandle->webdav.rootNode,
                               "lp1:getcontentlength",
                               NULL,
                               NULL,
                               MXML_DESCEND
                              );
        if (   (node != NULL)
            && (node->type == MXML_ELEMENT)
            && (node->child != NULL)
            && (node->child->type == MXML_OPAQUE)
            && (node->child->value.opaque != NULL)
           )
        {
          fileInfo->size = strtol(node->child->value.opaque,NULL,10);
        }
        node = mxmlFindElement(storageDirectoryListHandle->webdav.currentNode,
                               storageDirectoryListHandle->webdav.rootNode,
                               "lp1:getlastmodified",
                               NULL,
                               NULL,
                               MXML_DESCEND
                              );
        if (   (node != NULL)
            && (node->type == MXML_ELEMENT)
            && (node->child != NULL)
            && (node->child->type == MXML_OPAQUE)
            && (node->child->value.opaque != NULL)
           )
        {
          fileInfo->timeModified = Misc_parseDateTime(node->child->value.opaque);
        }
      }

      // next file
      storageDirectoryListHandle->webdav.lastNode    = storageDirectoryListHandle->webdav.currentNode;
      storageDirectoryListHandle->webdav.currentNode = NULL;

      error = ERROR_NONE;
    }
    else
    {
      error = ERROR_READ_DIRECTORY;
    }
  #else /* not defined(HAVE_CURL) && defined(HAVE_MXML) */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileInfo);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* defined(HAVE_CURL) && defined(HAVE_MXML) */
  assert(error != ERROR_UNKNOWN);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
