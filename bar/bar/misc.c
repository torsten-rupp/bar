/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: miscellaneous functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>
#include <assert.h>

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
  int  n;
  char ch;

  do
  {
    // check if data available
    ioctl(fd,FIONREAD,&n);

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
            if (String_length(line) > 0) return TRUE;
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
  struct tm tmStruct;

  n = (time_t)dateTime;
  localtime_r(&n,&tmStruct);
  if (year    != NULL) (*year)    = tmStruct.tm_year + 1900;
  if (month   != NULL) (*month)   = tmStruct.tm_mon + 1;
  if (day     != NULL) (*day)     = tmStruct.tm_mday;
  if (hour    != NULL) (*hour)    = tmStruct.tm_hour;
  if (minute  != NULL) (*minute)  = tmStruct.tm_min;
  if (second  != NULL) (*second)  = tmStruct.tm_sec;
  if (weekDay != NULL) (*weekDay) = (tmStruct.tm_wday + WEEKDAY_SUN) % 7;
}

String Misc_formatDateTime(String string, uint64 dateTime, const char *format)
{
  #define START_BUFFER_SIZE 256
  #define DELTA_BUFFER_SIZE 64

  time_t    n;
  struct tm tmStruct;
  char      *buffer;
  uint      bufferSize;
  int       length;

  assert(string != NULL);

  n = (time_t)dateTime;
  localtime_r(&n,&tmStruct);

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
    length = strftime(buffer,bufferSize-1,format,&tmStruct);
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
  assert(hour >= 0);
  assert(hour <= 23);
  assert(minute >= 0);
  assert(minute <= 59);
  assert(second >= 0);
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
  struct timespec ts;

  ts.tv_sec  = (ulong)(time/1000000LL);
  ts.tv_nsec = (ulong)((time%1000000LL)*1000);
  while (nanosleep(&ts,&ts) == -1)
  {
  }
}

/*---------------------------------------------------------------------*/

String Misc_expandMacros(String          string,
                         const char      *template,
                         const TextMacro macros[],
                         uint            macroCount
                        )
{
  long       templateLength;
  long       i0,i1;
  int        index;
  long       i;
  const char *s;
  int        z;
  char       format[128];

  assert(template != NULL);
  assert((macroCount == 0) || (macros != NULL));

  templateLength = strlen(template);

  String_clear(string);
  i0 = 0;
  do
  {
    // find next macro
    i1    = -1;
    index = -1;
    for (z = 0; z < macroCount; z++)
    {
      s = strstr(&template[i0],macros[z].name);
      if (s != NULL)
      {
        i = (long)(s-template);
        if ((i1 < 0) || (i < i1))
        {
          i1    = i;
          index = z;
        }
      }
    }

    // expand macro
    if (index >= 0)
    {
      // add prefix string
      String_appendBuffer(string,&template[i0],i1-i0);
      i0 = i1+strlen(macros[index].name);

      // find format string (if any)
      if ((i0 < templateLength) && (template[i0] == ':'))
      {
        // skip ':'
        i0++;

        // get format string
        i = 0;
        format[i] = '%'; i++;
        while (   (i0 < templateLength)
               && (   isdigit(template[i0])
                   || (template[i0] == '-')
                   || (template[i0] == '.')
                  )
              )
        {
          if (i < sizeof(format)-1)
          {
            format[i] = template[i0]; i++;
          }
          i0++;
        }
        if (i0 < templateLength)
        {
          if (i < sizeof(format)-1)
          {
            format[i] = template[i0]; i++;
          }
          i0++;
        }
        format[i] = '\0';
      }
      else
      {
        // predefined format string
        switch (macros[index].type)
        {
          case TEXT_MACRO_TYPE_INTEGER:
            strcpy(format,"%d");
            break;
          case TEXT_MACRO_TYPE_INTEGER64:
            strcpy(format,"%lld");
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

      switch (macros[index].type)
      {
        case TEXT_MACRO_TYPE_INTEGER:
          String_format(string,format,macros[index].value.i);
          break;
        case TEXT_MACRO_TYPE_INTEGER64:
          String_format(string,format,macros[index].value.l);
          break;
        case TEXT_MACRO_TYPE_CSTRING:
          String_format(string,format,macros[index].value.s);
          break;
        case TEXT_MACRO_TYPE_STRING:
          String_format(string,format,macros[index].value.string);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
    }
  }
  while (index >= 0);

  // add postfix string
  String_appendBuffer(string,&template[i0],templateLength-i0);

  return string;
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
      return ERRORX(PARSE_COMMAND,0,String_cString(commandLine));
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

#if 1
    // create i/o pipes
    if (pipe(pipeStdin) != 0)
    {
      error = ERRORX(IO_REDIRECT_FAIL,errno,String_cString(commandLine));
      StringList_done(&argumentList);
      String_delete(command);
      String_delete(commandLine);
      return error;
    }
    if (pipe(pipeStdout) != 0)
    {
      error = ERRORX(IO_REDIRECT_FAIL,errno,String_cString(commandLine));
      close(pipeStdin[0]);
      close(pipeStdin[1]);
      StringList_done(&argumentList);
      String_delete(command);
      String_delete(commandLine);
      return error;
    }
    if (pipe(pipeStderr) != 0)
    {
      error = ERRORX(IO_REDIRECT_FAIL,errno,String_cString(commandLine));
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
#if 1
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
#endif /* 0 */

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
      error = ERRORX(EXEC_FAIL,errno,String_cString(commandLine));
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
      if (readProcessIO(pipeStdout[0],stdoutLine))
      {
        if (stdoutExecuteIOFunction != NULL) stdoutExecuteIOFunction(executeIOUserData,stdoutLine);
        String_clear(stdoutLine);
      }
      if (readProcessIO(pipeStderr[0],stderrLine))
      {
        if (stderrExecuteIOFunction != NULL) stderrExecuteIOFunction(executeIOUserData,stderrLine);
        String_clear(stderrLine);
      }
      else
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
        error = ERRORX(EXEC_FAIL,exitcode,String_cString(commandLine));
        StringList_done(&argumentList);
        String_delete(command);
        String_delete(commandLine);
        return error;
      }
    }
    else if (WIFSIGNALED(status))
    {
      terminateSignal = WTERMSIG(status);
      error = ERRORX(EXEC_FAIL,terminateSignal,String_cString(commandLine));
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
  struct termios oldTermioSettings;
  struct termios termioSettings;
  char           s[2];

  // save current console settings
  tcgetattr(STDIN_FILENO,&oldTermioSettings);

  // disable echo
  memcpy(&termioSettings,&oldTermioSettings,sizeof(struct termios));
  termioSettings.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO,TCSANOW,&termioSettings);

  // read line
  if (fgets(s,2,stdin) == NULL)
  {
    // ignore error
  }

  // restore console settings
  tcsetattr(STDIN_FILENO,TCSANOW,&oldTermioSettings);
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

double Misc_performanceFilterGetValue(PerformanceFilter *performanceFilter,
                                      uint              seconds
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
