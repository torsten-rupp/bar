/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: global definitions
* Systems: Linux
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory 

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef HAVE_BACKTRACE
  #include <execinfo.h>
#endif
#include <assert.h>

#include "global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/**************************** Datatypes ********************************/

/**************************** Variables ********************************/

/****************************** Macros *********************************/

/**************************** Functions ********************************/

#ifdef __cplusplus
extern "C" {
#endif

void __halt(const char   *filename,
            unsigned int lineNb,
            int          exitcode,
            const char   *format,
            ...
           )
{
  va_list arguments;

  assert(filename != NULL);
  assert(format != NULL);

  va_start(arguments,format);
  vfprintf(stderr,format,arguments);
  va_end(arguments);
  fprintf(stderr," - halt in file %s, line %d\n", filename, lineNb);
  exit(exitcode);
}

void __abort(const char   *filename,
             unsigned int lineNb,
             const char   *prefix,
             const char   *format,
             ...
            )
{
  va_list arguments;

  assert(filename != NULL);
  assert(format != NULL);

  if (prefix != NULL) fprintf(stderr,"%s", prefix);
  va_start(arguments,format);
  vfprintf(stderr,format,arguments);
  va_end(arguments);
  fprintf(stderr," - program aborted in file %s, line %d\n", filename, lineNb);
  abort();
}

#if !defined(NDEBUG) && defined(HAVE_BACKTRACE)
void debugDumpStackTrace(FILE *handle, const char *title, uint indent, void const *stackTrace[], uint stackTraceSize)
{
  const char **functionNames;
  uint       i,z;

  assert(handle != NULL);
  assert(title != NULL);
  assert(stackTrace != NULL);

  for (i = 0; i < indent; i++) fprintf(handle," ");
  fprintf(handle,"C stack trace: %s\n",title);
  functionNames = (const char **)backtrace_symbols((void *const*)stackTrace,stackTraceSize);
  if (functionNames != NULL)
  {
    for (z = 1; z < stackTraceSize; z++)
    {
      for (i = 0; i < indent; i++) fprintf(handle," ");
      fprintf(handle,"  %2d %p: %s\n",z,stackTrace[z],functionNames[z]);
    }
    free(functionNames);
  }
}

void debugDumpCurrentStackTrace(FILE *handle, const char *title, uint indent)
{
  const int MAX_STACK_TRACE_SIZE = 256;

  void *currentStackTrace;
  int  currentStackTraceSize;

  assert(handle != NULL);
  assert(title != NULL);

  currentStackTrace = malloc(sizeof(void*)*MAX_STACK_TRACE_SIZE);
  if (currentStackTrace == NULL) return;

  currentStackTraceSize = backtrace(currentStackTrace,MAX_STACK_TRACE_SIZE);
  debugDumpStackTrace(handle,title,indent,currentStackTrace,currentStackTraceSize);

  free(currentStackTrace);
}
#endif /* !defined(NDEBUG) && defined(HAVE_BACKTRACE) */

#ifndef NDEBUG
void debugDumpMemory(bool printAddress, const void *address, uint length)
{
  const byte *p;
  uint       z,i;

  z = 0;
  while (z < length)
  {
    p = (const byte*)address+z;
    if (printAddress) fprintf(stderr,"%08lx:",(unsigned long)p);
    fprintf(stderr,"%08lx  ",(unsigned long)(p-(byte*)address));

    for (i = 0; i < 16; i++)
    {
      if ((z+i) < length)
      {
        p = (const byte*)address+z+i;
        fprintf(stderr,"%02x ",((uint)(*p)) & 0xFF);
      }
      else
      {
        fprintf(stderr,"   ");
      }
    }
    fprintf(stderr,"  ");

    for (i = 0; i < 16; i++)
    {
      if ((z+i) < length)
      {
        p = (const byte*)address+z+i;
        fprintf(stderr,"%c",isprint((int)(*p))?(*p):'.');
      }
      else
      {
      }
    }
    fprintf(stderr,"\n");

    z += 16;
  }
}
#endif /* NDEBUG */

#ifdef __cplusplus
}
#endif

/* end of file */
