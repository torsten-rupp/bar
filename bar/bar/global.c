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
#include <unistd.h>
#ifdef HAVE_BACKTRACE
  #include <execinfo.h>
#endif
#ifdef HAVE_BFD_H
  #include "stacktraces.h"
#endif
#include <assert.h>

#ifndef NDEBUG
  #include <pthread.h>
  #include "lists.h"
#endif /* not NDEBUG */

#include "errors.h"

#include "global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define DEBUG_MAX_FREE_LIST 4000

#define DEBUG_TESTCODE_NAME          "TESTCODE"       // testcode to execute
#define DEBUG_TESTCODE_STOP          "TESTCODE_STOP"  // stop (breakpoint) if test fail

#define DEBUG_TESTCODE_LIST_FILENAME "TESTCODE_LIST"  // file with list with all testcode names
//#define DEBUG_TESTCODE_LIST_FILENAME "TESTCODE_RUN"   //
#define DEBUG_TESTCODE_SKIP_FILENAME "TESTCODE_SKIP"  // file with  testcodes to skip
#define DEBUG_TESTCODE_DONE_FILENAME "TESTCODE_DONE"  // file with  list with done testcode names

#define DEBUG_TESTCODE_NAME_FILENAME "TESTCODE_NAME"  // file with name of current testcode

/**************************** Datatypes ********************************/
#ifndef NDEBUG
  typedef struct DebugResourceNode
  {
    LIST_NODE_HEADER(struct DebugResourceNode);

    const char      *allocFileName;
    ulong           allocLineNb;
    #ifdef HAVE_BACKTRACE
      void const *stackTrace[16];
      int        stackTraceSize;
    #endif /* HAVE_BACKTRACE */

    const char      *freeFileName;
    ulong           freeLineNb;
    #ifdef HAVE_BACKTRACE
      void const *deleteStackTrace[16];
      int        deleteStackTraceSize;
    #endif /* HAVE_BACKTRACE */
    const char *typeName;
    const void *resource;
    uint       size;
  } DebugResourceNode;

  typedef struct
  {
    LIST_HEADER(DebugResourceNode);
  } DebugResourceList;
#endif /* not NDEBUG */

/**************************** Variables ********************************/
#ifndef NDEBUG
  LOCAL pthread_once_t    debugResourceInitFlag = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutex_t   debugResourceLock;
  LOCAL DebugResourceList debugResourceAllocList;
  LOCAL DebugResourceList debugResourceFreeList;
#endif /* not NDEBUG */

#ifndef NDEBUG
  pthread_mutex_t debugConsoleLock = PTHREAD_MUTEX_INITIALIZER;
  char            debugTestCodeName[256];
  const char      *__testCodeName__;
#endif /* not NDEBUG */

/****************************** Macros *********************************/

/**************************** Functions ********************************/

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************\
* Name   : debugResourceInit
* Purpose: initialize debug resource functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifndef NDEBUG
LOCAL void debugResourceInit(void)
{
  pthread_mutex_init(&debugResourceLock,NULL);
  List_init(&debugResourceAllocList);
  List_init(&debugResourceFreeList);
}
#endif /* not NDEBUG */

// ----------------------------------------------------------------------

#ifndef NDEBUG
void __dprintf__(const char *__fileName__,
                 uint       __lineNb__,
                 const char *format,
                 ...
                )
{
  va_list arguments;

  fprintf(stdout,"DEBUG %s, %u: ",__fileName__,__lineNb__);
  va_start(arguments,format);
  vfprintf(stdout,format,arguments);
  va_end(arguments);
  fprintf(stdout,"\n");
}
#endif /* NDEBUG */

#ifdef NDEBUG
void __halt(int        exitcode,
            const char *format,
            ...
           )
#else /* not NDEBUG */
void __halt(const char *__fileName__,
            uint       __lineNb__,
            int        exitcode,
            const char *format,
            ...
           )
#endif /* NDEBUG */
{
  va_list arguments;

  #ifndef NDEBUG
    assert(__fileName__ != NULL);
  #endif /* not NDEBUG */
  assert(format != NULL);

  va_start(arguments,format);
  vfprintf(stderr,format,arguments);
  va_end(arguments);
  #ifndef NDEBUG
    fprintf(stderr," - halt in file %s, line %u\n",__fileName__,__lineNb__);
  #else /* NDEBUG */
    fprintf(stderr," - halt\n");
  #endif /* not NDEBUG */
  exit(exitcode);
}

#ifdef NDEBUG
void __abort(const char *prefix,
             const char *format,
             ...
            )
#else /* not NDEBUG */
void __abort(const char *__fileName__,
             uint       __lineNb__,
             const char *prefix,
             const char *format,
             ...
            )
#endif /* NDEBUG */
{
  va_list arguments;

  #ifndef NDEBUG
    assert(__fileName__ != NULL);
  #endif /* not NDEBUG */
  assert(format != NULL);

  if (prefix != NULL) fprintf(stderr,"%s", prefix);
  va_start(arguments,format);
  vfprintf(stderr,format,arguments);
  va_end(arguments);
  #ifndef NDEBUG
    fprintf(stderr," - program aborted in file %s, line %u\n",__fileName__,__lineNb__);
  #else /* NDEBUG */
    fprintf(stderr," - program aborted\n");
  #endif /* not NDEBUG */
  abort();
}
void __abortAt(const char *fileName,
               uint       lineNb,
               const char *prefix,
               const char *format,
               ...
              )
{
  va_list arguments;

  assert(fileName != NULL);
  assert(format != NULL);

  if (prefix != NULL) fprintf(stderr,"%s", prefix);
  va_start(arguments,format);
  vfprintf(stderr,format,arguments);
  va_end(arguments);
  fprintf(stderr," - program aborted in file %s, line %u\n",fileName,lineNb);
  abort();
}

#ifndef NDEBUG
void __cyg_profile_func_enter(void *functionCode, void *callAddress) ATTRIBUTE_NO_INSTRUMENT_FUNCTION;
void __cyg_profile_func_enter(void *functionCode, void *callAddress)
{
  UNUSED_VARIABLE(functionCode);
  UNUSED_VARIABLE(callAddress);
//fprintf(stderr,"%s, %d: enter %p\n",__FILE__,__LINE__,functionCode);
}

void __cyg_profile_func_exit(void *functionCode, void *callAddress) ATTRIBUTE_NO_INSTRUMENT_FUNCTION;
void __cyg_profile_func_exit(void *functionCode, void *callAddress)
{
  UNUSED_VARIABLE(functionCode);
  UNUSED_VARIABLE(callAddress);
//fprintf(stderr,"%s, %d: exit %p\n",__FILE__,__LINE__,functionCode);
}

bool debugIsTestCodeEnabled(const char *__fileName__,
                            uint       __lineNb__,
                            const char *functionName,
                            uint       counter
                           )
{
  bool       isTestCodeEnabledFlag;
  const char *value;
  bool       isInListFileFlag,isInSkipFileFlag,isInDoneFileFlag;
  FILE       *file;
  char       line[1024];
  char       *s,*t;

  assert(functionName != NULL);

  isTestCodeEnabledFlag = FALSE;

  // get testcode name
  stringFormat(debugTestCodeName,sizeof(debugTestCodeName),"%s%d",functionName,counter);

  // check environment variable
  value = getenv(DEBUG_TESTCODE_NAME);
  if ((value != NULL) && !stringIsEmpty(value))
  {
    isTestCodeEnabledFlag = stringEquals(debugTestCodeName,value);
  }
  else
  {
    // check test code list file
    isInListFileFlag = FALSE;
    value = getenv(DEBUG_TESTCODE_LIST_FILENAME);
    if (value != NULL)
    {
      // open file
      file = fopen(value,"r");
      if (file != NULL)
      {
        // read file
        while ((fgets(line,sizeof(line),file) != NULL) && !isInListFileFlag)
        {
          // trim spaces, LF
          s = line;
          while (isspace(*s)) { s++; }
          t = s;
          while ((*t) != '\0') { t++; }
          t--;
          while ((t > s) && isspace(*t)) { (*t) = '\0'; t--; }

          // skip empty/commented lines
          if (((*s) == '\0') || ((*s) == '#')) continue;

          // name
          isInListFileFlag = stringEquals(debugTestCodeName,strtok(s," "));
        }

        // close file
        fclose(file);
      }

      // append to list file (if not exists)
      if (!isInListFileFlag)
      {
        file = fopen(value,"a");
        if (file != NULL)
        {
          fprintf(file,"%s %s %d\n",debugTestCodeName,__fileName__,__lineNb__);
          fclose(file);
        }
      }
    }

    // check test code skip file
    isInSkipFileFlag = FALSE;
    if (isInListFileFlag)
    {
      value = getenv(DEBUG_TESTCODE_SKIP_FILENAME);
      if (value != NULL)
      {
        // check if name is in done file
        file = fopen(value,"r");
        if (file != NULL)
        {
          while ((fgets(line,sizeof(line),file) != NULL) && !isInSkipFileFlag)
          {
            // trim spaces, LF
            s = line;
            while (isspace(*s)) { s++; }
            t = s;
            while ((*t) != '\0') { t++; }
            t--;
            while ((t > s) && isspace(*t)) { (*t) = '\0'; t--; }

            // skip empty/commented lines
            if (((*s) == '\0') || ((*s) == '#')) continue;

            // name
            isInSkipFileFlag = stringEquals(debugTestCodeName,s);
          }

          // close file
          fclose(file);
        }
      }
    }

    // check test code done file
    isInDoneFileFlag = TRUE;
    if (isInListFileFlag && !isInSkipFileFlag)
    {
      value = getenv(DEBUG_TESTCODE_DONE_FILENAME);
      if (value != NULL)
      {
        isInDoneFileFlag = FALSE;

        // check if name is in done file
        file = fopen(value,"r");
        if (file != NULL)
        {
          while ((fgets(line,sizeof(line),file) != NULL) && !isInDoneFileFlag)
          {
            // trim spaces, LF
            s = line;
            while (isspace(*s)) { s++; }
            t = s;
            while ((*t) != '\0') { t++; }
            t--;
            while ((t > s) && isspace(*t)) { (*t) = '\0'; t--; }

            // skip empty/commented lines
            if (((*s) == '\0') || ((*s) == '#')) continue;

            // name
            isInDoneFileFlag = stringEquals(debugTestCodeName,s);
          }

          // close file
          fclose(file);
        }
      }
    }

    isTestCodeEnabledFlag = isInListFileFlag && !isInSkipFileFlag && !isInDoneFileFlag;
  }

  if (isTestCodeEnabledFlag)
  {
    // append to done-list
    value = getenv(DEBUG_TESTCODE_DONE_FILENAME);
    if (value != NULL)
    {
      file = fopen(value,"a");
      if (file != NULL)
      {
        fprintf(file,"%s\n",debugTestCodeName);
        fclose(file);
      }
    }

    // create file with name of current testcode
    value = getenv(DEBUG_TESTCODE_NAME_FILENAME);
    if (value != NULL)
    {
      // write name to file
      file = fopen(value,"w");
      if (file != NULL)
      {
        fprintf(file,"%s\n",debugTestCodeName);
        fclose(file);
      }
      fprintf(stderr,"DEBUG: -----------------------------------------------------------------\n");
      fprintf(stderr,"DEBUG: Execute testcode '%s', %s, line %u\n",debugTestCodeName,__fileName__,__lineNb__);
    }

    // set testcode name
    __testCodeName__ = debugTestCodeName;
  }
  else
  {
    // clear testcode name
    __testCodeName__ = NULL;
  }

  return isTestCodeEnabledFlag;
}

Errors debugTestCodeError(void)
{
  if (getenv(DEBUG_TESTCODE_STOP) != NULL)
  {
    __BP();
  }
  return ERRORX_(TESTCODE,0,__testCodeName__);
}

void debugLocalResource(const char *__fileName__,
                        uint       __lineNb__,
                        const void *resource
                       )
{
  UNUSED_VARIABLE(__fileName__);
  UNUSED_VARIABLE(__lineNb__);
  UNUSED_VARIABLE(resource);
}
#endif /* not NDEBUG */

#ifndef NDEBUG

void debugAddResourceTrace(const char *__fileName__,
                           uint       __lineNb__,
                           const char *typeName,
                           const void *resource,
                           uint       size
                          )
{
  DebugResourceNode *debugResourceNode;

  pthread_once(&debugResourceInitFlag,debugResourceInit);

  pthread_mutex_lock(&debugResourceLock);
  {
    // check for duplicate initialization in allocated list
    debugResourceNode = debugResourceAllocList.head;
    while ((debugResourceNode != NULL) && ((debugResourceNode->resource != resource) || (debugResourceNode->size != size)))
    {
      debugResourceNode = debugResourceNode->next;
    }
    if (debugResourceNode != NULL)
    {
      fprintf(stderr,"DEBUG WARNING: multiple init of resource '%s' %p (%d bytes) at %s, %u which was previously initialized at %s, %ld!\n",
              typeName,
              resource,
              size,
              __fileName__,
              __lineNb__,
              debugResourceNode->allocFileName,
              debugResourceNode->allocLineNb
             );
      #ifdef HAVE_BACKTRACE
        debugDumpCurrentStackTrace(stderr,0,0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("add resource trace fail");
    }

    // find resource in free-list; reuse or allocate new debug node
    debugResourceNode = debugResourceFreeList.head;
    while ((debugResourceNode != NULL) && ((debugResourceNode->resource != resource) || (debugResourceNode->size != size)))
    {
      debugResourceNode = debugResourceNode->next;
    }
    if (debugResourceNode != NULL)
    {
      List_remove(&debugResourceFreeList,debugResourceNode);
    }
    else
    {
      debugResourceNode = LIST_NEW_NODEX(__fileName__,__lineNb__,DebugResourceNode);
      if (debugResourceNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }
    }

    // init resource node
    debugResourceNode->allocFileName = __fileName__;
    debugResourceNode->allocLineNb   = __lineNb__;
    #ifdef HAVE_BACKTRACE
      debugResourceNode->stackTraceSize = backtrace((void*)debugResourceNode->stackTrace,SIZE_OF_ARRAY(debugResourceNode->stackTrace));
    #endif /* HAVE_BACKTRACE */
    debugResourceNode->freeFileName  = NULL;
    debugResourceNode->freeLineNb    = 0;
    #ifdef HAVE_BACKTRACE
      debugResourceNode->deleteStackTraceSize = 0;
    #endif /* HAVE_BACKTRACE */
    debugResourceNode->typeName = typeName;
    debugResourceNode->resource = resource;
    debugResourceNode->size     = size;

    // add resource to allocated-list
    List_append(&debugResourceAllocList,debugResourceNode);
  }
  pthread_mutex_unlock(&debugResourceLock);
}

void debugRemoveResourceTrace(const char *__fileName__,
                              uint       __lineNb__,
                              const void *resource,
                              uint       size
                             )
{
  DebugResourceNode *debugResourceNode;

  pthread_once(&debugResourceInitFlag,debugResourceInit);

  pthread_mutex_lock(&debugResourceLock);
  {
    // find in free-list to check for duplicate free
    debugResourceNode = debugResourceFreeList.head;
    while ((debugResourceNode != NULL) && ((debugResourceNode->resource != resource) || (debugResourceNode->size != size)))
    {
      debugResourceNode = debugResourceNode->next;
    }
    if (debugResourceNode != NULL)
    {
      fprintf(stderr,"DEBUG ERROR: multiple free of resource '%s' %p (%d bytes) at %s, %u and previously at %s, %lu which was allocated at %s, %ld!\n",
              debugResourceNode->typeName,
              debugResourceNode->resource,
              debugResourceNode->size,
              __fileName__,
              __lineNb__,
              debugResourceNode->freeFileName,
              debugResourceNode->freeLineNb,
              debugResourceNode->allocFileName,
              debugResourceNode->allocLineNb
             );
      #ifdef HAVE_BACKTRACE
        fprintf(stderr,"  allocated at");
        debugDumpStackTrace(stderr,4,debugResourceNode->stackTrace,debugResourceNode->stackTraceSize,0);
        fprintf(stderr,"  deleted at");
        debugDumpStackTrace(stderr,4,debugResourceNode->deleteStackTrace,debugResourceNode->deleteStackTraceSize,0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("remove resource trace fail");
    }

    // remove resource from allocated list, add resource to free-list, shorten free-list
    debugResourceNode = debugResourceAllocList.head;
    while ((debugResourceNode != NULL) && ((debugResourceNode->resource != resource) || (debugResourceNode->size != size)))
    {
      debugResourceNode = debugResourceNode->next;
    }
    if (debugResourceNode != NULL)
    {
      // remove from allocated list
      List_remove(&debugResourceAllocList,debugResourceNode);

      // add to free list
      debugResourceNode->freeFileName = __fileName__;
      debugResourceNode->freeLineNb   = __lineNb__;
      #ifdef HAVE_BACKTRACE
        debugResourceNode->deleteStackTraceSize = backtrace((void*)debugResourceNode->deleteStackTrace,SIZE_OF_ARRAY(debugResourceNode->deleteStackTrace));
      #endif /* HAVE_BACKTRACE */
      List_append(&debugResourceFreeList,debugResourceNode);

      // shorten free list
      while (debugResourceFreeList.count > DEBUG_MAX_FREE_LIST)
      {
        debugResourceNode = (DebugResourceNode*)List_getFirst(&debugResourceFreeList);
        LIST_DELETE_NODE(debugResourceNode);
      }
    }
    else
    {
      fprintf(stderr,"DEBUG ERROR: resource '%p' (%d bytes) not found in debug list at %s, line %u\n",
              resource,
              size,
              __fileName__,
              __lineNb__
             );
      #ifdef HAVE_BACKTRACE
        debugDumpCurrentStackTrace(stderr,0,0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("remove resource trace fail");
    }
  }
  pthread_mutex_unlock(&debugResourceLock);
}

void debugCheckResourceTrace(const char *__fileName__,
                             uint       __lineNb__,
                             const void *resource
                            )
{
  DebugResourceNode *debugResourceNode;

  pthread_once(&debugResourceInitFlag,debugResourceInit);

  pthread_mutex_lock(&debugResourceLock);
  {
    // find in allocate-list
    debugResourceNode = debugResourceAllocList.head;
    while ((debugResourceNode != NULL) && (debugResourceNode->resource != resource))
    {
      debugResourceNode = debugResourceNode->next;
    }
    if (debugResourceNode == NULL)
    {
      // find in free-list to check for duplicate free
      debugResourceNode = debugResourceFreeList.head;
      while ((debugResourceNode != NULL) && (debugResourceNode->resource != resource))
      {
        debugResourceNode = debugResourceNode->next;
      }
      if (debugResourceNode != NULL)
      {
        fprintf(stderr,"DEBUG ERROR: resource '%s' %p (%d bytes) invalid at %s, %u which was allocated at %s, %ld and freed at %s, %ld!\n",
                debugResourceNode->typeName,
                debugResourceNode->resource,
                debugResourceNode->size,
                __fileName__,
                __lineNb__,
                debugResourceNode->allocFileName,
                debugResourceNode->allocLineNb,
                debugResourceNode->freeFileName,
                debugResourceNode->freeLineNb
               );
        #ifdef HAVE_BACKTRACE
          fprintf(stderr,"  allocated at");
          debugDumpStackTrace(stderr,4,debugResourceNode->stackTrace,debugResourceNode->stackTraceSize,0);
          fprintf(stderr,"  deleted at");
          debugDumpStackTrace(stderr,4,debugResourceNode->deleteStackTrace,debugResourceNode->deleteStackTraceSize,0);
        #endif /* HAVE_BACKTRACE */
      }
      else
      {
        fprintf(stderr,"DEBUG ERROR: resource '%p' not found in debug list at %s, line %u\n",
                resource,
                __fileName__,
                __lineNb__
               );
        #ifdef HAVE_BACKTRACE
          debugDumpCurrentStackTrace(stderr,0,0);
        #endif /* HAVE_BACKTRACE */
      }
      HALT_INTERNAL_ERROR("check resource trace fail");
    }
  }
  pthread_mutex_unlock(&debugResourceLock);
}

void debugResourceDone(void)
{
  pthread_once(&debugResourceInitFlag,debugResourceInit);

  debugResourceCheck();

  pthread_mutex_lock(&debugResourceLock);
  {
    List_done(&debugResourceAllocList,NULL,NULL);
    List_done(&debugResourceFreeList,NULL,NULL);
  }
  pthread_mutex_unlock(&debugResourceLock);
}

void debugResourceDumpInfo(FILE *handle)
{
  DebugResourceNode *debugResourceNode;

  pthread_once(&debugResourceInitFlag,debugResourceInit);

  pthread_mutex_lock(&debugResourceLock);
  {
    LIST_ITERATE(&debugResourceAllocList,debugResourceNode)
    {
      fprintf(handle,"DEBUG: resource '%s' %p (%d bytes) allocated at %s, line %ld\n",
              debugResourceNode->typeName,
              debugResourceNode->resource,
              debugResourceNode->size,
              debugResourceNode->allocFileName,
              debugResourceNode->allocLineNb
             );
    }
  }
  pthread_mutex_unlock(&debugResourceLock);
}

void debugResourcePrintInfo(void)
{
  debugResourceDumpInfo(stderr);
}

void debugResourcePrintStatistics(void)
{
  pthread_once(&debugResourceInitFlag,debugResourceInit);

  pthread_mutex_lock(&debugResourceLock);
  {
    fprintf(stderr,"DEBUG: %lu resource(s) allocated\n",
            List_count(&debugResourceAllocList)
           );
  }
  pthread_mutex_unlock(&debugResourceLock);
}

void debugResourceCheck(void)
{
  DebugResourceNode *debugResourceNode;

  pthread_once(&debugResourceInitFlag,debugResourceInit);


  pthread_mutex_lock(&debugResourceLock);
  {
    if (!List_isEmpty(&debugResourceAllocList))
    {
      LIST_ITERATE(&debugResourceAllocList,debugResourceNode)
      {
        fprintf(stderr,"DEBUG: lost resource '%s' %p (%d bytes) allocated at %s, line %ld\n",
                debugResourceNode->typeName,
                debugResourceNode->resource,
                debugResourceNode->size,
                debugResourceNode->allocFileName,
                debugResourceNode->allocLineNb
               );
        #ifdef HAVE_BACKTRACE
          debugDumpStackTrace(stderr,0,debugResourceNode->stackTrace,debugResourceNode->stackTraceSize,0);
        #endif /* HAVE_BACKTRACE */
      }
      fprintf(stderr,"DEBUG: %lu resource(s) lost\n",
              List_count(&debugResourceAllocList)
             );
      HALT_INTERNAL_ERROR_LOST_RESOURCE();
    }
  }
  pthread_mutex_unlock(&debugResourceLock);
}
#endif /* not NDEBUG */

#ifndef NDEBUG
typedef struct
{
  FILE *handle;
  uint indent;
  uint skipFrameCount;
  uint count;
} StackTraceOutputInfo;

LOCAL void debugDumpStackTraceOutputSymbol(const void *address,
                                           const char *fileName,
                                           const char *symbolName,
                                           uint       lineNb,
                                           void       *userData
                                          )
{
  StackTraceOutputInfo *stackTraceOutputInfo = (StackTraceOutputInfo*)userData;
  uint                 i;

  assert(stackTraceOutputInfo != NULL);
  assert(stackTraceOutputInfo->handle != NULL);

  // skip at least first two stack frames: this function and signal handler function
  if (stackTraceOutputInfo->count > 1+stackTraceOutputInfo->skipFrameCount)
  {
    if (fileName   == NULL) fileName   = "<unknown file>";
    if (symbolName == NULL) symbolName = "<unknown symbol>";
    for (i = 0; i < stackTraceOutputInfo->indent; i++) fputc(' ',stackTraceOutputInfo->handle);
    fprintf(stackTraceOutputInfo->handle,"  [0x%08lx] %s (%s:%u)\n",(uintptr_t)address,symbolName,fileName,lineNb);
  }
  stackTraceOutputInfo->count++;
}

void debugDumpStackTrace(FILE       *handle,
                         uint       indent,
                         void const *stackTrace[],
                         uint       stackTraceSize,
                         uint       skipFrameCount
                        )
{
  #ifdef HAVE_BFD_INIT
    char                 executableName[PATH_MAX];
    int                  n;
    StackTraceOutputInfo stackTraceOutputInfo;
  #elif HAVE_BACKTRACE_SYMBOLS
    const char **functionNames;
    uint       z;
    uint       i;
  #else /* not HAVE_... */
  #endif /* HAVE_... */

  assert(handle != NULL);
  assert(stackTrace != NULL);

  #ifdef HAVE_BFD_INIT
    // get executable name
    n = readlink("/proc/self/exe",executableName,sizeof(executableName)-1);
    if (n == -1)
    {
      return;
    }
    assert((size_t)n < sizeof(executableName));
    executableName[n] = '\0';

    // output stack trace
    stackTraceOutputInfo.handle         = handle;
    stackTraceOutputInfo.indent         = indent;
    stackTraceOutputInfo.skipFrameCount = skipFrameCount;
    stackTraceOutputInfo.count          = 0;
    Stacktrace_getSymbolInfo(executableName,
                             stackTrace,
                             stackTraceSize,
                             debugDumpStackTraceOutputSymbol,
                             &stackTraceOutputInfo
                            );
  #elif HAVE_BACKTRACE_SYMBOLS
    // get function names
    functionNames = (const char **)backtrace_symbols((void *const*)stackTrace,stackTraceSize);
    if (functionNames == NULL)
    {
      return;
    }

    // output stack trace
    for (z = 1+skipFrameCount; z < stackTraceSize; z++)
    {
      for (i = 0; i < indent; i++) fputc(' ',handle);
      fprintf(handle,"  %2d %p: %s\n",z,stackTrace[z],functionNames[z]);
    }
    free(functionNames);
  #else /* not HAVE_... */
    fprintf(handle,"  not available\n");
  #endif /* HAVE_... */
}

void debugDumpCurrentStackTrace(FILE *handle,
                                uint indent,
                                uint skipFrameCount
                               )
{
  #if defined(HAVE_BACKTRACE)
    const int MAX_STACK_TRACE_SIZE = 256;

    void *currentStackTrace;
    int  currentStackTraceSize;
  #else /* not defined(HAVE_BACKTRACE) */
    uint i;
  #endif /* defined(HAVE_BACKTRACE) */

  assert(handle != NULL);

  #if defined(HAVE_BACKTRACE)
    currentStackTrace = malloc(sizeof(void*)*MAX_STACK_TRACE_SIZE);
    if (currentStackTrace == NULL) return;

    currentStackTraceSize = backtrace(currentStackTrace,MAX_STACK_TRACE_SIZE);
    debugDumpStackTrace(handle,indent,currentStackTrace,currentStackTraceSize,1+skipFrameCount);

    free(currentStackTrace);
  #else /* not defined(HAVE_BACKTRACE) */
    for (i = 0; i < indent; i++) fputc(' ',handle);
    fprintf(handle,"  not available\n");
  #endif /* defined(HAVE_BACKTRACE) */
}
#endif /* not NDEBUG */

#ifndef NDEBUG
void debugDumpMemory(const void *address, uint length, bool printAddress)
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
#endif /* not NDEBUG */

#ifdef __cplusplus
}
#endif

/* end of file */
