/***********************************************************************\
*
* $Revision: 4012 $
* $Date: 2015-04-28 19:02:40 +0200 (Tue, 28 Apr 2015) $
* $Author: torsten $
* Contents: storage WebDAV functions
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
#ifdef HAVE_CURL
  #include <curl/curl.h>
#endif /* HAVE_CURL */
#ifdef HAVE_MXML
  #include <mxml.h>
#endif /* HAVE_MXML */
#include <signal.h>
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
// file data buffer size
#define BUFFER_SIZE (64*1024)

// different timeouts [ms]
#define WEBDAV_TIMEOUT (30*1000)

#define INITIAL_BUFFER_SIZE   (64*1024)
#define INCREMENT_BUFFER_SIZE ( 8*1024)
#define MAX_BUFFER_SIZE       (64*1024)
#define MAX_FILENAME_LENGTH   ( 8*1024)

// HTTP codes
#define HTTP_CODE_OK                     200
#define HTTP_CODE_CREATED                201
#define HTTP_CODE_ACCEPTED               202
#define HTTP_CODE_BAD_REQUEST            400
#define HTTP_CODE_UNAUTHORIZED           401
#define HTTP_CODE_FORBITTEN              403
#define HTTP_CODE_NOT_FOUND              404
#define HTTP_CODE_BAD_METHOD             405
#define HTTP_CODE_NOT_ACCEPTABLE         406
#define HTTP_CODE_RESOURCE_TIMEOUT       408
#define HTTP_CODE_RESOURCE_NOT_AVAILABLE 410
#define HTTP_CODE_ENTITY_TOO_LARGE       413
#define HTTP_CODE_EXPECTATIONFAILED      417

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
* Name   : initWebDAVLogin
* Purpose: init WebDAV login
* Input  : hostName                - host name
*          loginName               - login name
*          loginPassword           - login password
*          jobOptions              - job options
*          getNamePasswordFunction - get password call-back (can be
*                                    NULL)
*          getNamePasswordUserData - user data for get password call-back
* Output : -
* Return : TRUE if WebDAV password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initWebDAVLogin(ConstString             hostName,
                           String                  loginName,
                           Password                *loginPassword,
                           const JobOptions        *jobOptions,
                           GetNamePasswordFunction getNamePasswordFunction,
                           void                    *getNamePasswordUserData
                          )
{
  bool   initFlag;
  String s;

  assert(!String_isEmpty(hostName));
  assert(loginName != NULL);
  assert(loginPassword != NULL);

  initFlag = FALSE;

  if (jobOptions != NULL)
  {
    SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if (Password_isEmpty(&jobOptions->webDAVServer.password))
      {
        switch (globalOptions.runMode)
        {
          case RUN_MODE_INTERACTIVE:
            if (defaultWebDAVPassword == NULL)
            {
              s = !String_isEmpty(loginName)
                    ? String_format(String_new(),"WebDAV login password for %S@%S",loginName,hostName)
                    : String_format(String_new(),"WebDAV login password for %S",hostName);
              if (Password_input(loginPassword,String_cString(s),PASSWORD_INPUT_MODE_ANY))
              {
                initFlag = TRUE;
              }
              String_delete(s);
            }
            else
            {
              Password_set(loginPassword,defaultWebDAVPassword);
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
                                          PASSWORD_TYPE_WEBDAV,
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
            else
            {
              initFlag = TRUE;
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

/***********************************************************************\
* Name   : setWebDAVLogin
* Purpose: set WebDAV login
* Input  : curlHandle    - CURL handle
*          loginName     - login name
*          loginPassword - login password
*          timeout       - timeout [ms]
* Output : -
* Return : CURLE_OK if no error, CURL error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL CURLcode setWebDAVLogin(CURL *curlHandle, ConstString loginName, Password *loginPassword, long timeout)
{
  CURLcode curlCode;

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
  (void)curl_easy_setopt(storageInfo->webdav.curlMultiHandle,CURLOPT_NOSIGNAL,1L);
  (void)curl_easy_setopt(storageInfo->webdav.curlHandle,CURLOPT_NOSIGNAL,1L);
  */

  // set login
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_USERNAME,String_cString(loginName));
  }
  if (curlCode == CURLE_OK)
  {
    PASSWORD_DEPLOY_DO(plainPassword,loginPassword)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_PASSWORD,plainPassword);
    }
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

  // get URL
  url = String_format(String_new(),"http://%S",hostName);

  // init WebDAV login
  curlCode = setWebDAVLogin(curlHandle,loginName,loginPassword,WEBDAV_TIMEOUT);
  if (curlCode != CURLE_OK)
  {
    String_delete(url);
    (void)curl_easy_cleanup(curlHandle);
    return ERRORX_(WEBDAV_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
  }

  // check access root URL
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
  }
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
  }
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_perform(curlHandle);
  }
  if (curlCode != CURLE_OK)
  {
    String_delete(url);
    (void)curl_easy_cleanup(curlHandle);
    return ERRORX_(WEBDAV_AUTHENTICATION,0,"%s",curl_easy_strerror(curlCode));
  }

  // free resources
  String_delete(url);
  (void)curl_easy_cleanup(curlHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : fileDirectoryExists
* Purpose: check if file/directory exists
* Input  : curlHandle - CURL handle
*          url        - URL
* Output : -
* Return : TRUE iff file/directory exists
* Notes  : -
\***********************************************************************/

LOCAL bool fileDirectoryExists(CURL *curlHandle, ConstString url)
{
  CURLcode curlCode;

  assert(curlHandle != NULL);
  assert(url != NULL);

  curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
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

  return (curlCode == CURLE_OK);
}

/***********************************************************************\
* Name   : makeDirectory
* Purpose: make directory
* Input  : curlHandle - CURL handle
*          url        - URL
* Output : -
* Return : CURL code
* Notes  : -
\***********************************************************************/

LOCAL CURLcode makeDirectory(CURL *curlHandle, ConstString url)
{
  CURLcode curlCode;

  assert(curlHandle != NULL);
  assert(url != NULL);

  curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_FAILONERROR,1L);
  }
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
  }
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_CUSTOMREQUEST,"MKCOL");
  }
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_perform(curlHandle);
  }

  return curlCode;
}

/***********************************************************************\
* Name   : fileDirectoryDelete
* Purpose: delete file/directory
* Input  : curlHandle - CURL handle
*          url        - URL
* Output : -
* Return : TRUE iff file/directory deleted
* Notes  : -
\***********************************************************************/

LOCAL bool deleteFileDirectory(CURL *curlHandle, ConstString url)
{
  CURLcode curlCode;

  assert(curlHandle != NULL);
  assert(url != NULL);

  curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
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
    curlCode = curl_easy_perform(curlHandle);
  }

  return (curlCode == CURLE_OK);
}

/***********************************************************************\
* Name   : getResponseError
* Purpose: get HTTP response error
* Input  : curlHandle - CURL handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors getResponseError(CURL *curlHandle)
{
  CURLcode curlCode;
  long     responseCode;
  Errors   error;

  assert(curlHandle != NULL);

  curlCode = curl_easy_getinfo(curlHandle,CURLINFO_RESPONSE_CODE,&responseCode);
  if (curlCode == CURLE_OK)
  {
    switch (responseCode)
    {
      case HTTP_CODE_OK:
      case HTTP_CODE_CREATED:
      case HTTP_CODE_ACCEPTED:
        error = ERROR_NONE;
        break;
      case HTTP_CODE_BAD_REQUEST:
      case HTTP_CODE_BAD_METHOD:
        error = ERROR_WEBDAV_BAD_REQUEST;
        break;
      case HTTP_CODE_UNAUTHORIZED:
      case HTTP_CODE_FORBITTEN:
        error = ERROR_FILE_ACCESS_DENIED;
        break;
      case HTTP_CODE_NOT_FOUND:
        error = ERROR_FILE_NOT_FOUND_;
        break;
      default:
        error = ERRORX_(WEBDAV_FAIL,0,"%s",curl_easy_strerror(curlCode));
        break;
    }
  }
  else
  {
    error = ERRORX_(WEBDAV_FAIL,0,"%s",curl_easy_strerror(curlCode));
  }

  return error;
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
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->webdav);
  assert(storageHandle->webdav.sendBuffer.data != NULL);

//TODO: progress callback?

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
  size_t        bytesReceived;
  ulong         newSize;
  byte          *newData;

  assert(buffer != NULL);
  assert(size > 0);
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->webdav);

//TODO: progress callback?

  // calculate number of received bytes
  bytesReceived = n*size;

  // increase buffer size if required
  if ((storageHandle->webdav.receiveBuffer.length+(ulong)bytesReceived) > storageHandle->webdav.receiveBuffer.size)
  {
    newSize = ((storageHandle->webdav.receiveBuffer.length+(ulong)bytesReceived+INCREMENT_BUFFER_SIZE-1)/INCREMENT_BUFFER_SIZE)*INCREMENT_BUFFER_SIZE;
    newData = (byte*)realloc(storageHandle->webdav.receiveBuffer.data,newSize);
    if (newData == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    storageHandle->webdav.receiveBuffer.data = newData;
    storageHandle->webdav.receiveBuffer.size = newSize;
  }

  // append data to buffer
  memcpy(storageHandle->webdav.receiveBuffer.data+storageHandle->webdav.receiveBuffer.length,buffer,(ulong)bytesReceived);
  storageHandle->webdav.receiveBuffer.length += (ulong)bytesReceived;
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
  if      (String_matchCString(webdavSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,loginName,s,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>:<login password>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (loginPassword != NULL) Password_setString(loginPassword,s);

    result = TRUE;
  }
  else if (String_matchCString(webdavSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,loginName,STRING_NO_ASSIGN,hostName,NULL))
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

LOCAL bool StorageWebDAV_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
                                         ConstString            archiveName1,
                                         const StorageSpecifier *storageSpecifier2,
                                         ConstString            archiveName2
                                        )
{
  assert(storageSpecifier1 != NULL);
  assert(storageSpecifier1->type == STORAGE_TYPE_WEBDAV);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_WEBDAV);

  if (archiveName1 == NULL) archiveName1 = storageSpecifier1->archiveName;
  if (archiveName2 == NULL) archiveName2 = storageSpecifier2->archiveName;

  return    String_equals(storageSpecifier1->hostName,storageSpecifier2->hostName)
         && String_equals(storageSpecifier1->loginName,storageSpecifier2->loginName)
         && String_equals(archiveName1,archiveName2);
}

LOCAL String StorageWebDAV_getName(String                 string,
                                   const StorageSpecifier *storageSpecifier,
                                   ConstString            archiveName
                                  )
{
  ConstString storageFileName;

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

  String_appendCString(string,"webdav://");
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

LOCAL void StorageWebDAV_getPrintableName(String                 string,
                                          const StorageSpecifier *storageSpecifier,
                                          ConstString            fileName
                                         )
{
  ConstString storageFileName;

  assert(string != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_WEBDAV);

  // get file to use
  if      (!String_isEmpty(fileName))
  {
    storageFileName = fileName;
  }
  else if (!String_isEmpty(storageSpecifier->archivePatternString))
  {
    storageFileName = storageSpecifier->archivePatternString;
  }
  else
  {
    storageFileName = storageSpecifier->archiveName;
  }

  String_appendCString(string,"webdav://");
  if (!String_isEmpty(storageSpecifier->loginName))
  {
    String_append(string,storageSpecifier->loginName);
    String_appendChar(string,'@');
  }
  String_append(string,storageSpecifier->hostName);
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(string,'/');
    String_append(string,storageFileName);
  }
}

LOCAL Errors StorageWebDAV_init(StorageInfo                *storageInfo,
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
    uint         retries;
    Password     password;
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_WEBDAV);

  #ifdef HAVE_CURL
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif /* HAVE_CURL */

  UNUSED_VARIABLE(storageSpecifier);

  #ifdef HAVE_CURL
    // init variables
    AutoFree_init(&autoFreeList);
    initBandWidthLimiter(&storageInfo->webdav.bandWidthLimiter,maxBandWidthList);
    AUTOFREE_ADD(&autoFreeList,&storageInfo->webdav.bandWidthLimiter,{ doneBandWidthLimiter(&storageInfo->webdav.bandWidthLimiter); });

    // get WebDAV server settings
    storageInfo->webdav.serverId = Configuration_initWebDAVServerSettings(&webDAVServer,storageInfo->storageSpecifier.hostName,jobOptions);
    AUTOFREE_ADD(&autoFreeList,&webDAVServer,{ Configuration_doneWebDAVServerSettings(&webDAVServer); });
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_set(storageInfo->storageSpecifier.loginName,webDAVServer.loginName);
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("USER"));
    if (String_isEmpty(storageInfo->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate WebDAV server
    if (!allocateServer(storageInfo->webdav.serverId,serverConnectionPriority,60*1000L))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageInfo->webdav.serverId,{ freeServer(storageInfo->webdav.serverId); });

    // check WebDAV login, get correct password
    error = ERROR_WEBDAV_SESSION_FAIL;
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL) && !Password_isEmpty(storageInfo->storageSpecifier.loginPassword))
    {
      error = checkWebDAVLogin(storageInfo->storageSpecifier.hostName,
                               storageInfo->storageSpecifier.loginName,
                               storageInfo->storageSpecifier.loginPassword
                              );
    }
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL) && !Password_isEmpty(&webDAVServer.password))
    {
      error = checkWebDAVLogin(storageInfo->storageSpecifier.hostName,
                               storageInfo->storageSpecifier.loginName,
                               &webDAVServer.password
                              );
      if (error == ERROR_NONE)
      {
        Password_set(storageInfo->storageSpecifier.loginPassword,&webDAVServer.password);
      }
    }
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL) && !Password_isEmpty(defaultWebDAVPassword))
    {
      error = checkWebDAVLogin(storageInfo->storageSpecifier.hostName,
                               storageInfo->storageSpecifier.loginName,
                               defaultWebDAVPassword
                              );
      if (error == ERROR_NONE)
      {
        Password_set(storageInfo->storageSpecifier.loginPassword,defaultWebDAVPassword);
      }
    }
    if (Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL)
    {
      // initialize interactive/default password
      retries = 0;
      Password_init(&password);
      while ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && (retries < MAX_PASSWORD_REQUESTS))
      {
        if (initWebDAVLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.loginName,
                            &password,
                            jobOptions,
                            CALLBACK_(storageInfo->getNamePasswordFunction,storageInfo->getNamePasswordUserData)
                           )
           )
        {
          error = checkWebDAVLogin(storageInfo->storageSpecifier.hostName,
                                   storageInfo->storageSpecifier.loginName,
                                   &password
                                  );
          if (error == ERROR_NONE)
          {
            Password_set(storageInfo->storageSpecifier.loginPassword,&password);
          }
        }
        retries++;
      }
      Password_done(&password);
    }
    if (Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL)
    {
      error = (   !Password_isEmpty(storageInfo->storageSpecifier.loginPassword)
               || !Password_isEmpty(&webDAVServer.password)
               || !Password_isEmpty(defaultWebDAVPassword)
              )
                ? ERRORX_(INVALID_WEBDAV_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName))
                : ERRORX_(NO_WEBDAV_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
    }

    // store password as default webDAV password
    if (error == ERROR_NONE)
    {
      if (defaultWebDAVPassword == NULL) defaultWebDAVPassword = Password_new();
      Password_set(defaultWebDAVPassword,storageInfo->storageSpecifier.loginPassword);
    }

    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // free resources
    Configuration_doneWebDAVServerSettings(&webDAVServer);
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(maxBandWidthList);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
}

LOCAL Errors StorageWebDAV_done(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  // free WebDAV server connection
  #ifdef HAVE_CURL
    freeServer(storageInfo->webdav.serverId);
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
  #endif /* HAVE_CURL */

  return ERROR_NONE;
}

LOCAL bool StorageWebDAV_isServerAllocationPending(const StorageInfo *storageInfo)
{
  bool serverAllocationPending;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  serverAllocationPending = FALSE;
  #ifdef HAVE_CURL
    serverAllocationPending = isServerAllocationPending(storageInfo->webdav.serverId);
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);

    serverAllocationPending = FALSE;
  #endif /* HAVE_CURL */

  return serverAllocationPending;
}

LOCAL Errors StorageWebDAV_preProcess(const StorageInfo *storageInfo,
                                      ConstString       fileName,
                                      time_t            timestamp,
                                      bool              initialFlag
                                     )
{
  Errors error;
  #ifdef HAVE_CURL
    TextMacros (textMacros,2);
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  error = ERROR_NONE;

  #ifdef HAVE_CURL
    if (!initialFlag)
    {
      // init macros
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING ("%file",  fileName,                 NULL);
        TEXT_MACRO_X_INTEGER("%number",storageInfo->volumeNumber,NULL);
      }

      // write pre-processing
      if (!String_isEmpty(globalOptions.webdav.writePreProcessCommand))
      {
        printInfo(1,"Write pre-processing...");
        error = executeTemplate(String_cString(globalOptions.webdav.writePreProcessCommand),
                                timestamp,
                                textMacros.data,
                                textMacros.count
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(timestamp);
    UNUSED_VARIABLE(initialFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */

  return error;
}

LOCAL Errors StorageWebDAV_postProcess(const StorageInfo *storageInfo,
                                       ConstString       archiveName,
                                       time_t            timestamp,
                                       bool              finalFlag
                                      )
{
  Errors error;
  #ifdef HAVE_CURL
    TextMacros (textMacros,2);
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  error = ERROR_NONE;

  #ifdef HAVE_CURL
    if (!finalFlag)
    {
      // init macros
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING ("%file",  archiveName,              NULL);
        TEXT_MACRO_X_INTEGER("%number",storageInfo->volumeNumber,NULL);
      }

      // write post-process
      if (!String_isEmpty(globalOptions.webdav.writePostProcessCommand))
      {
        printInfo(1,"Write post-processing...");
        error = executeTemplate(String_cString(globalOptions.webdav.writePostProcessCommand),
                                timestamp,
                                textMacros.data,
                                textMacros.count
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(timestamp);
    UNUSED_VARIABLE(finalFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */

  return error;
}

LOCAL bool StorageWebDAV_exists(const StorageInfo*storageInfo, ConstString archiveName)
{
  bool existsFlag;
  #if   defined(HAVE_CURL)
    CURL            *curlHandle;
    String          directoryName,baseName;
    String          url;
    CURLcode        curlCode;
    StringTokenizer nameTokenizer;
    ConstString     token;
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(!String_isEmpty(archiveName));

  existsFlag = FALSE;

  #if   defined(HAVE_CURL)
    // open Curl handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      return ERROR_WEBDAV_SESSION_FAIL;
    }

    // get directory name, base name
    directoryName = File_getDirectoryName(String_new(),archiveName);
    baseName      = File_getBaseName(String_new(),archiveName);

    // get URL
    url = String_format(String_new(),"http://%S",storageInfo->storageSpecifier.hostName);
    if (storageInfo->storageSpecifier.hostPort != 0) String_appendFormat(url,":%d",storageInfo->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,directoryName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    File_doneSplitFileName(&nameTokenizer);
    String_appendChar(url,'/');
    String_append(url,baseName);

    // init WebDAV login
    curlCode = setWebDAVLogin(curlHandle,
                              storageInfo->storageSpecifier.loginName,
                              storageInfo->storageSpecifier.loginPassword,
                              WEBDAV_TIMEOUT
                             );
    if (curlCode != CURLE_OK)
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      (void)curl_easy_cleanup(curlHandle);
      return ERRORX_(WEBDAV_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // check if file exists
    existsFlag = fileDirectoryExists(curlHandle,url);

    // free resources
    String_delete(url);
    String_delete(baseName);
    String_delete(directoryName);
    (void)curl_easy_cleanup(curlHandle);
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
  #endif /* HAVE_CURL */

  return existsFlag;
}

LOCAL bool StorageWebDAV_isFile(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL bool StorageWebDAV_isDirectory(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL bool StorageWebDAV_isReadable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL bool StorageWebDAV_isWritable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_exists(archiveName);
}

LOCAL Errors StorageWebDAV_getTmpName(String archiveName, const StorageInfo *storageInfo)
{
  assert(archiveName != NULL);
  assert(!String_isEmpty(archiveName));
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(archiveName);
  UNUSED_VARIABLE(storageInfo);

//TODO
  return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL Errors StorageWebDAV_create(StorageHandle *storageHandle,
                                  ConstString   fileName,
                                  uint64        fileSize,
                                  bool          forceFlag
                                 )
{
  #ifdef HAVE_CURL
    String          baseURL;
    CURLcode        curlCode;
    CURLMcode       curlMCode;
    String          url;
    String          directoryName,baseName;
    StringTokenizer nameTokenizer;
    ConstString     token;
    Errors          error;
    int             runningHandles;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(!String_isEmpty(fileName));

//TODO: exists test?
UNUSED_VARIABLE(forceFlag);

  #ifdef HAVE_CURL
    // initialize variables
    storageHandle->webdav.curlMultiHandle      = NULL;
    storageHandle->webdav.curlHandle           = NULL;
    storageHandle->webdav.index                = 0LL;
    storageHandle->webdav.size                 = fileSize;
    storageHandle->webdav.receiveBuffer.data   = NULL;
    storageHandle->webdav.receiveBuffer.size   = 0LL;
    storageHandle->webdav.receiveBuffer.offset = 0LL;
    storageHandle->webdav.receiveBuffer.length = 0L;
    storageHandle->webdav.sendBuffer.data      = NULL;
    storageHandle->webdav.sendBuffer.index     = 0L;
    storageHandle->webdav.sendBuffer.length    = 0L;

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
    baseURL = String_format(String_new(),"http://%S",storageHandle->storageInfo->storageSpecifier.hostName);
    if (storageHandle->storageInfo->storageSpecifier.hostPort != 0) String_appendFormat(baseURL,":d",storageHandle->storageInfo->storageSpecifier.hostPort);

    // get directory name, base name
    directoryName = File_getDirectoryName(String_new(),fileName);
    baseName      = File_getBaseName(String_new(),fileName);

    // init WebDAV login
    curlCode = setWebDAVLogin(storageHandle->webdav.curlHandle,
                              storageHandle->storageInfo->storageSpecifier.loginName,
                              storageHandle->storageInfo->storageSpecifier.loginPassword,
                              WEBDAV_TIMEOUT
                             );
    if (curlCode != CURLE_OK)
    {
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      return ERRORX_(WEBDAV_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // create directories if necessary
    if (!String_isEmpty(directoryName))
    {
      curlCode = CURLE_OK;
      url = String_duplicate(baseURL);
      String_appendFormat(url,"/");
      File_initSplitFileName(&nameTokenizer,directoryName);
      while (   (curlCode == CURLE_OK)
             && File_getNextSplitFileName(&nameTokenizer,&token)
            )
      {
        String_append(url,token);
        String_appendChar(url,'/');

        // check if sub-directory exists
        if (!fileDirectoryExists(storageHandle->webdav.curlHandle,url))
        {
          // create sub-directory
          curlCode = makeDirectory(storageHandle->webdav.curlHandle,url);
        }
      }
      File_doneSplitFileName(&nameTokenizer);
      if (curlCode != CURLE_OK)
      {
        error = getResponseError(storageHandle->webdav.curlHandle);
        String_delete(url);
        String_delete(baseName);
        String_delete(directoryName);
        String_delete(baseURL);
        (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
        (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
        return error;
      }
      String_delete(url);
    }

    // get URL
    url = String_duplicate(baseURL);
    String_appendFormat(url,"/");
    File_initSplitFileName(&nameTokenizer,directoryName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_append(url,token);
      String_appendChar(url,'/');
    }
    File_doneSplitFileName(&nameTokenizer);
    String_append(url,baseName);

    // check to stop if exists/append/overwrite
    switch (storageHandle->storageInfo->jobOptions->archiveFileMode)
    {
      case ARCHIVE_FILE_MODE_STOP:
        // check if file exists
        if (fileDirectoryExists(storageHandle->webdav.curlHandle,url))
        {
          String_delete(url);
          String_delete(baseName);
          String_delete(directoryName);
          String_delete(baseURL);
          (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
          (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
          return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(fileName));
        }
        break;
      case ARCHIVE_FILE_MODE_APPEND:
        // not supported - ignored
        break;
      case ARCHIVE_FILE_MODE_OVERWRITE:
        // try to delete existing file (ignore error)
        (void)deleteFileDirectory(storageHandle->webdav.curlHandle,url);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }

    // init WebDAV upload
    curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_URL,String_cString(url));
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_FAILONERROR,1L);
    }
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
      error = getResponseError(storageHandle->webdav.curlHandle);
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      return error;
    }

#if 0
    // check response code
    curlCode = curl_easy_getinfo(storageHandle->webdav.curlHandle,CURLINFO_RESPONSE_CODE,&responseCode);
fprintf(stderr,"%s, %d: r=%d x=%d\n",__FILE__,__LINE__,curlCode,responseCode);
    if ((curlCode != CURLM_OK) || (responseCode >= 400))
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      return ERRORX_(WEBDAV_UPLOAD,0,"%s",curl_multi_strerror(curlMCode));
    }
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
#endif

    // add handle
    curlMCode = curl_multi_add_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
    if (curlMCode != CURLM_OK)
    {
      error = getResponseError(storageHandle->webdav.curlHandle);
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      return error;
    }

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
      error = getResponseError(storageHandle->webdav.curlHandle);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_multi_remove_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      return error;
    }

    // free resources
    String_delete(url);
    String_delete(baseName);
    String_delete(directoryName);
    String_delete(baseURL);

    DEBUG_ADD_RESOURCE_TRACE(&storageHandle->webdav,StorageHandleWebDAV);

    return ERROR_NONE;
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileSize);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
}

LOCAL Errors StorageWebDAV_open(StorageHandle *storageHandle,
                                ConstString   archiveName
                               )
{
  #ifdef HAVE_CURL
    String          baseURL;
    CURLcode        curlCode;
    CURLMcode       curlMCode;
    String          url;
    String          directoryName,baseName;
    StringTokenizer nameTokenizer;
    ConstString     token;
    Errors          error;
    double          fileSize;
    int             runningHandles;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(!String_isEmpty(archiveName));

  #ifdef HAVE_CURL
    // initialize variables
    storageHandle->webdav.curlMultiHandle      = NULL;
    storageHandle->webdav.curlHandle           = NULL;
    storageHandle->webdav.index                = 0LL;
    storageHandle->webdav.size                 = 0LL;
    storageHandle->webdav.receiveBuffer.data   = NULL;
    storageHandle->webdav.receiveBuffer.size   = INITIAL_BUFFER_SIZE;
    storageHandle->webdav.receiveBuffer.offset = 0LL;
    storageHandle->webdav.receiveBuffer.length = 0L;
    storageHandle->webdav.sendBuffer.data      = NULL;
    storageHandle->webdav.sendBuffer.index     = 0L;
    storageHandle->webdav.sendBuffer.length    = 0L;

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
    baseURL = String_format(String_new(),"http://%S",storageHandle->storageInfo->storageSpecifier.hostName);
    if (storageHandle->storageInfo->storageSpecifier.hostPort != 0) String_appendFormat(baseURL,":d",storageHandle->storageInfo->storageSpecifier.hostPort);

    // get directory name, base name
    directoryName = File_getDirectoryName(String_new(),archiveName);
    baseName      = File_getBaseName(String_new(),archiveName);

    // init WebDAV login
    curlCode = setWebDAVLogin(storageHandle->webdav.curlHandle,
                              storageHandle->storageInfo->storageSpecifier.loginName,
                              storageHandle->storageInfo->storageSpecifier.loginPassword,
                              WEBDAV_TIMEOUT
                             );
    if (curlCode != CURLE_OK)
    {
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return ERRORX_(WEBDAV_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // get url
    url = String_duplicate(baseURL);
    String_appendFormat(url,"/");
    File_initSplitFileName(&nameTokenizer,directoryName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_append(url,token);
      String_appendChar(url,'/');
    }
    File_doneSplitFileName(&nameTokenizer);
    String_append(url,baseName);

    // check if file exists
    if (!fileDirectoryExists(storageHandle->webdav.curlHandle,url))
    {
      error = ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(url));
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return error;
    }

    // get file size
    curlCode = curl_easy_getinfo(storageHandle->webdav.curlHandle,CURLINFO_CONTENT_LENGTH_DOWNLOAD,&fileSize);
    if (   (curlCode != CURLE_OK)
        || (fileSize < 0.0)
       )
    {
      error = ERRORX_(WEBDAV_GET_SIZE,0,"%s",String_cString(url));
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return error;
    }
    storageHandle->webdav.size = (uint64)fileSize;

    // init WebDAV download
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_URL,String_cString(url));
    }
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
      error = getResponseError(storageHandle->webdav.curlHandle);
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      error = getResponseError(storageHandle->webdav.curlHandle);
    }

#if 0
    // check response code
    curlCode = curl_easy_getinfo(storageHandle->webdav.curlHandle,CURLINFO_RESPONSE_CODE,&responseCode);
fprintf(stderr,"%s, %d: r=%d x=%d\n",__FILE__,__LINE__,curlCode,responseCode);
    if ((curlCode != CURLM_OK) || (responseCode >= 400))
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      return ERRORX_(WEBDAV_UPLOAD,0,"%s",curl_multi_strerror(curlMCode));
    }
fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
#endif

    // add handle
    curlMCode = curl_multi_add_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
    if (curlMCode != CURLM_OK)
    {
      error = getResponseError(storageHandle->webdav.curlHandle);
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return error;
    }

    DEBUG_ADD_RESOURCE_TRACE(&storageHandle->webdav,StorageHandleWebDAV);

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
      DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->webdav,StorageHandleWebDAV);
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_multi_remove_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return ERRORX_(WEBDAV_SESSION_FAIL,0,"%s",curl_multi_strerror(curlMCode));
    }

    // free resources
    String_delete(url);
    String_delete(baseName);
    String_delete(directoryName);
    String_delete(baseURL);

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
  #ifdef HAVE_CURL
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->webdav);
  #endif /* HAVE_CURL */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  #ifdef HAVE_CURL
    assert(storageHandle->webdav.curlHandle != NULL);
    assert(storageHandle->webdav.curlMultiHandle != NULL);

    DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->webdav,StorageHandleWebDAV);

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
  #ifdef HAVE_CURL
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->webdav);
  #endif /* HAVE_CURL */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  #ifdef HAVE_CURL
    return storageHandle->webdav.index >= storageHandle->webdav.size;
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    return TRUE;
  #endif /* HAVE_CURL */
}

LOCAL Errors StorageWebDAV_read(StorageHandle *storageHandle,
                                void          *buffer,
                                ulong         bufferSize,
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
  #ifdef HAVE_CURL
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->webdav);
  #endif /* HAVE_CURL */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(buffer != NULL);

  if (bytesRead != NULL) (*bytesRead) = 0L;
  error = ERROR_NONE;
  #ifdef HAVE_CURL
    assert(storageHandle->webdav.curlMultiHandle != NULL);
    assert(storageHandle->webdav.receiveBuffer.data != NULL);

    while (   (bufferSize > 0L)
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
        bytesAvail = MIN(bufferSize,storageHandle->webdav.receiveBuffer.length-index);
        memcpy(buffer,storageHandle->webdav.receiveBuffer.data+index,bytesAvail);

        // adjust buffer, bufferSize, bytes read, index
        buffer = (byte*)buffer+bytesAvail;
        bufferSize -= bytesAvail;
        if (bytesRead != NULL) (*bytesRead) += bytesAvail;
        storageHandle->webdav.index += (uint64)bytesAvail;
      }

      // read rest of data
      if (   (bufferSize > 0L)
          && (storageHandle->webdav.index < storageHandle->webdav.size)
         )
      {
        assert(storageHandle->webdav.index >= (storageHandle->webdav.receiveBuffer.offset+storageHandle->webdav.receiveBuffer.length));

        // get max. number of bytes to receive in one step
        if (storageHandle->storageInfo->webdav.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(storageHandle->storageInfo->webdav.bandWidthLimiter.blockSize,bufferSize);
        }
        else
        {
          length = bufferSize;
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
          waitCurlSocketRead(storageHandle->webdav.curlMultiHandle);
          if (error != ERROR_NONE)
          {
            break;
          }

          // perform curl action
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
            error = ERRORX_(NETWORK_RECEIVE,0,"%s",curl_multi_strerror(curlmCode));
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
              error = ERRORX_(NETWORK_RECEIVE,0,"%s",curl_easy_strerror(curlMsg[i].data.result));
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

        // adjust buffer, bufferSize, bytes read, index
        buffer = (byte*)buffer+bytesAvail;
        bufferSize -= bytesAvail;
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
          SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            limitBandWidth(&storageHandle->storageInfo->webdav.bandWidthLimiter,
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
    UNUSED_VARIABLE(bufferSize);
    UNUSED_VARIABLE(bytesRead);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageWebDAV_write(StorageHandle *storageHandle,
                                 const void    *buffer,
                                 ulong         bufferLength
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
  #ifdef HAVE_CURL
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->webdav);
  #endif /* HAVE_CURL */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(buffer != NULL);

  error = ERROR_NONE;
  #ifdef HAVE_CURL
    assert(storageHandle->webdav.curlMultiHandle != NULL);

    writtenBytes = 0L;
    while (writtenBytes < bufferLength)
    {
      // get max. number of bytes to send in one step
      if (storageHandle->storageInfo->webdav.bandWidthLimiter.maxBandWidthList != NULL)
      {
        length = MIN(storageHandle->storageInfo->webdav.bandWidthLimiter.blockSize,bufferLength-writtenBytes);
      }
      else
      {
        length = bufferLength-writtenBytes;
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
        waitCurlSocketWrite(storageHandle->webdav.curlMultiHandle);
        if (error != ERROR_NONE)
        {
          break;
        }

        // perform curl action
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
          error = ERRORX_(NETWORK_SEND,0,"%s",curl_multi_strerror(curlmCode));
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
//fprintf(stderr,"%s, %d: sent %d\n",__FILE__,__LINE__,storageHandle->webdav.sendBuffer.length);
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
        SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          limitBandWidth(&storageHandle->storageInfo->webdav.bandWidthLimiter,
                         storageHandle->webdav.sendBuffer.length,
                         endTimestamp-startTimestamp
                        );
        }
      }
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferLength);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL uint64 StorageWebDAV_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  #ifdef HAVE_CURL
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->webdav);
  #endif /* HAVE_CURL */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  size = 0LL;
  #ifdef HAVE_CURL
    size = storageHandle->webdav.size;
  #else /* not HAVE_CURL */
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
  #ifdef HAVE_CURL
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->webdav);
  #endif /* HAVE_CURL */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  #ifdef HAVE_CURL
    (*offset) = storageHandle->webdav.index;
    error     = ERROR_NONE;
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
  #ifdef HAVE_CURL
    DEBUG_CHECK_RESOURCE_TRACE(&storageHandle->webdav);
  #endif /* HAVE_CURL */
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

  error = ERROR_NONE;
  #ifdef HAVE_CURL
    assert(storageHandle->webdav.receiveBuffer.data != NULL);

    if      (offset > storageHandle->webdav.index)
    {
      // seek forward

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
          if (storageHandle->storageInfo->webdav.bandWidthLimiter.maxBandWidthList != NULL)
          {
            length = MIN(storageHandle->storageInfo->webdav.bandWidthLimiter.blockSize,MIN(skip,MAX_BUFFER_SIZE));
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
            waitCurlSocketRead(storageHandle->webdav.curlMultiHandle);
            if (error != ERROR_NONE)
            {
              break;
            }

            // perform curl action
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
              error = ERRORX_(NETWORK_RECEIVE,0,"%s",curl_multi_strerror(curlmCode));
            }
          }
          if (error != ERROR_NONE)
          {
            break;
          }
          if (storageHandle->webdav.receiveBuffer.length < length)
          {
            error = ERROR_IO;
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
            SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
            {
              limitBandWidth(&storageHandle->storageInfo->webdav.bandWidthLimiter,
                             length,
                             endTimestamp-startTimestamp
                            );
            }
          }
        }
      }
    }
    else if (   (offset >= storageHandle->webdav.receiveBuffer.offset)
             && (offset < storageHandle->webdav.receiveBuffer.offset+(uint64)storageHandle->webdav.receiveBuffer.length)
            )
    {
      // seek inside receive buffer (backward/forward)
      storageHandle->webdav.index = offset;
    }
    else if (offset < storageHandle->webdav.index)
    {
      // seek backward (not supported)
      error = ERROR_FUNCTION_NOT_SUPPORTED;
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageWebDAV_rename(const StorageInfo *storageInfo,
                                  ConstString       fromArchiveName,
                                  ConstString       toArchiveName
                                 )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);

UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(fromArchiveName);
UNUSED_VARIABLE(toArchiveName);
error = ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

LOCAL Errors StorageWebDAV_delete(const StorageInfo *storageInfo,
                                  ConstString       archiveName
                                 )
{
  Errors      error;
  #ifdef HAVE_CURL
    CURL            *curlHandle;
    String          baseURL;
    CURLcode        curlCode;
    String          directoryName,baseName;
    String          url;
    StringTokenizer nameTokenizer;
    ConstString     token;
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(!String_isEmpty(archiveName));

  error = ERROR_UNKNOWN;
  #ifdef HAVE_CURL
    // initialize variables
    curlHandle = curl_easy_init();
    if (curlHandle != NULL)
    {
      // get base URL
      baseURL = String_format(String_new(),"http://%S",storageInfo->storageSpecifier.hostName);
      if (storageInfo->storageSpecifier.hostPort != 0) String_appendFormat(baseURL,":d",storageInfo->storageSpecifier.hostPort);

      // get directory name, base name
      directoryName = File_getDirectoryName(String_new(),archiveName);
      baseName      = File_getBaseName(String_new(),archiveName);

      // get URL
      url = String_duplicate(baseURL);
      String_appendFormat(url,"/");
      File_initSplitFileName(&nameTokenizer,directoryName);
      while (File_getNextSplitFileName(&nameTokenizer,&token))
      {
        String_append(url,token);
        String_appendChar(url,'/');
      }
      File_doneSplitFileName(&nameTokenizer);
      String_append(url,baseName);

      // init WebDAV login
      curlCode = setWebDAVLogin(curlHandle,
                                storageInfo->storageSpecifier.loginName,
                                storageInfo->storageSpecifier.loginPassword,
                                WEBDAV_TIMEOUT
                               );
      if (curlCode != CURLE_OK)
      {
        String_delete(url);
        String_delete(baseName);
        String_delete(directoryName);
        String_delete(baseURL);
        (void)curl_easy_cleanup(curlHandle);
        return ERRORX_(WEBDAV_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
      }

      // delete file
      if (deleteFileDirectory(curlHandle,url))
      {
        error = ERROR_NONE;
      }
      else
      {
        error = ERRORX_(DELETE_FILE,0,"%s",curl_easy_strerror(curlCode));
      }

      // free resources
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(curlHandle);
    }
    else
    {
      error = ERROR_WEBDAV_SESSION_FAIL;
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
  assert(error != ERROR_UNKNOWN);

  return error;
}

#if 0
still not complete
LOCAL Errors StorageWebDAV_getInfo(const StorageInfo *storageInfo,
                                   ConstString       fileName,
                                   FileInfo          *fileInfo
                                  )
{
  String infoFileName;
  Errors error;
  #ifdef HAVE_CURL
    Server            server;
    CURL              *curlHandle;
    String            baseURL;
    CURLcode          curlCode;
    String            directoryName,baseName;
    String            url;
    StringTokenizer   nameTokenizer;
    ConstString       token;
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV);
  assert(fileInfo != NULL);

  infoFileName = (fileName != NULL) ? fileName : storageInfo->storageSpecifier.archiveName;
  memClear(fileInfo,sizeof(fileInfo));

  error = ERROR_UNKNOWN;
  #ifdef HAVE_CURL
    // get WebDAV server settings
    getWebDAVServerSettings(storageInfo->storageSpecifier.hostName,storageInfo->jobOptions,&server);
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_set(storageInfo->storageSpecifier.loginName,webDAVServer.loginName);
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("USER"));
    if (String_isEmpty(storageInfo->storageSpecifier.hostName))
    {
      return ERROR_NO_HOST_NAME;
    }

    // allocate webDAV server
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
    baseURL = String_format(String_new(),"http://%S",storageInfo->storageSpecifier.hostName);
    if (storageInfo->storageSpecifier.hostPort != 0) String_appendFormat(baseURL,":d",storageInfo->storageSpecifier.hostPort);

    // get directory name, base name
    directoryName = File_getDirectoryNameCString(String_new(),infoFileName);
    baseName      = File_getBaseName(String_new(),infoFileName);

    // get URL
    url = String_duplicate(baseURL);
    String_appendFormat(url,"/");
    File_initSplitFileName(&nameTokenizer,directoryName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_append(url,token);
      String_appendChar(url,'/');
    }
    File_doneSplitFileName(&nameTokenizer);
    String_append(url,baseName);

fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__);
    // init WebDAV login
    curlCode = setWebDAVLogin(curlHandle,
                              storageInfo->storageSpecifier.loginName,
                              storageInfo->storageSpecifier.loginPassword,
                              WEBDAV_TIMEOUT
                             );
    if (curlCode != CURLE_OK)
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(curlHandle);
      freeServer(&server);
      return ERRORX_(WEBDAV_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // get file info
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
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
      error = ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(url));
    }

    // free resources
    String_delete(url);
    String_delete(baseName);
    String_delete(directoryName);
    String_delete(baseURL);
    (void)curl_easy_cleanup(curlHandle);
    freeServer(&server);
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
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
                                             ConstString                pathName,
                                             const JobOptions           *jobOptions,
                                             ServerConnectionPriorities serverConnectionPriority
                                            )
{
  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    Errors            error;
    AutoFreeList      autoFreeList;
    WebDAVServer      webDAVServer;
    CURL              *curlHandle;
    String            url;
    CURLcode          curlCode;
    StringTokenizer   nameTokenizer;
    ConstString       token;
    String            directoryData;
    struct curl_slist *curlSList;
  #endif /* defined(HAVE_CURL) && defined(HAVE_MXML) */

//fprintf(stderr,"%s, %d: \n",__FILE__,__LINE__); __B();
  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_WEBDAV);
  assert(pathName != NULL);
  assert(jobOptions != NULL);

  UNUSED_VARIABLE(storageSpecifier);
  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif /* defined(HAVE_CURL) && defined(HAVE_MXML) */

  // open directory listing
  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    // init variables
    AutoFree_init(&autoFreeList);
    storageDirectoryListHandle->type               = STORAGE_TYPE_WEBDAV;
    storageDirectoryListHandle->webdav.rootNode    = NULL;
    storageDirectoryListHandle->webdav.lastNode    = NULL;
    storageDirectoryListHandle->webdav.currentNode = NULL;

    // get WebDAV server settings
    storageDirectoryListHandle->webdav.serverId = Configuration_initWebDAVServerSettings(&webDAVServer,storageDirectoryListHandle->storageSpecifier.hostName,jobOptions);
    AUTOFREE_ADD(&autoFreeList,&webDAVServer,{ Configuration_doneWebDAVServerSettings(&webDAVServer); });
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_set(storageDirectoryListHandle->storageSpecifier.loginName,webDAVServer.loginName);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("USER"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate WebDAV server
    if (!allocateServer(storageDirectoryListHandle->webdav.serverId,serverConnectionPriority,60*1000L))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->webdav.serverId,{ freeServer(storageDirectoryListHandle->webdav.serverId); });

    // check WebDAV login, get correct password
    error = ERROR_WEBDAV_SESSION_FAIL;
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL) && !Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword))
    {
      error = checkWebDAVLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                               storageDirectoryListHandle->storageSpecifier.loginName,
                               storageDirectoryListHandle->storageSpecifier.loginPassword
                              );
    }
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL) && !Password_isEmpty(&webDAVServer.password))
    {
      error = checkWebDAVLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                               storageDirectoryListHandle->storageSpecifier.loginName,
                               &webDAVServer.password
                              );
      if (error == ERROR_NONE)
      {
        Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,&webDAVServer.password);
      }
    }
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL) && !Password_isEmpty(defaultWebDAVPassword))
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
    if (Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL)
    {
      // initialize default password
      while (   (Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL)
             && initWebDAVLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                                storageDirectoryListHandle->storageSpecifier.loginName,
                                storageDirectoryListHandle->storageSpecifier.loginPassword,
                                jobOptions,
//TODO
                                CALLBACK_(NULL,NULL) //storageDirectoryListHandle->getNamePasswordFunction,
                               )
            )
      {
        error = checkWebDAVLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                                 storageDirectoryListHandle->storageSpecifier.loginName,
                                 storageDirectoryListHandle->storageSpecifier.loginPassword
                                );
      }
    }
    if (error != ERROR_NONE)
    {
      error = (   !Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword)
               || !Password_isEmpty(&webDAVServer.password)
               || !Password_isEmpty(defaultWebDAVPassword))
                ? ERRORX_(INVALID_WEBDAV_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName))
                : ERRORX_(NO_WEBDAV_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName));
    }

    // store password as default webDAV password
    if (error == ERROR_NONE)
    {
      if (defaultWebDAVPassword == NULL) defaultWebDAVPassword = Password_new();
      Password_set(defaultWebDAVPassword,storageDirectoryListHandle->storageSpecifier.loginPassword);
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
    if (storageDirectoryListHandle->storageSpecifier.hostPort != 0) String_appendFormat(url,":d",storageDirectoryListHandle->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,pathName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    File_doneSplitFileName(&nameTokenizer);
    String_appendChar(url,'/');

    // init WebDAV login
    curlCode = setWebDAVLogin(curlHandle,
                              storageDirectoryListHandle->storageSpecifier.loginName,
                              storageDirectoryListHandle->storageSpecifier.loginPassword,
                              WEBDAV_TIMEOUT
                             );
    if (curlCode != CURLE_OK)
    {
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      AutoFree_cleanup(&autoFreeList);
      return ERRORX_(WEBDAV_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // read directory data
    directoryData = String_new();
    curlSList = curl_slist_append(NULL,"Depth: 1");
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
    }
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
      error = getResponseError(curlHandle);
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
    AUTOFREE_ADD(&autoFreeList,storageDirectoryListHandle->webdav.rootNode,{ mxmlDelete(storageDirectoryListHandle->webdav.rootNode); });

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
    Configuration_doneWebDAVServerSettings(&webDAVServer);
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not defined(HAVE_CURL) && defined(HAVE_MXML) */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(pathName);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* defined(HAVE_CURL) && defined(HAVE_MXML) */
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
        && (mxmlGetType(storageDirectoryListHandle->webdav.currentNode) == MXML_ELEMENT)
        && (mxmlGetFirstChild(storageDirectoryListHandle->webdav.currentNode) != NULL)
        && (mxmlGetType(mxmlGetFirstChild(storageDirectoryListHandle->webdav.currentNode)) == MXML_OPAQUE)
        && (mxmlGetUserData(mxmlGetFirstChild(storageDirectoryListHandle->webdav.currentNode)) != NULL)
       )
    {
      // get file name
      String_setCString(fileName,mxmlGetUserData(mxmlGetFirstChild(storageDirectoryListHandle->webdav.currentNode)));

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
            && (mxmlGetType(node) == MXML_ELEMENT)
            && (mxmlGetFirstChild(node) != NULL)
            && (mxmlGetType(mxmlGetFirstChild(node)) == MXML_OPAQUE)
            && (mxmlGetUserData(mxmlGetFirstChild(node)) != NULL)
           )
        {
          fileInfo->size = strtol(mxmlGetUserData(mxmlGetFirstChild(node)),NULL,10);
        }
        node = mxmlFindElement(storageDirectoryListHandle->webdav.currentNode,
                               storageDirectoryListHandle->webdav.rootNode,
                               "lp1:getlastmodified",
                               NULL,
                               NULL,
                               MXML_DESCEND
                              );
        if (   (node != NULL)
            && (mxmlGetType(node) == MXML_ELEMENT)
            && (mxmlGetFirstChild(node) != NULL)
            && (mxmlGetType(mxmlGetFirstChild(node)) == MXML_OPAQUE)
            && (mxmlGetUserData(mxmlGetFirstChild(node)) != NULL)
           )
        {
          fileInfo->timeModified = Misc_parseDateTime(mxmlGetUserData(mxmlGetFirstChild(node)));
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
