/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: storage functions
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
#ifdef HAVE_FTP
  #include <ftplib.h>
#endif /* HAVE_FTP */
#ifdef HAVE_CURL
  #include <curl/curl.h>
#endif /* HAVE_CURL */
#ifdef HAVE_SSH2
  #include <libssh2.h>
  #include <libssh2_sftp.h>
#endif /* HAVE_SSH2 */
#ifdef HAVE_ISO9660
  #include <cdio/cdio.h>
  #include <cdio/iso9660.h>
#endif /* HAVE_ISO9660 */
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include "mxml.h"

#include "global.h"
#include "strings.h"
#include "stringlists.h"
#include "files.h"
#include "network.h"
#include "database.h"
#include "errors.h"

#include "errors.h"
#include "crypt.h"
#include "passwords.h"
#include "misc.h"
#include "archive.h"
#include "bar.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
/* file data buffer size */
#define BUFFER_SIZE (64*1024)

// different timeouts [ms]
#define FTP_TIMEOUT    (30*1000)
#define SSH_TIMEOUT    (30*1000)
#define WEBDAV_TIMEOUT (30*1000)
#define READ_TIMEOUT   (60*1000)

#define INITIAL_BUFFER_SIZE   (64*1024)
#define INCREMENT_BUFFER_SIZE ( 8*1024)
#define MAX_BUFFER_SIZE       (64*1024)
#define MAX_FILENAME_LENGTH   ( 8*1024)

#define UNLOAD_VOLUME_DELAY_TIME (10LL*MISC_US_PER_SECOND) /* [us] */
#define LOAD_VOLUME_DELAY_TIME   (10LL*MISC_US_PER_SECOND) /* [us] */

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

/***************************** Variables *******************************/
#if defined(HAVE_CURL) || defined(HAVE_FTP)
  LOCAL Password *defaultFTPPassword;
  LOCAL Password *defaultWebDAVPassword;
#endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */
#ifdef HAVE_SSH2
  LOCAL Password *defaultSSHPassword;
#endif /* HAVE_SSH2 */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : updateStatusInfo
* Purpose: update status info
* Input  : storageStatusInfo - storage status info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void updateStatusInfo(const StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);

  if (storageFileHandle->storageStatusInfoFunction != NULL)
  {
    storageFileHandle->storageStatusInfoFunction(storageFileHandle->storageStatusInfoUserData,
                                                 &storageFileHandle->runningInfo
                                                );
  }
}

#ifdef HAVE_CURL
/***********************************************************************\
* Name   : curlNopDataCallback
* Purpose: curl nop data callback: just discard data
* Input  : buffer   - buffer for data
*          size     - size of element
*          n        - number of elements
*          userData - user data
* Output : -
* Return : always size*n
* Notes  : -
\***********************************************************************/

LOCAL size_t curlNopDataCallback(void   *buffer,
                                 size_t size,
                                 size_t n,
                                 void   *userData
                                )
{
  UNUSED_VARIABLE(buffer);
  UNUSED_VARIABLE(userData);

  return size*n;
}
#endif /* HAVE_CURL */

#if defined(HAVE_CURL) || defined(HAVE_FTP)
/***********************************************************************\
* Name   : initFTPPassword
* Purpose: init FTP password
* Input  : hostName   - host name
*          loginName  - login name
*          jobOptions - job options
* Output : -
* Return : TRUE if FTP password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initFTPPassword(const String hostName, const String loginName, const JobOptions *jobOptions)
{
  SemaphoreLock semaphoreLock;
  String        s;
  bool          initFlag;

  assert(jobOptions != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    if (jobOptions->ftpServer.password == NULL)
    {
      if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
      {
        s = String_newCString("FTP login password for ");
        if (!String_isEmpty(loginName))
        {
          String_format(s,"'%S@%S'",loginName,hostName);
        }
        else
        {
          String_format(s,"'%S'",hostName);
        }
        initFlag = Password_input(defaultFTPPassword,String_cString(s),PASSWORD_INPUT_MODE_ANY);
        String_delete(s);
      }
      else
      {
        initFlag = FALSE;
      }
    }
    else
    {
      initFlag = TRUE;
    }
  }

  return initFlag;
}
#endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : initSSHPassword
* Purpose: init SSH password
* Input  : hostName   - host name
*          loginName  - login name
*          jobOptions - job options
* Output : -
* Return : TRUE if SSH password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initSSHPassword(const String hostName, const String loginName, const JobOptions *jobOptions)
{
  SemaphoreLock semaphoreLock;
  String        s;
  bool          initFlag;

  assert(jobOptions != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    if (jobOptions->sshServer.password == NULL)
    {
      if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
      {
        s = String_newCString("SSH login password for ");
        if (!String_isEmpty(loginName))
        {
          String_format(s,"'%S@%S'",loginName,hostName);
        }
        else
        {
          String_format(s,"'%S'",hostName);
        }
        initFlag = Password_input(defaultSSHPassword,String_cString(s),PASSWORD_INPUT_MODE_ANY);
        String_delete(s);
      }
      else
      {
        initFlag = FALSE;
      }
    }
    else
    {
      initFlag = TRUE;
    }
  }

  return initFlag;
}
#endif /* HAVE_SSH2 */

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

LOCAL Errors checkFTPLogin(const String hostName,
                           uint         hostPort,
                           const String loginName,
                           Password     *loginPassword
                          )
{
  #if   defined(HAVE_CURL)
    CURL       *curlHandle;
    String     url;
    CURLcode   curlCode;
    const char *plainLoginPassword;
  #elif defined(HAVE_FTP)
    netbuf     *ftpControl;
    const char *plainLoginPassword;
  #endif

  #if   defined(HAVE_CURL)
    // init handle
    curlHandle = curl_easy_init();
    if (curlHandle == NULL)
    {
      return ERROR_FTP_SESSION_FAIL;
    }
    (void)curl_easy_setopt(curlHandle,CURLOPT_FTP_RESPONSE_TIMEOUT,FTP_TIMEOUT/1000);
    if (globalOptions.verboseLevel >= 6)
    {
      (void)curl_easy_setopt(curlHandle,CURLOPT_VERBOSE,1L);
    }

    // set connect
    url = String_format(String_new(),"ftp://%S",hostName);
    if (hostPort != 0) String_format(url,":%d",hostPort);
    curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
    if (curlCode != CURLE_OK)
    {
      (void)curl_easy_cleanup(curlHandle);
      return ERRORX_(FTP_SESSION_FAIL,0,curl_easy_strerror(curlCode));
    }
    (void)curl_easy_setopt(curlHandle,CURLOPT_CONNECT_ONLY,1L);
    String_delete(url);

    // set login
    (void)curl_easy_setopt(curlHandle,CURLOPT_USERNAME,String_cString(loginName));
    plainLoginPassword = Password_deploy(loginPassword);
    (void)curl_easy_setopt(curlHandle,CURLOPT_PASSWORD,plainLoginPassword);
    Password_undeploy(loginPassword);

    // login
    curlCode = curl_easy_perform(curlHandle);
    if (curlCode != CURLE_OK)
    {
      (void)curl_easy_cleanup(curlHandle);
      return ERRORX_(FTP_AUTHENTICATION,0,curl_easy_strerror(curlCode));
    }

    // free resources
    (void)curl_easy_cleanup(curlHandle);
  #elif defined(HAVE_FTP)
// NYI: TODO: support different FTP port
    UNUSED_VARIABLE(hostPort);

    // check host name (Note: FTP library crash if host name is not valid!)
    if (!Network_hostExists(hostName))
    {
      return ERROR_HOST_NOT_FOUND;
    }

    // connect
    if (FtpConnect(String_cString(hostName),&ftpControl) != 1)
    {
      return ERROR_FTP_SESSION_FAIL;
    }

    // login
    plainLoginPassword = Password_deploy(loginPassword);
    if (FtpLogin(String_cString(loginName),
                 plainLoginPassword,
                 ftpControl
                ) != 1
       )
    {
      Password_undeploy(loginPassword);
      FtpClose(ftpControl);
      return ERROR_FTP_AUTHENTICATION;
    }
    Password_undeploy(loginPassword);
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
  StorageFileHandle *storageFileHandle = (StorageFileHandle*)userData;
  size_t            bytesSent;

  assert(buffer != NULL);
  assert(size > 0);
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->ftp.buffer != NULL);

  if (storageFileHandle->ftp.transferedBytes < storageFileHandle->ftp.length)
  {
    bytesSent = MIN(n,(size_t)(storageFileHandle->ftp.length-storageFileHandle->ftp.transferedBytes)/size)*size;

    memcpy(buffer,storageFileHandle->ftp.buffer,bytesSent);

    storageFileHandle->ftp.buffer          = (byte*)storageFileHandle->ftp.buffer+bytesSent;
    storageFileHandle->ftp.transferedBytes += (ulong)bytesSent;
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
  StorageFileHandle *storageFileHandle = (StorageFileHandle*)userData;
  size_t            bytesReceived;

  assert(buffer != NULL);
  assert(size > 0);
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->ftp.buffer != NULL);

  if (storageFileHandle->ftp.transferedBytes < storageFileHandle->ftp.length)
  {
    bytesReceived = MIN(n,(size_t)(storageFileHandle->ftp.length-storageFileHandle->ftp.transferedBytes)/size)*size;

    memcpy(storageFileHandle->ftp.buffer,buffer,bytesReceived);

    storageFileHandle->ftp.buffer          = (byte*)storageFileHandle->ftp.buffer+bytesReceived;
    storageFileHandle->ftp.transferedBytes += (ulong)bytesReceived;
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
* Input  : loginName - login name
*          password  - login password
*          hostName  - host name
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
      if (strcasecmp(MONTH_DEFINITIONS[z].name,s) == 0)
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
      if (strcasecmp(MONTH_DEFINITIONS[z].name,s) == 0)
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
#endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : checkSSHLogin
* Purpose: check if SSH login is possible
* Input  : loginName          - login name
*          password           - login password
*          hostName           - host name
*          hostPort           - host SSH port
*          publicKeyFileName  - SSH public key file name
*          privateKeyFileName - SSH private key file name
* Output : -
* Return : ERROR_NONE if login is possible, error code otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors checkSSHLogin(const String loginName,
                           Password     *password,
                           const String hostName,
                           uint         hostPort,
                           const String publicKeyFileName,
                           const String privateKeyFileName
                          )
{
  SocketHandle socketHandle;
  Errors       error;

  printInfo(5,"SSH: public key '%s'\n",String_cString(publicKeyFileName));
  printInfo(5,"SSH: private key '%s'\n",String_cString(privateKeyFileName));
  error = Network_connect(&socketHandle,
                          SOCKET_TYPE_SSH,
                          hostName,
                          hostPort,
                          loginName,
                          password,
                          publicKeyFileName,
                          privateKeyFileName,
                          0
                         );
  if (error != ERROR_NONE)
  {
    return error;
  }
  Network_disconnect(&socketHandle);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : sshSendCallback
* Purpose: ssh send callback: count total send bytes and pass to
*          original function
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to send
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes sent
* Notes  : -
\***********************************************************************/

LOCAL LIBSSH2_SEND_FUNC(sshSendCallback)
{
  StorageFileHandle *storageFileHandle;
  ssize_t           n;

  assert(abstract != NULL);

  storageFileHandle = *((StorageFileHandle**)abstract);
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->scp.oldSendCallback != NULL);

  n = storageFileHandle->scp.oldSendCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageFileHandle->scp.totalSentBytes += (uint64)n;

  return n;
}

/***********************************************************************\
* Name   : sshReceiveCallback
* Purpose: ssh receive callback: count total received bytes and pass to
*          original function
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to receive
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes received
* Notes  : -
\***********************************************************************/

LOCAL LIBSSH2_RECV_FUNC(sshReceiveCallback)
{
  StorageFileHandle *storageFileHandle;
  ssize_t           n;

  assert(abstract != NULL);

  storageFileHandle = *((StorageFileHandle**)abstract);
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->scp.oldReceiveCallback != NULL);

  n = storageFileHandle->scp.oldReceiveCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageFileHandle->scp.totalReceivedBytes += (uint64)n;

  return n;
}

/***********************************************************************\
* Name   : sftpSendCallback
* Purpose: sftp send callback: count total sent bytes and pass to
*          original function
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to send
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes sent
* Notes  : -
\***********************************************************************/

LOCAL LIBSSH2_SEND_FUNC(sftpSendCallback)
{
  StorageFileHandle *storageFileHandle;
  ssize_t           n;

  assert(abstract != NULL);

  storageFileHandle = *((StorageFileHandle**)abstract);
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->sftp.oldSendCallback != NULL);

  n = storageFileHandle->sftp.oldSendCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageFileHandle->sftp.totalSentBytes += (uint64)n;

  return n;
}

/***********************************************************************\
* Name   : sftpReceiveCallback
* Purpose: sftp receive callback: count total received bytes and pass to
*          original function
* Input  : socket   - libssh2 socket
*          buffer   - buffer with data
*          length   - length to receive
*          flags    - libssh2 flags
*          abstract - pointer to user data
* Output : -
* Return : number of bytes received
* Notes  : -
\***********************************************************************/

LOCAL LIBSSH2_RECV_FUNC(sftpReceiveCallback)
{
  StorageFileHandle *storageFileHandle;
  ssize_t           n;

  assert(abstract != NULL);

  storageFileHandle = *((StorageFileHandle**)abstract);
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->sftp.oldReceiveCallback != NULL);

  n = storageFileHandle->sftp.oldReceiveCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageFileHandle->sftp.totalReceivedBytes += (uint64)n;

  return n;
}

/***********************************************************************\
* Name   : waitSSHSessionSocket
* Purpose: wait a little until SSH session socket can be read/write
* Input  : socketHandle - socket handle
* Output : -
* Return : TRUE if session socket can be read/write, FALSE on
+          error/timeout
* Notes  : -
\***********************************************************************/

LOCAL bool waitSSHSessionSocket(SocketHandle *socketHandle)
{
  LIBSSH2_SESSION *session;
  sigset_t        signalMask;
  struct timespec ts;
  fd_set          fdSet;

  assert(socketHandle != NULL);

  // get session
  session = Network_getSSHSession(socketHandle);
  assert(session != NULL);

  // Note: ignore SIGALRM in pselect()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

  // wait for max. 60s
  ts.tv_sec  = 60L;
  ts.tv_nsec = 0L;
  FD_ZERO(&fdSet);
  FD_SET(socketHandle->handle,&fdSet);
  return (pselect(socketHandle->handle+1,
                 ((libssh2_session_block_directions(session) & LIBSSH2_SESSION_BLOCK_INBOUND ) != 0) ? &fdSet : NULL,
                 ((libssh2_session_block_directions(session) & LIBSSH2_SESSION_BLOCK_OUTBOUND) != 0) ? &fdSet : NULL,
                 NULL,
                 &ts,
                 &signalMask
                ) > 0
         );
}
#endif /* HAVE_SSH2 */

#ifdef HAVE_CURL
/***********************************************************************\
* Name   : initWebDAVPassword
* Purpose: init WebDAV password
* Input  : hostName   - host name
*          loginName  - login name
*          jobOptions - job options
* Output : -
* Return : TRUE if WebDAV password intialized, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool initWebDAVPassword(const String hostName, const String loginName, const JobOptions *jobOptions)
{
  SemaphoreLock semaphoreLock;
  String        s;
  bool          initFlag;

  assert(jobOptions != NULL);

  SEMAPHORE_LOCKED_DO(semaphoreLock,&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE)
  {
    if (jobOptions->webdavServer.password == NULL)
    {
      if (globalOptions.runMode == RUN_MODE_INTERACTIVE)
      {
        s = !String_isEmpty(loginName)
              ? String_format(String_new(),"WebDAV login password for %S@%S",loginName,hostName)
              : String_format(String_new(),"WebDAV login password for %S",hostName);
        initFlag = Password_input(defaultWebDAVPassword,String_cString(s),PASSWORD_INPUT_MODE_ANY);
        String_delete(s);
      }
      else
      {
        initFlag = FALSE;
      }
    }
    else
    {
      initFlag = TRUE;
    }
  }

  return initFlag;
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

LOCAL Errors checkWebDAVLogin(const String hostName,
                              const String loginName,
                              Password     *loginPassword
                             )
{
  CURL       *curlHandle;
  String     url;
  CURLcode   curlCode;
  const char *plainLoginPassword;

  // init handle
  curlHandle = curl_easy_init();
  if (curlHandle == NULL)
  {
    return ERROR_WEBDAV_SESSION_FAIL;
  }
  (void)curl_easy_setopt(curlHandle,CURLOPT_CONNECTTIMEOUT_MS,WEBDAV_TIMEOUT);
  if (globalOptions.verboseLevel >= 6)
  {
    (void)curl_easy_setopt(curlHandle,CURLOPT_VERBOSE,1L);
  }

  // set connect
  url = String_format(String_new(),"http://%S",hostName);
  curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
  if (curlCode != CURLE_OK)
  {
    (void)curl_easy_cleanup(curlHandle);
    return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_easy_strerror(curlCode));
  }
  (void)curl_easy_setopt(curlHandle,CURLOPT_CONNECT_ONLY,1L);
  String_delete(url);

  // set login
  (void)curl_easy_setopt(curlHandle,CURLOPT_USERNAME,String_cString(loginName));
  plainLoginPassword = Password_deploy(loginPassword);
  (void)curl_easy_setopt(curlHandle,CURLOPT_PASSWORD,plainLoginPassword);
  Password_undeploy(loginPassword);

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
* Purpose: curl WebDAV read data callback: receive data from remote
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
  StorageFileHandle *storageFileHandle = (StorageFileHandle*)userData;
  size_t            bytesSent;

  assert(buffer != NULL);
  assert(size > 0);
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->webdav.sendBuffer.data != NULL);

  if (storageFileHandle->webdav.sendBuffer.index < storageFileHandle->webdav.sendBuffer.length)
  {
    bytesSent = MIN(n,(size_t)(storageFileHandle->webdav.sendBuffer.length-storageFileHandle->webdav.sendBuffer.index)/size)*size;

    memcpy(buffer,storageFileHandle->webdav.sendBuffer.data+storageFileHandle->webdav.sendBuffer.index,bytesSent);
    storageFileHandle->webdav.sendBuffer.index += (ulong)bytesSent;
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
* Purpose: curl WebDAV write data callback: send data to remote
* Input  : buffer   - buffer with data
*          size     - size of an element
*          n        - number of elements
*          userData - user data
* Output : -
* Return : number of written bytes or 0
* Notes  : -
\***********************************************************************/

LOCAL size_t curlWebDAVWriteDataCallback(const void *buffer,
                                         size_t     size,
                                         size_t     n,
                                         void       *userData
                                        )
{
  StorageFileHandle *storageFileHandle = (StorageFileHandle*)userData;
  ulong             bytesReceived;
  ulong             newSize;
  byte              *newData;

  assert(buffer != NULL);
  assert(size > 0);
  assert(storageFileHandle != NULL);

  // calculate number of received bytes
  bytesReceived = n*size;

  // increase buffer if required
  if ((storageFileHandle->webdav.receiveBuffer.length+bytesReceived) > storageFileHandle->webdav.receiveBuffer.size)
  {
    newSize = ((storageFileHandle->webdav.receiveBuffer.length+bytesReceived+INCREMENT_BUFFER_SIZE-1)/INCREMENT_BUFFER_SIZE)*INCREMENT_BUFFER_SIZE;
    newData = (byte*)realloc(storageFileHandle->webdav.receiveBuffer.data,newSize);
    if (newData == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    storageFileHandle->webdav.receiveBuffer.data = newData;
    storageFileHandle->webdav.receiveBuffer.size = newSize;
  }

  // append to data
  memcpy(storageFileHandle->webdav.receiveBuffer.data+storageFileHandle->webdav.receiveBuffer.length,buffer,bytesReceived);
  storageFileHandle->webdav.receiveBuffer.length += bytesReceived;
//static size_t totalReceived = 0;
//totalReceived+=bytesReceived;
//fprintf(stderr,"%s, %d: storageFileHandle->webdav.receiveBuffer.length=%d bytesReceived=%d %d\n",__FILE__,__LINE__,storageFileHandle->webdav.receiveBuffer.length,bytesReceived,totalReceived);

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

LOCAL Errors waitCurlSocket(CURLM *curlMultiHandle)
{
  sigset_t        signalMask;
  fd_set          fdSetRead,fdSetWrite,fdSetException;
  int             maxFD;
  long            curlTimeout;
  struct timespec ts;
  Errors          error;

  assert(curlMultiHandle != NULL);

  // Note: ignore SIGALRM in pselect()
  sigemptyset(&signalMask);
  sigaddset(&signalMask,SIGALRM);

  // get file descriptors from the transfers
  FD_ZERO(&fdSetRead);
  FD_ZERO(&fdSetWrite);
  FD_ZERO(&fdSetException);
  curl_multi_fdset(curlMultiHandle,&fdSetRead,&fdSetWrite,&fdSetException,&maxFD);

  // get a suitable timeout
  curl_multi_timeout(curlMultiHandle,&curlTimeout);

  if (curlTimeout > (long)READ_TIMEOUT)
  {
    ts.tv_sec  = curlTimeout/1000L;
    ts.tv_nsec = (curlTimeout%1000L)*1000000L;
  }
  else
  {
    ts.tv_sec  = READ_TIMEOUT/1000L;
    ts.tv_nsec = (READ_TIMEOUT%1000L)*1000000L;
  }

  // wait
  switch (pselect(maxFD+1,&fdSetRead,&fdSetWrite,&fdSetException,&ts,&signalMask))
  {
    case -1:
      // error
      error = ERROR_NETWORK_RECEIVE;
      break;
    case 0:
      // timeout
      error = ERROR_NETWORK_TIMEOUT;
      break;
    default:
      // OK
      error = ERROR_NONE;
      break;
  }

  return error;
}

#endif /* HAVE_CURL */

#if defined(HAVE_CURL) || defined(HAVE_FTP) || defined(HAVE_SSH2)
/***********************************************************************\
* Name   : initBandWidthLimiter
* Purpose: init band width limiter structure
* Input  : storageBandWidthLimiter - storage band width limiter
*          maxBandWidthList        - list with max. band width to use
*                                    [bit/s] or NULL
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

uint64 xbytes = 0;
uint64 xtime  = 0;

LOCAL void initBandWidthLimiter(StorageBandWidthLimiter *storageBandWidthLimiter,
                                BandWidthList           *maxBandWidthList
                               )
{
  ulong maxBandWidth;
  uint  z;

  assert(storageBandWidthLimiter != NULL);

  maxBandWidth = getBandWidth(maxBandWidthList);

  storageBandWidthLimiter->maxBandWidthList     = maxBandWidthList;
  storageBandWidthLimiter->maxBlockSize         = 64*1024;
  storageBandWidthLimiter->blockSize            = 64*1024;
  for (z = 0; z < SIZE_OF_ARRAY(storageBandWidthLimiter->measurements); z++)
  {
    storageBandWidthLimiter->measurements[z] = maxBandWidth;
  }
  storageBandWidthLimiter->measurementCount     = 0;
  storageBandWidthLimiter->measurementNextIndex = 0;
  storageBandWidthLimiter->measurementBytes     = 0L;
  storageBandWidthLimiter->measurementTime      = 0LL;

xbytes = 0;
xtime  = 0;
}
#endif /* defined(HAVE_CURL) || defined(HAVE_FTP) || defined(HAVE_SSH2) */

#if defined(HAVE_CURL) || defined(HAVE_FTP) || defined(HAVE_SSH2)
/***********************************************************************\
* Name   : limitBandWidth
* Purpose: limit used band width
* Input  : storageBandWidthLimiter - storage band width limiter
*          transmittedBytes        - transmitted bytes
*          transmissionTime        - time for transmission [us]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void limitBandWidth(StorageBandWidthLimiter *storageBandWidthLimiter,
                          ulong                   transmittedBytes,
                          uint64                  transmissionTime
                         )
{
  uint   z;
  ulong  averageBandWidth;   // average band width [bits/s]
  ulong  maxBandWidth;
  uint64 calculatedTime;
  uint64 delayTime;          // delay time [us]

  assert(storageBandWidthLimiter != NULL);

xbytes += transmittedBytes;
xtime  += transmissionTime;
//fprintf(stderr,"%s, %d: %llu bytes/s\n",__FILE__,__LINE__,(xbytes*1000000LL)/xtime);


  if (storageBandWidthLimiter->maxBandWidthList != NULL)
  {
    storageBandWidthLimiter->measurementBytes += transmittedBytes;
    storageBandWidthLimiter->measurementTime += transmissionTime;
//fprintf(stderr,"%s, %d: sum %lu bytes %llu us\n",__FILE__,__LINE__,storageBandWidthLimiter->measurementBytes,storageBandWidthLimiter->measurementTime);

    if (storageBandWidthLimiter->measurementTime > MS_TO_US(100LL))   // too small time values are not reliable, thus accumlate over time
    {
      // calculate average band width
      averageBandWidth = 0;
      if (storageBandWidthLimiter->measurementCount > 0)
      {
        for (z = 0; z < storageBandWidthLimiter->measurementCount; z++)
        {
          averageBandWidth += storageBandWidthLimiter->measurements[z];
        }
        averageBandWidth /= storageBandWidthLimiter->measurementCount;
      }
      else
      {
        averageBandWidth = 0L;
      }
//fprintf(stderr,"%s, %d: averageBandWidth=%lu bits/s\n",__FILE__,__LINE__,averageBandWidth);

      // get max. band width to use
      maxBandWidth = getBandWidth(storageBandWidthLimiter->maxBandWidthList);

      // calculate delay time
      if (maxBandWidth > 0L)
      {
        calculatedTime = (BYTES_TO_BITS(storageBandWidthLimiter->measurementBytes)*US_PER_SECOND)/maxBandWidth;
        delayTime      = (calculatedTime > storageBandWidthLimiter->measurementTime) ? calculatedTime-storageBandWidthLimiter->measurementTime : 0LL;
      }
      else
      {
        // no band width limit -> no delay
        delayTime = 0LL;
      }

#if 0
/* disabled 2012-08-26: it seems the underlaying libssh2 implementation
   always transmit data in block sizes >32k? Thus it is not useful to
   reduce the block size, because this only cause additional overhead
   without any effect on the effectively used band width.
*/

      // recalculate optimal block size for band width limitation (hystersis: +-1024 bytes/s)
      if (maxBandWidth > 0L)
      {
        if     (   (averageBandWidth > (maxBandWidth+BYTES_TO_BITS(1024)))
                || (delayTime > 0LL)
               )
        {
          // recalculate max. block size to send to send a little less in a single step (if possible)
          if      ((storageBandWidthLimiter->blockSize > 32*1024) && (delayTime > 30000LL*MS_PER_SECOND)) storageBandWidthLimiter->blockSize -= 32*1024;
          else if ((storageBandWidthLimiter->blockSize > 16*1024) && (delayTime > 10000LL*MS_PER_SECOND)) storageBandWidthLimiter->blockSize -= 16*1024;
          else if ((storageBandWidthLimiter->blockSize >  8*1024) && (delayTime >  5000LL*MS_PER_SECOND)) storageBandWidthLimiter->blockSize -=  8*1024;
          else if ((storageBandWidthLimiter->blockSize >  4*1024) && (delayTime >  1000LL*MS_PER_SECOND)) storageBandWidthLimiter->blockSize -=  4*1024;
//fprintf(stderr,"%s,%d: storageBandWidthLimiter->measurementBytes=%ld storageBandWidthLimiter->max=%ld calculated time=%llu us storageBandWidthLimiter->measurementTime=%lu us blockSize=%ld\n",__FILE__,__LINE__,storageBandWidthLimiter->measurementBytes,storageBandWidthLimiter->max,calculatedTime,storageBandWidthLimiter->measurementTime,storageBandWidthLimiter->blockSize);
        }
        else if (averageBandWidth < (maxBandWidth-BYTES_TO_BITS(1024)))
        {
          // increase max. block size to send a little more in a single step (if possible)
          if (storageBandWidthLimiter->blockSize < (storageBandWidthLimiter->maxBlockSize-4*1024)) storageBandWidthLimiter->blockSize += 4*1024;
//fprintf(stderr,"%s,%d: ++ averageBandWidth=%lu bit/s storageBandWidthLimiter->max=%lu bit/s storageBandWidthLimiter->measurementTime=%llu us storageBandWidthLimiter->blockSize=%llu\n",__FILE__,__LINE__,averageBandWidth,storageBandWidthLimiter->max,storageBandWidthLimiter->measurementTime,storageBandWidthLimiter->blockSize);
        }
      }
#endif /* 0 */

      // delay if needed
      if (delayTime > 0)
      {
        Misc_udelay(delayTime);
      }

      // calculate and store new current bandwidth
      storageBandWidthLimiter->measurements[storageBandWidthLimiter->measurementNextIndex] = (ulong)((BYTES_TO_BITS((uint64)storageBandWidthLimiter->measurementBytes)*US_PER_SECOND)/(storageBandWidthLimiter->measurementTime+delayTime));
//fprintf(stderr,"%s, %d: new measurement[%d] %lu bits/us\n",__FILE__,__LINE__,storageBandWidthLimiter->measurementNextIndex,storageBandWidthLimiter->measurements[storageBandWidthLimiter->measurementNextIndex]);
      storageBandWidthLimiter->measurementNextIndex = (storageBandWidthLimiter->measurementNextIndex+1)%SIZE_OF_ARRAY(storageBandWidthLimiter->measurements);
      if (storageBandWidthLimiter->measurementCount < SIZE_OF_ARRAY(storageBandWidthLimiter->measurements)) storageBandWidthLimiter->measurementCount++;

      // reset accumulated measurement
      storageBandWidthLimiter->measurementBytes = 0L;
      storageBandWidthLimiter->measurementTime  = 0LL;
    }
  }
}
#endif /* defined(HAVE_CURL) || defined(HAVE_FTP) || defined(HAVE_SSH2) */

/***********************************************************************\
* Name   : executeIOOutput
* Purpose: process exec stdout, stderr output
* Input  : userData - user data (not used)
*          line     - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOOutput(void         *userData,
                           const String line
                          )
{
  assert(line != NULL);

  UNUSED_VARIABLE(userData);

  printInfo(4,"%s\n",String_cString(line));
}

/***********************************************************************\
* Name   : requestNewMedium
* Purpose: request new cd/dvd/bd medium
* Input  : storageFileHandle - storage file handle
*          waitFlag          - TRUE to wait for new medium
* Output : -
* Return : TRUE if new medium loaded, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL Errors requestNewMedium(StorageFileHandle *storageFileHandle, bool waitFlag)
{
  TextMacro             textMacros[2];
  bool                  mediumRequestedFlag;
  StorageRequestResults storageRequestResult;

  TEXT_MACRO_N_STRING (textMacros[0],"%device",storageFileHandle->storageSpecifier.deviceName);
  TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageFileHandle->requestedVolumeNumber      );

  if (   (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    // sleep a short time to give hardware time for finishing volume, then unload current volume
    printInfo(0,"Unload medium #%d...",storageFileHandle->volumeNumber);
    Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
    Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.unloadVolumeCommand),
                        textMacros,SIZE_OF_ARRAY(textMacros),
                        (ExecuteIOFunction)executeIOOutput,
                        (ExecuteIOFunction)executeIOOutput,
                        NULL
                       );
    printInfo(0,"ok\n");

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  // request new medium
  mediumRequestedFlag  = FALSE;
  storageRequestResult = STORAGE_REQUEST_VOLUME_UNKNOWN;
  if      (storageFileHandle->requestVolumeFunction != NULL)
  {
    mediumRequestedFlag = TRUE;

    // request new medium via call back, unload if requested
    do
    {
      storageRequestResult = storageFileHandle->requestVolumeFunction(storageFileHandle->requestVolumeUserData,
                                                                      storageFileHandle->requestedVolumeNumber
                                                                     );
      if (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD)
      {
        // sleep a short time to give hardware time for finishing volume, then unload current medium
        printInfo(0,"Unload medium...");
        Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.unloadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           );
        printInfo(0,"ok\n");
      }
    }
    while (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD);

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (storageFileHandle->opticalDisk.write.requestVolumeCommand != NULL)
  {
    mediumRequestedFlag = TRUE;

    // request new volume via external command
    printInfo(0,"Request new medium #%d...",storageFileHandle->requestedVolumeNumber);
    if (Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.requestVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           ) == ERROR_NONE
       )
    {
      printInfo(0,"ok\n");
      storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
    }
    else
    {
      printInfo(0,"FAIL\n");
      storageRequestResult = STORAGE_REQUEST_VOLUME_FAIL;
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else
  {
    if (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
    {
      if (waitFlag)
      {
        mediumRequestedFlag = TRUE;

        printInfo(0,"Please insert medium #%d into drive '%s' and press ENTER to continue\n",storageFileHandle->requestedVolumeNumber,String_cString(storageFileHandle->storageSpecifier.deviceName));
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
      else
      {
        printInfo(0,"Please insert medium #%d into drive '%s'\n",storageFileHandle->requestedVolumeNumber,String_cString(storageFileHandle->storageSpecifier.deviceName));
      }
    }
    else
    {
      if (waitFlag)
      {
        mediumRequestedFlag = TRUE;

        printInfo(0,"Press ENTER to continue\n");
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (mediumRequestedFlag)
  {
    switch (storageRequestResult)
    {
      case STORAGE_REQUEST_VOLUME_OK:
        // load volume, then sleep a short time to give hardware time for reading volume information
        printInfo(0,"Load medium #%d...",storageFileHandle->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           );
        Misc_udelay(LOAD_VOLUME_DELAY_TIME);
        printInfo(0,"ok\n");

        // store new volume number
        storageFileHandle->volumeNumber = storageFileHandle->requestedVolumeNumber;

        // update status info
        storageFileHandle->runningInfo.volumeNumber = storageFileHandle->volumeNumber;
        updateStatusInfo(storageFileHandle);

        storageFileHandle->volumeState = STORAGE_VOLUME_STATE_LOADED;
        return ERROR_NONE;
        break;
      case STORAGE_REQUEST_VOLUME_ABORTED:
        return ERROR_NONE;
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
* Name   : requestNewVolume
* Purpose: request new volume
* Input  : storageFileHandle - storage file handle
*          waitFlag          - TRUE to wait for new volume
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors requestNewVolume(StorageFileHandle *storageFileHandle, bool waitFlag)
{
  TextMacro             textMacros[2];
  bool                  volumeRequestedFlag;
  StorageRequestResults storageRequestResult;

  TEXT_MACRO_N_STRING (textMacros[0],"%device",storageFileHandle->storageSpecifier.deviceName);
  TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageFileHandle->requestedVolumeNumber);

  if (   (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNKNOWN)
      || (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_LOADED)
     )
  {
    // sleep a short time to give hardware time for finishing volume; unload current volume
    printInfo(0,"Unload volume #%d...",storageFileHandle->volumeNumber);
    Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
    Misc_executeCommand(String_cString(storageFileHandle->device.unloadVolumeCommand),
                        textMacros,SIZE_OF_ARRAY(textMacros),
                        (ExecuteIOFunction)executeIOOutput,
                        (ExecuteIOFunction)executeIOOutput,
                        NULL
                       );
    printInfo(0,"ok\n");

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_UNLOADED;
  }

  // request new volume
  volumeRequestedFlag  = FALSE;
  storageRequestResult = STORAGE_REQUEST_VOLUME_UNKNOWN;
  if      (storageFileHandle->requestVolumeFunction != NULL)
  {
    volumeRequestedFlag = TRUE;

    // request new volume via call back, unload if requested
    do
    {
      storageRequestResult = storageFileHandle->requestVolumeFunction(storageFileHandle->requestVolumeUserData,
                                                                      storageFileHandle->requestedVolumeNumber
                                                                     );
      if (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD)
      {
        // sleep a short time to give hardware time for finishing volume, then unload current medium
        printInfo(0,"Unload volume...");
        Misc_udelay(UNLOAD_VOLUME_DELAY_TIME);
        Misc_executeCommand(String_cString(storageFileHandle->device.unloadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           );
        printInfo(0,"ok\n");
      }
    }
    while (storageRequestResult == STORAGE_REQUEST_VOLUME_UNLOAD);

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else if (storageFileHandle->device.requestVolumeCommand != NULL)
  {
    volumeRequestedFlag = TRUE;

    // request new volume via external command
    printInfo(0,"Request new volume #%d...",storageFileHandle->requestedVolumeNumber);
    if (Misc_executeCommand(String_cString(storageFileHandle->device.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           ) == ERROR_NONE
       )
    {
      printInfo(0,"ok\n");
      storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
    }
    else
    {
      printInfo(0,"FAIL\n");
      storageRequestResult = STORAGE_REQUEST_VOLUME_FAIL;
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }
  else
  {
    if (storageFileHandle->volumeState == STORAGE_VOLUME_STATE_UNLOADED)
    {
      if (waitFlag)
      {
        volumeRequestedFlag = TRUE;

        printInfo(0,"Please insert volume #%d into drive '%s' and press ENTER to continue\n",storageFileHandle->requestedVolumeNumber,storageFileHandle->storageSpecifier.deviceName);
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
      else
      {
        printInfo(0,"Please insert volume #%d into drive '%s'\n",storageFileHandle->requestedVolumeNumber,storageFileHandle->storageSpecifier.deviceName);
      }
    }
    else
    {
      if (waitFlag)
      {
        volumeRequestedFlag = TRUE;

        printInfo(0,"Press ENTER to continue\n");
        Misc_waitEnter();

        storageRequestResult = STORAGE_REQUEST_VOLUME_OK;
      }
    }

    storageFileHandle->volumeState = STORAGE_VOLUME_STATE_WAIT;
  }

  if (volumeRequestedFlag)
  {
    switch (storageRequestResult)
    {
      case STORAGE_REQUEST_VOLUME_OK:
        // load volume; sleep a short time to give hardware time for reading volume information
        printInfo(0,"Load volume #%d...",storageFileHandle->requestedVolumeNumber);
        Misc_executeCommand(String_cString(storageFileHandle->device.loadVolumeCommand),
                            textMacros,SIZE_OF_ARRAY(textMacros),
                            (ExecuteIOFunction)executeIOOutput,
                            (ExecuteIOFunction)executeIOOutput,
                            NULL
                           );
        Misc_udelay(LOAD_VOLUME_DELAY_TIME);
        printInfo(0,"ok\n");

        // store new volume number
        storageFileHandle->volumeNumber = storageFileHandle->requestedVolumeNumber;

        // update status info
        storageFileHandle->runningInfo.volumeNumber = storageFileHandle->volumeNumber;
        updateStatusInfo(storageFileHandle);

        storageFileHandle->volumeState = STORAGE_VOLUME_STATE_LOADED;
        return ERROR_NONE;
        break;
      case STORAGE_REQUEST_VOLUME_ABORTED:
        return ERROR_NONE;
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
* Name   : executeIOmkisofs
* Purpose: process mkisofs output
* Input  : storageFileHandle - storage file handle variable
*          line              - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOmkisofs(StorageFileHandle *storageFileHandle,
                            const String      line
                           )
{
  String s;
  double p;

  assert(storageFileHandle != NULL);

//fprintf(stderr,"%s,%d: line=%s\n",__FILE__,__LINE__,String_cString(line));
  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".* ([0-9\\.]+)% done.*",NULL,NULL,s,NULL))
  {
//fprintf(stderr,"%s,%d: mkisofs: %s\n",__FILE__,__LINE__,String_cString(line));
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)storageFileHandle->opticalDisk.write.step*100.0+p)/(double)(storageFileHandle->opticalDisk.write.steps*100);
    updateStatusInfo(storageFileHandle);
  }
  String_delete(s);

  executeIOOutput(NULL,line);
}

/***********************************************************************\
* Name   : executeIODVDisaster
* Purpose: process dvdisaster output
* Input  : storageFileHandle - storage file handle variable
*          line              - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOdvdisaster(StorageFileHandle *storageFileHandle,
                               const String      line
                              )
{
  String s;
  double p;

  assert(storageFileHandle != NULL);

  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".*adding space\\): +([0-9\\.]+)%",NULL,NULL,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)(storageFileHandle->opticalDisk.write.step+0)*100.0+p)/(double)(storageFileHandle->opticalDisk.write.steps*100);
    updateStatusInfo(storageFileHandle);
  }
  if (String_matchCString(line,STRING_BEGIN,".*generation: +([0-9\\.]+)%",NULL,NULL,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)(storageFileHandle->opticalDisk.write.step+1)*100.0+p)/(double)(storageFileHandle->opticalDisk.write.steps*100);
    updateStatusInfo(storageFileHandle);
  }
  String_delete(s);

  executeIOOutput(NULL,line);
}

/***********************************************************************\
* Name   : executeIOgrowisofs
* Purpose: process growisofs output
* Input  : storageFileHandle - storage file handle variable
*          line              - line
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void executeIOgrowisofs(StorageFileHandle *storageFileHandle,
                              const String      line
                             )
{
  String s;
  double p;

  assert(storageFileHandle != NULL);

  s = String_new();
  if (String_matchCString(line,STRING_BEGIN,".* \\(([0-9\\.]+)%\\) .*",NULL,NULL,s,NULL))
  {
    p = String_toDouble(s,0,NULL,NULL,0);
    storageFileHandle->runningInfo.volumeProgress = ((double)storageFileHandle->opticalDisk.write.step*100.0+p)/(double)(storageFileHandle->opticalDisk.write.steps*100);
    updateStatusInfo(storageFileHandle);
  }
  String_delete(s);

  executeIOOutput(NULL,line);
}

/*---------------------------------------------------------------------*/

Errors Storage_initAll(void)
{
  #if   defined(HAVE_CURL)
    if (curl_global_init(CURL_GLOBAL_ALL) != 0)
    {
      return ERROR_INIT_STORAGE;
    }
  #elif defined(HAVE_FTP)
    FtpInit();
  #endif /* HAVE_CURL || HAVE_FTP */
  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    defaultFTPPassword    = Password_new();
    defaultWebDAVPassword = Password_new();
  #endif /* HAVE_CURL */
  #ifdef HAVE_SSH2
    defaultSSHPassword = Password_new();
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

void Storage_doneAll(void)
{
  #ifdef HAVE_SSH2
    Password_delete(defaultSSHPassword);
  #endif /* HAVE_SSH2 */
  #if defined(HAVE_CURL) || defined(HAVE_FTP)
    Password_delete(defaultWebDAVPassword);
    Password_delete(defaultFTPPassword);
  #endif /* defined(HAVE_CURL) || defined(HAVE_FTP) */
  #if   defined(HAVE_CURL)
    curl_global_cleanup();
  #elif defined(HAVE_FTP)
  #endif /* HAVE_CURL || HAVE_FTP */
}

void Storage_initSpecifier(StorageSpecifier *storageSpecifier)
{
  assert(storageSpecifier != NULL);

  storageSpecifier->hostName      = String_new();
  storageSpecifier->hostPort      = 0;
  storageSpecifier->loginName     = String_new();
  storageSpecifier->loginPassword = Password_new();
  storageSpecifier->deviceName    = String_new();

  DEBUG_ADD_RESOURCE_TRACE("storage specifier",storageSpecifier);
}

void Storage_doneSpecifier(StorageSpecifier *storageSpecifier)
{
  assert(storageSpecifier != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(storageSpecifier);

  String_delete(storageSpecifier->deviceName);
  Password_delete(storageSpecifier->loginPassword);
  String_delete(storageSpecifier->loginName);
  String_delete(storageSpecifier->hostName);
}

void Storage_duplicateSpecifier(StorageSpecifier       *destinationStorageSpecifier,
                                const StorageSpecifier *sourceStorageSpecifier
                               )
{
  assert(destinationStorageSpecifier != NULL);
  assert(sourceStorageSpecifier != NULL);

  destinationStorageSpecifier->type          = sourceStorageSpecifier->type;
//  destinationStorageSpecifier->string        = String_duplicate(sourceStorageSpecifier->string);
  destinationStorageSpecifier->hostName      = String_duplicate(sourceStorageSpecifier->hostName);
  destinationStorageSpecifier->hostPort      = sourceStorageSpecifier->hostPort;
  destinationStorageSpecifier->loginName     = String_duplicate(sourceStorageSpecifier->loginName);
  destinationStorageSpecifier->loginPassword = Password_duplicate(sourceStorageSpecifier->loginPassword);
  destinationStorageSpecifier->deviceName    = String_duplicate(sourceStorageSpecifier->deviceName);

  DEBUG_ADD_RESOURCE_TRACE("duplicated storage specifier",destinationStorageSpecifier);
}

bool Storage_parseFTPSpecifier(const String ftpSpecifier,
                               String       hostName,
                               uint         *hostPort,
                               String       loginName,
                               Password     *loginPassword
                              )
{
  const char* LOGINNAME_MAP_FROM[] = {"\\@"};
  const char* LOGINNAME_MAP_TO[]   = {"@"};

  bool   result;
  String s,t;

  assert(ftpSpecifier != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(loginName);
  if (loginPassword != NULL) Password_clear(loginPassword);

  s = String_new();
  t = String_new();
  if      (String_matchCString(ftpSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,NULL,loginName,s,STRING_NO_ASSIGN,hostName,t,NULL))
  {
    // <login name>:<login password>@<host name>:<host port>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (loginPassword != NULL) Password_setString(loginPassword,s);
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(t,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)$",NULL,NULL,loginName,s,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>:<login password>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (loginPassword != NULL) Password_setString(loginPassword,s);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^@/]*?)$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));

    result = TRUE;
  }
  else if (String_matchCString(ftpSpecifier,STRING_BEGIN,"^([^@:/]*?):([[:digit:]]+)$",NULL,NULL,hostName,s,NULL))
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

bool Storage_parseSSHSpecifier(const String sshSpecifier,
                               String       hostName,
                               uint         *hostPort,
                               String       loginName
                              )
{
  const char* LOGINNAME_MAP_FROM[] = {"\\@"};
  const char* LOGINNAME_MAP_TO[]   = {"@"};

  bool   result;
  String s;

  assert(sshSpecifier != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(loginName);

  s = String_new();
  if      (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^:]+?):(\\d*)/{0,1}$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    if (loginName != NULL) String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^/]+)/{0,1}$",NULL,NULL,loginName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    if (loginName != NULL) String_mapCString(loginName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM));

    result = TRUE;
  }
  else if (String_matchCString(sshSpecifier,STRING_BEGIN,"^([^@:/]*?):(\\d*)/{0,1}$",NULL,NULL,hostName,s,NULL))
  {
    // <host name>:<host port>
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (!String_isEmpty(sshSpecifier))
  {
    // <host name>
    String_set(hostName,sshSpecifier);

    result = TRUE;
  }
  else
  {
    result = FALSE;
  }
  String_delete(s);

  return result;
}

bool Storage_parseWebDAVSpecifier(const String webdavSpecifier,
                                  String       hostName,
                                  String       loginName,
                                  Password     *loginPassword
                                 )
{
  const char* LOGINNAME_MAP_FROM[] = {"\\@"};
  const char* LOGINNAME_MAP_TO[]   = {"@"};

  bool   result;
  String s;

  assert(webdavSpecifier != NULL);

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

bool Storage_parseDeviceSpecifier(const String deviceSpecifier,
                                  const String defaultDeviceName,
                                  String       deviceName
                                 )
{
  bool result;

  assert(deviceSpecifier != NULL);

  String_clear(deviceName);

  if (String_matchCString(deviceSpecifier,STRING_BEGIN,"^([^:]*):$",NULL,NULL,deviceName,NULL))
  {
    // <device name>

    result = TRUE;
  }
  else
  {
    if (deviceName != NULL) String_set(deviceName,defaultDeviceName);

    result = TRUE;
  }

  return result;
}

StorageTypes Storage_getType(const String storageName)
{
  StorageSpecifier storageSpecifier;
  StorageTypes     type;

  Storage_initSpecifier(&storageSpecifier);
  if (Storage_parseName(storageName,&storageSpecifier,NULL) == ERROR_NONE)
  {
    type = storageSpecifier.type;
  }
  else
  {
    type = STORAGE_TYPE_UNKNOWN;
  }
  Storage_doneSpecifier(&storageSpecifier);

  return type;
}

Errors Storage_parseName(const String     storageName,
                         StorageSpecifier *storageSpecifier,
                         String           fileName
                        )
{
  String string;
  long   nextIndex;

  assert(storageSpecifier != NULL);

  // initialise variables
  string = String_new();

//  String_clear(storageSpecifier->string);
  String_clear(storageSpecifier->hostName);
  storageSpecifier->hostPort = 0;
  String_clear(storageSpecifier->loginName);
  Password_clear(storageSpecifier->loginPassword);
  String_clear(storageSpecifier->deviceName);

  if      (String_startsWithCString(storageName,"ftp://"))
  {
#warning ftp port?
    if (   String_matchCString(storageName,6,"^[^:]+:([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)  // ftp://<login name>:<login password>@<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)        // ftp://<login name>@<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                     // ftp://<host name>[:<host port>]/<file name>
       )
    {
      String_sub(string,storageName,6,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseFTPSpecifier(string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName,
                                     storageSpecifier->loginPassword
                                    )
         )
      {
        return ERROR_INVALID_FTP_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      // ftp://<file name>
      if (fileName != NULL) String_sub(fileName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_FTP;
  }
  else if (String_startsWithCString(storageName,"ssh://"))
  {
    if (   String_matchCString(storageName,6,"^([^@]|\\@)+?@[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)   // ssh://<login name>@<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)                // ssh://<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                     // ssh://<host name>/<file name>
       )
    {
      String_sub(string,storageName,6,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseSSHSpecifier(string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName
                                    )
         )
      {
        return ERROR_INVALID_SSH_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      // ssh://<file name>
      if (fileName != NULL) String_sub(fileName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_SSH;
  }
  else if (String_startsWithCString(storageName,"scp://"))
  {
    if (   String_matchCString(storageName,6,"^([^@]|\\@)+?@[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)   // scp://<login name>@<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)                // scp://<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,6,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                     // scp://<host name>/<file name>
       )
    {
      String_sub(string,storageName,6,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseSSHSpecifier(string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName
                                    )
         )
      {
        return ERROR_INVALID_SSH_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      // scp://<file name>
      if (fileName != NULL) String_sub(fileName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_SCP;
  }
  else if (String_startsWithCString(storageName,"sftp://"))
  {
    if (   String_matchCString(storageName,7,"^([^@]|\\@)+?@[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)   // sftp://<login name>@<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,7,"^[^:]+:\\d*/{0,1}",&nextIndex,NULL,NULL)                // sftp://<host name>:[<host port>]/<file name>
        || String_matchCString(storageName,7,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                     // sftp://<host name>/<file name>
       )
    {
      String_sub(string,storageName,7,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseSSHSpecifier(string,
                                     storageSpecifier->hostName,
                                     &storageSpecifier->hostPort,
                                     storageSpecifier->loginName
                                    )
         )
      {
        return ERROR_INVALID_SSH_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,7+nextIndex,STRING_END);
    }
    else
    {
      // sftp://<file name>
      if (fileName != NULL) String_sub(fileName,storageName,7,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_SFTP;
  }
  else if (String_startsWithCString(storageName,"webdav://"))
  {
    if (   String_matchCString(storageName,9,"^[^:]+:([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)  // webdav://<login name>:<login password>@<host name>/<file name>
        || String_matchCString(storageName,9,"^([^@]|\\@)+?@[^/]+/{0,1}",&nextIndex,NULL,NULL)        // webdav://<login name>@<host name>/<file name>
        || String_matchCString(storageName,9,"^[^/]+/{0,1}",&nextIndex,NULL,NULL)                     // webdav://<host name>/<file name>
       )
    {
      String_sub(string,storageName,9,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseWebDAVSpecifier(string,
                                        storageSpecifier->hostName,
                                        storageSpecifier->loginName,
                                        storageSpecifier->loginPassword
                                       )
         )
      {
        return ERROR_INVALID_FTP_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,9+nextIndex,STRING_END);
    }
    else
    {
      // webdav://<file name>
      if (fileName != NULL) String_sub(fileName,storageName,9,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_WEBDAV;
  }
  else if (String_startsWithCString(storageName,"cd://"))
  {
    if (String_matchCString(storageName,5,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL))
    {
      // cd://<device name>:<file name>
      String_sub(string,storageName,5,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseDeviceSpecifier(string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,5+nextIndex,STRING_END);
    }
    else
    {
      // cd://<file name>
      if (fileName != NULL) String_sub(fileName,storageName,5,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_CD;
  }
  else if (String_startsWithCString(storageName,"dvd://"))
  {
    if (String_matchCString(storageName,6,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL))
    {
      // dvd://<device name>:<file name>
      String_sub(string,storageName,6,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseDeviceSpecifier(string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,6+nextIndex,STRING_END);
    }
    else
    {
      // dvd://<file name>
      if (fileName != NULL) String_sub(fileName,storageName,6,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_DVD;
  }
  else if (String_startsWithCString(storageName,"bd://"))
  {
    if (String_matchCString(storageName,5,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL))
    {
      // bd://<device name>:<file name>
      String_sub(string,storageName,5,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseDeviceSpecifier(string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,5+nextIndex,STRING_END);
    }
    else
    {
      // bd://<file name>
      if (fileName != NULL) String_sub(fileName,storageName,5,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_BD;
  }
  else if (String_startsWithCString(storageName,"device://"))
  {
    if (String_matchCString(storageName,9,"^[^:]+:[^/]*/{0,1}",&nextIndex,NULL,NULL))
    {
      // device://<device name>:<file name>
      String_sub(string,storageName,9,nextIndex);
      String_trimRight(string,"/");
      if (!Storage_parseDeviceSpecifier(string,
                                        NULL,
                                        storageSpecifier->deviceName
                                       )
         )
      {
        return ERROR_INVALID_DEVICE_SPECIFIER;
      }
      if (fileName != NULL) String_sub(fileName,storageName,9+nextIndex,STRING_END);
    }
    else
    {
      // device://<file name>
      if (fileName != NULL) String_sub(fileName,storageName,9,STRING_END);
    }

    storageSpecifier->type = STORAGE_TYPE_DEVICE;
  }
  else if (String_startsWithCString(storageName,"file://"))
  {
    if (fileName != NULL) String_sub(fileName,storageName,7,STRING_END);

    storageSpecifier->type = STORAGE_TYPE_FILESYSTEM;
  }
  else
  {
    if (fileName != NULL) String_set(fileName,storageName);

    storageSpecifier->type = STORAGE_TYPE_FILESYSTEM;
  }

  // free resources
  String_delete(string);

  return ERROR_NONE;
}

bool Storage_equalNames(const String storageName1,
                        const String storageName2
                       )
{
  StorageSpecifier storageSpecifier1,storageSpecifier2;
  String           fileName1,fileName2;
  bool             result;

  // parse storage names
  Storage_initSpecifier(&storageSpecifier1);
  Storage_initSpecifier(&storageSpecifier2);
  fileName1         = String_new();
  fileName2         = String_new();
  if (   (Storage_parseName(storageName1,&storageSpecifier1,fileName1) != ERROR_NONE)
      || (Storage_parseName(storageName2,&storageSpecifier2,fileName2) != ERROR_NONE)
     )
  {
    String_delete(fileName2);
    String_delete(fileName1);
    Storage_doneSpecifier(&storageSpecifier2);
    Storage_doneSpecifier(&storageSpecifier1);
    return FALSE;
  }

  // compare
  result = FALSE;
  if (storageSpecifier1.type == storageSpecifier2.type)
  {
    switch (storageSpecifier1.type)
    {
      case STORAGE_TYPE_FILESYSTEM:
        result = String_equals(fileName1,fileName2);
        break;
      case STORAGE_TYPE_FTP:
      case STORAGE_TYPE_SSH:
      case STORAGE_TYPE_SCP:
      case STORAGE_TYPE_SFTP:
      case STORAGE_TYPE_WEBDAV:
        result =    String_equals(storageSpecifier1.hostName,storageSpecifier2.hostName)
                 && String_equals(storageSpecifier1.loginName,storageSpecifier2.loginName)
                 && String_equals(fileName1,fileName2);
        break;
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
      case STORAGE_TYPE_DEVICE:
        result =    String_equals(storageSpecifier1.deviceName,storageSpecifier2.deviceName)
                 && String_equals(fileName1,fileName2);
        result = String_equals(fileName1,fileName2);
        break;
      default:
        break;
    }
  }

  // free resources
  String_delete(fileName2);
  String_delete(fileName1);
  Storage_doneSpecifier(&storageSpecifier2);
  Storage_doneSpecifier(&storageSpecifier1);

  return result;
}

String Storage_getName(String                 storageName,
                       const StorageSpecifier *storageSpecifier,
                       const String           fileName
                      )
{
  const char *plainLoginPassword;

  assert(storageName != NULL);
  assert(storageSpecifier != NULL);

  String_clear(storageName);
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!String_isEmpty(fileName))
      {
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_FTP:
      String_appendCString(storageName,"ftp://");
      if (!String_isEmpty(storageSpecifier->loginName))
      {
        String_append(storageName,storageSpecifier->loginName);
        if (!Password_isEmpty(storageSpecifier->loginPassword))
        {
          String_appendChar(storageName,':');
          plainLoginPassword = Password_deploy(storageSpecifier->loginPassword);
          String_appendCString(storageName,plainLoginPassword);
          Password_undeploy(storageSpecifier->loginPassword);
        }
        String_appendChar(storageName,'@');
      }
      String_append(storageName,storageSpecifier->hostName);
      if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 21))
      {
        String_format(storageName,":%d",storageSpecifier->hostPort);
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_SSH:
      if (!String_isEmpty(fileName))
      {
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_SCP:
      String_appendCString(storageName,"scp://");
      if (!String_isEmpty(storageSpecifier->loginName))
      {
        String_append(storageName,storageSpecifier->loginName);
        if (!Password_isEmpty(storageSpecifier->loginPassword))
        {
          String_appendChar(storageName,':');
          plainLoginPassword = Password_deploy(storageSpecifier->loginPassword);
          String_appendCString(storageName,plainLoginPassword);
          Password_undeploy(storageSpecifier->loginPassword);
        }
        String_appendChar(storageName,'@');
      }
      String_append(storageName,storageSpecifier->hostName);
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_SFTP:
      String_appendCString(storageName,"sftp://");
      if (!String_isEmpty(storageSpecifier->loginName))
      {
        String_append(storageName,storageSpecifier->loginName);
        if (!Password_isEmpty(storageSpecifier->loginPassword))
        {
          String_appendChar(storageName,':');
          plainLoginPassword = Password_deploy(storageSpecifier->loginPassword);
          String_appendCString(storageName,plainLoginPassword);
          Password_undeploy(storageSpecifier->loginPassword);
        }
        String_appendChar(storageName,'@');
      }
      String_append(storageName,storageSpecifier->hostName);
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_WEBDAV:
      String_appendCString(storageName,"webdav://");
      if (!String_isEmpty(storageSpecifier->loginName))
      {
        String_append(storageName,storageSpecifier->loginName);
        if (!Password_isEmpty(storageSpecifier->loginPassword))
        {
          String_appendChar(storageName,':');
          plainLoginPassword = Password_deploy(storageSpecifier->loginPassword);
          String_appendCString(storageName,plainLoginPassword);
          Password_undeploy(storageSpecifier->loginPassword);
        }
        String_appendChar(storageName,'@');
      }
      String_append(storageName,storageSpecifier->hostName);
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_CD:
      String_appendCString(storageName,"cd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(storageName,storageSpecifier->deviceName);
        String_appendChar(storageName,':');
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_DVD:
      String_appendCString(storageName,"dvd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(storageName,storageSpecifier->deviceName);
        String_appendChar(storageName,':');
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_BD:
      String_appendCString(storageName,"bd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(storageName,storageSpecifier->deviceName);
        String_appendChar(storageName,':');
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_DEVICE:
      String_appendCString(storageName,"device://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(storageName,storageSpecifier->deviceName);
        String_appendChar(storageName,':');
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return storageName;
}

String Storage_getPrintableName(String                 storageName,
                                const StorageSpecifier *storageSpecifier,
                                const String           fileName
                               )
{
  assert(storageName != NULL);
  assert(storageSpecifier != NULL);

  String_clear(storageName);
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!String_isEmpty(fileName))
      {
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_FTP:
      String_appendCString(storageName,"ftp://");
      if (!String_isEmpty(storageSpecifier->loginName))
      {
        String_append(storageName,storageSpecifier->loginName);
        String_appendChar(storageName,'@');
      }
      String_append(storageName,storageSpecifier->hostName);
      if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 21))
      {
        String_format(storageName,":%d",storageSpecifier->hostPort);
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_SSH:
      if (!String_isEmpty(fileName))
      {
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_SCP:
      String_appendCString(storageName,"scp://");
      String_append(storageName,storageSpecifier->hostName);
      if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 22))
      {
        String_format(storageName,":%d",storageSpecifier->hostPort);
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_SFTP:
      String_appendCString(storageName,"sftp://");
      String_append(storageName,storageSpecifier->hostName);
      if ((storageSpecifier->hostPort != 0) && (storageSpecifier->hostPort != 22))
      {
        String_format(storageName,":%d",storageSpecifier->hostPort);
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_WEBDAV:
      String_appendCString(storageName,"webdav://");
      if (!String_isEmpty(storageSpecifier->loginName))
      {
        String_append(storageName,storageSpecifier->loginName);
        String_appendChar(storageName,'@');
      }
      String_append(storageName,storageSpecifier->hostName);
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_CD:
      String_appendCString(storageName,"cd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(storageName,storageSpecifier->deviceName);
        String_appendChar(storageName,':');
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_DVD:
      String_appendCString(storageName,"dvd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(storageName,storageSpecifier->deviceName);
        String_appendChar(storageName,':');
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_BD:
      String_appendCString(storageName,"bd://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(storageName,storageSpecifier->deviceName);
        String_appendChar(storageName,':');
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    case STORAGE_TYPE_DEVICE:
      String_appendCString(storageName,"device://");
      if (!String_isEmpty(storageSpecifier->deviceName))
      {
        String_append(storageName,storageSpecifier->deviceName);
        String_appendChar(storageName,':');
      }
      if (!String_isEmpty(fileName))
      {
        String_appendChar(storageName,'/');
        String_append(storageName,fileName);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return storageName;
}

Errors Storage_init(StorageFileHandle            *storageFileHandle,
                    const StorageSpecifier       *storageSpecifier,
                    const String                 storageFileName,
                    const JobOptions             *jobOptions,
                    BandWidthList                *maxBandWidthList,
                    StorageRequestVolumeFunction storageRequestVolumeFunction,
                    void                         *storageRequestVolumeUserData,
                    StorageStatusInfoFunction    storageStatusInfoFunction,
                    void                         *storageStatusInfoUserData
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageFileName != NULL);
  assert(jobOptions != NULL);

  // initialize variables
  storageFileHandle->mode                      = STORAGE_MODE_UNKNOWN;
  Storage_duplicateSpecifier(&storageFileHandle->storageSpecifier,storageSpecifier);
  storageFileHandle->jobOptions                = jobOptions;
  storageFileHandle->requestVolumeFunction     = storageRequestVolumeFunction;
  storageFileHandle->requestVolumeUserData     = storageRequestVolumeUserData;
  if (jobOptions->waitFirstVolumeFlag)
  {
    storageFileHandle->volumeNumber            = 0;
    storageFileHandle->requestedVolumeNumber   = 0;
    storageFileHandle->volumeState             = STORAGE_VOLUME_STATE_UNKNOWN;
  }
  else
  {
    storageFileHandle->volumeNumber            = 1;
    storageFileHandle->requestedVolumeNumber   = 1;
    storageFileHandle->volumeState             = STORAGE_VOLUME_STATE_LOADED;
  }
  storageFileHandle->storageStatusInfoFunction = storageStatusInfoFunction;
  storageFileHandle->storageStatusInfoUserData = storageStatusInfoUserData;

  // check parameters
  if (String_isEmpty(storageFileName))
  {
    Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
    return ERROR_NO_ARCHIVE_FILE_NAME;
  }

  // init prootocol specific values
  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      UNUSED_VARIABLE(maxBandWidthList);

      break;
    case STORAGE_TYPE_FILESYSTEM:
      UNUSED_VARIABLE(maxBandWidthList);

      // init variables
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL)
        {
          FTPServer ftpServer;

          // init variables
          storageFileHandle->ftp.curlMultiHandle        = NULL;
          storageFileHandle->ftp.curlHandle             = NULL;
          storageFileHandle->ftp.index                  = 0LL;
          storageFileHandle->ftp.size                   = 0LL;
          storageFileHandle->ftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->ftp.readAheadBuffer.length = 0L;
          storageFileHandle->ftp.buffer                 = NULL;
          storageFileHandle->ftp.length                 = 0L;
          storageFileHandle->ftp.transferedBytes        = 0L;
          initBandWidthLimiter(&storageFileHandle->ftp.bandWidthLimiter,maxBandWidthList);

          // allocate read-ahead buffer
          storageFileHandle->ftp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
          if (storageFileHandle->ftp.readAheadBuffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          // get FTP server settings
          storageFileHandle->ftp.serverAllocation = getFTPServerSettings(storageFileHandle->storageSpecifier.hostName,jobOptions,&ftpServer);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_set(storageFileHandle->storageSpecifier.loginName,ftpServer.loginName);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_setCString(storageFileHandle->storageSpecifier.loginName,getenv("LOGNAME"));
          if (String_isEmpty(storageFileHandle->storageSpecifier.hostName))
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return ERROR_NO_HOST_NAME;
          }

          // allocate FTP server connection
          if (!allocateServer(storageFileHandle->ftp.serverAllocation,SERVER_ALLOCATION_PRIORITY_HIGH,ftpServer.maxConnectionCount))
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return ERROR_TOO_MANY_CONNECTIONS;
          }

          // check FTP login, get correct password
          error = ERROR_FTP_SESSION_FAIL;
          if ((error != ERROR_NONE) && !Password_isEmpty(storageFileHandle->storageSpecifier.loginPassword))
          {
            error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  storageFileHandle->storageSpecifier.loginPassword
                                 );
          }
          if ((error != ERROR_NONE) && !Password_isEmpty(ftpServer.password))
          {
            error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  ftpServer.password
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,ftpServer.password);
            }
          }
          if ((error != ERROR_NONE) && !Password_isEmpty(defaultFTPPassword))
          {
            error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  defaultFTPPassword
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultFTPPassword);
            }
          }
          if (error != ERROR_NONE)
          {
            // initialize default password
            if (initFTPPassword(storageFileHandle->storageSpecifier.hostName,storageFileHandle->storageSpecifier.loginName,jobOptions))
            {
              error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                    storageFileHandle->storageSpecifier.hostPort,
                                    storageFileHandle->storageSpecifier.loginName,
                                    defaultFTPPassword
                                   );
              if (error == ERROR_NONE)
              {
                Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultFTPPassword);
              }
            }
            else
            {
              error = !Password_isEmpty(defaultFTPPassword) ? ERROR_INVALID_FTP_PASSWORD : ERROR_NO_FTP_PASSWORD;
            }
          }
          if (error != ERROR_NONE)
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return error;
          }
        }
      #elif defined(HAVE_FTP)
        {
          FTPServer ftpServer;

          // init variables
          storageFileHandle->ftp.control                = NULL;
          storageFileHandle->ftp.data                   = NULL;
          storageFileHandle->ftp.index                  = 0LL;
          storageFileHandle->ftp.size                   = 0LL;
          storageFileHandle->ftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->ftp.readAheadBuffer.length = 0L;
          initBandWidthLimiter(&storageFileHandle->ftp.bandWidthLimiter,maxBandWidthList);

          // allocate read-ahead buffer
          storageFileHandle->ftp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
          if (storageFileHandle->ftp.readAheadBuffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          // get FTP server settings
          getFTPServerSettings(storageFileHandle->storageSpecifier.hostName,jobOptions,&ftpServer);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_set(storageFileHandle->storageSpecifier.loginName,ftpServer.loginName);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_setCString(storageFileHandle->storageSpecifier.loginName,getenv("LOGNAME"));
          if (String_isEmpty(storageFileHandle->storageSpecifier.hostName))
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return ERROR_NO_HOST_NAME;
          }

          // allocate FTP server connection
          if (!allocateFTPServerConnection(storageFileHandle->storageSpecifier.hostName))
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return ERROR_TOO_MANY_CONNECTIONS;
          }

          // check FTP login, get correct password
          error = ERROR_FTP_SESSION_FAIL;
          if ((error != ERROR_NONE) && !Password_isEmpty(storageFileHandle->storageSpecifier.loginPassword))
          {
            error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  storageFileHandle->storageSpecifier.loginPassword
                                 );
          }
          if ((error != ERROR_NONE) && !Password_isEmpty(ftpServer.password))
          {
            error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  ftpServer.password
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,ftpServer.password);
            }
          }
          if ((error != ERROR_NONE) && !Password_isEmpty(defaultFTPPassword))
          {
            error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  defaultFTPPassword
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultFTPPassword);
            }
          }
          if (error != ERROR_NONE)
          {
            // initialize default password
            if (initFTPPassword(storageFileHandle->storageSpecifier.hostName,storageFileHandle->storageSpecifier.loginName,jobOptions))
            {
              error = checkFTPLogin(storageFileHandle->storageSpecifier.hostName,
                                    storageFileHandle->storageSpecifier.hostPort,
                                    storageFileHandle->storageSpecifier.loginName,
                                    defaultFTPPassword
                                   );
              if (error == ERROR_NONE)
              {
                Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultFTPPassword);
              }
            }
            else
            {
              error = !Password_isEmpty(defaultFTPPassword) ? ERROR_INVALID_FTP_PASSWORD : ERROR_NO_FTP_PASSWORD;
            }
          }
          if (error != ERROR_NONE)
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return error;
          }
        }
      #else /* not HAVE_CURL || HAVE_FTP */
        Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      UNUSED_VARIABLE(maxBandWidthList);

      Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          SSHServer sshServer;

          // init variables
          storageFileHandle->scp.sshPublicKeyFileName   = NULL;
          storageFileHandle->scp.sshPrivateKeyFileName  = NULL;
          storageFileHandle->scp.channel                = NULL;
          storageFileHandle->scp.oldSendCallback        = NULL;
          storageFileHandle->scp.oldReceiveCallback     = NULL;
          storageFileHandle->scp.totalSentBytes         = 0LL;
          storageFileHandle->scp.totalReceivedBytes     = 0LL;
          storageFileHandle->scp.readAheadBuffer.offset = 0LL;
          storageFileHandle->scp.readAheadBuffer.length = 0L;
          initBandWidthLimiter(&storageFileHandle->scp.bandWidthLimiter,maxBandWidthList);

          // allocate read-ahead buffer
          storageFileHandle->scp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
          if (storageFileHandle->scp.readAheadBuffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          // get SSH server settings
          storageFileHandle->scp.serverAllocation = getSSHServerSettings(storageFileHandle->storageSpecifier.hostName,jobOptions,&sshServer);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_set(storageFileHandle->storageSpecifier.loginName,sshServer.loginName);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_setCString(storageFileHandle->storageSpecifier.loginName,getenv("LOGNAME"));
          if (storageFileHandle->storageSpecifier.hostPort == 0) storageFileHandle->storageSpecifier.hostPort = sshServer.port;
          storageFileHandle->scp.sshPublicKeyFileName  = sshServer.publicKeyFileName;
          storageFileHandle->scp.sshPrivateKeyFileName = sshServer.privateKeyFileName;
          if (String_isEmpty(storageFileHandle->storageSpecifier.hostName))
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return ERROR_NO_HOST_NAME;
          }

          // allocate SSH server connection
          if (!allocateServer(storageFileHandle->scp.serverAllocation,SERVER_ALLOCATION_PRIORITY_HIGH,sshServer.maxConnectionCount))
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return ERROR_TOO_MANY_CONNECTIONS;
          }

          // check if SSH login is possible
          error = ERROR_UNKNOWN;
          if ((error == ERROR_UNKNOWN) && !Password_isEmpty(sshServer.password))
          {
            error = checkSSHLogin(storageFileHandle->storageSpecifier.loginName,
                                  sshServer.password,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->scp.sshPublicKeyFileName,
                                  storageFileHandle->scp.sshPrivateKeyFileName
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,sshServer.password);
            }
          }
          if (error == ERROR_UNKNOWN)
          {
            // initialize default password
            if (   initSSHPassword(storageFileHandle->storageSpecifier.hostName,storageFileHandle->storageSpecifier.loginName,jobOptions)
                && !Password_isEmpty(defaultSSHPassword)
               )
            {
              error = checkSSHLogin(storageFileHandle->storageSpecifier.loginName,
                                    defaultSSHPassword,
                                    storageFileHandle->storageSpecifier.hostName,
                                    storageFileHandle->storageSpecifier.hostPort,
                                    storageFileHandle->scp.sshPublicKeyFileName,
                                    storageFileHandle->scp.sshPrivateKeyFileName
                                   );
              if (error == ERROR_NONE)
              {
                Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultSSHPassword);
              }
            }
            else
            {
              error = !Password_isEmpty(defaultSSHPassword)
                        ? ERRORX_(INVALID_SSH_PASSWORD,0,String_cString(storageFileHandle->storageSpecifier.hostName))
                        : ERRORX_(NO_SSH_PASSWORD,0,String_cString(storageFileHandle->storageSpecifier.hostName));
            }
          }
          if (error != ERROR_NONE)
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return error;
          }

          // free resources
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(maxBandWidthList);

        Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          SSHServer sshServer;

          // init variables
          storageFileHandle->sftp.sshPublicKeyFileName   = NULL;
          storageFileHandle->sftp.sshPrivateKeyFileName  = NULL;
          storageFileHandle->sftp.oldSendCallback        = NULL;
          storageFileHandle->sftp.oldReceiveCallback     = NULL;
          storageFileHandle->sftp.totalSentBytes         = 0LL;
          storageFileHandle->sftp.totalReceivedBytes     = 0LL;
          storageFileHandle->sftp.sftp                   = NULL;
          storageFileHandle->sftp.sftpHandle             = NULL;
          storageFileHandle->sftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->sftp.readAheadBuffer.length = 0L;
          initBandWidthLimiter(&storageFileHandle->sftp.bandWidthLimiter,maxBandWidthList);

          // allocate read-ahead buffer
          storageFileHandle->sftp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
          if (storageFileHandle->sftp.readAheadBuffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          // get SSH server settings
          storageFileHandle->sftp.serverAllocation = getSSHServerSettings(storageFileHandle->storageSpecifier.hostName,jobOptions,&sshServer);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_set(storageFileHandle->storageSpecifier.loginName,sshServer.loginName);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_setCString(storageFileHandle->storageSpecifier.loginName,getenv("LOGNAME"));
          if (storageFileHandle->storageSpecifier.hostPort == 0) storageFileHandle->storageSpecifier.hostPort = sshServer.port;
          storageFileHandle->sftp.sshPublicKeyFileName  = sshServer.publicKeyFileName;
          storageFileHandle->sftp.sshPrivateKeyFileName = sshServer.privateKeyFileName;
          if (String_isEmpty(storageFileHandle->storageSpecifier.hostName))
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return ERROR_NO_HOST_NAME;
          }

          // allocate SSH server connection
          if (!allocateServer(storageFileHandle->sftp.serverAllocation,SERVER_ALLOCATION_PRIORITY_HIGH,sshServer.maxConnectionCount))
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return ERROR_TOO_MANY_CONNECTIONS;
          }

          // check if SSH login is possible
          error = ERROR_UNKNOWN;
          if ((error != ERROR_NONE) && (sshServer.password != NULL))
          {
            error = checkSSHLogin(storageFileHandle->storageSpecifier.loginName,
                                  sshServer.password,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->sftp.sshPublicKeyFileName,
                                  storageFileHandle->sftp.sshPrivateKeyFileName
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,sshServer.password);
            }
          }
          if (error != ERROR_NONE)
          {
            // initialize default password
            if (   initSSHPassword(storageFileHandle->storageSpecifier.hostName,storageFileHandle->storageSpecifier.loginName,jobOptions)
                && !Password_isEmpty(defaultSSHPassword)
               )
            {
              error = checkSSHLogin(storageFileHandle->storageSpecifier.loginName,
                                    defaultSSHPassword,
                                    storageFileHandle->storageSpecifier.hostName,
                                    storageFileHandle->storageSpecifier.hostPort,
                                    storageFileHandle->sftp.sshPublicKeyFileName,
                                    storageFileHandle->sftp.sshPrivateKeyFileName
                                   );
              if (error == ERROR_NONE)
              {
                Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultSSHPassword);
              }
            }
            else
            {
              error = !Password_isEmpty(defaultSSHPassword) ? ERROR_INVALID_SSH_PASSWORD : ERROR_NO_SSH_PASSWORD;
            }
          }
          if (error != ERROR_NONE)
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return error;
          }

          // free resources
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(maxBandWidthList);

        Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      #if   defined(HAVE_CURL)
        {
          WebDAVServer webdavServer;

          // init variables
          storageFileHandle->webdav.curlMultiHandle      = NULL;
          storageFileHandle->webdav.curlHandle           = NULL;
          storageFileHandle->webdav.index                = 0LL;
          storageFileHandle->webdav.size                 = 0LL;
          storageFileHandle->webdav.receiveBuffer.data   = NULL;
          storageFileHandle->webdav.receiveBuffer.size   = 0L;
          storageFileHandle->webdav.receiveBuffer.offset = 0LL;
          storageFileHandle->webdav.receiveBuffer.length = 0L;
          storageFileHandle->webdav.sendBuffer.data      = NULL;
          storageFileHandle->webdav.sendBuffer.index     = 0L;
          storageFileHandle->webdav.sendBuffer.length    = 0L;
          initBandWidthLimiter(&storageFileHandle->webdav.bandWidthLimiter,maxBandWidthList);

          // get WebDAV server settings
          storageFileHandle->webdav.serverAllocation = getWebDAVServerSettings(storageFileHandle->storageSpecifier.hostName,jobOptions,&webdavServer);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_set(storageFileHandle->storageSpecifier.loginName,webdavServer.loginName);
          if (String_isEmpty(storageFileHandle->storageSpecifier.loginName)) String_setCString(storageFileHandle->storageSpecifier.loginName,getenv("LOGNAME"));
          if (String_isEmpty(storageFileHandle->storageSpecifier.hostName))
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return ERROR_NO_HOST_NAME;
          }

          // allocate WebDAV server connection
          if (!allocateServer(storageFileHandle->webdav.serverAllocation,SERVER_ALLOCATION_PRIORITY_HIGH,webdavServer.maxConnectionCount))
          {
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return ERROR_TOO_MANY_CONNECTIONS;
          }

          // check WebDAV login, get correct password
          error = ERROR_WEBDAV_SESSION_FAIL;
          if ((error != ERROR_NONE) && !Password_isEmpty(storageFileHandle->storageSpecifier.loginPassword))
          {
            error = checkWebDAVLogin(storageFileHandle->storageSpecifier.hostName,
                                     storageFileHandle->storageSpecifier.loginName,
                                     storageFileHandle->storageSpecifier.loginPassword
                                    );
          }
          if ((error != ERROR_NONE) && !Password_isEmpty(webdavServer.password))
          {
            error = checkWebDAVLogin(storageFileHandle->storageSpecifier.hostName,
                                     storageFileHandle->storageSpecifier.loginName,
                                     webdavServer.password
                                    );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,webdavServer.password);
            }
          }
          if ((error != ERROR_NONE) && !Password_isEmpty(defaultWebDAVPassword))
          {
            error = checkWebDAVLogin(storageFileHandle->storageSpecifier.hostName,
                                     storageFileHandle->storageSpecifier.loginName,
                                     defaultWebDAVPassword
                                    );
            if (error == ERROR_NONE)
            {
              Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultWebDAVPassword);
            }
          }
          if (error != ERROR_NONE)
          {
            // initialize default password
            if (initWebDAVPassword(storageFileHandle->storageSpecifier.hostName,storageFileHandle->storageSpecifier.loginName,jobOptions))
            {
              error = checkWebDAVLogin(storageFileHandle->storageSpecifier.hostName,
                                       storageFileHandle->storageSpecifier.loginName,
                                       defaultWebDAVPassword
                                      );
              if (error == ERROR_NONE)
              {
                Password_set(storageFileHandle->storageSpecifier.loginPassword,defaultWebDAVPassword);
              }
            }
            else
            {
              error = !Password_isEmpty(defaultWebDAVPassword) ? ERROR_INVALID_WEBDAV_PASSWORD : ERROR_NO_WEBDAV_PASSWORD;
            }
          }
          if (error != ERROR_NONE)
          {
            freeServer(storageFileHandle->webdav.serverAllocation);
            Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
            return error;
          }
        }
      #else /* not HAVE_CURL */
        Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      {
        OpticalDisk    opticalDisk;
        uint64         volumeSize,maxMediumSize;
        FileSystemInfo fileSystemInfo;
        String         sourceFileName,fileBaseName,destinationFileName;

        UNUSED_VARIABLE(maxBandWidthList);

        // get device name
        if (String_isEmpty(storageFileHandle->storageSpecifier.deviceName))
        {
          switch (storageFileHandle->storageSpecifier.type)
          {
            case STORAGE_TYPE_CD:
              String_set(storageFileHandle->storageSpecifier.deviceName,globalOptions.cd.defaultDeviceName);
              break;
            case STORAGE_TYPE_DVD:
              String_set(storageFileHandle->storageSpecifier.deviceName,globalOptions.dvd.defaultDeviceName);
              break;
            case STORAGE_TYPE_BD:
              String_set(storageFileHandle->storageSpecifier.deviceName,globalOptions.bd.defaultDeviceName);
              break;
            default:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
               #endif /* NDEBUG */
              break;
          }
        }

        // get cd/dvd/bd settings
        switch (storageFileHandle->storageSpecifier.type)
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
          Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
          return error;
        }
        volumeSize    = 0LL;
        maxMediumSize = 0LL;
        switch (storageFileHandle->storageSpecifier.type)
        {
          case STORAGE_TYPE_CD:
            volumeSize = (jobOptions->volumeSize > 0LL)
                          ?jobOptions->volumeSize
                          :((globalOptions.cd.volumeSize > 0LL)
                            ?globalOptions.cd.volumeSize
                            :(jobOptions->errorCorrectionCodesFlag
                              ?CD_VOLUME_ECC_SIZE
                              :CD_VOLUME_SIZE
                             )
                           );
            maxMediumSize = MAX_CD_SIZE;
            break;
          case STORAGE_TYPE_DVD:
            volumeSize = (jobOptions->volumeSize > 0LL)
                          ?jobOptions->volumeSize
                          :((globalOptions.dvd.volumeSize > 0LL)
                            ?globalOptions.dvd.volumeSize
                            :(jobOptions->errorCorrectionCodesFlag
                              ?DVD_VOLUME_ECC_SIZE
                              :DVD_VOLUME_SIZE
                             )
                           );
            maxMediumSize = MAX_DVD_SIZE;
            break;
          case STORAGE_TYPE_BD:
            volumeSize = (jobOptions->volumeSize > 0LL)
                          ?jobOptions->volumeSize
                          :((globalOptions.bd.volumeSize > 0LL)
                            ?globalOptions.bd.volumeSize
                            :(jobOptions->errorCorrectionCodesFlag
                              ?BD_VOLUME_ECC_SIZE
                              :BD_VOLUME_SIZE
                            )
                           );
            maxMediumSize = MAX_BD_SIZE;
            break;
          default:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            #endif /* NDEBUG */
            break;
        }
        if (fileSystemInfo.freeBytes < (volumeSize+maxMediumSize*(jobOptions->errorCorrectionCodesFlag?2:1)))
        {
          printWarning("Insufficient space in temporary directory '%s' for medium (%.1f%s free, %.1f%s recommended)!\n",
                       String_cString(tmpDirectory),
                       BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                       BYTES_SHORT((volumeSize+maxMediumSize*(jobOptions->errorCorrectionCodesFlag?2:1))),BYTES_UNIT((volumeSize+maxMediumSize*(jobOptions->errorCorrectionCodesFlag?2:1)))
                      );
        }

        // init variables
        #ifdef HAVE_ISO9660
          storageFileHandle->opticalDisk.read.iso9660Handle     = NULL;
          storageFileHandle->opticalDisk.read.iso9660Stat       = NULL;
          storageFileHandle->opticalDisk.read.index             = 0LL;
          storageFileHandle->opticalDisk.read.buffer.blockIndex = 0LL;
          storageFileHandle->opticalDisk.read.buffer.length     = 0L;

          storageFileHandle->opticalDisk.read.buffer.data = (byte*)malloc(ISO_BLOCKSIZE);
          if (storageFileHandle->opticalDisk.read.buffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
        #endif /* HAVE_ISO9660 */

        storageFileHandle->opticalDisk.write.requestVolumeCommand   = opticalDisk.requestVolumeCommand;
        storageFileHandle->opticalDisk.write.unloadVolumeCommand    = opticalDisk.unloadVolumeCommand;
        storageFileHandle->opticalDisk.write.loadVolumeCommand      = opticalDisk.loadVolumeCommand;
        storageFileHandle->opticalDisk.write.volumeSize             = volumeSize;
        storageFileHandle->opticalDisk.write.imagePreProcessCommand = opticalDisk.imagePreProcessCommand;
        storageFileHandle->opticalDisk.write.imagePostProcessCommand= opticalDisk.imagePostProcessCommand;
        storageFileHandle->opticalDisk.write.imageCommand           = opticalDisk.imageCommand;
        storageFileHandle->opticalDisk.write.eccPreProcessCommand   = opticalDisk.eccPreProcessCommand;
        storageFileHandle->opticalDisk.write.eccPostProcessCommand  = opticalDisk.eccPostProcessCommand;
        storageFileHandle->opticalDisk.write.eccCommand             = opticalDisk.eccCommand;
        storageFileHandle->opticalDisk.write.writePreProcessCommand = opticalDisk.writePreProcessCommand;
        storageFileHandle->opticalDisk.write.writePostProcessCommand= opticalDisk.writePostProcessCommand;
        storageFileHandle->opticalDisk.write.writeCommand           = opticalDisk.writeCommand;
        storageFileHandle->opticalDisk.write.writeImageCommand      = opticalDisk.writeImageCommand;
        storageFileHandle->opticalDisk.write.steps                  = jobOptions->errorCorrectionCodesFlag?4:1;
        storageFileHandle->opticalDisk.write.directory              = String_new();
        storageFileHandle->opticalDisk.write.step                   = 0;
        if (jobOptions->waitFirstVolumeFlag)
        {
          storageFileHandle->opticalDisk.write.number        = 0;
          storageFileHandle->opticalDisk.write.newVolumeFlag = TRUE;
        }
        else
        {
          storageFileHandle->opticalDisk.write.number        = 1;
          storageFileHandle->opticalDisk.write.newVolumeFlag = FALSE;
        }
        StringList_init(&storageFileHandle->opticalDisk.write.fileNameList);
        storageFileHandle->opticalDisk.write.fileName               = String_new();
        storageFileHandle->opticalDisk.write.totalSize              = 0LL;

        // create temporary directory for medium files
        error = File_getTmpDirectoryName(storageFileHandle->opticalDisk.write.directory,NULL,tmpDirectory);
        if (error != ERROR_NONE)
        {
          #ifdef HAVE_ISO9660
            free(storageFileHandle->opticalDisk.read.buffer.data);
          #endif /* HAVE_ISO9660 */
          String_delete(storageFileHandle->opticalDisk.write.fileName);
          String_delete(storageFileHandle->opticalDisk.write.directory);
          Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
          return error;
        }

        if (!jobOptions->noBAROnMediumFlag)
        {
          // store a copy of BAR executable on medium (ignore errors)
          sourceFileName = String_newCString(globalOptions.barExecutable);
          fileBaseName = File_getFileBaseName(String_new(),sourceFileName);
          destinationFileName = File_appendFileName(String_duplicate(storageFileHandle->opticalDisk.write.directory),fileBaseName);
          File_copy(sourceFileName,destinationFileName);
          StringList_append(&storageFileHandle->opticalDisk.write.fileNameList,destinationFileName);
          String_delete(destinationFileName);
          String_delete(fileBaseName);
          String_delete(sourceFileName);
        }

        // request first medium
        storageFileHandle->requestedVolumeNumber = 1;
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        Device         device;
        FileSystemInfo fileSystemInfo;

        UNUSED_VARIABLE(maxBandWidthList);

        // get device settings
        getDeviceSettings(storageFileHandle->storageSpecifier.deviceName,jobOptions,&device);

        // check space in temporary directory: 2x volumeSize
        error = File_getFileSystemInfo(&fileSystemInfo,tmpDirectory);
        if (error != ERROR_NONE)
        {
          Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
          return error;
        }
        if (fileSystemInfo.freeBytes < (device.volumeSize*2))
        {
          printWarning("Insufficient space in temporary directory '%s' (%.1f%s free, %.1f%s recommended)!\n",
                       String_cString(tmpDirectory),
                       BYTES_SHORT(fileSystemInfo.freeBytes),BYTES_UNIT(fileSystemInfo.freeBytes),
                       BYTES_SHORT(device.volumeSize*2),BYTES_UNIT(device.volumeSize*2)
                      );
        }

        // init variables
        storageFileHandle->device.requestVolumeCommand   = device.requestVolumeCommand;
        storageFileHandle->device.unloadVolumeCommand    = device.unloadVolumeCommand;
        storageFileHandle->device.loadVolumeCommand      = device.loadVolumeCommand;
        storageFileHandle->device.volumeSize             = device.volumeSize;
        storageFileHandle->device.imagePreProcessCommand = device.imagePreProcessCommand;
        storageFileHandle->device.imagePostProcessCommand= device.imagePostProcessCommand;
        storageFileHandle->device.imageCommand           = device.imageCommand;
        storageFileHandle->device.eccPreProcessCommand   = device.eccPreProcessCommand;
        storageFileHandle->device.eccPostProcessCommand  = device.eccPostProcessCommand;
        storageFileHandle->device.eccCommand             = device.eccCommand;
        storageFileHandle->device.writePreProcessCommand = device.writePreProcessCommand;
        storageFileHandle->device.writePostProcessCommand= device.writePostProcessCommand;
        storageFileHandle->device.writeCommand           = device.writeCommand;
        storageFileHandle->device.directory              = String_new();
        if (jobOptions->waitFirstVolumeFlag)
        {
          storageFileHandle->device.number        = 0;
          storageFileHandle->device.newVolumeFlag = TRUE;
        }
        else
        {
          storageFileHandle->device.number        = 1;
          storageFileHandle->device.newVolumeFlag = FALSE;
        }
        StringList_init(&storageFileHandle->device.fileNameList);
        storageFileHandle->device.fileName               = String_new();
        storageFileHandle->device.totalSize              = 0LL;

        // create temporary directory for device files
        error = File_getTmpDirectoryName(storageFileHandle->device.directory,NULL,tmpDirectory);
        if (error != ERROR_NONE)
        {
          String_delete(storageFileHandle->device.fileName);
          String_delete(storageFileHandle->device.directory);
          Storage_doneSpecifier(&storageFileHandle->storageSpecifier);
          return error;
        }

        // request first volume for device
        storageFileHandle->requestedVolumeNumber = 1;
      }
      break;
    default:
      UNUSED_VARIABLE(maxBandWidthList);

      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  storageFileHandle->runningInfo.volumeNumber   = 0;
  storageFileHandle->runningInfo.volumeProgress = 0;

  DEBUG_ADD_RESOURCE_TRACE("storage file handle",storageFileHandle);

  return ERROR_NONE;
}

Errors Storage_done(StorageFileHandle *storageFileHandle)
{
  Errors error;

  assert(storageFileHandle != NULL);

  DEBUG_REMOVE_RESOURCE_TRACE(storageFileHandle);

  error = ERROR_NONE;

  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      break;
    case STORAGE_TYPE_FTP:
      // free FTP server connection
      freeServer(storageFileHandle->ftp.serverAllocation);
      #if   defined(HAVE_CURL)
        free(storageFileHandle->ftp.readAheadBuffer.data);
      #elif defined(HAVE_FTP)
        free(storageFileHandle->ftp.readAheadBuffer.data);
      #else /* not HAVE_CURL || HAVE_FTP */
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      // free SSH server connection
      freeServer(storageFileHandle->ssh.serverAllocation);
      break;
    case STORAGE_TYPE_SCP:
      // free SSH server connection
      freeServer(storageFileHandle->scp.serverAllocation);
      #ifdef HAVE_SSH2
        free(storageFileHandle->scp.readAheadBuffer.data);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      // free SSH server connection
      freeServer(storageFileHandle->sftp.serverAllocation);
      #ifdef HAVE_SSH2
        free(storageFileHandle->sftp.readAheadBuffer.data);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      // free WebDAV server connection
      freeServer(storageFileHandle->webdav.serverAllocation);
      #if   defined(HAVE_CURL)
      #else /* not HAVE_CURL || HAVE_FTP */
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      {
        String fileName;
        Errors tmpError;

        // delete files
        fileName = String_new();
        while (!StringList_isEmpty(&storageFileHandle->opticalDisk.write.fileNameList))
        {
          StringList_getFirst(&storageFileHandle->opticalDisk.write.fileNameList,fileName);
          tmpError = File_delete(fileName,FALSE);
          if (tmpError != ERROR_NONE)
          {
            if (error == ERROR_NONE) error = tmpError;
          }
        }
        String_delete(fileName);

        // delete temporare directory
        File_delete(storageFileHandle->opticalDisk.write.directory,FALSE);

        // free resources
        #ifdef HAVE_ISO9660
          free(storageFileHandle->opticalDisk.read.buffer.data);
        #endif /* HAVE_ISO9660 */
        String_delete(storageFileHandle->opticalDisk.write.fileName);
        String_delete(storageFileHandle->opticalDisk.write.directory);
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        String fileName;
        Errors tmpError;

        // delete files
        fileName = String_new();
        while (!StringList_isEmpty(&storageFileHandle->device.fileNameList))
        {
          StringList_getFirst(&storageFileHandle->device.fileNameList,fileName);
          tmpError = File_delete(fileName,FALSE);
          if (tmpError != ERROR_NONE)
          {
            if (error == ERROR_NONE) error = tmpError;
          }
        }
        String_delete(fileName);

        // delete temporare directory
        File_delete(storageFileHandle->device.directory,FALSE);

        // free resources
        String_delete(storageFileHandle->device.fileName);
        String_delete(storageFileHandle->device.directory);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
  Storage_doneSpecifier(&storageFileHandle->storageSpecifier);

  return error;
}

String Storage_getHandleName(String                  storageName,
                             const StorageFileHandle *storageFileHandle,
                             const String            fileName
                            )
{
  assert(storageName != NULL);
  assert(storageFileHandle != NULL);

  Storage_getName(storageName,&storageFileHandle->storageSpecifier,fileName);

  return storageName;
}

Errors Storage_preProcess(StorageFileHandle *storageFileHandle,
                          bool              initialFlag
                         )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  error = ERROR_NONE;
  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      {
        TextMacro textMacros[2];

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          if (!initialFlag)
          {
            // init macros
            TEXT_MACRO_N_STRING (textMacros[0],"%file",  storageFileHandle->fileSystem.fileHandle.name);
            TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageFileHandle->volumeNumber              );

            if (globalOptions.file.writePreProcessCommand != NULL)
            {
              // write pre-processing
              if (error == ERROR_NONE)
              {
                printInfo(0,"Write pre-processing...");
                error = Misc_executeCommand(String_cString(globalOptions.file.writePreProcessCommand),
                                            textMacros,
                                            SIZE_OF_ARRAY(textMacros),
                                            (ExecuteIOFunction)executeIOOutput,
                                            (ExecuteIOFunction)executeIOOutput,
                                            NULL
                                           );
                printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
              }
            }
            if (error != ERROR_NONE)
            {
              break;
            }
          }
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL) || defined(HAVE_FTP)
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!initialFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber              );

              if (globalOptions.ftp.writePreProcessCommand != NULL)
              {
                // write pre-processing
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write pre-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.ftp.writePreProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_CURL || HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!initialFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber              );

              if (globalOptions.scp.writePreProcessCommand != NULL)
              {
                // write pre-processing
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write pre-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.scp.writePreProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!initialFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber              );

              if (globalOptions.sftp.writePreProcessCommand != NULL)
              {
                // write pre-processing
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write pre-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.sftp.writePreProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      #ifdef HAVE_CURL
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!initialFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber              );

              if (globalOptions.ftp.writePreProcessCommand != NULL)
              {
                // write pre-processing
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write pre-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.webdav.writePreProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_CURL */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        // request next medium
        if (storageFileHandle->opticalDisk.write.newVolumeFlag)
        {
          storageFileHandle->opticalDisk.write.number++;
          storageFileHandle->opticalDisk.write.newVolumeFlag = FALSE;

          storageFileHandle->requestedVolumeNumber = storageFileHandle->opticalDisk.write.number;
        }

        // check if new medium is required
        if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
        {
          // request load new medium
          error = requestNewMedium(storageFileHandle,FALSE);
        }
      }
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        // request next volume
        if (storageFileHandle->device.newVolumeFlag)
        {
          storageFileHandle->device.number++;
          storageFileHandle->device.newVolumeFlag = FALSE;

          storageFileHandle->requestedVolumeNumber = storageFileHandle->device.number;
        }

        // check if new volume is required
        if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
        {
          error = requestNewVolume(storageFileHandle,FALSE);
        }
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_postProcess(StorageFileHandle *storageFileHandle,
                           bool              finalFlag
                          )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  error = ERROR_NONE;
  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      {
        TextMacro textMacros[2];

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          if (!finalFlag)
          {
            // init macros
            TEXT_MACRO_N_STRING (textMacros[0],"%file",  storageFileHandle->fileSystem.fileHandle.name);
            TEXT_MACRO_N_INTEGER(textMacros[1],"%number",storageFileHandle->volumeNumber              );

            if (globalOptions.file.writePostProcessCommand != NULL)
            {
              // write post-process
              if (error == ERROR_NONE)
              {
                printInfo(0,"Write post-processing...");
                error = Misc_executeCommand(String_cString(globalOptions.file.writePostProcessCommand),
                                            textMacros,
                                            SIZE_OF_ARRAY(textMacros),
                                            (ExecuteIOFunction)executeIOOutput,
                                            (ExecuteIOFunction)executeIOOutput,
                                            NULL
                                           );
                printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
              }
            }
            if (error != ERROR_NONE)
            {
              break;
            }
          }
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL) || defined(HAVE_FTP)
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!finalFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber);

              if (globalOptions.ftp.writePostProcessCommand != NULL)
              {
                // write post-process
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write post-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.ftp.writePostProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_CURL || HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!finalFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber);

              if (globalOptions.scp.writePostProcessCommand != NULL)
              {
                // write post-process
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write post-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.scp.writePostProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!finalFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber);

              if (globalOptions.sftp.writePostProcessCommand != NULL)
              {
                // write post-process
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write post-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.sftp.writePostProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      #ifdef HAVE_CURL
        {
          TextMacro textMacros[1];

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            if (!finalFlag)
            {
              // init macros
              TEXT_MACRO_N_INTEGER(textMacros[0],"%number",storageFileHandle->volumeNumber);

              if (globalOptions.ftp.writePostProcessCommand != NULL)
              {
                // write post-process
                if (error == ERROR_NONE)
                {
                  printInfo(0,"Write post-processing...");
                  error = Misc_executeCommand(String_cString(globalOptions.webdav.writePostProcessCommand),
                                              textMacros,
                                              SIZE_OF_ARRAY(textMacros),
                                              (ExecuteIOFunction)executeIOOutput,
                                              (ExecuteIOFunction)executeIOOutput,
                                              NULL
                                             );
                  printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
                }
              }
              if (error != ERROR_NONE)
              {
                break;
              }
            }
          }
        }
      #else /* not HAVE_CURL */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      {
        String    imageFileName;
        TextMacro textMacros[5];
        String    fileName;
        FileInfo  fileInfo;
        bool      retryFlag;

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          if (   (storageFileHandle->opticalDisk.write.totalSize > storageFileHandle->opticalDisk.write.volumeSize)
              || (finalFlag && (storageFileHandle->opticalDisk.write.totalSize > 0LL))
             )
          {
            // medium size limit reached or final medium -> create medium and request new volume

            // update info
            storageFileHandle->runningInfo.volumeProgress = 0.0;
            updateStatusInfo(storageFileHandle);

            // get temporary image file name
            imageFileName = String_new();
            error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
            if (error != ERROR_NONE)
            {
              break;
            }

            // init macros
            TEXT_MACRO_N_STRING (textMacros[0],"%device",   storageFileHandle->storageSpecifier.deviceName);
            TEXT_MACRO_N_STRING (textMacros[1],"%directory",storageFileHandle->opticalDisk.write.directory);
            TEXT_MACRO_N_STRING (textMacros[2],"%image",    imageFileName                                 );
            TEXT_MACRO_N_INTEGER(textMacros[3],"%sectors",  0                                             );
            TEXT_MACRO_N_INTEGER(textMacros[4],"%number",   storageFileHandle->volumeNumber               );

            if (storageFileHandle->jobOptions->alwaysCreateImageFlag || storageFileHandle->jobOptions->errorCorrectionCodesFlag)
            {
              // create medium image
              printInfo(0,"Make medium image #%d with %d part(s)...",storageFileHandle->opticalDisk.write.number,StringList_count(&storageFileHandle->opticalDisk.write.fileNameList));
              storageFileHandle->opticalDisk.write.step = 0;
              error = Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.imageCommand),
                                          textMacros,SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOmkisofs,
                                          (ExecuteIOFunction)executeIOmkisofs,
                                          storageFileHandle
                                         );
              if (error != ERROR_NONE)
              {
                printInfo(0,"FAIL\n");
                File_delete(imageFileName,FALSE);
                String_delete(imageFileName);
                break;
              }
              File_getFileInfo(&fileInfo,imageFileName);
              printInfo(0,"ok (%llu bytes)\n",fileInfo.size);

              if (storageFileHandle->jobOptions->errorCorrectionCodesFlag)
              {
                // add error-correction codes to medium image
                printInfo(0,"Add ECC to image #%d...",storageFileHandle->opticalDisk.write.number);
                storageFileHandle->opticalDisk.write.step = 1;
                error = Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.eccCommand),
                                            textMacros,SIZE_OF_ARRAY(textMacros),
                                            (ExecuteIOFunction)executeIOdvdisaster,
                                            (ExecuteIOFunction)executeIOdvdisaster,
                                            storageFileHandle
                                           );
                if (error != ERROR_NONE)
                {
                  printInfo(0,"FAIL\n");
                  File_delete(imageFileName,FALSE);
                  String_delete(imageFileName);
                  break;
                }
                File_getFileInfo(&fileInfo,imageFileName);
                printInfo(0,"ok (%llu bytes)\n",fileInfo.size);
              }

              // get number of image sectors
              if (File_getFileInfo(&fileInfo,imageFileName) == ERROR_NONE)
              {
                TEXT_MACRO_N_INTEGER(textMacros[3],"%sectors",(ulong)(fileInfo.size/2048LL));
              }

              // check if new medium is required
              if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
              {
                // request load new medium
                error = requestNewMedium(storageFileHandle,TRUE);
                if (error != ERROR_NONE)
                {
                  File_delete(imageFileName,FALSE);
                  String_delete(imageFileName);
                  break;
                }
                updateStatusInfo(storageFileHandle);
              }

              retryFlag = TRUE;
              do
              {
                retryFlag = FALSE;

                // write image to medium
                printInfo(0,"Write image to medium #%d...",storageFileHandle->opticalDisk.write.number);
                storageFileHandle->opticalDisk.write.step = 3;
                error = Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.writeImageCommand),
                                            textMacros,SIZE_OF_ARRAY(textMacros),
                                            (ExecuteIOFunction)executeIOgrowisofs,
                                            (ExecuteIOFunction)executeIOgrowisofs,
                                            storageFileHandle
                                           );
                if (error == ERROR_NONE)
                {
                  printInfo(0,"ok\n");
                  retryFlag = FALSE;
                }
                else
                {
                  printInfo(0,"FAIL\n");
                  retryFlag = Misc_getYesNo("Retry write image to medium?");
                }
              }
              while ((error != ERROR_NONE) && retryFlag);
              if (error != ERROR_NONE)
              {
                File_delete(imageFileName,FALSE);
                String_delete(imageFileName);
                break;
              }
            }
            else
            {
              // check if new medium is required
              if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
              {
                // request load new medium
                error = requestNewMedium(storageFileHandle,TRUE);
                if (error != ERROR_NONE)
                {
                  File_delete(imageFileName,FALSE);
                  String_delete(imageFileName);
                  break;
                }
                updateStatusInfo(storageFileHandle);
              }

              retryFlag = TRUE;
              do
              {
                retryFlag = FALSE;

                // write to medium
                printInfo(0,"Write medium #%d with %d part(s)...",storageFileHandle->opticalDisk.write.number,StringList_count(&storageFileHandle->opticalDisk.write.fileNameList));
                storageFileHandle->opticalDisk.write.step = 0;
                error = Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.writeCommand),
                                            textMacros,SIZE_OF_ARRAY(textMacros),
                                            (ExecuteIOFunction)executeIOgrowisofs,
                                            (ExecuteIOFunction)executeIOOutput,
                                            storageFileHandle
                                           );
                if (error == ERROR_NONE)
                {
                  printInfo(0,"ok\n");
                }
                else
                {
                  printInfo(0,"FAIL (error: %s)\n",Errors_getText(error));
                  retryFlag = Misc_getYesNo("Retry write image to medium?");
                }
              }
              while ((error != ERROR_NONE) && retryFlag);
              if (error != ERROR_NONE)
              {
                File_delete(imageFileName,FALSE);
                String_delete(imageFileName);
                break;
              }
            }

            // delete image
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);

            // update info
            storageFileHandle->runningInfo.volumeProgress = 1.0;
            updateStatusInfo(storageFileHandle);

            // delete stored files
            fileName = String_new();
            while (!StringList_isEmpty(&storageFileHandle->opticalDisk.write.fileNameList))
            {
              StringList_getFirst(&storageFileHandle->opticalDisk.write.fileNameList,fileName);
              error = File_delete(fileName,FALSE);
              if (error != ERROR_NONE)
              {
                break;
              }
            }
            String_delete(fileName);
            if (error != ERROR_NONE)
            {
              break;
            }

            // reset
            storageFileHandle->opticalDisk.write.newVolumeFlag = TRUE;
            storageFileHandle->opticalDisk.write.totalSize     = 0;
          }
        }
        else
        {
          // update info
          storageFileHandle->opticalDisk.write.step     = 3;
          storageFileHandle->runningInfo.volumeProgress = 1.0;
          updateStatusInfo(storageFileHandle);
        }
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        String    imageFileName;
        TextMacro textMacros[4];
        String    fileName;

        if (storageFileHandle->device.volumeSize == 0LL)
        {
          printWarning("Device volume size is 0 bytes!\n");
        }

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          if (   (storageFileHandle->device.totalSize > storageFileHandle->device.volumeSize)
              || (finalFlag && storageFileHandle->device.totalSize > 0LL)
             )
          {
            // device size limit reached -> write to device volume and request new volume

            // check if new volume is required
            if (storageFileHandle->volumeNumber != storageFileHandle->requestedVolumeNumber)
            {
              error = requestNewVolume(storageFileHandle,TRUE);
              if (error != ERROR_NONE)
              {
                break;
              }
              updateStatusInfo(storageFileHandle);
            }

            // get temporary image file name
            imageFileName = String_new();
            error = File_getTmpFileName(imageFileName,NULL,tmpDirectory);
            if (error != ERROR_NONE)
            {
              break;
            }

            // init macros
            TEXT_MACRO_N_STRING (textMacros[0],"%device",   storageFileHandle->storageSpecifier.deviceName);
            TEXT_MACRO_N_STRING (textMacros[1],"%directory",storageFileHandle->device.directory           );
            TEXT_MACRO_N_STRING (textMacros[2],"%image",    imageFileName                                 );
            TEXT_MACRO_N_INTEGER(textMacros[3],"%number",   storageFileHandle->volumeNumber               );

            // create image
            if (error == ERROR_NONE)
            {
              printInfo(0,"Make image pre-processing of volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.imagePreProcessCommand ),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
            }
            if (error == ERROR_NONE)
            {
              printInfo(0,"Make image volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.imageCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
            }
            if (error == ERROR_NONE)
            {
              printInfo(0,"Make image post-processing of volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.imagePostProcessCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
            }

            // write to device
            if (error == ERROR_NONE)
            {
              printInfo(0,"Write device pre-processing of volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.writePreProcessCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
            }
            if (error == ERROR_NONE)
            {
              printInfo(0,"Write device volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.writeCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
            }
            if (error == ERROR_NONE)
            {
              printInfo(0,"Write device post-processing of volume #%d...",storageFileHandle->volumeNumber);
              error = Misc_executeCommand(String_cString(storageFileHandle->device.writePostProcessCommand),
                                          textMacros,
                                          SIZE_OF_ARRAY(textMacros),
                                          (ExecuteIOFunction)executeIOOutput,
                                          (ExecuteIOFunction)executeIOOutput,
                                          NULL
                                         );
              printInfo(0,(error == ERROR_NONE) ? "ok\n" : "FAIL\n");
            }

            if (error != ERROR_NONE)
            {
              File_delete(imageFileName,FALSE);
              String_delete(imageFileName);
              break;
            }
            File_delete(imageFileName,FALSE);
            String_delete(imageFileName);

            // delete stored files
            fileName = String_new();
            while (!StringList_isEmpty(&storageFileHandle->device.fileNameList))
            {
              StringList_getFirst(&storageFileHandle->device.fileNameList,fileName);
              error = File_delete(fileName,FALSE);
              if (error != ERROR_NONE)
              {
                break;
              }
            }
            String_delete(fileName);
            if (error != ERROR_NONE)
            {
              break;
            }

            // reset
            storageFileHandle->device.newVolumeFlag = TRUE;
            storageFileHandle->device.totalSize     = 0;
          }
        }
        else
        {
          // update info
          storageFileHandle->runningInfo.volumeProgress = 1.0;
          updateStatusInfo(storageFileHandle);
        }
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
       #endif /* NDEBUG */
      break;
  }

  return error;
}

uint Storage_getVolumeNumber(const StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);

  return storageFileHandle->volumeNumber;
}

void Storage_setVolumeNumber(StorageFileHandle *storageFileHandle,
                             uint              volumeNumber
                            )
{
  assert(storageFileHandle != NULL);

  storageFileHandle->volumeNumber = volumeNumber;
}

Errors Storage_unloadVolume(StorageFileHandle *storageFileHandle)
{
  Errors error;

  assert(storageFileHandle != NULL);

  error = ERROR_UNKNOWN;
  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
    case STORAGE_TYPE_FILESYSTEM:
    case STORAGE_TYPE_FTP:
    case STORAGE_TYPE_SSH:
    case STORAGE_TYPE_SCP:
    case STORAGE_TYPE_SFTP:
    case STORAGE_TYPE_WEBDAV:
      error = ERROR_NONE;
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      {
        TextMacro textMacros[1];

        TEXT_MACRO_N_STRING(textMacros[0],"%device",storageFileHandle->storageSpecifier.deviceName);
        error = Misc_executeCommand(String_cString(storageFileHandle->opticalDisk.write.unloadVolumeCommand),
                                    textMacros,SIZE_OF_ARRAY(textMacros),
                                    (ExecuteIOFunction)executeIOOutput,
                                    (ExecuteIOFunction)executeIOOutput,
                                    NULL
                                   );
      }
      break;
    case STORAGE_TYPE_DEVICE:
      {
        TextMacro textMacros[1];

        TEXT_MACRO_N_STRING(textMacros[0],"%device",storageFileHandle->storageSpecifier.deviceName);
        error = Misc_executeCommand(String_cString(storageFileHandle->device.unloadVolumeCommand),
                                    textMacros,SIZE_OF_ARRAY(textMacros),
                                    (ExecuteIOFunction)executeIOOutput,
                                    (ExecuteIOFunction)executeIOOutput,
                                    NULL
                                   );
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_create(StorageFileHandle *storageFileHandle,
                      const String      fileName,
                      uint64            fileSize
                     )
{
  Errors error;
  String directoryName;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);
  assert(fileName != NULL);

  // init variables
  storageFileHandle->mode = STORAGE_MODE_WRITE;

  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      // check if archive file exists
      if (!storageFileHandle->jobOptions->overwriteArchiveFilesFlag && File_exists(fileName))
      {
        return ERRORX_(FILE_EXISTS_,0,String_cString(fileName));
      }

      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        // create directory if not existing
        directoryName = File_getFilePathName(String_new(),fileName);
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

        // open file
        error = File_open(&storageFileHandle->fileSystem.fileHandle,
                          fileName,
                          FILE_OPEN_CREATE
                         );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL)
        {
          String          pathName,baseName;
          String          url;
          CURLcode        curlCode;
          const char      *plainLoginPassword;
          CURLMcode       curlMCode;
          StringTokenizer nameTokenizer;
          String          name;
          int             runningHandles;

          // initialize variables
          storageFileHandle->ftp.index                  = 0LL;
          storageFileHandle->ftp.size                   = fileSize;
          storageFileHandle->ftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->ftp.readAheadBuffer.length = 0L;
          storageFileHandle->ftp.length                 = 0L;
          storageFileHandle->ftp.transferedBytes        = 0L;

          // open Curl handles
          storageFileHandle->ftp.curlMultiHandle = curl_multi_init();
          if (storageFileHandle->ftp.curlMultiHandle == NULL)
          {
            return ERROR_FTP_SESSION_FAIL;
          }
          storageFileHandle->ftp.curlHandle = curl_easy_init();
          if (storageFileHandle->ftp.curlHandle == NULL)
          {
            curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
            return ERROR_FTP_SESSION_FAIL;
          }
          (void)curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_FTP_RESPONSE_TIMEOUT,FTP_TIMEOUT/1000);
          if (globalOptions.verboseLevel >= 6)
          {
            // enable debug mode
            (void)curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_VERBOSE,1L);
          }

          /* work-around for curl limitation/bug: curl trigger sometimes an alarm
             signal if signal are not switched off.
             Note: if signals are switched of c-ares library for asynchronous DNS
             is recommented to avoid infinite lookups
          */
          (void)curl_easy_setopt(storageFileHandle->ftp.curlMultiHandle,CURLOPT_NOSIGNAL,1);
          (void)curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_NOSIGNAL,1);

          // get pathname, basename
          pathName = File_getFilePathName(String_new(),fileName);
          baseName = File_getFileBaseName(String_new(),fileName);

          // get URL
          url = String_format(String_new(),"ftp://%S",storageFileHandle->storageSpecifier.hostName);
          if (storageFileHandle->storageSpecifier.hostPort != 0) String_format(url,":d",storageFileHandle->storageSpecifier.hostPort);
          File_initSplitFileName(&nameTokenizer,pathName);
          while (File_getNextSplitFileName(&nameTokenizer,&name))
          {
            String_appendChar(url,'/');
            String_append(url,name);
          }
          File_doneSplitFileName(&nameTokenizer);
          String_append(url,baseName);

          // set FTP connect
          curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_URL,String_cString(url));
          if (curlCode != CURLE_OK)
          {
            String_delete(url);
            String_delete(baseName);
            String_delete(pathName);
            (void)curl_easy_cleanup(storageFileHandle->ftp.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
            return ERRORX_(FTP_SESSION_FAIL,0,curl_easy_strerror(curlCode));
          }

          // set FTP login
          (void)curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_USERNAME,String_cString(storageFileHandle->storageSpecifier.loginName));
          plainLoginPassword = Password_deploy(storageFileHandle->storageSpecifier.loginPassword);
          (void)curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_PASSWORD,plainLoginPassword);
          Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // create directories if necessary
            curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_FTP_CREATE_MISSING_DIRS,1L);
            if (curlCode != CURLE_OK)
            {
              String_delete(url);
              String_delete(baseName);
              String_delete(pathName);
              (void)curl_easy_cleanup(storageFileHandle->ftp.curlHandle);
              (void)curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
              return ERRORX_(CREATE_DIRECTORY,0,curl_easy_strerror(curlCode));
            }

            // init FTP upload (Note: by default curl use passive FTP)
            curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_READFUNCTION,curlFTPReadDataCallback);
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_READDATA,storageFileHandle);
            }
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_UPLOAD,1L);
            }
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_INFILESIZE_LARGE,(curl_off_t)storageFileHandle->ftp.size);
            }
            curlMCode = curl_multi_add_handle(storageFileHandle->ftp.curlMultiHandle,storageFileHandle->ftp.curlHandle);
            if (curlMCode != CURLM_OK)
            {
              String_delete(url);
              String_delete(baseName);
              String_delete(pathName);
              (void)curl_easy_cleanup(storageFileHandle->ftp.curlHandle);
              (void)curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
              return ERRORX_(FTP_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
            }

            // start FTP upload
            do
            {
              curlMCode = curl_multi_perform(storageFileHandle->ftp.curlMultiHandle,&runningHandles);
            }
            while (   (curlMCode == CURLM_CALL_MULTI_PERFORM)
                   && (runningHandles > 0)
                  );
//fprintf(stderr,"%s, %d: storageFileHandle->ftp.runningHandles=%d\n",__FILE__,__LINE__,storageFileHandle->ftp.runningHandles);
            if (curlMCode != CURLM_OK)
            {
              String_delete(url);
              String_delete(baseName);
              String_delete(pathName);
              (void)curl_multi_remove_handle(storageFileHandle->ftp.curlMultiHandle,storageFileHandle->ftp.curlHandle);
              (void)curl_easy_cleanup(storageFileHandle->ftp.curlHandle);
              (void)curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
              return ERRORX_(FTP_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
            }

            // free resources
            String_delete(url);
            String_delete(baseName);
            String_delete(pathName);
          }
        }
      #elif defined(HAVE_FTP)
        {
          String          pathName;
          String          directoryName;
          StringTokenizer pathNameTokenizer;
          String          name;
          const char      *plainPassword;

          // initialize variables
          storageFileHandle->ftp.index                  = 0LL;
          storageFileHandle->ftp.size                   = fileSize;
          storageFileHandle->ftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->ftp.readAheadBuffer.length = 0L;
          storageFileHandle->ftp.length                 = 0L;
          storageFileHandle->ftp.transferedBytes        = 0L;

          // connect
          if (!Network_hostExists(storageFileHandle->storageSpecifier.hostName))
          {
            return ERROR_HOST_NOT_FOUND;
          }
          if (FtpConnect(String_cString(storageFileHandle->storageSpecifier.hostName),&storageFileHandle->ftp.control) != 1)
          {
            return ERROR_FTP_SESSION_FAIL;
          }

          // login
          plainPassword = Password_deploy(storageFileHandle->storageSpecifier.loginPassword);
          if (FtpLogin(String_cString(storageFileHandle->storageSpecifier.loginName),
                       plainPassword,
                       storageFileHandle->ftp.control
                      ) != 1
             )
          {
            Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);
            FtpClose(storageFileHandle->ftp.control);
            return ERROR_FTP_AUTHENTICATION;
          }
          Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // create directory (try it and ignore errors)
            pathName      = File_getFilePathName(String_new(),fileName);
            directoryName = File_newFileName();
            File_initSplitFileName(&pathNameTokenizer,pathName);
            if (File_getNextSplitFileName(&pathNameTokenizer,&name))
            {
              // create root-directory
              if (!String_isEmpty(name))
              {
                File_setFileName(directoryName,name);
              }
              else
              {
                File_setFileNameChar(directoryName,FILES_PATHNAME_SEPARATOR_CHAR);
              }
              (void)FtpMkdir(String_cString(directoryName),storageFileHandle->ftp.control);

              // create sub-directories
              while (File_getNextSplitFileName(&pathNameTokenizer,&name))
              {
                if (!String_isEmpty(name))
                {
                  // get sub-directory
                  File_appendFileName(directoryName,name);

                  // create sub-directory
                  (void)FtpMkdir(String_cString(directoryName),storageFileHandle->ftp.control);
                }
              }
            }
            File_doneSplitFileName(&pathNameTokenizer);
            File_deleteFileName(directoryName);
            File_deleteFileName(pathName);

            // set timeout callback (120s) to avoid infinite waiting on read/write
            if (FtpOptions(FTPLIB_IDLETIME,
                           120*1000,
                           storageFileHandle->ftp.control
                          ) != 1
               )
            {
              FtpClose(storageFileHandle->ftp.control);
              return ERRORX_(CREATE_FILE,0,"ftp access");
            }
            if (FtpOptions(FTPLIB_CALLBACK,
                           (long)ftpTimeoutCallback,
                           storageFileHandle->ftp.control
                          ) != 1
               )
            {
              FtpClose(storageFileHandle->ftp.control);
              return ERRORX_(CREATE_FILE,0,"ftp access");
            }

            // create file: first try non-passive, then passive mode
            FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,storageFileHandle->ftp.control);
            if (FtpAccess(String_cString(fileName),
                          FTPLIB_FILE_WRITE,
                          FTPLIB_IMAGE,
                          storageFileHandle->ftp.control,
                          &storageFileHandle->ftp.data
                         ) != 1
               )
            {
              FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,storageFileHandle->ftp.control);
              if (FtpAccess(String_cString(fileName),
                            FTPLIB_FILE_WRITE,
                            FTPLIB_IMAGE,
                            storageFileHandle->ftp.control,
                            &storageFileHandle->ftp.data
                           ) != 1
                 )
              {
                FtpClose(storageFileHandle->ftp.control);
                return ERRORX_(CREATE_FILE,0,"ftp access");
              }
            }
          }
        }
      #else /* not HAVE_CURL || HAVE_FTP */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          // connect
          error = Network_connect(&storageFileHandle->scp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  storageFileHandle->storageSpecifier.loginPassword,
                                  storageFileHandle->scp.sshPublicKeyFileName,
                                  storageFileHandle->scp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),READ_TIMEOUT);

          // install send/receive callback to track number of sent/received bytes
          storageFileHandle->scp.totalSentBytes     = 0LL;
          storageFileHandle->scp.totalReceivedBytes = 0LL;
          (*(libssh2_session_abstract(Network_getSSHSession(&storageFileHandle->scp.socketHandle)))) = storageFileHandle;
          storageFileHandle->scp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,sshSendCallback   );
          storageFileHandle->scp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,sshReceiveCallback);

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // open channel and file for writing
            #ifdef HAVE_SSH2_SCP_SEND64
              storageFileHandle->scp.channel = libssh2_scp_send64(Network_getSSHSession(&storageFileHandle->scp.socketHandle),
                                                                  String_cString(fileName),
// ???
0600,
                                                                  (libssh2_uint64_t)fileSize,
// ???
                                                                  0L,
                                                                  0L
                                                                 );
            #else /* not HAVE_SSH2_SCP_SEND64 */
              storageFileHandle->scp.channel = libssh2_scp_send(Network_getSSHSession(&storageFileHandle->scp.socketHandle),
                                                                String_cString(fileName),
// ???
0600,
                                                                (size_t)fileSize
                                                               );
            #endif /* HAVE_SSH2_SCP_SEND64 */
            if (storageFileHandle->scp.channel == NULL)
            {
              char *sshErrorText;

              libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->scp.socketHandle),&sshErrorText,NULL,0);
              error = ERRORX_(SSH,
                              libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->scp.socketHandle)),
                              sshErrorText
                             );
              Network_disconnect(&storageFileHandle->scp.socketHandle);
              return error;
            }
          }
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(fileSize);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          // connect
          error = Network_connect(&storageFileHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  storageFileHandle->storageSpecifier.loginPassword,
                                  storageFileHandle->sftp.sshPublicKeyFileName,
                                  storageFileHandle->sftp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),READ_TIMEOUT);

          // install send/receive callback to track number of sent/received bytes
          storageFileHandle->sftp.totalSentBytes     = 0LL;
          storageFileHandle->sftp.totalReceivedBytes = 0LL;
          (*(libssh2_session_abstract(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)))) = storageFileHandle;
          storageFileHandle->sftp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),LIBSSH2_CALLBACK_SEND,sftpSendCallback   );
          storageFileHandle->sftp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),LIBSSH2_CALLBACK_RECV,sftpReceiveCallback);

          // init session
          storageFileHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageFileHandle->sftp.socketHandle));
          if (storageFileHandle->sftp.sftp == NULL)
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX_(SSH,
                            libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                            sshErrorText
                           );
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return error;
          }

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // create file
            storageFileHandle->sftp.sftpHandle = libssh2_sftp_open(storageFileHandle->sftp.sftp,
                                                                   String_cString(fileName),
                                                                   LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_TRUNC,
// ???
LIBSSH2_SFTP_S_IRUSR|LIBSSH2_SFTP_S_IWUSR
                                                                  );
            if (storageFileHandle->sftp.sftpHandle == NULL)
            {
              char *sshErrorText;

              libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
              error = ERRORX_(SSH,
                              libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                              sshErrorText
                             );
              libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
              Network_disconnect(&storageFileHandle->sftp.socketHandle);
              return error;
            }
          }
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(fileSize);
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
#warning todo webdav
      #ifdef HAVE_CURL
        {
          String          baseURL;
          CURLcode        curlCode;
          const char      *plainLoginPassword;
          CURLMcode       curlMCode;
          String          url;
          String          pathName,baseName;
          StringTokenizer nameTokenizer;
          String          name;
          int             runningHandles;

          // initialize variables
          storageFileHandle->webdav.index             = 0LL;
          storageFileHandle->webdav.size              = fileSize;
          storageFileHandle->webdav.sendBuffer.index  = 0L;
          storageFileHandle->webdav.sendBuffer.length = 0L;

          // open Curl handles
          storageFileHandle->webdav.curlMultiHandle = curl_multi_init();
          if (storageFileHandle->webdav.curlMultiHandle == NULL)
          {
            return ERROR_WEBDAV_SESSION_FAIL;
          }
          storageFileHandle->webdav.curlHandle = curl_easy_init();
          if (storageFileHandle->webdav.curlHandle == NULL)
          {
            curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
            return ERROR_WEBDAV_SESSION_FAIL;
          }
          (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_CONNECTTIMEOUT_MS,WEBDAV_TIMEOUT);
          if (globalOptions.verboseLevel >= 6)
          {
            // enable debug mode
            (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_VERBOSE,1L);
          }

          /* work-around for curl limitation/bug: curl trigger sometimes an alarm
             signal if signal are not switched off.
             Note: if signals are switched of c-ares library for asynchronous DNS
             is recommented to avoid infinite lookups
          */
          (void)curl_easy_setopt(storageFileHandle->webdav.curlMultiHandle,CURLOPT_NOSIGNAL,1);
          (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_NOSIGNAL,1);

          // get base URL
          baseURL = String_format(String_new(),"http://%S",storageFileHandle->storageSpecifier.hostName);
          if (storageFileHandle->storageSpecifier.hostPort != 0) String_format(baseURL,":d",storageFileHandle->storageSpecifier.hostPort);

          // set WebDAV connect
          curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_URL,String_cString(baseURL));
          if (curlCode != CURLE_OK)
          {
            String_delete(baseURL);
            (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
            return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_easy_strerror(curlCode));
          }

          // set WebDAV login
          (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_USERNAME,String_cString(storageFileHandle->storageSpecifier.loginName));
          plainLoginPassword = Password_deploy(storageFileHandle->storageSpecifier.loginPassword);
          (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_PASSWORD,plainLoginPassword);
          Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // get pathname, basename
            pathName = File_getFilePathName(String_new(),fileName);
            baseName = File_getFileBaseName(String_new(),fileName);

            // create directories if necessary
            if (!String_isEmpty(pathName))
            {
              url = String_format(String_duplicate(baseURL),"/");
              File_initSplitFileName(&nameTokenizer,pathName);
              while (File_getNextSplitFileName(&nameTokenizer,&name))
              {
                String_append(url,name);
                String_appendChar(url,'/');

                curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_NOBODY,1L);
                if (curlCode != CURLE_OK)
                {
                  break;
                }
                curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_CUSTOMREQUEST,"MKCOL");
                if (curlCode != CURLE_OK)
                {
                  break;
                }
                curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_URL,String_cString(url));
                if (curlCode != CURLE_OK)
                {
                  break;
                }
                curlCode = curl_easy_perform(storageFileHandle->webdav.curlHandle);
                if (curlCode != CURLE_OK)
                {
                  break;
                }
              }
              File_doneSplitFileName(&nameTokenizer);
              if (curlCode != CURLE_OK)
              {
                String_delete(url);
                String_delete(baseName);
                String_delete(pathName);
                String_delete(baseURL);
                (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
                (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
                return ERRORX_(CREATE_DIRECTORY,0,curl_easy_strerror(curlCode));
              }
              String_delete(url);
            }

            // first delete file if overwrite requested
            if (storageFileHandle->jobOptions->overwriteArchiveFilesFlag)
            {
              url = String_format(String_duplicate(baseURL),"/");
              File_initSplitFileName(&nameTokenizer,pathName);
              while (File_getNextSplitFileName(&nameTokenizer,&name))
              {
                String_append(url,name);
                String_appendChar(url,'/');
              }
              File_doneSplitFileName(&nameTokenizer);
              String_append(url,baseName);

              curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_NOBODY,1L);
              if (curlCode == CURLE_OK)
              {
                curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_CUSTOMREQUEST,"DELETE");
              }
              if (curlCode == CURLE_OK)
              {
                curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_URL,String_cString(url));
              }
              if (curlCode == CURLE_OK)
              {
                curlCode = curl_easy_perform(storageFileHandle->webdav.curlHandle);
              }
              if (curlCode != CURLE_OK)
              {
                String_delete(url);
                String_delete(baseName);
                String_delete(pathName);
                String_delete(baseURL);
                (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
                (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
                return ERRORX_(DELETE_FILE,0,curl_easy_strerror(curlCode));
              }

              String_delete(url);
            }

            // init WebDAV upload
            url = String_format(String_duplicate(baseURL),"/");
            File_initSplitFileName(&nameTokenizer,pathName);
            while (File_getNextSplitFileName(&nameTokenizer,&name))
            {
              String_append(url,name);
              String_appendChar(url,'/');
            }
            File_doneSplitFileName(&nameTokenizer);
            String_append(url,baseName);

            curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_NOBODY,0L);
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_CUSTOMREQUEST,"PUT");
            }
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_URL,String_cString(url));
            }
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_NOBODY,0L);
            }
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_UPLOAD,1L);
            }
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_READFUNCTION,curlWebDAVReadDataCallback);
            }
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_READDATA,storageFileHandle);
            }
            if (curlCode == CURLE_OK)
            {
              curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_INFILESIZE_LARGE,(curl_off_t)storageFileHandle->webdav.size);
            }
            if (curlCode != CURLE_OK)
            {
              String_delete(url);
              String_delete(baseName);
              String_delete(pathName);
              String_delete(baseURL);
              (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
              (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
              return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_easy_strerror(curlCode));
            }
            curlMCode = curl_multi_add_handle(storageFileHandle->webdav.curlMultiHandle,storageFileHandle->webdav.curlHandle);
            if (curlMCode != CURLM_OK)
            {
              String_delete(url);
              String_delete(baseName);
              String_delete(pathName);
              String_delete(baseURL);
              (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
              (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
              return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
            }
            String_delete(url);

            // start WebDAV upload
            do
            {
              curlMCode = curl_multi_perform(storageFileHandle->webdav.curlMultiHandle,&runningHandles);
            }
            while (   (curlMCode == CURLM_CALL_MULTI_PERFORM)
                   && (runningHandles > 0)
                  );
//fprintf(stderr,"%s, %d: storageFileHandle->webdav.runningHandles=%d\n",__FILE__,__LINE__,runningHandles);
            if (curlMCode != CURLM_OK)
            {
              String_delete(baseName);
              String_delete(pathName);
              String_delete(baseURL);
              (void)curl_multi_remove_handle(storageFileHandle->webdav.curlMultiHandle,storageFileHandle->webdav.curlHandle);
              (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
              (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
              return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
            }

            // free resources
            String_delete(baseName);
            String_delete(pathName);
          }

          // free resources
          String_delete(baseURL);
        }
      #else /* not HAVE_CURL */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      // create file name
      String_set(storageFileHandle->opticalDisk.write.fileName,storageFileHandle->opticalDisk.write.directory);
      File_appendFileName(storageFileHandle->opticalDisk.write.fileName,fileName);

      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        // create directory if not existing
        directoryName = File_getFilePathName(String_new(),storageFileHandle->opticalDisk.write.fileName);
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
        error = File_open(&storageFileHandle->opticalDisk.write.fileHandle,
                          storageFileHandle->opticalDisk.write.fileName,
                          FILE_OPEN_CREATE
                         );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      break;
    case STORAGE_TYPE_DEVICE:
      // create file name
      String_set(storageFileHandle->device.fileName,storageFileHandle->device.directory);
      File_appendFileName(storageFileHandle->device.fileName,fileName);

      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        // open file
        error = File_open(&storageFileHandle->device.fileHandle,
                          storageFileHandle->device.fileName,
                          FILE_OPEN_CREATE
                         );
        if (error != ERROR_NONE)
        {
          return error;
        }
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return ERROR_NONE;
}

#warning storageSpecifier needed? see create
Errors Storage_open(StorageFileHandle *storageFileHandle,
                    const StorageSpecifier *storageSpecifier,
                    const String           storageFileName
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageFileName != NULL);

  // init variables
  storageFileHandle->mode = STORAGE_MODE_READ;

  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      // check if file exists
      if (!File_exists(storageFileName))
      {
        return ERRORX_(FILE_NOT_FOUND_,0,String_cString(storageFileName));
      }

      // open file
      error = File_open(&storageFileHandle->fileSystem.fileHandle,
                        storageFileName,
                        FILE_OPEN_READ
                       );
      if (error != ERROR_NONE)
      {
        return error;
      }
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL)
        {
          String          pathName,baseName;
          String          url;
          CURLcode        curlCode;
          const char      *plainLoginPassword;
          CURLMcode       curlMCode;
          StringTokenizer nameTokenizer;
          String          name;
          long            httpCode;
          double          fileSize;
          int             runningHandles;

          // initialize variables
          storageFileHandle->ftp.index                  = 0LL;
          storageFileHandle->ftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->ftp.readAheadBuffer.length = 0L;

          // open Curl handles
          storageFileHandle->ftp.curlMultiHandle = curl_multi_init();
          if (storageFileHandle->ftp.curlMultiHandle == NULL)
          {
            return ERROR_FTP_SESSION_FAIL;
          }
          storageFileHandle->ftp.curlHandle = curl_easy_init();
          if (storageFileHandle->ftp.curlHandle == NULL)
          {
            curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
            return ERROR_FTP_SESSION_FAIL;
          }
          (void)curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_FTP_RESPONSE_TIMEOUT,FTP_TIMEOUT/1000);
          if (globalOptions.verboseLevel >= 6)
          {
            // enable debug mode
            (void)curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_VERBOSE,1L);
          }

          /* work-around for curl limitation/bug: curl trigger sometimes an alarm
             signal if signal are not switched off.
             Note: if signals are switched of c-ares library for asynchronous DNS
             is recommented to avoid infinite lookups
          */
          (void)curl_easy_setopt(storageFileHandle->ftp.curlMultiHandle,CURLOPT_NOSIGNAL,1);
          (void)curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_NOSIGNAL,1);

          // get pathname, basename
          pathName = File_getFilePathName(String_new(),storageFileName);
          baseName = File_getFileBaseName(String_new(),storageFileName);

          // get URL
          url = String_format(String_new(),"ftp://%S",storageSpecifier->hostName);
          if (storageFileHandle->storageSpecifier.hostPort != 0) String_format(url,":d",storageFileHandle->storageSpecifier.hostPort);
          File_initSplitFileName(&nameTokenizer,pathName);
          while (File_getNextSplitFileName(&nameTokenizer,&name))
          {
            String_appendChar(url,'/');
            String_append(url,name);
          }
          File_doneSplitFileName(&nameTokenizer);
          String_append(url,baseName);

          // set FTP connect
          curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_URL,String_cString(url));
          if (curlCode != CURLE_OK)
          {
            String_delete(url);
            String_delete(baseName);
            String_delete(pathName);
            (void)curl_easy_cleanup(storageFileHandle->ftp.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
            return ERRORX_(FTP_SESSION_FAIL,0,curl_easy_strerror(curlCode));
          }

          // set FTP login
          (void)curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_USERNAME,String_cString(storageFileHandle->storageSpecifier.loginName));
          plainLoginPassword = Password_deploy(storageFileHandle->storageSpecifier.loginPassword);
          (void)curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_PASSWORD,plainLoginPassword);
          Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);

          // check if file exists (Note: by default curl use passive FTP)
          curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_NOBODY,1L);
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_HEADERFUNCTION,curlNopDataCallback);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_HEADER,0L);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_WRITEFUNCTION,curlNopDataCallback);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_WRITEDATA,0L);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_perform(storageFileHandle->ftp.curlHandle);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_getinfo(storageFileHandle->ftp.curlHandle,CURLINFO_RESPONSE_CODE,&httpCode);
          }
          if (curlCode != CURLE_OK)
          {
            String_delete(url);
            String_delete(baseName);
            String_delete(pathName);
            (void)curl_easy_cleanup(storageFileHandle->ftp.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
            return ERRORX_(FILE_NOT_FOUND_,0,String_cString(storageFileName));
          }
#warning todo
fprintf(stderr,"%s, %d: httpCode=%ld\n",__FILE__,__LINE__,httpCode);

          // get file size
          curlCode = curl_easy_getinfo(storageFileHandle->ftp.curlHandle,CURLINFO_CONTENT_LENGTH_DOWNLOAD,&fileSize);
          if (   (curlCode != CURLE_OK)
              || (fileSize < 0.0)
             )
          {
            String_delete(url);
            String_delete(baseName);
            String_delete(pathName);
            (void)curl_easy_cleanup(storageFileHandle->ftp.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
            return ERROR_FTP_GET_SIZE;
          }
          storageFileHandle->ftp.size = (uint64)fileSize;
//fprintf(stderr,"%s, %d: storageFileHandle->ftp.size=%llu\n",__FILE__,__LINE__,storageFileHandle->ftp.size);

          // init FTP download (Note: by default curl use passive FTP)
          (void)curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_NOBODY,0L);
          curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_WRITEFUNCTION,curlFTPWriteDataCallback);
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->ftp.curlHandle,CURLOPT_WRITEDATA,storageFileHandle);
          }
          if (curlCode != CURLE_OK)
          {
            String_delete(url);
            String_delete(baseName);
            String_delete(pathName);
            (void)curl_easy_cleanup(storageFileHandle->ftp.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
            return ERRORX_(FTP_SESSION_FAIL,0,curl_easy_strerror(curlCode));
          }
          curlMCode = curl_multi_add_handle(storageFileHandle->ftp.curlMultiHandle,storageFileHandle->ftp.curlHandle);
          if (curlMCode != CURLM_OK)
          {
            String_delete(url);
            String_delete(baseName);
            String_delete(pathName);
            (void)curl_easy_cleanup(storageFileHandle->ftp.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
            return ERRORX_(FTP_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
          }

          // start FTP download
          do
          {
            curlMCode = curl_multi_perform(storageFileHandle->ftp.curlMultiHandle,&runningHandles);
          }
          while (   (curlMCode == CURLM_CALL_MULTI_PERFORM)
                 && (runningHandles > 0)
                );
//fprintf(stderr,"%s, %d: storageFileHandle->ftp.runningHandles=%d\n",__FILE__,__LINE__,storageFileHandle->ftp.runningHandles);
          if (curlMCode != CURLM_OK)
          {
            String_delete(url);
            String_delete(baseName);
            String_delete(pathName);
            (void)curl_multi_remove_handle(storageFileHandle->ftp.curlMultiHandle,storageFileHandle->ftp.curlHandle);
            (void)curl_easy_cleanup(storageFileHandle->ftp.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
            return ERRORX_(FTP_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
          }

          // free resources
          String_delete(url);
          String_delete(baseName);
          String_delete(pathName);
        }
      #elif defined(HAVE_FTP)
        {
          const char *plainLoginPassword;
          String     tmpFileName;
          FileHandle fileHandle;
          bool       foundFlag;
          String     line;
          int        size;

          // initialize variables
          storageFileHandle->ftp.control                = NULL;
          storageFileHandle->ftp.data                   = NULL;
          storageFileHandle->ftp.index                  = 0LL;
          storageFileHandle->ftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->ftp.readAheadBuffer.length = 0L;

          // FTP connect
          if (!Network_hostExists(storageFileHandle->storageSpecifier.hostName))
          {
            return ERROR_HOST_NOT_FOUND;
          }
          if (FtpConnect(String_cString(storageFileHandle->storageSpecifier.hostName),&storageFileHandle->ftp.control) != 1)
          {
            return ERROR_FTP_SESSION_FAIL;
          }

          // FTP login
          plainLoginPassword = Password_deploy(storageFileHandle->storageSpecifier.loginPassword);
          if (FtpLogin(String_cString(storageFileHandle->storageSpecifier.loginName),
                       plainLoginPassword,
                       storageFileHandle->ftp.control
                      ) != 1
             )
          {
            Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);
            FtpClose(storageFileHandle->ftp.control);
            return ERROR_FTP_AUTHENTICATION;
          }
          Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);

          // check if file exists: first try non-passive, then passive mode
          tmpFileName = File_newFileName();
          error = File_getTmpFileName(tmpFileName,NULL,tmpDirectory);
          if (error != ERROR_NONE)
          {
            FtpClose(storageFileHandle->ftp.control);
            return error;
          }
          FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,storageFileHandle->ftp.control);
          if (FtpDir(String_cString(tmpFileName),String_cString(storageFileName),storageFileHandle->ftp.control) != 1)
          {
            FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,storageFileHandle->ftp.control);
            if (FtpDir(String_cString(tmpFileName),String_cString(storageFileName),storageFileHandle->ftp.control) != 1)
            {
              File_delete(tmpFileName,FALSE);
              File_deleteFileName(tmpFileName);
              FtpClose(storageFileHandle->ftp.control);
              return ERRORX_(FILE_NOT_FOUND_,0,String_cString(storageFileName));
            }
          }
          error = File_open(&fileHandle,tmpFileName,FILE_OPEN_READ);
          if (error != ERROR_NONE)
          {
            File_delete(tmpFileName,FALSE);
            File_deleteFileName(tmpFileName);
            FtpClose(storageFileHandle->ftp.control);
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
            FtpClose(storageFileHandle->ftp.control);
            return ERRORX_(FILE_NOT_FOUND_,0,String_cString(storageFileName));
          }

          // get file size
          if (FtpSize(String_cString(storageFileName),
                      &size,
                      FTPLIB_IMAGE,
                      storageFileHandle->ftp.control
                     ) != 1
             )
          {
            FtpClose(storageFileHandle->ftp.control);
            return ERROR_FTP_GET_SIZE;
          }
          storageFileHandle->ftp.size = (uint64)size;

          // init FTP download: first try non-passive, then passive mode
          FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,storageFileHandle->ftp.control);
          if (FtpAccess(String_cString(storageFileName),
                        FTPLIB_FILE_READ,
                        FTPLIB_IMAGE,
                        storageFileHandle->ftp.control,
                        &storageFileHandle->ftp.data
                       ) != 1
             )
          {
            FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,storageFileHandle->ftp.control);
            if (FtpAccess(String_cString(storageFileName),
                          FTPLIB_FILE_READ,
                          FTPLIB_IMAGE,
                          storageFileHandle->ftp.control,
                          &storageFileHandle->ftp.data
                         ) != 1
               )
            {
              FtpClose(storageFileHandle->ftp.control);
              return ERRORX_(OPEN_FILE,0,"ftp access");
            }
          }
        }
      #else /* not HAVE_CURL || HAVE_FTP */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          struct stat fileInfo;

          // init variables
          storageFileHandle->scp.index                  = 0LL;
          storageFileHandle->scp.readAheadBuffer.offset = 0LL;
          storageFileHandle->scp.readAheadBuffer.length = 0L;

          // connect
          error = Network_connect(&storageFileHandle->scp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  storageFileHandle->storageSpecifier.loginPassword,
                                  storageFileHandle->scp.sshPublicKeyFileName,
                                  storageFileHandle->scp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageFileHandle->scp.socketHandle),READ_TIMEOUT);

          // install send/receive callback to track number of sent/received bytes
          storageFileHandle->scp.totalSentBytes     = 0LL;
          storageFileHandle->scp.totalReceivedBytes = 0LL;
          (*(libssh2_session_abstract(Network_getSSHSession(&storageFileHandle->scp.socketHandle)))) = storageFileHandle;
          storageFileHandle->scp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->scp.socketHandle),LIBSSH2_CALLBACK_SEND,sshSendCallback   );
          storageFileHandle->scp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->scp.socketHandle),LIBSSH2_CALLBACK_RECV,sshReceiveCallback);

          // open channel and file for reading
          storageFileHandle->scp.channel = libssh2_scp_recv(Network_getSSHSession(&storageFileHandle->scp.socketHandle),
                                                            String_cString(storageFileName),
                                                            &fileInfo
                                                           );
          if (storageFileHandle->scp.channel == NULL)
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->scp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX_(SSH,
                            libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->scp.socketHandle)),
                            sshErrorText
                           );
            Network_disconnect(&storageFileHandle->scp.socketHandle);
            return error;
          }
          storageFileHandle->scp.size = (uint64)fileInfo.st_size;
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(storageFileName);
        UNUSED_VARIABLE(storageSpecifier);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;

          // init variables
          storageFileHandle->sftp.index                  = 0LL;
          storageFileHandle->sftp.readAheadBuffer.offset = 0LL;
          storageFileHandle->sftp.readAheadBuffer.length = 0L;

          // connect
          error = Network_connect(&storageFileHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageFileHandle->storageSpecifier.hostName,
                                  storageFileHandle->storageSpecifier.hostPort,
                                  storageFileHandle->storageSpecifier.loginName,
                                  storageFileHandle->storageSpecifier.loginPassword,
                                  storageFileHandle->sftp.sshPublicKeyFileName,
                                  storageFileHandle->sftp.sshPrivateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            return error;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),READ_TIMEOUT);

          // install send/receive callback to track number of sent/received bytes
          storageFileHandle->sftp.totalSentBytes     = 0LL;
          storageFileHandle->sftp.totalReceivedBytes = 0LL;
          (*(libssh2_session_abstract(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)))) = storageFileHandle;
          storageFileHandle->sftp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),LIBSSH2_CALLBACK_SEND,sftpSendCallback   );
          storageFileHandle->sftp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),LIBSSH2_CALLBACK_RECV,sftpReceiveCallback);

          // init session
          storageFileHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageFileHandle->sftp.socketHandle));
          if (storageFileHandle->sftp.sftp == NULL)
          {
            error = ERROR_(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)));
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return error;
          }

          // open file
          storageFileHandle->sftp.sftpHandle = libssh2_sftp_open(storageFileHandle->sftp.sftp,
                                                                 String_cString(storageFileName),
                                                                 LIBSSH2_FXF_READ,
                                                                 0
                                                                );
          if (storageFileHandle->sftp.sftpHandle == NULL)
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX_(SSH,
                            libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                            sshErrorText
                           );
            libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return error;
          }

          // get file size
          if (libssh2_sftp_fstat(storageFileHandle->sftp.sftpHandle,
                                 &sftpAttributes
                                ) != 0
             )
          {
            char *sshErrorText;

            libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
            error = ERRORX_(SSH,
                            libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                            sshErrorText
                           );
            libssh2_sftp_close(storageFileHandle->sftp.sftpHandle);
            libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
            Network_disconnect(&storageFileHandle->sftp.socketHandle);
            return error;
          }
          storageFileHandle->sftp.size = sftpAttributes.filesize;
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(storageFileName);
        UNUSED_VARIABLE(storageSpecifier);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
#warning todo webdav
      #ifdef HAVE_CURL
        {
          String          baseURL;
          CURLcode        curlCode;
          const char      *plainLoginPassword;
          CURLMcode       curlMCode;
          String          url;
          String          pathName,baseName;
          StringTokenizer nameTokenizer;
          String          name;
          long            httpCode;
          double          fileSize;
          int             runningHandles;

          // initialize variables
          storageFileHandle->webdav.index                = 0LL;
          storageFileHandle->webdav.receiveBuffer.size   = INITIAL_BUFFER_SIZE;
          storageFileHandle->webdav.receiveBuffer.offset = 0LL;
          storageFileHandle->webdav.receiveBuffer.length = 0L;

          // allocate transfer buffer
          storageFileHandle->webdav.receiveBuffer.data = (byte*)malloc(storageFileHandle->webdav.receiveBuffer.size);
          if (storageFileHandle->webdav.receiveBuffer.data == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }

          // open Curl handles
          storageFileHandle->webdav.curlMultiHandle = curl_multi_init();
          if (storageFileHandle->webdav.curlMultiHandle == NULL)
          {
            free(storageFileHandle->webdav.receiveBuffer.data);
            return ERROR_WEBDAV_SESSION_FAIL;
          }
          storageFileHandle->webdav.curlHandle = curl_easy_init();
          if (storageFileHandle->webdav.curlHandle == NULL)
          {
            curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
            free(storageFileHandle->webdav.receiveBuffer.data);
            return ERROR_WEBDAV_SESSION_FAIL;
          }
          (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_CONNECTTIMEOUT_MS,WEBDAV_TIMEOUT);
          if (globalOptions.verboseLevel >= 6)
          {
            // enable debug mode
            (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_VERBOSE,1L);
          }

          /* work-around for curl limitation/bug: curl trigger sometimes an alarm
             signal if signal are not switched off.
             Note: if signals are switched of c-ares library for asynchronous DNS
             is recommented to avoid infinite lookups
          */
          (void)curl_easy_setopt(storageFileHandle->webdav.curlMultiHandle,CURLOPT_NOSIGNAL,1);
          (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_NOSIGNAL,1);

          // get base URL
          baseURL = String_format(String_new(),"http://%S",storageFileHandle->storageSpecifier.hostName);
          if (storageFileHandle->storageSpecifier.hostPort != 0) String_format(baseURL,":d",storageFileHandle->storageSpecifier.hostPort);

          // set WebDAV connect
          curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_URL,String_cString(baseURL));
          if (curlCode != CURLE_OK)
          {
            String_delete(baseURL);
            (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
            free(storageFileHandle->webdav.receiveBuffer.data);
            return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_easy_strerror(curlCode));
          }

          // set WebDAV login
          (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_USERNAME,String_cString(storageFileHandle->storageSpecifier.loginName));
          plainLoginPassword = Password_deploy(storageFileHandle->storageSpecifier.loginPassword);
          (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_PASSWORD,plainLoginPassword);
          Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);

          // get pathname, basename
          pathName = File_getFilePathName(String_new(),storageFileName);
          baseName = File_getFileBaseName(String_new(),storageFileName);

          // check if file exists
          url = String_format(String_duplicate(baseURL),"/");
          File_initSplitFileName(&nameTokenizer,pathName);
          while (File_getNextSplitFileName(&nameTokenizer,&name))
          {
            String_append(url,name);
            String_appendChar(url,'/');
          }
          File_doneSplitFileName(&nameTokenizer);
          String_append(url,baseName);

          curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_URL,String_cString(url));
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_NOBODY,1L);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_HEADERFUNCTION,curlNopDataCallback);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_HEADER,0L);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_WRITEFUNCTION,curlNopDataCallback);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_WRITEDATA,0L);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_perform(storageFileHandle->webdav.curlHandle);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_getinfo(storageFileHandle->ftp.curlHandle,CURLINFO_RESPONSE_CODE,&httpCode);
          }
          if (   (curlCode != CURLE_OK)
              || (httpCode != 200)  // HTTP OK
             )
          {
            String_delete(url);
            String_delete(baseName);
            String_delete(pathName);
            String_delete(baseURL);
            (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
            free(storageFileHandle->webdav.receiveBuffer.data);
            return ERROR_FILE_NOT_FOUND_;
          }
          String_delete(url);

          // get file size
          curlCode = curl_easy_getinfo(storageFileHandle->webdav.curlHandle,CURLINFO_CONTENT_LENGTH_DOWNLOAD,&fileSize);
          if (   (curlCode != CURLE_OK)
              || (fileSize < 0.0)
             )
          {
            String_delete(baseName);
            String_delete(pathName);
            String_delete(baseURL);
            (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
            free(storageFileHandle->webdav.receiveBuffer.data);
            return ERROR_WEBDAV_GET_SIZE;
          }
          storageFileHandle->webdav.size = (uint64)fileSize;

          // init WebDAV download
          url = String_format(String_duplicate(baseURL),"/");
          File_initSplitFileName(&nameTokenizer,pathName);
          while (File_getNextSplitFileName(&nameTokenizer,&name))
          {
            String_append(url,name);
            String_appendChar(url,'/');
          }
          File_doneSplitFileName(&nameTokenizer);
          String_append(url,baseName);

          curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_CUSTOMREQUEST,"GET");
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_URL,String_cString(url));
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_NOBODY,0L);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_WRITEFUNCTION,curlWebDAVWriteDataCallback);
          }
          if (curlCode == CURLE_OK)
          {
            curlCode = curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_WRITEDATA,storageFileHandle);
          }
          if (curlCode != CURLE_OK)
          {
            String_delete(baseName);
            String_delete(pathName);
            String_delete(baseURL);
            (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
            free(storageFileHandle->webdav.receiveBuffer.data);
            return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_easy_strerror(curlCode));
          }
          curlMCode = curl_multi_add_handle(storageFileHandle->webdav.curlMultiHandle,storageFileHandle->webdav.curlHandle);
          if (curlMCode != CURLM_OK)
          {
            String_delete(baseName);
            String_delete(pathName);
            String_delete(baseURL);
            (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
            free(storageFileHandle->webdav.receiveBuffer.data);
            return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
          }
          String_delete(url);

          // start WebDAV download
          do
          {
            curlMCode = curl_multi_perform(storageFileHandle->webdav.curlMultiHandle,&runningHandles);
          }
          while (   (curlMCode == CURLM_CALL_MULTI_PERFORM)
                 && (runningHandles > 0)
                );
//fprintf(stderr,"%s, %d: storageFileHandle->webdav.runningHandles=%d\n",__FILE__,__LINE__,runningHandles);
          if (curlMCode != CURLM_OK)
          {
            String_delete(baseName);
            String_delete(pathName);
            String_delete(baseURL);
            (void)curl_multi_remove_handle(storageFileHandle->webdav.curlMultiHandle,storageFileHandle->webdav.curlHandle);
            (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
            (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
            free(storageFileHandle->webdav.receiveBuffer.data);
            return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_multi_strerror(curlMCode));
          }

          // free resources
          String_delete(baseName);
          String_delete(pathName);
          String_delete(baseURL);
        }
      #else /* not HAVE_CURL */
        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        {
          // initialize variables
          storageFileHandle->opticalDisk.read.index             = 0LL;
          storageFileHandle->opticalDisk.read.buffer.blockIndex = 0LL;
          storageFileHandle->opticalDisk.read.buffer.length     = 0L;

          // check if device exists
          if (!File_exists(storageFileHandle->storageSpecifier.deviceName))
          {
            return ERRORX_(OPTICAL_DISK_NOT_FOUND,0,String_cString(storageFileHandle->storageSpecifier.deviceName));
          }

          // open optical disk/ISO 9660 file
          storageFileHandle->opticalDisk.read.iso9660Handle = iso9660_open(String_cString(storageFileHandle->storageSpecifier.deviceName));
          if (storageFileHandle->opticalDisk.read.iso9660Handle == NULL)
          {
            if (File_isFile(storageFileHandle->storageSpecifier.deviceName))
            {
              return ERRORX_(OPEN_ISO9660_FILE,errno,String_cString(storageFileHandle->storageSpecifier.deviceName));
            }
            else
            {
              return ERRORX_(OPEN_OPTICAL_DISK,errno,String_cString(storageFileHandle->storageSpecifier.deviceName));
            }
          }

          // prepare file for reading
          storageFileHandle->opticalDisk.read.iso9660Stat = iso9660_ifs_stat_translate(storageFileHandle->opticalDisk.read.iso9660Handle,
                                                                                       String_cString(storageFileName)
                                                                                      );
          if (storageFileHandle->opticalDisk.read.iso9660Stat == NULL)
          {
            iso9660_close(storageFileHandle->opticalDisk.read.iso9660Handle);
            return ERRORX_(FILE_NOT_FOUND_,errno,String_cString(storageFileName));
          }
        }
      #else /* not HAVE_ISO9660 */
        UNUSED_VARIABLE(storageFileName);
        UNUSED_VARIABLE(storageSpecifier);

        return ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      // init variables

      // open file
#if 0
      error = File_open(&storageFileHandle->fileSystem.fileHandle,
                        storageFileHandle->fileSystem.fileName,
                        FILE_OPEN_READ
                       );
      if (error != ERROR_NONE)
      {
        String_delete(storageFileHandle->fileSystem.fileName);
        String_delete(storageSpecifier);
        return error;
      }
#endif /* 0 */
      UNUSED_VARIABLE(storageFileName);
      UNUSED_VARIABLE(storageSpecifier);

      return ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return ERROR_NONE;
}

void Storage_close(StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      switch (storageFileHandle->mode)
      {
        case STORAGE_MODE_UNKNOWN:
          break;
        case STORAGE_MODE_WRITE:
          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            File_close(&storageFileHandle->fileSystem.fileHandle);
          }
          break;
        case STORAGE_MODE_READ:
          File_close(&storageFileHandle->fileSystem.fileHandle);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL)
        assert(storageFileHandle->ftp.curlHandle != NULL);
        assert(storageFileHandle->ftp.curlMultiHandle != NULL);

        // for some reason signals must be reenabled, otherwise curl is crashing!
        (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_NOSIGNAL,0);
        (void)curl_easy_setopt(storageFileHandle->webdav.curlMultiHandle,CURLOPT_NOSIGNAL,0);

        (void)curl_multi_remove_handle(storageFileHandle->ftp.curlMultiHandle,storageFileHandle->ftp.curlHandle);
        (void)curl_easy_cleanup(storageFileHandle->ftp.curlHandle);
        (void)curl_multi_cleanup(storageFileHandle->ftp.curlMultiHandle);
      #elif defined(HAVE_FTP)
        assert(storageFileHandle->ftp.control != NULL);

        switch (storageFileHandle->mode)
        {
          case STORAGE_MODE_UNKNOWN:
            break;
          case STORAGE_MODE_WRITE:
            if (!storageFileHandle->jobOptions->dryRunFlag)
            {
              assert(storageFileHandle->ftp.data != NULL);
              FtpClose(storageFileHandle->ftp.data);
            }
            break;
          case STORAGE_MODE_READ:
            assert(storageFileHandle->ftp.data != NULL);
            FtpClose(storageFileHandle->ftp.data);
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }
        FtpClose(storageFileHandle->ftp.control);
      #else /* not HAVE_CURL || HAVE_FTP */
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        switch (storageFileHandle->mode)
        {
          case STORAGE_MODE_UNKNOWN:
            break;
          case STORAGE_MODE_WRITE:
            if (!storageFileHandle->jobOptions->dryRunFlag)
            {
              libssh2_channel_send_eof(storageFileHandle->scp.channel);
//???
//              libssh2_channel_wait_eof(storageFileHandle->scp.channel);
              libssh2_channel_wait_closed(storageFileHandle->scp.channel);
              libssh2_channel_free(storageFileHandle->scp.channel);
            }
            break;
          case STORAGE_MODE_READ:
            libssh2_channel_close(storageFileHandle->scp.channel);
            libssh2_channel_wait_closed(storageFileHandle->scp.channel);
            libssh2_channel_free(storageFileHandle->scp.channel);
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }
        Network_disconnect(&storageFileHandle->scp.socketHandle);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        switch (storageFileHandle->mode)
        {
          case STORAGE_MODE_UNKNOWN:
            break;
          case STORAGE_MODE_WRITE:
            if (!storageFileHandle->jobOptions->dryRunFlag)
            {
              libssh2_sftp_close(storageFileHandle->sftp.sftpHandle);
            }
            break;
          case STORAGE_MODE_READ:
            libssh2_sftp_close(storageFileHandle->sftp.sftpHandle);
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
          #endif /* NDEBUG */
        }
        libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
        Network_disconnect(&storageFileHandle->sftp.socketHandle);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      #ifdef HAVE_CURL
        assert(storageFileHandle->webdav.curlHandle != NULL);
        assert(storageFileHandle->webdav.curlMultiHandle != NULL);

        // for some reason signals must be reenabled, otherwise curl is crashing!
        (void)curl_easy_setopt(storageFileHandle->webdav.curlHandle,CURLOPT_NOSIGNAL,0);
        (void)curl_easy_setopt(storageFileHandle->webdav.curlMultiHandle,CURLOPT_NOSIGNAL,0);

        (void)curl_multi_remove_handle(storageFileHandle->webdav.curlMultiHandle,storageFileHandle->webdav.curlHandle);
        (void)curl_easy_cleanup(storageFileHandle->webdav.curlHandle);
        (void)curl_multi_cleanup(storageFileHandle->webdav.curlMultiHandle);
        if (storageFileHandle->webdav.receiveBuffer.data != NULL) free(storageFileHandle->webdav.receiveBuffer.data);
      #else /* not HAVE_CURL */
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      switch (storageFileHandle->mode)
      {
        case STORAGE_MODE_UNKNOWN:
          break;
        case STORAGE_MODE_WRITE:
          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            storageFileHandle->opticalDisk.write.totalSize += File_getSize(&storageFileHandle->opticalDisk.write.fileHandle);
            File_close(&storageFileHandle->opticalDisk.write.fileHandle);
          }
          StringList_append(&storageFileHandle->opticalDisk.write.fileNameList,storageFileHandle->opticalDisk.write.fileName);
          break;
        case STORAGE_MODE_READ:
          #ifdef HAVE_ISO9660
            assert(storageFileHandle->opticalDisk.read.iso9660Handle != NULL);
            assert(storageFileHandle->opticalDisk.read.iso9660Stat != NULL);

            free(storageFileHandle->opticalDisk.read.iso9660Stat);
            iso9660_close(storageFileHandle->opticalDisk.read.iso9660Handle);
          #endif /* HAVE_ISO9660 */
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
      break;
    case STORAGE_TYPE_DEVICE:
      switch (storageFileHandle->mode)
      {
        case STORAGE_MODE_UNKNOWN:
          break;
        case STORAGE_MODE_WRITE:
          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            storageFileHandle->device.totalSize += File_getSize(&storageFileHandle->device.fileHandle);
            File_close(&storageFileHandle->device.fileHandle);
          }
          break;
        case STORAGE_MODE_READ:
          storageFileHandle->device.totalSize += File_getSize(&storageFileHandle->device.fileHandle);
          File_close(&storageFileHandle->device.fileHandle);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
      StringList_append(&storageFileHandle->device.fileNameList,storageFileHandle->device.fileName);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
}

Errors Storage_delete(StorageFileHandle *storageFileHandle,
                      const String      storageFileName
                     )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  error = ERROR_UNKNOWN;
  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_delete(storageFileName,FALSE);
      }
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL)
        {
          CURL              *curlHandle;
          String            pathName,baseName;
          String            url;
          CURLcode          curlCode;
          const char        *plainLoginPassword;
          StringTokenizer   nameTokenizer;
          String            name;
          String            ftpCommand;
          struct curl_slist *curlSList;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // initialize variables

            // open Curl handles
            curlHandle = curl_easy_init();
            if (curlHandle == NULL)
            {
              return ERROR_FTP_SESSION_FAIL;
            }
            (void)curl_easy_setopt(curlHandle,CURLOPT_NOSIGNAL,1);
            (void)curl_easy_setopt(curlHandle,CURLOPT_FTP_RESPONSE_TIMEOUT,FTP_TIMEOUT/1000);
            if (globalOptions.verboseLevel >= 6)
            {
              // enable debug mode
              (void)curl_easy_setopt(curlHandle,CURLOPT_VERBOSE,1L);
            }

            // get pathname, basename
            pathName = File_getFilePathName(String_new(),storageFileName);
            baseName = File_getFileBaseName(String_new(),storageFileName);

            // get URL
            url = String_format(String_new(),"ftp://%S",storageFileHandle->storageSpecifier.hostName);
            if (storageFileHandle->storageSpecifier.hostPort != 0) String_format(url,":d",storageFileHandle->storageSpecifier.hostPort);
            File_initSplitFileName(&nameTokenizer,pathName);
            while (File_getNextSplitFileName(&nameTokenizer,&name))
            {
              String_appendChar(url,'/');
              String_append(url,name);
            }
            File_doneSplitFileName(&nameTokenizer);
            String_append(url,baseName);

            // set FTP connect
            curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
            if (curlCode != CURLE_OK)
            {
              String_delete(url);
              String_delete(baseName);
              String_delete(pathName);
              (void)curl_easy_cleanup(curlHandle);
              return ERRORX_(FTP_SESSION_FAIL,0,curl_easy_strerror(curlCode));
            }

            // set FTP login
            (void)curl_easy_setopt(curlHandle,CURLOPT_USERNAME,String_cString(storageFileHandle->storageSpecifier.loginName));
            plainLoginPassword = Password_deploy(storageFileHandle->storageSpecifier.loginPassword);
            (void)curl_easy_setopt(curlHandle,CURLOPT_PASSWORD,plainLoginPassword);
            Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);

            // delete file
            ftpCommand = String_format(String_new(),"*DELE %S",storageFileName);
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
            curl_slist_free_all(curlSList);
            String_delete(ftpCommand);
            if (curlCode != CURLE_OK)
            {
              String_delete(url);
              String_delete(baseName);
              String_delete(pathName);
              (void)curl_easy_cleanup(curlHandle);
              return ERRORX_(DELETE_FILE,0,curl_multi_strerror(curlCode));
            }

            // free resources
            (void)curl_easy_cleanup(curlHandle);
          }
        }
      #elif defined(HAVE_FTP)
        assert(storageFileHandle->ftp.data != NULL);

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          error = (FtpDelete(String_cString(storageFileName),storageFileHandle->ftp.data) == 1) ? ERROR_NONE : ERROR_DELETE_FILE;
        }
      #else /* not HAVE_CURL || HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
#if 0
whould this be a possible implementation?
        {
          String command;

          assert(storageFileHandle->scp.channel != NULL);

          // there is no unlink command for scp: execute either 'rm' or 'del' on remote server
          command = String_new();
          if (error != ERROR_NONE)
          {
            String_format(String_clear(command),"rm %'S",storageFileName);
            error = (libssh2_channel_exec(storageFileHandle->scp.channel,
                                          String_cString(command)
                                         ) != 0
                    ) ? ERROR_NONE : ERROR_DELETE_FILE;
          }
          if (error != ERROR_NONE)
          {
            String_format(String_clear(command),"del %'S",storageFileName);
            error = (libssh2_channel_exec(storageFileHandle->scp.channel,
                                          String_cString(command)
                                         ) != 0
                    ) ? ERROR_NONE : ERROR_DELETE_FILE;
          }
          String_delete(command);
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
#endif /* 0 */
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        error = Network_connect(&storageFileHandle->sftp.socketHandle,
                                SOCKET_TYPE_SSH,
                                storageFileHandle->storageSpecifier.hostName,
                                storageFileHandle->storageSpecifier.hostPort,
                                storageFileHandle->storageSpecifier.loginName,
                                storageFileHandle->storageSpecifier.loginPassword,
                                storageFileHandle->sftp.sshPublicKeyFileName,
                                storageFileHandle->sftp.sshPrivateKeyFileName,
                                0
                               );
        if (error == ERROR_NONE)
        {
          libssh2_session_set_timeout(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),READ_TIMEOUT);

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // init session
            storageFileHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageFileHandle->sftp.socketHandle));
            if (storageFileHandle->sftp.sftp != NULL)
            {
              // delete file
              if (libssh2_sftp_unlink(storageFileHandle->sftp.sftp,
                                      String_cString(storageFileName)
                                     ) == 0
                 )
              {
                error = ERROR_NONE;
              }
              else
              {
                 char *sshErrorText;

                 libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
                 error = ERRORX_(SSH,
                                 libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                                 sshErrorText
                                );
              }

              libssh2_sftp_shutdown(storageFileHandle->sftp.sftp);
            }
            else
            {
              char *sshErrorText;

              libssh2_session_last_error(Network_getSSHSession(&storageFileHandle->sftp.socketHandle),&sshErrorText,NULL,0);
              error = ERRORX_(SSH,
                              libssh2_session_last_errno(Network_getSSHSession(&storageFileHandle->sftp.socketHandle)),
                              sshErrorText
                             );
              Network_disconnect(&storageFileHandle->sftp.socketHandle);
            }
          }
          Network_disconnect(&storageFileHandle->sftp.socketHandle);
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      #ifdef HAVE_CURL
        {
          CURL              *curlHandle;
          String            baseURL;
          const char        *plainLoginPassword;
          CURLcode          curlCode;
          String            pathName,baseName;
          String            url;
          StringTokenizer   nameTokenizer;
          String            name;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            // initialize variables
            curlHandle = curl_easy_init();
            if (curlHandle == NULL)
            {
              return ERROR_WEBDAV_SESSION_FAIL;
            }
            (void)curl_easy_setopt(curlHandle,CURLOPT_NOSIGNAL,1);
            (void)curl_easy_setopt(curlHandle,CURLOPT_CONNECTTIMEOUT_MS,WEBDAV_TIMEOUT);
            if (globalOptions.verboseLevel >= 6)
            {
              // enable debug mode
              (void)curl_easy_setopt(curlHandle,CURLOPT_VERBOSE,1L);
            }

            // get base URL
            baseURL = String_format(String_new(),"http://%S",storageFileHandle->storageSpecifier.hostName);
            if (storageFileHandle->storageSpecifier.hostPort != 0) String_format(baseURL,":d",storageFileHandle->storageSpecifier.hostPort);

            // set WebDAV connect
            curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(baseURL));
            if (curlCode != CURLE_OK)
            {
              String_delete(baseURL);
              (void)curl_easy_cleanup(curlHandle);
              return ERRORX_(WEBDAV_SESSION_FAIL,0,curl_easy_strerror(curlCode));
            }

            // set WebDAV login
            (void)curl_easy_setopt(curlHandle,CURLOPT_USERNAME,String_cString(storageFileHandle->storageSpecifier.loginName));
            plainLoginPassword = Password_deploy(storageFileHandle->storageSpecifier.loginPassword);
            (void)curl_easy_setopt(curlHandle,CURLOPT_PASSWORD,plainLoginPassword);
            Password_undeploy(storageFileHandle->storageSpecifier.loginPassword);

            // get pathname, basename
            pathName = File_getFilePathName(String_new(),storageFileName);
            baseName = File_getFileBaseName(String_new(),storageFileName);

            // delete file
            url = String_format(String_duplicate(baseURL),"/");
            File_initSplitFileName(&nameTokenizer,pathName);
            while (File_getNextSplitFileName(&nameTokenizer,&name))
            {
              String_append(url,name);
              String_appendChar(url,'/');
            }
            File_doneSplitFileName(&nameTokenizer);
            String_append(url,baseName);
            curlCode = curl_easy_setopt(curlHandle,CURLOPT_NOBODY,1L);
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
              String_delete(url);
              String_delete(baseName);
              String_delete(pathName);
              String_delete(baseURL);
              (void)curl_easy_cleanup(curlHandle);
              return ERRORX_(DELETE_FILE,0,curl_easy_strerror(curlCode));
            }
            String_delete(url);

            // free resources
            String_delete(baseName);
            String_delete(pathName);
            String_delete(baseURL);
            (void)curl_easy_cleanup(curlHandle);
          }
        }
      #else /* not HAVE_CURL */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_DEVICE:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

bool Storage_eof(StorageFileHandle *storageFileHandle)
{
  assert(storageFileHandle != NULL);
  assert(storageFileHandle->mode == STORAGE_MODE_READ);
  assert(storageFileHandle->jobOptions != NULL);

  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        return File_eof(&storageFileHandle->fileSystem.fileHandle);
      }
      else
      {
        return TRUE;
      }
      break;
    case STORAGE_TYPE_FTP:
      #if defined(HAVE_CURL) || defined(HAVE_FTP)
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          return storageFileHandle->ftp.index >= storageFileHandle->ftp.size;
        }
        else
        {
          return TRUE;
        }
      #else /* not HAVE_CURL || HAVE_FTP */
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          return storageFileHandle->scp.index >= storageFileHandle->scp.size;
        }
        else
        {
          return TRUE;
        }
      #else /* not HAVE_SSH2 */
        return TRUE;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          return storageFileHandle->sftp.index >= storageFileHandle->sftp.size;
        }
        else
        {
          return TRUE;
        }
      #else /* not HAVE_SSH2 */
        return TRUE;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      #ifdef HAVE_CURL
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          return storageFileHandle->webdav.index >= storageFileHandle->webdav.size;
        }
        else
        {
          return TRUE;
        }
      #else /* not HAVE_CURL */
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        assert(storageFileHandle->opticalDisk.read.iso9660Handle != NULL);
        assert(storageFileHandle->opticalDisk.read.iso9660Stat != NULL);

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          return storageFileHandle->opticalDisk.read.index >= storageFileHandle->opticalDisk.read.iso9660Stat->size;
        }
        else
        {
          return TRUE;
        }
      #else /* not HAVE_ISO9660 */
        return TRUE;
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        return File_eof(&storageFileHandle->device.fileHandle);
      }
      else
      {
        return TRUE;
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return TRUE;
}

Errors Storage_read(StorageFileHandle *storageFileHandle,
                    void              *buffer,
                    ulong             size,
                    ulong             *bytesRead
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->mode == STORAGE_MODE_READ);
  assert(storageFileHandle->jobOptions != NULL);
  assert(storageFileHandle->mode == STORAGE_MODE_READ);
  assert(buffer != NULL);

//fprintf(stderr,"%s,%d: size=%lu\n",__FILE__,__LINE__,size);
  if (bytesRead != NULL) (*bytesRead) = 0L;
  error = ERROR_NONE;
  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_read(&storageFileHandle->fileSystem.fileHandle,buffer,size,bytesRead);
      }
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL)
        {
          ulong     i;
          ulong     n;
          ulong     length;
          uint64    startTimestamp,endTimestamp;
          CURLMcode curlmCode;
          int       runningHandles;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->ftp.curlMultiHandle != NULL);
            assert(storageFileHandle->ftp.readAheadBuffer.data != NULL);

            // copy as much as available from read-ahead buffer
            if (   (storageFileHandle->ftp.index >= storageFileHandle->ftp.readAheadBuffer.offset)
                && (storageFileHandle->ftp.index < (storageFileHandle->ftp.readAheadBuffer.offset+storageFileHandle->ftp.readAheadBuffer.length))
               )
            {
              // copy data
              i = (ulong)(storageFileHandle->ftp.index-storageFileHandle->ftp.readAheadBuffer.offset);
              n = MIN(size,storageFileHandle->ftp.readAheadBuffer.length-i);
              memcpy(buffer,storageFileHandle->ftp.readAheadBuffer.data+i,n);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+n;
              size -= n;
              if (bytesRead != NULL) (*bytesRead) += n;
              storageFileHandle->ftp.index += (uint64)n;
            }

            // read rest of data
            while (   (size > 0L)
                   && (storageFileHandle->ftp.index < storageFileHandle->ftp.size)
                   && (error == ERROR_NONE)
                  )
            {
              assert(storageFileHandle->ftp.index >= (storageFileHandle->ftp.readAheadBuffer.offset+storageFileHandle->ftp.readAheadBuffer.length));

              // get max. number of bytes to receive in one step
              if (storageFileHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->ftp.bandWidthLimiter.blockSize,size);
              }
              else
              {
                length = size;
              }
              assert(length > 0L);

              // get start time
              startTimestamp = Misc_getTimestamp();

              if (length < MAX_BUFFER_SIZE)
              {
                // read into read-ahead buffer
                storageFileHandle->ftp.buffer          = storageFileHandle->ftp.readAheadBuffer.data;
                storageFileHandle->ftp.length          = MIN((size_t)(storageFileHandle->ftp.size-storageFileHandle->ftp.index),MAX_BUFFER_SIZE);
                storageFileHandle->ftp.transferedBytes = 0L;
                runningHandles = 1;
                while (   (storageFileHandle->ftp.transferedBytes < storageFileHandle->ftp.length)
                       && (error == ERROR_NONE)
                       && (runningHandles > 0)
                      )
                {
                  // wait for socket
                  error = waitCurlSocket(storageFileHandle->ftp.curlMultiHandle);

                  // perform curl action
                  if (error == ERROR_NONE)
                  {
                    do
                    {
                      curlmCode = curl_multi_perform(storageFileHandle->ftp.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: %ld %ld\n",__FILE__,__LINE__,storageFileHandle->ftp.transferedBytes,storageFileHandle->ftp.length);
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
                if (storageFileHandle->ftp.transferedBytes <= 0L)
                {
                  error = ERROR_IO_ERROR;
                  break;
                }
                storageFileHandle->ftp.readAheadBuffer.offset = storageFileHandle->ftp.index;
                storageFileHandle->ftp.readAheadBuffer.length = storageFileHandle->ftp.transferedBytes;

                // copy data
                n = MIN(length,storageFileHandle->ftp.readAheadBuffer.length);
                memcpy(buffer,storageFileHandle->ftp.readAheadBuffer.data,n);

                // adjust buffer, size, bytes read, index
                buffer = (byte*)buffer+n;
                size -= n;
                if (bytesRead != NULL) (*bytesRead) += n;
                storageFileHandle->ftp.index += (uint64)n;
              }
              else
              {
                // read direct
                storageFileHandle->ftp.buffer          = buffer;
                storageFileHandle->ftp.length          = length;
                storageFileHandle->ftp.transferedBytes = 0L;
                runningHandles = 1;
                while (   (storageFileHandle->ftp.transferedBytes < storageFileHandle->ftp.length)
                       && (error == ERROR_NONE)
                       && (runningHandles > 0)
                      )
                {
                  // wait for socket
                  error = waitCurlSocket(storageFileHandle->ftp.curlMultiHandle);

                  // perform curl action
                  if (error == ERROR_NONE)
                  {
                    do
                    {
                      curlmCode = curl_multi_perform(storageFileHandle->ftp.curlMultiHandle,&runningHandles);
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
                if (storageFileHandle->ftp.transferedBytes <= 0L)
                {
                  error = ERROR_IO_ERROR;
                  break;
                }
                n = storageFileHandle->ftp.transferedBytes;

                // adjust buffer, size, bytes read, index
                buffer = (byte*)buffer+n;
                size -= n;
                if (bytesRead != NULL) (*bytesRead) += n;
                storageFileHandle->ftp.index += (uint64)n;
              }

              // get end time
              endTimestamp = Misc_getTimestamp();

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->ftp.bandWidthLimiter,
                               n,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #elif defined(HAVE_FTP)
        {
          ulong   i;
          ulong   n;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          ssize_t readBytes;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->ftp.control != NULL);
            assert(storageFileHandle->ftp.data != NULL);
            assert(storageFileHandle->ftp.readAheadBuffer.data != NULL);

            // copy as much as available from read-ahead buffer
            if (   (storageFileHandle->ftp.index >= storageFileHandle->ftp.readAheadBuffer.offset)
                && (storageFileHandle->ftp.index < (storageFileHandle->ftp.readAheadBuffer.offset+storageFileHandle->ftp.readAheadBuffer.length))
               )
            {
              // copy data
              i = (ulong)(storageFileHandle->ftp.index-storageFileHandle->ftp.readAheadBuffer.offset);
              n = MIN(size,storageFileHandle->ftp.readAheadBuffer.length-i);
              memcpy(buffer,storageFileHandle->ftp.readAheadBuffer.data+i,n);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+n;
              size -= n;
              if (bytesRead != NULL) (*bytesRead) += n;
              storageFileHandle->ftp.index += (uint64)n;
            }

            // read rest of data
            while (   (size > 0L)
                   && (storageFileHandle->ftp.index < storageFileHandle->ftp.size)
                   && (error == ERROR_NONE)
                  )
            {
              assert(storageFileHandle->ftp.index >= (storageFileHandle->ftp.readAheadBuffer.offset+storageFileHandle->ftp.readAheadBuffer.length));

              // get max. number of bytes to receive in one step
              if (storageFileHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->ftp.bandWidthLimiter.blockSize,size);
              }
              else
              {
                length = size;
              }
              assert(length > 0L);

              // get start time
              startTimestamp = Misc_getTimestamp();

              if (length < MAX_BUFFER_SIZE)
              {
                // read into read-ahead buffer
                readBytes = FtpRead(storageFileHandle->ftp.readAheadBuffer.data,
                                    MIN((size_t)(storageFileHandle->ftp.size-storageFileHandle->ftp.index),MAX_BUFFER_SIZE),
                                    storageFileHandle->ftp.data
                                  );
                if (readBytes == 0)
                {
                  // wait a short time for more data
                  Misc_udelay(250*1000);

                  // read into read-ahead buffer
                  readBytes = FtpRead(storageFileHandle->ftp.readAheadBuffer.data,
                                      MIN((size_t)(storageFileHandle->ftp.size-storageFileHandle->ftp.index),MAX_BUFFER_SIZE),
                                      storageFileHandle->ftp.data
                                     );
                }
                if (readBytes <= 0)
                {
                  error = ERROR_(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->ftp.readAheadBuffer.offset = storageFileHandle->ftp.index;
                storageFileHandle->ftp.readAheadBuffer.length = (ulong)readBytes;

                // copy data
                n = MIN(length,storageFileHandle->ftp.readAheadBuffer.length);
                memcpy(buffer,storageFileHandle->ftp.readAheadBuffer.data,n);

                // adjust buffer, size, bytes read, index
                buffer = (byte*)buffer+n;
                size -= n;
                if (bytesRead != NULL) (*bytesRead) += n;
                storageFileHandle->ftp.index += (uint64)n;
              }
              else
              {
                // read direct
                readBytes = FtpRead(buffer,
                                    length,
                                    storageFileHandle->ftp.data
                                   );
                if (readBytes == 0)
                {
                  // wait a short time for more data
                  Misc_udelay(250*1000);

                  // read direct
                  readBytes = FtpRead(buffer,
                                      size,
                                      storageFileHandle->ftp.data
                                     );

                }
                if (readBytes <= 0)
                {
                  error = ERROR(IO_ERROR,errno);
                  break;
                }
                n = (ulong)readBytes;

                // adjust buffer, size, bytes read, index
                buffer = (byte*)buffer+n;
                size -= n;
                if (bytesRead != NULL) (*bytesRead) += n;
                storageFileHandle->ftp.index += (uint64)n;
              }

              // get end time
              endTimestamp = Misc_getTimestamp();

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->ftp.bandWidthLimiter,
                               n,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #else /* not HAVE_CURL || HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          ulong   index;
          ulong   bytesAvail;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          uint64  startTotalReceivedBytes,endTotalReceivedBytes;
          ssize_t n;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->scp.channel != NULL);
            assert(storageFileHandle->scp.readAheadBuffer.data != NULL);

            // copy as much as available from read-ahead buffer
            if (   (storageFileHandle->scp.index >= storageFileHandle->scp.readAheadBuffer.offset)
                && (storageFileHandle->scp.index < (storageFileHandle->scp.readAheadBuffer.offset+storageFileHandle->scp.readAheadBuffer.length))
               )
            {
              // copy data from read-ahead buffer
              index      = (ulong)(storageFileHandle->scp.index-storageFileHandle->scp.readAheadBuffer.offset);
              bytesAvail = MIN(size,storageFileHandle->scp.readAheadBuffer.length-index);
              memcpy(buffer,storageFileHandle->scp.readAheadBuffer.data+index,bytesAvail);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+bytesAvail;
              size -= bytesAvail;
              if (bytesRead != NULL) (*bytesRead) += bytesAvail;
              storageFileHandle->scp.index += (uint64)bytesAvail;
            }

            // read rest of data
            while (size > 0L)
            {
              assert(storageFileHandle->scp.index >= (storageFileHandle->scp.readAheadBuffer.offset+storageFileHandle->scp.readAheadBuffer.length));

              // get max. number of bytes to receive in one step
              if (storageFileHandle->scp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->scp.bandWidthLimiter.blockSize,size);
              }
              else
              {
                length = size;
              }
              assert(length > 0L);

              // get start time, start received bytes
              startTimestamp          = Misc_getTimestamp();
              startTotalReceivedBytes = storageFileHandle->scp.totalReceivedBytes;

              if (length < MAX_BUFFER_SIZE)
              {
                // read into read-ahead buffer
                do
                {
                  n = libssh2_channel_read(storageFileHandle->scp.channel,
                                           (char*)storageFileHandle->scp.readAheadBuffer.data,
                                           MIN((size_t)(storageFileHandle->scp.size-storageFileHandle->scp.index),MAX_BUFFER_SIZE)
                                         );
                  if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100*1000);
                }
                while (n == LIBSSH2_ERROR_EAGAIN);
                if (n < 0)
                {
                  error = ERROR_(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->scp.readAheadBuffer.offset = storageFileHandle->scp.index;
                storageFileHandle->scp.readAheadBuffer.length = (ulong)n;
//fprintf(stderr,"%s,%d: n=%ld storageFileHandle->scp.bufferOffset=%llu storageFileHandle->scp.bufferLength=%lu\n",__FILE__,__LINE__,n,
//storageFileHandle->scp.readAheadBuffer.offset,storageFileHandle->scp.readAheadBuffer.length);

                // copy data from read-ahead buffer
                bytesAvail = MIN(length,storageFileHandle->scp.readAheadBuffer.length);
                memcpy(buffer,storageFileHandle->scp.readAheadBuffer.data,bytesAvail);

                // adjust buffer, size, bytes read, index
                buffer = (byte*)buffer+bytesAvail;
                size -= bytesAvail;
                if (bytesRead != NULL) (*bytesRead) += bytesAvail;
                storageFileHandle->scp.index += (uint64)bytesAvail;
              }
              else
              {
                // read direct
                do
                {
                  n = libssh2_channel_read(storageFileHandle->scp.channel,
                                                   buffer,
                                                   length
                                                  );
                  if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100*1000);
                }
                while (n == LIBSSH2_ERROR_EAGAIN);
                if (n < 0)
                {
                  error = ERROR_(IO_ERROR,errno);
                  break;
                }

                // adjust buffer, size, bytes read, index
                buffer = (byte*)buffer+(ulong)n;
                size -= (ulong)n;
                if (bytesRead != NULL) (*bytesRead) += (ulong)n;
                storageFileHandle->scp.index += (uint64)n;
              }

              // get end time, end received bytes
              endTimestamp          = Misc_getTimestamp();
              endTotalReceivedBytes = storageFileHandle->scp.totalReceivedBytes;
              assert(endTotalReceivedBytes >= startTotalReceivedBytes);

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->scp.bandWidthLimiter,
                               endTotalReceivedBytes-startTotalReceivedBytes,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          ulong   index;
          ulong   bytesAvail;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          uint64  startTotalReceivedBytes,endTotalReceivedBytes;
          ssize_t n;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->sftp.sftpHandle != NULL);
            assert(storageFileHandle->sftp.readAheadBuffer.data != NULL);

            // copy as much as available from read-ahead buffer
            if (   (storageFileHandle->sftp.index >= storageFileHandle->sftp.readAheadBuffer.offset)
                && (storageFileHandle->sftp.index < (storageFileHandle->sftp.readAheadBuffer.offset+storageFileHandle->sftp.readAheadBuffer.length))
               )
            {
              // copy data from read-ahead buffer
              index      = (ulong)(storageFileHandle->sftp.index-storageFileHandle->sftp.readAheadBuffer.offset);
              bytesAvail = MIN(size,storageFileHandle->sftp.readAheadBuffer.length-index);
              memcpy(buffer,storageFileHandle->sftp.readAheadBuffer.data+index,bytesAvail);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+bytesAvail;
              size -= bytesAvail;
              if (bytesRead != NULL) (*bytesRead) += bytesAvail;
              storageFileHandle->sftp.index += bytesAvail;
            }

            // read rest of data
            if (size > 0)
            {
              assert(storageFileHandle->sftp.index >= (storageFileHandle->sftp.readAheadBuffer.offset+storageFileHandle->sftp.readAheadBuffer.length));

              // get max. number of bytes to receive in one step
              if (storageFileHandle->sftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->sftp.bandWidthLimiter.blockSize,size);
              }
              else
              {
                length = size;
              }
              assert(length > 0L);

              // get start time, start received bytes
              startTimestamp          = Misc_getTimestamp();
              startTotalReceivedBytes = storageFileHandle->sftp.totalReceivedBytes;

              #if   defined(HAVE_SSH2_SFTP_SEEK64)
                libssh2_sftp_seek64(storageFileHandle->sftp.sftpHandle,storageFileHandle->sftp.index);
              #elif defined(HAVE_SSH2_SFTP_SEEK2)
                libssh2_sftp_seek2(storageFileHandle->sftp.sftpHandle,storageFileHandle->sftp.index);
              #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
                libssh2_sftp_seek(storageFileHandle->sftp.sftpHandle,storageFileHandle->sftp.index);
              #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */

              if (length <= MAX_BUFFER_SIZE)
              {
                // read into read-ahead buffer
                n = libssh2_sftp_read(storageFileHandle->sftp.sftpHandle,
                                      (char*)storageFileHandle->sftp.readAheadBuffer.data,
                                      MIN((size_t)(storageFileHandle->sftp.size-storageFileHandle->sftp.index),MAX_BUFFER_SIZE)
                                     );
                if (n < 0)
                {
                  error = ERROR_(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->sftp.readAheadBuffer.offset = storageFileHandle->sftp.index;
                storageFileHandle->sftp.readAheadBuffer.length = (ulong)n;
//fprintf(stderr,"%s,%d: n=%ld storageFileHandle->sftp.bufferOffset=%llu storageFileHandle->sftp.bufferLength=%lu\n",__FILE__,__LINE__,n,
//storageFileHandle->sftp.readAheadBuffer.offset,storageFileHandle->sftp.readAheadBuffer.length);

                // copy data from read-ahead buffer
                bytesAvail = MIN(length,storageFileHandle->sftp.readAheadBuffer.length);
                memcpy(buffer,storageFileHandle->sftp.readAheadBuffer.data,bytesAvail);

                // adjust buffer, size, bytes read, index
                if (bytesRead != NULL) (*bytesRead) += bytesAvail;
                storageFileHandle->sftp.index += (uint64)bytesAvail;
              }
              else
              {
                // read direct
                n = libssh2_sftp_read(storageFileHandle->sftp.sftpHandle,
                                      buffer,
                                      length
                                     );
                if (n < 0)
                {
                  error = ERROR_(IO_ERROR,errno);
                  break;
                }

                // adjust buffer, size, bytes read, index
                if (bytesRead != NULL) (*bytesRead) += (ulong)n;
                storageFileHandle->sftp.index += (uint64)n;
              }

              // get end time, end received bytes
              endTimestamp          = Misc_getTimestamp();
              endTotalReceivedBytes = storageFileHandle->sftp.totalReceivedBytes;
              assert(endTotalReceivedBytes >= startTotalReceivedBytes);

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->sftp.bandWidthLimiter,
                               endTotalReceivedBytes-startTotalReceivedBytes,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
#warning todo webdav
      #ifdef HAVE_CURL
        {
          ulong     i;
          ulong     n;
          ulong     length;
          uint64    startTimestamp,endTimestamp;
          CURLMcode curlmCode;
          int       runningHandles;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->webdav.curlMultiHandle != NULL);
            assert(storageFileHandle->webdav.receiveBuffer.data != NULL);

            // copy as much as available from read-ahead buffer
            if (   (storageFileHandle->webdav.index >= storageFileHandle->webdav.receiveBuffer.offset)
                && (storageFileHandle->webdav.index < (storageFileHandle->webdav.receiveBuffer.offset+storageFileHandle->webdav.receiveBuffer.length))
               )
            {
              // copy data
              i = (ulong)(storageFileHandle->webdav.index-storageFileHandle->webdav.receiveBuffer.offset);
              n = MIN(size,storageFileHandle->webdav.receiveBuffer.length-i);
              memcpy(buffer,storageFileHandle->webdav.receiveBuffer.data+i,n);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+n;
              size -= n;
              if (bytesRead != NULL) (*bytesRead) += n;
              storageFileHandle->webdav.index += (uint64)n;
            }

            // read rest of data
            while (   (size > 0L)
                   && (storageFileHandle->webdav.index < storageFileHandle->webdav.size)
                   && (error == ERROR_NONE)
                  )
            {
              assert(storageFileHandle->webdav.index >= (storageFileHandle->webdav.receiveBuffer.offset+storageFileHandle->webdav.receiveBuffer.length));

              // get max. number of bytes to receive in one step
              if (storageFileHandle->webdav.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->webdav.bandWidthLimiter.blockSize,size);
              }
              else
              {
                length = size;
              }
              assert(length > 0L);

              // get start time
              startTimestamp = Misc_getTimestamp();

              // receive data
              storageFileHandle->webdav.receiveBuffer.length = 0L;
              runningHandles = 1;
              while (   (storageFileHandle->webdav.receiveBuffer.length < length)
                     && (error == ERROR_NONE)
                     && (runningHandles > 0)
                    )
              {
                // wait for socket
                error = waitCurlSocket(storageFileHandle->webdav.curlMultiHandle);

                // perform curl action
                if (error == ERROR_NONE)
                {
                  do
                  {
                    curlmCode = curl_multi_perform(storageFileHandle->webdav.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: curlmCode=%d %ld %ld runningHandles=%d\n",__FILE__,__LINE__,curlmCode,storageFileHandle->webdav.sendBuffer.index,storageFileHandle->webdav.sendBuffer.length,runningHandles);
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
              if (storageFileHandle->webdav.receiveBuffer.length < length)
              {
                error = ERROR_IO_ERROR;
                break;
              }
              storageFileHandle->webdav.receiveBuffer.offset = storageFileHandle->webdav.index;

              // copy data
              n = MIN(length,storageFileHandle->webdav.receiveBuffer.length);
              memcpy(buffer,storageFileHandle->webdav.receiveBuffer.data,n);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+n;
              size -= n;
              if (bytesRead != NULL) (*bytesRead) += n;
              storageFileHandle->webdav.index += (uint64)n;

              // get end time
              endTimestamp = Misc_getTimestamp();

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->webdav.bandWidthLimiter,
                               n,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #else /* not HAVE_CURL */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        {
          uint64   blockIndex;
          uint     blockOffset;
          long int readBytes;
          ulong    n;

          assert(storageFileHandle->opticalDisk.read.iso9660Handle != NULL);
          assert(storageFileHandle->opticalDisk.read.iso9660Stat != NULL);

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->opticalDisk.read.buffer.data != NULL);

            while (   (size > 0L)
                   && (storageFileHandle->opticalDisk.read.index < (uint64)storageFileHandle->opticalDisk.read.iso9660Stat->size)
                  )
            {
              // get ISO9660 block index, offset
              blockIndex  = (int64)(storageFileHandle->opticalDisk.read.index/ISO_BLOCKSIZE);
              blockOffset = (uint)(storageFileHandle->opticalDisk.read.index%ISO_BLOCKSIZE);

              if (   (blockIndex != storageFileHandle->opticalDisk.read.buffer.blockIndex)
                  || (blockOffset >= storageFileHandle->opticalDisk.read.buffer.length)
                 )
              {
                // read ISO9660 block
                readBytes = iso9660_iso_seek_read(storageFileHandle->opticalDisk.read.iso9660Handle,
                                                  storageFileHandle->opticalDisk.read.buffer.data,
                                                  storageFileHandle->opticalDisk.read.iso9660Stat->lsn+(lsn_t)blockIndex,
                                                  1 // read 1 block
                                                 );
                if (readBytes < ISO_BLOCKSIZE)
                {
                  error = ERROR_(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->opticalDisk.read.buffer.blockIndex = blockIndex;
                storageFileHandle->opticalDisk.read.buffer.length     = (((blockIndex+1)*ISO_BLOCKSIZE) <= (uint64)storageFileHandle->opticalDisk.read.iso9660Stat->size)
                                                                          ? ISO_BLOCKSIZE
                                                                          : (ulong)(storageFileHandle->opticalDisk.read.iso9660Stat->size%ISO_BLOCKSIZE);
              }

              // copy data
              n = MIN(size,storageFileHandle->opticalDisk.read.buffer.length-blockOffset);
              memcpy(buffer,storageFileHandle->opticalDisk.read.buffer.data+blockOffset,n);

              // adjust buffer, size, bytes read, index
              buffer = (byte*)buffer+n;
              size -= n;
              if (bytesRead != NULL) (*bytesRead) += n;
              storageFileHandle->opticalDisk.read.index += n;
            }
          }
        }
      #else /* not HAVE_ISO9660 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_read(&storageFileHandle->device.fileHandle,buffer,size,bytesRead);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_write(StorageFileHandle *storageFileHandle,
                     const void        *buffer,
                     ulong             size
                    )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->mode == STORAGE_MODE_WRITE);
  assert(storageFileHandle->jobOptions != NULL);
  assert(buffer != NULL);

  error = ERROR_NONE;
  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_write(&storageFileHandle->fileSystem.fileHandle,buffer,size);
      }
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL)
        {
          ulong     writtenBytes;
          ulong     length;
          uint64    startTimestamp,endTimestamp;
          CURLMcode curlmCode;
          int       runningHandles;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->ftp.curlMultiHandle != NULL);

            writtenBytes = 0L;
            while (writtenBytes < size)
            {
              // get max. number of bytes to send in one step
              if (storageFileHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->ftp.bandWidthLimiter.blockSize,size-writtenBytes);
              }
              else
              {
                length = size-writtenBytes;
              }
              assert(length > 0L);

              // get start time
              startTimestamp = Misc_getTimestamp();

              // send data
              storageFileHandle->ftp.buffer          = (void*)buffer;
              storageFileHandle->ftp.length          = length;
              storageFileHandle->ftp.transferedBytes = 0L;
              runningHandles = 1;
              while (   (storageFileHandle->ftp.transferedBytes < storageFileHandle->ftp.length)
                     && (error == ERROR_NONE)
                     && (runningHandles > 0)
                    )
              {
                // wait for socket
                error = waitCurlSocket(storageFileHandle->ftp.curlMultiHandle);

                // perform curl action
                if (error == ERROR_NONE)
                {
                  do
                  {
                    curlmCode = curl_multi_perform(storageFileHandle->ftp.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: %ld %ld\n",__FILE__,__LINE__,storageFileHandle->ftp.transferedBytes,storageFileHandle->ftp.length);
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
              if      (storageFileHandle->ftp.transferedBytes <= 0L)
              {
                error = ERROR_NETWORK_SEND;
                break;
              }
              buffer = (byte*)buffer+storageFileHandle->ftp.transferedBytes;
              writtenBytes += storageFileHandle->ftp.transferedBytes;

              // get end time
              endTimestamp = Misc_getTimestamp();

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->ftp.bandWidthLimiter,
                               storageFileHandle->ftp.transferedBytes,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #elif defined(HAVE_FTP)
        {
          ulong  writtenBytes;
          ulong  length;
          uint64 startTimestamp,endTimestamp;
          long   n;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->ftp.control != NULL);
            assert(storageFileHandle->ftp.data != NULL);

            writtenBytes = 0L;
            while (writtenBytes < size)
            {
              // get max. number of bytes to send in one step
              if (storageFileHandle->ftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->ftp.bandWidthLimiter.blockSize,size-writtenBytes);
              }
              else
              {
                length = size-writtenBytes;
              }
              assert(length > 0L);

              // get start time
              startTimestamp = Misc_getTimestamp();

              // send data
              n = FtpWrite((void*)buffer,length,storageFileHandle->ftp.data);
              if      (n < 0)
              {
                error = ERROR_NETWORK_SEND;
                break;
              }
              else if (n == 0)
              {
                n = FtpWrite((void*)buffer,length,storageFileHandle->ftp.data);
                if      (n <= 0)
                {
                  error = ERROR_NETWORK_SEND;
                  break;
                }
              }
              buffer = (byte*)buffer+(ulong)n;
              writtenBytes += (ulong)n;

              // get end time
              endTimestamp = Misc_getTimestamp();

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->ftp.bandWidthLimiter,
                               (ulong)n,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #else /* not HAVE_CURL || HAVE_FTP */
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        {
          ulong   writtenBytes;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          uint64  startTotalSentBytes,endTotalSentBytes;
          ssize_t n;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->scp.channel != NULL);

            writtenBytes = 0L;
            while (writtenBytes < size)
            {
              // get max. number of bytes to send in one step
              if (storageFileHandle->scp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->scp.bandWidthLimiter.blockSize,size-writtenBytes);
              }
              else
              {
                length = size-writtenBytes;
              }
              assert(length > 0L);

              // workaround for libssh2-problem: it seems sending of blocks >=4k cause problems, e. g. corrupt ssh MAC?
              length = MIN(length,4*1024);

              // get start time, start received bytes
              startTimestamp      = Misc_getTimestamp();
              startTotalSentBytes = storageFileHandle->scp.totalSentBytes;

              // send data
              do
              {
                n = libssh2_channel_write(storageFileHandle->scp.channel,
                                          buffer,
                                          length
                                         );
                if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100*1000);
              }
              while (n == LIBSSH2_ERROR_EAGAIN);

              // get end time, end received bytes
              endTimestamp      = Misc_getTimestamp();
              endTotalSentBytes = storageFileHandle->scp.totalSentBytes;
              assert(endTotalSentBytes >= startTotalSentBytes);

// ??? is it possible in blocking-mode that write() return 0 and this is not an error?
#if 1
              if      (n == 0)
              {
                // should not happen in blocking-mode: bug? libssh2 API changed somewhere between 0.18 and 1.2.4? => wait for data
                if (!waitSSHSessionSocket(&storageFileHandle->scp.socketHandle))
                {
                  error = ERROR_NETWORK_SEND;
                  break;
                }
              }
              else if (n < 0)
              {
                error = ERROR_NETWORK_SEND;
                break;
              }
#else /* 0 */
              if (n <= 0)
              {
                error = ERROR_NETWORK_SEND;
                break;
              }
#endif /* 0 */
              buffer = (byte*)buffer+(ulong)n;
              writtenBytes += (ulong)n;


              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->scp.bandWidthLimiter,
                               endTotalSentBytes-startTotalSentBytes,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          ulong   writtenBytes;
          ulong   length;
          uint64  startTimestamp,endTimestamp;
          uint64  startTotalSentBytes,endTotalSentBytes;
          ssize_t n;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->sftp.sftpHandle != NULL);

            writtenBytes = 0L;
            while (writtenBytes < size)
            {
              // get max. number of bytes to send in one step
              if (storageFileHandle->sftp.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->sftp.bandWidthLimiter.blockSize,size-writtenBytes);
              }
              else
              {
                length = size-writtenBytes;
              }
              assert(length > 0L);

              // get start time, start received bytes
              startTimestamp      = Misc_getTimestamp();
              startTotalSentBytes = storageFileHandle->sftp.totalSentBytes;

              // send data
              do
              {
                n = libssh2_sftp_write(storageFileHandle->sftp.sftpHandle,
                                       buffer,
                                       length
                                      );
                if (n == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100*1000);
              }
              while (n == LIBSSH2_ERROR_EAGAIN);

              // get end time, end received bytes
              endTimestamp      = Misc_getTimestamp();
              endTotalSentBytes = storageFileHandle->sftp.totalSentBytes;
              assert(endTotalSentBytes >= startTotalSentBytes);

// ??? is it possible in blocking-mode that write() return 0 and this is not an error?
#if 1
              if      (n == 0)
              {
                // should not happen in blocking-mode: bug? libssh2 API changed somewhere between 0.18 and 1.2.4? => wait for data
                if (!waitSSHSessionSocket(&storageFileHandle->sftp.socketHandle))
                {
                  error = ERROR_NETWORK_SEND;
                  break;
                }
              }
              else if (n < 0)
              {
                error = ERROR_NETWORK_SEND;
                break;
              }
#else /* 0 */
              if (n <= 0)
              {
                error = ERROR_NETWORK_SEND;
                break;
              }
#endif /* 0 */
              buffer = (byte*)buffer+n;
              size -= n;

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->sftp.bandWidthLimiter,
                               endTotalSentBytes-startTotalSentBytes,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      #ifdef HAVE_CURL
        {
          ulong     writtenBytes;
          ulong     length;
          uint64    startTimestamp,endTimestamp;
          CURLMcode curlmCode;
          int       runningHandles;

          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->webdav.curlMultiHandle != NULL);

            writtenBytes = 0L;
            while (writtenBytes < size)
            {
              // get max. number of bytes to send in one step
              if (storageFileHandle->webdav.bandWidthLimiter.maxBandWidthList != NULL)
              {
                length = MIN(storageFileHandle->webdav.bandWidthLimiter.blockSize,size-writtenBytes);
              }
              else
              {
                length = size-writtenBytes;
              }
              assert(length > 0L);

              // get start time
              startTimestamp = Misc_getTimestamp();

              // send data
              storageFileHandle->webdav.sendBuffer.data   = buffer;
              storageFileHandle->webdav.sendBuffer.index  = 0L;
              storageFileHandle->webdav.sendBuffer.length = length;
              runningHandles = 1;
              while (   (storageFileHandle->webdav.sendBuffer.index < storageFileHandle->webdav.sendBuffer.length)
                     && (error == ERROR_NONE)
                     && (runningHandles > 0)
                    )
              {
                // wait for socket
                error = waitCurlSocket(storageFileHandle->webdav.curlMultiHandle);

                // perform curl action
                if (error == ERROR_NONE)
                {
                  do
                  {
                    curlmCode = curl_multi_perform(storageFileHandle->webdav.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: curlmCode=%d %ld %ld runningHandles=%d\n",__FILE__,__LINE__,curlmCode,storageFileHandle->webdav.sendBuffer.index,storageFileHandle->webdav.sendBuffer.length,runningHandles);
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
              if (storageFileHandle->webdav.sendBuffer.index < storageFileHandle->webdav.sendBuffer.length)
              {
                error = ERROR_NETWORK_SEND;
                break;
              }
              buffer = (byte*)buffer+storageFileHandle->webdav.sendBuffer.length;
              writtenBytes += storageFileHandle->webdav.sendBuffer.length;

              // get end time
              endTimestamp = Misc_getTimestamp();

              /* limit used band width if requested (note: when the system time is
                 changing endTimestamp may become smaller than startTimestamp;
                 thus do not check this with an assert())
              */
              if (endTimestamp >= startTimestamp)
              {
                limitBandWidth(&storageFileHandle->webdav.bandWidthLimiter,
                               storageFileHandle->webdav.sendBuffer.length,
                               endTimestamp-startTimestamp
                              );
              }
            }
          }
        }
      #else /* not HAVE_CURL */
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_write(&storageFileHandle->opticalDisk.write.fileHandle,buffer,size);
      }
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_write(&storageFileHandle->device.fileHandle,buffer,size);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

uint64 Storage_getSize(StorageFileHandle *storageFileHandle)
{
  uint64 size;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  size = 0LL;
  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        size = File_getSize(&storageFileHandle->fileSystem.fileHandle);
      }
      break;
    case STORAGE_TYPE_FTP:
      #if defined(HAVE_CURL) || defined(HAVE_FTP)
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          size = storageFileHandle->ftp.size;
        }
      #else /* not HAVE_CURL || HAVE_FTP */
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          size = storageFileHandle->scp.size;
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          size = storageFileHandle->sftp.size;
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
#warning todo webdav
      #ifdef HAVE_CURL
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          size = storageFileHandle->webdav.size;
        }
      #else /* not HAVE_CURL */
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          size = (uint64)storageFileHandle->opticalDisk.read.iso9660Stat->size;
        }
      #else /* not HAVE_ISO9660 */
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        size = File_getSize(&storageFileHandle->device.fileHandle);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return size;
}

Errors Storage_tell(StorageFileHandle *storageFileHandle,
                    uint64            *offset
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_tell(&storageFileHandle->fileSystem.fileHandle,offset);
      }
      break;
    case STORAGE_TYPE_FTP:
      #if defined(HAVE_CURL) || defined(HAVE_FTP)
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          (*offset) = storageFileHandle->ftp.index;
          error     = ERROR_NONE;
        }
      #else /* not HAVE_CURL || HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          (*offset) = storageFileHandle->scp.index;
          error     = ERROR_NONE;
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          (*offset) = storageFileHandle->sftp.index;
          error     = ERROR_NONE;
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      #ifdef HAVE_CURL
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          (*offset) = storageFileHandle->webdav.index;
          error     = ERROR_NONE;
        }
      #else /* not HAVE_CURL */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          (*offset) = storageFileHandle->opticalDisk.read.index;
          error     = ERROR_NONE;
        }
      #else /* not HAVE_ISO9660 */
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_tell(&storageFileHandle->device.fileHandle,offset);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_seek(StorageFileHandle *storageFileHandle,
                    uint64            offset
                   )
{
  Errors error;

  assert(storageFileHandle != NULL);
  assert(storageFileHandle->jobOptions != NULL);

  error = ERROR_NONE;
  switch (storageFileHandle->storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_seek(&storageFileHandle->fileSystem.fileHandle,offset);
      }
      break;
    case STORAGE_TYPE_FTP:
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
          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->ftp.readAheadBuffer.data != NULL);

            if      (offset > storageFileHandle->ftp.index)
            {
              uint64    skip;
              ulong     i;
              ulong     n;
              CURLMcode curlmCode;
              int       runningHandles;

              // calculate number of bytes to skip
              skip = offset-storageFileHandle->ftp.index;

              while (skip > 0LL)
              {
                // skip data in read-ahead buffer
                if (   (storageFileHandle->ftp.index >= storageFileHandle->ftp.readAheadBuffer.offset)
                    && (storageFileHandle->ftp.index < (storageFileHandle->ftp.readAheadBuffer.offset+storageFileHandle->ftp.readAheadBuffer.length))
                   )
                {
                  i = (ulong)storageFileHandle->ftp.index-storageFileHandle->ftp.readAheadBuffer.offset;
                  n = MIN(skip,storageFileHandle->ftp.readAheadBuffer.length-i);
                  skip -= (uint64)n;
                  storageFileHandle->ftp.index += (uint64)n;
                }

                if (skip > 0LL)
                {
                  assert(storageFileHandle->ftp.index >= (storageFileHandle->ftp.readAheadBuffer.offset+storageFileHandle->ftp.readAheadBuffer.length));

                  // read data into read-ahread buffer
                  storageFileHandle->ftp.buffer          = storageFileHandle->ftp.readAheadBuffer.data;
                  storageFileHandle->ftp.length          = MIN((size_t)(storageFileHandle->ftp.size-storageFileHandle->ftp.index),MAX_BUFFER_SIZE);
                  storageFileHandle->ftp.transferedBytes = 0L;
                  runningHandles = 1;
                  while (   (storageFileHandle->ftp.transferedBytes < storageFileHandle->ftp.length)
                         && (error == ERROR_NONE)
                         && (runningHandles > 0)
                        )
                  {
                    // wait for socket
                    error = waitCurlSocket(storageFileHandle->ftp.curlMultiHandle);

                    // perform curl action
                    if (error == ERROR_NONE)
                    {
                      do
                      {
                        curlmCode = curl_multi_perform(storageFileHandle->ftp.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: %ld %ld\n",__FILE__,__LINE__,storageFileHandle->ftp.transferedBytes,storageFileHandle->ftp.length);
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
                  if (storageFileHandle->ftp.transferedBytes <= 0L)
                  {
                    error = ERROR_IO_ERROR;
                    break;
                  }
                  storageFileHandle->ftp.readAheadBuffer.offset = storageFileHandle->ftp.index;
                  storageFileHandle->ftp.readAheadBuffer.length = (uint64)storageFileHandle->ftp.transferedBytes;
                }
              }
            }
            else if (offset < storageFileHandle->ftp.index)
            {
              error = ERROR_FUNCTION_NOT_SUPPORTED;
            }
          }
        }
      #elif defined(HAVE_FTP)
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          assert(storageFileHandle->ftp.readAheadBuffer.data != NULL);

          if      (offset > storageFileHandle->ftp.index)
          {
            uint64 skip;
            ulong  i;
            ulong  n;
            int    readBytes;

            skip = offset-storageFileHandle->ftp.index;
            while (skip > 0LL)
            {
              // skip data in read-ahead buffer
              if (   (storageFileHandle->ftp.index >= storageFileHandle->ftp.readAheadBuffer.offset)
                  && (storageFileHandle->ftp.index < (storageFileHandle->ftp.readAheadBuffer.offset+storageFileHandle->ftp.readAheadBuffer.length))
                 )
              {
                i = (ulong)storageFileHandle->ftp.index-storageFileHandle->ftp.readAheadBuffer.offset;
                n = MIN(skip,storageFileHandle->ftp.readAheadBuffer.length-i);
                skip -= (uint64)n;
                storageFileHandle->ftp.index += (uint64)n;
              }

              if (skip > 0LL)
              {
                assert(storageFileHandle->ftp.index >= (storageFileHandle->ftp.readAheadBuffer.offset+storageFileHandle->ftp.readAheadBuffer.length));

                // read data
                readBytes = FtpRead(storageFileHandle->ftp.readAheadBuffer.data,
                                    MIN((size_t)skip,MAX_BUFFER_SIZE),
                                    storageFileHandle->ftp.data
                                   );
                if (readBytes == 0)
                {
                  // wait a short time for more data
                  Misc_udelay(250*1000);

                  // read into read-ahead buffer
                  readBytes = FtpRead(storageFileHandle->ftp.readAheadBuffer.data,
                                      MIN((size_t)skip,MAX_BUFFER_SIZE),
                                      storageFileHandle->ftp.data
                                     );
                }
                if (readBytes <= 0)
                {
                  error = ERROR_(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->ftp.readAheadBuffer.offset = storageFileHandle->ftp.index;
                storageFileHandle->ftp.readAheadBuffer.length = (uint64)readBytes;
              }
            }
          }
          else if (offset < storageFileHandle->ftp.index)
          {
            error = ERROR_FUNCTION_NOT_SUPPORTED;
          }
        }
      #else /* not HAVE_CURL || HAVE_FTP */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      #ifdef HAVE_SSH2
        /* scp protocol does not support a seek-function. Thus try to
           read and discard data to position the read index to the
           requested offset.
           Note: this is slow!
        */

        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          assert(storageFileHandle->scp.channel != NULL);
          assert(storageFileHandle->scp.readAheadBuffer.data != NULL);

          if      (offset > storageFileHandle->scp.index)
          {
            uint64  skip;
            uint64  i;
            uint64  n;
            ssize_t readBytes;

            skip = offset-storageFileHandle->scp.index;
            while (skip > 0LL)
            {
              // skip data in read-ahead buffer
              if (   (storageFileHandle->scp.index >= storageFileHandle->scp.readAheadBuffer.offset)
                  && (storageFileHandle->scp.index < (storageFileHandle->scp.readAheadBuffer.offset+storageFileHandle->scp.readAheadBuffer.length))
                 )
              {
                i = storageFileHandle->scp.index-storageFileHandle->scp.readAheadBuffer.offset;
                n = MIN(skip,storageFileHandle->scp.readAheadBuffer.length-i);
                skip -= n;
                storageFileHandle->scp.index += n;
              }

              if (skip > 0LL)
              {
                assert(storageFileHandle->scp.index >= (storageFileHandle->scp.readAheadBuffer.offset+storageFileHandle->scp.readAheadBuffer.length));

                // wait for data
                if (!waitSSHSessionSocket(&storageFileHandle->scp.socketHandle))
                {
                  error = ERROR_(IO_ERROR,errno);
                  break;
                }

                // read data
                readBytes = libssh2_channel_read(storageFileHandle->scp.channel,
                                                 (char*)storageFileHandle->scp.readAheadBuffer.data,
                                                 MIN((size_t)skip,MAX_BUFFER_SIZE)
                                                );
                if (readBytes < 0)
                {
                  error = ERROR_(IO_ERROR,errno);
                  break;
                }
                storageFileHandle->scp.readAheadBuffer.offset = storageFileHandle->scp.index;
                storageFileHandle->scp.readAheadBuffer.length = (uint64)readBytes;
              }
            }
          }
          else if (offset < storageFileHandle->scp.index)
          {
            error = ERROR_FUNCTION_NOT_SUPPORTED;
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          assert(storageFileHandle->sftp.sftpHandle != NULL);
          assert(storageFileHandle->sftp.readAheadBuffer.data != NULL);

          if      (offset > storageFileHandle->sftp.index)
          {
            uint64 skip;
            uint64 i;
            uint64 n;

            skip = offset-storageFileHandle->sftp.index;
            if (skip > 0LL)
            {
              // skip data in read-ahead buffer
              if (   (storageFileHandle->sftp.index >= storageFileHandle->sftp.readAheadBuffer.offset)
                  && (storageFileHandle->sftp.index < (storageFileHandle->sftp.readAheadBuffer.offset+storageFileHandle->sftp.readAheadBuffer.length))
                 )
              {
                i = storageFileHandle->sftp.index-storageFileHandle->sftp.readAheadBuffer.offset;
                n = MIN(skip,storageFileHandle->sftp.readAheadBuffer.length-i);
                skip -= n;
                storageFileHandle->sftp.index += n;
              }

              if (skip > 0LL)
              {
                #if   defined(HAVE_SSH2_SFTP_SEEK64)
                  libssh2_sftp_seek64(storageFileHandle->sftp.sftpHandle,offset);
                #elif defined(HAVE_SSH2_SFTP_SEEK2)
                  libssh2_sftp_seek2(storageFileHandle->sftp.sftpHandle,offset);
                #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
                  libssh2_sftp_seek(storageFileHandle->sftp.sftpHandle,(size_t)offset);
                #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
                storageFileHandle->sftp.index = offset;
              }
            }
          }
          else if (offset < storageFileHandle->sftp.index)
          {
// NYI: ??? support seek backward
            error = ERROR_FUNCTION_NOT_SUPPORTED;
          }
        }
      #else /* not HAVE_SSH2 */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      #ifdef HAVE_CURL
        {
          if (!storageFileHandle->jobOptions->dryRunFlag)
          {
            assert(storageFileHandle->webdav.receiveBuffer.data != NULL);

            if      (offset > storageFileHandle->webdav.index)
            {
              uint64    skip;
              ulong     i;
              ulong     n;
              ulong     length;
              uint64    startTimestamp,endTimestamp;
              CURLMcode curlmCode;
              int       runningHandles;

              // calculate number of bytes to skip
              skip = offset-storageFileHandle->webdav.index;

              while (skip > 0LL)
              {
                // skip data in read-ahead buffer
                if (   (storageFileHandle->webdav.index >= storageFileHandle->webdav.receiveBuffer.offset)
                    && (storageFileHandle->webdav.index < (storageFileHandle->webdav.receiveBuffer.offset+storageFileHandle->webdav.receiveBuffer.length))
                   )
                {
                  i = (ulong)storageFileHandle->webdav.index-storageFileHandle->webdav.receiveBuffer.offset;
                  n = MIN(skip,storageFileHandle->webdav.receiveBuffer.length-i);

                  skip -= (uint64)n;
                  storageFileHandle->webdav.index += (uint64)n;
                }

                if (skip > 0LL)
                {
                  assert(storageFileHandle->webdav.index >= (storageFileHandle->webdav.receiveBuffer.offset+storageFileHandle->webdav.receiveBuffer.length));

                  // get max. number of bytes to receive in one step
                  if (storageFileHandle->webdav.bandWidthLimiter.maxBandWidthList != NULL)
                  {
                    length = MIN(storageFileHandle->webdav.bandWidthLimiter.blockSize,MIN(skip,MAX_BUFFER_SIZE));
                  }
                  else
                  {
                    length = MIN(skip,MAX_BUFFER_SIZE);
                  }
                  assert(length > 0L);

                  // get start time
                  startTimestamp = Misc_getTimestamp();

                  // receive data
                  storageFileHandle->webdav.receiveBuffer.length = 0L;
                  runningHandles = 1;
                  while (   (storageFileHandle->webdav.receiveBuffer.length < length)
                         && (error == ERROR_NONE)
                         && (runningHandles > 0)
                        )
                  {
                    // wait for socket
                    error = waitCurlSocket(storageFileHandle->webdav.curlMultiHandle);

                    // perform curl action
                    if (error == ERROR_NONE)
                    {
                      do
                      {
                        curlmCode = curl_multi_perform(storageFileHandle->webdav.curlMultiHandle,&runningHandles);
//fprintf(stderr,"%s, %d: curlmCode=%d %ld %ld runningHandles=%d\n",__FILE__,__LINE__,curlmCode,storageFileHandle->webdav.receiveBuffer.length,length,runningHandles);
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
                  if (storageFileHandle->webdav.receiveBuffer.length < length)
                  {
                    error = ERROR_IO_ERROR;
                    break;
                  }
                  storageFileHandle->webdav.receiveBuffer.offset = storageFileHandle->webdav.index;

                  // get end time
                  endTimestamp = Misc_getTimestamp();

                  /* limit used band width if requested (note: when the system time is
                     changing endTimestamp may become smaller than startTimestamp;
                     thus do not check this with an assert())
                  */
                  if (endTimestamp >= startTimestamp)
                  {
                    limitBandWidth(&storageFileHandle->webdav.bandWidthLimiter,
                                   length,
                                   endTimestamp-startTimestamp
                                  );
                  }
                }
              }
            }
            else if (   (offset >= storageFileHandle->webdav.receiveBuffer.offset)
                     && (offset < storageFileHandle->webdav.receiveBuffer.offset+(uint64)storageFileHandle->webdav.receiveBuffer.length)
                    )
            {
              storageFileHandle->webdav.index = offset;
            }
            else
            {
              error = ERROR_FUNCTION_NOT_SUPPORTED;
            }
          }
        }
      #else /* not HAVE_CURL */
        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        if (!storageFileHandle->jobOptions->dryRunFlag)
        {
          storageFileHandle->opticalDisk.read.index = offset;
          error = ERROR_NONE;
        }
      #else /* not HAVE_ISO9660 */
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      if (!storageFileHandle->jobOptions->dryRunFlag)
      {
        error = File_seek(&storageFileHandle->device.fileHandle,offset);
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

/*---------------------------------------------------------------------*/

Errors Storage_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                 const String               storageName,
                                 const JobOptions           *jobOptions
                                )
{
  Errors           error;
  StorageSpecifier storageSpecifier;
  String           pathName;

  assert(storageDirectoryListHandle != NULL);
  assert(storageName != NULL);
  assert(jobOptions != NULL);

  // get storage specifier, file name, path name
  Storage_initSpecifier(&storageSpecifier);
  pathName = String_new();
  error = Storage_parseName(storageName,&storageSpecifier,pathName);
  if (error != ERROR_NONE)
  {
    String_delete(pathName);
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }

  // open directory listing
  error = ERROR_UNKNOWN;
  switch (storageSpecifier.type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      UNUSED_VARIABLE(jobOptions);

      // init variables
      storageDirectoryListHandle->type = STORAGE_TYPE_FILESYSTEM;

      // open directory
      error = File_openDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle,
                                     pathName
                                    );
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL)
        {
          FTPServer       ftpServer;
          CURL            *curlHandle;
          String          url;
          CURLcode        curlCode;
          const char      *plainLoginPassword;
          StringTokenizer nameTokenizer;
          String          name;

          // init variables
          storageDirectoryListHandle->type         = STORAGE_TYPE_FTP;
          StringList_init(&storageDirectoryListHandle->ftp.lineList);
          storageDirectoryListHandle->ftp.fileName = String_new();

          // get FTP server settings
          storageDirectoryListHandle->ftp.serverAllocation = getFTPServerSettings(storageSpecifier.hostName,jobOptions,&ftpServer);
          if (String_isEmpty(storageSpecifier.loginName)) String_set(storageSpecifier.loginName,ftpServer.loginName);
          if (String_isEmpty(storageSpecifier.loginName)) String_setCString(storageSpecifier.loginName,getenv("LOGNAME"));
          if (String_isEmpty(storageSpecifier.hostName))
          {
            error = ERROR_NO_HOST_NAME;
            String_delete(storageDirectoryListHandle->ftp.fileName);
            StringList_done(&storageDirectoryListHandle->ftp.lineList);
            break;
          }

          // allocate FTP server connection
          if (!allocateServer(storageDirectoryListHandle->ftp.serverAllocation,SERVER_ALLOCATION_PRIORITY_HIGH,ftpServer.maxConnectionCount))
          {
            error = ERROR_TOO_MANY_CONNECTIONS;
            String_delete(storageDirectoryListHandle->ftp.fileName);
            StringList_done(&storageDirectoryListHandle->ftp.lineList);
            break;
          }

          // check FTP login, get correct password
          error = ERROR_UNKNOWN;
          if ((error != ERROR_NONE) && !Password_isEmpty(storageSpecifier.loginPassword))
          {
            error = checkFTPLogin(storageSpecifier.hostName,
                                  storageSpecifier.hostPort,
                                  storageSpecifier.loginName,
                                  storageSpecifier.loginPassword
                                 );
          }
          if ((error != ERROR_NONE) && !Password_isEmpty(ftpServer.password))
          {
            error = checkFTPLogin(storageSpecifier.hostName,
                                  storageSpecifier.hostPort,
                                  storageSpecifier.loginName,
                                  ftpServer.password
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageSpecifier.loginPassword,ftpServer.password);
            }
          }
          if ((error != ERROR_NONE) && !Password_isEmpty(defaultFTPPassword))
          {
            error = checkFTPLogin(storageSpecifier.hostName,
                                  storageSpecifier.hostPort,
                                  storageSpecifier.loginName,
                                  defaultFTPPassword
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(storageSpecifier.loginPassword,defaultFTPPassword);
            }
          }
          if (error != ERROR_NONE)
          {
            // initialize default password
            if (initFTPPassword(storageSpecifier.hostName,storageSpecifier.loginName,jobOptions))
            {
              error = checkFTPLogin(storageSpecifier.hostName,
                                    storageSpecifier.hostPort,
                                    storageSpecifier.loginName,
                                    defaultFTPPassword
                                   );
              if (error == ERROR_NONE)
              {
                Password_set(storageSpecifier.loginPassword,defaultFTPPassword);
              }
            }
            else
            {
              error = !Password_isEmpty(defaultFTPPassword) ? ERROR_INVALID_FTP_PASSWORD : ERROR_NO_FTP_PASSWORD;
            }
          }
          if (error != ERROR_NONE)
          {
            freeServer(storageDirectoryListHandle->ftp.serverAllocation);
            String_delete(storageDirectoryListHandle->ftp.fileName);
            StringList_done(&storageDirectoryListHandle->ftp.lineList);
            break;
          }

          // init Curl handle
          curlHandle = curl_easy_init();
          if (curlHandle == NULL)
          {
            freeServer(storageDirectoryListHandle->ftp.serverAllocation);
            String_delete(storageDirectoryListHandle->ftp.fileName);
            StringList_done(&storageDirectoryListHandle->ftp.lineList);
            error = ERROR_FTP_SESSION_FAIL;
            break;
          }
          (void)curl_easy_setopt(curlHandle,CURLOPT_NOSIGNAL,1);
          (void)curl_easy_setopt(curlHandle,CURLOPT_FTP_RESPONSE_TIMEOUT,FTP_TIMEOUT/1000);
          if (globalOptions.verboseLevel >= 6)
          {
            (void)curl_easy_setopt(curlHandle,CURLOPT_VERBOSE,1L);
          }

          // get URL
          url = String_format(String_new(),"ftp://%S",storageSpecifier.hostName);
          if (storageSpecifier.hostPort != 0) String_format(url,":d",storageSpecifier.hostPort);
          File_initSplitFileName(&nameTokenizer,pathName);
          while (File_getNextSplitFileName(&nameTokenizer,&name))
          {
            String_appendChar(url,'/');
            String_append(url,name);
          }
          File_doneSplitFileName(&nameTokenizer);

          // set FTP connect
          curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
          if (curlCode != CURLE_OK)
          {
            error = ERRORX_(FTP_SESSION_FAIL,0,curl_easy_strerror(curlCode));
            String_delete(url);
            (void)curl_easy_cleanup(curlHandle);
            freeServer(storageDirectoryListHandle->ftp.serverAllocation);
            String_delete(storageDirectoryListHandle->ftp.fileName);
            StringList_done(&storageDirectoryListHandle->ftp.lineList);
            break;
          }

          // set FTP login
          (void)curl_easy_setopt(curlHandle,CURLOPT_USERNAME,String_cString(storageSpecifier.loginName));
          plainLoginPassword = Password_deploy(storageSpecifier.loginPassword);
          (void)curl_easy_setopt(curlHandle,CURLOPT_PASSWORD,plainLoginPassword);
          Password_undeploy(storageSpecifier.loginPassword);

          // read directory
#warning obsolete?
          if (curlCode != CURLE_OK)
          {
            error = ERRORX_(FTP_SESSION_FAIL,0,curl_easy_strerror(curlCode));
            String_delete(url);
            (void)curl_easy_cleanup(curlHandle);
            freeServer(storageDirectoryListHandle->ftp.serverAllocation);
            String_delete(storageDirectoryListHandle->ftp.fileName);
            StringList_done(&storageDirectoryListHandle->ftp.lineList);
            break;
          }
          curlCode = curl_easy_setopt(curlHandle,CURLOPT_WRITEFUNCTION,curlFTPParseDirectoryListCallback);
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
            error = ERRORX_(FTP_SESSION_FAIL,0,curl_easy_strerror(curlCode));
            String_delete(url);
            (void)curl_easy_cleanup(curlHandle);
            freeServer(storageDirectoryListHandle->ftp.serverAllocation);
            String_delete(storageDirectoryListHandle->ftp.fileName);
            StringList_done(&storageDirectoryListHandle->ftp.lineList);
            break;
          }

          // free resources
          String_delete(url);
          (void)curl_easy_cleanup(curlHandle);
          freeServer(storageDirectoryListHandle->ftp.serverAllocation);
        }
      #elif defined(HAVE_FTP)
        {
          FTPServer  ftpServer;
          const char *plainLoginPassword;
          netbuf     *control;

          // init variables
          storageDirectoryListHandle->type                 = STORAGE_TYPE_FTP;
          storageDirectoryListHandle->ftp.fileListFileName = String_new();
          storageDirectoryListHandle->ftp.line             = String_new();

          // create temporary list file
          error = File_getTmpFileName(storageDirectoryListHandle->ftp.fileListFileName,NULL,tmpDirectory);
          if (error != ERROR_NONE)
          {
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }

          // get FTP server settings
          getFTPServerSettings(hostName,jobOptions,&ftpServer);
          if (String_isEmpty(storageSpecifier.loginName)) String_set(storageSpecifier.loginName,ftpServer.loginName);
          if (String_isEmpty(storageSpecifier.loginName)) String_setCString(storageSpecifier.loginName,getenv("LOGNAME"));
          if (String_isEmpty(storageSpecifier.hostName))
          {
            error = ERROR_NO_HOST_NAME;
            break;
          }

          // check FTP login, get correct password
          error = ERROR_UNKNOWN;
          if ((error != ERROR_NONE) && !Password_isEmpty(loginPassword))
          {
            error = checkFTPLogin(storageSpecifier.hostName,
                                  storageSpecifier.hostPort,
                                  storageSpecifier.loginName,
                                  storageSpecifier.loginPassword
                                 );
          }
          if ((error != ERROR_NONE) && (ftpServer.password != NULL))
          {
            error = checkFTPLogin(storageSpecifier.hostName,
                                  storageSpecifier.hostPort,
                                  storageSpecifier.loginName,
                                  ftpServer.password
                                 );
            if (error == ERROR_NONE)
            {
              Password_set(loginPassword,ftpServer.password);
            }
          }
          if ((error != ERROR_NONE) && !Password_isEmpty(defaultFTPPassword))
          {
            error = checkFTPLogin(storageSpecifier.hostName,
                                  storageSpecifier.hostPort,
                                  storageSpecifier.loginName,
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
            if (initFTPPassword(storageSpecifier.hostName,storageSpecifier.loginName,jobOptions))
            {
              error = checkFTPLogin(storageSpecifier.hostName,
                                    storageSpecifier.hostPort,
                                    storageSpecifier.loginName,
                                    defaultFTPPassword
                                   );
              if (error == ERROR_NONE)
              {
                Password_set(loginPassword,defaultFTPPassword);
              }
            }
            else
            {
              error = !Password_isEmpty(ftpServer.password) ? ERROR_INVALID_FTP_PASSWORD : ERROR_NO_FTP_PASSWORD;
            }
          }
          if (error != ERROR_NONE)
          {
            File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }

          // FTP connect
          if (!Network_hostExists(hostName))
          {
            error = ERRORX_(HOST_NOT_FOUND,0,String_cString(hostName));
            File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }
          if (FtpConnect(String_cString(hostName),&control) != 1)
          {
            error = ERROR_FTP_SESSION_FAIL;
            File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }

          // FTP login
          plainLoginPassword = Password_deploy(storageSpecifier.loginPassword);
          if (FtpLogin(String_cString(storageSpecifierloginName),
                       plainLoginPassword,
                       control
                      ) != 1
             )
          {
            error = ERROR_FTP_AUTHENTICATION;
            Password_undeploy(loginPassword);
            FtpClose(control);
            File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }
          Password_undeploy(loginPassword);

          // read directory: first try non-passive, then passive mode
          FtpOptions(FTPLIB_CONNMODE,FTPLIB_PORT,control);
          if (FtpDir(String_cString(storageDirectoryListHandle->ftp.fileListFileName),String_cString(pathName),control) != 1)
          {
            FtpOptions(FTPLIB_CONNMODE,FTPLIB_PASSIVE,control);
            if (FtpDir(String_cString(storageDirectoryListHandle->ftp.fileListFileName),String_cString(pathName),control) != 1)
            {
              error = ERRORX_(OPEN_DIRECTORY,0,String_cString(pathName));
              FtpClose(control);
              File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
              String_delete(storageDirectoryListHandle->ftp.line);
              String_delete(storageDirectoryListHandle->ftp.fileListFileName);
              break;
            }
          }

          // disconnect
          FtpQuit(control);

          // open list file
          error = File_open(&storageDirectoryListHandle->ftp.fileHandle,storageDirectoryListHandle->ftp.fileListFileName,FILE_OPEN_READ);
          if (error != ERROR_NONE)
          {
            File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
            String_delete(storageDirectoryListHandle->ftp.line);
            String_delete(storageDirectoryListHandle->ftp.fileListFileName);
            break;
          }

          // free resources
        }
      #else /* not HAVE_CURL || HAVE_FTP */
        UNUSED_VARIABLE(jobOptions);

        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
error = ERROR_FUNCTION_NOT_SUPPORTED;
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(jobOptions);

        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
//      error = ERROR_FUNCTION_NOT_SUPPORTED;
//      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          SSHServer sshServer;

          // init variables
          storageDirectoryListHandle->type               = STORAGE_TYPE_SFTP;
          storageDirectoryListHandle->sftp.pathName      = String_new();
          storageDirectoryListHandle->sftp.buffer        = (char*)malloc(MAX_FILENAME_LENGTH);
          if (storageDirectoryListHandle->sftp.buffer == NULL)
          {
            error = ERROR_INSUFFICIENT_MEMORY;
            break;
          }
          storageDirectoryListHandle->sftp.bufferLength  = 0;
          storageDirectoryListHandle->sftp.entryReadFlag = FALSE;

          // set pathname
          String_set(storageDirectoryListHandle->sftp.pathName,pathName);

          // get SSH server settings
          getSSHServerSettings(storageSpecifier.hostName,jobOptions,&sshServer);
          if (String_isEmpty(storageSpecifier.loginName)) String_set(storageSpecifier.loginName,sshServer.loginName);
          if (String_isEmpty(storageSpecifier.loginName)) String_setCString(storageSpecifier.loginName,getenv("LOGNAME"));
          if (String_isEmpty(storageSpecifier.hostName))
          {
            error = ERROR_NO_HOST_NAME;
            break;
          }

          // open network connection
          error = Network_connect(&storageDirectoryListHandle->sftp.socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageSpecifier.hostName,
                                  (storageSpecifier.hostPort != 0) ? storageSpecifier.hostPort : sshServer.port,
                                  storageSpecifier.loginName,
                                  (sshServer.password != NULL) ? sshServer.password : defaultSSHPassword,
                                  sshServer.publicKeyFileName,
                                  sshServer.privateKeyFileName,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            free(storageDirectoryListHandle->sftp.buffer);
            String_delete(storageDirectoryListHandle->sftp.pathName);
            break;
          }
          libssh2_session_set_timeout(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle),READ_TIMEOUT);

          // init SFTP session
          storageDirectoryListHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle));
          if (storageDirectoryListHandle->sftp.sftp == NULL)
          {
            error = ERROR_(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle)));
            Network_disconnect(&storageDirectoryListHandle->sftp.socketHandle);
            free(storageDirectoryListHandle->sftp.buffer);
            String_delete(storageDirectoryListHandle->sftp.pathName);
            break;
          }

          // open directory for reading
          storageDirectoryListHandle->sftp.sftpHandle = libssh2_sftp_opendir(storageDirectoryListHandle->sftp.sftp,
                                                                             String_cString(storageDirectoryListHandle->sftp.pathName)
                                                                            );
          if (storageDirectoryListHandle->sftp.sftpHandle == NULL)
          {
            error = ERROR_(SSH,libssh2_session_last_errno(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle)));
            libssh2_sftp_shutdown(storageDirectoryListHandle->sftp.sftp);
            Network_disconnect(&storageDirectoryListHandle->sftp.socketHandle);
            free(storageDirectoryListHandle->sftp.buffer);
            String_delete(storageDirectoryListHandle->sftp.pathName);
            break;
          }

          // free resources
        }
      #else /* not HAVE_SSH2 */
        UNUSED_VARIABLE(jobOptions);

        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
#warning todo webdav
      #ifdef HAVE_CURL
        {
          WebDAVServer      webdavServer;
          CURL              *curlHandle;
          String            url;
          CURLcode          curlCode;
          const char        *plainLoginPassword;
          StringTokenizer   nameTokenizer;
          String            name;
          String            directoryData;
          struct curl_slist *curlSList;

          // init variables
          storageDirectoryListHandle->type               = STORAGE_TYPE_WEBDAV;
          storageDirectoryListHandle->webdav.rootNode    = NULL;
          storageDirectoryListHandle->webdav.lastNode    = NULL;
          storageDirectoryListHandle->webdav.currentNode = NULL;

          // get WebDAV server settings
          storageDirectoryListHandle->webdav.serverAllocation = getWebDAVServerSettings(storageSpecifier.hostName,jobOptions,&webdavServer);
          if (String_isEmpty(storageSpecifier.loginName)) String_set(storageSpecifier.loginName,webdavServer.loginName);
          if (String_isEmpty(storageSpecifier.loginName)) String_setCString(storageSpecifier.loginName,getenv("LOGNAME"));
          if (String_isEmpty(storageSpecifier.hostName))
          {
            error = ERROR_NO_HOST_NAME;
            break;
          }

          // allocate WebDAV server connection
#warning webdav
          if (!allocateServer(storageDirectoryListHandle->webdav.serverAllocation,SERVER_ALLOCATION_PRIORITY_LOW,webdavServer.maxConnectionCount))
          {
            error = ERROR_TOO_MANY_CONNECTIONS;
            break;
          }

          // check WebDAV login, get correct password
          error = ERROR_UNKNOWN;
          if ((error != ERROR_NONE) && !Password_isEmpty(storageSpecifier.loginPassword))
          {
            error = checkWebDAVLogin(storageSpecifier.hostName,
                                     storageSpecifier.loginName,
                                     storageSpecifier.loginPassword
                                    );
          }
          if ((error != ERROR_NONE) && !Password_isEmpty(webdavServer.password))
          {
            error = checkWebDAVLogin(storageSpecifier.hostName,
                                     storageSpecifier.loginName,
                                     webdavServer.password
                                    );
            if (error == ERROR_NONE)
            {
              Password_set(storageSpecifier.loginPassword,webdavServer.password);
            }
          }
          if ((error != ERROR_NONE) && !Password_isEmpty(defaultWebDAVPassword))
          {
            error = checkWebDAVLogin(storageSpecifier.hostName,
                                     storageSpecifier.loginName,
                                     defaultWebDAVPassword
                                    );
            if (error == ERROR_NONE)
            {
              Password_set(storageSpecifier.loginPassword,defaultWebDAVPassword);
            }
          }
          if (error != ERROR_NONE)
          {
            // initialize default password
            if (initWebDAVPassword(storageSpecifier.hostName,storageSpecifier.loginName,jobOptions))
            {
              error = checkWebDAVLogin(storageSpecifier.hostName,
                                       storageSpecifier.loginName,
                                       defaultWebDAVPassword
                                      );
              if (error == ERROR_NONE)
              {
                Password_set(storageSpecifier.loginPassword,defaultWebDAVPassword);
              }
            }
            else
            {
              error = !Password_isEmpty(defaultWebDAVPassword) ? ERROR_INVALID_WEBDAV_PASSWORD : ERROR_NO_WEBDAV_PASSWORD;
            }
          }
          if (error != ERROR_NONE)
          {
            freeServer(storageDirectoryListHandle->webdav.serverAllocation);
            break;
          }

          // init handle
          curlHandle = curl_easy_init();
          if (curlHandle == NULL)
          {
            freeServer(storageDirectoryListHandle->webdav.serverAllocation);
            error = ERROR_WEBDAV_SESSION_FAIL;
            break;
          }
          (void)curl_easy_setopt(curlHandle,CURLOPT_NOSIGNAL,1);
          (void)curl_easy_setopt(curlHandle,CURLOPT_CONNECTTIMEOUT_MS,WEBDAV_TIMEOUT);
          if (globalOptions.verboseLevel >= 6)
          {
            // enable debug mode
            (void)curl_easy_setopt(curlHandle,CURLOPT_VERBOSE,1L);
          }

          // get URL
          url = String_format(String_new(),"http://%S",storageSpecifier.hostName);
          if (storageSpecifier.hostPort != 0) String_format(url,":d",storageSpecifier.hostPort);
          File_initSplitFileName(&nameTokenizer,pathName);
          while (File_getNextSplitFileName(&nameTokenizer,&name))
          {
            String_appendChar(url,'/');
            String_append(url,name);
          }
          File_doneSplitFileName(&nameTokenizer);
          String_appendChar(url,'/');

          // set WebDAV connect
          curlCode = curl_easy_setopt(curlHandle,CURLOPT_URL,String_cString(url));
          if (curlCode != CURLE_OK)
          {
            error = ERRORX_(WEBDAV_SESSION_FAIL,0,curl_easy_strerror(curlCode));
            String_delete(url);
            (void)curl_easy_cleanup(curlHandle);
            freeServer(storageDirectoryListHandle->webdav.serverAllocation);
            break;
          }

          // set login
          (void)curl_easy_setopt(curlHandle,CURLOPT_USERNAME,String_cString(storageSpecifier.loginName));
          plainLoginPassword = Password_deploy(storageSpecifier.loginPassword);
          (void)curl_easy_setopt(curlHandle,CURLOPT_PASSWORD,plainLoginPassword);
          Password_undeploy(storageSpecifier.loginPassword);

          // read directory data
          directoryData = String_new();
          curlSList = curl_slist_append(NULL,"Depth: 1");
#warning webdav
          curlCode = curl_easy_setopt(curlHandle,CURLOPT_CUSTOMREQUEST,"PROPFIND");
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
            curl_slist_free_all(curlSList);
            String_delete(url);
            (void)curl_easy_cleanup(curlHandle);
            freeServer(storageDirectoryListHandle->webdav.serverAllocation);
            break;
          }
//fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,String_cString(directoryData));

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
            freeServer(storageDirectoryListHandle->webdav.serverAllocation);
            break;
          }
          storageDirectoryListHandle->webdav.lastNode = storageDirectoryListHandle->webdav.rootNode;

          // free resources
          String_delete(directoryData);
          String_delete(url);
          (void)curl_easy_cleanup(curlHandle);
          freeServer(storageDirectoryListHandle->webdav.serverAllocation);
        }
      #else /* not HAVE_CURL */
        UNUSED_VARIABLE(jobOptions);

        error = ERROR_FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      // init variables
      switch (storageSpecifier.type)
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

      #ifdef HAVE_ISO9660
        UNUSED_VARIABLE(jobOptions);

        storageDirectoryListHandle->opticalDisk.pathName = String_duplicate(pathName);

        // check if device exists
        if (!File_exists(storageName))
        {
          return ERRORX_(OPTICAL_DISK_NOT_FOUND,0,String_cString(storageName));
        }

        // open optical disk/ISO 9660 file
        storageDirectoryListHandle->opticalDisk.iso9660Handle = iso9660_open(String_cString(storageName));
        if (storageDirectoryListHandle->opticalDisk.iso9660Handle == NULL)
        {
          if (File_isFile(storageName))
          {
            error = ERRORX_(OPEN_ISO9660_FILE,errno,String_cString(storageName));
          }
          else
          {
            error = ERRORX_(OPEN_OPTICAL_DISK,errno,String_cString(storageName));
          }
        }

        // open directory for reading
        storageDirectoryListHandle->opticalDisk.cdioList = iso9660_ifs_readdir(storageDirectoryListHandle->opticalDisk.iso9660Handle,
                                                                               String_cString(pathName)
                                                                              );
        if (storageDirectoryListHandle->opticalDisk.cdioList == NULL)
        {
          iso9660_close(storageDirectoryListHandle->opticalDisk.iso9660Handle);
          return ERRORX_(FILE_NOT_FOUND_,errno,String_cString(pathName));
        }

        storageDirectoryListHandle->opticalDisk.cdioNextNode = _cdio_list_begin(storageDirectoryListHandle->opticalDisk.cdioList);
      #else /* not HAVE_ISO9660 */
        // open directory
        error = File_openDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle,
                                       storageName
                                      );
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      UNUSED_VARIABLE(jobOptions);

      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  // free resources
  String_delete(pathName);
  Storage_doneSpecifier(&storageSpecifier);

  return error;
}

void Storage_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);

  switch (storageDirectoryListHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      File_closeDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle);
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL)
        String_delete(storageDirectoryListHandle->ftp.fileName);
        StringList_done(&storageDirectoryListHandle->ftp.lineList);
      #elif defined(HAVE_FTP)
        File_close(&storageDirectoryListHandle->ftp.fileHandle);
        File_delete(storageDirectoryListHandle->ftp.fileListFileName,FALSE);
        String_delete(storageDirectoryListHandle->ftp.line);
        String_delete(storageDirectoryListHandle->ftp.fileListFileName);
      #else /* not HAVE_CURL || HAVE_FTP */
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      HALT_INTERNAL_ERROR("scp does not support directory operations");
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        libssh2_sftp_closedir(storageDirectoryListHandle->sftp.sftpHandle);
        libssh2_sftp_shutdown(storageDirectoryListHandle->sftp.sftp);
        Network_disconnect(&storageDirectoryListHandle->sftp.socketHandle);
        free(storageDirectoryListHandle->sftp.buffer);
        String_delete(storageDirectoryListHandle->sftp.pathName);
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      #ifdef HAVE_CURL
        mxmlDelete(storageDirectoryListHandle->webdav.rootNode);
      #else /* not HAVE_CURL */
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        _cdio_list_free(storageDirectoryListHandle->opticalDisk.cdioList,true);
        iso9660_close(storageDirectoryListHandle->opticalDisk.iso9660Handle);
        String_delete(storageDirectoryListHandle->opticalDisk.pathName);
      #else /* not HAVE_ISO9660 */
        File_closeDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle);
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }
}

bool Storage_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  bool endOfDirectoryFlag;

  assert(storageDirectoryListHandle != NULL);

  endOfDirectoryFlag = TRUE;
  switch (storageDirectoryListHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      endOfDirectoryFlag = File_endOfDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle);
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL)
        {
          String line;

          while (!storageDirectoryListHandle->ftp.entryReadFlag && !StringList_isEmpty(&storageDirectoryListHandle->ftp.lineList))
          {
            // get next line
            line = StringList_getFirst(&storageDirectoryListHandle->ftp.lineList,NULL);

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
        }
      #elif defined(HAVE_FTP)
        {
          String line;

          line = String_new();
          while (!storageDirectoryListHandle->ftp.entryReadFlag && !StringList_isEmpty(&storageDirectoryListHandle->ftp.lineList))
          {
            // read line
            error = File_readLine(&storageDirectoryListHandle->ftp.fileHandle,line);
            if (error != ERROR_NONE)
            {
              break;
            }

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
          }
          String_delete(line);

          endOfDirectoryFlag = !storageDirectoryListHandle->ftp.entryReadFlag;
        }
      #else /* not HAVE_CURL || HAVE_FTP */
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      HALT_INTERNAL_ERROR("scp does not support directory operations");
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          int n;

          // read entry iff not already read
          while (!storageDirectoryListHandle->sftp.entryReadFlag)
          {
            n = libssh2_sftp_readdir(storageDirectoryListHandle->sftp.sftpHandle,
                                     storageDirectoryListHandle->sftp.buffer,
                                     MAX_FILENAME_LENGTH,
                                     &storageDirectoryListHandle->sftp.attributes
                                    );
            if (n > 0)
            {
              if (   !S_ISDIR(storageDirectoryListHandle->sftp.attributes.flags)
                  && ((n != 1) || (strncmp(storageDirectoryListHandle->sftp.buffer,".", 1) != 0))
                  && ((n != 2) || (strncmp(storageDirectoryListHandle->sftp.buffer,"..",2) != 0))
                 )
              {
                storageDirectoryListHandle->sftp.bufferLength = n;
                storageDirectoryListHandle->sftp.entryReadFlag = TRUE;
              }
            }
            else
            {
              break;
            }
          }

          endOfDirectoryFlag = !storageDirectoryListHandle->sftp.entryReadFlag;
        }
      #else /* not HAVE_SSH2 */
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
#warning todo webdav
      #ifdef HAVE_CURL
        {
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
        }
      #else /* not HAVE_CURL */
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        endOfDirectoryFlag = (storageDirectoryListHandle->opticalDisk.cdioNextNode == NULL);
      #else /* not HAVE_ISO9660 */
        endOfDirectoryFlag = File_endOfDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle);
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      endOfDirectoryFlag = TRUE;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return endOfDirectoryFlag;
}

Errors Storage_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                 String                     fileName,
                                 FileInfo                   *fileInfo
                                )
{
  Errors error;

  assert(storageDirectoryListHandle != NULL);

  error = ERROR_NONE;
  switch (storageDirectoryListHandle->type)
  {
    case STORAGE_TYPE_NONE:
      break;
    case STORAGE_TYPE_FILESYSTEM:
      error = File_readDirectoryList(&storageDirectoryListHandle->fileSystem.directoryListHandle,fileName);
      if (error == ERROR_NONE)
      {
        if (fileInfo != NULL)
        {
          error = File_getFileInfo(fileInfo,fileName);
        }
      }
      break;
    case STORAGE_TYPE_FTP:
      #if   defined(HAVE_CURL)
        {
          String line;

          if (!storageDirectoryListHandle->ftp.entryReadFlag)
          {
            while (!storageDirectoryListHandle->ftp.entryReadFlag && !StringList_isEmpty(&storageDirectoryListHandle->ftp.lineList))
            {
              // get next line
              line = StringList_getFirst(&storageDirectoryListHandle->ftp.lineList,NULL);

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
        }
      #elif defined(HAVE_FTP)
        {
          String line;

          if (!storageDirectoryListHandle->ftp.entryReadFlag)
          {
            line = String_new();
            while (!storageDirectoryListHandle->ftp.entryReadFlag && !StringList_isEmpty(&storageDirectoryListHandle->ftp.lineList))
            {
              // read line
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
            }
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
        }
      #else /* not HAVE_CURL || HAVE_FTP */
        error = FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL || HAVE_FTP */
      break;
    case STORAGE_TYPE_SSH:
      #ifdef HAVE_SSH2
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
      #else /* not HAVE_SSH2 */
        error = FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_SCP:
      HALT_INTERNAL_ERROR("scp does not support directory operations");
      break;
    case STORAGE_TYPE_SFTP:
      #ifdef HAVE_SSH2
        {
          int n;

          if (!storageDirectoryListHandle->sftp.entryReadFlag)
          {
            do
            {
              n = libssh2_sftp_readdir(storageDirectoryListHandle->sftp.sftpHandle,
                                       storageDirectoryListHandle->sftp.buffer,
                                       MAX_FILENAME_LENGTH,
                                       &storageDirectoryListHandle->sftp.attributes
                                      );
              if      (n > 0)
              {
                if (   !S_ISDIR(storageDirectoryListHandle->sftp.attributes.flags)
                    && ((n != 1) || (strncmp(storageDirectoryListHandle->sftp.buffer,".", 1) != 0))
                    && ((n != 2) || (strncmp(storageDirectoryListHandle->sftp.buffer,"..",2) != 0))
                   )
                {
                  storageDirectoryListHandle->sftp.entryReadFlag = TRUE;
                  error = ERROR_NONE;
                }
              }
              else
              {
                error = ERROR_(IO_ERROR,errno);
              }
            }
            while (!storageDirectoryListHandle->sftp.entryReadFlag && (error == ERROR_NONE));
          }
          else
          {
            error = ERROR_NONE;
          }

          if (storageDirectoryListHandle->sftp.entryReadFlag)
          {
            String_set(fileName,storageDirectoryListHandle->sftp.pathName);
            File_appendFileNameBuffer(fileName,storageDirectoryListHandle->sftp.buffer,storageDirectoryListHandle->sftp.bufferLength);

            if (fileInfo != NULL)
            {
              if      (S_ISREG(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_FILE;
              }
              else if (S_ISDIR(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_DIRECTORY;
              }
              #ifdef HAVE_S_ISLNK
              else if (S_ISLNK(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_LINK;
              }
              #endif /* HAVE_S_ISLNK */
              else if (S_ISCHR(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_SPECIAL;
                fileInfo->specialType = FILE_SPECIAL_TYPE_CHARACTER_DEVICE;
              }
              else if (S_ISBLK(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_SPECIAL;
                fileInfo->specialType = FILE_SPECIAL_TYPE_BLOCK_DEVICE;
              }
              else if (S_ISFIFO(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_SPECIAL;
                fileInfo->specialType = FILE_SPECIAL_TYPE_FIFO;
              }
              #ifdef HAVE_S_ISSOCK
              else if (S_ISSOCK(storageDirectoryListHandle->sftp.attributes.permissions))
              {
                fileInfo->type        = FILE_TYPE_SPECIAL;
                fileInfo->specialType = FILE_SPECIAL_TYPE_SOCKET;
              }
              #endif /* HAVE_S_ISSOCK */
              else
              {
                fileInfo->type        = FILE_TYPE_UNKNOWN;
              }
              fileInfo->size            = storageDirectoryListHandle->sftp.attributes.filesize;
              fileInfo->timeLastAccess  = storageDirectoryListHandle->sftp.attributes.atime;
              fileInfo->timeModified    = storageDirectoryListHandle->sftp.attributes.mtime;
              fileInfo->timeLastChanged = 0LL;
              fileInfo->userId          = storageDirectoryListHandle->sftp.attributes.uid;
              fileInfo->groupId         = storageDirectoryListHandle->sftp.attributes.gid;
              fileInfo->permission      = storageDirectoryListHandle->sftp.attributes.permissions;
              fileInfo->major           = 0;
              fileInfo->minor           = 0;
              memset(fileInfo->cast,0,sizeof(FileCast));
            }

            storageDirectoryListHandle->sftp.entryReadFlag = FALSE;
          }
        }
      #else /* not HAVE_SSH2 */
        error = FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_SSH2 */
      break;
    case STORAGE_TYPE_WEBDAV:
      #ifdef HAVE_CURL
        {
          mxml_node_t *node;

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

          if (storageDirectoryListHandle->webdav.currentNode != NULL)
          {
            // get file name
            assert(storageDirectoryListHandle->webdav.currentNode->type == MXML_ELEMENT);
            assert(storageDirectoryListHandle->webdav.currentNode->child != NULL);
            assert(storageDirectoryListHandle->webdav.currentNode->child->type == MXML_OPAQUE);
            assert(storageDirectoryListHandle->webdav.currentNode->child->value.opaque != NULL);
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
                                     "lp1:getlastmodified",
                                     NULL,
                                     NULL,
                                     MXML_DESCEND
                                    );
              if (node != NULL)
              {
                assert(node->type == MXML_ELEMENT);
                assert(node->child != NULL);
                assert(node->child->type == MXML_OPAQUE);
                assert(node->child->value.opaque != NULL);

                fileInfo->timeModified = Misc_parseDateTime(node->child->value.opaque);
              }
            }

            // next file
            storageDirectoryListHandle->webdav.lastNode    = storageDirectoryListHandle->webdav.currentNode;
            storageDirectoryListHandle->webdav.currentNode = NULL;
//fprintf(stderr,"%s, %d: fileName=%s\n",__FILE__,__LINE__,String_cString(fileName));

            error = ERROR_NONE;
          }
          else
          {
            error = ERROR_READ_DIRECTORY;
          }
        }
      #else /* not HAVE_CURL */
        error = FUNCTION_NOT_SUPPORTED;
      #endif /* HAVE_CURL */
      break;
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      #ifdef HAVE_ISO9660
        {
          iso9660_stat_t *iso9660Stat;
          char           *s;

          if (storageDirectoryListHandle->opticalDisk.cdioNextNode != NULL)
          {
            iso9660Stat = (iso9660_stat_t*)_cdio_list_node_data(storageDirectoryListHandle->opticalDisk.cdioNextNode);
            assert(iso9660Stat != NULL);

            s = (char*)malloc(strlen(iso9660Stat->filename)+1);
            if (s == NULL)
            {
              error = ERROR_INSUFFICIENT_MEMORY;
              break;
            }
            iso9660_name_translate(iso9660Stat->filename,s);
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
              memset(fileInfo->cast,0,sizeof(FileCast));
            }

            storageDirectoryListHandle->opticalDisk.cdioNextNode = _cdio_list_node_next(storageDirectoryListHandle->opticalDisk.cdioNextNode);
          }
          else
          {
            error = ERROR_END_OF_DIRECTORY;
          }
        }
      #else /* not HAVE_ISO9660 */
        error = File_readDirectoryList(&storageDirectoryListHandle->opticalDisk.directoryListHandle,fileName);
      #endif /* HAVE_ISO9660 */
      break;
    case STORAGE_TYPE_DEVICE:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return error;
}

Errors Storage_copy(const StorageSpecifier       *storageSpecifier,
                    const String                 storageFileName,
                    const JobOptions             *jobOptions,
                    BandWidthList                *maxBandWidthList,
                    StorageRequestVolumeFunction storageRequestVolumeFunction,
                    void                         *storageRequestVolumeUserData,
                    StorageStatusInfoFunction    storageStatusInfoFunction,
                    void                         *storageStatusInfoUserData,
                    const String                 localFileName
                   )
{
  String            fileName;
  void              *buffer;
  Errors            error;
  StorageFileHandle storageFileHandle;
  FileHandle        fileHandle;
  ulong             bytesRead;

  assert(storageSpecifier != NULL);
  assert(storageFileName != NULL);
  assert(jobOptions != NULL);
  assert(localFileName != NULL);

  // initialize variables
  buffer   = (byte*)malloc(BUFFER_SIZE);
  if (buffer == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // open storage
  fileName = String_new();
  error = Storage_init(&storageFileHandle,
                       storageSpecifier,
                       storageFileName,
                       jobOptions,
                       maxBandWidthList,
                       CALLBACK(storageRequestVolumeFunction,storageRequestVolumeUserData),
                       CALLBACK(storageStatusInfoFunction,storageStatusInfoUserData)
                      );
  if (error != ERROR_NONE)
  {
    String_delete(fileName);
    free(buffer);
    return error;
  }
  error = Storage_open(&storageFileHandle,
                       storageSpecifier,
                       storageFileName
                      );
  if (error != ERROR_NONE)
  {
    (void)Storage_done(&storageFileHandle);
    String_delete(fileName);
    free(buffer);
    return error;
  }

  // create local file
  error = File_open(&fileHandle,
                    localFileName,
                    FILE_OPEN_CREATE
                   );
  if (error != ERROR_NONE)
  {
    Storage_close(&storageFileHandle);
    (void)Storage_done(&storageFileHandle);
    String_delete(fileName);
    free(buffer);
    return error;
  }

  // copy data from archive to local file
  while (   (error == ERROR_NONE)
         && !Storage_eof(&storageFileHandle)
        )
  {
    error = Storage_read(&storageFileHandle,
                         buffer,
                         BUFFER_SIZE,
                         &bytesRead
                        );
    if (error == ERROR_NONE)
    {
      error = File_write(&fileHandle,
                         buffer,
                         bytesRead
                        );
    }
  }
  if (error != ERROR_NONE)
  {
    File_close(&fileHandle);
    (void)File_delete(localFileName,FALSE);
    Storage_close(&storageFileHandle);
    (void)Storage_done(&storageFileHandle);
    String_delete(fileName);
    free(buffer);
    return error;
  }

  // close local file
  File_close(&fileHandle);

  // close archive
  Storage_close(&storageFileHandle);
  (void)Storage_done(&storageFileHandle);
  String_delete(fileName);

  // free resources
  free(buffer);

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
