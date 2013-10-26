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

#ifndef NDEBUG
  #include <pthread.h>
  #include "lists.h"
#endif /* not NDEBUG */

#include "global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define DEBUG_MAX_FREE_LIST 4000

#define DEBUG_TESTCODE_NAME          "TESTCODE"
#define DEBUG_TESTCODE_LIST_FILENAME "TESTCODE_LIST"
#define DEBUG_TESTCODE_SKIP_FILENAME "TESTCODE_SKIP"
#define DEBUG_TESTCODE_NAME_FILENAME "TESTCODE_NAME"
#define DEBUG_TESTCODE_DONE_FILENAME "TESTCODE_DONE"

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
  const char *__testCodeName__;
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

void __halt(const char *__fileName__,
            uint       __lineNb__,
            int        exitcode,
            const char *format,
            ...
           )
{
  va_list arguments;

  assert(__fileName__ != NULL);
  assert(format != NULL);

  va_start(arguments,format);
  vfprintf(stderr,format,arguments);
  va_end(arguments);
  fprintf(stderr," - halt in file %s, line %u\n",__fileName__,__lineNb__);
  exit(exitcode);
}

void __abort(const char *__fileName__,
             uint       __lineNb__,
             const char *prefix,
             const char *format,
             ...
            )
{
  va_list arguments;

  assert(__fileName__ != NULL);
  assert(format != NULL);

  if (prefix != NULL) fprintf(stderr,"%s", prefix);
  va_start(arguments,format);
  vfprintf(stderr,format,arguments);
  va_end(arguments);
  fprintf(stderr," - program aborted in file %s, line %u\n",__fileName__,__lineNb__);
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
                            const char *name
                           )
{
  bool       isTestCodeEnabledFlag;
  const char *value;
  bool       isInListFileFlag,isInSkipFileFlag,isInDoneFileFlag;
  FILE       *file;
  char       line[1024];
  char       *s,*t;

  assert(name != NULL);

  isTestCodeEnabledFlag = FALSE;

  // check environment variable
  value = getenv(DEBUG_TESTCODE_NAME);
  if ((value != NULL) && stringEquals(name,value))
  {
    isTestCodeEnabledFlag = TRUE;
  }
  else
  {
    isInListFileFlag = FALSE;
    isInSkipFileFlag = FALSE;
    isInDoneFileFlag = FALSE;

    // check test code list file
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
          isInListFileFlag = stringEquals(name,s);
        }

        // close file
        fclose(file);
      }
    }

    // check test code done file
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
            isInSkipFileFlag = stringEquals(name,s);
          }

          // close file
          fclose(file);
        }
      }
    }

    // check test code done file
    if (isInListFileFlag && !isInSkipFileFlag)
    {
      value = getenv(DEBUG_TESTCODE_DONE_FILENAME);
      if (value != NULL)
      {
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
            isInDoneFileFlag = stringEquals(name,s);
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
    value = getenv(DEBUG_TESTCODE_NAME_FILENAME);
    if (value != NULL)
    {
      // write name to file
      file = fopen(value,"w");
      if (file != NULL)
      {
        fputs(name,file); fputc('\n',file);
        fclose(file);
      }
      fprintf(stderr,"DEBUG: Execute testcode '%s', %s, line %u\n",name,__fileName__,__lineNb__);
    }

    __testCodeName__ = name;
  }
  else
  {
    __testCodeName__ = NULL;
  }

  return isTestCodeEnabledFlag;
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
                           const void *resource
                          )
{
  DebugResourceNode *debugResourceNode;

  pthread_once(&debugResourceInitFlag,debugResourceInit);

  pthread_mutex_lock(&debugResourceLock);
  {
    // check for duplicate initialization in allocated list
    debugResourceNode = debugResourceAllocList.head;
    while ((debugResourceNode != NULL) && (debugResourceNode->resource != resource))
    {
      debugResourceNode = debugResourceNode->next;
    }
    if (debugResourceNode != NULL)
    {
      fprintf(stderr,"DEBUG WARNING: multiple init of resource '%s' %p at %s, %u which was previously initialized at %s, %ld!\n",
              typeName,
              resource,
              __fileName__,
              __lineNb__,
              debugResourceNode->allocFileName,
              debugResourceNode->allocLineNb
             );
      #ifdef HAVE_BACKTRACE
        debugDumpCurrentStackTrace(stderr,"",0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("");
    }

    // find resource in free-list; reuse or allocate new debug node
    debugResourceNode = debugResourceFreeList.head;
    while ((debugResourceNode != NULL) && (debugResourceNode->resource != resource))
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

    // add resource to allocated-list
    List_append(&debugResourceAllocList,debugResourceNode);
  }
  pthread_mutex_unlock(&debugResourceLock);
}

void debugRemoveResourceTrace(const char *__fileName__,
                              uint       __lineNb__,
                              const void *resource
                             )
{
  DebugResourceNode *debugResourceNode;

  pthread_once(&debugResourceInitFlag,debugResourceInit);

  pthread_mutex_lock(&debugResourceLock);
  {
    // find in free-list to check for duplicate free
    debugResourceNode = debugResourceFreeList.head;
    while ((debugResourceNode != NULL) && (debugResourceNode->resource != resource))
    {
      debugResourceNode = debugResourceNode->next;
    }
    if (debugResourceNode != NULL)
    {
      fprintf(stderr,"DEBUG ERROR: multiple free of resource '%s' %p at %s, %u and previously at %s, %lu which was allocated at %s, %ld!\n",
              debugResourceNode->typeName,
              debugResourceNode->resource,
              __fileName__,
              __lineNb__,
              debugResourceNode->freeFileName,
              debugResourceNode->freeLineNb,
              debugResourceNode->allocFileName,
              debugResourceNode->allocLineNb
             );
      #ifdef HAVE_BACKTRACE
        debugDumpStackTrace(stderr,"allocated at",2,debugResourceNode->stackTrace,debugResourceNode->stackTraceSize);
        debugDumpStackTrace(stderr,"deleted at",2,debugResourceNode->deleteStackTrace,debugResourceNode->deleteStackTraceSize);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("");
    }

    // remove resource from allocated list, add resource to free-list, shorten list
    debugResourceNode = debugResourceAllocList.head;
    while ((debugResourceNode != NULL) && (debugResourceNode->resource != resource))
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
      fprintf(stderr,"DEBUG ERROR: resource '%p' not found in debug list at %s, line %u\n",
              resource,
              __fileName__,
              __lineNb__
             );
      #ifdef HAVE_BACKTRACE
        debugDumpCurrentStackTrace(stderr,"",0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("");
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
        fprintf(stderr,"DEBUG ERROR: resource '%s' %p invalid at %s, %u which was allocated at %s, %ld and freed at %s, %ld!\n",
                debugResourceNode->typeName,
                debugResourceNode->resource,
                __fileName__,
                __lineNb__,
                debugResourceNode->allocFileName,
                debugResourceNode->allocLineNb,
                debugResourceNode->freeFileName,
                debugResourceNode->freeLineNb
               );
        #ifdef HAVE_BACKTRACE
          debugDumpStackTrace(stderr,"allocated at",2,debugResourceNode->stackTrace,debugResourceNode->stackTraceSize);
          debugDumpStackTrace(stderr,"deleted at",2,debugResourceNode->deleteStackTrace,debugResourceNode->deleteStackTraceSize);
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
          debugDumpCurrentStackTrace(stderr,"",0);
        #endif /* HAVE_BACKTRACE */
      }
      HALT_INTERNAL_ERROR("");
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
      fprintf(handle,"DEBUG: resource '%s' %p allocated at %s, line %ld\n",
              debugResourceNode->typeName,
              debugResourceNode->resource,
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
        fprintf(stderr,"DEBUG: lost resource '%s' %p allocated at %s, line %ld\n",
                debugResourceNode->typeName,
                debugResourceNode->resource,
                debugResourceNode->allocFileName,
                debugResourceNode->allocLineNb
               );
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
