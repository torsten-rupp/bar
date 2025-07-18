/***********************************************************************\
*
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
#ifdef HAVE_CURL
  #include <curl/curl.h>
#endif /* HAVE_CURL */
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

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
/* file data buffer size */
#define BUFFER_SIZE (64*1024)

// different timeouts [ms]
#define FTP_TIMEOUT (30*1000)


/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#ifdef HAVE_CURL
  LOCAL Password defaultFTPPassword;
#endif /* HAVE_CURL */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#ifdef HAVE_CURL
/***********************************************************************\
* Name   : initFTPLogin
* Purpose: init FTP login
* Input  : hostName                - host name
*          userName                - user name
*          password                - password
*          jobOptions              - job options
*          getNamePasswordFunction - get password call-back (can be
*                                    NULL)
*          getNamePasswordUserData - user data for get password call-back
* Output : -
* Return : TRUE if FTP login intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initFTPLogin(ConstString             hostName,
                        String                  userName,
                        Password                *password,
                        const JobOptions        *jobOptions,
                        GetNamePasswordFunction getNamePasswordFunction,
                        void                    *getNamePasswordUserData
                       )
{
  String s;
  bool   initFlag;

  assert(!String_isEmpty(hostName));
  assert(userName != NULL);
  assert(password != NULL);

  initFlag = FALSE;

  if (jobOptions != NULL)
  {
    SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      if (Password_isEmpty(&jobOptions->ftpServer.password))
      {
        switch (globalOptions.runMode)
        {
          case RUN_MODE_INTERACTIVE:
            if (Password_isEmpty(&defaultFTPPassword))
            {
              s = !String_isEmpty(userName)
                    ? String_format(String_new(),"FTP login password for %S@%S",userName,hostName)
                    : String_format(String_new(),"FTP login password for %S",hostName);
              if (Password_input(password,String_cString(s),PASSWORD_INPUT_MODE_ANY))
              {
                initFlag = TRUE;
              }
              String_delete(s);
            }
            else
            {
              Password_set(password,&defaultFTPPassword);
              initFlag = TRUE;
            }
            break;
          case RUN_MODE_BATCH:
          case RUN_MODE_SERVER:
            if (getNamePasswordFunction != NULL)
            {
              s = !String_isEmpty(userName)
                    ? String_format(String_new(),"%S@%S",userName,hostName)
                    : String_format(String_new(),"%S",hostName);
              if (getNamePasswordFunction(userName,
                                          password,
                                          PASSWORD_TYPE_FTP,
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

/***********************************************************************\
* Name   : setFTPLogin
* Purpose: set FTP login
* Input  : curlHandle - CURL handle
*          userName   - user name
*          password   - password
*          timeout    - timeout [ms]
* Output : -
* Return : CURLE_OK if no error, CURL error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL CURLcode setFTPLogin(CURL *curlHandle, ConstString userName, Password *password, long timeout)
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

  // set login
  if (curlCode == CURLE_OK)
  {
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_USERNAME,String_cString(userName));
  }
  if (curlCode == CURLE_OK)
  {
    PASSWORD_DEPLOY_DO(plainPassword,password)
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
#endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */

#ifdef HAVE_CURL
/***********************************************************************\
* Name   : checkFTPLogin
* Purpose: check if FTP login is possible
* Input  : hostName - host name
*          hostPort - host port or 0
*          userName - user name
*          password - password
* Output : -
* Return : ERROR_NONE if login is possible, error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors checkFTPLogin(ConstString hostName,
                           uint        hostPort,
                           ConstString userName,
                           Password    *password
                          )
{
  #if   defined(HAVE_CURL)
    CURL       *curlHandle;
    String     url;
    CURLcode   curlCode;
  #endif

  // check host name (Note: FTP library crash if host name is not valid!)
  if (!Network_hostExists(hostName))
  {
    return ERRORX_(HOST_NOT_FOUND,0,"%s",String_cString(hostName));
  }

  #if   defined(HAVE_CURL)
    // init handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      return ERROR_FTP_SESSION_FAIL;
    }

    // get URL
    url = String_format(String_new(),"ftp://%S",hostName);
    if (hostPort != 0) String_appendFormat(url,":%d",hostPort);

    // init FTP connect
    curlCode = setFTPLogin(curlHandle,userName,password,FTP_TIMEOUT);
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
    }
    if (curlCode != CURLE_OK)
    {
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // login
    curlCode = curl_easy_perform(curlHandle);
    if      (   (curlCode == CURLE_COULDNT_CONNECT)
             || (curlCode == CURLE_OPERATION_TIMEDOUT)
            )
    {
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      return ERRORX_(CONNECT_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }
    else if (curlCode != CURLE_OK)
    {
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      return ERRORX_(FTP_AUTHENTICATION,0,"%s",curl_easy_strerror(curlCode));
    }

    // free resources
    String_delete(url);
    (void)curl_easy_cleanup(curlHandle);
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
  assert(storageHandle->ftp.buffer != NULL);

  if (storageHandle->ftp.transferedBytes < storageHandle->ftp.length)
  {
    bytesSent = MIN(n,(size_t)(storageHandle->ftp.length-storageHandle->ftp.transferedBytes)/size)*size;

    memCopyFast(buffer,bytesSent,storageHandle->ftp.buffer,bytesSent);

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
  assert(storageHandle->ftp.buffer != NULL);

  if ((n*size) <= (storageHandle->ftp.length-storageHandle->ftp.transferedBytes))
  {
    bytesReceived = n*size;

    memCopyFast(storageHandle->ftp.buffer,bytesReceived,buffer,bytesReceived);
//fprintf(stderr,"%s, %d: curlFTPWriteDataCallback size=%d n=%d bytesReceived=%d %x\n",__FILE__,__LINE__,size,n,bytesReceived,bytesReceived);
//debugDumpMemory(storageHandle->ftp.buffer,128,0);
    storageHandle->ftp.buffer          = (byte*)storageHandle->ftp.buffer+bytesReceived;
    storageHandle->ftp.transferedBytes += (ulong)bytesReceived;
  }
  else
  {
    bytesReceived = CURL_WRITEFUNC_PAUSE;
  }

  return bytesReceived;
}

/***********************************************************************\
* Name   : curlFTPLineListCallback
* Purpose: curl FTP list callback: store lines into string list
* Input  : buffer   - buffer with data: receive data from remote
*          size     - size of an element
*          n        - number of elements
*          userData - string list
* Output : -
* Return : number of processed bytes or 0
* Notes  : -
\***********************************************************************/

LOCAL size_t curlFTPLineListCallback(const void *buffer,
                                     size_t     size,
                                     size_t     n,
                                     void       *userData
                                    )
{
  StringList *stringList = (StringList*)userData;

  assert(buffer != NULL);
  assert(size > 0);
  assert(stringList != NULL);

  String     line = String_new();
  const char *s   = (const char*)buffer;
  for (size_t i = 0; i < n; i++)
  {
    switch (*s)
    {
      case '\n':
        StringList_append(stringList,line);
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

  return size * n;
}
#endif /* HAVE_CURL */

#ifdef HAVE_CURL
/***********************************************************************\
* Name   : parseFTPDirectoryLine
* Purpose: parse FTP directory entry line
* Input  : line - line
* Output : fileInfo - filled file info
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

LOCAL bool parseFTPDirectoryLine(String          line,
                                 String          fileName,
                                 FileTypes       *type,
                                 uint64          *size,
                                 uint64          *timeModified,
                                 uint32          *userId,
                                 uint32          *groupId,
                                 FilePermissions *permission
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
  uint       i;

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
                        "%32s %* %* %* %"PRIu64" %u-%u-%u %u:%u % S",
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

    permissionStringLength = stringLength(permissionString);

    switch (permissionString[0])
    {
      case 'd': (*type) = FILE_TYPE_DIRECTORY; break;
      default:  (*type) = FILE_TYPE_FILE;      break;
    }
    (*timeModified) = Misc_makeDateTime(TIME_TYPE_LOCAL,
                                        year,month,day,
                                        hour,minute,0,
                                        DAY_LIGHT_SAVING_MODE_AUTO
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
                        "%32s %* %* %* %"PRIu64" %32s %u %u:%u % S",
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

    permissionStringLength = stringLength(permissionString);

    // get year, month
    Misc_splitDateTime(Misc_getCurrentDateTime(),
                       TIME_TYPE_LOCAL,
                       &year,
                       &month,
                       NULL,  // day,
                       NULL,  // hour,
                       NULL,  // minute,
                       NULL,  // second
                       NULL,  // weekDay
                       NULL  // isDayLightSaving
                      );
    s = monthName;
    while (((*s) == '0'))
    {
      s++;
    }
    for (i = 0; i < SIZE_OF_ARRAY(MONTH_DEFINITIONS); i++)
    {
      if (stringEqualsIgnoreCase(MONTH_DEFINITIONS[i].name,s))
      {
        month = MONTH_DEFINITIONS[i].month;
        break;
      }
    }

    // fill file info
    switch (permissionString[0])
    {
      case 'd': (*type) = FILE_TYPE_DIRECTORY; break;
      default:  (*type) = FILE_TYPE_FILE; break;
    }
    (*timeModified) = Misc_makeDateTime(TIME_TYPE_LOCAL,
                                        year,month,day,
                                        hour,minute,0,
                                        DAY_LIGHT_SAVING_MODE_AUTO
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
                        "%32s %* %* %* %"PRIu64" %32s %u %u % S",
                        NULL,
                        permissionString,
                        size,
                        monthName,&day,&year,
                        fileName
                       )
          )
  {
    // format:  <permission flags> * * * <size> <month> <day> <year> <file name>

    permissionStringLength = stringLength(permissionString);

    // get month
    Misc_splitDateTime(Misc_getCurrentDateTime(),
                       TIME_TYPE_LOCAL,
                       NULL,  // year
                       &month,
                       NULL,  // day,
                       NULL,  // hour,
                       NULL,  // minute,
                       NULL,  // second
                       NULL,  // weekDay
                       NULL  // isDayLightSaving
                      );
    s = monthName;
    while (((*s) == '0'))
    {
      s++;
    }
    for (i = 0; i < SIZE_OF_ARRAY(MONTH_DEFINITIONS); i++)
    {
      if (stringEqualsIgnoreCase(MONTH_DEFINITIONS[i].name,s))
      {
        month = MONTH_DEFINITIONS[i].month;
        break;
      }
    }

    switch (permissionString[0])
    {
      case 'd': (*type) = FILE_TYPE_DIRECTORY; break;
      default:  (*type) = FILE_TYPE_FILE; break;
    }
    (*timeModified) = Misc_makeDateTime(TIME_TYPE_LOCAL,
                                        year,month,day,
                                        0,0,0,
                                        DAY_LIGHT_SAVING_MODE_AUTO
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
                        "%32s %* %* %* %"PRIu64" %* %* %*:%* % S",
                        NULL,
                        permissionString,
                        size,
                        fileName
                       )
          )
  {
    // format:  <permission flags> * * * <size> * * *:* <file name>

    permissionStringLength = stringLength(permissionString);

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
                        "%32s %* %* %* %"PRIu64" %* %* %* % S",
                        NULL,
                        permissionString,
                        size,
                        fileName
                       )
          )
  {
    // format:  <permission flags> * * * <size> * * * <file name>

    permissionStringLength = stringLength(permissionString);

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
  #ifdef HAVE_CURL
    Password_init(&defaultFTPPassword);
  #endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */

  return ERROR_NONE;
}

LOCAL void StorageFTP_doneAll(void)
{
  #ifdef HAVE_CURL
    Password_done(&defaultFTPPassword);
    #if   defined(HAVE_CURL)
    #endif /* HAVE_CURL || HAVE_FTP */
  #endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */
}

LOCAL bool StorageFTP_parseSpecifier(ConstString ftpSpecifier,
                                     String      hostName,
                                     uint        *hostPort,
                                     String      userName,
                                     Password    *password
                                    )
{
  const char* LOGINNAME_MAP_FROM[] = {"\\@"};
  const char* LOGINNAME_MAP_TO[]   = {"@"};

  bool   result;
  String s,t;

  assert(ftpSpecifier != NULL);
  assert(hostName != NULL);
  assert(userName != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(userName);
  if (password != NULL) Password_clear(password);

  s = String_new();
  t = String_new();
  if      (String_matchCString(ftpSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,STRING_NO_ASSIGN,userName,s,STRING_NO_ASSIGN,hostName,t,NULL))
  {
    // <login name>:<login password>@<host name>:<host port>
    String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (password != NULL) Password_setString(password,s);
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(t,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,userName,s,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>:<login password>@<host name>
    String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (password != NULL) Password_setString(password,s);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,STRING_NO_ASSIGN,userName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,userName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);

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
         && String_equals(archiveName1,archiveName2);
}

LOCAL void StorageFTP_getName(String                 string,
                              const StorageSpecifier *storageSpecifier,
                              ConstString            archiveName
                             )
{
  ConstString storageFileName;

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
  if (!String_isEmpty(storageSpecifier->userName))
  {
    String_append(string,storageSpecifier->userName);
    if (!Password_isEmpty(&storageSpecifier->password))
    {
      String_appendChar(string,':');
      PASSWORD_DEPLOY_DO(plainPassword,&storageSpecifier->password)
      {
        String_appendCString(string,plainPassword);
      }
    }
    String_appendChar(string,'@');
  }
  String_append(string,storageSpecifier->hostName);
  if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 21))
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
* Name   : StorageFTP_getPrintableName
* Purpose: get printable storage name (without password)
* Input  : string           - name variable (can be NULL)
*          storageSpecifier - storage specifier string
*          archiveName      - archive name (can be NULL)
* Output : -
* Return : printable storage name
* Notes  : if archiveName is NULL file name from storageSpecifier is used
\***********************************************************************/

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
  if (!String_isEmpty(storageSpecifier->userName))
  {
    String_append(string,storageSpecifier->userName);
    String_appendChar(string,'@');
  }
  String_append(string,storageSpecifier->hostName);
  if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 21))
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
* Name   : StorageFTP_init
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

LOCAL Errors StorageFTP_init(StorageInfo                *storageInfo,
                             const JobOptions           *jobOptions,
                             BandWidthList              *maxBandWidthList,
                             ServerConnectionPriorities serverConnectionPriority
                            )
{
  #ifdef HAVE_CURL
    Errors   error;
    uint     retries;
  #endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);

  #if !defined(HAVE_CURL) && !defined(HAVE_FTP)
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif /* !defined(HAVE_CURL) && !defined(HAVE_FTP) */

  #if   defined(HAVE_CURL)
    {
      // init variables
      initBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter,maxBandWidthList);

      // get FTP server settings
      FTPServer ftpServer;
      storageInfo->ftp.serverId = Configuration_initFTPServerSettings(&ftpServer,storageInfo->storageSpecifier.hostName,jobOptions);
      if (String_isEmpty(storageInfo->storageSpecifier.userName)) String_set(storageInfo->storageSpecifier.userName,ftpServer.userName);
      if (String_isEmpty(storageInfo->storageSpecifier.userName)) String_setCString(storageInfo->storageSpecifier.userName,getenv("LOGNAME"));
      if (String_isEmpty(storageInfo->storageSpecifier.userName)) String_setCString(storageInfo->storageSpecifier.userName,getenv("USER"));
      if (Password_isEmpty(&storageInfo->storageSpecifier.password)) Password_set(&storageInfo->storageSpecifier.password,&ftpServer.password);
      if (String_isEmpty(storageInfo->storageSpecifier.hostName))
      {
        Configuration_doneFTPServerSettings(&ftpServer);
        doneBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter);
        return ERROR_NO_HOST_NAME;
      }

      // allocate FTP server
      if (!allocateServer(storageInfo->ftp.serverId,serverConnectionPriority,ALLOCATE_SERVER_TIMEOUT))
      {
        Configuration_doneFTPServerSettings(&ftpServer);
        doneBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter);
        return ERROR_TOO_MANY_CONNECTIONS;
      }

      // check FTP login, get correct password
      error = ERROR_FTP_AUTHENTICATION;
      if ((Error_getCode(error) == ERROR_CODE_FTP_AUTHENTICATION) && !Password_isEmpty(&storageInfo->storageSpecifier.password))
      {
        error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.userName,
                              &storageInfo->storageSpecifier.password
                             );
      }
      if ((Error_getCode(error) == ERROR_CODE_FTP_AUTHENTICATION) && !Password_isEmpty(&ftpServer.password))
      {
        error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.userName,
                              &ftpServer.password
                             );
        if (error == ERROR_NONE)
        {
          Password_set(&storageInfo->storageSpecifier.password,&ftpServer.password);
        }
      }
      if ((Error_getCode(error) == ERROR_CODE_FTP_AUTHENTICATION) && !Password_isEmpty(&ftpServer.password))
      {
        error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.userName,
                              &defaultFTPPassword
                             );
        if (error == ERROR_NONE)
        {
          Password_set(&storageInfo->storageSpecifier.password,&defaultFTPPassword);
        }
      }
      if (Error_getCode(error) == ERROR_CODE_FTP_AUTHENTICATION)
      {
        // initialize interactive/default password
        retries = 0;
        while ((Error_getCode(error) == ERROR_CODE_FTP_AUTHENTICATION) && (retries < MAX_PASSWORD_REQUESTS))
        {
          if (initFTPLogin(storageInfo->storageSpecifier.hostName,
                           storageInfo->storageSpecifier.userName,
                           &storageInfo->storageSpecifier.password,
                           jobOptions,
                           CALLBACK_(storageInfo->getNamePasswordFunction,storageInfo->getNamePasswordUserData)
                          )
             )
          {
            error = checkFTPLogin(storageInfo->storageSpecifier.hostName,
                                  storageInfo->storageSpecifier.hostPort,
                                  storageInfo->storageSpecifier.userName,
                                  &storageInfo->storageSpecifier.password
                                 );
          }
          retries++;
        }
      }
      if (Error_getCode(error) == ERROR_CODE_FTP_AUTHENTICATION)
      {
        error = (   !Password_isEmpty(&storageInfo->storageSpecifier.password)
                 || !Password_isEmpty(&ftpServer.password)
                 || !Password_isEmpty(&defaultFTPPassword)
                )
                  ? ERRORX_(INVALID_FTP_PASSWORD,0,"a %s",String_cString(storageInfo->storageSpecifier.hostName))
                  : ERRORX_(NO_FTP_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
      }

      // store password as default password
      if (error == ERROR_NONE)
      {
        Password_set(&defaultFTPPassword,&storageInfo->storageSpecifier.password);
      }

      if (error != ERROR_NONE)
      {
        freeServer(storageInfo->ftp.serverId);
        Configuration_doneFTPServerSettings(&ftpServer);
        doneBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter);
        return error;
      }

      // free resources
      Configuration_doneFTPServerSettings(&ftpServer);
    }
    return ERROR_NONE;
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(maxBandWidthList);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL Errors StorageFTP_done(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);

  #if   defined(HAVE_CURL)
    freeServer(storageInfo->ftp.serverId);
    doneBandWidthLimiter(&storageInfo->ftp.bandWidthLimiter);
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);
  #endif /* HAVE_CURL || HAVE_FTP */

  return ERROR_NONE;
}

LOCAL bool StorageFTP_isServerAllocationPending(const StorageInfo *storageInfo)
{
  bool serverAllocationPending;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);

  #ifdef HAVE_CURL
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
  #ifdef HAVE_CURL
    TextMacros (textMacros,3);
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);

  error = ERROR_NONE;

  #if   defined(HAVE_CURL) || defined(HAVE_FTP)
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
      if (!String_isEmpty(globalOptions.ftp.writePreProcessCommand))
      {
        printInfo(1,"Write pre-processing...");
        error = executeTemplate(String_cString(globalOptions.ftp.writePreProcessCommand),
                                time,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL),
                                globalOptions.commandTimeout
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }

      // free resources
      String_delete(directory);
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
  #ifdef HAVE_CURL
    TextMacros (textMacros,3);
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);

  error = ERROR_NONE;

  #if   defined(HAVE_CURL) || defined(HAVE_FTP)
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
      if (!String_isEmpty(globalOptions.ftp.writePostProcessCommand))
      {
        printInfo(1,"Write post-processing...");
        error = executeTemplate(String_cString(globalOptions.ftp.writePostProcessCommand),
                                time,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL),
                                globalOptions.commandTimeout
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }

      // free resources
      String_delete(directory);
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

LOCAL bool StorageFTP_exists(StorageInfo *storageInfo,
                             ConstString archiveName
                            )
{
  bool existsFlag;
  #if   defined(HAVE_CURL)
    CURL            *curlHandle;
    String          directoryName,baseName;
    String          url;
    CURLcode        curlCode;
    StringTokenizer nameTokenizer;
    ConstString     token;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);
  assert(!String_isEmpty(archiveName));

  existsFlag = FALSE;

  #if   defined(HAVE_CURL)
    // open curl handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      return ERROR_FTP_SESSION_FAIL;
    }

    // get directory name, base name
    directoryName = File_getDirectoryName(String_new(),archiveName);
    baseName      = File_getBaseName(String_new(),archiveName,TRUE);

    // get URL
    url = String_format(String_new(),"ftp://%S",storageInfo->storageSpecifier.hostName);
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

    // set FTP connect
    curlCode = setFTPLogin(curlHandle,
                           storageInfo->storageSpecifier.userName,
                           &storageInfo->storageSpecifier.password,
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
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
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
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
  #endif /* HAVE_CURL || HAVE_FTP */

  return existsFlag;
}

LOCAL bool StorageFTP_isFile(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL bool StorageFTP_isDirectory(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL bool StorageFTP_isReadable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL bool StorageFTP_isWritable(const StorageInfo *storageInfo, ConstString archiveName)
{
  assert(storageInfo != NULL);
  assert(!String_isEmpty(archiveName));

//TODO: still not implemented
  UNUSED_VARIABLE(storageInfo);
  UNUSED_VARIABLE(archiveName);

  return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL Errors StorageFTP_getTmpName(String archiveName, const StorageInfo *storageInfo)
{
  assert(archiveName != NULL);
  assert(!String_isEmpty(archiveName));
  assert(storageInfo != NULL);

  UNUSED_VARIABLE(archiveName);
  UNUSED_VARIABLE(storageInfo);

//TODO: still not implemented
  return ERROR_STILL_NOT_IMPLEMENTED;
}

LOCAL Errors StorageFTP_create(StorageHandle *storageHandle,
                               ConstString   fileName,
                               uint64        fileSize,
                               bool          forceFlag
                              )
{
  #if   defined(HAVE_CURL)
    String            directoryName,baseName;
    String            url;
    CURLcode          curlCode;
    String            ftpCommand;
    struct curl_slist *curlSList;
    CURLMcode         curlMCode;
    StringTokenizer   nameTokenizer;
    ConstString       token;
    int               runningHandles;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);
  assert(!String_isEmpty(fileName));

  // check if file exists
  if (   !forceFlag
      && (storageHandle->storageInfo->jobOptions != NULL)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_APPEND)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_OVERWRITE)
      && StorageFTP_exists(storageHandle->storageInfo,fileName)
     )
  {
    return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(fileName));
  }

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

    // open curl handles
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

    // set FTP connect
    curlCode = setFTPLogin(storageHandle->ftp.curlHandle,
                           storageHandle->storageInfo->storageSpecifier.userName,
                           &storageHandle->storageInfo->storageSpecifier.password,
                           FTP_TIMEOUT
                          );
    if (curlCode != CURLE_OK)
    {
      (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
      (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // get directory name, base name
    directoryName = File_getDirectoryName(String_new(),fileName);
    baseName      = File_getBaseName(String_new(),fileName,TRUE);

    // get URL
    url = String_format(String_new(),"ftp://%S",storageHandle->storageInfo->storageSpecifier.hostName);
    if (storageHandle->storageInfo->storageSpecifier.hostPort != 0) String_appendFormat(url,":%d",storageHandle->storageInfo->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,directoryName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    File_doneSplitFileName(&nameTokenizer);
    String_appendChar(url,'/');
    String_append(url,baseName);

    // check to stop if exists/append/overwrite
    switch (storageHandle->storageInfo->jobOptions->archiveFileMode)
    {
      case ARCHIVE_FILE_MODE_STOP:
        // check if file exists
        curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_URL,String_cString(url));
        if (curlCode == CURLE_OK)
        {
          curlCode = curl_easy_perform(storageHandle->ftp.curlHandle);
        }
        if (curlCode == CURLE_OK)
        {
          String_delete(url);
          String_delete(baseName);
          String_delete(directoryName);
          (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
          (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
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
        // delete existing file (ignore error)
        ftpCommand = String_format(String_new(),"*DELE %S",fileName);
        curlSList = curl_slist_append(NULL,String_cString(ftpCommand));
        curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_URL,String_cString(url));
        if (curlCode == CURLE_OK)
        {
          curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_NOBODY,1L);
        }
        if (curlCode == CURLE_OK)
        {
          curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_QUOTE,curlSList);
        }
        if (curlCode == CURLE_OK)
        {
          (void)curl_easy_perform(storageHandle->ftp.curlHandle);
        }
        (void)curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_QUOTE,NULL);
        curl_slist_free_all(curlSList);
        String_delete(ftpCommand);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }

    // create directories if necessary
    curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_URL,String_cString(url));
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_FTP_CREATE_MISSING_DIRS,1L);
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
    if (curlCode != CURLE_OK)
    {
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
      (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      return ERRORX_(CREATE_DIRECTORY,0,"%s",curl_easy_strerror(curlCode));
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
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);
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

    // open curl handles
    storageHandle->ftp.curlMultiHandle = curl_multi_init();
    if (storageHandle->ftp.curlMultiHandle == NULL)
    {
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERROR_FTP_SESSION_FAIL;
    }
    storageHandle->ftp.curlHandle = curl_easy_init();
    if (storageHandle->ftp.curlHandle == NULL)
    {
      (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERROR_FTP_SESSION_FAIL;
    }

    // set FTP login
    curlCode = setFTPLogin(storageHandle->ftp.curlHandle,
                           storageHandle->storageInfo->storageSpecifier.userName,
                           &storageHandle->storageInfo->storageSpecifier.password,
                           FTP_TIMEOUT
                          );
    if (curlCode != CURLE_OK)
    {
      (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
      (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
      free(storageHandle->ftp.readAheadBuffer.data);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // get pathname, basename
    directoryName = File_getDirectoryName(String_new(),archiveName);
    baseName      = File_getBaseName(String_new(),archiveName,TRUE);

    // get URL
    url = String_format(String_new(),"ftp://%S",storageHandle->storageInfo->storageSpecifier.hostName);
    if (storageHandle->storageInfo->storageSpecifier.hostPort != 0) String_appendFormat(url,":%d",storageHandle->storageInfo->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,directoryName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    File_doneSplitFileName(&nameTokenizer);
    String_appendChar(url,'/');
    String_append(url,baseName);

    // check if file exists (Note: by default curl use passive FTP)
    curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_URL,String_cString(url));
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
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);

  #if   defined(HAVE_CURL)
    assert(storageHandle->ftp.curlHandle != NULL);
    assert(storageHandle->ftp.curlMultiHandle != NULL);
  #endif /* HAVE_CURL || HAVE_FTP */

  switch (storageHandle->mode)
  {
    case STORAGE_MODE_WRITE:
      #if   defined(HAVE_CURL)
        free(storageHandle->ftp.readAheadBuffer.data);
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_MODE_READ:
      #if   defined(HAVE_CURL)
        free(storageHandle->ftp.readAheadBuffer.data);
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  #if   defined(HAVE_CURL)
    (void)curl_multi_remove_handle(storageHandle->ftp.curlMultiHandle,storageHandle->ftp.curlHandle);
    (void)curl_easy_cleanup(storageHandle->ftp.curlHandle);
    (void)curl_multi_cleanup(storageHandle->ftp.curlMultiHandle);
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL bool StorageFTP_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);

  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    return storageHandle->ftp.index >= storageHandle->ftp.size;
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
    uint64    startTotalReceivedBytes,endTotalReceivedBytes;
    CURLMcode curlmCode;
    int       runningHandles;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);
  assert(buffer != NULL);

  error = ERROR_UNKNOWN;
  #if   defined(HAVE_CURL)
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
      memCopyFast(buffer,bytesAvail,storageHandle->ftp.readAheadBuffer.data+index,bytesAvail);

      // adjust buffer, bufferSize, bytes read, index
      buffer = (byte*)buffer+bytesAvail;
      bufferSize -= bytesAvail;
      if (bytesRead != NULL) (*bytesRead) += bytesAvail;
      storageHandle->ftp.index += (uint64)bytesAvail;
    }

    // read rest of data
    error = ERROR_NONE;
    while (   (bufferSize > 0L)
           && (storageHandle->ftp.index < storageHandle->ftp.size)
          )
    {
      assert(storageHandle->ftp.index >= (storageHandle->ftp.readAheadBuffer.offset+storageHandle->ftp.readAheadBuffer.length));

      // get max. number of bytes to receive in one step
      if (storageHandle->storageInfo->ftp.bandWidthLimiter.maxBandWidthList != NULL)
      {
        length = MIN(storageHandle->storageInfo->ftp.bandWidthLimiter.blockSize,bufferSize);
      }
      else
      {
        length = bufferSize;
      }
      assert(length > 0L);

      // get start time, start received bytes
      startTimestamp          = Misc_getTimestamp();
      startTotalReceivedBytes = 0;

      if (length < BUFFER_SIZE)
      {
        // read into read-ahead buffer
        storageHandle->ftp.buffer          = storageHandle->ftp.readAheadBuffer.data;
        storageHandle->ftp.length          = MIN((size_t)(storageHandle->ftp.size-storageHandle->ftp.index),BUFFER_SIZE);
        storageHandle->ftp.transferedBytes = 0L;
        runningHandles = 1;
        while (   (storageHandle->ftp.transferedBytes == 0L)
               && (runningHandles > 0)
              )
        {
          // wait for socket
          error = waitCurlSocketRead(storageHandle->ftp.curlMultiHandle);
          if (error != ERROR_NONE)
          {
            break;
          }

          // perform curl action
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
          if (error != ERROR_NONE)
          {
            break;
          }
        }
        if      (error != ERROR_NONE)
        {
          break;
        }
        else if (storageHandle->ftp.transferedBytes <= 0L)
        {
          const CURLMsg *curlMsg;
          int           n,i;

          curlMsg = curl_multi_info_read(storageHandle->ftp.curlMultiHandle,&n);
          for (i = 0; i < n; i++)
          {
            if ((curlMsg[i].easy_handle == storageHandle->ftp.curlHandle) && (curlMsg[i].msg == CURLMSG_DONE))
            {
              error = ERRORX_(NETWORK_RECEIVE,0,"%s",curl_easy_strerror(curlMsg[i].data.result));
              break;
            }
            curlMsg++;
          }
          break;
        }
        storageHandle->ftp.readAheadBuffer.offset = storageHandle->ftp.index;
        storageHandle->ftp.readAheadBuffer.length = storageHandle->ftp.transferedBytes;

        // copy data from read-ahead buffer
        bytesAvail = MIN(length,storageHandle->ftp.readAheadBuffer.length);
        memCopyFast(buffer,bytesAvail,storageHandle->ftp.readAheadBuffer.data,bytesAvail);

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
        while (   (storageHandle->ftp.transferedBytes == 0L)
               && (runningHandles > 0)
              )
        {
          // wait for socket
          error = waitCurlSocketRead(storageHandle->ftp.curlMultiHandle);
          if (error != ERROR_NONE)
          {
            break;
          }

          // perform curl action
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
          if (error != ERROR_NONE)
          {
            break;
          }
        }
        if      (error != ERROR_NONE)
        {
          break;
        }
        else if (storageHandle->ftp.transferedBytes <= 0L)
        {
          const CURLMsg *curlMsg;
          int           n,i;

          curlMsg = curl_multi_info_read(storageHandle->ftp.curlMultiHandle,&n);
          for (i = 0; i < n; i++)
          {
            if ((curlMsg[i].easy_handle == storageHandle->ftp.curlHandle) && (curlMsg[i].msg == CURLMSG_DONE))
            {
              error = ERRORX_(NETWORK_RECEIVE,0,"%s",curl_easy_strerror(curlMsg[i].data.result));
              break;
            }
            curlMsg++;
          }
          break;
        }
        bytesAvail = storageHandle->ftp.transferedBytes;

        // adjust buffer, bufferSize, bytes read, index
        buffer = (byte*)buffer+bytesAvail;
        bufferSize -= bytesAvail;
        if (bytesRead != NULL) (*bytesRead) += bytesAvail;
        storageHandle->ftp.index += (uint64)bytesAvail;
      }

      // get end time, end received bytes
      endTimestamp          = Misc_getTimestamp();
      endTotalReceivedBytes = bytesAvail;
      assert(endTotalReceivedBytes >= startTotalReceivedBytes);

      /* limit used band width if requested (note: when the system time is
         changing endTimestamp may become smaller than startTimestamp;
         thus do not check this with an assert())
      */
      if (endTimestamp >= startTimestamp)
      {
        SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          limitBandWidth(&storageHandle->storageInfo->ftp.bandWidthLimiter,
                         endTotalReceivedBytes-startTotalReceivedBytes,
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
    char      errorBuffer[CURL_ERROR_SIZE];
    ulong     writtenBytes;
    ulong     length;
    uint64    startTimestamp,endTimestamp;
    uint64    startTotalSentBytes,endTotalSentBytes;
    CURLMcode curlmCode;
    int       runningHandles;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);
  assert(buffer != NULL);

  error = ERROR_UNKNOWN;
  #if   defined(HAVE_CURL)
    assert(storageHandle->ftp.curlMultiHandle != NULL);

    // try to get error message
    stringClear(errorBuffer);
    (void)curl_easy_setopt(storageHandle->ftp.curlHandle, CURLOPT_ERRORBUFFER, errorBuffer);

    error        = ERROR_NONE;
    writtenBytes = 0L;
    while (writtenBytes < bufferLength)
    {
      // get max. number of bytes to send in one step
      if (storageHandle->storageInfo->ftp.bandWidthLimiter.maxBandWidthList != NULL)
      {
        length = MIN(storageHandle->storageInfo->ftp.bandWidthLimiter.blockSize,bufferLength-writtenBytes);
      }
      else
      {
        length = bufferLength-writtenBytes;
      }
      assert(length > 0L);

      // get start time, start received bytes
      startTimestamp      = Misc_getTimestamp();
      startTotalSentBytes = 0;

      // send data
      storageHandle->ftp.buffer          = (void*)buffer;
      storageHandle->ftp.length          = length;
      storageHandle->ftp.transferedBytes = 0L;
      runningHandles = 1;
      while (   (storageHandle->ftp.transferedBytes == 0L)
             && (runningHandles > 0)
            )
      {
        // wait for socket
        error = waitCurlSocketWrite(storageHandle->ftp.curlMultiHandle);
        if (error != ERROR_NONE)
        {
          break;
        }

        // perform curl action
        do
        {
          curlmCode = curl_multi_perform(storageHandle->ftp.curlMultiHandle,&runningHandles);
        }
        while (   (curlmCode == CURLM_CALL_MULTI_PERFORM)
               && (runningHandles > 0)
              );
        if (curlmCode != CURLM_OK)
        {
          error = ERRORX_(NETWORK_SEND,0,"%s",curl_multi_strerror(curlmCode));
        }
        if (error != ERROR_NONE)
        {
          break;
        }
      }
      if (error != ERROR_NONE)
      {
        break;
      }

      // check transmission error
      if (storageHandle->ftp.transferedBytes <= 0L)
      {
        error = ERRORX_(NETWORK_SEND,0,"%s",errorBuffer);
        break;
      }

      buffer = (byte*)buffer+storageHandle->ftp.transferedBytes;
      writtenBytes += storageHandle->ftp.transferedBytes;

      // get end time, end received bytes
      endTimestamp      = Misc_getTimestamp();
      endTotalSentBytes = storageHandle->ftp.transferedBytes;
      assert(endTotalSentBytes >= startTotalSentBytes);

      /* limit used band width if requested (note: when the system time is
         changing endTimestamp may become smaller than startTimestamp;
         thus do not check this with an assert())
      */
      if (endTimestamp >= startTimestamp)
      {
        SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          limitBandWidth(&storageHandle->storageInfo->ftp.bandWidthLimiter,
                         endTotalSentBytes-startTotalSentBytes,
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
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_UNKNOWN;
  #ifdef HAVE_CURL
    (*offset) = storageHandle->ftp.index;
    error     = ERROR_NONE;
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
     CURLcode  curlCode;
     CURLMcode curlMCode;
     int       runningHandles;
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);

  error = ERROR_UNKNOWN;
  #if   defined(HAVE_CURL)
    assert(storageHandle->ftp.readAheadBuffer.data != NULL);

    // restart download
    (void)curl_multi_remove_handle(storageHandle->ftp.curlMultiHandle,storageHandle->ftp.curlHandle);
    curlCode = curl_easy_setopt(storageHandle->ftp.curlHandle,CURLOPT_RESUME_FROM_LARGE,(curl_off_t)offset);
    if (curlCode != CURLE_OK)
    {
      return ERRORX_(IO,0,"%s",curl_easy_strerror(curlCode));
    }
    curlMCode = curl_multi_add_handle(storageHandle->ftp.curlMultiHandle,storageHandle->ftp.curlHandle);
    if (curlMCode != CURLM_OK)
    {
      return ERRORX_(IO,0,"%s",curl_easy_strerror(curlCode));
    }
    do
    {
      curlMCode = curl_multi_perform(storageHandle->ftp.curlMultiHandle,&runningHandles);
    }
    while (   (curlMCode == CURLM_CALL_MULTI_PERFORM)
           && (runningHandles > 0)
          );
    if (curlMCode != CURLM_OK)
    {
      error = ERRORX_(IO,0,"%s",curl_easy_strerror(curlCode));
    }
    else
    {
      error = getCurlHTTPResponseError(storageHandle->ftp.curlHandle,storageHandle->storageInfo->storageSpecifier.archiveName);
    }

    storageHandle->ftp.index = offset;
    storageHandle->ftp.readAheadBuffer.offset = offset;
    storageHandle->ftp.readAheadBuffer.length = 0;
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
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);

  size = 0LL;
  #ifdef HAVE_CURL
    size = storageHandle->ftp.size;
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_CURL || HAVE_FTP */

  return size;
}

LOCAL Errors StorageFTP_rename(StorageInfo *storageInfo,
                               ConstString fromArchiveName,
                               ConstString toArchiveName
                              )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);

UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(fromArchiveName);
UNUSED_VARIABLE(toArchiveName);
error = ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

LOCAL Errors StorageFTP_makeDirectory(StorageInfo *storageInfo,
                                      ConstString directoryName
                                     )
{
  #if   defined(HAVE_CURL)
    CURLM             *curlMultiHandle;
    CURL              *curlHandle;
    String            url;
    CURLcode          curlCode;
    CURLMcode         curlMCode;
    StringTokenizer   nameTokenizer;
    ConstString       token;
    int               runningHandles;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FILESYSTEM);
  assert(!String_isEmpty(directoryName));

  #if   defined(HAVE_CURL)
    // open curl handles
    curlMultiHandle = curl_multi_init();
    if (curlMultiHandle == NULL)
    {
      return ERROR_FTP_SESSION_FAIL;
    }
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      curl_multi_cleanup(curlMultiHandle);
      return ERROR_FTP_SESSION_FAIL;
    }

    // get URL
    url = String_format(String_new(),"ftp://%S",storageInfo->storageSpecifier.hostName);
    if (storageInfo->storageSpecifier.hostPort != 0) String_appendFormat(url,":%d",storageInfo->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,directoryName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    File_doneSplitFileName(&nameTokenizer);
    String_appendChar(url,'/');
    String_append(url,directoryName);

    // set FTP connect
    curlCode = setFTPLogin(curlHandle,
                           storageInfo->storageSpecifier.userName,
                           &storageInfo->storageSpecifier.password,
                           FTP_TIMEOUT
                          );
    if (curlCode != CURLE_OK)
    {
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      (void)curl_multi_cleanup(curlMultiHandle);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
    }

    // create directories if necessary
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_FTP_CREATE_MISSING_DIRS,1L);
    }
    curlMCode = curl_multi_add_handle(curlMultiHandle,curlHandle);
    if (curlMCode != CURLM_OK)
    {
      String_delete(url);
      (void)curl_easy_cleanup(curlHandle);
      (void)curl_multi_cleanup(curlMultiHandle);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_multi_strerror(curlMCode));
    }

    // start create
    do
    {
      curlMCode = curl_multi_perform(curlMultiHandle,&runningHandles);
    }
    while (   (curlMCode == CURLM_CALL_MULTI_PERFORM)
           && (runningHandles > 0)
          );
//fprintf(stderr,"%s, %d: storageHandle->ftp.runningHandles=%d\n",__FILE__,__LINE__,storageHandle->ftp.runningHandles);
    if (curlMCode != CURLM_OK)
    {
      String_delete(url);
      (void)curl_multi_remove_handle(curlMultiHandle,curlHandle);
      (void)curl_easy_cleanup(curlHandle);
      (void)curl_multi_cleanup(curlMultiHandle);
      return ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_multi_strerror(curlMCode));
    }

    // free resources
    String_delete(url);

    return ERROR_NONE;
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(directoryName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL Errors StorageFTP_delete(StorageInfo *storageInfo,
                               ConstString archiveName
                              )
{
  Errors            error;
  #if   defined(HAVE_CURL)
    CURL              *curlHandle;
    String            directoryName,baseName;
    String            url;
    CURLcode          curlCode;
    StringTokenizer   nameTokenizer;
    ConstString       token;
    String            ftpCommand;
    struct curl_slist *curlSList;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);
  assert(!String_isEmpty(archiveName));

  error = ERROR_UNKNOWN;
  #if   defined(HAVE_CURL)
    // open curl handle
    curlHandle = curl_easy_init();
    if (curlHandle != NULL)
    {
      // get directory name, base name
      directoryName = File_getDirectoryName(String_new(),archiveName);
      baseName      = File_getBaseName(String_new(),archiveName,TRUE);

      // get URL
      url = String_format(String_new(),"ftp://%S",storageInfo->storageSpecifier.hostName);
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

      // delete file
      curlCode = setFTPLogin(curlHandle,
                             storageInfo->storageSpecifier.userName,
                             &storageInfo->storageSpecifier.password,
                             FTP_TIMEOUT
                            );
      if (curlCode == CURLE_OK)
      {
        curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
      }
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
          error = ERRORX_(DELETE_FILE,0,"%s",curl_easy_strerror(curlCode));
        }
        curl_slist_free_all(curlSList);
        String_delete(ftpCommand);
      }

      // free resources
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryName);
      (void)curl_easy_cleanup(curlHandle);

      error = ERROR_NONE;
    }
    else
    {
      error = ERROR_FTP_SESSION_FAIL;
    }
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
  assert(error != ERROR_UNKNOWN);

  return error;
}

/***********************************************************************\
* Name   : StorageFTP_getFileInfo
* Purpose: get storage file info
* Input  : fileInfo    - file info variable
*          storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : fileInfo - file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageFTP_getFileInfo(FileInfo          *fileInfo,
                                    const StorageInfo *storageInfo,
                                    ConstString       archiveName
                                   )
{
  Errors error;

  assert(fileInfo != NULL);
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_FTP);
  assert(archiveName != NULL);

  if (String_isEmpty(storageInfo->storageSpecifier.hostName))
  {
    return ERROR_NO_HOST_NAME;
  }

  memClear(fileInfo,sizeof(fileInfo));

  error = ERROR_UNKNOWN;
  #if   defined(HAVE_CURL)
    // get FTP server settings
    FTPServer ftpServer;
    uint serverId = Configuration_initFTPServerSettings(&ftpServer,storageInfo->storageSpecifier.hostName,storageInfo->jobOptions);
    if (String_isEmpty(ftpServer.userName)) String_set(ftpServer.userName,ftpServer.userName);
    if (String_isEmpty(ftpServer.userName)) String_setCString(ftpServer.userName,getenv("LOGNAME"));
    if (String_isEmpty(ftpServer.userName)) String_setCString(ftpServer.userName,getenv("USER"));
    if (Password_isEmpty(&ftpServer.password)) Password_set(&ftpServer.password,&ftpServer.password);

    // allocate FTP server
    if (!allocateServer(serverId,SERVER_CONNECTION_PRIORITY_LOW,ALLOCATE_SERVER_TIMEOUT))
    {
      Configuration_doneFTPServerSettings(&ftpServer);
      return ERROR_TOO_MANY_CONNECTIONS;
    }

    // open curl handle
    CURL *curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      freeServer(serverId);
      Configuration_doneFTPServerSettings(&ftpServer);
      return ERROR_FTP_SESSION_FAIL;
    }

    // get directory, basename
    String directoryPath = File_getDirectoryName(String_new(),archiveName);
    String baseName      = File_getBaseName(String_new(),archiveName,TRUE);

    // get URL
    String  url = String_format(String_new(),"ftp://%S",storageInfo->storageSpecifier.hostName);
    if (storageInfo->storageSpecifier.hostPort != 0) String_appendFormat(url,":%d",storageInfo->storageSpecifier.hostPort);
    StringTokenizer nameTokenizer;
    File_initSplitFileName(&nameTokenizer,directoryPath);
    ConstString token;
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    String_appendChar(url,'/');
    File_doneSplitFileName(&nameTokenizer);

    // get file info
    StringList lineList;
    StringList_init(&lineList);
    CURLcode curlCode;
    curlCode = setFTPLogin(curlHandle,
                           ftpServer.userName,
                           &ftpServer.password,
                           FTP_TIMEOUT
                          );
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_WRITEFUNCTION,curlFTPLineListCallback);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_WRITEDATA,&lineList);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_perform(curlHandle);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_perform(curlHandle);
    }
    if (curlCode != CURLE_OK)
    {
      error = ERRORX_(FTP_SESSION_FAIL,0,"%s",curl_easy_strerror(curlCode));
      StringList_done(&lineList);
      String_delete(url);
      String_delete(baseName);
      String_delete(directoryPath);
      (void)curl_easy_cleanup(curlHandle);
      freeServer(serverId);
      Configuration_doneFTPServerSettings(&ftpServer);
      return error;
    }
    bool   done     = FALSE;
    String fileName = String_new();
    while (!StringList_isEmpty(&lineList) && !done)
    {
      String line = StringList_removeFirst(&lineList,NULL);
      done =    parseFTPDirectoryLine(line,
                                      fileName,
                                      &fileInfo->type,
                                      &fileInfo->size,
                                      &fileInfo->timeModified,
                                      &fileInfo->userId,
                                      &fileInfo->groupId,
                                      &fileInfo->permissions
                                     )
             && String_equals(fileName,baseName);
      String_delete(line);
    }
    String_delete(fileName);
    StringList_done(&lineList);

    // free resources
    String_delete(url);
    String_delete(baseName);
    String_delete(directoryPath);
    (void)curl_easy_cleanup(curlHandle);
    freeServer(serverId);
    Configuration_doneFTPServerSettings(&ftpServer);

    error = done ? ERROR_NONE : ERROR_FILE_NOT_FOUND_;
  #else /* not HAVE_CURL || HAVE_FTP */
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
  assert(error != ERROR_UNKNOWN);

  return error;
}

/*---------------------------------------------------------------------*/

LOCAL Errors StorageFTP_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                          const StorageSpecifier     *storageSpecifier,
                                          ConstString                pathName,
                                          const JobOptions           *jobOptions,
                                          ServerConnectionPriorities serverConnectionPriority
                                         )
{
  #if defined(HAVE_CURL)
    Errors          error;
    AutoFreeList    autoFreeList;
    FTPServer       ftpServer;
    Password        password;
    CURL            *curlHandle;
    String          url;
    CURLcode        curlCode;
    StringTokenizer nameTokenizer;
    ConstString     token;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_FTP);
  assert(pathName != NULL);
  assert(jobOptions != NULL);

  UNUSED_VARIABLE(storageSpecifier);
  #if !defined(HAVE_CURL) && !defined(HAVE_FTP) && (!defined(HAVE_CURL) || !defined(HAVE_MXML))
    UNUSED_VARIABLE(serverConnectionPriority);
  #endif

  // open directory listing
  #if   defined(HAVE_CURL)
    // init variables
    AutoFree_init(&autoFreeList);
    Password_init(&password);
    StringList_init(&storageDirectoryListHandle->ftp.lineList);
    storageDirectoryListHandle->ftp.fileName      = String_new();
    storageDirectoryListHandle->ftp.entryReadFlag = FALSE;
    AUTOFREE_ADD(&autoFreeList,&password,{ Password_done(&password); });
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->ftp.lineList,{ StringList_done(&storageDirectoryListHandle->ftp.lineList); });
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->ftp.fileName,{ String_delete(storageDirectoryListHandle->ftp.fileName); });

    // get FTP server settings
    storageDirectoryListHandle->ftp.serverId = Configuration_initFTPServerSettings(&ftpServer,storageDirectoryListHandle->storageSpecifier.hostName,jobOptions);
    AUTOFREE_ADD(&autoFreeList,&ftpServer,{ Configuration_doneFTPServerSettings(&ftpServer); });
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.userName)) String_set(storageDirectoryListHandle->storageSpecifier.userName,ftpServer.userName);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.userName)) String_setCString(storageDirectoryListHandle->storageSpecifier.userName,getenv("LOGNAME"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.userName)) String_setCString(storageDirectoryListHandle->storageSpecifier.userName,getenv("USER"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate FTP server
    if (!allocateServer(storageDirectoryListHandle->ftp.serverId,serverConnectionPriority,ALLOCATE_SERVER_TIMEOUT))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->ftp.serverId,{ freeServer(storageDirectoryListHandle->ftp.serverId); });

    // check FTP login, get correct password
    error = ERROR_FTP_SESSION_FAIL;
    if ((Error_getCode(error) == ERROR_CODE_FTP_SESSION_FAIL) && !Password_isEmpty(&storageDirectoryListHandle->storageSpecifier.password))
    {
      error = checkFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.userName,
                            &storageDirectoryListHandle->storageSpecifier.password
                           );
    }
    if ((Error_getCode(error) == ERROR_CODE_FTP_SESSION_FAIL) && !Password_isEmpty(&ftpServer.password))
    {
      error = checkFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.userName,
                            &ftpServer.password
                           );
      if (error == ERROR_NONE)
      {
        Password_set(&storageDirectoryListHandle->storageSpecifier.password,&ftpServer.password);
      }
    }
    if (Error_getCode(error) == ERROR_CODE_FTP_SESSION_FAIL)
    {
      // initialize default password
      while (   (Error_getCode(error) == ERROR_CODE_FTP_SESSION_FAIL)
             && initFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                             storageDirectoryListHandle->storageSpecifier.userName,
                             &storageDirectoryListHandle->storageSpecifier.password,
                             jobOptions,
//TODO
                             CALLBACK_(NULL,NULL) // CALLBACK_(storageInfo->getNamePasswordFunction,storageInfo->getNamePasswordUserData)
                            )
            )
      {
        error = checkFTPLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                              storageDirectoryListHandle->storageSpecifier.hostPort,
                              storageDirectoryListHandle->storageSpecifier.userName,
                              &storageDirectoryListHandle->storageSpecifier.password
                             );
      }
      if (Error_getCode(error) == ERROR_CODE_FTP_SESSION_FAIL)
      {
        error = (   !Password_isEmpty(&storageDirectoryListHandle->storageSpecifier.password)
                 || !Password_isEmpty(&ftpServer.password)
                 || !Password_isEmpty(&defaultFTPPassword)
                )
                  ? ERRORX_(INVALID_FTP_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName))
                  : ERRORX_(NO_FTP_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName));
      }

      // store password as default password
      if (error == ERROR_NONE)
      {
        Password_set(&defaultFTPPassword,&storageDirectoryListHandle->storageSpecifier.password);
      }
    }
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // init curl handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_FTP_SESSION_FAIL;
    }

    // get URL
    url = String_format(String_new(),"ftp://%S",storageDirectoryListHandle->storageSpecifier.hostName);
    if (storageDirectoryListHandle->storageSpecifier.hostPort != 0) String_appendFormat(url,":%d",storageDirectoryListHandle->storageSpecifier.hostPort);
    File_initSplitFileName(&nameTokenizer,pathName);
    while (File_getNextSplitFileName(&nameTokenizer,&token))
    {
      String_appendChar(url,'/');
      String_append(url,token);
    }
    String_appendChar(url,'/');
    File_doneSplitFileName(&nameTokenizer);

    // read directory
    curlCode = setFTPLogin(curlHandle,
                           storageDirectoryListHandle->storageSpecifier.userName,
                           &storageDirectoryListHandle->storageSpecifier.password,
                           FTP_TIMEOUT
                          );
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_WRITEFUNCTION,curlFTPLineListCallback);
    }
    if (curlCode == CURLE_OK)
    {
      curlCode = curl_easy_setopt(curlHandle,CURLOPT_WRITEDATA,&storageDirectoryListHandle->ftp.lineList);
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
    Configuration_doneFTPServerSettings(&ftpServer);
    Password_done(&password);
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(pathName);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL void StorageFTP_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_FTP);

  #if   defined(HAVE_CURL)
    freeServer(storageDirectoryListHandle->ftp.serverId);
    String_delete(storageDirectoryListHandle->ftp.fileName);
    StringList_done(&storageDirectoryListHandle->ftp.lineList);
  #else /* not HAVE_CURL || HAVE_FTP */
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* HAVE_CURL || HAVE_FTP */
}

LOCAL bool StorageFTP_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  bool endOfDirectoryFlag;
  #if   defined(HAVE_CURL)
    String line;
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_FTP);

  endOfDirectoryFlag = TRUE;
  #if   defined(HAVE_CURL)
    while (   !storageDirectoryListHandle->ftp.entryReadFlag
           && !StringList_isEmpty(&storageDirectoryListHandle->ftp.lineList)
          )
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
                                                                            &storageDirectoryListHandle->ftp.permissions
                                                                           );

      // free resources
      String_delete(line);
    }

    endOfDirectoryFlag = !storageDirectoryListHandle->ftp.entryReadFlag;
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
  #else /* not HAVE_CURL || HAVE_FTP */
  #endif /* HAVE_CURL || HAVE_FTP */

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_FTP);

  error = ERROR_UNKNOWN;
  #if   defined(HAVE_CURL)
    while (   !storageDirectoryListHandle->ftp.entryReadFlag
           && !StringList_isEmpty(&storageDirectoryListHandle->ftp.lineList)
          )
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
                                                                            &storageDirectoryListHandle->ftp.permissions
                                                                           );

      // free resources
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
        fileInfo->timeLastChanged = storageDirectoryListHandle->ftp.timeModified;  // Note: no timestamp for meta data - use timestamp for content
        fileInfo->userId          = storageDirectoryListHandle->ftp.userId;
        fileInfo->groupId         = storageDirectoryListHandle->ftp.groupId;
        fileInfo->permissions     = storageDirectoryListHandle->ftp.permissions;
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
