/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/misc.c,v $
* $Revision: 1.3 $
* $Author: torsten $
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
* Name   : expandMacros
* Purpose: expand macros %... in string
* Input  : s          - string variable
*          t          - string with macros
*          macros     - array with macro definitions
*          macroCount - number of macro definitions
* Output : s - string with expanded macros
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void expandMacros(String             s,
                        const String       t,
                        const ExecuteMacro macros[],
                        uint               macroCount
                       )
{
  long i0,i1;
  long i;
  int  iz,z;
  char buffer[128];

  assert((macroCount == 0) || (macros != NULL));

  String_clear(s);
  i0 = 0;
  do
  {
    /* find next macro */
    iz = -1;
    i1 = -1;
    for (z = 0; z < macroCount; z++)
    {
      i = String_findCString(t,i0,macros[z].name);
      if ((i >= 0) && ((i1 < 0) || (i < i1)))
      {
        iz = z;
        i1 = i;
      }
    }

    /* expand macro */
    if (iz >= 0)
    {
      String_appendSub(s,t,i0,i1-i0);
      switch (macros[iz].type)
      {
        case EXECUTE_MACRO_TYPE_INT:
          snprintf(buffer,sizeof(buffer)-1,"%d",macros[iz].i); buffer[sizeof(buffer)-1] = '\0';
          String_appendCString(s,buffer);
          break;
        case EXECUTE_MACRO_TYPE_INT64:
          snprintf(buffer,sizeof(buffer)-1,"%lld",macros[iz].l); buffer[sizeof(buffer)-1] = '\0';
          String_appendCString(s,buffer);
          break;
        case EXECUTE_MACRO_TYPE_STRING:
          String_append(s,macros[iz].string);
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break; /* not reached */
        #endif /* NDEBUG */
      }
      i0 = i1+strlen(macros[iz].name);
    }
  }
  while (iz >= 0);
  String_appendSub(s,t,i0,STRING_END);
}

/***********************************************************************\
* Name   : readProcessIO
* Purpose: read process i/o, EOL at LF/CR/BS, skip empty lines
* Input  : fd   - file handle
*          line - line
* Output : line - line
* Return : TRUE if line read, FALSE otherwise
* Notes  : -
\***********************************************************************/

LOCAL bool readProcessIO(int fd, String line)
{
  int  n;
  char ch;

  do
  {
    /* check if data available */
    ioctl(fd,FIONREAD,&n);

    /* read data until EOL found */
    while (n > 0)
    {
      read(fd,&ch,1);
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
//fprintf(stderr,"%s,%d: %ld %ld\n",__FILE__,__LINE__,tv.tv_sec,tv.tv_usec);
    return (uint64)tv.tv_usec+(uint64)tv.tv_sec*1000000LL;
  }
  else
  {
    return 0LL;
  }
}

void Misc_udelay(uint64 time)
{
  struct timespec ts;

//fprintf(stderr,"%s,%d: us=%llu\n",__FILE__,__LINE__,time);
  ts.tv_sec  = (ulong)(time/1000000LL);
  ts.tv_nsec = (ulong)((time%1000000LL)*1000);
  while (nanosleep(&ts,&ts) == -1)
  {
  }  
}

/*---------------------------------------------------------------------*/

Errors Misc_executeCommand(const char         *commandTemplate,
                           const ExecuteMacro macros[],
                           uint               macroCount,
                           ExecuteIOFunction  stdoutExecuteIOFunction,
                           ExecuteIOFunction  stderrExecuteIOFunction,
                           void               *executeIOUserData
                          )
{
  Errors          error;
  String          command;
  StringList      argumentList;
  StringTokenizer stringTokenizer;
  String          token;
  String          argument;
  char const      **arguments;
  int             pipeStdin[2],pipeStdout[2],pipeStderr[2];
  int             pid;
  StringNode      *stringNode;
  uint            n,z;
  int             status;
  String          stdoutLine,stderrLine;
  int             exitcode;

  error = ERROR_NONE;
  if (commandTemplate != NULL)
  {
    command = String_new();
    StringList_init(&argumentList);

    /* parse command */
    String_initTokenizerCString(&stringTokenizer,commandTemplate,STRING_WHITE_SPACES,STRING_QUOTES,FALSE);
    if (!String_getNextToken(&stringTokenizer,&token,NULL))
    {
      String_doneTokenizer(&stringTokenizer);
      StringList_done(&argumentList);
      String_delete(command);
      return ERROR_EXEC_FAIL;
    }
    expandMacros(command,token,macros,macroCount);

    /* parse arguments */
    argument = String_new();
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      expandMacros(argument,token,macros,macroCount);
      StringList_append(&argumentList,argument);
    }
    String_doneTokenizer(&stringTokenizer);
    String_delete(argument);

    /* execute command */
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
    /* create i/o pipes */
    if (pipe(pipeStdin) != 0)
    {
      StringList_done(&argumentList);
      String_delete(command);
      return ERROR_EXEC_FAIL;
    }
    if (pipe(pipeStdout) != 0)
    {
      close(pipeStdin[0]);
      close(pipeStdin[1]);
      StringList_done(&argumentList);
      String_delete(command);
      return ERROR_EXEC_FAIL;
    }
    if (pipe(pipeStderr) != 0)
    {
      close(pipeStdout[0]);
      close(pipeStdout[1]);
      close(pipeStdin[0]);
      close(pipeStdin[1]);
      StringList_done(&argumentList);
      String_delete(command);
      return ERROR_EXEC_FAIL;
    }

    /* do fork to start separated process */
    pid = fork();
    if      (pid == 0)
    {
      /* close stdin, stdout, and stderr and reassign them to the  pipes */
      close(STDERR_FILENO);
      close(STDOUT_FILENO);
      close(STDIN_FILENO);

      /* redirect stdin/stdout/stderr to pipe */
      dup2(pipeStdin[0],STDIN_FILENO);
      dup2(pipeStdout[1],STDOUT_FILENO);
      dup2(pipeStderr[1],STDERR_FILENO);

      /* close unused pipe handles (the pipes are duplicated by fork(), thus
         there are two open ends of the pipes)
      */
      close(pipeStderr[0]);
      close(pipeStdout[0]);
      close(pipeStdin[1]);

      /* execute of external program */
      n = 1+StringList_count(&argumentList)+1;
      arguments = (char const**)malloc((1000+n)*sizeof(char*));
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

      /* in case exec() fail, return a default exitcode */
fprintf(stderr,"%s,%d: hier?\n",__FILE__,__LINE__);
HALT_INTERNAL_ERROR("not reachable");
    }
    else if (pid < 0)
    {
      close(pipeStderr[0]);
      close(pipeStderr[1]);
      close(pipeStdout[0]);
      close(pipeStdout[1]);
      close(pipeStdin[0]);
      close(pipeStdin[1]);
      StringList_done(&argumentList);
      String_delete(command);
      return ERROR_EXEC_FAIL;
    }

    /* close unused pipe handles (the pipe is duplicated by fork(), thus there are two open ends of the pipe) */
    close(pipeStderr[1]);
    close(pipeStdout[1]);
    close(pipeStdin[0]);
#else /* 0 */
error = ERROR_NONE;
#endif /* 0 */

    /* wait until process terminate and read stdout/stderr */
    stdoutLine = String_new();
    stderrLine = String_new();
    while ((waitpid(pid,&status,WNOHANG) == 0) || !WIFEXITED(status))
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
        Misc_udelay(100LL*1000LL);
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

    /* free resources */
    close(pipeStderr[0]);
    close(pipeStdout[0]);
    close(pipeStdin[1]);
    StringList_done(&argumentList);
    String_delete(command);

    exitcode = WEXITSTATUS(status);
    if (exitcode != 0)
    {
      error = ERROR_EXEC_FAIL;
    }
  }

  return error;
}

/*---------------------------------------------------------------------*/

void Misc_waitEnter(void)
{
  struct termios oldTermioSettings;
  struct termios termioSettings;
  char           s[2];

  /* save current console settings */
  tcgetattr(STDIN_FILENO,&oldTermioSettings);

  /* disable echo */
  memcpy(&termioSettings,&oldTermioSettings,sizeof(struct termios));
  termioSettings.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO,TCSANOW,&termioSettings);

  /* read line */
  fgets(s,2,stdin);

  /* restore console settings */
  tcsetattr(STDIN_FILENO,TCSANOW,&oldTermioSettings);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
