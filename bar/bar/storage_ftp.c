/***********************************************************************\
*
* $Revision: 4012 $
* $Date: 2015-04-28 19:02:40 +0200 (Tue, 28 Apr 2015) $
* $Author: torsten $
* Contents: storage FTP functions
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
#ifdef HAVE_FTP
  #include <ftplib.h>
#endif /* HAVE_FTP */
#ifdef HAVE_CURL
  #include <curl/curl.h>
#endif /* HAVE_CURL */
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

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
/* file data buffer size */
#define BUFFER_SIZE (64*1024)

// different timeouts [ms]
#define FTP_TIMEOUT (30*1000)


/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#if defined(HAVE_CURL) || defined(HAVE_FTP)
  LOCAL Password *defaultFTPPassword;
#endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#if defined(HAVE_CURL) || defined(HAVE_FTP)
/***********************************************************************\
* Name   : initFTPLogin
* Purpose: init FTP login
* Input  : hostName            - host name
*          loginName           - login name
*          loginPassword       - password
*          jobOptions          - job options
*          getPasswordFunction - get password call-back (can
*                                be NULL)
*          getPasswordUserData - user data for get password call-back
* Output : -
* Return : TRUE if FTP password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initFTPLogin(ConstString         hostName,
                        String              loginName,
                        Password            *loginPassword,
                        const JobOptions    *jobOptions,
                        GetPasswordFunction getPasswordFunction,
                        void                *getPasswordUserData
                       )
{
  SemaphoreLock semaphoreLock;
  String        s;
  bool          initFlag;

  assert(!String_isEmpty(hostName));
  assert(loginName != NULL);
  assert(loginPassword != NULL);

  initFlag = FALSE;

  if (jobOptions != NULL)
  {
    SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if (jobOptions->ftpServer.password == NULL)
      {
        switch (globalOptions.runMode)
        {
          case RUN_MODE_INTERACTIVE:
            if (defaultFTPPassword == NULL)
            {
              s = !String_isEmpty(loginName)
                    ? String_format(String_new(),"FTP login password for %S@%S",loginName,hostName)
                    : String_format(String_new(),"FTP login password for %S",hostName);
              if (Password_input(loginPassword,String_cString(s),PASSWORD_INPUT_MODE_ANY))
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
          case RUN_MODE_BATCH:
          case RUN_MODE_SERVER:
            if (getPasswordFunction != NULL)
            {
              s = !String_isEmpty(loginName)
                    ? String_format(String_new(),"%S@%S",loginName,hostName)
                    : String_format(String_new(),"%S",hostName);
              if (getPasswordFunction(loginName,
                                      loginPassword,
                                      PASSWORD_TYPE_FTP,
                                      String_cString(s),
                                      TRUE,
                                      TRUE,
                                      getPasswordUserData
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

/***********************************************************************\
* Name   : initFTPHandle
* Purpose: init FTP handle
* Input  : curlHandle    - CURL handle
*          url           - URL
*          loginName     - login name
*          loginPassword - login password
*          timeout       - timeout [ms]
* Output : -
* Return : CURLE_OK if no error, CURL error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL CURLcode initFTPHandle(CURL *curlHandle, ConstString url, ConstString loginName, Password *loginPassword, long timeout)
{
  CURLcode    curlCode;
  const char *plainPassword;

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
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_FTP_RESPONSE_TIMEOUT,timeout/1000);
  }
  if (isPrintInfo(6))
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
  (void)curl_easy_setopt(storageInfo->ftp.curlMultiHandle,CURLOPT_NOSIGNAL,1L);
  (void)curl_easy_setopt(storageInfo->ftp.curlHandle,CURLOPT_NOSIGNAL,1L);
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
    plainPassword = Password_deploy(loginPassword);
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_PASSWORD,plainPassword);
    Password_undeploy(loginPassword,plainPassword);
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
#endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */

#if defined(HAVE_CURL) || defined(HAVE_FTP)
/***********************************************************************\
* Name   : checkFTPLogin
* Purpose: check if FTP login is possible
* Input  : hostName      - host name
*          hostPort      - host port or 0
*          loginName     - login name
*          loginPassword - login password
* Output : -
* Return : ERROR_NONE if login is possible, error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors checkFTPLogin(ConstString hostName,
                           uint        hostPort,
                           ConstString loginName,
                           Password    *loginPassword
                          )
{
  #if   defined(HAVE_CURL)
    CURL       *curlHandle;
    String     url;
    CURLcode   curlCode;
  #elif defined(HAVE_FTP)
    netbuf     *ftpControl;
    const char *plainPassword;
  #endif

  #if   defined(HAVE_CURL)
    // init handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      return ERROR_FTP_SESSION_FAIL;
    }

    // set connect
    url = String_format(String_new(),"ftp://%S",hostName);
    if (hostPort != 0) String_format(url,":%d",hostPort);
    curlCode = initFTPHandle(curlHandle,url,loginName,loginPassword,FTP_TIMEOUT);
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
    }
    String_delete(url);
    if (curlCode != CURLE_OK)
    {
      (void)curl_easy_cleanup(curlHandle);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // login
    curlCode = curl_easy_perform(curlHandle);
    if (curlCode != CURLE_OK)
    {
      (void)curl_easy_cleanup(curlHandle);
      return ERRORX_(FTP_AUTHENTICATION,0,"%s",curl_easy_strerror(curlCode));
    }

    // free resources
    (void)curl_easy_cleanup(curlHandle);
  #elif defined(HAVE_FTP)
// NYI: TODO: support different FTP port
    UNUSED_VARIABLE(hostPort);

    // check host name (Note: FTP library crash if host name is not valid!)
    if (!Network_hostExists(hostName))
    {
      return ERRORX_(HOST_NOT_FOUND,0,"%s",String_cString(hostName);
    }

    // connect
    if (FtpConnect(String_cString(hostName),&ftpControl) != 1)
    {
      return ERROR_FTP_SESSION_FAIL;
    }

    // login
    plainPassword = Password_deploy(loginPassword);
    if (FtpLogin(String_cString(loginName),
                 plainPassword,
                 ftpControl
                ) != 1
       )
    {
      Password_undeploy(loginPassword,plainPassword);
      FtpClose(ftpControl);
      return ERROR_FTP_AUTHENTICATION;
    }
    Password_undeploy(loginPassword,plainPassword);
    FtpQuit(ftpControl);
  #endif

  return ERROR_NONE;
}
#endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */

#ifdef HAVE_CURL
/***********************************************************************\
* Name   : curlFTPReadDataCallback
* Purpose: curl FTP read data callback: receive data from remote
* Input  : buffer   - buffer for data
*          size     - size of an element
*          n        - number of elements
*          userData - user data
* Output : -
* Return : number of read bytes or 0
* Notes  : -
\***********************************************************************/

LOCAL size_t curlFTPReadDataCallback(void   *buffer,
                                     size_t size,
                                     size_t n,
                                     void   *userData
                                    )
{
  StorageHandle *storageHandle = (StorageHandle*)userData;
  size_t               bytesSent;

  assert(buffer != NULL);
  assert(size > 0);
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->ftp.buffer != NULL);

  if (storageHandle->ftp.transferedBytes < storageHandle->ftp.length)
  {
    bytesSent = MIN(n,(size_t)(storageHandle->ftp.length-storageHandle->ftp.transferedBytes)/size)*size;

    memcpy(buffer,storageHandle->ftp.buffer,bytesSent);

    storageHandle->ftp.buffer          = (byte*)storageHandle->ftp.buffer+bytesSent;
    storageHandle->ftp.transferedBytes += (ulong)bytesSent;
  }
  else
  {
    bytesSent = 0;
  }
//fprintf(stderr,"%s, %d: bytesSent=%d\n",__FILE__,__LINE__,bytesSent);

  return bytesSent;
}

/***********************************************************************\
* Name   : curlFTPWriteDataCallback
* Purpose: curl FTP write data callback: send data to remote
* Input  : buffer   - buffer with data
*          size     - size of an element
*          n        - number of elements
*          userData - user data
* Output : -
* Return : number of written bytes or 0
* Notes  : -
\***********************************************************************/

LOCAL size_t curlFTPWriteDataCallback(const void *buffer,
                                      size_t     size,
                                      size_t     n,
                                      void       *userData
                                     )
{
  StorageHandle *storageHandle = (StorageHandle*)userData;
  size_t        bytesReceived;

  assert(buffer != NULL);
  assert(size > 0);
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->ftp.buffer != NULL);

  if (storageHandle->ftp.transferedBytes < storageHandle->ftp.length)
  {
    bytesReceived = MIN(n,(size_t)(storageHandle->ftp.length-storageHandle->ftp.transferedBytes)/size)*size;

    memcpy(storageHandle->ftp.buffer,buffer,bytesReceived);

    storageHandle->ftp.buffer          = (byte*)storageHandle->ftp.buffer+bytesReceived;
    storageHandle->ftp.transferedBytes += (ulong)bytesReceived;
  }
  else
  {
    bytesReceived = 0;
  }
//fprintf(stderr,"%s, %d: bytesReceived=%d\n",__FILE__,__LINE__,bytesReceived);

  return bytesReceived;
}

/***********************************************************************\
* Name   : curlFTPParseDirectoryListCallback
* Purpose: curl FTP parse directory list callback
* Input  : buffer   - buffer with data: receive data from remote
*          size     - size of an element
*          n        - number of elements
*          userData - user data
* Output : -
* Return : number of processed bytes or 0
* Notes  : -
\***********************************************************************/

LOCAL size_t curlFTPParseDirectoryListCallback(const void *buffer,
                                               size_t     size,
                                               size_t     n,
                                               void       *userData
                                              )
{
  StorageDirectoryListHandle *storageDirectoryListHandle = (StorageDirectoryListHandle*)userData;
  String                     line;
  const char                 *s;
  size_t                     i;

  assert(buffer != NULL);
  assert(size > 0);
  assert(storageDirectoryListHandle != NULL);

  line = String_new();
  s    = (const char*)buffer;
  for (i = 0; i < n; i++)
  {
    switch (*s)
    {
      case '\n':
        StringList_append(&storageDirectoryListHandle->ftp.lineList,line);
        String_clear(line);
        break;
      case '\r':
        break;
      default:
        String_appendChar(line,(*s));
        break;
    }
    s++;
  }
  String_delete(line);

  return size*n;
}
#endif /* HAVE_CURL */

#if !defined(HAVE_CURL) && defined(HAVE_FTP)
/***********************************************************************\
* Name   : ftpTimeoutCallback
* Purpose: callback on FTP timeout
* Input  : control        - FTP handle
-          transferdBytes - number of transfered bytes
-          userData       - user data
* Output : -
* Return : always 0 to trigger error
* Notes  : -
\***********************************************************************/

LOCAL int ftpTimeoutCallback(netbuf *control,
                             int    transferdBytes,
                             void   *userData
                            )
{
  UNUSED_VARIABLE(control);
  UNUSED_VARIABLE(transferdBytes);
  UNUSED_VARIABLE(userData);

  return 0;
}
#endif /* !defined(HAVE_CURL) && defined(HAVE_FTP) */

#if defined(HAVE_CURL) || defined(HAVE_FTP)
/***********************************************************************\
* Name   : parseFTPDirectoryLine
* Purpose: parse FTP directory entry line
* Input  : line - line
* Output : fileInfo - filled file info
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseFTPDirectoryLine(String         line,
                                 String         fileName,
                                 FileTypes      *type,
                                 int64          *size,
                                 uint64         *timeModified,
                                 uint32         *userId,
                                 uint32         *groupId,
                                 FilePermission *permission
                                )
{
  typedef struct
  {
    const char *name;
    uint       month;
  } MonthDefinition;

  const MonthDefinition MONTH_DEFINITIONS[] =
  {
    {"january",   1},
    {"february",  2},
    {"march",     3},
    {"april",     4},
    {"may",       5},
    {"june",      6},
    {"july",      7},
    {"august",    8},
    {"september", 9},
    {"october",  10},
    {"november", 11},
    {"december", 12},

    {"jan", 1},
    {"feb", 2},
    {"mar", 3},
    {"apr", 4},
    {"may", 5},
    {"jun", 6},
    {"jul", 7},
    {"aug", 8},
    {"sep", 9},
    {"oct",10},
    {"nov",11},
    {"dec",12},

    { "1", 1},
    { "2", 2},
    { "3", 3},
    { "4", 4},
    { "5", 5},
    { "6", 6},
    { "7", 7},
    { "8", 8},
    { "9", 9},
    {"10",10},
    {"11",11},
    {"12",12},
  };

  bool       parsedFlag;
  char       permissionString[32];
  uint       permissionStringLength;
  uint       year,month,day;
  uint       hour,minute;
  char       monthName[32];
  const char *s;
  uint       z;

  assert(line != NULL);
  assert(fileName != NULL);
  assert(type != NULL);
  assert(size != NULL);
  assert(timeModified != NULL);
  assert(userId != NULL);
  assert(groupId != NULL);
  assert(permission != NULL);

  parsedFlag = FALSE;

  if      (String_parse(line,
                        STRING_BEGIN,
                        "%32s %* %* %* %lld %u-%u-%u %u:%u % S",
                        NULL,
                        permissionString,
                        size,
                        &year,&month,&day,
                        &hour,&minute,
                        fileName
                       )
          )
  {
    // format:  <permission flags> * * * <size> <year>-<month>-<day> <hour>:<minute> <file name>

    permissionStringLength = strlen(permissionString);

    switch (permissionString[0])
    {
      case 'd': (*type) = FILE_TYPE_DIRECTORY; break;
      default:  (*type) = FILE_TYPE_FILE;      break;
    }
    (*timeModified) = Misc_makeDateTime(year,month,day,
                                        hour,minute,0
                                       );
    (*userId)       = 0;
    (*groupId)      = 0;
    (*permission)   = 0;
    if ((permissionStringLength > 1) && (permissionString[1] = 'r')) (*permission) |= FILE_PERMISSION_USER_READ;
    if ((permissionStringLength > 2) && (permissionString[2] = 'w')) (*permission) |= FILE_PERMISSION_USER_WRITE;
    if ((permissionStringLength > 3) && (permissionString[3] = 'x')) (*permission) |= FILE_PERMISSION_USER_EXECUTE;
    if ((permissionStringLength > 4) && (permissionString[4] = 'r')) (*permission) |= FILE_PERMISSION_GROUP_READ;
    if ((permissionStringLength > 5) && (permissionString[5] = 'w')) (*permission) |= FILE_PERMISSION_GROUP_WRITE;
    if ((permissionStringLength > 6) && (permissionString[6] = 'x')) (*permission) |= FILE_PERMISSION_GROUP_EXECUTE;
    if ((permissionStringLength > 7) && (permissionString[7] = 'r')) (*permission) |= FILE_PERMISSION_OTHER_READ;
    if ((permissionStringLength > 8) && (permissionString[8] = 'w')) (*permission) |= FILE_PERMISSION_OTHER_WRITE;
    if ((permissionStringLength > 9) && (permissionString[9] = 'x')) (*permission) |= FILE_PERMISSION_OTHER_EXECUTE;

    parsedFlag = TRUE;
  }
  else if (String_parse(line,
                        STRING_BEGIN,
                        "%32s %* %* %* %llu %32s %u %u:%u % S",
                        NULL,
                        permissionString,
                        size,
                        monthName,&day,
                        &hour,&minute,
                        fileName
                       )
          )
  {
    // format:  <permission flags> * * * <size> <month> <day> <hour>:<minute> <file name>

    permissionStringLength = strlen(permissionString);

    // get year, month
    Misc_splitDateTime(Misc_getCurrentDateTime(),
                       &year,
                       &month,
                       NULL,   // day,
                       NULL,   // hour,
                       NULL,   // minute,
                       NULL,
                       NULL
                      );
    s = monthName;
    while (((*s) == '0'))
    {
      s++;
    }
    for (z = 0; z < SIZE_OF_ARRAY(MONTH_DEFINITIONS); z++)
    {
      if (stringEqualsIgnoreCase(MONTH_DEFINITIONS[z].name,s))
      {
        month = MONTH_DEFINITIONS[z].month;
        break;
      }
    }

    // fill file info
    switch (permissionString[0])
    {
      case 'd': (*type) = FILE_TYPE_DIRECTORY; break;
      default:  (*type) = FILE_TYPE_FILE; break;
    }
    (*timeModified) = Misc_makeDateTime(year,month,day,
                                        hour,minute,0
                                       );
    (*userId)       = 0;
    (*groupId)      = 0;
    (*permission)   = 0;
    if ((permissionStringLength > 1) && (permissionString[1] = 'r')) (*permission) |= FILE_PERMISSION_USER_READ;
    if ((permissionStringLength > 2) && (permissionString[2] = 'w')) (*permission) |= FILE_PERMISSION_USER_WRITE;
    if ((permissionStringLength > 3) && (permissionString[3] = 'x')) (*permission) |= FILE_PERMISSION_USER_EXECUTE;
    if ((permissionStringLength > 4) && (permissionString[4] = 'r')) (*permission) |= FILE_PERMISSION_GROUP_READ;
    if ((permissionStringLength > 5) && (permissionString[5] = 'w')) (*permission) |= FILE_PERMISSION_GROUP_WRITE;
    if ((permissionStringLength > 6) && (permissionString[6] = 'x')) (*permission) |= FILE_PERMISSION_GROUP_EXECUTE;
    if ((permissionStringLength > 7) && (permissionString[7] = 'r')) (*permission) |= FILE_PERMISSION_OTHER_READ;
    if ((permissionStringLength > 8) && (permissionString[8] = 'w')) (*permission) |= FILE_PERMISSION_OTHER_WRITE;
    if ((permissionStringLength > 9) && (permissionString[9] = 'x')) (*permission) |= FILE_PERMISSION_OTHER_EXECUTE;

    parsedFlag = TRUE;
  }
  else if (String_parse(line,
                        STRING_BEGIN,
                        "%32s %* %* %* %llu %32s %u %u % S",
                        NULL,
                        permissionString,
                        size,
                        monthName,&day,&year,
                        fileName
                       )
          )
  {
    // format:  <permission flags> * * * <size> <month> <day> <year> <file name>

    permissionStringLength = strlen(permissionString);

    // get month
    Misc_splitDateTime(Misc_getCurrentDateTime(),
                       NULL,   // year
                       &month,
                       NULL,   // day,
                       NULL,   // hour,
                       NULL,   // minute,
                       NULL,
                       NULL
                      );
    s = monthName;
    while (((*s) == '0'))
    {
      s++;
    }
    for (z = 0; z < SIZE_OF_ARRAY(MONTH_DEFINITIONS); z++)
    {
      if (stringEqualsIgnoreCase(MONTH_DEFINITIONS[z].name,s))
      {
        month = MONTH_DEFINITIONS[z].month;
        break;
      }
    }

    switch (permissionString[0])
    {
      case 'd': (*type) = FILE_TYPE_DIRECTORY; break;
      default:  (*type) = FILE_TYPE_FILE; break;
    }
    (*timeModified) = Misc_makeDateTime(year,month,day,
                                        0,0,0
                                       );
    (*userId)       = 0;
    (*groupId)      = 0;
    (*permission)   = 0;
    if ((permissionStringLength > 1) && (permissionString[1] = 'r')) (*permission) |= FILE_PERMISSION_USER_READ;
    if ((permissionStringLength > 2) && (permissionString[2] = 'w')) (*permission) |= FILE_PERMISSION_USER_WRITE;
    if ((permissionStringLength > 3) && (permissionString[3] = 'x')) (*permission) |= FILE_PERMISSION_USER_EXECUTE;
    if ((permissionStringLength > 4) && (permissionString[4] = 'r')) (*permission) |= FILE_PERMISSION_GROUP_READ;
    if ((permissionStringLength > 5) && (permissionString[5] = 'w')) (*permission) |= FILE_PERMISSION_GROUP_WRITE;
    if ((permissionStringLength > 6) && (permissionString[6] = 'x')) (*permission) |= FILE_PERMISSION_GROUP_EXECUTE;
    if ((permissionStringLength > 7) && (permissionString[7] = 'r')) (*permission) |= FILE_PERMISSION_OTHER_READ;
    if ((permissionStringLength > 8) && (permissionString[8] = 'w')) (*permission) |= FILE_PERMISSION_OTHER_WRITE;
    if ((permissionStringLength > 9) && (permissionString[9] = 'x')) (*permission) |= FILE_PERMISSION_OTHER_EXECUTE;

    parsedFlag = TRUE;
  }
  else if (String_parse(line,
                        STRING_BEGIN,
                        "%32s %* %* %* %llu %* %* %*:%* % S",
                        NULL,
                        permissionString,
                        size,
                        fileName
                       )
          )
  {
    // format:  <permission flags> * * * <size> * * *:* <file name>

    permissionStringLength = strlen(permissionString);

    switch (permissionString[0])
    {
      case 'd': (*type) = FILE_TYPE_DIRECTORY; break;
      default:  (*type) = FILE_TYPE_FILE; break;
    }
    (*timeModified) = 0LL;
    (*userId)       = 0;
    (*groupId)      = 0;
    (*permission)   = 0;
    if ((permissionStringLength > 1) && (permissionString[1] = 'r')) (*permission) |= FILE_PERMISSION_USER_READ;
    if ((permissionStringLength > 2) && (permissionString[2] = 'w')) (*permission) |= FILE_PERMISSION_USER_WRITE;
    if ((permissionStringLength > 3) && (permissionString[3] = 'x')) (*permission) |= FILE_PERMISSION_USER_EXECUTE;
    if ((permissionStringLength > 4) && (permissionString[4] = 'r')) (*permission) |= FILE_PERMISSION_GROUP_READ;
    if ((permissionStringLength > 5) && (permissionString[5] = 'w')) (*permission) |= FILE_PERMISSION_GROUP_WRITE;
    if ((permissionStringLength > 6) && (permissionString[6] = 'x')) (*permission) |= FILE_PERMISSION_GROUP_EXECUTE;
    if ((permissionStringLength > 7) && (permissionString[7] = 'r')) (*permission) |= FILE_PERMISSION_OTHER_READ;
    if ((permissionStringLength > 8) && (permissionString[8] = 'w')) (*permission) |= FILE_PERMISSION_OTHER_WRITE;
    if ((permissionStringLength > 9) && (permissionString[9] = 'x')) (*permission) |= FILE_PERMISSION_OTHER_EXECUTE;

    parsedFlag = TRUE;
  }
  else if (String_parse(line,
                        STRING_BEGIN,
                        "%32s %* %* %* %llu %* %* %* % S",
                        NULL,
                        permissionString,
                        size,
                        fileName
                       )
          )
  {
    // format:  <permission flags> * * * <size> * * * <file name>

    permissionStringLength = strlen(permissionString);

    switch (permissionString[0])
    {
      case 'd': (*type) = FILE_TYPE_DIRECTORY; break;
      default:  (*type) = FILE_TYPE_FILE; break;
    }
    (*timeModified) = 0LL;
    (*userId)       = 0;
    (*groupId)      = 0;
    (*permission)   = 0;
    if ((permissionStringLength > 1) && (permissionString[1] = 'r')) (*permission) |= FILE_PERMISSION_USER_READ;
    if ((permissionStringLength > 2) && (permissionString[2] = 'w')) (*permission) |= FILE_PERMISSION_USER_WRITE;
    if ((permissionStringLength > 3) && (permissionString[3] = 'x')) (*permission) |= FILE_PERMISSION_USER_EXECUTE;
    if ((permissionStringLength > 4) && (permissionString[4] = 'r')) (*permission) |= FILE_PERMISSION_GROUP_READ;
    if ((permissionStringLength > 5) && (permissionString[5] = 'w')) (*permission) |= FILE_PERMISSION_GROUP_WRITE;
    if ((permissionStringLength > 6) && (permissionString[6] = 'x')) (*permission) |= FILE_PERMISSION_GROUP_EXECUTE;
    if ((permissionStringLength > 7) && (permissionString[7] = 'r')) (*permission) |= FILE_PERMISSION_OTHER_READ;
    if ((permissionStringLength > 8) && (permissionString[8] = 'w')) (*permission) |= FILE_PERMISSION_OTHER_WRITE;
    if ((permissionStringLength > 9) && (permissionString[9] = 'x')) (*permission) |= FILE_PERMISSION_OTHER_EXECUTE;

    parsedFlag = TRUE;
  }

  return parsedFlag;
}
#endif /* HAVE_CURL */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageFTP_initAll(void)
{
  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    #if   defined(HAVE_CURL)
    #elif defined(HAVE_FTP)
      FtpInit();
    #endif /* HAVE_CURL || HAVE_FTP */
    defaultFTPPassword = NULL;
  #endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */

  return ERROR_NONE;
}

LOCAL void StorageFTP_doneAll(void)
{
  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    Password_delete(defaultFTPPassword);
    #if   defined(HAVE_CURL)
    #elif defined(HAVE_FTP)
    #endif /* HAVE_CURL || HAVE_FTP */
  #endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */
}

LOCAL bool StorageFTP_parseSpecifier(ConstString ftpSpecifier,
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

  assert(ftpSpecifier != NULL);
  assert(hostName != NULL);
  assert(loginName != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(loginName);
  if (loginPassword != NULL) Password_clear(loginPassword);

  s = String_new();
  t = String_new();
  if      (String_matchCString(ftpSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,STRING_NO_ASSIGN,loginName,s,STRING_NO_ASSIGN,hostName,t,NULL))
  {
    // <login name>:<login password>@<host name>:<host port>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (loginPassword != NULL) Password_setString(loginPassword,s);
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(t,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,loginName,s,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>:<login password>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (loginPassword != NULL) Password_setString(loginPassword,s);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,STRING_NO_ASSIGN,loginName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,loginName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^([^@:/]*?):([[:digit:]]+)$",NULL,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <host name>:<host port>
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (!String_isEmpty(ftpSpecifier))
  {
    // <host name>
    String_set(hostName,ftpSpecifier);

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

LOCAL bool StorageFTP_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
                                      ConstString            archiveName1,
                                      const StorageSpecifier *storageSpecifier2,
                                      ConstString            archiveName2
                                     )
{
  assert(storageSpecifier1 != NULL);
  assert(storageSpecifier1->type == STORAGE_TYPE_FTP);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_FTP);

  if (archiveName1 == NULL) archiveName1 = storageSpecifier1->archiveName;
  if (archiveName2 == NULL) archiveName2 = storageSpecifier2->archiveName;

  return    String_equals(storageSpecifier1->hostName,storageSpecifier2->hostName)
         && String_equals(storageSpecifier1->loginName,storageSpecifier2->loginName)
         && String_equals(archiveName1,archiveName2);
}

LOCAL void StorageFTP_getName(String                 string,
                              const StorageSpecifier *storageSpecifier,
                              ConstString            archiveName
                             )
{
  ConstString storageFileName;
  const char  *plainPassword;

  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_FTP);

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

  String_appendCString(string,"ftp://");
  if (!String_isEmpty(storageSpecifier->loginName))
  {
    String_append(string,storageSpecifier->loginName);
    if (!Password_isEmpty(storageSpecifier->loginPassword))
    {
      String_appendChar(string,':');
      plainPassword = Password_deploy(storageSpecifier->loginPassword);
      String_appendCString(string,plainPassword);
      Password_undeploy(storageSpecifier->loginPassword,plainPassword);
    }
    String_appendChar(string,'@');
  }
  String_append(string,storageSpecifier->hostName);
  if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 21))
  {
    String_format(string,":%d",storageSpecifier->hostPort);
  }
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(string,'/');
    String_append(string,storageFileName);
  }
}

LOCAL void StorageFTP_getPrintableName(String                 string,
                                       const StorageSpecifier *storageSpecifier,
                                       ConstString            archiveName
                                      )
{
  ConstString storageFileName;

  assert(string != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_FTP);

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

  String_appendCString(string,"ftp://");
  if (!String_isEmpty(storageSpecifier->loginName))
  {
    String_append(string,storageSpecifier->loginName);
    String_appendChar(string,'@');
  }
  String_append(string,storageSpecifier->hostName);
  if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 21))
  {
    String_format(string,":%d",storageSpecifier->hostPort);
  }
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(string,'/');
    String_append(string,storageFileName);
  }
}

LOCAL Errors StorageFTP_init(StorageInfo                *storageInfo,
                             const StorageSpecifier     *storageSpecifier,
                             const JobOptions           *jobOptions,
                             BandWidthList              *maxBandWidthList,
                             ServerConnectionPriorities serverConnectionPriority
                            )
{
  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    Errors error;
  #endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */

  assert(storageInfo != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_FTP);

  #if !defined(HAVE_CURL) && !defined(HAVE_FTP)
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif /* !defined(HAVE_CURL) && !defined(HAVE_FTP) */

  UNUSED_VARIABLE(storageSpecifier);

  #if   defined(HAVE_CURL)
    {
      FTPServer ftpServer;

      // init variables
      initBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter,maxBandWidthList);

      // get FTP server settings
      storageInfo->ftp.serverId = getFTPServerSettings(storageInfo->storageSpecifier.hostName,jobOptions,&ftpServer);
      if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_set(storageInfo->storageSpecifier.loginName,ftpServer.loginName);
      if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("LOGNAME"));
      if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("USER"));
      if (String_isEmpty(storageInfo->storageSpecifier.hostName))
      {
        doneBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter);
        return ERROR_NO_HOST_NAME;
      }

      // allocate FTP server
      if (!allocateServer(storageInfo->ftp.serverId,serverConnectionPriority,60*1000L))
      {
        doneBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter);
        return ERROR_TOO_MANY_CONNECTIONS;
      }

      // check FTP login, get correct password
      error = ERROR_FTP_SESSION_FAIL;
      if ((error != ERROR_NONE) && !Password_isEmpty(storageInfo->storageSpecifier.loginPassword))
      {
        error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.loginName,
                              storageInfo->storageSpecifier.loginPassword
                             );
      }
      if ((error != ERROR_NONE) && !Password_isEmpty(ftpServer.password))
      {
        error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.loginName,
                              ftpServer.password
                             );
        if (error == ERROR_NONE)
        {
          Password_set(storageInfo->storageSpecifier.loginPassword,ftpServer.password);
        }
      }
      if ((error != ERROR_NONE) && !Password_isEmpty(defaultFTPPassword))
      {
        error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.loginName,
                              defaultFTPPassword
                             );
        if (error == ERROR_NONE)
        {
          Password_set(storageInfo->storageSpecifier.loginPassword,defaultFTPPassword);
        }
      }
      if (error != ERROR_NONE)
      {
        // initialize default password
        while (   (error != ERROR_NONE)
               && initFTPLogin(storageInfo->storageSpecifier.hostName,
                               storageInfo->storageSpecifier.loginName,
                               storageInfo->storageSpecifier.loginPassword,
                               jobOptions,
                               CALLBACK(storageInfo->getPasswordFunction,storageInfo->getPasswordUserData)
                              )
           )
        {
          error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                                storageInfo->storageSpecifier.hostPort,
                                storageInfo->storageSpecifier.loginName,
                                storageInfo->storageSpecifier.loginPassword
                               );
          if (error == ERROR_NONE)
          {
            Password_set(storageInfo->storageSpecifier.loginPassword,defaultFTPPassword);
          }
        }
        if (error != ERROR_NONE)
        {
          error = (!Password_isEmpty(storageInfo->storageSpecifier.loginPassword) || !Password_isEmpty(ftpServer.password) || !Password_isEmpty(defaultFTPPassword))
                    ? ERRORX_(INVALID_FTP_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName))
                    : ERRORX_(NO_FTP_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
        }

        // store passwrd as default FTP password
        if (error == ERROR_NONE)
        {
          if (defaultFTPPassword == NULL) defaultFTPPassword = Password_new();
          Password_set(defaultFTPPassword,storageInfo->storageSpecifier.loginPassword);
        }
      }
      if (error != ERROR_NONE)
      {
        freeServer(storageInfo->ftp.serverId);
        doneBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter);
        return error;
      }
    }
    return ERROR_NONE;
  #elif defined(HAVE_FTP)
    {
      FTPServer ftpServer;

      // init variables
      initBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter,maxBandWidthList);

      // get FTP server settings
      getFTPServerSettings(storageInfo->storageSpecifier.hostName,jobOptions,&ftpServer);
      if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_set(storageInfo->storageSpecifier.loginName,ftpServer.loginName);
      if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("LOGNAME"));
      if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("USER"));
      if (String_isEmpty(storageInfo->storageSpecifier.hostName))
      {
        doneBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter);
        return ERROR_NO_HOST_NAME;
      }

      // allocate FTP server
      if (!allocateServer(storageInfo->ftp.serverId,serverConnectionPriority,60*1000L))
      {
        doneBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter);
        return ERROR_TOO_MANY_CONNECTIONS;
      }

      // check FTP login, get correct password
      error = ERROR_FTP_SESSION_FAIL;
      if ((error != ERROR_NONE) && !Password_isEmpty(storageInfo->storageSpecifier.loginPassword))
      {
        error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.loginName,
                              storageInfo->storageSpecifier.loginPassword
                             );
      }
      if ((error != ERROR_NONE) && !Password_isEmpty(ftpServer.password))
      {
        error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.loginName,
                              ftpServer.password
                             );
        if (error == ERROR_NONE)
        {
          Password_set(storageInfo->storageSpecifier.loginPassword,ftpServer.password);
        }
      }
      if ((error != ERROR_NONE) && !Password_isEmpty(defaultFTPPassword))
      {
        error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.loginName,
                              defaultFTPPassword
                             );
        if (error == ERROR_NONE)
        {
          Password_set(storageInfo->storageSpecifier.loginPassword,defaultFTPPassword);
        }
      }
      if (error != ERROR_NONE)
      {
        // initialize login password
        while (   (error != ERROR_NONE)
               && initFTPLogin(storageInfo->storageSpecifier.hostName,
                               storageInfo->storageSpecifier.loginName,
                               storageInfo->storageSpecifier.loginPassword,
                               jobOptions
                               CALLBACK(storageInfo->getPasswordFunction,storageInfo->getPasswordUserData)
                              )
              )
        {
          error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                                storageInfo->storageSpecifier.hostPort,
                                storageInfo->storageSpecifier.loginName,
                                storageInfo->storageSpecifier.loginPassword
                               );
        }
        if (error != ERROR_NONE)
        {
          error = (!Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword) || !Password_isEmpty(ftpServer.password) || !Password_isEmpty(defaultFTPPassword))
                    ? ERRORX_(ERROR_INVALID_FTP_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName))
                    : ERRORX_(ERROR_NO_FTP_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
        }

        // store passwrd as default FTP password
        if (error == ERROR_NONE)
        {
          if (defaultFTPPassword == NULL) defaultFTPPassword = Password_new();
          Password_set(defaultFTPPassword,storageInfo->storageSpecifier.loginPassword);
        }
      }
      if (error != ERROR_NONE)
      {
        freeServer(storageInfo->ftp.serverId);
        doneBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter);
        return error;
      }
    }
    return ERROR_NONE;
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(maxBandWidthList);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL Errors StorageFTP_done(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_FTP);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  #if   defined(HAVE_CURL)
    freeServer(storageInfo->ftp.serverId);
  #elif defined(HAVE_FTP)
    freeServer(storageInfo->ftp.serverId);
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);
  #endif /* HAVE_CURL || HAVE_FTP */

  return ERROR_NONE;
}

LOCAL bool StorageFTP_isServerAllocationPending(const StorageInfo *storageInfo)
{
  bool serverAllocationPending;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_FTP);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    serverAllocationPending = isServerAllocationPending(storageInfo->ftp.serverId);
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);

    serverAllocationPending = FALSE;
  #endif /* HAVE_CURL || HAVE_FTP */

  return serverAllocationPending;
}

LOCAL Errors StorageFTP_preProcess(const StorageInfo *storageInfo,
                                   ConstString       archiveName,
                                   time_t            time,
                                   bool              initialFlag
                                  )
{
  Errors error;
  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    TextMacro textMacros[2];
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_FTP);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  error = ERROR_NONE;

  #if   defined(HAVE_CURL) || defined(HAVE_FTP)
    if ((storageInfo->jobOptions == NULL) || !storageInfo->jobOptions->dryRunFlag)
    {
      if (!initialFlag)
      {
        // init macros
        TEXT_MACRO_N_STRING (textMacros[0],"%file",  archiveName,                NULL);
        TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageInfo->volumeNumber,NULL);

        // write pre-processing
        if (!String_isEmpty(globalOptions.ftp.writePreProcessCommand))
        {
          printInfo(1,"Write pre-processing...");
          error = executeTemplate(String_cString(globalOptions.ftp.writePreProcessCommand),
                                  time,
                                  textMacros,
                                  SIZE_OF_ARRAY(textMacros)
                                 );
          printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
        }
      }
    }
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(time);
    UNUSED_VARIABLE(initialFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */

  return error;
}

LOCAL Errors StorageFTP_postProcess(const StorageInfo *storageInfo,
                                    ConstString       archiveName,
                                    time_t            time,
                                    bool              finalFlag
                                   )
{
  Errors error;
  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    TextMacro textMacros[2];
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_FTP);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  error = ERROR_NONE;

  #if   defined(HAVE_CURL) || defined(HAVE_FTP)
    if ((storageInfo->jobOptions == NULL) || !storageInfo->jobOptions->dryRunFlag)
    {
      if (!finalFlag)
      {
        // init macros
        TEXT_MACRO_N_STRING (textMacros[0],"%file",  archiveName,                NULL);
        TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageInfo->volumeNumber,NULL);

        // write post-process
        if (!String_isEmpty(globalOptions.ftp.writePostProcessCommand))
        {
          printInfo(1,"Write post-processing...");
          error = executeTemplate(String_cString(globalOptions.ftp.writePostProcessCommand),
                                  time,
                                  textMacros,
                                  SIZE_OF_ARRAY(textMacros)
                                 );
          printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
        }
      }
    }
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(time);
    UNUSED_VARIABLE(finalFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */

  return error;
}

LOCAL Errors StorageFTP_unloadVolume(const StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_FTP);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  UNUSED_VARIABLE(storageInfo);

  return ERROR_NONE;
}

LOCAL bool StorageFTP_exists(const StorageInfo *storageInfo, ConstString archiveName)
{
  bool existsFlag;
  #if   defined(HAVE_CURL)
    CURL            *curlHandle;
    String          directoryName,baseName;
    String          url;
    CURLcode        curlCode;
    StringTokenizer nameTokenizer;
    ConstString     token;
  #elif defined(HAVE_FTP)
    netbuf     *control;
    const char *plainPassword;
    String     tmpFileName;
    FileHandle fileHandle;
    bool       foundFlag;
    String     line;
    int        size;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_FTP);
  assert(!String_isEmpty(archiveName));

  existsFlag = FALSE;

  #if   defined(HAVE_CURL)
    // open Curl handles
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      return ERROR_FTP_SESSION_FAIL;
    }

    // get directory name, base name
    directoryName = File_getDirectoryName(String_new(),archiveName);
    baseName      = File_getBaseName(String_new(),archiveName);

    // get URL
    url = String_format(String_new(),"ftp://%S",storageInfo->storageSpecifier.hostName);
    if (storageInfo->storageSpecifier.hostPort != 0) String_format(url,":d",storageInfo->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,directoryName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    File_doneSplitFileName(&nameTokenizer);
    String_append(url,baseName);

    // set FTP connect
    curlCode = initFTPHandle(curlHandle,
                             url,
                             storageInfo->storageSpecifier.loginName,
                             storageInfo->storageSpecifier.loginPassword,
                             FTP_TIMEOUT
                            );
    if (curlCode != CURLE_OK)
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      (void)curl_easy_cleanup(curlHandle);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // check if file exists (Note: by default curl use passive FTP)
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_perform(curlHandle);
    }
    existsFlag = (curlCode == CURLE_OK);

    // close FTP connection
    (void)curl_easy_cleanup(curlHandle);

    // free resources
    String_delete(url);
    String_delete(baseName);
    String_delete(directoryName);
  #elif defined(HAVE_FTP)
    // initialize variables

    // FTP connect
    if (!Network_hostExists(storageInfo->storageSpecifier.hostName))
    {
      return ERRORX_(HOST_NOT_FOUND,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
    }
    if (FtpConnect(String_cString(storageInfo->storageSpecifier.hostName),&control) != 1)
    {
      return ERROR_FTP_SESSION_FAIL;
    }

    // FTP login
    plainPassword = Password_deploy(storageInfo->storageSpecifier.loginPassword);
    if (FtpLogin(String_cString(storageInfo->storageSpecifier.loginName),
                 plainPassword,
                 control
                ) != 1
       )
    {
      Password_undeploy(storageInfo->storageSpecifier.loginPassword,plainPassword);
      FtpClose(control);
      return ERROR_FTP_AUTHENTICATION;
    }
    Password_undeploy(storageInfo->storageSpecifier.loginPassword,plainPassword);

    // check if file exists: first try non-passive, then passive mode
    tmpFileName = File_newFileName();
    error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
    if (error != ERROR_NONE)
    {
      FtpClose(control);
      return error;
    }
    FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,control);
    if (FtpDir(String_cString(tmpFileName),String_cString(fileName),control) != 1)
    {
      FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,control);
      if (FtpDir(String_cString(tmpFileName),String_cString(fileName),control) != 1)
      {
        File_delete(tmpFileName,FALSE);
        File_deleteFileName(tmpFileName);
        FtpClose(control);
        return ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(fileName));
      }
    }
    error = File_open(&fileHandle,tmpFileName,FILE_OPEN_READ);
    if (error != ERROR_NONE)
    {
      File_delete(tmpFileName,FALSE);
      File_deleteFileName(tmpFileName);
      FtpClose(control);
      return error;
    }
    foundFlag = FALSE;
    line = String_new();
    while (!File_eof(&fileHandle) && !foundFlag)
    {
      error = File_readLine(&fileHandle,line);
      if (error == ERROR_NONE)
      {
        foundFlag =   String_parse(line,
                                   STRING_BEGIN,
                                   "%32s %* %* %* %llu %d-%d-%d %d:%d %S",
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL,NULL,NULL,
                                   NULL,NULL,
                                   NULL
                                  )
                   || String_parse(line,
                                   STRING_BEGIN,
                                   "%32s %* %* %* %llu %* %* %*:%* %S",
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL
                                  )
                   || String_parse(line,
                                   STRING_BEGIN,
                                   "%32s %* %* %* %llu %* %* %* %S",
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL
                                  );
      }
    }
    String_delete(line);
    File_close(&fileHandle);
    File_delete(tmpFileName,FALSE);
    File_deleteFileName(tmpFileName);
    if (!foundFlag)
    {
      FtpClose(control);
      return ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(archiveName));
    }

    // close FTP connection
    FtpClose(control);
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
  #endif /* HAVE_CURL || HAVE_FTP */

  return existsFlag;
}

LOCAL Errors StorageFTP_getTmpName(String archiveName, const StorageInfo *storageInfo)
{
  assert(archiveName != NULL);
  assert(!String_isEmpty(archiveName));
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(storageInfo);

//TODO: still not implemented
return ERROR_STILL_NOT_IMPLEMENTED;
  return File_getTmpFileName(archiveName,String_cString(archiveName),NULL);
}

LOCAL Errors StorageFTP_create(StorageHandle *storageHandle,
                               ConstString   fileName,
                               uint64        fileSize
                              )
{
  #if   defined(HAVE_CURL)
    String          directoryName,baseName;
    String          url;
    CURLcode        curlCode;
    CURLMcode       curlMCode;
    StringTokenizer nameTokenizer;
    ConstString     token;
    int             runningHandles;
  #elif defined(HAVE_FTP)
    String          directoryName,baseName;
    String          newDirectoryName;
    StringTokenizer pathNameTokenizer;
    ConstString     token;
    const char      *plainPassword;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_FTP);
  assert(!String_isEmpty(fileName));

  #if   defined(HAVE_CURL)
    // init variables
    storageHandle->ftp.curlMultiHandle        = NULL;
    storageHandle->ftp.curlHandle             = NULL;
    storageHandle->ftp.index                  = 0LL;
    storageHandle->ftp.size                   = fileSize;
    storageHandle->ftp.readAheadBuffer.data   = NULL;
    storageHandle->ftp.readAheadBuffer.offset = 0LL;
    storageHandle->ftp.readAheadBuffer.length = 0L;
    storageHandle->ftp.length                 = 0L;
    storageHandle->ftp.transferedBytes        = 0L;

    // open Curl handles
    storageHandle->ftp.curlMultiHandle = curl_multi_init();
    if (storageHandle->ftp.curlMultiHandle == NULL)
    {
      return ERROR_FTP_SESSION_FAIL;
    }
    storageHandle->ftp.curlHandle = curl_easy_init();
    if (storageHandle->ftp.curlHandle == NULL)
    {
      curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      return ERROR_FTP_SESSION_FAIL;
    }

    // get directory name, base name
    directoryName = File_getDirectoryName(String_new(),fileName);
    baseName      = File_getBaseName(String_new(),fileName);

    // get URL
    url = String_format(String_new(),"ftp://%S",storageHandle->storageInfo->storageSpecifier.hostName);
    if (storageHandle->storageInfo->storageSpecifier.hostPort != 0) String_format(url,":d",storageHandle->storageInfo->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,directoryName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    File_doneSplitFileName(&nameTokenizer);
    String_append(url,baseName);

    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
      // create directories if necessary
      curlCode = initFTPHandle(storageHandle->ftp.curlHandle,
                               url,
                               storageHandle->storageInfo->storageSpecifier.loginName,
                               storageHandle->storageInfo->storageSpecifier.loginPassword,
                               FTP_TIMEOUT
                              );
      if (curlCode != CURLE_OK)
      {
        curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_FTP_CREATE_MISSING_DIRS,1L);
      }
      if (curlCode != CURLE_OK)
      {
        String_delete(url);
        String_delete(baseName);
        String_delete(directoryName);
        (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
        (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
        return ERRORX_(CREATE_DIRECTORY,0,"%s",curl_easy_strerror(curlCode));
      }

      // init FTP upload (Note: by default curl use passive FTP)
      curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_READFUNCTION,curlFTPReadDataCallback);
      if (curlCode == CURLE_OK)
      {
        curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_READDATA,storageHandle);
      }
      if (curlCode == CURLE_OK)
      {
        curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_UPLOAD,1L);
      }
      if (curlCode == CURLE_OK)
      {
        curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_INFILESIZE_LARGE,(curl_off_t)storageHandle->ftp.size);
      }
      curlMCode = curl_multi_add_handle(storageHandle->ftp.curlMultiHandle,storageHandle->ftp.curlHandle);
      if (curlMCode != CURLM_OK)
      {
        String_delete(url);
        String_delete(baseName);
        String_delete(directoryName);
        (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
        (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
        return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_multi_strerror(curlMCode));
      }

      // start FTP upload
      do
      {
        curlMCode = curl_multi_perform(storageHandle->ftp.curlMultiHandle,&runningHandles);
      }
      while (   (curlMCode == CURLM_CALL_MULTI_PERFORM)
             && (runningHandles > 0)
            );
//fprintf(stderr,"%s, %d: storageHandle->ftp.runningHandles=%d\n",__FILE__,__LINE__,storageHandle->ftp.runningHandles);
      if (curlMCode != CURLM_OK)
      {
        String_delete(url);
        String_delete(baseName);
        String_delete(directoryName);
        (void)curl_multi_remove_handle(storageHandle->ftp.curlMultiHandle,storageHandle->ftp.curlHandle);
        (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
        (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
        return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_multi_strerror(curlMCode));
      }

      // free resources
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
    }

    DEBUG_ADD_RESOURCE_TRACE(&storageHandle->ftp,sizeof(storageHandle->ftp));

    return ERROR_NONE;
  #elif defined(HAVE_FTP)
    // initialize variables
    storageHandle->ftp.index                  = 0LL;
    storageHandle->ftp.size                   = fileSize;
    storageHandle->ftp.readAheadBuffer.offset = 0LL;
    storageHandle->ftp.readAheadBuffer.length = 0L;
    storageHandle->ftp.transferedBytes        = 0L;

    // connect
    if (!Network_hostExists(storageHandle->storageSpecifier.hostName))
    {
      return ERRORX_(HOST_NOT_FOUND,0,"%s",String_cString(storageHandle->storageSpecifier.hostName));
    }
    if (FtpConnect(String_cString(storageHandle->storageSpecifier.hostName),&storageHandle->ftp.control) != 1)
    {
      return ERROR_FTP_SESSION_FAIL;
    }

    // login
    plainPassword = Password_deploy(storageInfo->storageSpecifier.loginPassword);
    if (FtpLogin(String_cString(storageInfo->storageSpecifier.loginName),
                 plainPassword,
                 storageHandle->ftp.control
                ) != 1
       )
    {
      Password_undeploy(storageInfo->storageSpecifier.loginPassword);
      FtpClose(storageHandle->ftp.control);
      return ERROR_FTP_AUTHENTICATION;
    }
    Password_undeploy(storageInfo->storageSpecifier.loginPassword);

    if ((storageInfo->jobOptions == NULL) || !storageInfo->jobOptions->dryRunFlag)
    {
      // create directory (try it and ignore errors)
      directoryName    = File_getDirectoryName(String_new(),archiveName);
      newDirectoryName = File_newFileName();
      File_initSplitFileName(&pathNameTokenizer,directoryName);
      if (File_getNextSplitFileName(&pathNameTokenizer,&token))
      {
        // create root-directory
        if (!String_isEmpty(token))
        {
          File_setFileName(newDirectoryName,token);
        }
        else
        {
          File_setFileNameChar(newDirectoryName,FILES_PATHNAME_SEPARATOR_CHAR);
        }
        (void)FtpMkdir(String_cString(newDirectoryName),storageHandle->ftp.control);

        // create sub-directories
        while (File_getNextSplitFileName(&pathNameTokenizer,&token))
        {
          if (!String_isEmpty(token))
          {
            // get sub-directory
            File_appendFileName(newDirectoryName,token);

            // create sub-directory
            (void)FtpMkdir(String_cString(newDirectoryName),storageHandle->ftp.control);
          }
        }
      }
      File_doneSplitFileName(&pathNameTokenizer);
      File_deleteFileName(newDirectoryName);
      File_deleteFileName(directoryName);

      // set timeout callback (120s) to avoid infinite waiting on read/write
      if (FtpOptions(FTPLIB_IDLETIME,
                     120*1000,
                     storageHandle->ftp.control
                    ) != 1
         )
      {
        FtpClose(storageHandle->ftp.control);
        return ERRORX_(CREATE_FILE,0,"ftp access");
      }
      if (FtpOptions(FTPLIB_CALLBACK,
                     (long)ftpTimeoutCallback,
                     storageHandle->ftp.control
                    ) != 1
         )
      {
        FtpClose(storageHandle->ftp.control);
        return ERRORX_(CREATE_FILE,0,"ftp access");
      }

      // create file: first try non-passive, then passive mode
      FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,storageHandle->ftp.control);
      if (FtpAccess(String_cString(archiveName),
                    FTPLIB_FILE_WRITE,
                    FTPLIB_IMAGE,
                    storageHandle->ftp.control,
                    &storageHandle->ftp.data
                   ) != 1
         )
      {
        FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,storageHandle->ftp.control);
        if (FtpAccess(String_cString(archiveName),
                      FTPLIB_FILE_WRITE,
                      FTPLIB_IMAGE,
                      storageHandle->ftp.control,
                      &storageHandle->ftp.data
                     ) != 1
           )
        {
          FtpClose(storageHandle->ftp.control);
          return ERRORX_(CREATE_FILE,0,"ftp access");
        }
      }
    }

    DEBUG_ADD_RESOURCE_TRACE(&storageHandle->ftp,sizeof(storageHandle->ftp));

    return ERROR_NONE;
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileSize);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL Errors StorageFTP_open(StorageHandle *storageHandle,
                             ConstString   archiveName
                            )
{
  #if   defined(HAVE_CURL)
    String          directoryName,baseName;
    String          url;
    CURLcode        curlCode;
    CURLMcode       curlMCode;
    StringTokenizer nameTokenizer;
    ConstString     token;
    double          fileSize;
    int             runningHandles;
  #elif defined(HAVE_FTP)
    const char *plain;
    String     tmpFileName;
    FileHandle fileHandle;
    bool       foundFlag;
    String     line;
    int        size;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_FTP);
  assert(!String_isEmpty(archiveName));

  #if   defined(HAVE_CURL)
    // initialize variables
    storageHandle->ftp.curlMultiHandle        = NULL;
    storageHandle->ftp.curlHandle             = NULL;
    storageHandle->ftp.index                  = 0LL;
    storageHandle->ftp.size                   = 0LL;
    storageHandle->ftp.readAheadBuffer.offset = 0LL;
    storageHandle->ftp.readAheadBuffer.length = 0L;
    storageHandle->ftp.length                 = 0L;
    storageHandle->ftp.transferedBytes        = 0L;

    // allocate read-ahead buffer
    storageHandle->ftp.readAheadBuffer.data = (byte*)malloc(BUFFER_SIZE);
    if (storageHandle->ftp.readAheadBuffer.data == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    // open Curl handles
    storageHandle->ftp.curlMultiHandle = curl_multi_init();
    if (storageHandle->ftp.curlMultiHandle == NULL)
    {
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERROR_FTP_SESSION_FAIL;
    }
    storageHandle->ftp.curlHandle = curl_easy_init();
    if (storageHandle->ftp.curlHandle == NULL)
    {
      curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERROR_FTP_SESSION_FAIL;
    }

    // get pathname, basename
    directoryName = File_getDirectoryName(String_new(),archiveName);
    baseName      = File_getBaseName(String_new(),archiveName);

    // get URL
    url = String_format(String_new(),"ftp://%S",storageHandle->storageInfo->storageSpecifier.hostName);
    if (storageHandle->storageInfo->storageSpecifier.hostPort != 0) String_format(url,":d",storageHandle->storageInfo->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,directoryName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    File_doneSplitFileName(&nameTokenizer);
    String_append(url,baseName);

    // set FTP connect
    curlCode = initFTPHandle(storageHandle->ftp.curlHandle,
                             url,
                             storageHandle->storageInfo->storageSpecifier.loginName,
                             storageHandle->storageInfo->storageSpecifier.loginPassword,
                             FTP_TIMEOUT
                            );
    if (curlCode != CURLE_OK)
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
      (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // check if file exists (Note: by default curl use passive FTP)
    curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_NOBODY,1L);
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_perform(storageHandle->ftp.curlHandle);
    }
    if (curlCode != CURLE_OK)
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
      (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(archiveName));
    }

    // get file size
    curlCode = curl_easy_getinfo(storageHandle->ftp.curlHandle,CURLINFO_CONTENT_LENGTH_DOWNLOAD,&fileSize);
    if (   (curlCode != CURLE_OK)
        || (fileSize < 0.0)
       )
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
      (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERROR_FTP_GET_SIZE;
    }
    storageHandle->ftp.size = (uint64)fileSize;

    // init FTP download (Note: by default curl use passive FTP)
    curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_NOBODY,0L);
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_WRITEFUNCTION,curlFTPWriteDataCallback);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_WRITEDATA,storageHandle);
    }
    if (curlCode != CURLE_OK)
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
      (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }
    curlMCode = curl_multi_add_handle(storageHandle->ftp.curlMultiHandle,storageHandle->ftp.curlHandle);
    if (curlMCode != CURLM_OK)
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
      (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_multi_strerror(curlMCode));
    }

    // start FTP download
    do
    {
      curlMCode = curl_multi_perform(storageHandle->ftp.curlMultiHandle,&runningHandles);
    }
    while (   (curlMCode == CURLM_CALL_MULTI_PERFORM)
           && (runningHandles > 0)
          );
    if (curlMCode != CURLM_OK)
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      (void)curl_multi_remove_handle(storageHandle->ftp.curlMultiHandle,storageHandle->ftp.curlHandle);
      (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
      (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_multi_strerror(curlMCode));
    }

    // free resources
    String_delete(url);
    String_delete(baseName);
    String_delete(directoryName);

    DEBUG_ADD_RESOURCE_TRACE(&storageHandle->ftp,sizeof(storageHandle->ftp));

    return ERROR_NONE;
  #elif defined(HAVE_FTP)
    // initialize variables
    storageHandle->ftp.control                = NULL;
    storageHandle->ftp.data                   = NULL;
    storageHandle->ftp.index                  = 0LL;
    storageHandle->ftp.readAheadBuffer.offset = 0LL;
    storageHandle->ftp.readAheadBuffer.length = 0L;
    storageHandle->ftp.transferedBytes        = 0L;

    // allocate read-ahead buffer
    storageHandle->ftp.readAheadBuffer.data = (byte*)malloc(BUFFER_SIZE);
    if (storageHandle->ftp.readAheadBuffer.data == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    // FTP connect
    if (!Network_hostExists(storageInfo->storageSpecifier.hostName))
    {
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERRORX_(HOST_NOT_FOUND,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
    }
    if (FtpConnect(String_cString(storageInfo->storageSpecifier.hostName),&storageHandle->ftp.control) != 1)
    {
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERROR_FTP_SESSION_FAIL;
    }

    // FTP login
    plain = Password_deploy(storageInfo->storageSpecifier.loginPassword);
    if (FtpLogin(String_cString(storageInfo->storageSpecifier.loginName),
                 plain,
                 storageHandle->ftp.control
                ) != 1
       )
    {
      Password_undeploy(storageInfo->storageSpecifier.loginPassword,plain);
      FtpClose(storageHandle->ftp.control);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERROR_FTP_AUTHENTICATION;
    }
    Password_undeploy(storageInfo->storageSpecifier.loginPassword,plain);

    // check if file exists: first try non-passive, then passive mode
    tmpFileName = File_newFileName();
    error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
    if (error != ERROR_NONE)
    {
      FtpClose(storageHandle->ftp.control);
      free(storageHandle->ftp.readAheadBuffer.data);
      return error;
    }
    FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,storageHandle->ftp.control);
    if (FtpDir(String_cString(tmpFileName),String_cString(fileName),storageHandle->ftp.control) != 1)
    {
      FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,storageHandle->ftp.control);
      if (FtpDir(String_cString(tmpFileName),String_cString(fileName),storageHandle->ftp.control) != 1)
      {
        File_delete(tmpFileName,FALSE);
        File_deleteFileName(tmpFileName);
        FtpClose(storageHandle->ftp.control);
        free(storageHandle->ftp.readAheadBuffer.data);
        return ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(fileName));
      }
    }
    error = File_open(&fileHandle,tmpFileName,FILE_OPEN_READ);
    if (error != ERROR_NONE)
    {
      File_delete(tmpFileName,FALSE);
      File_deleteFileName(tmpFileName);
      FtpClose(storageHandle->ftp.control);
      free(storageHandle->ftp.readAheadBuffer.data);
      return error;
    }
    foundFlag = FALSE;
    line = String_new();
    while (!File_eof(&fileHandle) && !foundFlag)
    {
      error = File_readLine(&fileHandle,line);
      if (error == ERROR_NONE)
      {
        foundFlag =   String_parse(line,
                                   STRING_BEGIN,
                                   "%32s %* %* %* %llu %d-%d-%d %d:%d %S",
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL,NULL,NULL,
                                   NULL,NULL,
                                   NULL
                                  )
                   || String_parse(line,
                                   STRING_BEGIN,
                                   "%32s %* %* %* %llu %* %* %*:%* %S",
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL
                                  )
                   || String_parse(line,
                                   STRING_BEGIN,
                                   "%32s %* %* %* %llu %* %* %* %S",
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL
                                  );
      }
    }
    String_delete(line);
    File_close(&fileHandle);
    File_delete(tmpFileName,FALSE);
    File_deleteFileName(tmpFileName);
    if (!foundFlag)
    {
      FtpClose(storageHandle->ftp.control);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERRORX_(FILE_NOT_FOUND_,0,"%s",String_cString(archiveName));
    }

    // get file size
    if (FtpSize(String_cString(archiveName),
                &size,
                FTPLIB_IMAGE,
                storageHandle->ftp.control
               ) != 1
       )
    {
      FtpClose(storageHandle->ftp.control);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERROR_FTP_GET_SIZE;
    }
    storageHandle->ftp.size = (uint64)size;

    // init FTP download: first try non-passive, then passive mode
    FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,storageHandle->ftp.control);
    if (FtpAccess(String_cString(archiveName),
                  FTPLIB_FILE_READ,
                  FTPLIB_IMAGE,
                  storageHandle->ftp.control,
                  &storageHandle->ftp.data
                 ) != 1
       )
    {
      FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,storageHandle->ftp.control);
      if (FtpAccess(String_cString(archiveName),
                    FTPLIB_FILE_READ,
                    FTPLIB_IMAGE,
                    storageHandle->ftp.control,
                    &storageHandle->ftp.data
                   ) != 1
         )
      {
        FtpClose(storageHandle->ftp.control);
        free(storageHandle->ftp.readAheadBuffer.data);
        return ERRORX_(OPEN_FILE,0,"ftp access");
      }
    }

    DEBUG_ADD_RESOURCE_TRACE("storage open ftp",&storageHandle->ftp,sizeof(storageHandle->ftp));

    return ERROR_NONE;
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL void StorageFTP_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_FTP);

  #if   defined(HAVE_CURL)
    assert(storageHandle->ftp.curlHandle != NULL);
    assert(storageHandle->ftp.curlMultiHandle != NULL);

    DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->ftp,sizeof(storageHandle->ftp));

    (void)curl_multi_remove_handle(storageHandle->ftp.curlMultiHandle,storageHandle->ftp.curlHandle);
    (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
    (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
  #elif defined(HAVE_FTP)
    assert(storageHandle->ftp.control != NULL);

    DEBUG_REMOVE_RESOURCE_TRACE(&storageHandle->ftp);

    switch (storageHandle->mode)
    {
      case STORAGE_MODE_UNKNOWN:
        break;
      case STORAGE_MODE_WRITE:
        free(storageHandle->ftp.readAheadBuffer.data);
        if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
        {
          assert(storageHandle->ftp.data != NULL);
          FtpClose(storageHandle->ftp.data);
        }
        break;
      case STORAGE_MODE_READ:
        assert(storageHandle->ftp.data != NULL);
        FtpClose(storageHandle->ftp.data);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    FtpClose(storageHandle->ftp.control);
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL bool StorageFTP_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_FTP);

  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
      return storageHandle->ftp.index >= storageHandle->ftp.size;
    }
    else
    {
      return TRUE;
    }
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);
    return TRUE;
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL Errors StorageFTP_read(StorageHandle *storageHandle,
                             void          *buffer,
                             ulong         bufferSize,
                             ulong         *bytesRead
                            )
{
  Errors error;
  #if   defined(HAVE_CURL)
    ulong     index;
    ulong     bytesAvail;
    ulong     length;
    uint64    startTimestamp,endTimestamp;
    CURLMcode curlmCode;
    int       runningHandles;
  #elif defined(HAVE_FTP)
    ulong   index;
    ulong   bytesAvail;
    ulong   length;
    uint64  startTimestamp,endTimestamp;
    ssize_t n;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_FTP);
  assert(buffer != NULL);

  error = ERROR_NONE;
  #if   defined(HAVE_CURL)
    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
      assert(storageHandle->ftp.curlMultiHandle != NULL);
      assert(storageHandle->ftp.readAheadBuffer.data != NULL);

      // copy as much data as available from read-ahead buffer
      if (   (storageHandle->ftp.index >= storageHandle->ftp.readAheadBuffer.offset)
          && (storageHandle->ftp.index < (storageHandle->ftp.readAheadBuffer.offset+storageHandle->ftp.readAheadBuffer.length))
         )
      {
        // copy data from read-ahead buffer
        index      = (ulong)(storageHandle->ftp.index-storageHandle->ftp.readAheadBuffer.offset);
        bytesAvail = MIN(bufferSize,storageHandle->ftp.readAheadBuffer.length-index);
        memcpy(buffer,storageHandle->ftp.readAheadBuffer.data+index,bytesAvail);

        // adjust buffer, bufferSize, bytes read, index
        buffer = (byte*)buffer+bytesAvail;
        bufferSize -= bytesAvail;
        if (bytesRead != NULL) (*bytesRead) += bytesAvail;
        storageHandle->ftp.index += (uint64)bytesAvail;
      }

      // read rest of data
      while (   (bufferSize > 0L)
             && (storageHandle->ftp.index < storageHandle->ftp.size)
             && (error == ERROR_NONE)
            )
      {
        assert(storageHandle->ftp.index >= (storageHandle->ftp.readAheadBuffer.offset+storageHandle->ftp.readAheadBuffer.length));

        // get max. number of bytes to receive in one step
        if (storageHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(storageHandle->ftp.bandWidthLimiter.blockSize,bufferSize);
        }
        else
        {
          length = bufferSize;
        }
        assert(length > 0L);

        // get start time
        startTimestamp = Misc_getTimestamp();

        if (length < BUFFER_SIZE)
        {
          // read into read-ahead buffer
          storageHandle->ftp.buffer          = storageHandle->ftp.readAheadBuffer.data;
          storageHandle->ftp.length          = MIN((size_t)(storageHandle->ftp.size-storageHandle->ftp.index),BUFFER_SIZE);
          storageHandle->ftp.transferedBytes = 0L;
          runningHandles = 1;
          while (   (storageHandle->ftp.transferedBytes < storageHandle->ftp.length)
                 && (error == ERROR_NONE)
                 && (runningHandles > 0)
                )
          {
            // wait for socket
            error = waitCurlSocket(storageHandle->ftp.curlMultiHandle);

            // perform curl action
            if (error == ERROR_NONE)
            {
              do
              {
                curlmCode = curl_multi_perform(storageHandle->ftp.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: %ld %ld\n",__FILE__,__LINE__,storageHandle->ftp.transferedBytes,storageHandle->ftp.length);
              }
              while (   (curlmCode == CURLM_CALL_MULTI_PERFORM)
                     && (runningHandles > 0)
                    );
              if (curlmCode != CURLM_OK)
              {
                error = ERRORX_(NETWORK_RECEIVE,0,"%s",curl_multi_strerror(curlmCode));
              }
            }
          }
          if (error != ERROR_NONE)
          {
            break;
          }
          if (storageHandle->ftp.transferedBytes <= 0L)
          {
            error = ERROR_IO_ERROR;
            break;
          }
          storageHandle->ftp.readAheadBuffer.offset = storageHandle->ftp.index;
          storageHandle->ftp.readAheadBuffer.length = storageHandle->ftp.transferedBytes;

          // copy data from read-ahead buffer
          bytesAvail = MIN(length,storageHandle->ftp.readAheadBuffer.length);
          memcpy(buffer,storageHandle->ftp.readAheadBuffer.data,bytesAvail);

          // adjust buffer, bufferSize, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          bufferSize -= bytesAvail;
          if (bytesRead != NULL) (*bytesRead) += bytesAvail;
          storageHandle->ftp.index += (uint64)bytesAvail;
        }
        else
        {
          // read direct
          storageHandle->ftp.buffer          = buffer;
          storageHandle->ftp.length          = length;
          storageHandle->ftp.transferedBytes = 0L;
          runningHandles = 1;
          while (   (storageHandle->ftp.transferedBytes < storageHandle->ftp.length)
                 && (error == ERROR_NONE)
                 && (runningHandles > 0)
                )
          {
            // wait for socket
            error = waitCurlSocket(storageHandle->ftp.curlMultiHandle);

            // perform curl action
            if (error == ERROR_NONE)
            {
              do
              {
                curlmCode = curl_multi_perform(storageHandle->ftp.curlMultiHandle,&runningHandles);
              }
              while (   (curlmCode == CURLM_CALL_MULTI_PERFORM)
                     && (runningHandles > 0)
                    );
              if (curlmCode != CURLM_OK)
              {
                error = ERRORX_(NETWORK_RECEIVE,0,"%s",curl_multi_strerror(curlmCode));
              }
            }
          }
          if (error != ERROR_NONE)
          {
            break;
          }
          if (storageHandle->ftp.transferedBytes <= 0L)
          {
            error = ERROR_IO_ERROR;
            break;
          }
          bytesAvail = storageHandle->ftp.transferedBytes;

          // adjust buffer, bufferSize, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          bufferSize -= bytesAvail;
          if (bytesRead != NULL) (*bytesRead) += bytesAvail;
          storageHandle->ftp.index += (uint64)bytesAvail;
        }

        // get end time
        endTimestamp = Misc_getTimestamp();

        /* limit used band width if requested (note: when the system time is
           changing endTimestamp may become smaller than startTimestamp;
           thus do not check this with an assert())
        */
        if (endTimestamp >= startTimestamp)
        {
          limitBandWidth(&storageHandle->ftp.bandWidthLimiter,
                         bytesAvail,
                         endTimestamp-startTimestamp
                        );
        }
      }
    }
  #elif defined(HAVE_FTP)
    if ((storageHandle->storageHandle->jobOptions == NULL) || !storageHandle->storageHandle->jobOptions->dryRunFlag)
    {
      assert(storageHandle->ftp.control != NULL);
      assert(storageHandle->ftp.data != NULL);
      assert(storageHandle->ftp.readAheadBuffer.data != NULL);

      // copy as much data as available from read-ahead buffer
      if (   (storageHandle->ftp.index >= storageHandle->ftp.readAheadBuffer.offset)
          && (storageHandle->ftp.index < (storageHandle->ftp.readAheadBuffer.offset+storageHandle->ftp.readAheadBuffer.length))
         )
      {
        // copy data from read-ahead buffer
        index = (ulong)(storageHandle->ftp.index-storageHandle->ftp.readAheadBuffer.offset);
        bytesAvail = MIN(bufferSize,storageHandle->ftp.readAheadBuffer.length-index);
        memcpy(buffer,storageHandle->ftp.readAheadBuffer.data+index,bytesAvail);

        // adjust buffer, bufferSize, bytes read, index
        buffer = (byte*)buffer+bytesAvail;
        bufferSize -= bytesAvail;
        if (bytesRead != NULL) (*bytesRead) += bytesAvail;
        storageHandle->ftp.index += (uint64)bytesAvail;
      }

      // read rest of data
      while (   (bufferSize > 0L)
             && (storageHandle->ftp.index < storageHandle->ftp.size)
             && (error == ERROR_NONE)
            )
      {
        assert(storageHandle->ftp.index >= (storageHandle->ftp.readAheadBuffer.offset+storageHandle->ftp.readAheadBuffer.length));

        // get max. number of bytes to receive in one step
        if (storageHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(storageHandle->ftp.bandWidthLimiter.blockSize,bufferSize);
        }
        else
        {
          length = bufferSize;
        }
        assert(length > 0L);

        // get start time
        startTimestamp = Misc_getTimestamp();

        if (length < BUFFER_SIZE)
        {
          // read into read-ahead buffer
          n = FtpRead(storageHandle->ftp.readAheadBuffer.data,
                      MIN((size_t)(storageHandle->ftp.size-storageHandle->ftp.index),BUFFER_SIZE),
                      storageHandle->ftp.data
                    );
          if (n == 0)
          {
            // wait a short time for more data
            Misc_udelay(250LL*MISC_US_PER_MS);

            // read into read-ahead buffer
            n = FtpRead(storageHandle->ftp.readAheadBuffer.data,
                        MIN((size_t)(storageHandle->ftp.size-storageHandle->ftp.index),BUFFER_SIZE),
                        storageHandle->ftp.data
                       );
          }
          if (n <= 0)
          {
            error = ERROR_(IO_ERROR,errno);
            break;
          }
          storageHandle->ftp.readAheadBuffer.offset = storageHandle->ftp.index;
          storageHandle->ftp.readAheadBuffer.length = (ulong)n;

          // copy data from read-ahead buffer
          bytesAvail = MIN(length,storageHandle->ftp.readAheadBuffer.length);
          memcpy(buffer,storageHandle->ftp.readAheadBuffer.data,bytesAvail);

          // adjust buffer, bufferSize, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          bufferSize -= bytesAvail;
          if (bytesRead != NULL) (*bytesRead) += bytesAvail;
          storageHandle->ftp.index += (uint64)bytesAvail;
        }
        else
        {
          // read direct
          n = FtpRead(buffer,
                      length,
                      storageHandle->ftp.data
                     );
          if (n == 0)
          {
            // wait a short time for more data
            Misc_udelay(250LL*MISC_US_PER_MS);

            // read direct
            n = FtpRead(buffer,
                        bufferSize,
                        storageHandle->ftp.data
                       );
          }
          if (n <= 0)
          {
            error = ERROR_(IO_ERROR,errno);
            break;
          }
          bytesAvail = (ulong)n;

          // adjust buffer, bufferSize, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          bufferSize -= bytesAvail;
          if (bytesRead != NULL) (*bytesRead) += bytesAvail;
          storageHandle->ftp.index += (uint64)bytesAvail;
        }

        // get end time
        endTimestamp = Misc_getTimestamp();

        /* limit used band width if requested (note: when the system time is
           changing endTimestamp may become smaller than startTimestamp;
           thus do not check this with an assert())
        */
        if (endTimestamp >= startTimestamp)
        {
          limitBandWidth(&storageHandle->ftp.bandWidthLimiter,
                         bytesAvail,
                         endTimestamp-startTimestamp
                        );
        }
      }
    }
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferSize);
    UNUSED_VARIABLE(bytesRead);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageFTP_write(StorageHandle *storageHandle,
                              const void    *buffer,
                              ulong         bufferLength
                             )
{
  Errors error;
  #if   defined(HAVE_CURL)
    ulong     writtenBytes;
    ulong     length;
    uint64    startTimestamp,endTimestamp;
    CURLMcode curlmCode;
    int       runningHandles;
  #elif defined(HAVE_FTP)
    ulong  writtenBytes;
    ulong  length;
    uint64 startTimestamp,endTimestamp;
    long   n;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_FTP);
  assert(buffer != NULL);

  error = ERROR_NONE;
  #if   defined(HAVE_CURL)
    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
      assert(storageHandle->ftp.curlMultiHandle != NULL);

      writtenBytes = 0L;
      while (writtenBytes < bufferLength)
      {
        // get max. number of bytes to send in one step
        if (storageHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(storageHandle->ftp.bandWidthLimiter.blockSize,bufferLength-writtenBytes);
        }
        else
        {
          length = bufferLength-writtenBytes;
        }
        assert(length > 0L);

        // get start time
        startTimestamp = Misc_getTimestamp();

        // send data
        storageHandle->ftp.buffer          = (void*)buffer;
        storageHandle->ftp.length          = length;
        storageHandle->ftp.transferedBytes = 0L;
        runningHandles = 1;
        while (   (storageHandle->ftp.transferedBytes < storageHandle->ftp.length)
               && (error == ERROR_NONE)
               && (runningHandles > 0)
              )
        {
          // wait for socket
          error = waitCurlSocket(storageHandle->ftp.curlMultiHandle);

          // perform curl action
          if (error == ERROR_NONE)
          {
            do
            {
              curlmCode = curl_multi_perform(storageHandle->ftp.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: %ld %ld\n",__FILE__,__LINE__,storageHandle->ftp.transferedBytes,storageHandle->ftp.length);
            }
            while (   (curlmCode == CURLM_CALL_MULTI_PERFORM)
                   && (runningHandles > 0)
                  );
            if (curlmCode != CURLM_OK)
            {
              error = ERRORX_(NETWORK_SEND,0,"%s",curl_multi_strerror(curlmCode));
            }
          }
        }
        if (error != ERROR_NONE)
        {
          break;
        }
        if      (storageHandle->ftp.transferedBytes <= 0L)
        {
          error = ERROR_NETWORK_SEND;
          break;
        }
        buffer = (byte*)buffer+storageHandle->ftp.transferedBytes;
        writtenBytes += storageHandle->ftp.transferedBytes;

        // get end time
        endTimestamp = Misc_getTimestamp();

        /* limit used band width if requested (note: when the system time is
           changing endTimestamp may become smaller than startTimestamp;
           thus do not check this with an assert())
        */
        if (endTimestamp >= startTimestamp)
        {
          limitBandWidth(&storageHandle->ftp.bandWidthLimiter,
                         storageHandle->ftp.transferedBytes,
                         endTimestamp-startTimestamp
                        );
        }
      }
    }
  #elif defined(HAVE_FTP)
    if ((storageHandle->storageHandle->jobOptions == NULL) || !storageHandle->storageHandle->jobOptions->dryRunFlag)
    {
      assert(storageHandle->ftp.control != NULL);
      assert(storageHandle->ftp.data != NULL);

      writtenBytes = 0L;
      while (writtenBytes < bufferLength)
      {
        // get max. number of bytes to send in one step
        if (storageHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(storageHandle->ftp.bandWidthLimiter.blockSize,bufferLength-writtenBytes);
        }
        else
        {
          length = bufferLength-writtenBytes;
        }
        assert(length > 0L);

        // get start time
        startTimestamp = Misc_getTimestamp();

        // send data
        n = FtpWrite((void*)buffer,length,storageHandle->ftp.data);
        if      (n < 0)
        {
          error = ERROR_NETWORK_SEND;
          break;
        }
        else if (n == 0)
        {
          n = FtpWrite((void*)buffer,length,storageHandle->ftp.data);
          if      (n <= 0)
          {
            error = ERROR_NETWORK_SEND;
            break;
          }
        }
        buffer = (byte*)buffer+n;
        writtenBytes += (ulong)n;

        // get end time
        endTimestamp = Misc_getTimestamp();

        /* limit used band width if requested (note: when the system time is
           changing endTimestamp may become smaller than startTimestamp;
           thus do not check this with an assert())
        */
        if (endTimestamp >= startTimestamp)
        {
          limitBandWidth(&storageHandle->ftp.bandWidthLimiter,
                         (ulong)n,
                         endTimestamp-startTimestamp
                        );
        }
      }
    }
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferLength);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageFTP_tell(StorageHandle *storageHandle,
                             uint64        *offset
                            )
{
  Errors error;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_FTP);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
      (*offset) = storageHandle->ftp.index;
      error     = ERROR_NONE;
    }
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL Errors StorageFTP_seek(StorageHandle *storageHandle,
                             uint64        offset
                            )
{
  Errors error;
  #if   defined(HAVE_CURL)
     uint64    skip;
     ulong     i;
     ulong     n;
     CURLMcode curlmCode;
     int       runningHandles;
  #elif defined(HAVE_FTP)
     uint64 skip;
     ulong  i;
     ulong  n;
     int    readBytes;
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_FTP);

  error = ERROR_NONE;
  /* ftp protocol does not support a seek-function. Thus try to
     read and discard data to position the read index to the
     requested offset.
     Note: this is slow!

     Idea: Can ftp REST be used to implement a seek-function?
           With curl: CURLOPT_RESUME_FROM_LARGE?
           http://tools.ietf.org/html/rfc959
  */
  #if   defined(HAVE_CURL)
    {
      if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
      {
        assert(storageHandle->ftp.readAheadBuffer.data != NULL);

        if      (offset > storageHandle->ftp.index)
        {
          // calculate number of bytes to skip
          skip = offset-storageHandle->ftp.index;

          while (skip > 0LL)
          {
            // skip data in read-ahead buffer
            if (   (storageHandle->ftp.index >= storageHandle->ftp.readAheadBuffer.offset)
                && (storageHandle->ftp.index < (storageHandle->ftp.readAheadBuffer.offset+storageHandle->ftp.readAheadBuffer.length))
               )
            {
              i = (ulong)storageHandle->ftp.index-storageHandle->ftp.readAheadBuffer.offset;
              n = MIN(skip,storageHandle->ftp.readAheadBuffer.length-i);
              skip -= (uint64)n;
              storageHandle->ftp.index += (uint64)n;
            }

            if (skip > 0LL)
            {
              assert(storageHandle->ftp.index >= (storageHandle->ftp.readAheadBuffer.offset+storageHandle->ftp.readAheadBuffer.length));

              // read data into read-ahread buffer
              storageHandle->ftp.buffer          = storageHandle->ftp.readAheadBuffer.data;
              storageHandle->ftp.length          = MIN((size_t)(storageHandle->ftp.size-storageHandle->ftp.index),BUFFER_SIZE);
              storageHandle->ftp.transferedBytes = 0L;
              runningHandles = 1;
              while (   (storageHandle->ftp.transferedBytes < storageHandle->ftp.length)
                     && (error == ERROR_NONE)
                     && (runningHandles > 0)
                    )
              {
                // wait for socket
                error = waitCurlSocket(storageHandle->ftp.curlMultiHandle);

                // perform curl action
                if (error == ERROR_NONE)
                {
                  do
                  {
                    curlmCode = curl_multi_perform(storageHandle->ftp.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: %ld %ld\n",__FILE__,__LINE__,storageHandle->ftp.transferedBytes,storageHandle->ftp.length);
                  }
                  while (   (curlmCode == CURLM_CALL_MULTI_PERFORM)
                         && (runningHandles > 0)
                        );
                  if (curlmCode != CURLM_OK)
                  {
                    error = ERRORX_(NETWORK_RECEIVE,0,"%s",curl_multi_strerror(curlmCode));
                  }
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
              if (storageHandle->ftp.transferedBytes <= 0L)
              {
                error = ERROR_IO_ERROR;
                break;
              }
              storageHandle->ftp.readAheadBuffer.offset = storageHandle->ftp.index;
              storageHandle->ftp.readAheadBuffer.length = (uint64)storageHandle->ftp.transferedBytes;
            }
          }
        }
        else if (offset < storageHandle->ftp.index)
        {
          error = ERROR_FUNCTION_NOT_SUPPORTED;
        }
      }
    }
  #elif defined(HAVE_FTP)
    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
      assert(storageHandle->ftp.readAheadBuffer.data != NULL);

      if      (offset > storageHandle->ftp.index)
      {
        skip = offset-storageHandle->ftp.index;
        while (skip > 0LL)
        {
          // skip data in read-ahead buffer
          if (   (storageHandle->ftp.index >= storageHandle->ftp.readAheadBuffer.offset)
              && (storageHandle->ftp.index < (storageHandle->ftp.readAheadBuffer.offset+storageHandle->ftp.readAheadBuffer.length))
             )
          {
            i = (ulong)storageHandle->ftp.index-storageHandle->ftp.readAheadBuffer.offset;
            n = MIN(skip,storageHandle->ftp.readAheadBuffer.length-i);
            skip -= (uint64)n;
            storageHandle->ftp.index += (uint64)n;
          }

          if (skip > 0LL)
          {
            assert(storageHandle->ftp.index >= (storageHandle->ftp.readAheadBuffer.offset+storageHandle->ftp.readAheadBuffer.length));

            // read data
            readBytes = FtpRead(storageHandle->ftp.readAheadBuffer.data,
                                MIN((size_t)skip,BUFFER_SIZE),
                                storageHandle->ftp.data
                               );
            if (readBytes == 0)
            {
              // wait a short time for more data
              Misc_udelay(250LL*MISC_US_PER_MS);

              // read into read-ahead buffer
              readBytes = FtpRead(storageHandle->ftp.readAheadBuffer.data,
                                  MIN((size_t)skip,BUFFER_SIZE),
                                  storageHandle->ftp.data
                                 );
            }
            if (readBytes <= 0)
            {
              error = ERROR_(IO_ERROR,errno);
              break;
            }
            storageHandle->ftp.readAheadBuffer.offset = storageHandle->ftp.index;
            storageHandle->ftp.readAheadBuffer.length = (uint64)readBytes;
          }
        }
      }
      else if (offset < storageHandle->ftp.index)
      {
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      }
    }
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
  assert(error != ERROR_UNKNOWN);

  return error;
}

LOCAL uint64 StorageFTP_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageHandle);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->type == STORAGE_TYPE_FTP);

  size = 0LL;

  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
      size = storageHandle->ftp.size;
    }
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_CURL || HAVE_FTP */

  return size;
}

LOCAL Errors StorageFTP_rename(const StorageInfo *storageInfo,
                               ConstString       fromArchiveName,
                               ConstString       toArchiveName
                              )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_FTP);

UNUSED_VARIABLE(fromArchiveName);
UNUSED_VARIABLE(toArchiveName);
error = ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

LOCAL Errors StorageFTP_delete(StorageInfo  *storageInfo,
                                ConstString archiveName
                               )
{
  Errors      error;
  #if   defined(HAVE_CURL)
    CURL              *curlHandle;
    String            directoryName,baseName;
    String            url;
    CURLcode          curlCode;
    StringTokenizer   nameTokenizer;
    ConstString       token;
    String            ftpCommand;
    struct curl_slist *curlSList;
  #elif defined(HAVE_FTP)
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageInfo != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);
  assert(storageInfo->type == STORAGE_TYPE_FTP);
  assert(!String_isEmpty(archiveName));

  error = ERROR_UNKNOWN;
  #if   defined(HAVE_CURL)
    // open Curl handle
    curlHandle = curl_easy_init();
    if (curlHandle != NULL)
    {
      // get directory name, base name
      directoryName = File_getDirectoryName(String_new(),archiveName);
      baseName      = File_getBaseName(String_new(),archiveName);

      // get URL
      url = String_format(String_new(),"ftp://%S",storageInfo->storageSpecifier.hostName);
      if (storageInfo->storageSpecifier.hostPort != 0) String_format(url,":d",storageInfo->storageSpecifier.hostPort);
      File_initSplitFileName(&nameTokenizer,directoryName);
      while (File_getNextSplitFileName(&nameTokenizer,&token))
      {
        String_appendChar(url,'/');
        String_append(url,token);
      }
      File_doneSplitFileName(&nameTokenizer);
      String_append(url,baseName);

      if ((storageInfo->jobOptions == NULL) || !storageInfo->jobOptions->dryRunFlag)
      {
        // delete file
        curlCode = initFTPHandle(curlHandle,
                                 url,
                                 storageInfo->storageSpecifier.loginName,
                                 storageInfo->storageSpecifier.loginPassword,
                                 FTP_TIMEOUT
                                );
        if (curlCode == CURLE_OK)
        {
          ftpCommand = String_format(String_new(),"*DELE %S",archiveName);
          curlSList = curl_slist_append(NULL,String_cString(ftpCommand));
          curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(curlHandle,CURLOPT_QUOTE,curlSList);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_perform(curlHandle);
          }
          (void)curl_easy_setopt(curlHandle,CURLOPT_QUOTE,NULL);
          if (curlCode == CURLE_OK)
          {
            error = ERROR_NONE;
          }
          else
          {
            error = ERRORX_(DELETE_FILE,0,"%s",curl_multi_strerror(curlCode));
          }
          curl_slist_free_all(curlSList);
          String_delete(ftpCommand);
        }
      }
      else
      {
        error = ERROR_NONE;
      }

      // free resources
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      (void)curl_easy_cleanup(curlHandle);
    }
    else
    {
      error = ERROR_FTP_SESSION_FAIL;
    }
  #elif defined(HAVE_FTP)
    assert(storageInfo->ftp.data != NULL);

    storageFileName = (archiveName != NULL) ? archiveName : storageInfo->storageSpecifier.archiveName;

    if ((storageHandle->storageInfo->jobOptions == NULL) || !storageHandle->storageInfo->jobOptions->dryRunFlag)
    {
      error = (FtpDelete(String_cString(storageFileName),storageInfo->ftp.data) == 1) ? ERROR_NONE : ERROR_DELETE_FILE;
    }
    else
    {
      error = ERROR_NONE;
    }
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
  assert(error != ERROR_UNKNOWN);

  return error;
}

#if 0
still not complete
LOCAL Errors StorageFTP_getFileInfo(const StorageInfo *storageInfo,
                                    ConstString       fileName,
                                    FileInfo          *fileInfo
                                   )
{
  String infoFileName;
  Errors error;
  #if   defined(HAVE_CURL)
    Server            server;
    CURL              *curlHandle;
    String            directoryName,baseName;
    String            url;
    CURLcode          curlCode;
    const char        *plain;
    StringTokenizer   nameTokenizer;
    ConstString       token;
    String            ftpCommand;
    struct curl_slist *curlSList;
  #elif defined(HAVE_FTP)
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageInfo != NULL);
  assert(fileInfo != NULL);
  assert(storageInfo->type == STORAGE_TYPE_FTP);
  DEBUG_CHECK_RESOURCE_TRACE(storageInfo);

  infoFileName = (fileName != NULL) ? fileName : storageInfo->storageSpecifier.archiveName;
  memset(fileInfo,0,sizeof(fileInfo));

  error = ERROR_UNKNOWN;
  #if   defined(HAVE_CURL)
    // get FTP server settings
    getFTPServerSettings(storageInfo->storageSpecifier.hostName,storageInfo->jobOptions,&server);
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_set(storageInfo->storageSpecifier.loginName,ftpServer.loginName);
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageInfo->storageSpecifier.loginName)) String_setCString(storageInfo->storageSpecifier.loginName,getenv("USER"));
    if (String_isEmpty(storageInfo->storageSpecifier.hostName))
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
    if (curlHandle != NULL)
    {
      freeServer(ftpServer);
      return = ERROR_FTP_SESSION_FAIL;
    }

    // get directory name, base name
    directoryName = File_getDirectoryName(String_new(),infoFileName);
    baseName      = File_getBaseName(String_new(),infoFileName);

    // get URL
    url = String_format(String_new(),"ftp://%S",storageInfo->storageSpecifier.hostName);
    if (storageInfo->storageSpecifier.hostPort != 0) String_format(url,":d",storageInfo->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,pathName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    File_doneSplitFileName(&nameTokenizer);
    String_append(url,baseName);

    // get file info
    ftpCommand = String_format(String_new(),"*DIR %S",infoFileName);
    curlSList = curl_slist_append(NULL,String_cString(ftpCommand));
    curlCode = initFTPHandle(curlHandle,
                             url,
                             storageInfo->storageSpecifier.loginName,
                             storageInfo->storageSpecifier.loginPassword,
                             FTP_TIMEOUT
                            );
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_QUOTE,curlSList);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_perform(curlHandle);
    }
    (void)curl_easy_setopt(curlHandle,CURLOPT_QUOTE,NULL);
    if (curlCode == CURLE_OK)
    {
      error = ERROR_NONE;
    }
    else
    {
      error = ERRORX_(DELETE_FILE,0,"%s",curl_multi_strerror(curlCode));
    }
    curl_slist_free_all(curlSList);
    String_delete(ftpCommand);

    // free resources
    String_delete(url);
    String_delete(baseName);
    String_delete(pathName);
    (void)curl_easy_cleanup(curlHandle);
    freeServer(&server);
  #elif defined(HAVE_FTP)
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
  #else /* not HAVE_CURL || HAVE_FTP */
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
  assert(error != ERROR_UNKNOWN);

  return error;
}
#endif /* 0 */

/*---------------------------------------------------------------------*/

LOCAL Errors StorageFTP_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                          const StorageSpecifier     *storageSpecifier,
                                          ConstString                archiveName,
                                          const JobOptions           *jobOptions,
                                          ServerConnectionPriorities serverConnectionPriority
                                         )
{
  #if   defined(HAVE_CURL)
    Errors          error;
    AutoFreeList    autoFreeList;
    FTPServer       ftpServer;
    CURL            *curlHandle;
    String          url;
    CURLcode        curlCode;
    StringTokenizer nameTokenizer;
    ConstString     token;
  #elif defined(HAVE_FTP)
    Errors       error;
    AutoFreeList autoFreeList;
    FTPServer    ftpServer;
    const char   *plain;
    netbuf       *control;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_FTP);
  assert(!String_isEmpty(archiveName));

  UNUSED_VARIABLE(storageSpecifier);
  #if !defined(HAVE_CURL) && !defined(HAVE_FTP) && (!defined(HAVE_CURL) || !defined(HAVE_MXML))
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif

  // open directory listing
  #if   defined(HAVE_CURL)
    // init variables
    AutoFree_init(&autoFreeList);
    storageDirectoryListHandle->type         = STORAGE_TYPE_FTP;
    StringList_init(&storageDirectoryListHandle->ftp.lineList);
    storageDirectoryListHandle->ftp.fileName = String_new();
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->ftp.fileName,{ String_delete(storageDirectoryListHandle->ftp.fileName); });
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->ftp.lineList,{ StringList_done(&storageDirectoryListHandle->ftp.lineList); });

    // get FTP server settings
    storageDirectoryListHandle->ftp.serverId = getFTPServerSettings(storageDirectoryListHandle->storageSpecifier.hostName,jobOptions,&ftpServer);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_set(storageDirectoryListHandle->storageSpecifier.loginName,ftpServer.loginName);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("USER"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate FTP server
    if (!allocateServer(storageDirectoryListHandle->ftp.serverId,serverConnectionPriority,60*1000L))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->ftp.serverId,{ freeServer(storageDirectoryListHandle->ftp.serverId); });

    // check FTP login, get correct password
    error = ERROR_UNKNOWN;
    if ((error != ERROR_NONE) && !Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword))
    {
      error = checkFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.loginName,
                            storageDirectoryListHandle->storageSpecifier.loginPassword
                           );
    }
    if ((error != ERROR_NONE) && !Password_isEmpty(ftpServer.password))
    {
      error = checkFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.loginName,
                            ftpServer.password
                           );
      if (error == ERROR_NONE)
      {
        Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,ftpServer.password);
      }
    }
    if ((error != ERROR_NONE) && !Password_isEmpty(defaultFTPPassword))
    {
      error = checkFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.loginName,
                            defaultFTPPassword
                           );
      if (error == ERROR_NONE)
      {
        Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,defaultFTPPassword);
      }
    }
    if (error != ERROR_NONE)
    {
      // initialize default password
      while (   (error != ERROR_NONE)
             && initFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                             storageDirectoryListHandle->storageSpecifier.loginName,
                             storageDirectoryListHandle->storageSpecifier.loginPassword,
                             jobOptions,
//TODO
                             CALLBACK(NULL,NULL) // CALLBACK(storageInfo->getPasswordFunction,storageInfo->getPasswordUserData)
                            )
            )
      {
        error = checkFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                              storageDirectoryListHandle->storageSpecifier.hostPort,
                              storageDirectoryListHandle->storageSpecifier.loginName,
                              storageDirectoryListHandle->storageSpecifier.loginPassword
                             );
        if (error == ERROR_NONE)
        {
          Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,defaultFTPPassword);
        }
      }
      if (error != ERROR_NONE)
      {
        error = (!Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword) || !Password_isEmpty(ftpServer.password) || !Password_isEmpty(defaultFTPPassword))
                  ? ERRORX_(INVALID_FTP_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName))
                  : ERRORX_(NO_FTP_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName));
      }

      // store passwrd as default FTP password
      if (error == ERROR_NONE)
      {
        if (defaultFTPPassword == NULL) defaultFTPPassword = Password_new();
        Password_set(defaultFTPPassword,storageDirectoryListHandle->storageSpecifier.loginPassword);
      }
    }
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // init Curl handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_FTP_SESSION_FAIL;
    }

    // get URL
    url = String_format(String_new(),"ftp://%S",storageDirectoryListHandle->storageSpecifier.hostName);
    if (storageDirectoryListHandle->storageSpecifier.hostPort != 0) String_format(url,":d",storageDirectoryListHandle->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,archiveName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    File_doneSplitFileName(&nameTokenizer);

    // read directory
    curlCode = initFTPHandle(curlHandle,
                             url,
                             storageDirectoryListHandle->storageSpecifier.loginName,
                             storageDirectoryListHandle->storageSpecifier.loginPassword,
                             FTP_TIMEOUT
                            );
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_WRITEFUNCTION,curlFTPParseDirectoryListCallback);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_WRITEDATA,storageDirectoryListHandle);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_perform(curlHandle);
    }
    if (curlCode != CURLE_OK)
    {
      error = ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // free resources
    String_delete(url);
    (void)curl_easy_cleanup(curlHandle);
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #elif defined(HAVE_FTP)
    // init variables
    AutoFree_init(&autoFreeList);
    storageDirectoryListHandle->type                 = STORAGE_TYPE_FTP;
    storageDirectoryListHandle->ftp.fileListFileName = String_new();
    storageDirectoryListHandle->ftp.line             = String_new();
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->ftp.line,{ String_delete(storageDirectoryListHandle->ftp.line); });
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->ftp.fileListFileName,{ String_delete(storageDirectoryListHandle->ftp.fileListFileName); });

    // create temporary list file
    error = File_getTmpFileName(storageDirectoryListHandle->ftp.fileListFileName,NULL,tmpDirectory);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->ftp.fileListFileName,{ File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE); });

    // get FTP server settings
    getFTPServerSettings(storageDirectoryListHandle->storageSpecifier.hostName,jobOptions,&ftpServer);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_set(storageDirectoryListHandle->storageSpecifier.loginName,ftpServer.loginName);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("LOGNAME"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.loginName)) String_setCString(storageDirectoryListHandle->storageSpecifier.loginName,getenv("USER"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // check FTP login, get correct password
    error = ERROR_UNKNOWN;
    if ((error != ERROR_NONE) && !Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword))
    {
      error = checkFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.loginName,
                            storageDirectoryListHandle->storageSpecifier.loginPassword
                           );
    }
    if ((error != ERROR_NONE) && (ftpServer.password != NULL))
    {
      error = checkFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.loginName,
                            ftpServer.password
                           );
      if (error == ERROR_NONE)
      {
        Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,ftpServer.password);
      }
    }
    if ((error != ERROR_NONE) && !Password_isEmpty(defaultFTPPassword))
    {
      error = checkFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.loginName,
                            defaultFTPPassword
                           );
      if (error == ERROR_NONE)
      {
        Password_set(loginPassword,defaultFTPPassword);
      }
    }
    if (error != ERROR_NONE)
    {
      // initialize default password
      while (   (error != ERROR_NONE)
             && initFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                             storageDirectoryListHandle->storageSpecifier.loginName,
                             jobOptions,
//TODO
                             CALLBACK(NULL,NULL)  // CALLBACK(storageInfo->getPasswordFunction,storageInfo->getPasswordUserData)
                            )
            )
      {
        error = checkFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                              storageDirectoryListHandle->storageSpecifier.hostPort,
                              storageDirectoryListHandle->storageSpecifier.loginName,
                              defaultFTPPassword
                             );
        if (error == ERROR_NONE)
        {
          Password_set(storageDirectoryListHandle->storageSpecifier.loginPassword,defaultFTPPassword);
        }
      }
      if (error != ERROR_NONE)
      {
        error = (!Password_isEmpty(storageDirectoryListHandle->storageSpecifier.loginPassword) || !Password_isEmpty(ftpServer.password) || !Password_isEmpty(defaultFTPPassword))
                  ? ERRORX_(ERROR_INVALID_FTP_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName))
                  : ERRORX_(ERROR_NO_FTP_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
      }

      // store passwrd as default FTP password
      if (error == ERROR_NONE)
      {
        if (defaultFTPPassword == NULL) defaultFTPPassword = Password_new();
        Password_set(defaultFTPPassword,storageInfo->storageSpecifier.loginPassword);
      }
    }
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // FTP connect
    if (!Network_hostExists(storageDirectoryListHandle->storageSpecifier.hostName))
    {
      error = ERRORX_(HOST_NOT_FOUND,0,"%s",String_cString(hostName));
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    if (FtpConnect(String_cString(hostName),&control) != 1)
    {
      error = ERROR_FTP_SESSION_FAIL;
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // FTP login
    plainPassword = Password_deploy(storageDirectoryListHandle->storageSpecifier.loginPassword);
    if (FtpLogin(String_cString(storageDirectoryListHandle->storageSpecifier.loginName),
                 plainPassword,
                 control
                ) != 1
       )
    {
      Password_undeploy(loginPassword,plainPassword);
      FtpClose(control);
      AutoFree_cleanup(&autoFreeList);
      return ERROR_FTP_AUTHENTICATION;
    }
    Password_undeploy(loginPassword,plainPassword);

    // read directory: first try non-passive, then passive mode
    FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,control);
    if (FtpDir(String_cString(storageDirectoryListHandle->ftp.fileListFileName),String_cString(archiveName),control) != 1)
    {
      FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,control);
      if (FtpDir(String_cString(storageDirectoryListHandle->ftp.fileListFileName),String_cString(archiveName),control) != 1)
      {
        error = ERRORX_(OPEN_DIRECTORY,0,"%s",String_cString(archiveName));
        FtpClose(control);
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    // disconnect
    FtpQuit(control);

    // open list file
    error = File_open(&storageDirectoryListHandle->ftp.fileHandle,storageDirectoryListHandle->ftp.fileListFileName,FILE_OPEN_READ);
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // free resources
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL void StorageFTP_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->type == STORAGE_TYPE_FTP);

  #if   defined(HAVE_CURL)
    freeServer(storageDirectoryListHandle->ftp.serverId);
    String_delete(storageDirectoryListHandle->ftp.fileName);
    StringList_done(&storageDirectoryListHandle->ftp.lineList);
  #elif defined(HAVE_FTP)
    File_close(&storageDirectoryListHandle->ftp.fileHandle);
    File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
    String_delete(storageDirectoryListHandle->ftp.line);
    String_delete(storageDirectoryListHandle->ftp.fileListFileName);
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL bool StorageFTP_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  bool endOfDirectoryFlag;
  #if   defined(HAVE_CURL)
    String line;
  #elif defined(HAVE_FTP)
    String line;
    Errors error;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->type == STORAGE_TYPE_FTP);

  endOfDirectoryFlag = TRUE;
  #if   defined(HAVE_CURL)
    while (!storageDirectoryListHandle->ftp.entryReadFlag && !StringList_isEmpty(&storageDirectoryListHandle->ftp.lineList))
    {
      // get next line
      line = StringList_removeFirst(&storageDirectoryListHandle->ftp.lineList,NULL);

      // parse
      storageDirectoryListHandle->ftp.entryReadFlag = parseFTPDirectoryLine(line,
                                                                            storageDirectoryListHandle->ftp.fileName,
                                                                            &storageDirectoryListHandle->ftp.type,
                                                                            &storageDirectoryListHandle->ftp.size,
                                                                            &storageDirectoryListHandle->ftp.timeModified,
                                                                            &storageDirectoryListHandle->ftp.userId,
                                                                            &storageDirectoryListHandle->ftp.groupId,
                                                                            &storageDirectoryListHandle->ftp.permission
                                                                           );

      // free resources
      String_delete(line);
    }

    endOfDirectoryFlag = !storageDirectoryListHandle->ftp.entryReadFlag;
  #elif defined(HAVE_FTP)
    // read line
    line = String_new();
    if (File_readLine(&storageDirectoryListHandle->ftp.fileHandle,line) == ERROR_NONE)
    {
      // parse
      storageDirectoryListHandle->ftp.entryReadFlag = parseFTPDirectoryLine(line,
                                                                            storageDirectoryListHandle->ftp.fileName,
                                                                            &storageDirectoryListHandle->ftp.type,
                                                                            &storageDirectoryListHandle->ftp.size,
                                                                            &storageDirectoryListHandle->ftp.timeModified,
                                                                            &storageDirectoryListHandle->ftp.userId,
                                                                            &storageDirectoryListHandle->ftp.groupId,
                                                                            &storageDirectoryListHandle->ftp.permission
                                                                           );
      endOfDirectoryFlag = !storageDirectoryListHandle->ftp.entryReadFlag;
    }
    String_delete(line);
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* HAVE_CURL || HAVE_FTP */

  return endOfDirectoryFlag;
}

LOCAL Errors StorageFTP_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                          String                     fileName,
                                          FileInfo                   *fileInfo
                                         )
{
  Errors error;
  #if   defined(HAVE_CURL)
    String line;
  #elif defined(HAVE_FTP)
    String line;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->type == STORAGE_TYPE_FTP);

  error = ERROR_NONE;
  #if   defined(HAVE_CURL)
    if (!storageDirectoryListHandle->ftp.entryReadFlag)
    {
      while (!storageDirectoryListHandle->ftp.entryReadFlag && !StringList_isEmpty(&storageDirectoryListHandle->ftp.lineList))
      {
        // get next line
        line = StringList_removeFirst(&storageDirectoryListHandle->ftp.lineList,NULL);

        // parse
        storageDirectoryListHandle->ftp.entryReadFlag = parseFTPDirectoryLine(line,
                                                                              fileName,
                                                                              &storageDirectoryListHandle->ftp.type,
                                                                              &storageDirectoryListHandle->ftp.size,
                                                                              &storageDirectoryListHandle->ftp.timeModified,
                                                                              &storageDirectoryListHandle->ftp.userId,
                                                                              &storageDirectoryListHandle->ftp.groupId,
                                                                              &storageDirectoryListHandle->ftp.permission
                                                                             );

        // free resources
        String_delete(line);
      }
    }

    if (storageDirectoryListHandle->ftp.entryReadFlag)
    {
      String_set(fileName,storageDirectoryListHandle->ftp.fileName);
      if (fileInfo != NULL)
      {
        fileInfo->type            = storageDirectoryListHandle->ftp.type;
        fileInfo->size            = storageDirectoryListHandle->ftp.size;
        fileInfo->timeLastAccess  = 0LL;
        fileInfo->timeModified    = storageDirectoryListHandle->ftp.timeModified;
        fileInfo->timeLastChanged = 0LL;
        fileInfo->userId          = storageDirectoryListHandle->ftp.userId;
        fileInfo->groupId         = storageDirectoryListHandle->ftp.groupId;
        fileInfo->permission      = storageDirectoryListHandle->ftp.permission;
        fileInfo->major           = 0;
        fileInfo->minor           = 0;
      }
      storageDirectoryListHandle->ftp.entryReadFlag = FALSE;

      error = ERROR_NONE;
    }
    else
    {
      error = ERROR_READ_DIRECTORY;
    }
  #elif defined(HAVE_FTP)
    if (!storageDirectoryListHandle->ftp.entryReadFlag)
    {
      // read line
      line = String_new();
      error = File_readLine(&storageDirectoryListHandle->ftp.fileHandle,line);
      if (error != ERROR_NONE)
      {
        break;
      }

      // parse
      storageDirectoryListHandle->ftp.entryReadFlag = parseFTPDirectoryLine(line,
                                                                            fileName,
                                                                            &storageDirectoryListHandle->ftp.type,
                                                                            &storageDirectoryListHandle->ftp.size,
                                                                            &storageDirectoryListHandle->ftp.timeModified,
                                                                            &storageDirectoryListHandle->ftp.userId,
                                                                            &storageDirectoryListHandle->ftp.groupId,
                                                                            &storageDirectoryListHandle->ftp.permission
                                                                           );
      String_delete(line);
    }

    if (storageDirectoryListHandle->ftp.entryReadFlag)
    {
      String_set(fileName,storageDirectoryListHandle->ftp.fileName);
      if (fileInfo != NULL)
      {
        fileInfo->type            = storageDirectoryListHandle->ftp.type;
        fileInfo->size            = storageDirectoryListHandle->ftp.size;
        fileInfo->timeLastAccess  = 0LL;
        fileInfo->timeModified    = storageDirectoryListHandle->ftp.timeModified;
        fileInfo->timeLastChanged = 0LL;
        fileInfo->userId          = storageDirectoryListHandle->ftp.userId;
        fileInfo->groupId         = storageDirectoryListHandle->ftp.groupId;
        fileInfo->permission      = storageDirectoryListHandle->ftp.permission;
        fileInfo->major           = 0;
        fileInfo->minor           = 0;
      }
      storageDirectoryListHandle->ftp.entryReadFlag = FALSE;

      error = ERROR_NONE;
    }
    else
    {
      error = ERROR_READ_DIRECTORY;
    }
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileInfo);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
  assert(error != ERROR_UNKNOWN);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
