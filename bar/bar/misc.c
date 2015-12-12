/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: miscellaneous functions
* Systems: all
*
\***********************************************************************/

#define __MISC_IMPLEMENATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
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
#include <errno.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
  #include <windows.h>
#endif /* PLATFORM_... */

#include "global.h"
#include "errors.h"
#include "strings.h"
#include "stringlists.h"

#include "bar.h"
#include "files.h"

#include "misc.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

#if   defined(PLATFORM_LINUX)
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

/***********************************************************************\
* Name   : readProcessIO
* Purpose: read process i/o, EOL at LF/CR/BS, skip empty lines
* Input  : fd   - file handle
*          line - line
* Output : line - read line
* Return : TRUE if line read, FALSE otherwise
* Notes  : -
\***********************************************************************/

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

LOCAL Errors execute(const char        *command,
                     const char const  **arguments,
                     ExecuteIOFunction stdoutExecuteIOFunction,
                     void              *stdoutExecuteIOUserData,
                     ExecuteIOFunction stderrExecuteIOFunction,
                     void              *stderrExecuteIOUserData
                    )
{
  Errors     error;
  int        pipeStdin[2],pipeStdout[2],pipeStderr[2];
  pid_t      pid;
  int        status;
  bool       sleepFlag;
  String     stdoutLine,stderrLine;
  int        exitcode;
  int        terminateSignal;

  error = ERROR_NONE;

#if 0
{
fprintf(stderr,"%s,%d: command %s\n",__FILE__,__LINE__,command);

const char **t = arguments;
while (*t != NULL)
{
fprintf(stderr,"%s,%d: argument %s\n",__FILE__,__LINE__,*t);
t++;
}
}
#endif /* 0 */

  #if defined(HAVE_PIPE) && defined(HAVE_FORK) && defined(HAVE_WAITPID)
#if 1
    // create i/o pipes
    if (pipe(pipeStdin) != 0)
    {
      error = ERRORX_(IO_REDIRECT_FAIL,errno,command);
      return error;
    }
    if (pipe(pipeStdout) != 0)
    {
      error = ERRORX_(IO_REDIRECT_FAIL,errno,command);
      close(pipeStdin[0]);
      close(pipeStdin[1]);
      return error;
    }
    if (pipe(pipeStderr) != 0)
    {
      error = ERRORX_(IO_REDIRECT_FAIL,errno,command);
      close(pipeStdout[0]);
      close(pipeStdout[1]);
      close(pipeStdin[0]);
      close(pipeStdin[1]);
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
      exit(EXITCODE_FAIL);
    }
    else if (pid < 0)
    {
      error = ERRORX_(EXEC_FAIL,errno,command);

      close(pipeStderr[0]);
      close(pipeStderr[1]);
      close(pipeStdout[0]);
      close(pipeStdout[1]);
      close(pipeStdin[0]);
      close(pipeStdin[1]);
      return error;
    }

    // close unused pipe handles (the pipe is duplicated by fork(), thus there are two open ends of the pipe)
    close(pipeStderr[1]);
    close(pipeStdout[1]);
    close(pipeStdin[0]);
#else /* 0 */
error = ERROR_NONE;
#endif /* 0 */

    // read stdout/stderr and wait until process terminate
    stdoutLine = String_new();
    stderrLine = String_new();
    status = 0xFFFFFFFF;
    while (   (waitpid(pid,&status,WNOHANG) == 0)
           || (   !WIFEXITED(status)
               && !WIFSIGNALED(status)
              )
          )
    {
      sleepFlag = TRUE;

      if (readProcessIO(pipeStdout[0],stdoutLine))
      {
        if (stdoutExecuteIOFunction != NULL) stdoutExecuteIOFunction(stdoutExecuteIOUserData,stdoutLine);
        String_clear(stdoutLine);
        sleepFlag = FALSE;
      }
      if (readProcessIO(pipeStderr[0],stderrLine))
      {
        if (stderrExecuteIOFunction != NULL) stderrExecuteIOFunction(stderrExecuteIOUserData,stderrLine);
        String_clear(stderrLine);
        sleepFlag = FALSE;
      }

      if (sleepFlag)
      {
        Misc_udelay(1000LL*1000LL);
      }
    }
    while (readProcessIO(pipeStdout[0],stdoutLine))
    {
      if (stdoutExecuteIOFunction != NULL) stdoutExecuteIOFunction(stdoutExecuteIOUserData,stdoutLine);
      String_clear(stdoutLine);
    }
    while (readProcessIO(pipeStderr[0],stderrLine))
    {
      if (stderrExecuteIOFunction != NULL) stderrExecuteIOFunction(stderrExecuteIOUserData,stderrLine);
      String_clear(stderrLine);
    }
    String_delete(stderrLine);
    String_delete(stdoutLine);

    // close i/o
    close(pipeStderr[0]);
    close(pipeStdout[0]);
    close(pipeStdin[1]);

    // check exit code
    exitcode = -1;
    if      (WIFEXITED(status))
    {
      exitcode = WEXITSTATUS(status);
      if (exitcode == 0)
      {
        printInfo(3,"ok\n");
      }
      else
      {
        printInfo(3,"FAIL (exitcode %d)\n",exitcode);
        error = ERRORX_(EXEC_FAIL,exitcode,command);
        return error;
      }
    }
    else if (WIFSIGNALED(status))
    {
      terminateSignal = WTERMSIG(status);
      error = ERRORX_(EXEC_FAIL,terminateSignal,command);
      printInfo(3,"FAIL (signal %d)\n",terminateSignal);
      return error;
    }
    else
    {
      printInfo(3,"FAIL (unknown exit)\n");
      return ERROR_UNKNOWN;
    }
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

  return error;
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
    return (uint64)tv.tv_usec+(uint64)tv.tv_sec*1000000LL;
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
    date = (uint64)(tv.tv_sec-tv.tv_sec%(24L*60L*60L));
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
    time = (uint64)(tv.tv_sec%(24L*60L*60L));
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

  #ifdef HAVE_GETDATE_R
    struct tm tmBuffer;
  #endif /* HAVE_GETDATE_R */
  struct tm  *tm;
  uint       z;
  const char *s;
  uint64     dateTime;

  assert(string != NULL);

  #ifdef HAVE_GETDATE_R
    tm = (getdate_r(string,&tmBuffer) == 0) ? &tmBuffer : NULL;
  #else /* not HAVE_GETDATE_R */
    tm = getdate(string);
  #endif /* HAVE_GETDATE_R */

  if (tm == NULL)
  {
    z = 0;
    while ((z < SIZE_OF_ARRAY(DATE_TIME_FORMATS)) && (tm == NULL))
    {
      s = (const char*)strptime(string,DATE_TIME_FORMATS[z],&tmBuffer);
      if ((s != NULL) && ((*s) == '\0'))
      {
        tm = &tmBuffer;
      }
      z++;
    }
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

  // set string
  String_setBuffer(string,buffer,length);

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
  #ifdef HAVE_NANOSLEEP
    struct timespec ts;
  #endif /* HAVE_NANOSLEEP */

  #if   defined(HAVE_NANOSLEEP)
    ts.tv_sec  = (ulong)(time/1000000LL);
    ts.tv_nsec = (ulong)((time%1000000LL)*1000);
    while (   (nanosleep(&ts,&ts) == -1)
           && (errno == EINTR)
          )
    {
      // nothing to do
    }
  #elif defined(PLATFORM_WINDOWS)
    Sleep(time/1000LL);
  #else
    #error nanosleep() not available nor Windows system!
  #endif
}

/*---------------------------------------------------------------------*/

String Misc_getUUID(String string)
{
  char buffer[64];

  assert(string != NULL);

  return String_setCString(string,Misc_getUUIDCString(buffer,sizeof(buffer)));
}

const char *Misc_getUUIDCString(char *buffer, uint bufferSize)
{
  #if HAVE_UUID_GENERATE
    uuid_t uuid;
    char   s[36+1];
  #else /* not HAVE_UUID_GENERATE */
    FILE *file;
    char *s;
  #endif /* HAVE_UUID_GENERATE */

  assert(buffer != NULL);
  assert(bufferSize > 0);

  buffer[0] = '\0';

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

  return buffer;
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

  String expanded;
  bool   macroFlag;
  ulong  i;
  uint   j;
  char   name[128];
  char   format[128];

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
        if ((templateString[i+1] == '%'))
        {
          if (expandMacroCharacter)
          {
            String_appendChar(expanded,'%');
          }
          else
          {
            String_appendCString(expanded,"%%");
          }
          i+=2;
        }
        else
        {
          macroFlag = TRUE;
          i++;
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
      if (strlen(name) > 0)
      {
        // find macro
        j = 0;
        while (   (j < macroCount)
               && (strcmp(name,macros[j].name) != 0)
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
              if (strlen(format) == 0)
              {
                switch (macros[j].type)
                {
                  case TEXT_MACRO_TYPE_INTEGER:
                    strcpy(format,"%d");
                    break;
                  case TEXT_MACRO_TYPE_INTEGER64:
                    strcpy(format,"%lld");
                    break;
                  case TEXT_MACRO_TYPE_DOUBLE:
                    strcpy(format,"%lf");
                    break;
                  case TEXT_MACRO_TYPE_CSTRING:
                    strcpy(format,"%s");
                    break;
                  case TEXT_MACRO_TYPE_STRING:
                    strcpy(format,"%S");
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
                  String_format(expanded,format,macros[j].value.i);
                  break;
                case TEXT_MACRO_TYPE_INTEGER64:
                  String_format(expanded,format,macros[j].value.l);
                  break;
                case TEXT_MACRO_TYPE_DOUBLE:
                  String_format(expanded,format,macros[j].value.d);
                  break;
                case TEXT_MACRO_TYPE_CSTRING:
                  String_format(expanded,format,macros[j].value.s);
                  break;
                case TEXT_MACRO_TYPE_STRING:
                  String_format(expanded,format,macros[j].value.string);
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
        // empty macro: add empty string
        String_format(expanded,format,"");
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
  if (commandTemplate != NULL)
  {
    commandLine = String_new();
    name        = String_new();
    command     = File_newFileName();
    StringList_init(&argumentList);

    // expand command line
    Misc_expandMacros(commandLine,commandTemplate,EXPAND_MACRO_MODE_STRING,macros,macroCount,TRUE);
    printInfo(3,"Execute command '%s'...",String_cString(commandLine));

    // parse command line
    String_initTokenizer(&stringTokenizer,commandLine,STRING_BEGIN,STRING_WHITE_SPACES,STRING_QUOTES,FALSE);
    if (!String_getNextToken(&stringTokenizer,&token,NULL))
    {
      String_doneTokenizer(&stringTokenizer);
      StringList_done(&argumentList);
      String_delete(command);
      String_delete(name);
      String_delete(commandLine);
      return ERRORX_(PARSE_COMMAND,0,String_cString(commandLine));
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
                    CALLBACK(stdoutExecuteIOFunction,stdoutExecuteIOUserData),
                    CALLBACK(stderrExecuteIOFunction,stderrExecuteIOUserData)
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

Errors Misc_executeScript(const char        *scriptTemplate,
                          const TextMacro   macros[],
                          uint              macroCount,
                          ExecuteIOFunction stdoutExecuteIOFunction,
                          void              *stdoutExecuteIOUserData,
                          ExecuteIOFunction stderrExecuteIOFunction,
                          void              *stderrExecuteIOUserData
                         )
{
  Errors          error;
  String          script;
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
  if (scriptTemplate != NULL)
  {
    script      = String_new();
    command     = String_new();
    tmpFileName = String_new();

    // expand script
    Misc_expandMacros(script,scriptTemplate,EXPAND_MACRO_MODE_STRING,macros,macroCount,TRUE);
    printInfo(3,"Execute script...");

#warning todo
#if 0
    // parse script #!-line
    String_initTokenizer(&stringTokenizer,commandLine,STRING_BEGIN,STRING_WHITE_SPACES,STRING_QUOTES,FALSE);
    if (!String_getNextToken(&stringTokenizer,&token,NULL))
    {
      String_doneTokenizer(&stringTokenizer);
      String_delete(command);
      String_delete(script);
      return ERRORX_PARSE_COMMAND;
    }
    File_setFileName(command,token);
#endif
String_setCString(command,"/bin/sh");

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
      String_delete(script);
      return ERROR_OPEN_FILE;
    }
    error = File_writeLine(&fileHandle,script);
    if (error != ERROR_NONE)
    {
      (void)File_delete(tmpFileName,FALSE);
      String_delete(tmpFileName);
      String_delete(command);
      String_delete(script);
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
                    CALLBACK(stdoutExecuteIOFunction,stdoutExecuteIOUserData),
                    CALLBACK(stderrExecuteIOFunction,stderrExecuteIOUserData)
                   );

    // free resources
    (void)File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(command);
    String_delete(script);
  }

  return error;
}

Errors Misc_executeScript2(const char        *script,
                          ExecuteIOFunction stdoutExecuteIOFunction,
                          void              *stdoutExecuteIOUserData,
                          ExecuteIOFunction stderrExecuteIOFunction,
                          void              *stderrExecuteIOUserData
                         )
{
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
  if (script != NULL)
  {
    command     = String_new();
    tmpFileName = String_new();

#warning todo
#if 0
    // parse script #!-line
    String_initTokenizer(&stringTokenizer,commandLine,STRING_BEGIN,STRING_WHITE_SPACES,STRING_QUOTES,FALSE);
    if (!String_getNextToken(&stringTokenizer,&token,NULL))
    {
      String_doneTokenizer(&stringTokenizer);
      String_delete(command);
      String_delete(script);
      return ERRORX_PARSE_COMMAND;
    }
    File_setFileName(command,token);
#endif
String_setCString(command,"/bin/sh");

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
                    CALLBACK(stdoutExecuteIOFunction,stdoutExecuteIOUserData),
                    CALLBACK(stderrExecuteIOFunction,stderrExecuteIOUserData)
                   );

    // free resources
    (void)File_delete(tmpFileName,FALSE);
    String_delete(tmpFileName);
    String_delete(command);
  }

  return error;
}

/*---------------------------------------------------------------------*/

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
#warning no console input on windows
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
      memcpy(&termioSettings,&oldTermioSettings,sizeof(struct termios));
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
  struct winsize size;

  if (rows    != NULL) (*rows   ) = 25;
  if (columns != NULL) (*columns) = 80;

  if (ioctl(STDOUT_FILENO,TIOCGWINSZ,&size) == 0)
  {
    if (rows    != NULL) (*rows   ) = size.ws_row;
    if (columns != NULL) (*columns) = size.ws_col;
  }
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

String Misc_base64Encode(String s, const byte *data, uint length)
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

  uint i;
  char b0,b1,b2;
  uint i0,i1,i2,i3;

  String_clear(s);

  i = 0;
  while (i < length)
  {
    b0 = ((i+0) < length)?data[i+0]:0;
    b1 = ((i+1) < length)?data[i+1]:0;
    b2 = ((i+2) < length)?data[i+2]:0;

    i0 = (uint)(b0 & 0xFC) >> 2;
    assert(i0 < 64);
    i1 = (uint)((b0 & 0x03) << 4) | (uint)((b1 & 0xF0) >> 4);
    assert(i1 < 64);
    i2 = (uint)((b1 & 0x0F) << 2) | (uint)((b2 & 0xC0) >> 6);
    assert(i2 < 64);
    i3 = (uint)(b2 & 0x3F);
    assert(i3 < 64);

    String_appendChar(s,BASE64_ENCODING_TABLE[i0]);
    String_appendChar(s,BASE64_ENCODING_TABLE[i1]);
    String_appendChar(s,BASE64_ENCODING_TABLE[i2]);
    String_appendChar(s,BASE64_ENCODING_TABLE[i3]);

    i += 3;
  }

  return s;
}

int Misc_base64Decode(byte *data, uint maxLength, const String s)
{
  #define VALID_BASE64_CHAR(ch) (   (((ch) >= 'A') && ((ch) <= 'Z')) \
                                 || (((ch) >= 'a') && ((ch) <= 'z')) \
                                 || (((ch) >= '0') && ((ch) <= '9')) \
                                 || ((ch) == '+') \
                                 || ((ch) == '/') \
                                 || ((ch) == '=') \
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

  uint length;
  uint i;
  char x0,x1,x2,x3;
  uint i0,i1,i2,i3;
  char b0,b1,b2;

  length = 0;
  i = 0;
  while (i < (uint)String_length(s))
  {
    x0 = String_index(s,i+0); if (!VALID_BASE64_CHAR(x0)) return -1;
    x1 = String_index(s,i+1); if (!VALID_BASE64_CHAR(x1)) return -1;
    x2 = String_index(s,i+2); if (!VALID_BASE64_CHAR(x2)) return -1;
    x3 = String_index(s,i+3); if (!VALID_BASE64_CHAR(x3)) return -1;

    i0 = ((i+0) < (uint)String_length(s))?BASE64_DECODING_TABLE[(byte)x0]:0;
    i1 = ((i+1) < (uint)String_length(s))?BASE64_DECODING_TABLE[(byte)x1]:0;
    i2 = ((i+2) < (uint)String_length(s))?BASE64_DECODING_TABLE[(byte)x2]:0;
    i3 = ((i+3) < (uint)String_length(s))?BASE64_DECODING_TABLE[(byte)x3]:0;

    b0 = (char)((i0 << 2) | ((i1 & 0x30) >> 4));
    b1 = (char)(((i1 & 0x0F) << 4) | ((i2 & 0x3C) >> 2));
    b2 = (char)(((i2 & 0x03) << 6) | i3);

    if (length < maxLength) { data[length] = b0; length++; }
    if (length < maxLength) { data[length] = b1; length++; }
    if (length < maxLength) { data[length] = b2; length++; }

    i += 4;
  }

  return length;

  #undef VALID_BASE64_CHAR
}

#ifdef __cplusplus
  }
#endif

/* end of file */
