/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: miscellaneous functions
* Systems: all
*
\***********************************************************************/

#define __MISC_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_SYS_WAIT_H
  #include <sys/wait.h>
#endif /* HAVE_SYS_WAIT_H */
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_SYS_IOCTL_H
  #include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */
#ifdef HAVE_TERMIOS_H
  #include <termios.h>
#endif /* HAVE_TERMIOS_H */
#ifdef HAVE_UUID_UUID_H
  #include <uuid/uuid.h>
#endif /* HAVE_UUID_UUID_H */
#ifdef HAVE_PWD_H
  #include <pwd.h>
#endif
#ifdef HAVE_GRP_H
  #include <grp.h>
#endif
#ifdef HAVE_SYSTEMD_SD_ID128_H
  #include <systemd/sd-id128.h>
#endif
#include <errno.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
  #include <windows.h>
  #include <rpcdce.h>
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/files.h"

#include "errors.h"

#include "misc.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL byte machineId[MISC_MACHINE_ID_LENGTH] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : initMachineId
* Purpose: init machine id
* Input  : applicationIdData       - optional application id data
*          applicationIdDataLength - length of application id data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void initMachineId(const byte applicationIdData[], uint applicationIdDataLength)
{
  static enum {NONE,BASE,COMPLETE} state = NONE;

  uint i;
  #if   defined(PLATFORM_LINUX)
    #if   defined(HAVE_SD_ID128_GET_MACHINE_APP_SPECIFIC) || defined(HAVE_SD_ID128_GET_MACHINE)
      sd_id128_t sdApplicationId128;
      sd_id128_t sdId128;
    #endif
    int    handle;
    char   buffer[64];
    char   s[2+1];
    uuid_t uuid;
  #elif defined(PLATFORM_WINDOWS)
    String value;
    UUID   uuid;
  #endif /* PLATFORM_... */

  if (state != COMPLETE)
  {
    #if   defined(PLATFORM_LINUX)
      #ifdef HAVE_SD_ID128_GET_MACHINE_APP_SPECIFIC
        if (state == NONE)
        {
          memCopyFast(sdApplicationId128.bytes,sizeof(sdApplicationId128.bytes),&SD_ID128_NULL,sizeof(SD_ID128_NULL));
          if (applicationIdData != NULL)
          {
            memCopyFast(sdApplicationId128.bytes,sizeof(sdApplicationId128.bytes),applicationIdData,applicationIdDataLength);
          }
          if (sd_id128_get_machine_app_specific(sdApplicationId128,&sdId128) == 0)
          {
            memCopyFast(machineId,MISC_MACHINE_ID_LENGTH,sdId128.bytes,sizeof(sdId128));
            state = COMPLETE;
          }
        }
      #endif

      #ifdef HAVE_SD_ID128_GET_MACHINE
        if (state == NONE)
        {
          if (sd_id128_get_machine(&sdId128) == 0)
          {
            memCopyFast(machineId,MISC_MACHINE_ID_LENGTH,sdId128.bytes,sizeof(sd_id128_t));
            state = BASE;
          }
        }
      #endif

      if (state == NONE)
      {
        // try to read /var/lib/dbus/machine-id
        handle = open("/var/lib/dbus/machine-id",O_RDONLY);
        if (handle != -1)
        {
          if (read(handle,buffer,2*16) == (2*16))
          {
            for (i = 0; i < MISC_MACHINE_ID_LENGTH; i++)
            {
              s[0] = buffer[2*i+0];
              s[1] = buffer[2*i+1];
              s[2] = '\0';
              machineId[i] = strtol(s,NULL,16);
            }
            state = BASE;
          }
          close(handle);
        }
      }

      #ifdef HAVE_UUID_PARSE
        if (state == NONE)
        {
          // try to read /sys/class/dmi/id/product_uuid
          handle = open("/sys/class/dmi/id/product_uuid",O_RDONLY);
          if (handle != -1)
          {
            if (read(handle,buffer,UUID_STR_LEN) == UUID_STR_LEN)
            {
              buffer[UUID_STR_LEN-1] = NUL;
              if (uuid_parse(buffer,uuid) == 0)
              {
                memCopyFast(machineId,MISC_MACHINE_ID_LENGTH,uuid,sizeof(uuid_t));
              }
              state = BASE;
            }
            close(handle);
          }
        }
      #endif

      // integrate application id
      if (state != COMPLETE)
      {
        if (applicationIdData != NULL)
        {
          for (i = 0; i < MISC_MACHINE_ID_LENGTH; i++)
          {
            machineId[i] = machineId[i] ^ applicationIdData[i];
          }
        }
        state = COMPLETE;
      }
    #elif defined(PLATFORM_WINDOWS)
      if (state != COMPLETE)
      {
        value = String_new();
        if (Misc_getRegistryString(value,HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Cryptography","MachineGuid"))
        {
          if (UuidFromString(String_cString(value),&uuid) == RPC_S_OK)
          {
            memCopyFast(machineId,MISC_MACHINE_ID_LENGTH,&uuid,sizeof(uuid));
            state = COMPLETE;
          }
        }
        String_delete(value);
      }
    #endif /* PLATFORM_... */
  }
}

/***********************************************************************\
* Name   : readProcessIO
* Purpose: read process i/o, EOL at LF/CR/BS, skip empty lines
* Input  : fd   - file handle
*          line - line
* Output : line - read line
* Return : TRUE if line read, FALSE otherwise
* Notes  : -
\***********************************************************************/

#if defined(HAVE_PIPE) && defined(HAVE_FORK) && defined(HAVE_WAITPID)
LOCAL bool readProcessIO(int fd, String line)
{
  #if   defined(PLATFORM_LINUX)
    int    n;
  #elif defined(PLATFORM_WINDOWS)
    u_long n;
  #endif /* PLATFORM_... */
  char ch;

  do
  {
    // check if data available
    #if   defined(PLATFORM_LINUX)
      ioctl(fd,FIONREAD,&n);
    #elif defined(PLATFORM_WINDOWS)
// NYI ???
#warning not implemented
    #endif /* PLATFORM_... */

    // read data until EOL found
//TODO: read more than 1 character
    while (n > 0)
    {
      if (read(fd,&ch,1) == 1)
      {
        switch (ch)
        {
          case '\n':
          case '\r':
          case '\b':
            if (String_length(line) > 0L) return TRUE;
            break;
          default:
            String_appendChar(line,ch);
            break;
        }
        n--;
      }
      else
      {
        n = 0;
      }
    }
  }
  while (n > 0);

  return FALSE;
}
#endif /* defined(HAVE_PIPE) && defined(HAVE_FORK) && defined(HAVE_WAITPID) */

/***********************************************************************\
* Name   : execute
* Purpose: execute command
* Input  : command                 - command to execute
*          arguments               - arguments
*          errorText               - error text or NULL
*          stdoutExecuteIOFunction - stdout callback
*          stdoutExecuteIOUserData - stdout callback user data
*          stdoutStripCount        - number of character to strip from
*                                    stdout output text
*          stderrExecuteIOFunction - stderr callback
*          stderrExecuteIOUserData - stderr callback user data
*          stderrStripCount        - number of character to strip from
*                                    stderr output text
* Output : ERROR_NONE or error code
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors execute(const char        *command,
                     const char        *arguments[],
                     const char        *errorText,
                     ExecuteIOFunction stdoutExecuteIOFunction,
                     void              *stdoutExecuteIOUserData,
                     uint              stdoutStripCount,
                     ExecuteIOFunction stderrExecuteIOFunction,
                     void              *stderrExecuteIOUserData,
                     uint              stderrStripCount
                    )
{
  String     text;
  const char * const *s;
  Errors     error;
  int        pipeStdin[2],pipeStdout[2],pipeStderr[2];
  pid_t      pid;
  int        status;
  bool       sleepFlag;
  String     line;
  int        exitcode;
  int        terminateSignal;

  // init variables
  text  = String_new();
  error = ERROR_NONE;

  // get command as text
  if (errorText != NULL)
  {
    String_setCString(text,errorText);
  }
  else
  {
    String_setCString(text,command);
    if ((arguments != NULL) && (arguments[0] != NULL))
    {
      s = &arguments[1];
      while ((*s) != NULL)
      {
        String_joinCString(text,*s,' ' );
        s++;
      }
    }
  }
//fprintf(stderr,"%s,%d: command %s\n",__FILE__,__LINE__,String_cString(text));

  #if defined(HAVE_PIPE) && defined(HAVE_FORK) && defined(HAVE_WAITPID)
    // create i/o pipes
    if (pipe(pipeStdin) != 0)
    {
      error = ERRORX_(IO_REDIRECT_FAIL,errno,"%s",String_cString(text));
      String_delete(text);
      return error;
    }
    if (pipe(pipeStdout) != 0)
    {
      error = ERRORX_(IO_REDIRECT_FAIL,errno,"%s",String_cString(text));
      close(pipeStdin[0]);
      close(pipeStdin[1]);
      String_delete(text);
      return error;
    }
    if (pipe(pipeStderr) != 0)
    {
      error = ERRORX_(IO_REDIRECT_FAIL,errno,"%s",String_cString(text));
      close(pipeStdout[0]);
      close(pipeStdout[1]);
      close(pipeStdin[0]);
      close(pipeStdin[1]);
      String_delete(text);
      return error;
    }

    // do fork to start separated process
    pid = fork();
    if      (pid == 0)
    {
      /* Note: do not use any function here which may synchronize (lock)
               with the main program!
      */

      // close stdin, stdout, and stderr and reassign them to the pipes
      close(STDERR_FILENO);
      close(STDOUT_FILENO);
      close(STDIN_FILENO);

      // redirect stdin/stdout/stderr to pipe
      dup2(pipeStdin[0],STDIN_FILENO);
      dup2(pipeStdout[1],STDOUT_FILENO);
      dup2(pipeStderr[1],STDERR_FILENO);

      /* close unused pipe handles (the pipes are duplicated by fork(), thus
         there are two open ends of the pipes)
      */
      close(pipeStderr[0]);
      close(pipeStdout[0]);
      close(pipeStdin[1]);

      // execute external program
      execvp(command,(char**)arguments);

      // in case exec() fail, return a default exitcode
      exit(1);
    }
    else if (pid < 0)
    {
      error = ERRORX_(EXEC_FAIL,errno,"%s",String_cString(text));

      close(pipeStderr[0]);
      close(pipeStderr[1]);
      close(pipeStdout[0]);
      close(pipeStdout[1]);
      close(pipeStdin[0]);
      close(pipeStdin[1]);
      String_delete(text);
      return error;
    }

    // close unused pipe handles (the pipe is duplicated by fork(), thus there are two open ends of the pipe)
    close(pipeStderr[1]);
    close(pipeStdout[1]);
    close(pipeStdin[0]);

    // read stdout/stderr and wait until process terminate
    line   = String_new();
    status = 0xFFFFFFFF;
    while (   (waitpid(pid,&status,WNOHANG) == 0)
           || (   !WIFEXITED(status)
               && !WIFSIGNALED(status)
              )
          )
    {
      sleepFlag = TRUE;

      if (readProcessIO(pipeStdout[0],line))
      {
        String_remove(line,STRING_BEGIN,stdoutStripCount);
        if (stdoutExecuteIOFunction != NULL) stdoutExecuteIOFunction(line,stdoutExecuteIOUserData);
        String_clear(line);
        sleepFlag = FALSE;
      }
      if (readProcessIO(pipeStderr[0],line))
      {
        String_remove(line,STRING_BEGIN,stderrStripCount);
        if (stderrExecuteIOFunction != NULL) stderrExecuteIOFunction(line,stderrExecuteIOUserData);
        String_clear(line);
        sleepFlag = FALSE;
      }

      if (sleepFlag)
      {
        Misc_mdelay(250);
      }
    }
    while (readProcessIO(pipeStdout[0],line))
    {
      String_remove(line,STRING_BEGIN,stdoutStripCount);
      if (stdoutExecuteIOFunction != NULL) stdoutExecuteIOFunction(line,stdoutExecuteIOUserData);
      String_clear(line);
    }
    while (readProcessIO(pipeStderr[0],line))
    {
      String_remove(line,STRING_BEGIN,stderrStripCount);
      if (stderrExecuteIOFunction != NULL) stderrExecuteIOFunction(line,stderrExecuteIOUserData);
      String_clear(line);
    }
    String_delete(line);

    // close i/o
    close(pipeStderr[0]);
    close(pipeStdout[0]);
    close(pipeStdin[1]);

    // check exit code
    exitcode = -1;
    if      (WIFEXITED(status))
    {
      exitcode = WEXITSTATUS(status);
      if (exitcode != 0)
      {
        error = ERRORX_(EXEC_FAIL,exitcode,"%s",String_cString(text));
        String_delete(text);
        return error;
      }
    }
    else if (WIFSIGNALED(status))
    {
      terminateSignal = WTERMSIG(status);
      error = ERRORX_(EXEC_TERMINATE,0,"%s",String_cString(text),terminateSignal);
      String_delete(text);
      return error;
    }
    else
    {
      String_delete(text);
      return ERROR_UNKNOWN;
    }
// TODO: use PLATFORM_WINDOWS instead WIN32
  #elif defined(WIN32)
#if 0
HANDLE hOutputReadTmp,hOutputRead,hOutputWrite;
    HANDLE hInputWriteTmp,hInputRead,hInputWrite;
    HANDLE hErrorWrite;
    HANDLE hThread;
    DWORD ThreadId;
    SECURITY_ATTRIBUTES sa;


    // Set up the security attributes struct.
    sa.nLength= sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;


    // Create the child output pipe.
    if (!CreatePipe(&hOutputReadTmp,&hOutputWrite,&sa,0))
       DisplayError("CreatePipe");


    // Create a duplicate of the output write handle for the std error
    // write handle. This is necessary in case the child application
    // closes one of its std output handles.
    if (!DuplicateHandle(GetCurrentProcess(),hOutputWrite,
                         GetCurrentProcess(),&hErrorWrite,0,
                         TRUE,DUPLICATE_SAME_ACCESS))
       DisplayError("DuplicateHandle");


    // Create the child input pipe.
    if (!CreatePipe(&hInputRead,&hInputWriteTmp,&sa,0))
       DisplayError("CreatePipe");


    // Create new output read handle and the input write handles. Set
    // the Properties to FALSE. Otherwise, the child inherits the
    // properties and, as a result, non-closeable handles to the pipes
    // are created.
    if (!DuplicateHandle(GetCurrentProcess(),hOutputReadTmp,
                         GetCurrentProcess(),
                         &hOutputRead, // Address of new handle.
                         0,FALSE, // Make it uninheritable.
                         DUPLICATE_SAME_ACCESS))
       DisplayError("DupliateHandle");

    if (!DuplicateHandle(GetCurrentProcess(),hInputWriteTmp,
                         GetCurrentProcess(),
                         &hInputWrite, // Address of new handle.
                         0,FALSE, // Make it uninheritable.
                         DUPLICATE_SAME_ACCESS))
    DisplayError("DupliateHandle");


    // Close inheritable copies of the handles you do not want to be
    // inherited.
    if (!CloseHandle(hOutputReadTmp)) DisplayError("CloseHandle");
    if (!CloseHandle(hInputWriteTmp)) DisplayError("CloseHandle");


    // Get std input handle so you can close it and force the ReadFile to
    // fail when you want the input thread to exit.
    if ( (hStdIn = GetStdHandle(STD_INPUT_HANDLE)) ==
                                              INVALID_HANDLE_VALUE )
       DisplayError("GetStdHandle");

    PrepAndLaunchRedirectedChild(hOutputWrite,hInputRead,hErrorWrite);


    // Close pipe handles (do not continue to modify the parent).
    // You need to make sure that no handles to the write end of the
    // output pipe are maintained in this process or else the pipe will
    // not close when the child process exits and the ReadFile will hang.
    if (!CloseHandle(hOutputWrite)) DisplayError("CloseHandle");
    if (!CloseHandle(hInputRead )) DisplayError("CloseHandle");
    if (!CloseHandle(hErrorWrite)) DisplayError("CloseHandle");


    // Launch the thread that gets the input and sends it to the child.
    hThread = CreateThread(NULL,0,GetAndSendInputThread,
                            (LPVOID)hInputWrite,0,&ThreadId);
    if (hThread == NULL) DisplayError("CreateThread");


    // Read the child's output.
    ReadAndHandleOutput(hOutputRead);
    // Redirection is complete


    // Force the read on the input to return by closing the stdin handle.
    if (!CloseHandle(hStdIn)) DisplayError("CloseHandle");


    // Tell the thread to exit and wait for thread to die.
    bRunThread = FALSE;

    if (WaitForSingleObject(hThread,INFINITE) == WAIT_FAILED)
       DisplayError("WaitForSingleObject");

    if (!CloseHandle(hOutputRead)) DisplayError("CloseHandle");
    if (!CloseHandle(hInputWrite)) DisplayError("CloseHandle");
#endif
  #else /* not defined(HAVE_PIPE) && defined(HAVE_FORK) && defined(HAVE_WAITPID) || PLATFORM_WINDOWS */
    #error pipe()/fork()/waitpid() not available nor Windows system!
  #endif /* defined(HAVE_PIPE) && defined(HAVE_FORK) && defined(HAVE_WAITPID) || PLATFORM_WINDOWS */

  // free resources
  String_delete(text);

  return error;
}

/***********************************************************************\
* Name   : base64Encode
* Purpose: encode base64
* Input  : buffer       - buffer variable
*          bufferSize   - buffer size
*          bufferLength - buffer length variable (can be NULL)
*          data         - data
*          dataLength   - data length
* Output : buffer       - encoded data
*          bufferLength - length of encoded data
* Return : TRUE iff encoded
* Notes  : -
\***********************************************************************/

LOCAL bool base64Encode(char *buffer, ulong bufferSize, ulong *bufferLength, const void *data, ulong dataLength)
{
  const char BASE64_ENCODING_TABLE[] =
  {
    'A','B','C','D','E','F','G','H',
    'I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X',
    'Y','Z','a','b','c','d','e','f',
    'g','h','i','j','k','l','m','n',
    'o','p','q','r','s','t','u','v',
    'w','x','y','z','0','1','2','3',
    '4','5','6','7','8','9','+','/'
  };

  ulong n;
  ulong i;
  byte  b0,b1,b2;
  uint  i0,i1,i2,i3;

  assert(buffer != NULL);

  n = 0;

  if (dataLength > 0)
  {
    // encode 3-byte tupels
    i = 0;
    while ((i+3) <= dataLength)
    {
      b0 = ((i+0) < dataLength) ? ((byte*)data)[i+0] : 0;
      b1 = ((i+1) < dataLength) ? ((byte*)data)[i+1] : 0;
      b2 = ((i+2) < dataLength) ? ((byte*)data)[i+2] : 0;

      i0 = (uint)(b0 & 0xFC) >> 2;
      assert(i0 < 64);
      i1 = (uint)((b0 & 0x03) << 4) | (uint)((b1 & 0xF0) >> 4);
      assert(i1 < 64);
      i2 = (uint)((b1 & 0x0F) << 2) | (uint)((b2 & 0xC0) >> 6);
      assert(i2 < 64);
      i3 = (uint)(b2 & 0x3F);
      assert(i3 < 64);

      if ((n+4) > bufferSize)
      {
        return FALSE;
      }
      buffer[n] = BASE64_ENCODING_TABLE[i0]; n++;
      buffer[n] = BASE64_ENCODING_TABLE[i1]; n++;
      buffer[n] = BASE64_ENCODING_TABLE[i2]; n++;
      buffer[n] = BASE64_ENCODING_TABLE[i3]; n++;

      i += 3;
    }

    // encode last 1,2 bytes
    if      ((i+2) <= dataLength)
    {
      // 2 byte => XYZ=
      b0 = ((byte*)data)[i+0];
      b1 = ((byte*)data)[i+1];

      i0 = (uint)(b0 & 0xFC) >> 2;
      assert(i0 < 64);
      i1 = (uint)((b0 & 0x03) << 4) | (uint)((b1 & 0xF0) >> 4);
      assert(i1 < 64);
      i2 = (uint)((b1 & 0x0F) << 2);
      assert(i2 < 64);

      if ((n+4) > bufferSize)
      {
        return FALSE;
      }
      buffer[n] = BASE64_ENCODING_TABLE[i0]; n++;
      buffer[n] = BASE64_ENCODING_TABLE[i1]; n++;
      buffer[n] = BASE64_ENCODING_TABLE[i2]; n++;
      buffer[n] = '='; n++;
    }
    else if ((i+1) <= dataLength)
    {
      // 1 byte => XY==
      b0 = ((byte*)data)[i+0];

      i0 = (uint)(b0 & 0xFC) >> 2;
      assert(i0 < 64);
      i1 = (uint)((b0 & 0x03) << 4);
      assert(i1 < 64);

      if ((n+4) > bufferSize)
      {
        return FALSE;
      }
      buffer[n] = BASE64_ENCODING_TABLE[i0]; n++;
      buffer[n] = BASE64_ENCODING_TABLE[i1]; n++;
      buffer[n] = '='; n++;
      buffer[n] = '='; n++;
    }
  }

  if (bufferLength != NULL) (*bufferLength) = n;

  return TRUE;
}

/***********************************************************************\
* Name   : base64Decode
* Purpose: decode base64
* Input  : buffer       - buffer variable
*          bufferSize   - buffer size
*          bufferLength - data length variable (can be NULL)
*          s            - base64 encoded string
*          n            - length of base64 encoded string
* Output : data       - data
*          dataLength - length of decoded data
* Return : TRUE iff decoded
* Notes  : -
\***********************************************************************/

LOCAL bool base64Decode(void *buffer, uint bufferSize, uint *bufferLength, const char *s, ulong n)
{
  #define VALID_BASE64_CHAR(ch) (   (((ch) >= 'A') && ((ch) <= 'Z')) \
                                 || (((ch) >= 'a') && ((ch) <= 'z')) \
                                 || (((ch) >= '0') && ((ch) <= '9')) \
                                 || ((ch) == '+') \
                                 || ((ch) == '/') \
                                )

  const byte BASE64_DECODING_TABLE[] =
  {
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,62,0,0,0,63,
    52,53,54,55,56,57,58,59,
    60,61,0,0,0,0,0,0,
    0,0,1,2,3,4,5,6,
    7,8,9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,
    23,24,25,0,0,0,0,0,
    0,26,27,28,29,30,31,32,
    33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,
    49,50,51,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
  };

  uint  length;
  char  c0,c1,c2,c3;
  uint  i0,i1,i2,i3;
  ulong i;
  byte  b0,b1,b2;

  length = 0;

  c0 = 0;
  c1 = 0;
  c2 = 0;
  c3 = 0;
  i0 = 0;
  i1 = 0;
  i2 = 0;
  i3 = 0;
  i  = 0;
  while ((i+4) <= n)
  {
    c0 = s[i+0];
    c1 = s[i+1];
    c2 = s[i+2];
    c3 = s[i+3];

    if (!VALID_BASE64_CHAR(c0)) return FALSE;
    if (!VALID_BASE64_CHAR(c1)) return FALSE;

    if      ((c2 == '=') && (c3 == '='))
    {
      // 1 byte
      i0 = BASE64_DECODING_TABLE[(byte)c0];
      i1 = BASE64_DECODING_TABLE[(byte)c1];

      b0 = (byte)((i0 << 2) | ((i1 & 0x30) >> 4));

      if (length < bufferSize) { ((byte*)buffer)[length] = b0; length++; }
    }
    else if (c3 == '=')
    {
      // 2 bytes
      if (!VALID_BASE64_CHAR(c2)) return FALSE;

      i0 = BASE64_DECODING_TABLE[(byte)c0];
      i1 = BASE64_DECODING_TABLE[(byte)c1];
      i2 = BASE64_DECODING_TABLE[(byte)c2];

      b0 = (byte)((i0 << 2) | ((i1 & 0x30) >> 4));
      b1 = (byte)(((i1 & 0x0F) << 4) | ((i2 & 0x3C) >> 2));

      if (length < bufferSize) { ((byte*)buffer)[length] = b0; length++; }
      if (length < bufferSize) { ((byte*)buffer)[length] = b1; length++; }
    }
    else
    {
      // 3 bytes
      if (!VALID_BASE64_CHAR(c2)) return FALSE;
      if (!VALID_BASE64_CHAR(c3)) return FALSE;

      i0 = BASE64_DECODING_TABLE[(byte)c0];
      i1 = BASE64_DECODING_TABLE[(byte)c1];
      i2 = BASE64_DECODING_TABLE[(byte)c2];
      i3 = BASE64_DECODING_TABLE[(byte)c3];

      b0 = (byte)((i0 << 2) | ((i1 & 0x30) >> 4));
      b1 = (byte)(((i1 & 0x0F) << 4) | ((i2 & 0x3C) >> 2));
      b2 = (byte)(((i2 & 0x03) << 6) | i3);

      if (length < bufferSize) { ((byte*)buffer)[length] = b0; length++; }
      if (length < bufferSize) { ((byte*)buffer)[length] = b1; length++; }
      if (length < bufferSize) { ((byte*)buffer)[length] = b2; length++; }
    }

    i += 4;
  }
  if (bufferLength != NULL) (*bufferLength) = length;

  return TRUE;

  #undef VALID_BASE64_CHAR
}

/***********************************************************************\
* Name   : hexDecode
* Purpose: decode hex-string into data
* Input  : data          - data variable
*          dataLength    - length of data
*          s             - hex encoded string
*          n             - length of hex encoded string
*          maxDataLength - max. data length
* Output : data - data
* Return : length of decoded data
* Notes  : -
\***********************************************************************/

LOCAL bool hexDecode(byte *data, uint *dataLength, const char *s, ulong n, uint maxDataLength)
{
  uint length;
  char t[3];
  char *w;

  assert(s != NULL);
  assert(data != NULL);

  length = 0;

  while (((*s) != '\0') && (length < n))
  {
    t[0] = (*s); s++;
    if ((*s) != '\0')
    {
      t[1] = (*s); s++;
      t[2] = '\0';

      if (length < maxDataLength) { data[length] = (byte)strtol(t,&w,16); if ((*w) != '\0') break; length++; }
    }
    else
    {
      break;
    }
  }
  if (dataLength != NULL) (*dataLength) = length;

  return TRUE;
}

/*---------------------------------------------------------------------*/

uint64 Misc_getRandom(uint64 min, uint64 max)
{
  uint n;

  srand(time(NULL));

  n = max-min;

  return min+(  (((uint64)(rand() & 0xFFFF)) << 48)
              | (((uint64)(rand() & 0xFFFF)) << 32)
              | (((uint64)(rand() & 0xFFFF)) << 16)
              | (((uint64)(rand() & 0xFFFF)) <<  0)
             )%n;
}

uint64 Misc_getTimestamp(void)
{
  struct timeval tv;

  if (gettimeofday(&tv,NULL) == 0)
  {
    return (uint64)tv.tv_usec+((uint64)tv.tv_sec)*US_PER_S;
  }
  else
  {
    return 0LL;
  }
}

uint64 Misc_getCurrentDateTime(void)
{
  uint64 dateTime;
  struct timeval tv;

  if (gettimeofday(&tv,NULL) == 0)
  {
    dateTime = (uint64)tv.tv_sec;
  }
  else
  {
    dateTime = 0LL;
  }

  return dateTime;
}

uint64 Misc_getCurrentDate(void)
{
  uint64 date;
  struct timeval tv;

  if (gettimeofday(&tv,NULL) == 0)
  {
    date = (uint64)(tv.tv_sec-tv.tv_sec%S_PER_DAY);
  }
  else
  {
    date = 0LL;
  }

  return date;
}

uint32 Misc_getCurrentTime(void)
{
  uint64 time;
  struct timeval tv;

  if (gettimeofday(&tv,NULL) == 0)
  {
    time = (uint64)(tv.tv_sec%S_PER_DAY);
  }
  else
  {
    time = 0LL;
  }

  return time;
}

void Misc_splitDateTime(uint64   dateTime,
                        uint     *year,
                        uint     *month,
                        uint     *day,
                        uint     *hour,
                        uint     *minute,
                        uint     *second,
                        WeekDays *weekDay
                       )
{
  time_t    n;
  #ifdef HAVE_LOCALTIME_R
    struct tm tmBuffer;
  #endif /* HAVE_LOCALTIME_R */
  struct tm *tm;

  n = (time_t)dateTime;
  #ifdef HAVE_LOCALTIME_R
    tm = localtime_r(&n,&tmBuffer);
  #else /* not HAVE_LOCALTIME_R */
    tm = localtime(&n);
  #endif /* HAVE_LOCALTIME_R */
  assert(tm != NULL);

  if (year    != NULL) (*year)    = tm->tm_year + 1900;
  if (month   != NULL) (*month)   = tm->tm_mon + 1;
  if (day     != NULL) (*day)     = tm->tm_mday;
  if (hour    != NULL) (*hour)    = tm->tm_hour;
  if (minute  != NULL) (*minute)  = tm->tm_min;
  if (second  != NULL) (*second)  = tm->tm_sec;
  if (weekDay != NULL) (*weekDay) = (tm->tm_wday + WEEKDAY_SUN) % 7;
}

uint64 Misc_parseDateTime(const char *string)
{
  #if defined(HAVE_STRPTIME)
    const char *DATE_TIME_FORMATS[] =
    {
      "%Y-%m-%dT%H:%i:%s%Q",       // 2011-03-11T14:46:23-06:00

      "%A, %d %b %y %H:%M:%S %z",  // Fri, 11 Mar 11 14:46:23 -0500
      "%a, %d %b %y %H:%M:%S %z",  // Friday, 11 Mar 11 14:46:23 -0500
      "%A, %d %B %y %H:%M:%S %z",  // Fri, 11 Mar 11 14:46:23 -0500
      "%a, %d %B %y %H:%M:%S %z",  // Friday, 11 Mar 11 14:46:23 -0500
      "%A, %d %b %Y %H:%M:%S %z",  // Fri, 11 Mar 2011 14:46:23 -0500
      "%a, %d %b %Y %H:%M:%S %z",  // Friday, 11 Mar 2011 14:46:23 -0500
      "%A, %d %B %Y %H:%M:%S %z",  // Fri, 11 Mar 2011 14:46:23 -0500
      "%a, %d %B %Y %H:%M:%S %z",  // Friday, 11 Mar 2011 14:46:23 -0500

      "%A, %d-%b-%y %H:%M:%S UTC", // Fri, 11-Mar-11 14:46:23 UTC
      "%a, %d-%b-%y %H:%M:%S UTC", // Friday, 11-Mar-11 14:46:23 UTC
      "%A, %d-%B-%y %H:%M:%S UTC", // Fri, 11-March-11 14:46:23 UTC
      "%a, %d-%B-%y %H:%M:%S UTC", // Friday, 11-March-11 14:46:23 UTC
      "%A, %d-%b-%Y %H:%M:%S UTC", // Fri, 11-Mar-2011 14:46:23 UTC
      "%a, %d-%b-%Y %H:%M:%S UTC", // Friday, 11-Mar-2-11 14:46:23 UTC
      "%A, %d-%B-%Y %H:%M:%S UTC", // Fri, 11-March-2011 14:46:23 UTC
      "%a, %d-%B-%Y %H:%M:%S UTC", // Friday, 11-March-2011 14:46:23 UTC

      "%A, %d %b %y %H:%M:%S GMT",  // Fri, 11 Mar 11 14:46:23 GMT
      "%a, %d %b %y %H:%M:%S GMT",  // Friday, 11 Mar 11 14:46:23 GMT
      "%A, %d %B %y %H:%M:%S GMT",  // Fri, 11 March 11 14:46:23 GMT
      "%a, %d %B %y %H:%M:%S GMT",  // Friday, 11 March 11 14:46:23 GMT
      "%A, %d %b %Y %H:%M:%S GMT",  // Fri, 11 Mar 2011 14:46:23 GMT
      "%a, %d %b %Y %H:%M:%S GMT",  // Friday, 11 Mar 2011 14:46:23 GMT
      "%A, %d %B %Y %H:%M:%S GMT",  // Fri, 11 March 2011 14:46:23 GMT
      "%a, %d %B %Y %H:%M:%S GMT",  // Friday, 11 March 2011 14:46:23 GMT

       DATE_TIME_FORMAT_DEFAULT
    };
  #endif /* HAVE_STRPTIME */

  #if defined(HAVE_GETDATE_R) || defined(HAVE_STRPTIME)
    struct tm tmBuffer;
  #endif /* HAVE_GETDATE_R */
  struct tm  *tm;
  #if defined(HAVE_STRPTIME)
    uint       i;
    const char *s;
  #endif /* HAVE_STRPTIME */
  uint64     dateTime;

  assert(string != NULL);

  #if   defined(HAVE_GETDATE_R)
    memClear(&tmBuffer,sizeof(tmBuffer));
    tm = (getdate_r(string,&tmBuffer) == 0) ? &tmBuffer : NULL;
  #elif defined(HAVE_GETDATE)
    tm = getdate(string);
   #else
#ifndef WERROR
#warning implement strptime
#endif
//TODO: use http://cvsweb.netbsd.org/bsdweb.cgi/src/lib/libc/time/strptime.c?rev=HEAD
    tm = NULL;
  #endif /* HAVE_GETDATE... */

  if (tm == NULL)
  {
    #ifdef HAVE_STRPTIME
      memClear(&tmBuffer,sizeof(tmBuffer));
      i = 0;
      while ((i < SIZE_OF_ARRAY(DATE_TIME_FORMATS)) && (tm == NULL))
      {
        s = (const char*)strptime(string,DATE_TIME_FORMATS[i],&tmBuffer);
        if ((s != NULL) && ((*s) == '\0'))
        {
          tm = &tmBuffer;
        }
        i++;
      }
    #else
#ifndef WERROR
#warning implement strptime
#endif
//TODO: use http://cvsweb.netbsd.org/bsdweb.cgi/src/lib/libc/time/strptime.c?rev=HEAD
    #endif
  }

  if (tm != NULL)
  {
    dateTime = (uint64)mktime(tm);
  }
  else
  {
    dateTime = 0LL;
  }

  return dateTime;
}

String Misc_formatDateTime(String string, uint64 dateTime, const char *format)
{
  #define START_BUFFER_SIZE 256
  #define DELTA_BUFFER_SIZE 64

  time_t    n;
  #ifdef HAVE_LOCALTIME_R
    struct tm tmBuffer;
  #endif /* HAVE_LOCALTIME_R */
  struct tm *tm;
  char      *buffer;
  uint      bufferSize;
  int       length;

  assert(string != NULL);

  n = (time_t)dateTime;
  #ifdef HAVE_LOCALTIME_R
    tm = localtime_r(&n,&tmBuffer);
  #else /* not HAVE_LOCALTIME_R */
    tm = localtime(&n);
  #endif /* HAVE_LOCALTIME_R */
  assert(tm != NULL);

  if (format == NULL) format = "%c";

  // allocate buffer and format date/time
  bufferSize = START_BUFFER_SIZE;
  do
  {
    buffer = (char*)malloc(bufferSize);
    if (buffer == NULL)
    {
      return NULL;
    }
    length = strftime(buffer,bufferSize-1,format,tm);
    if (length == 0)
    {
      free(buffer);
      bufferSize += DELTA_BUFFER_SIZE;
    }
  }
  while (length == 0);
  buffer[length] = '\0';

  // append to string
  String_appendBuffer(string,buffer,length);

  // free resources
  free(buffer);

  return string;
}

const char* Misc_formatDateTimeCString(char *buffer, uint bufferSize, uint64 dateTime, const char *format)
{
  time_t    n;
  #ifdef HAVE_LOCALTIME_R
    struct tm tmBuffer;
  #endif /* HAVE_LOCALTIME_R */
  struct tm *tm;
  int       length;

  assert(buffer != NULL);
  assert(bufferSize > 0);

  n = (time_t)dateTime;
  #ifdef HAVE_LOCALTIME_R
    tm = localtime_r(&n,&tmBuffer);
  #else /* not HAVE_LOCALTIME_R */
    tm = localtime(&n);
  #endif /* HAVE_LOCALTIME_R */
  assert(tm != NULL);

  if (format == NULL) format = "%c";

  // allocate buffer and format date/time
  length = strftime(buffer,bufferSize-1,format,tm);
  if (length == 0)
  {
    return NULL;
  }
  buffer[length] = '\0';

  return buffer;
}

uint64 Misc_makeDateTime(uint year,
                         uint month,
                         uint day,
                         uint hour,
                         uint minute,
                         uint second
                        )
{
  struct tm tmStruct;

  assert(year >= 1900);
  assert(month >= 1);
  assert(month <= 12);
  assert(day >= 1);
  assert(day <= 31);
  assert(hour <= 23);
  assert(minute <= 59);
  assert(second <= 59);

  tmStruct.tm_year = year - 1900;
  tmStruct.tm_mon  = month - 1;
  tmStruct.tm_mday = day;
  tmStruct.tm_hour = hour;
  tmStruct.tm_min  = minute;
  tmStruct.tm_sec  = second;

  return (uint64)mktime(&tmStruct);
}

void Misc_udelay(uint64 time)
{
  #if   defined(PLATFORM_LINUX)
    #if   defined(HAVE_USLEEP)
    #elif defined(HAVE_NANOSLEEP)
      struct timespec ts;
    #endif /* HAVE_NANOSLEEP */
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  // Note: usleep() seems not work on MinGW
  #if   defined(PLATFORM_LINUX)
    #if   defined(HAVE_USLEEP)
      usleep(time);
    #elif defined(HAVE_NANOSLEEP)
      ts.tv_sec  = (ulong)(time/1000000LL);
      ts.tv_nsec = (ulong)((time%1000000LL)*1000);
      while (   (nanosleep(&ts,&ts) == -1)
             && (errno == EINTR)
            )
      {
        // nothing to do
      }
    #else
      #error usleep()/nanosleep() not available nor Windows system!
    #endif
  #elif defined(PLATFORM_WINDOWS)
    Sleep((time+1000L-1L)/1000LL);
  #endif /* PLATFORM_... */
}

/*---------------------------------------------------------------------*/

uint32 Misc_userNameToUserId(const char *name)
{
  #define BUFFER_DELTA_SIZE 1024
  #define MAX_BUFFER_SIZE   (64*1024)

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWNAM_R)
    long          bufferSize;
    char          *buffer,*newBuffer;
    struct passwd passwordEntry;
    struct passwd *result;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETPWNAM_R) */
  uint32        userId;

  assert(name != NULL);

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWNAM_R)
    // allocate buffer
    bufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufferSize == -1L)
    {
      return FILE_DEFAULT_USER_ID;
    }
    buffer = (char*)malloc(bufferSize);
    if (buffer == NULL)
    {
      return FILE_DEFAULT_USER_ID;
    }

    // get user passwd entry
    while (getpwnam_r(name,&passwordEntry,buffer,bufferSize,&result) != 0)
    {
      if ((errno != ERANGE) || ((bufferSize+BUFFER_DELTA_SIZE) >= MAX_BUFFER_SIZE))
      {
        free(buffer);
        return FILE_DEFAULT_USER_ID;
      }
      else
      {
        // Note: returned size may not be enough. Increase buffer size.
        newBuffer = (char*)realloc(buffer,bufferSize+BUFFER_DELTA_SIZE);
        if (newBuffer == NULL)
        {
          free(buffer);
          return FILE_DEFAULT_USER_ID;
        }
        buffer     =  newBuffer;
        bufferSize += BUFFER_DELTA_SIZE;
      }
    }

    // get user id
    userId = (result != NULL) ? result->pw_uid : FILE_DEFAULT_USER_ID;

    // free resources
    free(buffer);
  #else /* not defined(HAVE_SYSCONF) && defined(HAVE_GETPWNAM_R) */
    UNUSED_VARIABLE(name);

    userId = FILE_DEFAULT_USER_ID;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETPWNAM_R) */

  return userId;

  #undef BUFFER_DELTA_SIZE
  #undef MAX_BUFFER_SIZE
}

const char *Misc_userIdToUserName(char *name, uint nameSize, uint32 userId)
{
  #define BUFFER_DELTA_SIZE 1024
  #define MAX_BUFFER_SIZE   (64*1024)

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R)
    long          bufferSize;
    char          *buffer,*newBuffer;
    struct passwd groupEntry;
    struct passwd *result;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R) */

  assert(name != NULL);
  assert(nameSize > 0);

  stringClear(name);

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R)
    // allocate buffer
    bufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufferSize == -1L)
    {
      return NULL;
    }
    buffer = (char*)malloc(bufferSize);
    if (buffer == NULL)
    {
      return NULL;
    }

    // get user passwd entry
    while (getpwuid_r((uid_t)userId,&groupEntry,buffer,bufferSize,&result) != 0)
    {
      if ((errno != ERANGE) || ((bufferSize+BUFFER_DELTA_SIZE) >= MAX_BUFFER_SIZE))
      {
        free(buffer);
        return NULL;
      }
      else
      {
        // Note: returned size may not be enough. Increase buffer size.
        newBuffer = (char*)realloc(buffer,bufferSize+BUFFER_DELTA_SIZE);
        if (newBuffer == NULL)
        {
          free(buffer);
          return NULL;
        }
        buffer     =  newBuffer;
        bufferSize += BUFFER_DELTA_SIZE;
      }
    }

    // get user name
    if (result != NULL)
    {
      strncpy(name,result->pw_name,nameSize);
    }
    else
    {
      strncpy(name,"NONE",nameSize);
    }
    name[nameSize-1] = NUL;

    // free resources
    free(buffer);
  #else /* not defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R) */
    UNUSED_VARIABLE(userId);

    strncpy(name,"NONE",nameSize);
    name[nameSize-1] = NUL;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R) */

  return name;

  #undef BUFFER_DELTA_SIZE
  #undef MAX_BUFFER_SIZE
}

uint32 Misc_groupNameToGroupId(const char *name)
{
  #define BUFFER_DELTA_SIZE 1024
  #define MAX_BUFFER_SIZE   (64*1024)

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETGRNAM_R)
    long         bufferSize;
    char         *buffer,*newBuffer;
    struct group groupEntry;
    struct group *result;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R) */
  uint32       groupId;

  assert(name != NULL);

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETGRNAM_R)
    // allocate buffer
    bufferSize = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (bufferSize == -1L)
    {
      return FILE_DEFAULT_GROUP_ID;
    }
    buffer = (char*)malloc(bufferSize);
    if (buffer == NULL)
    {
      return FILE_DEFAULT_GROUP_ID;
    }

    // get user passwd entry
    while (getgrnam_r(name,&groupEntry,buffer,bufferSize,&result) != 0)
    {
      if ((errno != ERANGE) || ((bufferSize+BUFFER_DELTA_SIZE) >= MAX_BUFFER_SIZE))
      {
        free(buffer);
        return FILE_DEFAULT_GROUP_ID;
      }
      else
      {
        // Note: returned size may not be enough. Increase buffer size.
        newBuffer = (char*)realloc(buffer,bufferSize+BUFFER_DELTA_SIZE);
        if (newBuffer == NULL)
        {
          free(buffer);
          return FILE_DEFAULT_GROUP_ID;
        }
        buffer     =  newBuffer;
        bufferSize += BUFFER_DELTA_SIZE;
      }
    }

    // get group id
    groupId = (result != NULL) ? result->gr_gid : FILE_DEFAULT_GROUP_ID;

    // free resources
    free(buffer);
  #else /* not defined(HAVE_SYSCONF) && defined(HAVE_GETGRNAM_R) */
    UNUSED_VARIABLE(name);

    groupId = FILE_DEFAULT_GROUP_ID;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETGRNAM_R) */

  return groupId;

  #undef BUFFER_DELTA_SIZE
  #undef MAX_BUFFER_SIZE
}

const char *Misc_groupIdToGroupName(char *name, uint nameSize, uint32 groupId)
{
  #define BUFFER_DELTA_SIZE 1024
  #define MAX_BUFFER_SIZE   (64*1024)

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETPWUID_R)
    long         bufferSize;
    char         *buffer,*newBuffer;
    struct group groupEntry;
    struct group *result;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETGRGID_R) */

  assert(name != NULL);
  assert(nameSize > 0);

  stringClear(name);

  #if defined(HAVE_SYSCONF) && defined(HAVE_GETGRGID_R)
    // allocate buffer
    bufferSize = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (bufferSize == -1L)
    {
      return NULL;
    }
    buffer = (char*)malloc(bufferSize);
    if (buffer == NULL)
    {
      return NULL;
    }

    // get user passwd entry
    while (getgrgid_r((gid_t)groupId,&groupEntry,buffer,bufferSize,&result) != 0)
    {
      if ((errno != ERANGE) || ((bufferSize+BUFFER_DELTA_SIZE) >= MAX_BUFFER_SIZE))
      {
        free(buffer);
        return NULL;
      }
      else
      {
        // Note: returned size may not be enough. Increase buffer size.
        newBuffer = (char*)realloc(buffer,bufferSize+BUFFER_DELTA_SIZE);
        if (newBuffer == NULL)
        {
          free(buffer);
          return NULL;
        }
        buffer     =  newBuffer;
        bufferSize += BUFFER_DELTA_SIZE;
      }
    }

    // get group name
    if (result != NULL)
    {
      strncpy(name,result->gr_name,nameSize);
    }
    else
    {
      strncpy(name,"NONE",nameSize);
    }
    name[nameSize-1] = NUL;

    // free resources
    free(buffer);
  #else /* not defined(HAVE_SYSCONF) && defined(HAVE_GETGRGID_R) */
    UNUSED_VARIABLE(groupId);

    strncpy(name,"NONE",nameSize);
    name[nameSize-1] = NUL;
  #endif /* defined(HAVE_SYSCONF) && defined(HAVE_GETGRGID_R) */

  return name;

  #undef BUFFER_DELTA_SIZE
  #undef MAX_BUFFER_SIZE
}

String Misc_getCurrentUserName(String string)
{
  #ifdef HAVE_GETLOGIN_R
    char buffer[256];
  #endif

  #ifdef HAVE_GETLOGIN_R
    if (getlogin_r(buffer,sizeof(buffer)) == 0)
    {
      String_setCString(string,buffer);
    }
    else
    {
      String_clear(string);
    }
  #else
//TODO: not available on Windows?
    String_setCString(string,getlogin());
  #endif

  return string;
}

/*---------------------------------------------------------------------*/

uint Misc_getId(void)
{
  static uint id = 1;

  return atomicIncrement(&id,1);
}

String Misc_getUUID(String string)
{
  char buffer[64];

  assert(string != NULL);

  return String_setCString(string,Misc_getUUIDCString(buffer,sizeof(buffer)));
}

const char *Misc_getUUIDCString(char *buffer, uint bufferSize)
{
  #if   defined(PLATFORM_LINUX)
    #if HAVE_UUID_GENERATE
      uuid_t uuid;
      char   s[36+1];
    #else /* not HAVE_UUID_GENERATE */
      FILE *file;
      char *s;
    #endif /* HAVE_UUID_GENERATE */
  #elif defined(PLATFORM_WINDOWS)
    UUID     uuid;
    RPC_CSTR *rpcString;
  #endif /* PLATFORM_... */

  assert(buffer != NULL);
  assert(bufferSize > 0);

  stringClear(buffer);

  #if   defined(PLATFORM_LINUX)
    #if HAVE_UUID_GENERATE
      uuid_generate(uuid);

      uuid_unparse_lower(uuid,s);
      s[36] = '\0';

      strncpy(buffer,s,bufferSize-1);
      buffer[bufferSize-1] = '\0';
    #else /* not HAVE_UUID_GENERATE */
      file = fopen("/proc/sys/kernel/random/uuid","r");
      if (file != NULL)
      {
        // read kernel uuid device
        if (fgets(buffer,bufferSize,file) == NULL) { /* ignored */ };
        fclose(file);

        // remove trailing white spaces
        s = buffer;
        while ((*s) != '\0')
        {
          s++;
        }
        do
        {
          (*s) = '\0';
          s--;
        }
        while ((s >= buffer) && isspace(*s));
      }
    #endif /* HAVE_UUID_GENERATE */
  #elif defined(PLATFORM_WINDOWS)
    if (UuidCreate(&uuid) == RPC_S_OK)
    {
      if (UuidToString(&uuid,&rpcString) == RPC_S_OK)
      {
        stringSet(buffer,bufferSize,rpcString);
        RpcStringFree(&rpcString);
      }
    }
  #endif /* PLATFORM_... */

  return buffer;
}

void Misc_setApplicationId(const byte data[], uint length)
{
  initMachineId(data,length);
}

void Misc_setApplicationIdCString(const char *data)
{
  initMachineId((const byte*)data,stringLength(data));
}

MachineId Misc_getMachineId(void)
{
  initMachineId(NULL,0);
  return machineId;
}

/*---------------------------------------------------------------------*/

String Misc_expandMacros(String           string,
                         const char       *templateString,
                         ExpandMacroModes expandMacroMode,
                         const TextMacro  macros[],
                         uint             macroCount,
                         bool             expandMacroCharacter
                        )
{
  #define APPEND_CHAR(string,index,ch) \
    do \
    { \
      if ((index) < sizeof(string)-1) \
      { \
        (string)[index] = ch; \
        (index)++; \
      } \
    } \
    while (0)

  #define SKIP_SPACES(string,i) \
    do \
    { \
      while (   ((string)[i] != '\0') \
             && isspace((string)[i]) \
            ) \
      { \
        (i)++; \
      } \
    } \
    while (0)

  String          expanded;
  bool            macroFlag;
  ulong           i;
  uint            j;
  char            name[128];
  char            format[128];
  CStringIterator cStringIterator;
  Codepoint       codepoint;
  StringIterator  stringIterator;
  char            ch;

  assert(string != NULL);
  assert(templateString != NULL);
  assert((macroCount == 0) || (macros != NULL));

  expanded = String_new();
  i = 0;
  do
  {
    // add prefix string
    macroFlag = FALSE;
    while ((templateString[i] != '\0') && !macroFlag)
    {
      if (templateString[i] == '%')
      {
        switch (templateString[i+1])
        {
          case '%':
            // escaped %
            if (expandMacroCharacter)
            {
              String_appendChar(expanded,'%');
            }
            else
            {
              String_appendCString(expanded,"%%");
            }
            i+=2;
            break;
          case ':':
            // escaped :
            String_appendChar(expanded,':');
            i+=2;
            break;
          default:
            // macro %
            macroFlag = TRUE;
            i++;
            break;
        }
      }
      else
      {
        String_appendChar(expanded,templateString[i]);
        i++;
      }
    }

    if (macroFlag)
    {
      // skip spaces
      SKIP_SPACES(templateString,i);

      // get macro name
      j = 0;
      if (   (templateString[i] != '\0')
          && isalpha(templateString[i])
         )
      {
        APPEND_CHAR(name,j,'%');
        do
        {
          APPEND_CHAR(name,j,templateString[i]);
          i++;
        }
        while (   (templateString[i] != '\0')
               && isalnum(templateString[i])
              );
      }
      name[j] = '\0';

      // get format data (if any)
      j = 0;
      if (templateString[i] == ':')
      {
        // skip ':'
        i++;

        // skip spaces
        SKIP_SPACES(templateString,i);

        // get format string
        APPEND_CHAR(format,j,'%');
        while (   (templateString[i] != '\0')
               && (   isdigit(templateString[i])
                   || (templateString[i] == '-')
                   || (templateString[i] == '.')
                  )
              )
        {
          APPEND_CHAR(format,j,templateString[i]);
          i++;
        }
        while (   (templateString[i] != '\0')
               && (strchr("l",templateString[i]) != NULL)
              )
        {
          APPEND_CHAR(format,j,templateString[i]);
          i++;
        }
        if (   (templateString[i] != '\0')
            && (strchr("duxfsS",templateString[i]) != NULL)
           )
        {
          APPEND_CHAR(format,j,templateString[i]);
          i++;
        }
      }
      format[j] = '\0';

      // find macro
      if (!stringIsEmpty(name))
      {
        j = 0;
        while (   (j < macroCount)
               && !stringEquals(name,macros[j].name)
              )
        {
          j++;
        }

        if (j < macroCount)
        {
          switch (expandMacroMode)
          {
            case EXPAND_MACRO_MODE_STRING:
              // get default format if no format given
              if (stringIsEmpty(format))
              {
                switch (macros[j].type)
                {
                  case TEXT_MACRO_TYPE_INTEGER:
                    stringSet(format,sizeof(format),"%d");
                    break;
                  case TEXT_MACRO_TYPE_INTEGER64:
                    stringSet(format,sizeof(format),"%"PRIi64);
                    break;
                  case TEXT_MACRO_TYPE_DOUBLE:
                    stringSet(format,sizeof(format),"%lf");
                    break;
                  case TEXT_MACRO_TYPE_CSTRING:
                    stringSet(format,sizeof(format),"%s");
                    break;
                  case TEXT_MACRO_TYPE_STRING:
                    stringSet(format,sizeof(format),"%S");
                    break;
                  #ifndef NDEBUG
                    default:
                      HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                      break; /* not reached */
                  #endif /* NDEBUG */
                }
              }

              // expand macro into string
              switch (macros[j].type)
              {
                case TEXT_MACRO_TYPE_INTEGER:
                  String_appendFormat(expanded,format,macros[j].value.i);
                  break;
                case TEXT_MACRO_TYPE_INTEGER64:
                  String_appendFormat(expanded,format,macros[j].value.l);
                  break;
                case TEXT_MACRO_TYPE_DOUBLE:
                  String_appendFormat(expanded,format,macros[j].value.d);
                  break;
                case TEXT_MACRO_TYPE_CSTRING:
                  if (expandMacroCharacter)
                  {
                    String_appendFormat(expanded,format,macros[j].value.s);
                  }
                  else
                  {
                    CSTRING_CHAR_ITERATE(macros[j].value.s,cStringIterator,codepoint)
                    {
                      if (codepoint != '%')
                      {
                        String_appendChar(expanded,codepoint);
                      }
                      else
                      {
                        String_appendCString(expanded,"%%");
                      }
                    }
                  }
                  break;
                case TEXT_MACRO_TYPE_STRING:
                  if (expandMacroCharacter)
                  {
                    String_appendFormat(expanded,format,macros[j].value.string);
                  }
                  else
                  {
                    STRING_CHAR_ITERATE(macros[j].value.string,stringIterator,ch)
                    {
                      if (ch != '%')
                      {
                        String_appendChar(expanded,ch);
                      }
                      else
                      {
                        String_appendCString(expanded,"%%");
                      }
                    }
                  }
                  break;
                #ifndef NDEBUG
                  default:
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                    break; /* not reached */
                #endif /* NDEBUG */
              }
              break;
            case EXPAND_MACRO_MODE_PATTERN:
              // expand macro into pattern
              String_appendCString(expanded,macros[j].pattern);
              break;
            #ifndef NDEBUG
              default:
                HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                break; /* not reached */
              #endif /* NDEBUG */
          }
        }
        else
        {
          // keep unknown macro
          String_appendCString(expanded,name);
        }
      }
      else
      {
        // empty macro: expand with empty value
        switch (expandMacroMode)
        {
          case EXPAND_MACRO_MODE_STRING:
            // get default format if no format given
            if (stringIsEmpty(format))
            {
              stringSet(format,sizeof(format),"%s");
            }

            // expand macro into string
            String_appendFormat(expanded,format,"");
            break;
          case EXPAND_MACRO_MODE_PATTERN:
            // expand macro into pattern
            String_appendCString(expanded,"\\s*");
            break;
          #ifndef NDEBUG
            default:
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
              break; /* not reached */
            #endif /* NDEBUG */
        }
      }
    }
  }
  while (macroFlag);

  // store result
  String_set(string,expanded);

  // free resources
  String_delete(expanded);

  return string;

  #undef SKIP_SPACES
  #undef APPEND_CHAR
}

/*---------------------------------------------------------------------*/

uint Misc_waitHandle(int        handle,
                     SignalMask *signalMask,
                     uint       events,
                     long       timeout
                    )
{
  #if  defined(PLATFORM_LINUX)
    struct pollfd   pollfds[1];
    struct timespec pollTimeout;
  #elif defined(PLATFORM_WINDOWS)
    #ifdef HAVE_WSAPOLL
      WSAPOLLFD pollfds[1];
    #else /* not HAVE_WSAPOLL */
      fd_set          readfds;
      fd_set          writefds;
      fd_set          exceptionfds;
      #ifdef HAVE_PSELECT
        struct timespec selectTimeout;
      #else /* not HAVE_PSELECT */
        struct timeval selectTimeout;
      #endif /* HAVE_PSELECT */
      int             n;
    #endif /* HAVE_WSAPOLL */
  #endif /* PLATFORM_... */

  assert(handle >= 0);

  #if   defined(PLATFORM_LINUX)
    pollfds[0].fd       = handle;
    pollfds[0].events   = events;
    pollfds[0].revents  = 0;
    pollTimeout.tv_sec  = (long)(timeout /MS_PER_SECOND);
    pollTimeout.tv_nsec = (long)((timeout%MS_PER_SECOND)*NS_PER_MS);
    events = (ppoll(pollfds,1,&pollTimeout,signalMask) > 0) ? pollfds[0].revents : 0;
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(signalMask);
    #ifdef HAVE_WSAPOLL
      pollfds[0].fd      = handle;
      pollfds[0].events  = events;
      pollfds[0].revents = 0;
      events = (WSAPoll(pollfds,1,timeout) > 0) ? pollfds[0].revents : 0;
    #else /* not HAVE_WSAPOLL */
      FD_ZERO(&readfds);
      FD_ZERO(&writefds);
      FD_ZERO(&exceptionfds);
      FD_SET(handle,&readfds);
      FD_SET(handle,&writefds);
      FD_SET(handle,&exceptionfds);
      #ifdef HAVE_PSELECT
        selectTimeout.tv_sec  = (long)(timeout/MS_PER_SECOND);
        selectTimeout.tv_nsec = (long)(timeout%MS_PER_SECOND)*NS_PER_MS;
        n = pselect(handle+1,&readfds,&writefds,&exceptionfds,&selectTimeout,signalMask);
      #else /* not HAVE_PSELECT */
        UNUSED_VARIABLE(signalMask);

        selectTimeout.tv_sec  = (long)(timeout/MS_PER_SECOND);
        selectTimeout.tv_usec = (long)(timeout%MS_PER_SECOND)*US_PER_MS;
        n = select(handle+1,&readfds,&writefds,&exceptionfds,&selectTimeout);
      #endif /* HAVE_PSELECT */
      if (n > 0)
      {
        events = 0;
        if (FD_ISSET(handle,&readfds     )) events |= HANDLE_EVENT_INPUT;
        if (FD_ISSET(handle,&writefds    )) events |= HANDLE_EVENT_OUTPUT;
        if (FD_ISSET(handle,&exceptionfds)) events |= HANDLE_EVENT_ERROR;
      }
      else
      {
        events = 0;
      }
    #endif /* HAVE_WSAPOLL */
  #endif /* PLATFORM_... */

  return events;
}

void Misc_initWait(WaitHandle *waitHandle, uint maxHandleCount)
{
  assert(waitHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    waitHandle->pollfds = (struct pollfd*)malloc(maxHandleCount*sizeof(struct pollfd));
    if (waitHandle->pollfds == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
  #elif defined(PLATFORM_WINDOWS)
    #ifdef HAVE_WSAPOLL
      waitHandle->pollfds = (struct pollfd*)malloc(maxHandleCount*sizeof(WSAPOLLFD));
      if (waitHandle->pollfds == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
    #else /* not HAVE_WSAPOLL */
    #endif /* HAVE_WSAPOLL */
  #endif /* PLATFORM_... */
  waitHandle->handleCount    = 0;
  waitHandle->maxHandleCount = maxHandleCount;
}

void Misc_doneWait(WaitHandle *waitHandle)
{
  assert(waitHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    assert(waitHandle->pollfds != NULL);

    free(waitHandle->pollfds);
  #elif defined(PLATFORM_WINDOWS)
    #ifdef HAVE_WSAPOLL
      assert(waitHandle->pollfds != NULL);

      free(waitHandle->pollfds);
    #else /* not HAVE_WSAPOLL */
    #endif /* HAVE_WSAPOLL */
  #endif /* PLATFORM_... */
}

void Misc_waitReset(WaitHandle *waitHandle)
{
  assert(waitHandle != NULL);

  waitHandle->handleCount = 0;
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    #ifdef HAVE_WSAPOLL
    #else /* not HAVE_WSAPOLL */
      FD_ZERO(&waitHandle->readfds);
      FD_ZERO(&waitHandle->writefds);
      FD_ZERO(&waitHandle->exceptionfds);
    #endif /* HAVE_WSAPOLL */
  #endif /* PLATFORM_... */
}

void Misc_waitAdd(WaitHandle *waitHandle, int handle, uint events)
{
  assert(waitHandle != NULL);
  assert(handle >= 0);

  #if   defined(PLATFORM_LINUX)
    assert(waitHandle->pollfds != NULL);

    if (waitHandle->handleCount >= waitHandle->maxHandleCount)
    {
      waitHandle->maxHandleCount += 64;
      waitHandle->pollfds = (struct pollfd*)realloc(waitHandle->pollfds,waitHandle->maxHandleCount*sizeof(struct pollfd));
      if (waitHandle->pollfds == NULL) HALT_INSUFFICIENT_MEMORY();
    }
    waitHandle->pollfds[waitHandle->handleCount].fd      = handle;
    waitHandle->pollfds[waitHandle->handleCount].events  = events;
    waitHandle->pollfds[waitHandle->handleCount].revents = 0;
  #elif defined(PLATFORM_WINDOWS)
    #ifdef HAVE_WSAPOLL
      assert(waitHandle->pollfds != NULL);

      if (waitHandle->handleCount >= waitHandle->maxHandleCount)
      {
        waitHandle->maxHandleCount += 64;
        waitHandle->pollfds = (struct pollfd*)realloc(waitHandle->pollfds,waitHandle->maxHandleCount*sizeof(WSAPOLLFD));
        if (waitHandle->pollfds == NULL) HALT_INSUFFICIENT_MEMORY();
      }
      waitHandle->pollfds[waitHandle->handleCount].fd      = handle;
      waitHandle->pollfds[waitHandle->handleCount].events  = (events & (HANDLE_EVENT_INPUT|HANDLE_EVENT_OUTPUT));
      waitHandle->pollfds[waitHandle->handleCount].revents = 0;
    #else /* not HAVE_WSAPOLL */
      assert(handle < FD_SETSIZE);

      if ((events & HANDLE_EVENT_INPUT ) != 0) FD_SET(handle,&waitHandle->readfds);
      if ((events & HANDLE_EVENT_OUTPUT) != 0) FD_SET(handle,&waitHandle->writefds);
      if ((events & HANDLE_EVENT_ERROR ) != 0) FD_SET(handle,&waitHandle->exceptionfds);
      waitHandle->handleCount = MAX((uint)handle,waitHandle->handleCount);
    #endif /* HAVE_WSAPOLL */
  #endif /* PLATFORM_... */
  waitHandle->handleCount++;
}

int Misc_waitHandles(WaitHandle *waitHandle,
                     SignalMask *signalMask,
                     long       timeout
                    )
{
  #if  defined(PLATFORM_LINUX)
    struct timespec pollTimeout;
  #elif defined(PLATFORM_WINDOWS)
    #ifdef HAVE_WSAPOLL
    #else /* not HAVE_WSAPOLL */
      #ifdef HAVE_PSELECT
        struct timespec selectTimeout;
      #else /* not HAVE_PSELECT */
        struct timeval selectTimeout;
      #endif /* HAVE_PSELECT */
    #endif /* HAVE_WSAPOLL */
  #endif /* PLATFORM_... */

  assert(waitHandle != NULL);

  #if   defined(PLATFORM_LINUX)
    assert(waitHandle->pollfds != NULL);

    pollTimeout.tv_sec  = (long)(timeout /MS_PER_SECOND);
    pollTimeout.tv_nsec = (long)((timeout%MS_PER_SECOND)*NS_PER_MS);
    return ppoll(waitHandle->pollfds,waitHandle->handleCount,&pollTimeout,signalMask);
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(signalMask);
    #ifdef HAVE_WSAPOLL
      return WSAPoll(waitHandle->pollfds,waitHandle->handleCount,timeout);
    #else /* not HAVE_WSAPOLL */
      #ifdef HAVE_PSELECT
        selectTimeout.tv_sec  = (long)(timeout/MS_PER_SECOND);
        selectTimeout.tv_nsec = (long)(timeout%MS_PER_SECOND)*NS_PER_MS;
        return pselect(waitHandle->handleCount+1,&waitHandle->readfds,&waitHandle->writefds,&waitHandle->exceptionfds,&selectTimeout,signalMask);
      #else /* not HAVE_PSELECT */
        UNUSED_VARIABLE(signalMask);

        selectTimeout.tv_sec  = (long)(timeout/MS_PER_SECOND);
        selectTimeout.tv_usec = (long)(timeout%MS_PER_SECOND)*US_PER_MS;
        return select(waitHandle->handleCount+1,&waitHandle->readfds,&waitHandle->writefds,&waitHandle->exceptionfds,&selectTimeout);
      #endif /* HAVE_PSELECT */
    #endif /* HAVE_WSAPOLL */
  #endif /* PLATFORM_... */
}

bool Misc_findCommandInPath(String     command,
                            const char *name
                           )
{
  bool            foundFlag;
  const char      *path;
  StringTokenizer stringTokenizer;
  ConstString     token;

  assert(command != NULL);
  assert(name != NULL);

  foundFlag = FALSE;

  path = getenv("PATH");
  if (path != NULL)
  {
    String_initTokenizerCString(&stringTokenizer,path,":","",FALSE);
    while (String_getNextToken(&stringTokenizer,&token,NULL) && !foundFlag)
    {
      File_setFileName(command,token);
      File_appendFileNameCString(command,name);
      foundFlag = File_exists(command);
    }
    String_doneTokenizer(&stringTokenizer);
  }

  return foundFlag;
}

Errors Misc_executeCommand(const char        *commandTemplate,
                           const TextMacro   macros[],
                           uint              macroCount,
                           ExecuteIOFunction stdoutExecuteIOFunction,
                           void              *stdoutExecuteIOUserData,
                           ExecuteIOFunction stderrExecuteIOFunction,
                           void              *stderrExecuteIOUserData
                          )
{
  Errors          error;
  String          commandLine;
  StringTokenizer stringTokenizer;
  ConstString     token;
  String          command;
  String          name;
  StringList      argumentList;
  char const      **arguments;
  StringNode      *stringNode;
  uint            n,z;

  error = ERROR_NONE;
  if (!stringIsEmpty(commandTemplate))
  {
    commandLine = String_new();
    name        = String_new();
    command     = File_newFileName();
    StringList_init(&argumentList);

    // expand command line
    Misc_expandMacros(commandLine,commandTemplate,EXPAND_MACRO_MODE_STRING,macros,macroCount,TRUE);
//fprintf(stderr,"%s, %d: execute command: %s\n",__FILE__,__LINE__,String_cString(commandLine));

    // parse command line
    String_initTokenizer(&stringTokenizer,commandLine,STRING_BEGIN,STRING_WHITE_SPACES,STRING_QUOTES,FALSE);
    if (!String_getNextToken(&stringTokenizer,&token,NULL))
    {
      error = ERRORX_(PARSE_COMMAND,0,"%s",String_cString(commandLine));
      String_doneTokenizer(&stringTokenizer);
      StringList_done(&argumentList);
      String_delete(command);
      String_delete(name);
      String_delete(commandLine);
      return error;
    }
    File_setFileName(name,token);

    // parse arguments
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      StringList_append(&argumentList,token);
    }
    String_doneTokenizer(&stringTokenizer);
#if 0
fprintf(stderr,"%s,%d: command %s\n",__FILE__,__LINE__,String_cString(command));
stringNode = argumentList.head;
while (stringNode != NULL)
{
fprintf(stderr,"%s,%d: argument %s\n",__FILE__,__LINE__,String_cString(stringNode->string));
stringNode = stringNode->next;
}
#endif /* 0 */

    // find command in PATH if possible
    if (!Misc_findCommandInPath(command,String_cString(name)))
    {
      File_setFileName(command,name);
    }

    // get arguments
    n = 1+StringList_count(&argumentList)+1;
    arguments = (char const**)malloc(n*sizeof(char*));
    if (arguments == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    z = 0;
    arguments[z] = String_cString(command); z++;
    stringNode = argumentList.head;
    while (stringNode != NULL)
    {
      assert(z < n);
      arguments[z] = String_cString(stringNode->string); z++;
      stringNode = stringNode->next;
    }
    assert(z < n);
    arguments[z] = NULL; z++;

    // execute command
    error = execute(String_cString(command),
                    arguments,
                    NULL,  // errorText
                    CALLBACK_(stdoutExecuteIOFunction,stdoutExecuteIOUserData),
                    0,  // stdoutStripCount
                    CALLBACK_(stderrExecuteIOFunction,stderrExecuteIOUserData),
                    0  // stderrStripCount
                   );

    // free resources
    free(arguments);
    StringList_done(&argumentList);
    String_delete(command);
    String_delete(name);
    String_delete(commandLine);
  }

  return error;
}

Errors Misc_executeScript(const char        *script,
                          ExecuteIOFunction stdoutExecuteIOFunction,
                          void              *stdoutExecuteIOUserData,
                          ExecuteIOFunction stderrExecuteIOFunction,
                          void              *stderrExecuteIOUserData
                         )
{
  const char      *shellCommand;
  Errors          error;
  String          command;
  StringTokenizer stringTokenizer;
  ConstString     token;
  String          tmpFileName;
  const char      *path;
  String          fileName;
  bool            foundFlag;
  FileHandle      fileHandle;
  char const      *arguments[3];

  error = ERROR_NONE;
  if (!stringIsEmpty(script))
  {
    command     = String_new();
    tmpFileName = String_new();

    // get shell-command
    shellCommand = getenv("SHELL");
    if (shellCommand == NULL) shellCommand = "/bin/sh";
    String_setCString(command,shellCommand);

    // find command in PATH
    path = getenv("PATH");
    if (path != NULL)
    {
      fileName  = File_newFileName();
      foundFlag = FALSE;
      String_initTokenizerCString(&stringTokenizer,path,":","",FALSE);
      while (String_getNextToken(&stringTokenizer,&token,NULL) && !foundFlag)
      {
        File_setFileName(fileName,token);
        File_appendFileName(fileName,command);
        if (File_exists(fileName))
        {
          File_setFileName(command,fileName);
          foundFlag = TRUE;
        }
      }
      String_doneTokenizer(&stringTokenizer);
      File_deleteFileName(fileName);
    }

    // create temporary script file
    File_getTmpFileName(tmpFileName,NULL,NULL);
    error = File_open(&fileHandle,tmpFileName,FILE_OPEN_WRITE);
    if (error != ERROR_NONE)
    {
      (void)File_delete(tmpFileName,FALSE);
      String_delete(tmpFileName);
      String_delete(command);
      return ERROR_OPEN_FILE;
    }
    error = File_write(&fileHandle,script,strlen(script));
    if (error != ERROR_NONE)
    {
      (void)File_delete(tmpFileName,FALSE);
      String_delete(tmpFileName);
      String_delete(command);
      return ERROR_OPEN_FILE;
    }
    File_close(&fileHandle);

    // get arguments
    arguments[0] = String_cString(command);
    arguments[1] = String_cString(tmpFileName);
    arguments[2] = NULL;

    // execute command
    error = execute(String_cString(command),
                    arguments,
                    script,
                    CALLBACK_(stdoutExecuteIOFunction,stdoutExecuteIOUserData),
                    0,  // stdoutStripCount
                    CALLBACK_(stderrExecuteIOFunction,stderrExecuteIOUserData),
                    String_length(tmpFileName)+1+1
                   );

    // free resources
    (void)File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(command);
  }

  return error;
}

/*---------------------------------------------------------------------*/

bool Misc_isTerminal(int handle)
{
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  #if   defined(PLATFORM_LINUX)
    return isatty(handle) == 1;
  #elif defined(PLATFORM_WINDOWS)
    return TRUE;
  #endif /* PLATFORM_... */
}

void Misc_waitEnter(void)
{
  #if   defined(PLATFORM_LINUX)
    struct termios oldTermioSettings;
    struct termios termioSettings;
    char           s[2];
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  #if   defined(PLATFORM_LINUX)
    if (isatty(File_getDescriptor(stdin)) != 0)
    {
      // save current console settings
      tcgetattr(File_getDescriptor(stdin),&oldTermioSettings);

      // disable echo
      memcpy(&termioSettings,&oldTermioSettings,sizeof(struct termios));
      termioSettings.c_lflag &= ~ECHO;
      tcsetattr(File_getDescriptor(stdin),TCSANOW,&termioSettings);

      // read line (and ignore)
      if (fgets(s,2,stdin) != NULL) { /* ignored */ };

      // restore console settings
      tcsetattr(File_getDescriptor(stdin),TCSANOW,&oldTermioSettings);
    }
  #elif defined(PLATFORM_WINDOWS)
// NYI ???
    #ifndef WERROR
      #warning no console input on windows
    #endif /* NDEBUG */
  #endif /* PLATFORM_... */
}

bool Misc_getYesNo(const char *message)
{
  #if   defined(PLATFORM_LINUX)
    struct termios oldTermioSettings;
    struct termios termioSettings;
    int            keyCode;
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  #if   defined(PLATFORM_LINUX)
    if (isatty(File_getDescriptor(stdin)) != 0)
    {
      fputs(message,stdout); fputs(" [y/N]",stdout); fflush(stdout);

      // save current console settings
      tcgetattr(File_getDescriptor(stdin),&oldTermioSettings);

      // set raw mode
      memCopy(&termioSettings,sizeof(termioSettings),&oldTermioSettings,sizeof(oldTermioSettings));
      cfmakeraw(&termioSettings);
      tcsetattr(File_getDescriptor(stdin),TCSANOW,&termioSettings);

      // read yes/no
      do
      {
        keyCode = toupper(fgetc(stdin));
      }
      while ((keyCode != (int)'Y') && (keyCode != (int)'N') && (keyCode != 13));

      // restore console settings
      tcsetattr(File_getDescriptor(stdin),TCSANOW,&oldTermioSettings);

      fputc('\n',stdout);

      return (keyCode == (int)'Y');
    }
    else
    {
      return FALSE;
    }
  #elif defined(PLATFORM_WINDOWS)
// NYI ???
#warning no console input on windows
    UNUSED_VARIABLE(message);

    return FALSE;
  #endif /* PLATFORM_... */
}

void Misc_getConsoleSize(uint *rows, uint *columns)
{
  #if   defined(PLATFORM_LINUX)
    struct winsize size;
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  if (rows    != NULL) (*rows   ) = 25;
  if (columns != NULL) (*columns) = 80;

  #if   defined(PLATFORM_LINUX)
    if (ioctl(STDOUT_FILENO,TIOCGWINSZ,&size) == 0)
    {
      if (rows    != NULL) (*rows   ) = size.ws_row;
      if (columns != NULL) (*columns) = size.ws_col;
    }
  #elif defined(PLATFORM_WINDOWS)
    // TODO: NYI
  #endif /* PLATFORM_... */
}

/*---------------------------------------------------------------------*/

void Misc_performanceFilterInit(PerformanceFilter *performanceFilter,
                                uint              maxSeconds
                               )
{
  uint z;

  assert(performanceFilter != NULL);
  assert(maxSeconds > 0);

  performanceFilter->performanceValues = (PerformanceValue*)malloc(maxSeconds*sizeof(PerformanceValue));
  if (performanceFilter->performanceValues == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  performanceFilter->performanceValues[0].timeStamp = Misc_getTimestamp()/1000L;
  performanceFilter->performanceValues[0].value     = 0.0;
  for (z = 1; z < maxSeconds; z++)
  {
    performanceFilter->performanceValues[z].timeStamp = 0;
    performanceFilter->performanceValues[z].value     = 0.0;
  }
  performanceFilter->maxSeconds = maxSeconds;
  performanceFilter->seconds    = 0;
  performanceFilter->index      = 0;
  performanceFilter->average    = 0;
  performanceFilter->n          = 0;
}

void Misc_performanceFilterDone(PerformanceFilter *performanceFilter)
{
  assert(performanceFilter != NULL);
  assert(performanceFilter->performanceValues != NULL);

  free(performanceFilter->performanceValues);
}

void Misc_performanceFilterClear(PerformanceFilter *performanceFilter)
{
  uint z;

  assert(performanceFilter != NULL);
  assert(performanceFilter->performanceValues != NULL);

  performanceFilter->performanceValues[0].timeStamp = Misc_getTimestamp()/1000L;
  performanceFilter->performanceValues[0].value     = 0.0;
  for (z = 1; z < performanceFilter->maxSeconds; z++)
  {
    performanceFilter->performanceValues[z].timeStamp = 0;
    performanceFilter->performanceValues[z].value     = 0.0;
  }
  performanceFilter->seconds = 0;
  performanceFilter->index   = 0;
  performanceFilter->average = 0;
  performanceFilter->n       = 0;
}

void Misc_performanceFilterAdd(PerformanceFilter *performanceFilter,
                               double            value
                              )
{
  uint64 timeStamp;
  double valueDelta;
  uint64 timeStampDelta;
  double average;

  assert(performanceFilter != NULL);
  assert(performanceFilter->performanceValues != NULL);
  assert(performanceFilter->index < performanceFilter->maxSeconds);

  timeStamp = Misc_getTimestamp()/1000L;

  if (timeStamp > (performanceFilter->performanceValues[performanceFilter->index].timeStamp+1000))
  {
    // calculate new average value
    if (performanceFilter->seconds > 0)
    {
      valueDelta     = value-performanceFilter->performanceValues[performanceFilter->index].value;
      timeStampDelta = timeStamp-performanceFilter->performanceValues[performanceFilter->index].timeStamp;
      average = (valueDelta*1000)/(double)timeStampDelta;
      if (performanceFilter->n > 0)
      {
        performanceFilter->average = average/(double)performanceFilter->n+((double)(performanceFilter->n-1)*performanceFilter->average)/(double)performanceFilter->n;
      }
      else
      {
        performanceFilter->average = average;
      }
      performanceFilter->n++;
    }

    // move to next index in ring buffer
    performanceFilter->index = (performanceFilter->index+1)%performanceFilter->maxSeconds;
    assert(performanceFilter->index < performanceFilter->maxSeconds);

    // store value
    performanceFilter->performanceValues[performanceFilter->index].timeStamp = timeStamp;
    performanceFilter->performanceValues[performanceFilter->index].value     = value;
    if (performanceFilter->seconds < performanceFilter->maxSeconds) performanceFilter->seconds++;
  }
}

double Misc_performanceFilterGetValue(const PerformanceFilter *performanceFilter,
                                      uint                    seconds
                                     )
{
  uint   i0,i1;
  double valueDelta;
  uint64 timeStampDelta;

  assert(performanceFilter != NULL);
  assert(performanceFilter->performanceValues != NULL);
  assert(seconds <= performanceFilter->maxSeconds);

  seconds = MIN(seconds,performanceFilter->seconds);
  i0 = (performanceFilter->index+(performanceFilter->maxSeconds-seconds))%performanceFilter->maxSeconds;
  assert(i0 < performanceFilter->maxSeconds);
  i1 = performanceFilter->index;
  assert(i1 < performanceFilter->maxSeconds);

  valueDelta     = performanceFilter->performanceValues[i1].value-performanceFilter->performanceValues[i0].value;
  timeStampDelta = performanceFilter->performanceValues[i1].timeStamp-performanceFilter->performanceValues[i0].timeStamp;
  return (timeStampDelta > 0) ? (valueDelta*1000)/(double)timeStampDelta : 0.0;
}

double Misc_performanceFilterGetAverageValue(PerformanceFilter *performanceFilter)
{
  assert(performanceFilter != NULL);

  return performanceFilter->average;
}

/*---------------------------------------------------------------------*/

String Misc_base64Encode(String string, const void *data, uint dataLength)
{
  void *buffer;
  uint bufferSize;

  if ((data != NULL) && (dataLength > 0))
  {
    bufferSize = Misc_base64EncodeLength(data,dataLength);
    buffer = malloc(bufferSize);
    if (buffer != NULL)
    {
      base64Encode(buffer,bufferSize,NULL,data,dataLength);
      String_appendBuffer(string,buffer,bufferSize);
      free(buffer);
    }
  }

  return string;
}

void *Misc_base64EncodeBuffer(void *buffer, uint bufferLength, const void *data, uint dataLength)
{
  base64Encode(buffer,bufferLength,NULL,data,dataLength);

  return buffer;
}

bool Misc_base64Decode(void *data, uint maxDataLength, uint *dataLength, ConstString string, ulong index)
{
  assert(data != NULL);
  assert(string != NULL);

  if (String_length(string) >= index)
  {
    return base64Decode(data,maxDataLength,dataLength,String_cString(string)+index,String_length(string)-index);
  }
  else
  {
    return FALSE;
  }
}

bool Misc_base64DecodeCString(void *data, uint maxDataLength, uint *dataLength, const char *s)
{
  assert(data != NULL);
  assert(s != NULL);

  return base64Decode(data,maxDataLength,dataLength,s,strlen(s));
}

bool Misc_base64DecodeBuffer(void *data, uint maxDataLength, uint *dataLength, const void *buffer, uint bufferLength)
{
  return base64Decode(data,maxDataLength,dataLength,buffer,bufferLength);
}

uint Misc_base64DecodeLength(ConstString string, ulong index)
{
  size_t n;
  uint   length;

  assert(string != NULL);

  if (String_length(string) > index)
  {
    n = String_length(string)-index;
  }
  else
  {
    n = 0;
  }
  length = (n/4)*3;
  if ((n >= 1) && (String_index(string,index+n-1) == '=')) length--;
  if ((n >= 2) && (String_index(string,index+n-2) == '=')) length--;

  return length;
}

uint Misc_base64DecodeLengthCString(const char *s)
{
  size_t n;
  uint   length;

  assert(s != NULL);

  n = strlen(s);
  length = (n/4)*3;
  if ((n >= 1) && (s[n-1] == '=')) length--;
  if ((n >= 2) && (s[n-2] == '=')) length--;

  return length;
}

uint Misc_base64DecodeLengthBuffer(const void *buffer, uint bufferLength)
{
  uint length;

  assert(buffer != NULL);

  length = (bufferLength/4)*3;
  if ((bufferLength >= 1) && (((char*)buffer)[bufferLength-1] == '=')) length--;
  if ((bufferLength >= 2) && (((char*)buffer)[bufferLength-2] == '=')) length--;

  return length;
}

String Misc_hexEncode(String string, const void *data, uint dataLength)
{
  uint i;

  assert(string != NULL);

  for (i = 0; i < dataLength; i++)
  {
    String_appendFormat(string,"%02x",((byte*)data)[i]);
  }

  return string;
}

bool Misc_hexDecode(void *data, uint *dataLength, ConstString string, ulong index, uint maxDataLength)
{
  assert(data != NULL);
  assert(string != NULL);

  if (String_length(string) >= index)
  {
    return hexDecode(data,dataLength,String_cString(string)+index,String_length(string)-index,maxDataLength);
  }
  else
  {
    return FALSE;
  }
}

bool Misc_hexDecodeCString(void *data, uint *dataLength, const char *s, uint maxDataLength)
{
  assert(data != NULL);
  assert(s != NULL);

  return hexDecode(data,dataLength,s,strlen(s),maxDataLength);
}

uint Misc_hexDecodeLength(ConstString string, ulong index)
{
  assert(string != NULL);

  if (String_length(string) > index)
  {
    return (String_length(string)-index)/2;
  }
  else
  {
    return 0;
  }
}

uint Misc_hexDecodeLengthCString(const char *s)
{
  assert(s != NULL);

  return strlen(s)/2;
}

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
bool Misc_getRegistryString(String string, HKEY parentKey, const char *subKey, const char *name)
{
  #define BUFFER_SIZE 256

  HKEY  hKey;
  DWORD dwType;
  TCHAR buffer[BUFFER_SIZE];
  DWORD bufferLength;
  bool  result;

  if (RegOpenKeyEx(parentKey,subKey,0,KEY_READ|KEY_WOW64_64KEY,&hKey) == ERROR_SUCCESS)
  {
    dwType       = REG_SZ;
    bufferLength = BUFFER_SIZE;
    if (RegQueryValueEx(hKey,name,NULL, &dwType, (BYTE*)buffer, &bufferLength) == ERROR_SUCCESS)
                {
      String_setBuffer(string,buffer,bufferLength);
      result = TRUE;
    }
    else
    {
      result = FALSE;
    }
    RegCloseKey(hKey);
  }

  return result;

  #undef BUFFER_SIZE
}
#endif /* PLATFORM_... */

#ifdef __cplusplus
  }
#endif

/* end of file */
