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
#endif /* HAVE_TERMIOS */
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

/*---------------------------------------------------------------------*/

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
  struct timeval tv;

  gettimeofday(&tv,NULL);

  return (uint64)tv.tv_sec;
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
      s = strptime(string,DATE_TIME_FORMATS[z],&tmBuffer);
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
  #elif defined(WIN32)
    Sleep(time/1000LL);
  #else
    #error nanosleep() not available nor Win32 system!
  #endif
}

/*---------------------------------------------------------------------*/

String Misc_getUUID(String string)
{
  char buffer[1024];

  assert(string != NULL);

  return String_setCString(string,Misc_getUUIDCString(buffer,sizeof(buffer)));
}

const char *Misc_getUUIDCString(char *buffer, uint bufferSize)
{
  #if HAVE_UUID_GENERATE
    uuid_t uuid;
    char   *s;
  #else /* not HAVE_UUID_GENERATE */
    FILE *file;
    char *s;
  #endif /* HAVE_UUID_GENERATE */

  assert(buffer != NULL);
  assert(bufferSize > 0);

  buffer[0] = '\0';

  #if HAVE_UUID_GENERATE
    uuid_generate(uuid);

    s = (char*)malloc(36+1);
    if (s != NULL)
    {
      uuid_unparse_lower(uuid,s);
      strncpy(buffer,s,bufferSize-1);
      buffer[bufferSize-1] = '\0';
      free(s);
    }
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

String Misc_expandMacros(String          string,
                         const char      *macroTemplate,
                         const TextMacro macros[],
                         uint            macroCount
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

  bool  macroFlag;
  ulong i;
  uint  j;
  char  name[128];
  char  format[128];

  assert(macroTemplate != NULL);
  assert((macroCount == 0) || (macros != NULL));

  String_clear(string);
  i = 0;
  do
  {
    // add prefix string
    macroFlag = FALSE;
    while ((macroTemplate[i] != '\0') && !macroFlag)
    {
      if (macroTemplate[i] == '%')
      {
        if ((macroTemplate[i+1] == '%'))
        {
          String_appendChar(string,'%');
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
        String_appendChar(string,macroTemplate[i]);
        i++;
      }
    }

    if (macroFlag)
    {
      // skip spaces
      SKIP_SPACES(macroTemplate,i);

      // get macro name
      j = 0;
      if (   (macroTemplate[i] != '\0')
          && isalpha(macroTemplate[i])
         )
      {
        APPEND_CHAR(name,j,'%');
        do
        {
          APPEND_CHAR(name,j,macroTemplate[i]);
          i++;
        }
        while (   (macroTemplate[i] != '\0')
               && isalnum(macroTemplate[i])
              );
      }
      name[j] = '\0';

      // get format data (if any)
      j = 0;
      if (macroTemplate[i] == ':')
      {
        // skip ':'
        i++;

        // skip spaces
        SKIP_SPACES(macroTemplate,i);

        // get format string
        APPEND_CHAR(format,j,'%');
        while (   (macroTemplate[i] != '\0')
               && (   isdigit(macroTemplate[i])
                   || (macroTemplate[i] == '-')
                   || (macroTemplate[i] == '.')
                  )
              )
        {
          APPEND_CHAR(format,j,macroTemplate[i]);
          i++;
        }
        while (   (macroTemplate[i] != '\0')
               && (strchr("l",macroTemplate[i]) != NULL)
              )
        {
          APPEND_CHAR(format,j,macroTemplate[i]);
          i++;
        }
        if (   (macroTemplate[i] != '\0')
            && (strchr("duxfsS",macroTemplate[i]) != NULL)
           )
        {
          APPEND_CHAR(format,j,macroTemplate[i]);
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

          // expand macro
          switch (macros[j].type)
          {
            case TEXT_MACRO_TYPE_INTEGER:
              String_format(string,format,macros[j].value.i);
              break;
            case TEXT_MACRO_TYPE_INTEGER64:
              String_format(string,format,macros[j].value.l);
              break;
            case TEXT_MACRO_TYPE_DOUBLE:
              String_format(string,format,macros[j].value.d);
              break;
            case TEXT_MACRO_TYPE_CSTRING:
              String_format(string,format,macros[j].value.s);
              break;
            case TEXT_MACRO_TYPE_STRING:
              String_format(string,format,macros[j].value.string);
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
          String_appendCString(string,name);
        }
      }
      else
      {
        // empty macro: add empty string
        String_format(string,format,"");
      }
    }
  }
  while (macroFlag);

  return string;

  #undef SKIP_SPACES
  #undef APPEND_CHAR
}

/*---------------------------------------------------------------------*/

Errors Misc_executeCommand(const char        *commandTemplate,
                           const TextMacro   macros[],
                           uint              macroCount,
                           ExecuteIOFunction stdoutExecuteIOFunction,
                           ExecuteIOFunction stderrExecuteIOFunction,
                           void              *executeIOUserData
                          )
{
  Errors          error;
  String          commandLine;
  StringTokenizer stringTokenizer;
  String          token;
  String          command;
  StringList      argumentList;
  const char      *path;
  String          fileName;
  bool            foundFlag;
  char const      **arguments;
  int             pipeStdin[2],pipeStdout[2],pipeStderr[2];
  int             pid;
  StringNode      *stringNode;
  uint            n,z;
  int             status;
  bool            sleepFlag;
  String          stdoutLine,stderrLine;
  int             exitcode;
  int             terminateSignal;

  error = ERROR_NONE;
  if (commandTemplate != NULL)
  {
    commandLine = String_new();
    command     = File_newFileName();
    StringList_init(&argumentList);

    // expand command line
    Misc_expandMacros(commandLine,commandTemplate,macros,macroCount);
    printInfo(3,"Execute command '%s'...",String_cString(commandLine));

    // parse command
    String_initTokenizer(&stringTokenizer,commandLine,STRING_BEGIN,STRING_WHITE_SPACES,STRING_QUOTES,FALSE);
    if (!String_getNextToken(&stringTokenizer,&token,NULL))
    {
      String_doneTokenizer(&stringTokenizer);
      StringList_done(&argumentList);
      String_delete(command);
      String_delete(commandLine);
      return ERRORX_(PARSE_COMMAND,0,String_cString(commandLine));
    }
    File_setFileName(command,token);

    // parse arguments
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      StringList_append(&argumentList,token);
    }
    String_doneTokenizer(&stringTokenizer);

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

#if 0
fprintf(stderr,"%s,%d: command %s\n",__FILE__,__LINE__,String_cString(command));
stringNode = argumentList.head;
while (stringNode != NULL)
{
fprintf(stderr,"%s,%d: argument %s\n",__FILE__,__LINE__,String_cString(stringNode->string));
stringNode = stringNode->next;
}
#endif /* 0 */

    #if defined(HAVE_PIPE) && defined(HAVE_FORK) && defined(HAVE_WAITPID)
#if 1
      // create i/o pipes
      if (pipe(pipeStdin) != 0)
      {
        error = ERRORX_(IO_REDIRECT_FAIL,errno,String_cString(commandLine));
        StringList_done(&argumentList);
        String_delete(command);
        String_delete(commandLine);
        return error;
      }
      if (pipe(pipeStdout) != 0)
      {
        error = ERRORX_(IO_REDIRECT_FAIL,errno,String_cString(commandLine));
        close(pipeStdin[0]);
        close(pipeStdin[1]);
        StringList_done(&argumentList);
        String_delete(command);
        String_delete(commandLine);
        return error;
      }
      if (pipe(pipeStderr) != 0)
      {
        error = ERRORX_(IO_REDIRECT_FAIL,errno,String_cString(commandLine));
        close(pipeStdout[0]);
        close(pipeStdout[1]);
        close(pipeStdin[0]);
        close(pipeStdin[1]);
        StringList_done(&argumentList);
        String_delete(command);
        String_delete(commandLine);
        return error;
      }

      // do fork to start separated process
      pid = fork();
      if      (pid == 0)
      {
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

        // execute of external program
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
        execvp(String_cString(command),(char**)arguments);

        // in case exec() fail, return a default exitcode
        HALT_INTERNAL_ERROR("execvp() returned");
      }
      else if (pid < 0)
      {
        error = ERRORX_(EXEC_FAIL,errno,String_cString(commandLine));
        printInfo(3,"FAIL!\n");

        close(pipeStderr[0]);
        close(pipeStderr[1]);
        close(pipeStdout[0]);
        close(pipeStdout[1]);
        close(pipeStdin[0]);
        close(pipeStdin[1]);
        StringList_done(&argumentList);
        String_delete(command);
        String_delete(commandLine);
        return error;
      }

      // close unused pipe handles (the pipe is duplicated by fork(), thus there are two open ends of the pipe)
      close(pipeStderr[1]);
      close(pipeStdout[1]);
      close(pipeStdin[0]);
#else /* 0 */
error = ERROR_NONE;
#endif /* 0 */

      // wait until process terminate and read stdout/stderr
      stdoutLine = String_new();
      stderrLine = String_new();
      status = 0;
      while ((waitpid(pid,&status,WNOHANG) == 0) || (!WIFEXITED(status) && !WIFSIGNALED(status)))
      {
        sleepFlag = TRUE;

        if (readProcessIO(pipeStdout[0],stdoutLine))
        {
          if (stdoutExecuteIOFunction != NULL) stdoutExecuteIOFunction(executeIOUserData,stdoutLine);
          String_clear(stdoutLine);
          sleepFlag = FALSE;
        }
        if (readProcessIO(pipeStderr[0],stderrLine))
        {
          if (stderrExecuteIOFunction != NULL) stderrExecuteIOFunction(executeIOUserData,stderrLine);
          String_clear(stderrLine);
          sleepFlag = FALSE;
        }

        if (sleepFlag)
        {
          Misc_udelay(500LL*1000LL);
        }
      }
      while (readProcessIO(pipeStdout[0],stdoutLine))
      {
        if (stdoutExecuteIOFunction != NULL) stdoutExecuteIOFunction(executeIOUserData,stdoutLine);
        String_clear(stdoutLine);
      }
      while (readProcessIO(pipeStderr[0],stderrLine))
      {
        if (stderrExecuteIOFunction != NULL) stderrExecuteIOFunction(executeIOUserData,stderrLine);
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
        printInfo(3,"ok (exitcode %d)\n",exitcode);
        if (exitcode != 0)
        {
          error = ERRORX_(EXEC_FAIL,exitcode,String_cString(commandLine));
          StringList_done(&argumentList);
          String_delete(command);
          String_delete(commandLine);
          return error;
        }
      }
      else if (WIFSIGNALED(status))
      {
        terminateSignal = WTERMSIG(status);
        error = ERRORX_(EXEC_FAIL,terminateSignal,String_cString(commandLine));
        printInfo(3,"FAIL (signal %d)\n",terminateSignal);
        StringList_done(&argumentList);
        String_delete(command);
        String_delete(commandLine);
        return error;
      }
      else
      {
        printInfo(3,"ok (unknown exit)\n");
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
    #else /* not defined(HAVE_PIPE) && defined(HAVE_FORK) && defined(HAVE_WAITPID) || WIN32 */
      #error pipe()/fork()/waitpid() not available nor Win32 system!
    #endif /* defined(HAVE_PIPE) && defined(HAVE_FORK) && defined(HAVE_WAITPID) || WIN32 */

    // free resources
    StringList_done(&argumentList);
    String_delete(command);
    String_delete(commandLine);
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
  return (timeStampDelta > 0)?(valueDelta*1000)/(double)timeStampDelta:0.0;
}

double Misc_performanceFilterGetAverageValue(PerformanceFilter *performanceFilter)
{
  assert(performanceFilter != NULL);

  return performanceFilter->average;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
