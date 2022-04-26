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
  int             exitCode;

  // parse command line
  String_initTokenizerCString(&stringTokenizer,commandLine,STRING_WHITE_SPACES,STRING_QUOTES,FALSE);
  if (!String_getNextToken(&stringTokenizer,&token,NULL))
  {
    return 127;
  }

  i = 0;

  executable = stringDuplicate(String_cString(token));
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
  exitCode = execvp(executable,(char * const*)arguments);

  // free resources
  do
  {
    i--;
    stringDelete(arguments[i]);
  }
  while (i >= 0);
  stringDelete(executable);
  String_doneTokenizer(&stringTokenizer);

  return exitCode;
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
    if (pids[pidCount] == 0)
    {
      return execute(argv[i]);
    }
    pidCount++;
  }
  assert(pidCount > 0);

  // wait for last process
  waitpid(pids[pidCount-1],&status,0);
  exitCode = WEXITSTATUS(status);

  // kill concurrent processes
  for (uint i = 0; i < pidCount-1; i++)
  {
    kill(pids[i],9);
  }

  return exitCode;
}
