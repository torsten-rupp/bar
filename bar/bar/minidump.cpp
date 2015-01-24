/***********************************************************************\
*
* $Source$
* $Revision$
* $Author$
* Contents: crash minidump functions
* Systems: Linux
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <assert.h>

#ifdef HAVE_BREAKPAD
  #if   defined(PLATFORM_LINUX)
    #include "client/linux/handler/exception_handler.h"
  #elif defined(PLATFORM_WINDOWS)
    #include "client/windows/handler/exception_handler.h"
  #endif
#endif /* HAVE_BREAKPAD */

#include "global.h"
#include "files.h"

#include "minidump.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define MINIDUMP_FILENAME "bar.mdmp"

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#ifdef HAVE_BREAKPAD
  LOCAL bool               initFlag = FALSE;
  LOCAL char               minidumpFileName[1024];
  LOCAL int                minidumpFileDescriptor;
  LOCAL google_breakpad::MinidumpDescriptor *minidumpDescriptor;
  LOCAL google_breakpad::ExceptionHandler   *exceptionHandler;
#endif /* HAVE_BREAKPAD */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef HAVE_BREAKPAD
/***********************************************************************\
* Name   : printString
* Purpose: print string
* Input  : s - string to print
* Output : -
* Return : -
* Notes  : Do not use printf
\***********************************************************************/

LOCAL void printString(const char *s)
{
  (void)write(STDERR_FILENO,s,strlen(s));
}

/***********************************************************************\
* Name   : minidumpCallback
* Purpose: minidump call back
* Input  : minidumpDescriptor - minidump descriptor
*          context            - context
*          succeeded          - succeeded
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool minidumpCallback(const google_breakpad::MinidumpDescriptor &minidumpDescriptor,
                            void                                      *context,
                            bool                                      succeeded
                           )
{
  struct utsname utsname;

  #define __TO_STRING(z) __TO_STRING_TMP(z)
  #define __TO_STRING_TMP(z) #z
  #define VERSION_MAJOR_STRING __TO_STRING(VERSION_MAJOR)
  #define VERSION_MINOR_STRING __TO_STRING(VERSION_MINOR)
  #define VERSION_SVN_STRING __TO_STRING(VERSION_SVN)
  #define VERSION_STRING VERSION_MAJOR_STRING "." VERSION_MINOR_STRING " (rev. " VERSION_SVN_STRING ")"

  close(minidumpFileDescriptor);

  uname(&utsname);

  // Note: do not use fprintf; it does not work here
  printString("+++ BAR CRASH DUMP ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
  printString("Dump file   : "); printString(minidumpFileName); printString("\n");
  printString("Version     : "); printString(VERSION_STRING); printString("\n");
  printString("OS          : "); printString(utsname.sysname); printString(", "); printString(utsname.release); printString(", "); printString(utsname.machine);  printString("\n");
  printString("Please send the crash dump file and this information to torsten.rupp@gmx.net\n");
  printString("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

  return succeeded;

  #undef __TO_STRING
  #undef __TO_STRING_TMP
  #undef VERSION_MAJOR_STRING
  #undef VERSION_MINOR_STRING
  #undef VERSION_SVN_STRING
  #undef VERSION_STRING
}
#endif /* HAVE_BREAKPAD */

/*---------------------------------------------------------------------*/

//LOCAL void crash() { volatile int* a = (int*)(NULL); *a = 1; }

bool MiniDump_init(void)
{
  #ifdef HAVE_BREAKPAD
    // create minidump file name
    snprintf(minidumpFileName,sizeof(minidumpFileName),
             "%s%c%s",
             FILE_TMP_DIRECTORY,
             FILES_PATHNAME_SEPARATOR_CHAR,
             MINIDUMP_FILENAME
    );

    // open minidump file./src/google_breakpad/processor/minidump_processor.h
    minidumpFileDescriptor = open(minidumpFileName,O_CREAT|O_RDWR,0660);
    if (minidumpFileDescriptor == -1)
    {
      (void)unlink(minidumpFileName);
      return FALSE;
    }

    // init minidump
    minidumpDescriptor = new google_breakpad::MinidumpDescriptor(minidumpFileDescriptor);
    if (minidumpDescriptor == NULL)
    {
      (void)close(minidumpFileDescriptor);
      (void)unlink(minidumpFileName);
      return FALSE;
    }
    exceptionHandler = new google_breakpad::ExceptionHandler(*minidumpDescriptor,
                                                             NULL,
                                                             minidumpCallback,
                                                             NULL,
                                                             true,
                                                             -1
                                                            );
    if (exceptionHandler == NULL)
    {
      delete minidumpDescriptor;
      (void)close(minidumpFileDescriptor);
      (void)unlink(minidumpFileName);
      return FALSE;
    }

    initFlag = TRUE;
  #endif /* HAVE_BREAKPAD */

//crash();

  return TRUE;
}

void MiniDump_done(void)
{
  #ifdef HAVE_BREAKPAD
    if (initFlag)
    {
      delete exceptionHandler;
      delete minidumpDescriptor;
      (void)close(minidumpFileDescriptor);
      (void)unlink(minidumpFileName);

      initFlag = FALSE;
    }
  #endif /* HAVE_BREAKPAD */
}

/* end of file */
