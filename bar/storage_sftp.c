/***********************************************************************\
*
* Contents: storage SFTP functions
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
#ifdef HAVE_SSH2
  #include <libssh2.h>
  #include <libssh2_sftp.h>
#endif /* HAVE_SSH2 */
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
/* file data buffer size */
#define BUFFER_SIZE (64*1024)

#define INITIAL_BUFFER_SIZE   (64*1024)
#define INCREMENT_BUFFER_SIZE ( 8*1024)
#define MAX_BUFFER_SIZE       (64*1024)
#define MAX_FILENAME_LENGTH   ( 8*1024)

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

// ----------------------------------------------------------------------

#ifdef HAVE_SSH2
/***********************************************************************\
* Name   : sftpGetPermissions
* Purpose: get SFTP permissions
* Input  : filePermissions - file permissions
* Output : -
* Return : SFTP permissions
* Notes  : -
\***********************************************************************/

LOCAL long sftpGetPermissions(FilePermissions filePermissions)
{
  long permissions;

  permissions = 0;
  if (IS_SET(filePermissions,FILE_PERMISSION_USER_READ    )) permissions |= LIBSSH2_SFTP_S_IRUSR;
  if (IS_SET(filePermissions,FILE_PERMISSION_USER_WRITE   )) permissions |= LIBSSH2_SFTP_S_IWUSR;
  if (IS_SET(filePermissions,FILE_PERMISSION_USER_EXECUTE )) permissions |= LIBSSH2_SFTP_S_IXUSR;
  if (IS_SET(filePermissions,FILE_PERMISSION_USER_ACCESS  )) permissions |= LIBSSH2_SFTP_S_IXUSR;
  if (IS_SET(filePermissions,FILE_PERMISSION_GROUP_READ   )) permissions |= LIBSSH2_SFTP_S_IRGRP;
  if (IS_SET(filePermissions,FILE_PERMISSION_GROUP_WRITE  )) permissions |= LIBSSH2_SFTP_S_IWGRP;
  if (IS_SET(filePermissions,FILE_PERMISSION_GROUP_EXECUTE)) permissions |= LIBSSH2_SFTP_S_IXGRP;
  if (IS_SET(filePermissions,FILE_PERMISSION_GROUP_ACCESS )) permissions |= LIBSSH2_SFTP_S_IXGRP;
  if (IS_SET(filePermissions,FILE_PERMISSION_OTHER_READ   )) permissions |= LIBSSH2_SFTP_S_IROTH;
  if (IS_SET(filePermissions,FILE_PERMISSION_OTHER_WRITE  )) permissions |= LIBSSH2_SFTP_S_IWOTH;
  if (IS_SET(filePermissions,FILE_PERMISSION_OTHER_EXECUTE)) permissions |= LIBSSH2_SFTP_S_IXOTH;
  if (IS_SET(filePermissions,FILE_PERMISSION_OTHER_ACCESS )) permissions |= LIBSSH2_SFTP_S_IXOTH;

  return permissions;
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
* Notes  : parameters are hidden in LIBSSH2_SEND_FUNC()!
\***********************************************************************/

LOCAL LIBSSH2_SEND_FUNC(sftpSendCallback)
{
  StorageHandle *storageHandle;
  ssize_t       n;

  assert(abstract != NULL);

  storageHandle = *((StorageHandle**)abstract);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(storageHandle->sftp.oldSendCallback != NULL);

  n = storageHandle->sftp.oldSendCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageHandle->sftp.totalSentBytes += (uint64)n;

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
* Notes  : parameters are hidden in LIBSSH2_RECV_FUNC()!
\***********************************************************************/

LOCAL LIBSSH2_RECV_FUNC(sftpReceiveCallback)
{
  StorageHandle *storageHandle;
  ssize_t       n;

  assert(abstract != NULL);

  storageHandle = *((StorageHandle**)abstract);
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(storageHandle->sftp.oldReceiveCallback != NULL);

  n = storageHandle->sftp.oldReceiveCallback(socket,buffer,length,flags,abstract);
  if (n > 0) storageHandle->sftp.totalReceivedBytes += (uint64)n;

  return n;
}

/***********************************************************************\
* Name   : sftpRead
* Purpose: sftp read data
* Input  : socketHandle - socket handle
*          sftp         - sftp session
*          sftpHandle   - sftp handle
*          buffer       - data
*          bufferSize   - size of data
* Output : readBytes - number of bytes read
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors sftpRead(SocketHandle        *socketHandle,
                      LIBSSH2_SFTP        *sftp,
                      LIBSSH2_SFTP_HANDLE *sftpHandle,
                      void                *buffer,
                      ulong               bufferSize,
                      ulong               *bytesRead
                     )
{
  Errors  error;
  #ifdef HAVE_SSH2
    ssize_t n;
  #endif /* HAVE_SSH2 */

  assert(socketHandle != NULL);
  assert(sftp != NULL);
  assert(sftpHandle != NULL);
  assert(buffer != NULL);
  assert(bytesRead != NULL);

  #ifdef HAVE_SSH2
    error = ERROR_UNKNOWN;
    ssize_t retryCount = 5;
    do
    {
      n = libssh2_sftp_read(sftpHandle,
                            (char*)buffer,
                            bufferSize
                           );
      if      (n == 0)
      {
        retryCount--;
        // should not happen in blocking-mode: bug? libssh2 API changed somewhere between 0.18 and 1.2.4? => wait for data
        if (!waitSSHSessionSocket(socketHandle))
        {
          error = ERROR_NETWORK_SEND;
        }
      }
      else if (n == LIBSSH2_ERROR_EAGAIN)
      {
        retryCount--;
        if (retryCount >= 0)
        {
          Misc_mdelay(100);
        }
      }
    }
    while ((n == LIBSSH2_ERROR_EAGAIN) && (retryCount >= 0));
    if      (n >= 0)
    {
      (*bytesRead) = (ulong)n;
      error        = ERROR_NONE;
    }
    else if (n == LIBSSH2_ERROR_SFTP_PROTOCOL)
    {
      char *ssh2ErrorText;

      libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
      error = ERRORX_(IO,libssh2_sftp_last_error(sftp),"%s",ssh2ErrorText);
    }
    else
    {
      char *ssh2ErrorText;

      libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
      error = ERRORX_(IO,n,"%s",ssh2ErrorText);
    }
    assert(error != ERROR_UNKNOWN);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(socketHandle);
    UNUSED_VARIABLE(sftp);
    UNUSED_VARIABLE(sftpHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferSize);
    UNUSED_VARIABLE(bytesRead);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

/***********************************************************************\
* Name   : sftpWrite
* Purpose: sftp write data
* Input  : socketHandle - socket handle
*          sftp         - sftp session
*          sftpHandle   - sftp handle
*          buffer       - data
*          length       - data length
* Output : bytesWritten - number of written bytes
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors sftpWrite(SocketHandle        *socketHandle,
                       LIBSSH2_SFTP        *sftp,
                       LIBSSH2_SFTP_HANDLE *sftpHandle,
                       const void          *buffer,
                       ulong               length,
                       ulong               *bytesWritten
                     )
{
  Errors  error;
  #ifdef HAVE_SSH2
    ssize_t n;
  #endif /* HAVE_SSH2 */

  assert(socketHandle != NULL);
  assert(sftp != NULL);
  assert(sftpHandle != NULL);
  assert(buffer != NULL);
  assert(bytesWritten != NULL);

  #ifdef HAVE_SSH2
    error = ERROR_UNKNOWN;
    ssize_t retryCount = 5;
    do
    {
      n = libssh2_sftp_write(sftpHandle,
                             buffer,
                             length
                            );
      if      (n == 0)
      {
        retryCount--;
        // should not happen in blocking-mode: bug? libssh2 API changed somewhere between 0.18 and 1.2.4? => wait for data
        if (!waitSSHSessionSocket(socketHandle))
        {
          error = ERROR_NETWORK_SEND;
        }
      }
      else if (n == LIBSSH2_ERROR_EAGAIN)
      {
        retryCount--;
        if (retryCount >= 0)
        {
          Misc_mdelay(100);
        }
      }
    }
    while ((n == LIBSSH2_ERROR_EAGAIN) && (retryCount >= 0));
    if      (n > 0)
    {
      (*bytesWritten) = (ulong)n;
      error           = ERROR_NONE;
    }
    else if (n == LIBSSH2_ERROR_SFTP_PROTOCOL)
    {
      char *ssh2ErrorText;

      libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
      error = ERRORX_(IO,libssh2_sftp_last_error(sftp),"%s",ssh2ErrorText);
    }
    else
    {
      char *ssh2ErrorText;

      libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
      error = ERRORX_(IO,n,"%s",ssh2ErrorText);
    }
    assert(error != ERROR_UNKNOWN);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(socketHandle);
    UNUSED_VARIABLE(sftp);
    UNUSED_VARIABLE(sftpHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(length);
    UNUSED_VARIABLE(bytesWritten);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

/***********************************************************************\
* Name   : sftpStat
* Purpose: sftp file stat
* Input  : socketHandle - socket handle
*          fileName     - file name
*          fileInfo     - file info (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors sftpStat(SocketHandle *socketHandle,
                      ConstString  fileName,
                      FileInfo     *fileInfo
                     )
{
  Errors       error;
  #ifdef HAVE_SSH2
    int                     ssh2ErrorCode;
    uint                    retries;
    LIBSSH2_SFTP            *sftp;
    LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;
  #endif /* HAVE_SSH2 */

  assert(socketHandle != NULL);
  assert(fileName != NULL);

  #ifdef HAVE_SSH2
    // init SFTP session
    ssh2ErrorCode = 0;
    do
    {
      sftp = libssh2_sftp_init(Network_getSSHSession(socketHandle));
      if (sftp == NULL)
      {
        ssh2ErrorCode = libssh2_session_last_errno(Network_getSSHSession(socketHandle));
        if (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
      }
    }
    while ((sftp == NULL) && (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN));

    if (sftp != NULL)
    {
      // get file info
      error   = ERROR_UNKNOWN;
      retries = 0;
      do
      {
        ssh2ErrorCode = libssh2_sftp_lstat(sftp,
                                           String_cString(fileName),
                                           &sftpAttributes
                                          );
        if      (ssh2ErrorCode == 0)
        {
          if (fileInfo != NULL)
          {
            if      (LIBSSH2_SFTP_S_ISREG (sftpAttributes.permissions)) fileInfo->type = FILE_TYPE_FILE;
            else if (LIBSSH2_SFTP_S_ISDIR (sftpAttributes.permissions)) fileInfo->type = FILE_TYPE_DIRECTORY;
            else if (LIBSSH2_SFTP_S_ISLNK (sftpAttributes.permissions)) fileInfo->type = FILE_TYPE_LINK;
            else if (LIBSSH2_SFTP_S_ISCHR (sftpAttributes.permissions)) fileInfo->type = FILE_TYPE_SPECIAL;
            else if (LIBSSH2_SFTP_S_ISBLK (sftpAttributes.permissions)) fileInfo->type = FILE_TYPE_SPECIAL;
            else if (LIBSSH2_SFTP_S_ISFIFO(sftpAttributes.permissions)) fileInfo->type = FILE_TYPE_SPECIAL;
            else if (LIBSSH2_SFTP_S_ISSOCK(sftpAttributes.permissions)) fileInfo->type = FILE_TYPE_SPECIAL;
            else                                                  fileInfo->type = FILE_TYPE_UNKNOWN;
            fileInfo->size           = (uint64)sftpAttributes.filesize;
            fileInfo->timeLastAccess = (uint64)sftpAttributes.atime;
            fileInfo->timeModified   = (uint64)sftpAttributes.mtime;
            fileInfo->userId         = sftpAttributes.uid;
            fileInfo->groupId        = sftpAttributes.gid;
            fileInfo->permissions    = (FilePermissions)sftpAttributes.permissions;
          }

          error = ERROR_NONE;
        }
        else if ((ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) && (retries < MAX_LSTATE_RETRIES))
        {
          Misc_udelay(500LL*US_PER_MS);
          retries++;
        }
        else
        {
          char *ssh2ErrorText;

          libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
          error = ERRORX_(IO,
                          libssh2_session_last_errno(Network_getSSHSession(socketHandle)),
                          "%s",
                          ssh2ErrorText
                         );
        }
      }
      while (error == ERROR_UNKNOWN);

      (void)libssh2_sftp_shutdown(sftp);
    }
    else
    {
      if (ssh2ErrorCode == LIBSSH2_ERROR_SFTP_PROTOCOL)
      {
        char *ssh2ErrorText;

        libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
        error = ERRORX_(IO,libssh2_sftp_last_error(sftp),"%s",ssh2ErrorText);
      }
      else
      {
        char *ssh2ErrorText;

        ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
        error = ERRORX_(IO,
                        ssh2ErrorCode,
                        "%s",
                        ssh2ErrorText
                       );
      }
    }
    assert(error != ERROR_UNKNOWN);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(socketHandle);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(fileInfo);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

/***********************************************************************\
* Name   : sftpMakeDirectory
* Purpose: sftp make directory
* Input  : socketHandle  - socket handle
*          directoryName - directory name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors sftpMakeDirectory(SocketHandle *socketHandle,
                               ConstString  directoryName
                              )
{
  Errors       error;
  #ifdef HAVE_SSH2
    int          ssh2ErrorCode;
    uint         retries;
    LIBSSH2_SFTP *sftp;
  #endif /* HAVE_SSH2 */

  assert(socketHandle != NULL);
  assert(directoryName != NULL);

  #ifdef HAVE_SSH2
    // init SFTP session
    ssh2ErrorCode = 0;
    do
    {
      sftp = libssh2_sftp_init(Network_getSSHSession(socketHandle));
      if (sftp == NULL)
      {
        ssh2ErrorCode = libssh2_session_last_errno(Network_getSSHSession(socketHandle));
        if (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
      }
    }
    while ((sftp == NULL) && (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN));

    if (sftp != NULL)
    {
      // create directory
      error   = ERROR_UNKNOWN;
      retries = 0;
      do
      {
        ssh2ErrorCode = libssh2_sftp_mkdir(sftp,
                                           String_cString(directoryName),
                                           sftpGetPermissions(File_getDefaultDirectoryPermissions())
                                          );
        if      (ssh2ErrorCode == 0)
        {
          error = ERROR_NONE;
        }
        else if ((ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) && (retries < MAX_MKDIR_RETRIES))
        {
          Misc_udelay(500LL*US_PER_MS);
          retries++;
        }
        else
        {
          char *ssh2ErrorText;

          libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
          error = ERRORX_(IO,
                          libssh2_session_last_errno(Network_getSSHSession(socketHandle)),
                          "%s",
                          ssh2ErrorText
                         );
        }
      }
      while (error == ERROR_UNKNOWN);

      (void)libssh2_sftp_shutdown(sftp);
    }
    else
    {
      if (ssh2ErrorCode == LIBSSH2_ERROR_SFTP_PROTOCOL)
      {
        char *ssh2ErrorText;

        libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
        error = ERRORX_(IO,libssh2_sftp_last_error(sftp),"%s",ssh2ErrorText);
      }
      else
      {
        char *ssh2ErrorText;

        ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
        error = ERRORX_(IO,
                        ssh2ErrorCode,
                        "%s",
                        ssh2ErrorText
                       );
      }
    }
    assert(error != ERROR_UNKNOWN);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(socketHandle);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(fileInfo);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

/***********************************************************************\
* Name   : sftpUnlink
* Purpose: sftp unlink file/directory
* Input  : socketHandle - socket handle
*          name         - name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors sftpUnlink(SocketHandle *socketHandle,
                        ConstString  name
                       )
{
  Errors       error;
  #ifdef HAVE_SSH2
    int                     ssh2ErrorCode;
    uint                    retries;
    LIBSSH2_SFTP            *sftp;
    LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;
  #endif /* HAVE_SSH2 */

  assert(socketHandle != NULL);
  assert(name != NULL);

  #ifdef HAVE_SSH2
    // init SFTP session
    ssh2ErrorCode = 0;
    do
    {
      sftp = libssh2_sftp_init(Network_getSSHSession(socketHandle));
      if (sftp == NULL)
      {
        ssh2ErrorCode = libssh2_session_last_errno(Network_getSSHSession(socketHandle));
        if (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
      }
    }
    while ((sftp == NULL) && (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN));

    if (sftp != NULL)
    {
      error   = ERROR_UNKNOWN;
      retries = 0;
      do
      {
        ssh2ErrorCode = libssh2_sftp_lstat(sftp,
                                           String_cString(name),
                                           &sftpAttributes
                                          );
        if      (ssh2ErrorCode == 0)
        {
          if (LIBSSH2_SFTP_S_ISDIR (sftpAttributes.permissions))
          {
            // remove direcotry
            ssh2ErrorCode = libssh2_sftp_rmdir(sftp,
                                               String_cString(name)
                                              );
          }
          else
          {
            // delete file
            ssh2ErrorCode = libssh2_sftp_unlink(sftp,
                                                String_cString(name)
                                               );
          }
        }

        if      (ssh2ErrorCode == 0)
        {
          error = ERROR_NONE;
        }
        else if ((ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) && (retries < MAX_UNLINK_RETRIES))
        {
          Misc_udelay(500LL*US_PER_MS);
          retries++;
        }
        else
        {
          char *ssh2ErrorText;

          ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
          error = ERRORX_(IO,
                          ssh2ErrorCode,
                          "%s",
                          ssh2ErrorText
                         );
        }
      }
      while (error == ERROR_UNKNOWN);
    }
    else
    {
      if (ssh2ErrorCode == LIBSSH2_ERROR_SFTP_PROTOCOL)
      {
        char *ssh2ErrorText;

        libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
        error = ERRORX_(IO,libssh2_sftp_last_error(sftp),"%s",ssh2ErrorText);
      }
      else
      {
        char *ssh2ErrorText;

        ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(socketHandle),&ssh2ErrorText,NULL,0);
        error = ERRORX_(IO,
                        ssh2ErrorCode,
                        "%s",
                        ssh2ErrorText
                       );
      }
    }
    assert(error != ERROR_UNKNOWN);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(socketHandle);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(fileInfo);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

#endif /* HAVE_SSH2 */

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : StorageSFTP_initAll
* Purpose: initialize SFTP storage
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_initAll(void)
{
  return ERROR_NONE;
}

/***********************************************************************\
* Name   : StorageSFTP_doneAll
* Purpose: deinitialize SFTP storage
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void StorageSFTP_doneAll(void)
{
}

/***********************************************************************\
* Name   : StorageSFTP_parseSpecifier
* Purpose: parse SFTP specifier
* Input  : sftpSpecifier - SFTP specifier
* Output : hostName      - host name
*          hostPort      - host port
*          userName      - user name
*          password      - password
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSFTP_parseSpecifier(ConstString sftpSpecifier,
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

  assert(sftpSpecifier != NULL);
  assert(hostName != NULL);
  assert(userName != NULL);

  String_clear(hostName);
  if (hostPort != NULL) (*hostPort) = 0;
  String_clear(userName);
  if (password != NULL) Password_clear(password);

  s = String_new();
  t = String_new();
  if      (String_matchCString(sftpSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):([[:digit:]]+)$",NULL,STRING_NO_ASSIGN,userName,s,STRING_NO_ASSIGN,hostName,t,NULL))
  {
    // <login name>:<login password>@<host name>:<host port>
    String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (password != NULL) Password_setString(password,s);
    if (hostPort != NULL) (*hostPort) = (uint)String_toInteger(t,STRING_BEGIN,NULL,NULL,0);

    result = TRUE;
  }
  else if (String_matchCString(sftpSpecifier,STRING_BEGIN,"^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)$",NULL,STRING_NO_ASSIGN,userName,s,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>:<login password>@<host name>
    String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (password != NULL) Password_setString(password,s);

    result = TRUE;
  }
  else if (String_matchCString(sftpSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^:]+?):(\\d*)/{0,1}$",NULL,STRING_NO_ASSIGN,userName,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <login name>@<host name>:<host port>
    if (userName != NULL) String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (String_matchCString(sftpSpecifier,STRING_BEGIN,"^(([^@]|\\@)*?)@([^/]+)/{0,1}$",NULL,STRING_NO_ASSIGN,userName,STRING_NO_ASSIGN,hostName,NULL))
  {
    // <login name>@<host name>
    if (userName != NULL) String_mapCString(userName,STRING_BEGIN,LOGINNAME_MAP_FROM,LOGINNAME_MAP_TO,SIZE_OF_ARRAY(LOGINNAME_MAP_FROM),NULL);

    result = TRUE;
  }
  else if (String_matchCString(sftpSpecifier,STRING_BEGIN,"^([^@:/]*?):(\\d*)/{0,1}$",NULL,STRING_NO_ASSIGN,hostName,s,NULL))
  {
    // <host name>:<host port>
    if (hostPort != NULL)
    {
      if (!String_isEmpty(s)) (*hostPort) = (uint)String_toInteger(s,STRING_BEGIN,NULL,NULL,0);
    }

    result = TRUE;
  }
  else if (!String_isEmpty(sftpSpecifier))
  {
    // <host name>
    String_set(hostName,sftpSpecifier);

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
* Name   : StorageSFTP_equalSpecifiers
* Purpose: compare specifiers if equals
* Input  : storageSpecifier1,storageSpecifier2 - specifiers
*          archiveName1,archiveName2           - archive names (can be
*                                                NULL)
* Output : -
* Return : TRUE iff equals
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSFTP_equalSpecifiers(const StorageSpecifier *storageSpecifier1,
                                       ConstString            archiveName1,
                                       const StorageSpecifier *storageSpecifier2,
                                       ConstString            archiveName2
                                      )
{
  assert(storageSpecifier1 != NULL);
  assert(storageSpecifier1->type == STORAGE_TYPE_SFTP);
  assert(storageSpecifier2 != NULL);
  assert(storageSpecifier2->type == STORAGE_TYPE_SFTP);

  if (archiveName1 == NULL) archiveName1 = storageSpecifier1->archiveName;
  if (archiveName2 == NULL) archiveName2 = storageSpecifier2->archiveName;

  return    String_equals(storageSpecifier1->hostName,storageSpecifier2->hostName)
         && String_equals(archiveName1,archiveName2);
}

/***********************************************************************\
* Name   : StorageSFTP_getName
* Purpose: get storage name
* Input  : string           - name variable (can be NULL)
*          storageSpecifier - storage specifier string
*          archiveName      - archive name (can be NULL)
* Output : -
* Return : storage name
* Notes  : if archiveName is NULL file name from storageSpecifier is used
\***********************************************************************/

LOCAL String StorageSFTP_getName(String                 string,
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

  String_appendCString(string,"sftp://");
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
  if (!String_isEmpty(storageFileName))
  {
    String_appendChar(string,'/');
    String_append(string,storageFileName);
  }

  return string;
}

/***********************************************************************\
* Name   : StorageSFTP_getPrintableName
* Purpose: get printable storage name (without password)
* Input  : string           - name variable (can be NULL)
*          storageSpecifier - storage specifier string
*          archiveName      - archive name (can be NULL)
* Output : -
* Return : printable storage name
* Notes  : if archiveName is NULL file name from storageSpecifier is used
\***********************************************************************/

LOCAL void StorageSFTP_getPrintableName(String                 string,
                                        const StorageSpecifier *storageSpecifier,
                                        ConstString            archiveName
                                       )
{
  ConstString storageFileName;

  assert(string != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SFTP);

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

  String_appendCString(string,"sftp://");
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
* Name   : StorageSFTP_init
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

LOCAL Errors StorageSFTP_init(StorageInfo                *storageInfo,
                              const JobOptions           *jobOptions,
                              BandWidthList              *maxBandWidthList,
                              ServerConnectionPriorities serverConnectionPriority
                             )
{
  #ifdef HAVE_SSH2
    AutoFreeList autoFreeList;
    Errors       error;
    SSHServer    sshServer;
    uint         retries;
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  #ifdef HAVE_SSH2
    // init variables
    AutoFree_init(&autoFreeList);
    initBandWidthLimiter(&storageInfo->sftp.bandWidthLimiter,maxBandWidthList);
    AUTOFREE_ADD(&autoFreeList,&storageInfo->sftp.bandWidthLimiter,{ doneBandWidthLimiter(&storageInfo->sftp.bandWidthLimiter); });

    // get SSH server settings
    storageInfo->sftp.serverId = Configuration_initSSHServerSettings(&sshServer,storageInfo->storageSpecifier.hostName,jobOptions);
    AUTOFREE_ADD(&autoFreeList,&sshServer,{ Configuration_doneSSHServerSettings(&sshServer); });
    if (String_isEmpty(storageInfo->storageSpecifier.userName)) String_set(storageInfo->storageSpecifier.userName,sshServer.userName);
    if (String_isEmpty(storageInfo->storageSpecifier.userName)) String_setCString(storageInfo->storageSpecifier.userName,getenv("LOGNAME"));
    if (String_isEmpty(storageInfo->storageSpecifier.userName)) String_setCString(storageInfo->storageSpecifier.userName,getenv("USER"));
    if (storageInfo->storageSpecifier.hostPort == 0) storageInfo->storageSpecifier.hostPort = sshServer.port;
    if (Password_isEmpty(&storageInfo->storageSpecifier.password)) Password_set(&storageInfo->storageSpecifier.password,&sshServer.password);
    Configuration_duplicateKey(&storageInfo->sftp.publicKey, &sshServer.publicKey );
    Configuration_duplicateKey(&storageInfo->sftp.privateKey,&sshServer.privateKey);
    AUTOFREE_ADD(&autoFreeList,&storageInfo->sftp.publicKey,{ Configuration_doneKey(&storageInfo->sftp.publicKey); });
    AUTOFREE_ADD(&autoFreeList,&storageInfo->sftp.privateKey,{ Configuration_doneKey(&storageInfo->sftp.privateKey); });
    if (String_isEmpty(storageInfo->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate SSH server
    if (!allocateServer(storageInfo->sftp.serverId,serverConnectionPriority,ALLOCATE_SERVER_TIMEOUT))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageInfo->sftp.serverId,{ freeServer(storageInfo->sftp.serverId); });

    // check if SSH login is possible
    error = ERROR_SSH_AUTHENTICATION;
    if ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && !Password_isEmpty(&storageInfo->storageSpecifier.password))
    {
      error = checkSSHLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.hostPort,
                            storageInfo->storageSpecifier.userName,
                            &storageInfo->storageSpecifier.password,
                            storageInfo->sftp.publicKey.data,
                            storageInfo->sftp.publicKey.length,
                            storageInfo->sftp.privateKey.data,
                            storageInfo->sftp.privateKey.length
                           );
    }
    if ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && !Password_isEmpty(&sshServer.password))
    {
      error = checkSSHLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.hostPort,
                            storageInfo->storageSpecifier.userName,
                            &sshServer.password,
                            storageInfo->sftp.publicKey.data,
                            storageInfo->sftp.publicKey.length,
                            storageInfo->sftp.privateKey.data,
                            storageInfo->sftp.privateKey.length
                           );
      if (error == ERROR_NONE)
      {
        Password_set(&storageInfo->storageSpecifier.password,&sshServer.password);
      }
    }
    if ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && !Password_isEmpty(&sshServer.password))
    {
      error = checkSSHLogin(storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.hostPort,
                            storageInfo->storageSpecifier.userName,
                            &defaultSSHPassword,
                            storageInfo->sftp.publicKey.data,
                            storageInfo->sftp.publicKey.length,
                            storageInfo->sftp.privateKey.data,
                            storageInfo->sftp.privateKey.length
                           );
      if (error == ERROR_NONE)
      {
        Password_set(&storageInfo->storageSpecifier.password,&defaultSSHPassword);
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION)
    {
      // initialize interactive/default password
      retries = 0;
      while ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && (retries < MAX_PASSWORD_REQUESTS))
      {
        if (initSSHLogin(storageInfo->storageSpecifier.hostName,
                         storageInfo->storageSpecifier.userName,
                         &storageInfo->storageSpecifier.password,
                         jobOptions,
                         CALLBACK_(storageInfo->getNamePasswordFunction,storageInfo->getNamePasswordUserData)
                        )
           )
        {
          error = checkSSHLogin(storageInfo->storageSpecifier.hostName,
                                storageInfo->storageSpecifier.hostPort,
                                storageInfo->storageSpecifier.userName,
                                &storageInfo->storageSpecifier.password,
                                storageInfo->sftp.publicKey.data,
                                storageInfo->sftp.publicKey.length,
                                storageInfo->sftp.privateKey.data,
                                storageInfo->sftp.privateKey.length
                               );
        }
        retries++;
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION)
    {
      error = (   !Password_isEmpty(&storageInfo->storageSpecifier.password)
               || !Password_isEmpty(&sshServer.password)
               || !Password_isEmpty(&defaultSSHPassword)
              )
                ? ERRORX_(INVALID_SSH_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName))
                : ERRORX_(NO_SSH_PASSWORD,0,"%s",String_cString(storageInfo->storageSpecifier.hostName));
    }

    // store password as default SSH password
    if (error == ERROR_NONE)
    {
      Password_set(&defaultSSHPassword,&storageInfo->storageSpecifier.password);
    }

    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // free resources
    Configuration_doneSSHServerSettings(&sshServer);
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(maxBandWidthList);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

/***********************************************************************\
* Name   : StorageSFTP_done
* Purpose: deinit storage
* Input  : storageInfo - storage info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_done(StorageInfo *storageInfo)
{
  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  #ifdef HAVE_SSH2
    Configuration_doneKey(&storageInfo->sftp.privateKey);
    Configuration_doneKey(&storageInfo->sftp.publicKey);
    freeServer(storageInfo->sftp.serverId);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
  #endif /* HAVE_SSH2 */

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : StorageSFTP_isServerAllocationPending
* Purpose: check if server allocation is pending
* Input  : storageInfo - storage info
* Output : -
* Return : TRUE iff server allocation pending
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSFTP_isServerAllocationPending(const StorageInfo *storageInfo)
{
  bool serverAllocationPending;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  serverAllocationPending = FALSE;
  #if defined(HAVE_SSH2)
    serverAllocationPending = isServerAllocationPending(storageInfo->sftp.serverId);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);

    serverAllocationPending = FALSE;
  #endif /* HAVE_SSH2 */

  return serverAllocationPending;
}

/***********************************************************************\
* Name   : StorageSFTP_preProcess
* Purpose: pre-process storage
* Input  : storageInfo - storage info
*          archiveName - archive name
*          time        - time
*          initialFlag - TRUE iff initial call, FALSE otherwise
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_preProcess(const StorageInfo *storageInfo,
                                    ConstString       archiveName,
                                    time_t            time,
                                    bool              initialFlag
                                   )
{
  Errors error;
  #ifdef HAVE_SSH2
    TextMacros (textMacros,3);
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  error = ERROR_NONE;

  #ifdef HAVE_SSH2
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
      if (!String_isEmpty(globalOptions.sftp.writePreProcessCommand))
      {
        printInfo(1,"Write pre-processing...");
        error = executeTemplate(String_cString(globalOptions.sftp.writePreProcessCommand),
                                time,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL)
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }

      // free resources
      String_delete(directory);
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(time);
    UNUSED_VARIABLE(initialFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

/***********************************************************************\
* Name   : StorageSFTP_postProcess
* Purpose: post-process storage
* Input  : storageInfo - storage info
*          archiveName - archive name
*          time        - time
*          finalFlag   - TRUE iff final call, FALSE otherwise
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_postProcess(const StorageInfo *storageInfo,
                                     ConstString       archiveName,
                                     time_t            time,
                                     bool              finalFlag
                                    )
{
  Errors error;
  #ifdef HAVE_SSH2
    TextMacros (textMacros,3);
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  error = ERROR_NONE;

  #ifdef HAVE_SSH2
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
      if (!String_isEmpty(globalOptions.sftp.writePostProcessCommand))
      {
        printInfo(1,"Write post-processing...");
        error = executeTemplate(String_cString(globalOptions.sftp.writePostProcessCommand),
                                time,
                                textMacros.data,
                                textMacros.count,
                                CALLBACK_(executeIOOutput,NULL)
                               );
        printInfo(1,(error == ERROR_NONE) ? "OK\n" : "FAIL\n");
      }

      // free resources
      String_delete(directory);
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
    UNUSED_VARIABLE(time);
    UNUSED_VARIABLE(finalFlag);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

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

LOCAL bool StorageSFTP_exists(StorageInfo *storageInfo,
                              ConstString archiveName
                             )
{
  bool   existsFlag;
  #ifdef HAVE_SSH2
    Errors       error;
    SocketHandle socketHandle;
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  existsFlag = FALSE;

  #ifdef HAVE_SSH2
    {
      error = Network_connect(&socketHandle,
                              SOCKET_TYPE_SSH,
                              storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.userName,
                              &storageInfo->storageSpecifier.password,
                              NULL,  // caData
                              0,     // caLength
                              NULL,  // certData
                              0,     // certLength
                              storageInfo->sftp.publicKey.data,
                              storageInfo->sftp.publicKey.length,
                              storageInfo->sftp.privateKey.data,
                              storageInfo->sftp.privateKey.length,
                                SOCKET_FLAG_NONE
                              | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                              | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                              30*MS_PER_SECOND
                             );
      if (error == ERROR_NONE)
      {
        libssh2_session_set_timeout(Network_getSSHSession(&socketHandle),READ_TIMEOUT);

        existsFlag = (sftpStat(&socketHandle,
                               archiveName,
                               NULL
                              ) == ERROR_NONE
                     );

        Network_disconnect(&socketHandle);
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
  #endif /* HAVE_SSH2 */

  return existsFlag;
}

/***********************************************************************\
* Name   : StorageSFTP_isFile
* Purpose: check if storage file
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE if storage file, FALSE otherweise
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSFTP_isFile(StorageInfo *storageInfo,
                              ConstString archiveName
                             )
{
  bool         isFileFlag;
  #ifdef HAVE_SSH2
    Errors       error;
    SocketHandle socketHandle;
    FileInfo     fileInfo;
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  isFileFlag = FALSE;

  #ifdef HAVE_SSH2
    {
      error = Network_connect(&socketHandle,
                              SOCKET_TYPE_SSH,
                              storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.userName,
                              &storageInfo->storageSpecifier.password,
                              NULL,  // caData
                              0,     // caLength
                              NULL,  // certData
                              0,     // certLength
                              storageInfo->sftp.publicKey.data,
                              storageInfo->sftp.publicKey.length,
                              storageInfo->sftp.privateKey.data,
                              storageInfo->sftp.privateKey.length,
                                SOCKET_FLAG_NONE
                              | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                              | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                              30*MS_PER_SECOND
                             );
      if (error == ERROR_NONE)
      {
        libssh2_session_set_timeout(Network_getSSHSession(&socketHandle),READ_TIMEOUT);

        isFileFlag = (   (sftpStat(&socketHandle,
                                   archiveName,
                                   &fileInfo
                                  ) == ERROR_NONE
                         )
                      && (fileInfo.type == FILE_TYPE_FILE)
                     );

        Network_disconnect(&socketHandle);
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
  #endif /* HAVE_SSH2 */

  return isFileFlag;
}

/***********************************************************************\
* Name   : StorageSFTP_isDirectory
* Purpose: check if storage directory
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE if storage directory, FALSE otherweise
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSFTP_isDirectory(StorageInfo *storageInfo,
                                   ConstString archiveName
                                  )
{
  bool isDirectoryFlag;
  #ifdef HAVE_SSH2
    Errors       error;
    SocketHandle socketHandle;
    FileInfo     fileInfo;
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  isDirectoryFlag = FALSE;

  #ifdef HAVE_SSH2
    {
      error = Network_connect(&socketHandle,
                              SOCKET_TYPE_SSH,
                              storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.userName,
                              &storageInfo->storageSpecifier.password,
                              NULL,  // caData
                              0,     // caLength
                              NULL,  // certData
                              0,     // certLength
                              storageInfo->sftp.publicKey.data,
                              storageInfo->sftp.publicKey.length,
                              storageInfo->sftp.privateKey.data,
                              storageInfo->sftp.privateKey.length,
                                SOCKET_FLAG_NONE
                              | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                              | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                              30*MS_PER_SECOND
                             );
      if (error == ERROR_NONE)
      {
        libssh2_session_set_timeout(Network_getSSHSession(&socketHandle),READ_TIMEOUT);

        isDirectoryFlag = (   (sftpStat(&socketHandle,
                                        archiveName,
                                        &fileInfo
                                       ) == ERROR_NONE
                              )
                           && (fileInfo.type == FILE_TYPE_DIRECTORY)
                          );

        Network_disconnect(&socketHandle);
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);
  #endif /* HAVE_SSH2 */

  return isDirectoryFlag;
}

/***********************************************************************\
* Name   : StorageSFTP_isReadable
* Purpose: check if storage file exists and is readable
* Input  : storageInfo - storage info
*          archiveName - archive name (can be NULL)
* Output : -
* Return : TRUE if storage file/directory exists and is readable, FALSE
*          otherweise
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSFTP_isReadable(const StorageInfo *storageInfo, ConstString archiveName)
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

LOCAL bool StorageSFTP_isWritable(const StorageInfo *storageInfo, ConstString archiveName)
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
* Name   : StorageSFTP_getTmpName
* Purpose: get temporary archive name
* Input  : archiveName - archive name variable
*          storageInfo - storage info
* Output : archiveName - temporary archive name
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_getTmpName(String archiveName, const StorageInfo *storageInfo)
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
* Name   : StorageSFTP_create
* Purpose: create new/append to storage
* Input  : storageHandle - storage handle variable
*          archiveName   - archive name (can be NULL)
*          archiveSize   - archive size [bytes]
*          forceFlag     - TRUE to force overwrite existing storage
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_create(StorageHandle *storageHandle,
                                ConstString   fileName,
                                uint64        fileSize,
                                bool          forceFlag
                               )
{
  #ifdef HAVE_SSH2
    Errors                  error;
    int                     ssh2ErrorCode;
    String                  directoryName;
    LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(!String_isEmpty(fileName));

  UNUSED_VARIABLE(fileSize);

  // check if file exists
  if (   !forceFlag
      && (storageHandle->storageInfo->jobOptions != NULL)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_APPEND)
      && (storageHandle->storageInfo->jobOptions->archiveFileMode != ARCHIVE_FILE_MODE_OVERWRITE)
      && StorageSFTP_exists(storageHandle->storageInfo,fileName)
     )
  {
    return ERRORX_(FILE_EXISTS_,0,"%s",String_cString(fileName));
  }

  #ifdef HAVE_SSH2
    // init variables
    storageHandle->sftp.oldSendCallback        = NULL;
    storageHandle->sftp.oldReceiveCallback     = NULL;
    storageHandle->sftp.totalSentBytes         = 0LL;
    storageHandle->sftp.totalReceivedBytes     = 0LL;
    storageHandle->sftp.sftp                   = NULL;
    storageHandle->sftp.sftpHandle             = NULL;
    storageHandle->sftp.index                  = 0LL;
    storageHandle->sftp.size                   = 0LL;
    storageHandle->sftp.readAheadBuffer.offset = 0LL;
    storageHandle->sftp.readAheadBuffer.length = 0L;

    // connect
    error = Network_connect(&storageHandle->sftp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageHandle->storageInfo->storageSpecifier.hostName,
                            storageHandle->storageInfo->storageSpecifier.hostPort,
                            storageHandle->storageInfo->storageSpecifier.userName,
                            &storageHandle->storageInfo->storageSpecifier.password,
                            NULL,  // caData
                            0,     // caLength
                            NULL,  // certData
                            0,     // certLength
                            storageHandle->storageInfo->sftp.publicKey.data,
                            storageHandle->storageInfo->sftp.publicKey.length,
                            storageHandle->storageInfo->sftp.privateKey.data,
                            storageHandle->storageInfo->sftp.privateKey.length,
                              SOCKET_FLAG_NONE
                            | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                            | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                            30*MS_PER_SECOND
                           );
    if (error != ERROR_NONE)
    {
      return error;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&storageHandle->sftp.socketHandle),READ_TIMEOUT);

    // install send/receive callback to track number of sent/received bytes
    (*(libssh2_session_abstract(Network_getSSHSession(&storageHandle->sftp.socketHandle)))) = storageHandle;
    storageHandle->sftp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->sftp.socketHandle),LIBSSH2_CALLBACK_SEND,sftpSendCallback   );
    storageHandle->sftp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->sftp.socketHandle),LIBSSH2_CALLBACK_RECV,sftpReceiveCallback);

    // init SFTP session
    ssh2ErrorCode = 0;
    do
    {
      storageHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageHandle->sftp.socketHandle));
      if (storageHandle->sftp.sftp == NULL)
      {
        ssh2ErrorCode = libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle));
        if (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
      }
    }
    while ((storageHandle->sftp.sftp == NULL) && (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN));
    if (storageHandle->sftp.sftp == NULL)
    {
      char *ssh2ErrorText;

      libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&ssh2ErrorText,NULL,0);
      error = ERRORX_(SSH,
                      libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                      "%s",
                      ssh2ErrorText
                     );
      Network_disconnect(&storageHandle->sftp.socketHandle);
      return error;
    }

    // create directories if not existing
    directoryName = File_getDirectoryName(String_new(),fileName);
    String name = String_new();
    FILE_PATH_ITERATEX(directoryName,name,TRUE,error == ERROR_NONE)
    {
      if (libssh2_sftp_lstat(storageHandle->sftp.sftp,
                             String_cString(name),
                             &sftpAttributes
                            ) == 0
         )
      {
        // check if directory
        if (!LIBSSH2_SFTP_S_ISDIR(sftpAttributes.permissions))
        {
          error = ERRORX_(NOT_A_DIRECTORY,0,"%s",String_cString(name));
        }
      }
      else
      {
        // create directory
        if (libssh2_sftp_mkdir(storageHandle->sftp.sftp,
                               String_cString(name),
                               sftpGetPermissions(File_getDefaultDirectoryPermissions())
                              ) != 0
           )
        {
          char *ssh2ErrorText;

          libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&ssh2ErrorText,NULL,0);
          error = ERRORX_(SSH,
                          libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                          "create '%s' fail: %s",
                          String_cString(name),
                          ssh2ErrorText
                         );
        }
      }
    }
    String_delete(name);
    String_delete(directoryName);
    if (error != ERROR_NONE)
    {
      (void)libssh2_sftp_shutdown(storageHandle->sftp.sftp);
      Network_disconnect(&storageHandle->sftp.socketHandle);
      return error;
    }

    // create file
    storageHandle->sftp.sftpHandle = libssh2_sftp_open(storageHandle->sftp.sftp,
                                                       String_cString(fileName),
                                                       (storageHandle->storageInfo->jobOptions->archiveFileMode == ARCHIVE_FILE_MODE_APPEND)
                                                         ? LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_APPEND
                                                         : LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_TRUNC,
                                                       sftpGetPermissions(File_getDefaultFilePermissions())
                                                      );
    if (storageHandle->sftp.sftpHandle == NULL)
    {
      char *ssh2ErrorText;

      libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&ssh2ErrorText,NULL,0);
      error = ERRORX_(SSH,
                      libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle)),
                      "%s",
                      ssh2ErrorText
                     );
      (void)libssh2_sftp_shutdown(storageHandle->sftp.sftp);
      Network_disconnect(&storageHandle->sftp.socketHandle);
      return error;
    }

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileSize);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

/***********************************************************************\
* Name   : StorageSFTP_open
* Purpose: open storage for reading
* Input  : storageHandle - storage handle variable
*          archiveName   - archive name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_open(StorageHandle *storageHandle,
                              ConstString   archiveName
                             )
{
  #ifdef HAVE_SSH2
    Errors                  error;
    int                     ssh2ErrorCode;
    LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(!String_isEmpty(archiveName));

  #ifdef HAVE_SSH2
    // init variables
    storageHandle->sftp.oldSendCallback        = NULL;
    storageHandle->sftp.oldReceiveCallback     = NULL;
    storageHandle->sftp.totalSentBytes         = 0LL;
    storageHandle->sftp.totalReceivedBytes     = 0LL;
    storageHandle->sftp.sftp                   = NULL;
    storageHandle->sftp.sftpHandle             = NULL;
    storageHandle->sftp.index                  = 0LL;
    storageHandle->sftp.size                   = 0LL;
    storageHandle->sftp.readAheadBuffer.offset = 0LL;
    storageHandle->sftp.readAheadBuffer.length = 0L;

    // allocate read-ahead buffer
    storageHandle->sftp.readAheadBuffer.data = (byte*)malloc(MAX_BUFFER_SIZE);
    if (storageHandle->sftp.readAheadBuffer.data == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }

    // connect
    error = Network_connect(&storageHandle->sftp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageHandle->storageInfo->storageSpecifier.hostName,
                            storageHandle->storageInfo->storageSpecifier.hostPort,
                            storageHandle->storageInfo->storageSpecifier.userName,
                            &storageHandle->storageInfo->storageSpecifier.password,
                            NULL,  // caData
                            0,     // caLength
                            NULL,  // certData
                            0,     // certLength
                            storageHandle->storageInfo->sftp.publicKey.data,
                            storageHandle->storageInfo->sftp.publicKey.length,
                            storageHandle->storageInfo->sftp.privateKey.data,
                            storageHandle->storageInfo->sftp.privateKey.length,
                              SOCKET_FLAG_NONE
                            | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                            | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                            30*MS_PER_SECOND
                           );
    if (error != ERROR_NONE)
    {
      free(storageHandle->sftp.readAheadBuffer.data);
      return error;
    }
    libssh2_session_set_timeout(Network_getSSHSession(&storageHandle->sftp.socketHandle),READ_TIMEOUT);

    // install send/receive callback to track number of sent/received bytes
    (*(libssh2_session_abstract(Network_getSSHSession(&storageHandle->sftp.socketHandle)))) = storageHandle;
    storageHandle->sftp.oldSendCallback    = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->sftp.socketHandle),LIBSSH2_CALLBACK_SEND,sftpSendCallback   );
    storageHandle->sftp.oldReceiveCallback = libssh2_session_callback_set(Network_getSSHSession(&storageHandle->sftp.socketHandle),LIBSSH2_CALLBACK_RECV,sftpReceiveCallback);

    // init SFTP session
    ssh2ErrorCode = 0;
    do
    {
      storageHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageHandle->sftp.socketHandle));
      if (storageHandle->sftp.sftp == NULL)
      {
        ssh2ErrorCode = libssh2_session_last_errno(Network_getSSHSession(&storageHandle->sftp.socketHandle));
        if (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
      }
    }
    while ((storageHandle->sftp.sftp == NULL) && (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN));
    if (storageHandle->sftp.sftp == NULL)
    {
      char *ssh2ErrorText;

      ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&ssh2ErrorText,NULL,0);
      error = ERRORX_(SSH,
                      ssh2ErrorCode,
                      "%s",
                      ssh2ErrorText
                     );
      Network_disconnect(&storageHandle->sftp.socketHandle);
      free(storageHandle->sftp.readAheadBuffer.data);
      return error;
    }

    // open file
    storageHandle->sftp.sftpHandle = libssh2_sftp_open(storageHandle->sftp.sftp,
                                                       String_cString(archiveName),
                                                       LIBSSH2_FXF_READ,
                                                       0
                                                      );
    if (storageHandle->sftp.sftpHandle == NULL)
    {
      char *ssh2ErrorText;

      ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&ssh2ErrorText,NULL,0);
      error = ERRORX_(SSH,
                      ssh2ErrorCode,
                      "%s",
                      ssh2ErrorText
                     );
      (void)libssh2_sftp_shutdown(storageHandle->sftp.sftp);
      Network_disconnect(&storageHandle->sftp.socketHandle);
      free(storageHandle->sftp.readAheadBuffer.data);
      return error;
    }

    // get file size
    if (libssh2_sftp_fstat(storageHandle->sftp.sftpHandle,
                           &sftpAttributes
                          ) != 0
       )
    {
      char *ssh2ErrorText;

      ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(&storageHandle->sftp.socketHandle),&ssh2ErrorText,NULL,0);
      error = ERRORX_(SSH,
                      ssh2ErrorCode,
                      "%s",
                      ssh2ErrorText
                     );
      (void)libssh2_sftp_close(storageHandle->sftp.sftpHandle);
      (void)libssh2_sftp_shutdown(storageHandle->sftp.sftp);
      Network_disconnect(&storageHandle->sftp.socketHandle);
      free(storageHandle->sftp.readAheadBuffer.data);
      return error;
    }
    storageHandle->sftp.size = sftpAttributes.filesize;

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(archiveName);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

/***********************************************************************\
* Name   : StorageSFTP_close
* Purpose: close storage file
* Input  : storageHandle - storage handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void StorageSFTP_close(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  #ifdef HAVE_SSH2
    switch (storageHandle->mode)
    {
      case STORAGE_MODE_READ:
        (void)libssh2_sftp_close(storageHandle->sftp.sftpHandle);
        free(storageHandle->sftp.readAheadBuffer.data);
        break;
      case STORAGE_MODE_WRITE:
        (void)libssh2_sftp_close(storageHandle->sftp.sftpHandle);
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break; /* not reached */
      #endif /* NDEBUG */
    }
    (void)libssh2_sftp_shutdown(storageHandle->sftp.sftp);
    Network_disconnect(&storageHandle->sftp.socketHandle);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_SSH2 */
}

/***********************************************************************\
* Name   : StorageSFTP_eof
* Purpose: check if end-of-file in storage
* Input  : storageHandle - storage handle
* Output : -
* Return : TRUE if end-of-file, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSFTP_eof(StorageHandle *storageHandle)
{
  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  #ifdef HAVE_SSH2
    return storageHandle->sftp.index >= storageHandle->sftp.size;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);

    return TRUE;
  #endif /* HAVE_SSH2 */
}

/***********************************************************************\
* Name   : StorageSFTP_read
* Purpose: read from storage file
* Input  : storageHandle - storage handle
*          buffer        - buffer with data to write
*          size          - data size
*          bytesRead     - number of bytes read or NULL
* Output : bytesRead - number of bytes read
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_read(StorageHandle *storageHandle,
                              void          *buffer,
                              ulong         bufferSize,
                              ulong         *bytesRead
                             )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_READ);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(buffer != NULL);

  if (bytesRead != NULL) (*bytesRead) = 0L;

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    {
      ulong   index;
      ulong   bytesAvail;
      ulong   length;
      uint64  startTimestamp,endTimestamp;
      uint64  startTotalReceivedBytes,endTotalReceivedBytes;

      assert(storageHandle->sftp.sftpHandle != NULL);
      assert(storageHandle->sftp.readAheadBuffer.data != NULL);

      while (bufferSize > 0)
      {
        // copy as much data as available from read-ahead buffer
        if (   (storageHandle->sftp.index >= storageHandle->sftp.readAheadBuffer.offset)
            && (storageHandle->sftp.index < (storageHandle->sftp.readAheadBuffer.offset+storageHandle->sftp.readAheadBuffer.length))
           )
        {
          // copy data from read-ahead buffer
          index      = (ulong)(storageHandle->sftp.index-storageHandle->sftp.readAheadBuffer.offset);
          bytesAvail = MIN(bufferSize,storageHandle->sftp.readAheadBuffer.length-index);
          memCopyFast(buffer,bytesAvail,storageHandle->sftp.readAheadBuffer.data+index,bytesAvail);

          // adjust buffer, bufferSize, bytes read, index
          buffer = (byte*)buffer+bytesAvail;
          bufferSize -= bytesAvail;
          if (bytesRead != NULL) (*bytesRead) += bytesAvail;
          storageHandle->sftp.index += (uint64)bytesAvail;
        }

        // read rest of data
        if (bufferSize > 0)
        {
          assert(storageHandle->sftp.index >= (storageHandle->sftp.readAheadBuffer.offset+storageHandle->sftp.readAheadBuffer.length));

          // get max. number of bytes to receive in one step
          if (storageHandle->storageInfo->sftp.bandWidthLimiter.maxBandWidthList != NULL)
          {
            length = MIN(storageHandle->storageInfo->sftp.bandWidthLimiter.blockSize,bufferSize);
          }
          else
          {
            length = bufferSize;
          }
          assert(length > 0L);

          // get start time, start received bytes
          startTimestamp          = Misc_getTimestamp();
          startTotalReceivedBytes = storageHandle->sftp.totalReceivedBytes;

          #if   defined(HAVE_SSH2_SFTP_SEEK64)
            libssh2_sftp_seek64(storageHandle->sftp.sftpHandle,storageHandle->sftp.index);
          #elif defined(HAVE_SSH2_SFTP_SEEK2)
            libssh2_sftp_seek2(storageHandle->sftp.sftpHandle,storageHandle->sftp.index);
          #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
            libssh2_sftp_seek(storageHandle->sftp.sftpHandle,storageHandle->sftp.index);
          #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */

          if (length <= MAX_BUFFER_SIZE)
          {
            // read into read-ahead buffer
            error = sftpRead(&storageHandle->sftp.socketHandle,
                             storageHandle->sftp.sftp,
                             storageHandle->sftp.sftpHandle,
                             storageHandle->sftp.readAheadBuffer.data,
                             MIN((size_t)(storageHandle->sftp.size-storageHandle->sftp.index),MAX_BUFFER_SIZE),
                             &bytesAvail
                            );
            if (error != ERROR_NONE)
            {
              break;
            }
            storageHandle->sftp.readAheadBuffer.offset = storageHandle->sftp.index;
            storageHandle->sftp.readAheadBuffer.length = bytesAvail;

            // copy data from read-ahead buffer
            bytesAvail = MIN(length,storageHandle->sftp.readAheadBuffer.length);
            memcpy(buffer,storageHandle->sftp.readAheadBuffer.data,bytesAvail);

            // adjust buffer, bufferSize, bytes read, index
            buffer = (byte*)buffer+bytesAvail;
            bufferSize -= bytesAvail;
            if (bytesRead != NULL) (*bytesRead) += bytesAvail;
            storageHandle->sftp.index += (uint64)bytesAvail;
          }
          else
          {
            // read direct
            error = sftpRead(&storageHandle->sftp.socketHandle,
                             storageHandle->sftp.sftp,
                             storageHandle->sftp.sftpHandle,
                             buffer,
                             length,
                             &bytesAvail
                            );
            if (error != ERROR_NONE)
            {
              break;
            }

            // adjust buffer, bufferSize, bytes read, index
            buffer = (byte*)buffer+(ulong)bytesAvail;
            bufferSize -= (ulong)bytesAvail;
            if (bytesRead != NULL) (*bytesRead) += (ulong)bytesAvail;
            storageHandle->sftp.index += (uint64)bytesAvail;
          }

          // get end time, end received bytes
          endTimestamp          = Misc_getTimestamp();
          endTotalReceivedBytes = storageHandle->sftp.totalReceivedBytes;
          assert(endTotalReceivedBytes >= startTotalReceivedBytes);

          /* limit used band width if requested (note: when the system time is
             changing endTimestamp may become smaller than startTimestamp;
             thus do not check this with an assert())
          */
          if (endTimestamp >= startTimestamp)
          {
            SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
            {
              limitBandWidth(&storageHandle->storageInfo->sftp.bandWidthLimiter,
                             endTotalReceivedBytes-startTotalReceivedBytes,
                             endTimestamp-startTimestamp
                            );
            }
          }
        }
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferSize);
    UNUSED_VARIABLE(bytesRead);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */

  return error;
}

/***********************************************************************\
* Name   : StorageSFTP_write
* Purpose: write into storage file
* Input  : storageHandle - storage handle
*          buffer        - buffer with data to write
*          size          - data size
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_write(StorageHandle *storageHandle,
                               const void    *buffer,
                               ulong         bufferLength
                              )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->mode == STORAGE_MODE_WRITE);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(buffer != NULL);

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    {
      ulong   bytesWritten;
      ulong   length;
      uint64  startTimestamp,endTimestamp;
      uint64  startTotalSentBytes,endTotalSentBytes;
      ulong   n;

      assert(storageHandle->sftp.sftpHandle != NULL);

      bytesWritten = 0L;
      while (bytesWritten < bufferLength)
      {
        // get max. number of bytes to send in one step
        if (storageHandle->storageInfo->sftp.bandWidthLimiter.maxBandWidthList != NULL)
        {
          length = MIN(storageHandle->storageInfo->sftp.bandWidthLimiter.blockSize,bufferLength-bytesWritten);
        }
        else
        {
          length = bufferLength-bytesWritten;
        }
        assert(length > 0L);

        // get start time, start received bytes
        startTimestamp      = Misc_getTimestamp();
        startTotalSentBytes = storageHandle->sftp.totalSentBytes;

        // send data
        error = sftpWrite(&storageHandle->sftp.socketHandle,
                          storageHandle->sftp.sftp,
                          storageHandle->sftp.sftpHandle,
                          buffer,
                          length,
                          &n
                         );
        if (error != ERROR_NONE)
        {
          break;
        }
        buffer = (byte*)buffer+n;
        bytesWritten += n;

        // get end time, end received bytes
        endTimestamp      = Misc_getTimestamp();
        endTotalSentBytes = storageHandle->sftp.totalSentBytes;
        assert(endTotalSentBytes >= startTotalSentBytes);

        /* limit used band width if requested (note: when the system time is
           changing endTimestamp may become smaller than startTimestamp;
           thus do not check this with an assert())
        */
        if (endTimestamp >= startTimestamp)
        {
          SEMAPHORE_LOCKED_DO(&storageHandle->storageInfo->lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
          {
            limitBandWidth(&storageHandle->storageInfo->sftp.bandWidthLimiter,
                           endTotalSentBytes-startTotalSentBytes,
                           endTimestamp-startTimestamp
                          );
          }
        }
      }
      storageHandle->sftp.size += bytesWritten;
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(buffer);
    UNUSED_VARIABLE(bufferLength);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

/***********************************************************************\
* Name   : StorageSFTP_getSize
* Purpose: get storage file size
* Input  : storageHandle - storage handle
* Output : -
* Return : size of storage
* Notes  : -
\***********************************************************************/

LOCAL uint64 StorageSFTP_getSize(StorageHandle *storageHandle)
{
  uint64 size;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  size = 0LL;
  #ifdef HAVE_SSH2
    size = storageHandle->sftp.size;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
  #endif /* HAVE_SSH2 */

  return size;
}

/***********************************************************************\
* Name   : StorageSFTP_tell
* Purpose: get current position in storage file
* Input  : storageHandle - storage handle
* Output : offset - offset (0..n-1)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_tell(StorageHandle *storageHandle,
                              uint64        *offset
                             )
{
  Errors error;

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(offset != NULL);

  (*offset) = 0LL;

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    (*offset) = storageHandle->sftp.index;
    error     = ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

/***********************************************************************\
* Name   : StorageSFTP_seek
* Purpose: seek in storage file
* Input  : storageHandle - storage handle
*          offset        - offset (0..n-1)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_seek(StorageHandle *storageHandle,
                              uint64        offset
                             )
{
  Errors error;
  #ifdef HAVE_SSH2
    uint64 skip;
    uint64 i;
    uint64 n;
  #endif /* HAVE_SSH2 */

  assert(storageHandle != NULL);
  assert(storageHandle->storageInfo != NULL);
  assert(storageHandle->storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

  error = ERROR_NONE;
  #ifdef HAVE_SSH2
    assert(storageHandle->sftp.sftpHandle != NULL);
    assert(storageHandle->sftp.readAheadBuffer.data != NULL);

    if      (offset > storageHandle->sftp.index)
    {
      skip = offset-storageHandle->sftp.index;
      if (skip > 0LL)
      {
        // skip data in read-ahead buffer
        if (   (storageHandle->sftp.index >= storageHandle->sftp.readAheadBuffer.offset)
            && (storageHandle->sftp.index < (storageHandle->sftp.readAheadBuffer.offset+storageHandle->sftp.readAheadBuffer.length))
           )
        {
          i = storageHandle->sftp.index-storageHandle->sftp.readAheadBuffer.offset;
          n = MIN(skip,storageHandle->sftp.readAheadBuffer.length-i);
          skip -= n;
          storageHandle->sftp.index += (uint64)n;
        }

        if (skip > 0LL)
        {
          #if   defined(HAVE_SSH2_SFTP_SEEK64)
            libssh2_sftp_seek64(storageHandle->sftp.sftpHandle,offset);
          #elif defined(HAVE_SSH2_SFTP_SEEK2)
            libssh2_sftp_seek2(storageHandle->sftp.sftpHandle,offset);
          #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
            libssh2_sftp_seek(storageHandle->sftp.sftpHandle,(size_t)offset);
          #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
          storageHandle->sftp.readAheadBuffer.offset = offset;
          storageHandle->sftp.readAheadBuffer.length = 0L;

          storageHandle->sftp.index = offset;
        }
      }
    }
    else if (offset < storageHandle->sftp.index)
    {
      skip = storageHandle->sftp.index-offset;
      if (skip > 0LL)
      {
        // skip data in read-ahead buffer
        if (   (storageHandle->sftp.index >= storageHandle->sftp.readAheadBuffer.offset)
            && (storageHandle->sftp.index < (storageHandle->sftp.readAheadBuffer.offset+storageHandle->sftp.readAheadBuffer.length))
           )
        {
          i = storageHandle->sftp.index-storageHandle->sftp.readAheadBuffer.offset;
          n = MIN(skip,i);
          skip -= n;
          storageHandle->sftp.index -= (uint64)n;
        }

        if (skip > 0LL)
        {
          #if   defined(HAVE_SSH2_SFTP_SEEK64)
            libssh2_sftp_seek64(storageHandle->sftp.sftpHandle,offset);
          #elif defined(HAVE_SSH2_SFTP_SEEK2)
            libssh2_sftp_seek2(storageHandle->sftp.sftpHandle,offset);
          #else /* not HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
            libssh2_sftp_seek(storageHandle->sftp.sftpHandle,(size_t)offset);
          #endif /* HAVE_SSH2_SFTP_SEEK64 || HAVE_SSH2_SFTP_SEEK2 */
          storageHandle->sftp.readAheadBuffer.offset = offset;
          storageHandle->sftp.readAheadBuffer.length = 0L;

          storageHandle->sftp.index = offset;
        }
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageHandle);
    UNUSED_VARIABLE(offset);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

/***********************************************************************\
* Name   : StorageSFTP_rename
* Purpose: rename storage file
* Input  : storageInfo    - storage
*          oldArchiveName - archive names (can be NULL)
*          newArchiveName - new archive name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_rename(const StorageInfo *storageInfo,
                                ConstString       fromArchiveName,
                                ConstString       toArchiveName
                               )
{
  Errors error;

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);

UNUSED_VARIABLE(storageInfo);
UNUSED_VARIABLE(fromArchiveName);
UNUSED_VARIABLE(toArchiveName);
fprintf(stderr,"%s:%d: _\n",__FILE__,__LINE__);
error = ERROR_STILL_NOT_IMPLEMENTED;

  return error;
}

/***********************************************************************\
* Name   : StorageSFTP_makeDirectory
* Purpose: create directories
* Input  : storageInfo - storage info
*          pathName    - path name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_makeDirectory(StorageInfo *storageInfo,
                                       ConstString directoryName
                                      )
{
  Errors       error;
  #ifdef HAVE_SSH2
    SocketHandle socketHandle;
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(!String_isEmpty(directoryName));

  error = ERROR_UNKNOWN;
  #ifdef HAVE_SSH2
    error = Network_connect(&socketHandle,
                            SOCKET_TYPE_SSH,
                            storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.hostPort,
                            storageInfo->storageSpecifier.userName,
                            &storageInfo->storageSpecifier.password,
                            NULL,  // caData
                            0,     // caLength
                            NULL,  // certData
                            0,     // certLength
                            storageInfo->sftp.publicKey.data,
                            storageInfo->sftp.publicKey.length,
                            storageInfo->sftp.privateKey.data,
                            storageInfo->sftp.privateKey.length,
                              SOCKET_FLAG_NONE
                            | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                            | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                            30*MS_PER_SECOND
                           );
    if (error == ERROR_NONE)
    {
      libssh2_session_set_timeout(Network_getSSHSession(&socketHandle),READ_TIMEOUT);

      error = sftpMakeDirectory(&socketHandle,
                                directoryName
                               );

      Network_disconnect(&socketHandle);
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(directoryName);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

/***********************************************************************\
* Name   : StorageSFTP_delete
* Purpose: delete storage file/directory
* Input  : storageInfo - storage
*          archiveName - archive name (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_delete(StorageInfo *storageInfo,
                                ConstString archiveName
                               )
{
  Errors       error;
  #ifdef HAVE_SSH2
    SocketHandle socketHandle;
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(!String_isEmpty(archiveName));

  error = ERROR_UNKNOWN;
  #ifdef HAVE_SSH2
    error = Network_connect(&socketHandle,
                            SOCKET_TYPE_SSH,
                            storageInfo->storageSpecifier.hostName,
                            storageInfo->storageSpecifier.hostPort,
                            storageInfo->storageSpecifier.userName,
                            &storageInfo->storageSpecifier.password,
                            NULL,  // caData
                            0,     // caLength
                            NULL,  // certData
                            0,     // certLength
                            storageInfo->sftp.publicKey.data,
                            storageInfo->sftp.publicKey.length,
                            storageInfo->sftp.privateKey.data,
                            storageInfo->sftp.privateKey.length,
                              SOCKET_FLAG_NONE
                            | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                            | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                            30*MS_PER_SECOND
                           );
    if (error == ERROR_NONE)
    {
      libssh2_session_set_timeout(Network_getSSHSession(&socketHandle),READ_TIMEOUT);

      error = sftpUnlink(&socketHandle,archiveName);

      Network_disconnect(&socketHandle);
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageInfo);
    UNUSED_VARIABLE(archiveName);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

#if 0
//TODO
Errors StorageSFTP_getInfo(FileInfo          *fileInfo,
                           const StorageInfo *storageInfo,
                           ConstString       archiveName
                          )
{
  Errors       error;
  #ifdef HAVE_SSH2
    SocketHandle socketHandle;
  #endif /* HAVE_SSH2 */

  assert(storageInfo != NULL);
  assert(storageInfo->storageSpecifier.type == STORAGE_TYPE_SFTP);
  assert(fileInfo != NULL);

  memClear(fileInfo,sizeof(FileInfo));

  error = ERROR_UNKNOWN;
  #ifdef HAVE_SSH2
    {
      LIBSSH2_SFTP_ATTRIBUTES sftpAttributes;

      error = Network_connect(&socketHandle,
                              SOCKET_TYPE_SSH,
                              storageInfo->storageSpecifier.hostName,
                              storageInfo->storageSpecifier.hostPort,
                              storageInfo->storageSpecifier.userName,
                              storageInfo->storageSpecifier.password,
                              NULL,  // caData
                              0,     // caLength
                              NULL,  // certData
                              0,     // certLength
                              storageInfo->sftp.publicKey.data,
                              storageInfo->sftp.publicKey.length,
                              storageInfo->sftp.privateKey.data,
                              storageInfo->sftp.privateKey.length,
                                SOCKET_FLAG_NONE
                              | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                              | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                              30*MS_PER_SECOND
                             );
      if (error == ERROR_NONE)
      {
        libssh2_session_set_timeout(Network_getSSHSession(&socketHandle),READ_TIMEOUT);

        error = sftpStat(&socketHandle,
                         archiveName,
                         fileInfo
                        );

        Network_disconnect(&socketHandle);
      }
    }
  #else /* not HAVE_SSH2 */
    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}
#endif

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : StorageSFTP_openDirectoryList
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

LOCAL Errors StorageSFTP_openDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                           const StorageSpecifier     *storageSpecifier,
                                           ConstString                pathName,
                                           const JobOptions           *jobOptions,
                                           ServerConnectionPriorities serverConnectionPriority
                                          )
{
  #ifdef HAVE_SSH2
    AutoFreeList autoFreeList;
    Errors       error;
    int          ssh2ErrorCode;
    SSHServer    sshServer;
    uint         retries;
  #endif /* HAVE_SSH2 */

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(storageSpecifier->type == STORAGE_TYPE_SFTP);
  assert(pathName != NULL);
  assert(jobOptions != NULL);

  UNUSED_VARIABLE(storageSpecifier);
  UNUSED_VARIABLE(serverConnectionPriority);

  #ifdef HAVE_SSH2
    // init variables
    AutoFree_init(&autoFreeList);
    storageDirectoryListHandle->sftp.pathName      = String_new();
    storageDirectoryListHandle->sftp.buffer        = (char*)malloc(MAX_FILENAME_LENGTH);
    if (storageDirectoryListHandle->sftp.buffer == NULL)
    {
      String_delete(storageDirectoryListHandle->sftp.pathName);
      AutoFree_cleanup(&autoFreeList);
      return ERROR_INSUFFICIENT_MEMORY;
    }
    storageDirectoryListHandle->sftp.bufferLength  = 0;
    storageDirectoryListHandle->sftp.entryReadFlag = FALSE;
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.buffer,{ free(storageDirectoryListHandle->sftp.buffer); });
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.pathName,{ String_delete(storageDirectoryListHandle->sftp.pathName); });

    // set pathname
    String_set(storageDirectoryListHandle->sftp.pathName,pathName);

    // get SSH server settings
    storageDirectoryListHandle->sftp.serverId = Configuration_initSSHServerSettings(&sshServer,storageDirectoryListHandle->storageSpecifier.hostName,jobOptions);
    AUTOFREE_ADD(&autoFreeList,&sshServer,{ Configuration_doneSSHServerSettings(&sshServer); });
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.userName)) String_set(storageDirectoryListHandle->storageSpecifier.userName,sshServer.userName);
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.userName)) String_setCString(storageDirectoryListHandle->storageSpecifier.userName,getenv("LOGNAME"));
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.userName)) String_setCString(storageDirectoryListHandle->storageSpecifier.userName,getenv("USER"));
    if (storageDirectoryListHandle->storageSpecifier.hostPort == 0) storageDirectoryListHandle->storageSpecifier.hostPort = sshServer.port;
    if (String_isEmpty(storageDirectoryListHandle->storageSpecifier.hostName))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_HOST_NAME;
    }

    // allocate SSH server
    if (!allocateServer(storageDirectoryListHandle->sftp.serverId,serverConnectionPriority,ALLOCATE_SERVER_TIMEOUT))
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_TOO_MANY_CONNECTIONS;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.serverId,{ freeServer(storageDirectoryListHandle->sftp.serverId); });

    // check if SSH login is possible
    error = ERROR_SSH_AUTHENTICATION;
    if ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && !Password_isEmpty(&storageDirectoryListHandle->storageSpecifier.password))
    {
      error = checkSSHLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.userName,
                            &storageDirectoryListHandle->storageSpecifier.password,
                            sshServer.publicKey.data,
                            sshServer.publicKey.length,
                            sshServer.privateKey.data,
                            sshServer.privateKey.length
                           );
    }
    if ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && !Password_isEmpty(&sshServer.password))
    {
      error = checkSSHLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.userName,
                            &sshServer.password,
                            sshServer.publicKey.data,
                            sshServer.publicKey.length,
                            sshServer.privateKey.data,
                            sshServer.privateKey.length
                           );
      if (error == ERROR_NONE)
      {
        Password_set(&storageDirectoryListHandle->storageSpecifier.password,&sshServer.password);
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION)
    {
      // initialize interactive/default password
      retries = 0;
      while ((Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION) && (retries < MAX_PASSWORD_REQUESTS))
      {
        if (initSSHLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                         storageDirectoryListHandle->storageSpecifier.userName,
                         &storageDirectoryListHandle->storageSpecifier.password,
                         jobOptions,
// TODO:
CALLBACK_(NULL,NULL)//                         CALLBACK_(storageDirectoryListHandle->getNamePasswordFunction,storageDirectoryListHandle->getNamePasswordUserData)
                        )
           )
        {
          error = checkSSHLogin(storageDirectoryListHandle->storageSpecifier.hostName,
                                storageDirectoryListHandle->storageSpecifier.hostPort,
                                storageDirectoryListHandle->storageSpecifier.userName,
                                &storageDirectoryListHandle->storageSpecifier.password,
                                sshServer.publicKey.data,
                                sshServer.publicKey.length,
                                sshServer.privateKey.data,
                                sshServer.privateKey.length
                               );
        }
        retries++;
      }
    }
    if (Error_getCode(error) == ERROR_CODE_SSH_AUTHENTICATION)
    {
      error = (   !Password_isEmpty(&storageDirectoryListHandle->storageSpecifier.password)
               || !Password_isEmpty(&sshServer.password)
               || !Password_isEmpty(&defaultSSHPassword)
              )
                ? ERRORX_(INVALID_SSH_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName))
                : ERRORX_(NO_SSH_PASSWORD,0,"%s",String_cString(storageDirectoryListHandle->storageSpecifier.hostName));
    }
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // store password as default SSH password
    Password_set(&defaultSSHPassword,&storageDirectoryListHandle->storageSpecifier.password);

    // connect
    error = Network_connect(&storageDirectoryListHandle->sftp.socketHandle,
                            SOCKET_TYPE_SSH,
                            storageDirectoryListHandle->storageSpecifier.hostName,
                            storageDirectoryListHandle->storageSpecifier.hostPort,
                            storageDirectoryListHandle->storageSpecifier.userName,
                            &defaultSSHPassword,
                            NULL,  // caData
                            0,     // caLength
                            NULL,  // certData
                            0,     // certLength
                            sshServer.publicKey.data,
                            sshServer.publicKey.length,
                            sshServer.privateKey.data,
                            sshServer.privateKey.length,
                              SOCKET_FLAG_NONE
                            | ((globalOptions.verboseLevel >= 5) ? SOCKET_FLAG_VERBOSE1 : 0)
                            | ((globalOptions.verboseLevel >= 6) ? SOCKET_FLAG_VERBOSE2 : 0),
                            30*MS_PER_SECOND
                           );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.socketHandle,{ Network_disconnect(&storageDirectoryListHandle->sftp.socketHandle); });

    // init SFTP session
    ssh2ErrorCode = 0;
    do
    {
      storageDirectoryListHandle->sftp.sftp = libssh2_sftp_init(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle));
      if (storageDirectoryListHandle->sftp.sftp == NULL)
      {
        ssh2ErrorCode = libssh2_session_last_errno(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle));
        if (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN) Misc_udelay(100LL*US_PER_MS);
      }
    }
    while ((storageDirectoryListHandle->sftp.sftp == NULL) && (ssh2ErrorCode == LIBSSH2_ERROR_EAGAIN));
    if (storageDirectoryListHandle->sftp.sftp == NULL)
    {
      char *ssh2ErrorText;

      ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle),&ssh2ErrorText,NULL,0);
      error = ERRORX_(SSH,
                      ssh2ErrorCode,
                      "%s",
                      ssh2ErrorText
                     );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&storageDirectoryListHandle->sftp.sftp,{ libssh2_sftp_shutdown(storageDirectoryListHandle->sftp.sftp); });

    // open directory for reading
    storageDirectoryListHandle->sftp.sftpHandle = libssh2_sftp_opendir(storageDirectoryListHandle->sftp.sftp,
                                                                       !String_isEmpty(storageDirectoryListHandle->sftp.pathName)
                                                                         ? String_cString(storageDirectoryListHandle->sftp.pathName)
                                                                         : "."
                                                                      );
    if (storageDirectoryListHandle->sftp.sftpHandle == NULL)
    {
      char *ssh2ErrorText;

      ssh2ErrorCode = libssh2_session_last_error(Network_getSSHSession(&storageDirectoryListHandle->sftp.socketHandle),&ssh2ErrorText,NULL,0);
      error = ERRORX_(SSH,
                      ssh2ErrorCode,
                      "%s",
                      ssh2ErrorText
                     );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // free resources
    Configuration_doneSSHServerSettings(&sshServer);
    AutoFree_done(&autoFreeList);

    return ERROR_NONE;
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(storageSpecifier);
    UNUSED_VARIABLE(pathName);
    UNUSED_VARIABLE(jobOptions);
    UNUSED_VARIABLE(serverConnectionPriority);

    return ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
}

/***********************************************************************\
* Name   : StorageSFTP_closeDirectoryList
* Purpose: close storage directory list
* Input  : storageDirectoryListHandle - storage directory list handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void StorageSFTP_closeDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  #ifdef HAVE_SSH2
    (void)libssh2_sftp_closedir(storageDirectoryListHandle->sftp.sftpHandle);
    (void)libssh2_sftp_shutdown(storageDirectoryListHandle->sftp.sftp);
    Network_disconnect(&storageDirectoryListHandle->sftp.socketHandle);
    free(storageDirectoryListHandle->sftp.buffer);
    String_delete(storageDirectoryListHandle->sftp.pathName);
    freeServer(storageDirectoryListHandle->sftp.serverId);
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* HAVE_SSH2 */
}

/***********************************************************************\
* Name   : StorageSFTP_endOfDirectoryList
* Purpose: check if end of storage directory list reached
* Input  : storageDirectoryListHandle - storage directory list handle
* Output : -
* Return : TRUE if not more diretory entries to read, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool StorageSFTP_endOfDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle)
{
  bool endOfDirectoryFlag;

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  endOfDirectoryFlag = TRUE;
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
          if (   !LIBSSH2_SFTP_S_ISDIR(storageDirectoryListHandle->sftp.attributes.permissions)
              || (   ((n != 1) || (strncmp(storageDirectoryListHandle->sftp.buffer,".", 1) != 0))
                  && ((n != 2) || (strncmp(storageDirectoryListHandle->sftp.buffer,"..",2) != 0))
                 )
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
    UNUSED_VARIABLE(storageDirectoryListHandle);
  #endif /* HAVE_SSH2 */

  return endOfDirectoryFlag;
}

/***********************************************************************\
* Name   : StorageSFTP_readDirectoryList
* Purpose: read next storage directory list entry in storage
* Input  : storageDirectoryListHandle - storage directory list handle
*          fileName                   - file name variable
*          fileInfo                   - file info (can be NULL)
* Output : fileName - next file name (including path)
*          fileInfo - next file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors StorageSFTP_readDirectoryList(StorageDirectoryListHandle *storageDirectoryListHandle,
                                           String                     fileName,
                                           FileInfo                   *fileInfo
                                          )
{
  Errors error;

  assert(storageDirectoryListHandle != NULL);
  assert(storageDirectoryListHandle->storageSpecifier.type == STORAGE_TYPE_SFTP);

  error = ERROR_NONE;
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
            if (   !LIBSSH2_SFTP_S_ISDIR(storageDirectoryListHandle->sftp.attributes.permissions)
                || (   ((n != 1) || (strncmp(storageDirectoryListHandle->sftp.buffer,".", 1) != 0))
                    && ((n != 2) || (strncmp(storageDirectoryListHandle->sftp.buffer,"..",2) != 0))
                   )
               )
            {
              storageDirectoryListHandle->sftp.entryReadFlag = TRUE;
              error = ERROR_NONE;
            }
          }
          else
          {
            error = ERROR_(IO,errno);
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
          if      (LIBSSH2_SFTP_S_ISREG(storageDirectoryListHandle->sftp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_FILE;
          }
          else if (LIBSSH2_SFTP_S_ISDIR(storageDirectoryListHandle->sftp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_DIRECTORY;
          }
          else if (LIBSSH2_SFTP_S_ISLNK(storageDirectoryListHandle->sftp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_LINK;
          }
          else if (LIBSSH2_SFTP_S_ISCHR(storageDirectoryListHandle->sftp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_SPECIAL;
            fileInfo->specialType = FILE_SPECIAL_TYPE_CHARACTER_DEVICE;
          }
          else if (LIBSSH2_SFTP_S_ISBLK(storageDirectoryListHandle->sftp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_SPECIAL;
            fileInfo->specialType = FILE_SPECIAL_TYPE_BLOCK_DEVICE;
          }
          else if (LIBSSH2_SFTP_S_ISFIFO(storageDirectoryListHandle->sftp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_SPECIAL;
            fileInfo->specialType = FILE_SPECIAL_TYPE_FIFO;
          }
          else if (LIBSSH2_SFTP_S_ISSOCK(storageDirectoryListHandle->sftp.attributes.permissions))
          {
            fileInfo->type        = FILE_TYPE_SPECIAL;
            fileInfo->specialType = FILE_SPECIAL_TYPE_SOCKET;
          }
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
          fileInfo->permissions     = storageDirectoryListHandle->sftp.attributes.permissions;
          fileInfo->major           = 0;
          fileInfo->minor           = 0;
          memClear(&fileInfo->cast,sizeof(FileCast));
        }

        storageDirectoryListHandle->sftp.entryReadFlag = FALSE;
      }
    }
  #else /* not HAVE_SSH2 */
    UNUSED_VARIABLE(storageDirectoryListHandle);
    UNUSED_VARIABLE(fileName);
    UNUSED_VARIABLE(fileInfo);

    error = ERROR_FUNCTION_NOT_SUPPORTED;
  #endif /* HAVE_SSH2 */
  assert(error != ERROR_UNKNOWN);

  return error;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
