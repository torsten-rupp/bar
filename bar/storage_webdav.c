/***********************************************************************\
*
* Contents: storage WebDAV functions
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

#include "bar.h"
#include "bar_common.h"
#include "errors.h"
#include "crypt.h"
#include "archive.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
// file data buffer size
#define BUFFER_SIZE (64*1024)

// different timeouts [ms]
#define WEBDAV_TIMEOUT (10*1000)

#define INITIAL_BUFFER_SIZE   (64*1024)
#define INCREMENT_BUFFER_SIZE ( 8*1024)
#define MAX_BUFFER_SIZE       (64*1024)
#define MAX_FILENAME_LENGTH   ( 8*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#ifdef HAVE_CURL
  LOCAL Password defaultWebDAVPassword;
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
            if (Password_isEmpty(&defaultWebDAVPassword))
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
              Password_set(loginPassword,&defaultWebDAVPassword);
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
* Input  : curlHandle       - CURL handle
*          loginName        - login name
*          loginPassword    - login password
*          publicKey        - SSH public key
*          publicKeyLength  - SSH public key length
*          privateKey       - SSH private key
*          privateKeyLength - SSH private key length
*          timeout          - timeout [ms]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors setWebDAVLogin(CURL        *curlHandle,
                            ConstString loginName,
                            Password    *loginPassword,
                            const void  *publicKeyData,
                            uint        publicKeyLength,
                            const void  *privateKeyData,
                            uint        privateKeyLength,
                            long        timeout
                           )
{
  CURLcode         curlCode;
  struct curl_blob curlBLOB;

// TODO: use
UNUSED_VARIABLE(privateKeyData);
UNUSED_VARIABLE(privateKeyLength);

  // reset
  curl_easy_reset(curlHandle);

  curlCode = CURLE_OK;

  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_FAILONERROR,1L);
  }
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_TIMEOUT_MS,timeout);
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
  if (curlCode == CURLE_OK)
  {
// TODO:
    curlBLOB.data  = (void*)publicKeyData;
    curlBLOB.len   = publicKeyLength;
    curlBLOB.flags = 0;
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_CAINFO_BLOB,&curlBLOB);
  }
// TODO: option
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_SSL_VERIFYPEER,0L);
  }
// TODO: option
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_SSL_VERIFYHOST,0L);
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

  (void)curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "BAR/" VERSION_STRING);

  return (curlCode == CURLE_OK)
    ? ERROR_NONE
    : ERRORX_(WEBDAV_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
}

/***********************************************************************\
* Name   : getWebDAVURL
* Purpose: get WebDAV url
* Input  : type      - storage type
*          hostName  - host name
*          hostPort  - host port
*          pathName  - path name or NULL
* Output : -
* Return : URL
* Notes  : -
\***********************************************************************/

LOCAL String getWebDAVURL(StorageTypes type,
                          ConstString  hostName,
                          uint         hostPort,
                          ConstString  pathName
                         )
{
  String          url;
  StringTokenizer nameTokenizer;
  ConstString     token;

  url = String_new();
  switch (type)
  {
    case STORAGE_TYPE_WEBDAV:  String_format(url,"http://%S", hostName); break;
    case STORAGE_TYPE_WEBDAVS: String_format(url,"https://%S",hostName); break;
    default:
      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      break;
  }
  if (hostPort != 0) String_appendFormat(url,":%d",hostPort);
  if (pathName != NULL)
  {
    File_initSplitFileName(&nameTokenizer,pathName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      if (!String_isEmpty(token))
      {
        String_appendChar(url,'/');
        String_append(url,token);
      }
    }
    File_doneSplitFileName(&nameTokenizer);
  }

  return url;
}

/***********************************************************************\
* Name   : checkWebDAVLogin
* Purpose: check if WebDAV login is possible
* Input  : type             - storage type
*          hostName         - host name
*          hostPort         - host port
*          loginName        - login name
*          loginPassword    - login password
*          publicKey        - SSH public key
*          publicKeyLength  - SSH public key length
*          privateKey       - SSH private key
*          privateKeyLength - SSH private key length
*          pathName         - path name or NULL
* Output : -
* Return : ERROR_NONE if login is possible, error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors checkWebDAVLogin(StorageTypes type,
                              ConstString  hostName,
                              uint         hostPort,
                              ConstString  loginName,
                              Password     *loginPassword,
                              const void   *publicKeyData,
                              uint         publicKeyLength,
                              const void   *privateKeyData,
                              uint         privateKeyLength,
                              ConstString  fileName
                             )
{
  CURL              *curlHandle;
  String            url;
  CURLcode          curlCode;
  Errors            error;
  StringTokenizer   stringTokenizer;
  bool              doneFlag;
  String            pathName;
  ConstString       token;
  struct curl_slist *curlSList;


  assert(   (type == STORAGE_TYPE_WEBDAV)
         || (type == STORAGE_TYPE_WEBDAVS)
        );
  assert(hostName != NULL);

  // init handle
  curlHandle = curl_easy_init();
  if (curlHandle == NULL)
  {
    return ERROR_WEBDAV_SESSION_FAIL;
  }

  File_initSplitFileName(&stringTokenizer,fileName);
  doneFlag  = FALSE;
  curlSList = curl_slist_append(NULL,"Depth: 1");
  pathName  = String_new();
  do
  {
    // get URL
    url = getWebDAVURL(type,hostName,hostPort,pathName);

    // init WebDAV login
    error = setWebDAVLogin(curlHandle,
                           loginName,
                           loginPassword,
                           publicKeyData,
                           publicKeyLength,
                           privateKeyData,
                           privateKeyLength,
                           WEBDAV_TIMEOUT
                          );

    // check access via URL
    if (error == ERROR_NONE)
    {
      curlCode = CURLE_OK;

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
        curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
      }
      if (curlCode == CURLE_OK)
      {
        curlCode = curl_easy_perform(curlHandle);
      }
      if (curlCode != CURLE_OK)
      {
        switch (curlCode)
        {
          case CURLE_COULDNT_CONNECT: error = ERRORX_(CONNECT_FAIL,0,"%s",curl_easy_strerror(curlCode)); break;
          default:                    error = ERRORX_(WEBDAV_AUTHENTICATION,0,"%s",curl_easy_strerror(curlCode)); break;
        }
      }
    }

    String_delete(url);

    // next directory
    if (File_getNextSplitFileName(&stringTokenizer,&token))
    {
      File_appendFileName(pathName,token);
    }
    else
    {
      doneFlag = TRUE;
    }
  }
  while ((error != ERROR_NONE) && !doneFlag);
  String_delete(pathName);
  curl_slist_free_all(curlSList);
  if (error != ERROR_NONE)
  {
    File_doneSplitFileName(&stringTokenizer);
    (void)curl_easy_cleanup(curlHandle);
    return error;
  }
  File_doneSplitFileName(&stringTokenizer);

  // free resources
  (void)curl_easy_cleanup(curlHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : fileExists
* Purpose: check if file exists
* Input  : curlHandle - CURL handle
*          url        - URL
* Output : -
* Return : TRUE iff file exists
* Notes  : -
\***********************************************************************/

LOCAL bool fileExists(CURL *curlHandle, ConstString url)
{
  CURLcode curlCode;
  long     responseCode;

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
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_getinfo(curlHandle,CURLINFO_RESPONSE_CODE,&responseCode);
  }
//fprintf(stderr,"%s:%d: file exists %s: %ld %s\n",__FILE__,__LINE__,String_cString(url),responseCode,(responseCode == HTTP_CODE_OK) ? "yes" : "no");

  return (curlCode == CURLE_OK) && (responseCode == HTTP_CODE_OK);
}

/***********************************************************************\
* Name   : directoryExists
* Purpose: check if directory exists
* Input  : curlHandle - CURL handle
*          url        - URL
* Output : -
* Return : TRUE iff directory exists
* Notes  : -
\***********************************************************************/

LOCAL bool directoryExists(CURL *curlHandle, ConstString url)
{
  String   directoryURL;
  CURLcode curlCode;
  long     responseCode;

  assert(curlHandle != NULL);
  assert(url != NULL);

  directoryURL = String_appendChar(String_duplicate(url),'/');
  curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(directoryURL));
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
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_getinfo(curlHandle,CURLINFO_RESPONSE_CODE,&responseCode);
  }
//fprintf(stderr,"%s:%d: directory exists %s: %ld\n",__FILE__,__LINE__,String_cString(directoryURL),responseCode,(responseCode == HTTP_CODE_OK) ? "yes" : "no");

  // free resources
  String_delete(directoryURL);

  return (curlCode == CURLE_OK) && (responseCode == HTTP_CODE_OK);
}

/***********************************************************************\
* Name   : makeDirectory
* Purpose: make directory
* Input  : curlHandle - CURL handle
*          url        - URL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors makeDirectory(CURL *curlHandle, ConstString url)
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

  return (curlCode == CURLE_OK)
    ? ERROR_NONE
    : getCurlHTTPResponseError(curlHandle,url);
}

/***********************************************************************\
* Name   : fileDirectoryDelete
* Purpose: delete file/directory
* Input  : curlHandle - CURL handle
*          url        - URL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors deleteFileDirectory(CURL *curlHandle, ConstString url)
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

  return (curlCode == CURLE_OK)
    ? ERROR_NONE
    : getCurlHTTPResponseError(curlHandle,url);
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
  assert(   (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
  assert(storageHandle->webdav.sendBuffer.data != NULL);

//TODO: progress callback?

  if (storageHandle->webdav.sendBuffer.index < storageHandle->webdav.sendBuffer.length)
  {
    bytesSent = MIN(n,(size_t)(storageHandle->webdav.sendBuffer.length-storageHandle->webdav.sendBuffer.index)/size)*size;

    memCopyFast(buffer,
                bytesSent,
                storageHandle->webdav.sendBuffer.data+storageHandle->webdav.sendBuffer.index,
                bytesSent
               );
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
  assert(   (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

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
  memCopyFast(storageHandle->webdav.receiveBuffer.data+storageHandle->webdav.receiveBuffer.length,
              bytesReceived,
              buffer,
              bytesReceived
             );
  storageHandle->webdav.receiveBuffer.length += (ulong)bytesReceived;
#if 0
static size_t totalReceived = 0;
totalReceived+=bytesReceived;
fprintf(stderr,"%s, %d: storageHandle->webdav.receiveBuffer.length=%d bytesReceived=%d %d\n",__FILE__,__LINE__,storageHandle->webdav.receiveBuffer.length,bytesReceived,totalReceived);
#if 0
fprintf(stderr,"%s:%d: storageHandle->webdav.receiveBuffer.length=%lld\n",__FILE__,__LINE__,storageHandle->webdav.receiveBuffer.length);
debugDumpMemory(storageHandle->webdav.receiveBuffer.data,storageHandle->webdav.receiveBuffer.length,1);
#endif
#endif

  return bytesReceived;
}

/***********************************************************************\
* Name   : initDownload
* Purpose: init WebDAV download
* Input  : storageHandle - storage handle
*          url           - URL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors initDownload(StorageHandle *storageHandle,
                          ConstString   url
                         )
{
  CURLcode  curlCode;
  CURLMcode curlmCode;
  int       runningHandles;
  Errors    error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(url != NULL);

  curlCode = CURLE_OK;

  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_URL,String_cString(url));
  }
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
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
    return getCurlHTTPResponseError(storageHandle->webdav.curlHandle,storageHandle->storageInfo->storageSpecifier.archiveName);
  }

  // add handle
  curlmCode = curl_multi_add_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
  if (curlmCode != CURLM_OK)
  {
    return ERRORX_(NETWORK_RECEIVE,0,"%s",curl_multi_strerror(curlmCode));
  }

  // start WebDAV download
  do
  {
    curlmCode = curl_multi_perform(storageHandle->webdav.curlMultiHandle,&runningHandles);
  }
  while (   (curlmCode == CURLM_CALL_MULTI_PERFORM)
         && (runningHandles > 0)
        );
  if (curlmCode != CURLM_OK)
  {
    error = ERRORX_(WEBDAV_SESSION_FAIL,0,"%s",curl_multi_strerror(curlmCode));
  }
  else
  {
    error = getCurlHTTPResponseError(storageHandle->webdav.curlHandle,storageHandle->storageInfo->storageSpecifier.archiveName);
  }

  return error;
}

/***********************************************************************\
* Name   : initUpload
* Purpose: init WebDAV upload
* Input  : storageHandle - storage handle
*          url           - URL
*          fileSize      - file size [bytes]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors initUpload(StorageHandle *storageHandle,
                        ConstString   url,
                        uint64        fileSize
                       )
{
  CURLcode  curlCode;
  CURLMcode curlmCode;
  int       runningHandles;
  Errors    error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(url != NULL);

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
    curlCode = curl_easy_setopt(storageHandle->webdav.curlHandle,CURLOPT_INFILESIZE_LARGE,(curl_off_t)fileSize);
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
    return getCurlHTTPResponseError(storageHandle->webdav.curlHandle,storageHandle->storageInfo->storageSpecifier.archiveName);
  }

  // add handle
  curlmCode = curl_multi_add_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
  if (curlmCode != CURLM_OK)
  {
    return ERRORX_(NETWORK_RECEIVE,0,"%s",curl_multi_strerror(curlmCode));
  }

  // start WebDAV upload
  do
  {
    curlmCode = curl_multi_perform(storageHandle->webdav.curlMultiHandle,&runningHandles);
  }
  while (   (curlmCode == CURLM_CALL_MULTI_PERFORM)
         && (runningHandles > 0)
        );
  if (curlmCode == CURLM_OK)
  {
    error = ERROR_NONE;
  }
  else
  {
    error = getCurlHTTPResponseError(storageHandle->webdav.curlHandle,storageHandle->storageInfo->storageSpecifier.archiveName);
  }

  return error;
}

/***********************************************************************\
* Name   : doneDownloadUpload
* Purpose: done download/upload
* Input  : storageHandle - storage handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneDownloadUpload(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->webdav.curlHandle != NULL);
  assert(storageHandle->webdav.curlMultiHandle != NULL);

  UNUSED_VARIABLE(storageHandle);
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

/***********************************************************************\
* Name   : StorageWebDAV_initAll
* Purpose: initialize WebDAV storage
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageWebDAV_initAll(void)
{
  Errors error;

  error = ERROR_NONE;

  #ifdef HAVE_CURL
    Password_init(&defaultWebDAVPassword);
  #endif /* HAVE_CURL */

  return error;
}

/***********************************************************************\
* Name   : StorageWebDAV_doneAll
* Purpose: deinitialize WebDAV storage
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void StorageWebDAV_doneAll(void)
{
  #ifdef HAVE_CURL
    Password_done(&defaultWebDAVPassword);
  #endif /* HAVE_CURL */
}

/***********************************************************************\
* Name   : StorageSFTP_parseSpecifier
* Purpose: parse SFTP specifier
* Input  : sftpSpecifier - SFTP specifier
* Output : hostName      - host name
*          hostPort      - host port
*          loginName     - login name
*          loginPassword - login password
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

LOCAL bool StorageWebDAV_parseSpecifier(ConstString webdavSpecifier,
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

  assert(webdavSpecifier != NULL);
  assert(hostName != NULL);
  assert(loginName != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(loginName);
  if (loginPassword != NULL) Password_clear(loginPassword);

  s = String_new();
  t = String_new();
  if      (String_matchCString(webdavSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,STRING_NO_ASSIGN,loginName,s,STRING_NO_ASSIGN,hostName,t,NULL))
  {
    // <login name>:<login password>@<host name>:<host port>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (loginPassword != NULL) Password_setString(loginPassword,s);
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(t,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(webdavSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,loginName,s,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>:<login password>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (loginPassword != NULL) Password_setString(loginPassword,s);

    result = TRUE;
  }
  else if (String_matchCString(webdavSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^:]+?):(\\d*)/{0,1}$",NULL,STRING_NO_ASSIGN,loginName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    if (loginName != NULL) String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (String_matchCString(webdavSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,loginName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);

    result = TRUE;
  }
  else if (String_matchCString(webdavSpecifier,STRING_BEGIN,"^([^:]+?):(\\d*)/{0,1}$",NULL,STRING_NO_ASSIGN,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <host name>:<host port>
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

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
  String_delete(t);
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
  assert(   (storageSpecifier1->type == STORAGE_TYPE_WEBDAV)
         || (storageSpecifier1->type == STORAGE_TYPE_WEBDAVS)
        );
  assert(storageSpecifier2 != NULL);
  assert(   (storageSpecifier2->type == STORAGE_TYPE_WEBDAV)
         || (storageSpecifier2->type == STORAGE_TYPE_WEBDAVS)
        );

  if (archiveName1 == NULL) archiveName1 = storageSpecifier1->archiveName;
  if (archiveName2 == NULL) archiveName2 = storageSpecifier2->archiveName;

  return    (storageSpecifier1->type == storageSpecifier2->type)
         && String_equals(storageSpecifier1->hostName,storageSpecifier2->hostName)
         && String_equals(archiveName1,archiveName2);
}

LOCAL String StorageWebDAV_getName(String                 string,
                                   const StorageSpecifier *storageSpecifier,
                                   ConstString            archiveName
                                  )
{
  ConstString storageFileName;

  assert(storageSpecifier != NULL);
  assert(  (storageSpecifier->type == STORAGE_TYPE_WEBDAV)
         ||(storageSpecifier->type == STORAGE_TYPE_WEBDAVS)
        );

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
    case STORAGE_TYPE_WEBDAV:  String_appendCString(string,"webdav://");  break;
    case STORAGE_TYPE_WEBDAVS: String_appendCString(string,"webdavs://"); break;
    default:
      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      break;
  }
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
  if (storageSpecifier->hostPort != 0) String_appendFormat(string,":%d",storageSpecifier->hostPort);
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
  assert(   (storageSpecifier->type == STORAGE_TYPE_WEBDAV)
         || (storageSpecifier->type == STORAGE_TYPE_WEBDAVS)
        );

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

  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_WEBDAV:  String_appendCString(string,"webdav://");  break;
    case STORAGE_TYPE_WEBDAVS: String_appendCString(string,"webdavs://"); break;
    default:
      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      break;
  }
  if (!String_isEmpty(storageSpecifier->loginName))
  {
    String_append(string,storageSpecifier->loginName);
    String_appendChar(string,'@');
  }
  String_append(string,storageSpecifier->hostName);
  if (storageSpecifier->hostPort != 0) String_appendFormat(string,":%d",storageSpecifier->hostPort);
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
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

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
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_WEBDAV:
        storageInfo->webdav.serverId = Configuration_initWebDAVServerSettings(&webDAVServer,storageInfo->storageSpecifier.hostName,jobOptions);
        AUTOFREE_ADD(&autoFreeList,&webDAVServer,{ Configuration_doneWebDAVServerSettings(&webDAVServer); });
        break;
      case STORAGE_TYPE_WEBDAVS:
        storageInfo->webdav.serverId = Configuration_initWebDAVSServerSettings(&webDAVServer,storageInfo->storageSpecifier.hostName,jobOptions);
        AUTOFREE_ADD(&autoFreeList,&webDAVServer,{ Configuration_doneWebDAVSServerSettings(&webDAVServer); });
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; // not reached
    }
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_set(storageInfo->storageSpecifier.loginName,webDAVServer.loginName);
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("USER"));
    if (storageInfo->storageSpecifier.hostPort == 0) storageInfo->storageSpecifier.hostPort = webDAVServer.port;
    Configuration_duplicateKey(&storageInfo->webdav.publicKey, &webDAVServer.publicKey );
    Configuration_duplicateKey(&storageInfo->webdav.privateKey,&webDAVServer.privateKey);
    AUTOFREE_ADD(&autoFreeList,&storageInfo->webdav.publicKey,{ Configuration_doneKey(&storageInfo->webdav.publicKey); });
    AUTOFREE_ADD(&autoFreeList,&storageInfo->webdav.privateKey,{ Configuration_doneKey(&storageInfo->webdav.privateKey); });
    if (String_isEmpty(storageInfo->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate WebDAV server
    if (!allocateServer(storageInfo->webdav.serverId,serverConnectionPriority,ALLOCATE_SERVER_TIMEOUT))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageInfo->webdav.serverId,{ freeServer(storageInfo->webdav.serverId); });

    // check WebDAV login, get correct password
    error = ERROR_WEBDAV_AUTHENTICATION;
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_AUTHENTICATION) && !Password_isEmpty(storageInfo->storageSpecifier.loginPassword))
    {
      error = checkWebDAVLogin(storageInfo->storageSpecifier.type,
                               storageInfo->storageSpecifier.hostName,
                               storageInfo->storageSpecifier.hostPort,
                               storageInfo->storageSpecifier.loginName,
                               storageInfo->storageSpecifier.loginPassword,
                               storageInfo->webdav.publicKey.data,
                               storageInfo->webdav.publicKey.length,
                               storageInfo->webdav.privateKey.data,
                               storageInfo->webdav.privateKey.length,
                               storageSpecifier->archiveName
                              );
    }
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_AUTHENTICATION) && !Password_isEmpty(&webDAVServer.password))
    {
      error = checkWebDAVLogin(storageInfo->storageSpecifier.type,
                               storageInfo->storageSpecifier.hostName,
                               storageInfo->storageSpecifier.hostPort,
                               storageInfo->storageSpecifier.loginName,
                               &webDAVServer.password,
                               storageInfo->webdav.publicKey.data,
                               storageInfo->webdav.publicKey.length,
                               storageInfo->webdav.privateKey.data,
                               storageInfo->webdav.privateKey.length,
                               storageSpecifier->archiveName
                              );
      if (error == ERROR_NONE)
      {
        Password_set(storageInfo->storageSpecifier.loginPassword,&webDAVServer.password);
      }
    }
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_AUTHENTICATION) && !Password_isEmpty(&defaultWebDAVPassword))
    {
      error = checkWebDAVLogin(storageInfo->storageSpecifier.type,
                               storageInfo->storageSpecifier.hostName,
                               storageInfo->storageSpecifier.hostPort,
                               storageInfo->storageSpecifier.loginName,
                               &defaultWebDAVPassword,
                               storageInfo->webdav.publicKey.data,
                               storageInfo->webdav.publicKey.length,
                               storageInfo->webdav.privateKey.data,
                               storageInfo->webdav.privateKey.length,
                               storageSpecifier->archiveName
                              );
      if (error == ERROR_NONE)
      {
        Password_set(storageInfo->storageSpecifier.loginPassword,&defaultWebDAVPassword);
      }
    }
    if (Error_getCode(error) == ERROR_CODE_WEBDAV_AUTHENTICATION)
    {
      // initialize interactive/default password
      retries = 0;
      Password_init(&password);
      while ((Error_getCode(error) == ERROR_CODE_WEBDAV_AUTHENTICATION) && (retries < MAX_PASSWORD_REQUESTS))
      {
        if (initWebDAVLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.loginName,
                            &password,
                            jobOptions,
                            CALLBACK_(storageInfo->getNamePasswordFunction,storageInfo->getNamePasswordUserData)
                           )
           )
        {
          error = checkWebDAVLogin(storageInfo->storageSpecifier.type,
                                   storageInfo->storageSpecifier.hostName,
                                   storageInfo->storageSpecifier.hostPort,
                                   storageInfo->storageSpecifier.loginName,
                                   &password,
                                   storageInfo->webdav.publicKey.data,
                                   storageInfo->webdav.publicKey.length,
                                   storageInfo->webdav.privateKey.data,
                                   storageInfo->webdav.privateKey.length,
                                   storageSpecifier->archiveName
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
    if (Error_getCode(error) == ERROR_CODE_WEBDAV_AUTHENTICATION)
    {
      error = (   !Password_isEmpty(storageInfo->storageSpecifier.loginPassword)
               || !Password_isEmpty(&webDAVServer.password)
               || !Password_isEmpty(&defaultWebDAVPassword)
              )
                ? ERRORX_(INVALID_WEBDAV_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName))
                : ERRORX_(NO_WEBDAV_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
    }
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // store password as default webDAV password
    Password_set(&defaultWebDAVPassword,storageInfo->storageSpecifier.loginPassword);

    // free resources
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_WEBDAV:
        Configuration_doneWebDAVServerSettings(&webDAVServer);
        break;
      case STORAGE_TYPE_WEBDAVS:
        Configuration_doneWebDAVSServerSettings(&webDAVServer);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; // not reached
    }
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
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

  // free WebDAV server connection
  #ifdef HAVE_CURL
    Configuration_doneKey(&storageInfo->webdav.privateKey);
    Configuration_doneKey(&storageInfo->webdav.publicKey);
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
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

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
                                      ConstString       archiveName,
                                      time_t            timestamp,
                                      bool              initialFlag
                                     )
{
  Errors error;
  #ifdef HAVE_CURL
    TextMacros (textMacros,3);
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

  error = ERROR_NONE;

  #ifdef HAVE_CURL
    if (!initialFlag)
    {
      // init variables
      String directory = String_new();

      // init macros
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING("%directory",File_getDirectoryName(directory,archiveName),NULL);
        TEXT_MACRO_X_STRING("%file",     archiveName,                                 NULL);
        TEXT_MACRO_X_INT   ("%number",   storageInfo->volumeNumber,                   NULL);
      }

      // write pre-processing
      if (!String_isEmpty(globalOptions.webdav.writePreProcessCommand))
      {
        printInfo(1,"Write pre-processing...");
        error = executeTemplate(String_cString(globalOptions.webdav.writePreProcessCommand),
                                timestamp,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL)
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }

      // free resources
      String_delete(directory);
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
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
    TextMacros (textMacros,3);
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

  error = ERROR_NONE;

  #ifdef HAVE_CURL
    if (!finalFlag)
    {
      // init variables
      String directory = String_new();

      // init macros
      TEXT_MACROS_INIT(textMacros)
      {
        TEXT_MACRO_X_STRING("%directory",File_getDirectoryName(directory,archiveName),NULL);
        TEXT_MACRO_X_STRING("%file",     archiveName,                                 NULL);
        TEXT_MACRO_X_INT   ("%number",   storageInfo->volumeNumber,                   NULL);
      }

      // write post-process
      if (!String_isEmpty(globalOptions.webdav.writePostProcessCommand))
      {
        printInfo(1,"Write post-processing...");
        error = executeTemplate(String_cString(globalOptions.webdav.writePostProcessCommand),
                                timestamp,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL)
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }

      // free resources
      String_delete(directory);
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

LOCAL bool StorageWebDAV_exists(const StorageInfo *storageInfo, ConstString archiveName)
{
  bool existsFlag;
  #if   defined(HAVE_CURL)
    CURL   *curlHandle;
    String url;
    Errors error;
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
  assert(!String_isEmpty(archiveName));

  existsFlag = FALSE;

  #if   defined(HAVE_CURL)
    // open Curl handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      return ERROR_WEBDAV_SESSION_FAIL;
    }

    // get URL
    url = getWebDAVURL(storageInfo->storageSpecifier.type,
                       storageInfo->storageSpecifier.hostName,
                       storageInfo->storageSpecifier.hostPort,
                       archiveName
                      );

    // init WebDAV login
    error = setWebDAVLogin(curlHandle,
                           storageInfo->storageSpecifier.loginName,
                           storageInfo->storageSpecifier.loginPassword,
                           storageInfo->webdav.publicKey.data,
                           storageInfo->webdav.publicKey.length,
                           storageInfo->webdav.privateKey.data,
                           storageInfo->webdav.privateKey.length,
                           WEBDAV_TIMEOUT
                          );
    if (error != ERROR_NONE)
    {
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      return error;
    }

    // check if file/directory exists
    existsFlag =    fileExists(curlHandle,url)
                 || directoryExists(curlHandle,url);

    // free resources
    String_delete(url);
    (void)curl_easy_cleanup(curlHandle);
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
  #endif /* HAVE_CURL */

  return existsFlag;
}

LOCAL bool StorageWebDAV_isFile(const StorageInfo *storageInfo, ConstString archiveName)
{
  bool isFileFlag;
  #if   defined(HAVE_CURL)
    CURL   *curlHandle;
    String url;
    Errors error;
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
  assert(!String_isEmpty(archiveName));

  isFileFlag = FALSE;

  #if   defined(HAVE_CURL)
    // open Curl handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      return ERROR_WEBDAV_SESSION_FAIL;
    }

    // get URL
    url = getWebDAVURL(storageInfo->storageSpecifier.type,
                       storageInfo->storageSpecifier.hostName,
                       storageInfo->storageSpecifier.hostPort,
                       archiveName
                      );

    // init WebDAV login
    error = setWebDAVLogin(curlHandle,
                           storageInfo->storageSpecifier.loginName,
                           storageInfo->storageSpecifier.loginPassword,
                           storageInfo->webdav.publicKey.data,
                           storageInfo->webdav.publicKey.length,
                           storageInfo->webdav.privateKey.data,
                           storageInfo->webdav.privateKey.length,
                           WEBDAV_TIMEOUT
                          );
    if (error != ERROR_NONE)
    {
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      return error;
    }

    // check if file exists
    isFileFlag = fileExists(curlHandle,url);

    // free resources
    String_delete(url);
    (void)curl_easy_cleanup(curlHandle);
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
  #endif /* HAVE_CURL */

  return isFileFlag;
}

LOCAL bool StorageWebDAV_isDirectory(const StorageInfo *storageInfo, ConstString archiveName)
{
  bool isDirectoryFlag;
  #if   defined(HAVE_CURL)
    CURL   *curlHandle;
    String url;
    Errors error;
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
  assert(!String_isEmpty(archiveName));

  isDirectoryFlag = FALSE;

  #if   defined(HAVE_CURL)
    // open Curl handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      return ERROR_WEBDAV_SESSION_FAIL;
    }

    // get URL
    url = getWebDAVURL(storageInfo->storageSpecifier.type,
                       storageInfo->storageSpecifier.hostName,
                       storageInfo->storageSpecifier.hostPort,
                       archiveName
                      );

    // init WebDAV login
    error = setWebDAVLogin(curlHandle,
                           storageInfo->storageSpecifier.loginName,
                           storageInfo->storageSpecifier.loginPassword,
                           storageInfo->webdav.publicKey.data,
                           storageInfo->webdav.publicKey.length,
                           storageInfo->webdav.privateKey.data,
                           storageInfo->webdav.privateKey.length,
                           WEBDAV_TIMEOUT
                          );
    if (error != ERROR_NONE)
    {
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      return error;
    }

    // check if directory exists
    isDirectoryFlag = directoryExists(curlHandle,url);

    // free resources
    String_delete(url);
    (void)curl_easy_cleanup(curlHandle);
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
  #endif /* HAVE_CURL */

  return isDirectoryFlag;
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
    String          directoryName,baseName;
    StringTokenizer stringTokenizer;
    ConstString     token;
    Errors          error;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(   (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
  assert(!String_isEmpty(fileName));

//TODO: exists test?
UNUSED_VARIABLE(forceFlag);

  #ifdef HAVE_CURL
    // initialize variables
    storageHandle->webdav.curlMultiHandle      = NULL;
    storageHandle->webdav.curlHandle           = NULL;
    storageHandle->webdav.additionalHeader     = NULL;
    storageHandle->webdav.index                = 0LL;
    storageHandle->webdav.size                 = fileSize;
    storageHandle->webdav.receiveBuffer.data   = NULL;
    storageHandle->webdav.receiveBuffer.size   = 0LL;
    storageHandle->webdav.receiveBuffer.offset = 0LL;
    storageHandle->webdav.receiveBuffer.length = 0L;
    storageHandle->webdav.sendBuffer.data      = NULL;
    storageHandle->webdav.sendBuffer.index     = 0L;
    storageHandle->webdav.sendBuffer.length    = 0L;
  #endif /* HAVE_CURL */

  // check if file exists
  if (   !forceFlag
      && (storageHandle->storageInfo->jobOptions != NULL)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_APPEND)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_OVERWRITE)
      && StorageWebDAV_exists(storageHandle->storageInfo,fileName)
     )
  {
    return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(fileName));
  }

  #ifdef HAVE_CURL
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

    // init WebDAV login
    error = setWebDAVLogin(storageHandle->webdav.curlHandle,
                           storageHandle->storageInfo->storageSpecifier.loginName,
                           storageHandle->storageInfo->storageSpecifier.loginPassword,
                           storageHandle->storageInfo->webdav.publicKey.data,
                           storageHandle->storageInfo->webdav.publicKey.length,
                           storageHandle->storageInfo->webdav.privateKey.data,
                           storageHandle->storageInfo->webdav.privateKey.length,
                           WEBDAV_TIMEOUT
                          );
    if (error != ERROR_NONE)
    {
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      return error;
    }

    // get directory name, base name
    directoryName = File_getDirectoryName(String_new(),fileName);
    baseName      = File_getBaseName(String_new(),fileName,TRUE);

    // get base URL
    baseURL = getWebDAVURL(storageHandle->storageInfo->storageSpecifier.type,
                           storageHandle->storageInfo->storageSpecifier.hostName,
                           storageHandle->storageInfo->storageSpecifier.hostPort,
                           NULL
                          );

    // create directories if necessary
    if (!String_isEmpty(directoryName))
    {
      File_initSplitFileName(&stringTokenizer,directoryName);
      while (   File_getNextSplitFileName(&stringTokenizer,&token)
             && (error == ERROR_NONE)
            )
      {
        String_appendChar(baseURL,'/');
        String_append(baseURL,token);

        // check if sub-directory exists
        if (!directoryExists(storageHandle->webdav.curlHandle,baseURL))
        {
          // create sub-directory
          error = makeDirectory(storageHandle->webdav.curlHandle,baseURL);
        }
      }
      File_doneSplitFileName(&stringTokenizer);
      if (error != ERROR_NONE)
      {
        String_delete(baseName);
        String_delete(directoryName);
        String_delete(baseURL);
        (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
        (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
        return error;
      }
    }

    // get URL
#if 0
    String_set(storageHandle->webdav.url,baseURL);
    String_appendFormat(storageHandle->webdav.url,"/");
    File_initSplitFileName(&stringTokenizer,directoryName);
    while (File_getNextSplitFileName(&stringTokenizer,&token))
    {
      String_append(storageHandle->webdav.url,token);
      String_appendChar(storageHandle->webdav.url,'/');
    }
    File_doneSplitFileName(&stringTokenizer);
#endif
    String_appendChar(baseURL,'/');
    String_append(baseURL,baseName);

    // check to stop if exists/append/overwrite
    switch (storageHandle->storageInfo->jobOptions->archiveFileMode)
    {
      case ARCHIVE_FILE_MODE_STOP:
        // check if file exists
        if (fileExists(storageHandle->webdav.curlHandle,baseURL))
        {
          String_delete(baseName);
          String_delete(directoryName);
          String_delete(baseURL);
          (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
          (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
          return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(fileName));
        }
        break;
      case ARCHIVE_FILE_MODE_RENAME:
// TODO:
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
        break;
      case ARCHIVE_FILE_MODE_APPEND:
        // not supported - ignored
        break;
      case ARCHIVE_FILE_MODE_OVERWRITE:
        // try to delete existing file (ignore error)
        (void)deleteFileDirectory(storageHandle->webdav.curlHandle,baseURL);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }

    // init WebDAV upload
    error = initUpload(storageHandle,baseURL,fileSize);
    if (error != ERROR_NONE)
    {
      error = getCurlHTTPResponseError(storageHandle->webdav.curlHandle,storageHandle->storageInfo->storageSpecifier.archiveName);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_multi_remove_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      return error;
    }

    // free resources
    String_delete(baseName);
    String_delete(directoryName);
    String_delete(baseURL);

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
    CURLcode   curlCode;
    String     url;
    Errors     error;
    struct
    {
      double     d;
      curl_off_t i;
    } contentLength;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(   (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
  assert(!String_isEmpty(archiveName));

  #ifdef HAVE_CURL
    // initialize variables
    storageHandle->webdav.curlMultiHandle      = NULL;
    storageHandle->webdav.curlHandle           = NULL;
//    storageHandle->webdav.url                  = String_new();
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
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return ERROR_WEBDAV_SESSION_FAIL;
    }

    // set WebDAV login
    error = setWebDAVLogin(storageHandle->webdav.curlHandle,
                           storageHandle->storageInfo->storageSpecifier.loginName,
                           storageHandle->storageInfo->storageSpecifier.loginPassword,
                           storageHandle->storageInfo->webdav.publicKey.data,
                           storageHandle->storageInfo->webdav.publicKey.length,
                           storageHandle->storageInfo->webdav.privateKey.data,
                           storageHandle->storageInfo->webdav.privateKey.length,
                           WEBDAV_TIMEOUT
                          );
    if (error != ERROR_NONE)
    {
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return error;
    }

    // get base URL
    url = getWebDAVURL(storageHandle->storageInfo->storageSpecifier.type,
                       storageHandle->storageInfo->storageSpecifier.hostName,
                       storageHandle->storageInfo->storageSpecifier.hostPort,
                       archiveName
                      );

    // check if file exists
    if (!fileExists(storageHandle->webdav.curlHandle,url))
    {
      error = ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(url));
      String_delete(url);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return error;
    }

    // get file size
    #ifdef HAVE_CURLINFO_CONTENT_LENGTH_DOWNLOAD_T
      curlCode = curl_easy_getinfo(storageHandle->webdav.curlHandle,CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,&contentLength.i);
      if ((curlCode == CURLE_OK) && (contentLength.i > 0LL))
      {
        storageHandle->webdav.size = (int64)contentLength.i;
      }
      else
      {
        storageHandle->webdav.size = -1LL;
      }
    #else  // not HAVE_CURLINFO_CONTENT_LENGTH_DOWNLOAD_T
      curlCode = curl_easy_getinfo(storageHandle->webdav.curlHandle,CURLINFO_CONTENT_LENGTH_DOWNLOAD,&contentLength.d);
      if ((curlCode == CURLE_OK) && (contentLength.d > 0LL))
      {
        storageHandle->webdav.size = (int64)contentLength.d;
      }
      else
      {
        storageHandle->webdav.size = -1LL;
      }
    #endif // HAVE_CURLINFO_CONTENT_LENGTH_DOWNLOAD_T
//fprintf(stderr,"%s:%d: storageHandle->webdav.size=%lld\n",__FILE__,__LINE__,storageHandle->webdav.size);

    // init WebDAV download
    error = initDownload(storageHandle,url);
    if (error != ERROR_NONE)
    {
      String_delete(url);
      (void)curl_multi_remove_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
      (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
      (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
      free(storageHandle->webdav.receiveBuffer.data);
      return error;
    }

    // free resources
    String_delete(url);

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
  assert(storageHandle->storageInfo != NULL);
  assert(   (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

  #ifdef HAVE_CURL
    doneDownloadUpload(storageHandle);

    (void)curl_multi_remove_handle(storageHandle->webdav.curlMultiHandle,storageHandle->webdav.curlHandle);
    (void)curl_easy_cleanup(storageHandle->webdav.curlHandle);
    (void)curl_multi_cleanup(storageHandle->webdav.curlMultiHandle);
    curl_slist_free_all(storageHandle->webdav.additionalHeader);
    if (storageHandle->webdav.receiveBuffer.data != NULL) free(storageHandle->webdav.receiveBuffer.data);
//    String_delete(storageHandle->webdav.url);
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_CURL */
}

LOCAL bool StorageWebDAV_eof(StorageHandle *storageHandle)
{
  #ifdef HAVE_CURL
    int runningHandles;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(   (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

  #ifdef HAVE_CURL
    return    (   (storageHandle->webdav.index < storageHandle->webdav.receiveBuffer.offset)
               || (storageHandle->webdav.index >= (storageHandle->webdav.receiveBuffer.offset+storageHandle->webdav.receiveBuffer.length))
              )
           && (   (curl_multi_perform(storageHandle->webdav.curlMultiHandle,&runningHandles) != CURLM_OK)
               || (runningHandles < 1)
              );
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
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(   (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
  assert(buffer != NULL);

  if (bytesRead != NULL) (*bytesRead) = 0L;

  error = ERROR_NONE;
  #ifdef HAVE_CURL
    assert(storageHandle->webdav.curlMultiHandle != NULL);
    assert(storageHandle->webdav.receiveBuffer.data != NULL);

    error = ERROR_NONE;

    while (   (bufferSize > 0L)
//           && (storageHandle->webdav.index < storageHandle->webdav.size)
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
        memCopyFast(buffer,
                    bytesAvail,
                    storageHandle->webdav.receiveBuffer.data+index,
                    bytesAvail
                   );

        // adjust buffer, bufferSize, bytes read, index
        buffer = (byte*)buffer+bytesAvail;
        bufferSize -= bytesAvail;
        if (bytesRead != NULL) (*bytesRead) += bytesAvail;
        storageHandle->webdav.index += (uint64)bytesAvail;
      }

      // read rest of data
      if (   (bufferSize > 0L)
//          && (storageHandle->webdav.index < storageHandle->webdav.size)
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
          }
          while (   (curlmCode == CURLM_CALL_MULTI_PERFORM)
                 && (runningHandles > 0)
                );
          if (curlmCode != CURLM_OK)
          {
            error = ERRORX_(NETWORK_RECEIVE,0,"%s",curl_multi_strerror(curlmCode));
          }
          else
          {
            error = getCurlHTTPResponseError(storageHandle->webdav.curlHandle,storageHandle->storageInfo->storageSpecifier.archiveName);
          }
          if (error != ERROR_NONE)
          {
            break;
          }
        }
        if      (error != ERROR_NONE)
        {
          break;
        }
        else if (storageHandle->webdav.receiveBuffer.length < length)
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
        memCopyFast(buffer,
                    bytesAvail,
                    storageHandle->webdav.receiveBuffer.data,
                    bytesAvail
                   );

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

  return error;
}

LOCAL Errors StorageWebDAV_write(StorageHandle *storageHandle,
                                 const void    *buffer,
                                 ulong         bufferLength
                                )
{
  Errors error;
  #ifdef HAVE_CURL
    char      errorBuffer[CURL_ERROR_SIZE];
    ulong     writtenBytes;
    ulong     length;
    uint64    startTimestamp,endTimestamp;
    CURLMcode curlmCode;
    int       runningHandles;
  #endif /* HAVE_CURL */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(   (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
  assert(buffer != NULL);

  error = ERROR_UNKNOWN;
  #ifdef HAVE_CURL
    assert(storageHandle->webdav.curlMultiHandle != NULL);

    error = ERROR_NONE;

    // try to get error message
    stringClear(errorBuffer);
    (void)curl_easy_setopt(storageHandle->ftp.curlHandle, CURLOPT_ERRORBUFFER, errorBuffer);

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
             && (runningHandles > 0)
            )
      {
        // wait for socket
        error = waitCurlSocketWrite(storageHandle->webdav.curlMultiHandle);
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
        else
        {
          error = getCurlHTTPResponseError(storageHandle->webdav.curlHandle,storageHandle->storageInfo->storageSpecifier.archiveName);
        }
        if (error != ERROR_NONE)
        {
          break;
        }
      }
      if      (error != ERROR_NONE)
      {
        break;
      }
      else if (storageHandle->webdav.sendBuffer.index < storageHandle->webdav.sendBuffer.length)
      {
        const CURLMsg *curlMsg;
        int           n,i;

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

LOCAL int64 StorageWebDAV_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(   (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

  size = 0LL;
  #ifdef HAVE_CURL
// TODO: size==0 how to handle?
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
  assert(storageHandle->storageInfo != NULL);
  assert(   (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_UNKNOWN;
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
  assert(storageHandle->storageInfo != NULL);
  assert(   (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

  error = ERROR_UNKNOWN;
  #ifdef HAVE_CURL
    assert(storageHandle->webdav.receiveBuffer.data != NULL);

    error = ERROR_NONE;

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
            else
            {
              error = getCurlHTTPResponseError(storageHandle->webdav.curlHandle,storageHandle->storageInfo->storageSpecifier.archiveName);
            }
          }
          if      (error != ERROR_NONE)
          {
            break;
          }
          else if (storageHandle->webdav.receiveBuffer.length < length)
          {
            const CURLMsg *curlMsg;
            int           n,i;

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
// TODO:
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
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(fromArchiveName);
UNUSED_VARIABLE(toArchiveName);
error = ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

LOCAL Errors StorageWebDAV_makeDirectory(const StorageInfo *storageInfo,
                                         ConstString       directoryName
                                        )
{
  Errors      error;
  #ifdef HAVE_CURL
    CURL     *curlHandle;
    String   url;
//    CURLcode curlCode;
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
  assert(!String_isEmpty(directoryName));

  error = ERROR_UNKNOWN;
  #ifdef HAVE_CURL
    // initialize variables
    curlHandle = curl_easy_init();
    if (curlHandle != NULL)
    {
      // get base URL
      url = getWebDAVURL(storageInfo->storageSpecifier.type,
                         storageInfo->storageSpecifier.hostName,
                         storageInfo->storageSpecifier.hostPort,
                         directoryName
                        );

      // init WebDAV login
      error = setWebDAVLogin(curlHandle,
                             storageInfo->storageSpecifier.loginName,
                             storageInfo->storageSpecifier.loginPassword,
                             storageInfo->webdav.publicKey.data,
                             storageInfo->webdav.publicKey.length,
                             storageInfo->webdav.privateKey.data,
                             storageInfo->webdav.privateKey.length,
                             WEBDAV_TIMEOUT
                            );
      if (error != ERROR_NONE)
      {
        String_delete(url);
        (void)curl_easy_cleanup(curlHandle);
        return error;
      }

      // make directory
      error = makeDirectory(curlHandle,url);

      // free resources
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
    }
    else
    {
      error = ERROR_WEBDAV_SESSION_FAIL;
    }
  #else /* not HAVE_CURL */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(directoryName);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageWebDAV_delete(const StorageInfo *storageInfo,
                                  ConstString       archiveName
                                 )
{
  Errors      error;
  #ifdef HAVE_CURL
    CURL     *curlHandle;
    String   url;
  #endif /* HAVE_CURL */

  assert(storageInfo != NULL);
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
  assert(!String_isEmpty(archiveName));

  error = ERROR_UNKNOWN;
  #ifdef HAVE_CURL
    // initialize variables
    curlHandle = curl_easy_init();
    if (curlHandle != NULL)
    {
      // get base URL
      url = getWebDAVURL(storageInfo->storageSpecifier.type,
                         storageInfo->storageSpecifier.hostName,
                         storageInfo->storageSpecifier.hostPort,
                         archiveName
                        );

      // init WebDAV login
      error = setWebDAVLogin(curlHandle,
                             storageInfo->storageSpecifier.loginName,
                             storageInfo->storageSpecifier.loginPassword,
                             storageInfo->webdav.publicKey.data,
                             storageInfo->webdav.publicKey.length,
                             storageInfo->webdav.privateKey.data,
                             storageInfo->webdav.privateKey.length,
                             WEBDAV_TIMEOUT
                            );
      if (error != ERROR_NONE)
      {
        String_delete(url);
        (void)curl_easy_cleanup(curlHandle);
        return error;
      }

      // delete file
      error = deleteFileDirectory(curlHandle,url);

      // free resources
      String_delete(url);
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
  assert(   (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageInfo->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );
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
    if (!allocateServer(&server,SERVER_CONNECTION_PRIORITY_LOW,ALLOCATE_SERVER_TIMEOUT))
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
    baseURL = getWebDAVURL();
    switch (storageInfo->storageSpecifier.type)
    {
      case STORAGE_TYPE_WEBDAV:  String_format(baseURL,"http://%S", hostName); break;
      case STORAGE_TYPE_WEBDAVS: String_format(baseURL,"https://%S",hostName); break;
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break;
    }
    if (storageInfo->storageSpecifier.hostPort != 0) String_appendFormat(baseURL,":d",storageInfo->storageSpecifier.hostPort);

    // get directory name, base name
    directoryName = File_getDirectoryNameCString(String_new(),infoFileName);
    baseName      = File_getBaseName(String_new(),infoFileName,TRUE);

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
    error = setWebDAVLogin(curlHandle,
                           storageInfo->storageSpecifier.loginName,
                           storageInfo->storageSpecifier.loginPassword,
                           WEBDAV_TIMEOUT
                          );
    if (error != ERROR_NONE)
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      String_delete(baseURL);
      (void)curl_easy_cleanup(curlHandle);
      freeServer(&server);
      return error;
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
    String            directoryData;
    struct curl_slist *curlSList;
  #endif /* defined(HAVE_CURL) && defined(HAVE_MXML) */

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(   (storageSpecifier->type == STORAGE_TYPE_WEBDAV)
         || (storageSpecifier->type == STORAGE_TYPE_WEBDAVS)
        );
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
    storageDirectoryListHandle->webdav.rootNode    = NULL;
    storageDirectoryListHandle->webdav.lastNode    = NULL;
    storageDirectoryListHandle->webdav.currentNode = NULL;

    // get WebDAV server settings
    switch (storageSpecifier->type)
    {
      case STORAGE_TYPE_WEBDAV:
        storageDirectoryListHandle->webdav.serverId = Configuration_initWebDAVServerSettings(&webDAVServer,storageDirectoryListHandle->storageSpecifier.hostName,jobOptions);
        AUTOFREE_ADD(&autoFreeList,&webDAVServer,{ Configuration_doneWebDAVServerSettings(&webDAVServer); });
        break;
      case STORAGE_TYPE_WEBDAVS:
        storageDirectoryListHandle->webdav.serverId = Configuration_initWebDAVSServerSettings(&webDAVServer,storageDirectoryListHandle->storageSpecifier.hostName,jobOptions);
        AUTOFREE_ADD(&autoFreeList,&webDAVServer,{ Configuration_doneWebDAVSServerSettings(&webDAVServer); });
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; // not reached
    }
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_set(storageDirectoryListHandle->storageSpecifier.loginName,webDAVServer.loginName);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("USER"));
    if (storageDirectoryListHandle->storageSpecifier.hostPort == 0) storageDirectoryListHandle->storageSpecifier.hostPort = webDAVServer.port;
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate WebDAV server
    if (!allocateServer(storageDirectoryListHandle->webdav.serverId,serverConnectionPriority,ALLOCATE_SERVER_TIMEOUT))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->webdav.serverId,{ freeServer(storageDirectoryListHandle->webdav.serverId); });

    // check WebDAV login, get correct password
    error = ERROR_WEBDAV_SESSION_FAIL;
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL) && !Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword))
    {
      error = checkWebDAVLogin(storageDirectoryListHandle->storageSpecifier.type,
                               storageDirectoryListHandle->storageSpecifier.hostName,
                               storageDirectoryListHandle->storageSpecifier.hostPort,
                               storageDirectoryListHandle->storageSpecifier.loginName,
                               storageDirectoryListHandle->storageSpecifier.loginPassword,
                               webDAVServer.publicKey.data,
                               webDAVServer.publicKey.length,
                               webDAVServer.privateKey.data,
                               webDAVServer.privateKey.length,
                               pathName
                              );
    }
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL) && !Password_isEmpty(&webDAVServer.password))
    {
      error = checkWebDAVLogin(storageDirectoryListHandle->storageSpecifier.type,
                               storageDirectoryListHandle->storageSpecifier.hostName,
                               storageDirectoryListHandle->storageSpecifier.hostPort,
                               storageDirectoryListHandle->storageSpecifier.loginName,
                               &webDAVServer.password,
                               webDAVServer.publicKey.data,
                               webDAVServer.publicKey.length,
                               webDAVServer.privateKey.data,
                               webDAVServer.privateKey.length,
                               pathName
                              );
      if (error == ERROR_NONE)
      {
        Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,&webDAVServer.password);
      }
    }
    if ((Error_getCode(error) == ERROR_CODE_WEBDAV_SESSION_FAIL) && !Password_isEmpty(&defaultWebDAVPassword))
    {
      error = checkWebDAVLogin(storageDirectoryListHandle->storageSpecifier.type,
                               storageDirectoryListHandle->storageSpecifier.hostName,
                               storageDirectoryListHandle->storageSpecifier.hostPort,
                               storageDirectoryListHandle->storageSpecifier.loginName,
                               &defaultWebDAVPassword,
                               webDAVServer.publicKey.data,
                               webDAVServer.publicKey.length,
                               webDAVServer.privateKey.data,
                               webDAVServer.privateKey.length,
                               pathName
                              );
      if (error == ERROR_NONE)
      {
        Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,&defaultWebDAVPassword);
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
        error = checkWebDAVLogin(storageDirectoryListHandle->storageSpecifier.type,
                                 storageDirectoryListHandle->storageSpecifier.hostName,
                                 storageDirectoryListHandle->storageSpecifier.hostPort,
                                 storageDirectoryListHandle->storageSpecifier.loginName,
                                 storageDirectoryListHandle->storageSpecifier.loginPassword,
                                 webDAVServer.publicKey.data,
                                 webDAVServer.publicKey.length,
                                 webDAVServer.privateKey.data,
                                 webDAVServer.privateKey.length,
                                 pathName
                                );
      }
    }
    if (error != ERROR_NONE)
    {
      error = (   !Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword)
               || !Password_isEmpty(&webDAVServer.password)
               || !Password_isEmpty(&defaultWebDAVPassword))
                ? ERRORX_(INVALID_WEBDAV_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName))
                : ERRORX_(NO_WEBDAV_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName));
    }
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // store password as default webDAV password
    Password_set(&defaultWebDAVPassword,storageDirectoryListHandle->storageSpecifier.loginPassword);

    // init handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_WEBDAV_SESSION_FAIL;
    }

    // init WebDAV login
    error = setWebDAVLogin(curlHandle,
                           storageDirectoryListHandle->storageSpecifier.loginName,
                           storageDirectoryListHandle->storageSpecifier.loginPassword,
                           webDAVServer.publicKey.data,
                           webDAVServer.publicKey.length,
                           webDAVServer.privateKey.data,
                           webDAVServer.privateKey.length,
                           WEBDAV_TIMEOUT
                          );
    if (error != ERROR_NONE)
    {
      (void)curl_easy_cleanup(curlHandle);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // get URL
    url = getWebDAVURL(storageDirectoryListHandle->storageSpecifier.type,
                       storageDirectoryListHandle->storageSpecifier.hostName,
                       storageDirectoryListHandle->storageSpecifier.hostPort,
                       pathName
                      );
    String_appendChar(url,'/');

    // read directory data
    directoryData = String_new();
    curlSList     = NULL;
    curlCode      = CURLE_OK;
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
      curlSList = curl_slist_append(NULL,"Depth: 1");
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
    error = getCurlHTTPResponseError(curlHandle,pathName);
    if (error != ERROR_NONE)
    {
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
                                                                  "D:response",
                                                                  NULL,
                                                                  NULL,
                                                                  MXML_DESCEND
                                                                 );

    // free resources
    String_delete(directoryData);
    String_delete(url);
    (void)curl_easy_cleanup(curlHandle);
    switch (storageSpecifier->type)
    {
      case STORAGE_TYPE_WEBDAV:
        Configuration_doneWebDAVServerSettings(&webDAVServer);
        break;
      case STORAGE_TYPE_WEBDAVS:
        Configuration_doneWebDAVSServerSettings(&webDAVServer);
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; // not reached
    }
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
  assert(   (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

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
  assert(   (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

  endOfDirectoryFlag = TRUE;

  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    if (storageDirectoryListHandle->webdav.currentNode == NULL)
    {
      storageDirectoryListHandle->webdav.currentNode = mxmlFindElement(storageDirectoryListHandle->webdav.lastNode,
                                                                       storageDirectoryListHandle->webdav.rootNode,
                                                                       "D:response",
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
    mxml_node_t *propNode;
    mxml_node_t *node;
  #endif /* HAVE_CURL */

  assert(storageDirectoryListHandle != NULL);
  assert(   (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAV)
         || (storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_WEBDAVS)
        );

  error = ERROR_UNKNOWN;
  #if defined(HAVE_CURL) && defined(HAVE_MXML)
    if (storageDirectoryListHandle->webdav.currentNode == NULL)
    {
      storageDirectoryListHandle->webdav.currentNode = mxmlFindElement(storageDirectoryListHandle->webdav.lastNode,
                                                                       storageDirectoryListHandle->webdav.rootNode,
                                                                       "D:response",
                                                                       NULL,
                                                                       NULL,
                                                                       MXML_DESCEND
                                                                      );
    }
    if (   (storageDirectoryListHandle->webdav.currentNode != NULL)
        && (mxmlGetType(storageDirectoryListHandle->webdav.currentNode) == MXML_ELEMENT)
        && (mxmlGetFirstChild(storageDirectoryListHandle->webdav.currentNode) != NULL)
        && (mxmlGetType(mxmlGetFirstChild(storageDirectoryListHandle->webdav.currentNode)) == MXML_OPAQUE)
        && (mxmlGetOpaque(mxmlGetFirstChild(storageDirectoryListHandle->webdav.currentNode)) != NULL)
       )
    {
      // get file name
      node = mxmlFindElement(storageDirectoryListHandle->webdav.currentNode,
                             storageDirectoryListHandle->webdav.currentNode,
                             "D:href",
                             NULL,
                             NULL,
                             MXML_DESCEND
                            );
      if (   (node != NULL)
          && (mxmlGetType(node) == MXML_ELEMENT)
          && (mxmlGetFirstChild(node) != NULL)
          && (mxmlGetType(mxmlGetFirstChild(node)) == MXML_OPAQUE)
          && (mxmlGetOpaque(mxmlGetFirstChild(node)) != NULL)
         )
      {
        String_setCString(fileName,mxmlGetOpaque(mxmlGetFirstChild(node)));
        if (String_startsWithChar(fileName,'/')) String_remove(fileName,STRING_BEGIN,1);
        if (String_length(fileName) > 1) String_trimEnd(fileName,"/");
      }

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
        fileInfo->permissions     = FILE_DEFAULT_PERMISSIONS;
        fileInfo->major           = 0;
        fileInfo->minor           = 0;

        propNode = mxmlFindElement(storageDirectoryListHandle->webdav.currentNode,
                                   storageDirectoryListHandle->webdav.currentNode,
                                   "D:prop",
                                   NULL,
                                   NULL,
                                   MXML_DESCEND
                                  );

        if (   (propNode != NULL)
            && (mxmlGetType(propNode) == MXML_ELEMENT)
            && (mxmlGetFirstChild(propNode) != NULL)
            && (mxmlGetType(mxmlGetFirstChild(propNode)) == MXML_OPAQUE)
            && (mxmlGetOpaque(mxmlGetFirstChild(propNode)) != NULL)
           )
        {
          node = mxmlFindElement(propNode,
                                 propNode,
                                 "D:getcontenttype",
                                 NULL,
                                 NULL,
                                 MXML_DESCEND
                                );
          if (   (node != NULL)
              && (mxmlGetType(node) == MXML_ELEMENT)
              && (mxmlGetFirstChild(node) != NULL)
              && (mxmlGetType(mxmlGetFirstChild(node)) == MXML_OPAQUE)
              && (mxmlGetOpaque(mxmlGetFirstChild(node)) != NULL)
             )
          {
            if (stringEndsWith(mxmlGetOpaque(mxmlGetFirstChild(node)),"unix-directory"))
            {
              fileInfo->type = FILE_TYPE_DIRECTORY;
            }
          }
          node = mxmlFindElement(propNode,
                                 propNode,
                                 "lp1:getcontentlength",
                                 NULL,
                                 NULL,
                                 MXML_DESCEND
                                );
          if (   (node != NULL)
              && (mxmlGetType(node) == MXML_ELEMENT)
              && (mxmlGetFirstChild(node) != NULL)
              && (mxmlGetType(mxmlGetFirstChild(node)) == MXML_OPAQUE)
              && (mxmlGetOpaque(mxmlGetFirstChild(node)) != NULL)
             )
          {
            stringToUInt64(mxmlGetOpaque(mxmlGetFirstChild(node)),&fileInfo->size,NULL);
          }
          node = mxmlFindElement(propNode,
                                 propNode,
                                 "lp1:getlastmodified",
                                 NULL,
                                 NULL,
                                 MXML_DESCEND
                                );
          if (   (node != NULL)
              && (mxmlGetType(node) == MXML_ELEMENT)
              && (mxmlGetFirstChild(node) != NULL)
              && (mxmlGetType(mxmlGetFirstChild(node)) == MXML_OPAQUE)
              && (mxmlGetOpaque(mxmlGetFirstChild(node)) != NULL)
             )
          {
            fileInfo->timeModified = Misc_parseDateTime(mxmlGetOpaque(mxmlGetFirstChild(node)));
          }
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
