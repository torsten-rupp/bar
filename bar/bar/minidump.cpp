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
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#ifdef HAVE_BREAKPAD
  #include "client/linux/handler/exception_handler.h"
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
  LOCAL bool                                initFlag = FALSE;
  LOCAL char                                minidumpFileName[1024];
  LOCAL int                                 minidumpFileDescriptor;
  LOCAL google_breakpad::MinidumpDescriptor *minidumpDescriptor;
  LOCAL google_breakpad::ExceptionHandler   *exceptionHandler;
#endif /* HAVE_BREAKPAD */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef HAVE_BREAKPAD
LOCAL bool minidumpCallback(const google_breakpad::MinidumpDescriptor &minidumpDescriptor,
                            void                                      *context,
                            bool                                      succeeded
                           )
{
  #define ERROR_TEXT1 "+++ Created crash dump file '"
  #define ERROR_TEXT2 "'. Please send the crash dump file to torsten.rupp@gmx.net +++\n."

  close(minidumpFileDescriptor);

  // Note: do not use fprintf; it does not work here
  write(STDERR_FILENO,ERROR_TEXT1,strlen(ERROR_TEXT1));
  write(STDERR_FILENO,minidumpFileName,strlen(minidumpFileName));
  write(STDERR_FILENO,ERROR_TEXT2,strlen(ERROR_TEXT2));

  return succeeded;
}
#endif /* HAVE_BREAKPAD */

/*---------------------------------------------------------------------*/

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

    // open minidump file
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
    if (minidumpDescriptor == NULL)
    {
      delete minidumpDescriptor;
      (void)close(minidumpFileDescriptor);
      (void)unlink(minidumpFileName);
      return FALSE;
    }

    initFlag = TRUE;
  #endif /* HAVE_BREAKPAD */

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
