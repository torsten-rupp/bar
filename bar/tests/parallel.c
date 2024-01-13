/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: execute commands in parallel and return exitcode of last
*           command
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "common/global.h"
#include "common/strings.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

/***********************************************************************\
* Name   : execute
* Purpose: execute command
* Input  : commandLine - commadn line
* Output : -
* Return : exit code
* Notes  : -
\***********************************************************************/

LOCAL int execute(const char *commandLine)
{
  StringTokenizer stringTokenizer;
  ConstString     token;
  char            *executable;
  char            *arguments[64+1];
  int             i;

  // parse command line
  String_initTokenizerCString(&stringTokenizer,commandLine,STRING_WHITE_SPACES,STRING_QUOTES,FALSE);
  if (!String_getNextToken(&stringTokenizer,&token,NULL))
  {
    return 127;
  }

  executable = stringDuplicate(String_cString(token));

  i = 0;
  arguments[i] = stringDuplicate(String_cString(token));
  i++;
  while (String_getNextToken(&stringTokenizer,&token,NULL))
  {
    if (i < (int)(SIZE_OF_ARRAY(arguments)-1))
    {
      arguments[i] = stringDuplicate(String_cString(token));
      i++;
    }
  }
  arguments[i] = NULL;

  // execute command
  execvp(executable,(char * const*)arguments);

  // free resources
  do
  {
    i--;
    stringDelete(arguments[i]);
  }
  while (i > 0);
  stringDelete(executable);
  String_doneTokenizer(&stringTokenizer);

  return 1;
}

/***********************************************************************\
* Name   : main
* Purpose: main function
* Input  : -
* Output : -
* Return : exit code
* Notes  : -
\***********************************************************************/

int main(int argc, const char* argv[])
{
  pid_t pids[32];
  uint  pidCount;
  int   status;
  int   exitCode;
  uint  n;

  exitCode = 0;

  // check arguments
  if (argc <= 1)
  {
    fprintf(stderr,"ERROR: no commands given!\n");
    return 1;
  }
  if (stringEquals(argv[1],"-h") || stringEquals(argv[1],"--help"))
  {
    printf("Usage: %s <command>...\n",argv[0]);
  }

  // start concurrent processes
  pidCount = 0;
  for (uint i = 1; i < (uint)argc; i++)
  {
    pids[pidCount] = fork();
    if      (pids[pidCount] == 0)
    {
      return execute(argv[i]);
    }
    else if (pids[pidCount] == -1)
    {
      return 1;
    }
    pidCount++;
  }
  assert(pidCount > 0);

  // wait for last process
  if (waitpid(pids[pidCount-1],&status,0) != -1)
  {
    if      (WIFSIGNALED(status))
    {
      exitCode = 128+WTERMSIG(status);
    }
    else if (WIFEXITED(status))
    {
      exitCode = WEXITSTATUS(status);
    }
    else
    {
      exitCode = 0;
    }
  }
  else
  {
    exitCode = 128;
  }

  // kill concurrent processes
  for (uint i = 0; i < pidCount-1; i++)
  {
    n = 60;
    while (   (waitpid(pids[i],&status,WNOHANG) != pids[i])
           && (n > 0)
          )
    {
      sleep(1);
      n--;
    }

    if ((n == 0) || !WIFEXITED(status))
    {
      kill(pids[i],SIGKILL);
      waitpid(pids[i],&status,0);
    }
  }

  return exitCode;
}
