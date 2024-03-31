/***********************************************************************\
*
* Contents: global definitions
* Systems: Linux
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#if defined(HAVE_PCRE)
  #include <pcreposix.h>
#elif defined(HAVE_REGEX_H)
  #include <regex.h>
#else
  #warning No regular expression library available!
#endif /* HAVE_PCRE || HAVE_REGEX_H */
#ifdef HAVE_GCRYPT
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  #include <gcrypt.h>
  #pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif /* HAVE_GCRYPT */
#ifdef HAVE_EXECINFO_H
  #include <execinfo.h>
#endif
#ifdef HAVE_BFD_H
  #include "common/stacktraces.h"
#endif
#include <signal.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
  #include <unistd.h>
  #include <sys/syscall.h>
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#include "strings.h"
#ifndef NDEBUG
  #include "common/lists.h"
#endif /* not NDEBUG */

#include "errors.h"

#include "global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define MAX_SECURE_MEMORY (4*MB)

#define DEBUG_MAX_FREE_LIST 4000

#define DEBUG_TESTCODE_NAME          "TESTCODE"       // testcode to execute
#define DEBUG_TESTCODE_STOP          "TESTCODE_STOP"  // stop (breakpoint) if test fail

#define DEBUG_TESTCODE_LIST_FILENAME "TESTCODE_LIST"  // file with list with all testcode names
//#define DEBUG_TESTCODE_LIST_FILENAME "TESTCODE_RUN"   //
#define DEBUG_TESTCODE_SKIP_FILENAME "TESTCODE_SKIP"  // file with  testcodes to skip
#define DEBUG_TESTCODE_DONE_FILENAME "TESTCODE_DONE"  // file with  list with done testcode names

#define DEBUG_TESTCODE_NAME_FILENAME "TESTCODE_NAME"  // file with name of current testcode

/**************************** Datatypes ********************************/
#if !defined(NDEBUG) || !defined(HAVE_GCRYPT)
  typedef struct
  {
    size_t size;
  } MemoryHeader;
#endif /* !NDEBUG || !HAVE_GCRYPT */

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
    const char *variableName;
    const void *resource;
  } DebugResourceNode;

  typedef struct
  {
    LIST_HEADER(DebugResourceNode);
  } DebugResourceList;

  typedef struct
  {
    FILE                           *handle;
    uint                           indent;
    DebugDumpStackTraceOutputTypes type;
    uint                           skipFrameCount;
    uint                           count;
  } StackTraceOutputInfo;
#endif /* not NDEBUG */

/**************************** Variables ********************************/
#ifndef NDEBUG
  LOCAL pthread_once_t      debugResourceInitFlag = PTHREAD_ONCE_INIT;
  LOCAL pthread_mutexattr_t debugResourceLockAttributes;
  LOCAL pthread_mutex_t     debugResourceLock;
  LOCAL DebugResourceList   debugResourceAllocList;
  LOCAL DebugResourceList   debugResourceFreeList;
#endif /* not NDEBUG */

#ifndef NDEBUG
  pthread_mutex_t debugConsoleLock  = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  const char      *__testCodeName__ = NULL;

  LOCAL char      debugTestCodeName[256];
#endif /* not NDEBUG */

#ifdef i386
  LOCAL pthread_mutex_t syncLock = PTHREAD_MUTEX_INITIALIZER;
#endif /* i386 */

#ifndef NDEBUG
  LOCAL struct
        {
          DebugDumpStackTraceOutputTypes    type;
          DebugDumpStackTraceOutputFunction function;
          void                              *userData;
        } debugDumpStackTraceOutputHandlers[4];
  LOCAL uint debugDumpStackTraceOutputHandlerCount = 0;
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
  pthread_mutexattr_init(&debugResourceLockAttributes);
  pthread_mutexattr_settype(&debugResourceLockAttributes,PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&debugResourceLock,&debugResourceLockAttributes);
  List_init(&debugResourceAllocList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
  List_init(&debugResourceFreeList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
}
#endif /* not NDEBUG */

// ----------------------------------------------------------------------

unsigned long gcd(unsigned long a, unsigned long b)
{
  unsigned long tmp;

  while (a != 0L)
  {
    tmp = a;
    a = b%a;
    b = tmp;
  }

  return b;
}

unsigned long lcm(unsigned long a, unsigned long b)
{
  unsigned long n;

  n = gcd(a,b);

  return (n > 0) ? (b/n)*a : 0;
}

// ----------------------------------------------------------------------

Errors initSecure(void)
{
  #ifdef HAVE_GCRYPT
    // check version and do internal library init
    assert(GCRYPT_VERSION_NUMBER >= 0x010600);
    if (!gcry_check_version(GCRYPT_VERSION))
    {
      return ERRORX_(INIT_CRYPT,0,"Wrong gcrypt version (needed: %d)",GCRYPT_VERSION);
    }

    gcry_control(GCRYCTL_SUSPEND_SECMEM_WARN);
    gcry_control(GCRYCTL_INIT_SECMEM,MAX_SECURE_MEMORY,0);
    #ifdef NDEBUG
      gcry_control(GCRYCTL_RESUME_SECMEM_WARN);
    #endif

    gcry_control(GCRYCTL_INITIALIZATION_FINISHED,0);
  #endif /* HAVE_GCRYPT */

  return ERROR_NONE;
}

void doneSecure(void)
{
}

#ifdef NDEBUG
void *allocSecure(size_t size)
#else /* not NDEBUG */
void *__allocSecure(const char *__fileName__,
                    ulong      __lineNb__,
                    size_t     size
                   )
#endif /* NDEBUG */
{
  void *p;
  #if !defined(NDEBUG) || !defined(HAVE_GCRYPT)
    MemoryHeader *memoryHeader;
  #endif

  #ifdef HAVE_GCRYPT
    #ifndef NDEBUG
      memoryHeader = gcry_malloc_secure(sizeof(MemoryHeader)+size);
      if (memoryHeader == NULL)
      {
        return NULL;
      }
      memoryHeader->size = size;
      p = (byte*)memoryHeader+sizeof(MemoryHeader);
    #else
      p = gcry_malloc_secure(size);
      if (p == NULL)
      {
        return NULL;
      }
    #endif
    memset(p,0,size);
  #else /* not HAVE_GCRYPT */
    memoryHeader = (MemoryHeader*)calloc(1,sizeof(MemoryHeader)+size);
    if (memoryHeader == NULL)
    {
      return NULL;
    }
    memoryHeader->size = size;
    p = (byte*)memoryHeader+sizeof(MemoryHeader);
  #endif /* HAVE_GCRYPT */

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(p,MemoryHeader);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,p,MemoryHeader);
  #endif /* NDEBUG */

  return p;
}

#ifdef NDEBUG
void freeSecure(void *p)
#else /* not NDEBUG */
void __freeSecure(const char *__fileName__,
                  ulong      __lineNb__,
                  void       *p
                 )
#endif /* NDEBUG */
{
  #if !defined(NDEBUG) || !defined(HAVE_GCRYPT)
    MemoryHeader *memoryHeader;
  #endif

  assert(p != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(p,MemoryHeader);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,p,MemoryHeader);
  #endif /* NDEBUG */

  #ifdef HAVE_GCRYPT
    #ifndef NDEBUG
      memoryHeader = (MemoryHeader*)((byte*)p-sizeof(MemoryHeader));
      gcry_free(memoryHeader);
    #else
      gcry_free(p);
    #endif
  #else /* not HAVE_GCRYPT */
    memoryHeader = (MemoryHeader*)((byte*)p-sizeof(MemoryHeader));
    memset(memoryHeader,0,sizeof(MemoryHeader)+memoryHeader->size);
    free(memoryHeader);
  #endif /* HAVE_GCRYPT */
}

// ----------------------------------------------------------------------

#ifndef NDEBUG
void __dprintf__(const char *__fileName__,
                 ulong      __lineNb__,
                 const char *format,
                 ...
                )
{
  #if   defined(PLATFORM_LINUX)
    pid_t threadLWPId;
  #elif defined(PLATFORM_WINDOWS)
    pid_t threadLWPId;
  #endif /* PLATFORM_... */
  va_list arguments;

  #if   defined(PLATFORM_LINUX)
    #ifdef SYS_gettid
      threadLWPId = syscall(SYS_gettid);
    #else
      threadLWPId = 0;
    #endif
  #elif defined(PLATFORM_WINDOWS)
    threadLWPId = 0;
  #endif /* PLATFORM_... */

  fprintf(stdout,"DEBUG [%6u] %s, %lu: ",threadLWPId,__fileName__,__lineNb__);
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
            ulong      __lineNb__,
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
    fprintf(stderr," - halt in file %s, line %lu\n",__fileName__,__lineNb__);
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
             ulong       __lineNb__,
             const char *prefix,
             const char *format,
             ...
            )
#endif /* NDEBUG */
{
  va_list arguments;
  #ifdef HAVE_SIGACTION
    struct sigaction signalAction;
  #endif /* HAVE_SIGACTION */

  #ifndef NDEBUG
    assert(__fileName__ != NULL);
  #endif /* not NDEBUG */
  assert(format != NULL);

  if (prefix != NULL) fprintf(stderr,"%s", prefix);
  va_start(arguments,format);
  vfprintf(stderr,format,arguments);
  va_end(arguments);
  #ifndef NDEBUG
    fprintf(stderr," - program aborted in file %s, line %lu\n",__fileName__,__lineNb__);
  #else /* NDEBUG */
    fprintf(stderr," - program aborted\n");
  #endif /* not NDEBUG */

  // remove signal abort handler
  #ifdef HAVE_SIGACTION
    sigfillset(&signalAction.sa_mask);
    signalAction.sa_handler = SIG_DFL;
    signalAction.sa_flags   = 0;
    sigaction(SIGABRT,&signalAction,NULL);
  #else /* not HAVE_SIGACTION */
    signal(SIGABRT,SIG_DFL);
  #endif /* HAVE_SIGACTION */

  abort();
}
void __abortAt(const char *fileName,
               ulong      lineNb,
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
  fprintf(stderr," - program aborted in file %s, line %lu\n",fileName,lineNb);
  abort();
}

#ifdef i386
/***********************************************************************\
* Name   : __sync_add_and_fetch_4
* Purpose: atomic add+fetch 32bit
* Input  : -
* Output : -
* Return : -
* Notes  : Some older GCC versions do not implement
*          __sync_add_and_fetch() on 32bit. This is a replacement.
\***********************************************************************/

uint __sync_add_and_fetch_4(void *p, uint n)
{
  uint x;

  pthread_mutex_lock(&syncLock);
  {
    (*((int32_t*)p)) += n;
    x = (*(int32_t*)p);
  }
  pthread_mutex_unlock(&syncLock);

  return x;
}
#endif /* i386 */

#ifndef NDEBUG

#if 0
/* Linker flags

-Wl,-wrap,malloc -Wl,-wrap,calloc -Wl,-wrap,realloc

*/

#define ALLOC_LIMIT (1000L*1024L*1024L)

/***********************************************************************\
* Name   : __wrap_malloc
* Purpose: wrapper function for malloc()
* Input  : size - size
* Output : -
* Return : pointer to memory
* Notes  : -
\***********************************************************************/

void *__wrap_malloc(size_t size);
void *__wrap_malloc(size_t size)
{
  extern void * __real_malloc(size_t size) __attribute((weak));

  if (size > ALLOC_LIMIT)
  {
    #ifdef ARCHTECTURE_X86
      asm("int3");  // breakpoint ok: malloc trace
    #endif
  }

  return __real_malloc(size);
}

/***********************************************************************\
* Name   : __wrap_calloc
* Purpose: wrapper function for calloc()
* Input  : size - size
* Output : -
* Return : pointer to memory
* Notes  : -
\***********************************************************************/

void *__wrap_calloc(size_t nmemb, size_t size);
void *__wrap_calloc(size_t nmemb, size_t size)
{
  extern void * __real_calloc(size_t nmemb, size_t size) __attribute((weak));

  if (nmemb*size > ALLOC_LIMIT)
  {
    #ifdef ARCHTECTURE_X86
      asm("int3");  // breakpoint ok: calloc trace
    #endif
  }

  return __real_calloc(nmemb,size);
}

/***********************************************************************\
* Name   : __wrap_realloc
* Purpose: wrapper function for realloc()
* Input  : size - size
* Output : -
* Return : pointer to memory
* Notes  : -
\***********************************************************************/

void *__wrap_realloc(void *ptr, size_t size);
void *__wrap_realloc(void *ptr, size_t size)
{
  extern void * __real_realloc(void *ptr, size_t size) __attribute((weak));

  if (size > ALLOC_LIMIT)
  {
    #ifdef ARCHTECTURE_X86
      asm("int3");  // breakpoint ok: realloc trace
    #endif
  }

  return __real_realloc(ptr,size);
}
#endif

/***********************************************************************\
* Name   : __cyg_profile_func_enter
* Purpose: profile function
* Input  : functionCode - function address
*          callAddress  - call address
* Output : -
* Return : pointer to memory
* Notes  : -
\***********************************************************************/

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
                            ulong      __lineNb__,
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
          fprintf(file,"%s %s %lu\n",debugTestCodeName,__fileName__,__lineNb__);
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
      fprintf(stderr,"DEBUG: Execute testcode '%s', %s, line %lu\n",debugTestCodeName,__fileName__,__lineNb__);
    }

    // set testcode name
    __testCodeName__ = debugTestCodeName;
  }

  return isTestCodeEnabledFlag;
}

Errors debugTestCodeError(const char *__fileName__,
                          ulong      __lineNb__
                         )
{
  assert(__testCodeName__ != NULL);

  if (getenv(DEBUG_TESTCODE_STOP) != NULL)
  {
    __B();
  }

  return ERRORX_(TESTCODE,0,"'%s' at %s, %lu",__testCodeName__,__fileName__,__lineNb__);
}

void debugLocalResource(const char *__fileName__,
                        ulong      __lineNb__,
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
                           ulong      __lineNb__,
                           const char *typeName,
                           const char *variableName,
                           const void *resource
                          )
{
  DebugResourceNode *debugResourceNode;

  pthread_once(&debugResourceInitFlag,debugResourceInit);

  pthread_mutex_lock(&debugResourceLock);
  {
    // check for duplicate initialization in allocated list
    debugResourceNode = LIST_FIND(&debugResourceAllocList,
                                  debugResourceNode,
                                     (debugResourceNode->resource == resource)
                                  && stringEquals(debugResourceNode->typeName,typeName)
                                 );
    if (debugResourceNode != NULL)
    {
      fprintf(stderr,"DEBUG WARNING: multiple init of resource %s '%s' 0x%016"PRIxPTR" at %s, %lu which was previously initialized at %s, %ld!\n",
              typeName,
              variableName,
              (uintptr_t)resource,
              __fileName__,
              __lineNb__,
              debugResourceNode->allocFileName,
              debugResourceNode->allocLineNb
             );
      #ifdef HAVE_BACKTRACE
        debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("add resource trace fail");
    }

    // find resource in free-list; reuse or allocate new debug node
    debugResourceNode = LIST_FIND(&debugResourceFreeList,
                                  debugResourceNode,
                                     (debugResourceNode->resource == resource)
                                  && stringEquals(debugResourceNode->typeName,typeName)
                                 );
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
      debugResourceNode->stackTraceSize = getStackTrace(debugResourceNode->stackTrace,SIZE_OF_ARRAY(debugResourceNode->stackTrace));
    #endif /* HAVE_BACKTRACE */
    debugResourceNode->freeFileName  = NULL;
    debugResourceNode->freeLineNb    = 0L;
    #ifdef HAVE_BACKTRACE
      debugResourceNode->deleteStackTraceSize = 0;
    #endif /* HAVE_BACKTRACE */
    debugResourceNode->typeName     = typeName;
    debugResourceNode->variableName = variableName;
    debugResourceNode->resource     = resource;

    // add resource to allocated-list
    List_append(&debugResourceAllocList,debugResourceNode);
  }
  pthread_mutex_unlock(&debugResourceLock);
}

void debugRemoveResourceTrace(const char *__fileName__,
                              ulong      __lineNb__,
                              const char *typeName,
                              const char *variableName,
                              const void *resource
                             )
{
  DebugResourceNode *debugResourceNode;

  pthread_once(&debugResourceInitFlag,debugResourceInit);

  pthread_mutex_lock(&debugResourceLock);
  {
    // find in free-list to check for duplicate free
    debugResourceNode = LIST_FIND(&debugResourceFreeList,
                                  debugResourceNode,
                                     (debugResourceNode->resource == resource)
                                  && stringEquals(debugResourceNode->typeName,typeName)
                                 );
    if (debugResourceNode != NULL)
    {
      fprintf(stderr,"DEBUG ERROR: multiple free of resource %s '%s', 0x%016"PRIxPTR" at %s, %lu and previously at %s, %lu which was allocated at %s, %lu!\n",
              debugResourceNode->typeName,
              debugResourceNode->variableName,
              (uintptr_t)debugResourceNode->resource,
              __fileName__,
              __lineNb__,
              debugResourceNode->freeFileName,
              debugResourceNode->freeLineNb,
              debugResourceNode->allocFileName,
              debugResourceNode->allocLineNb
             );
      #ifdef HAVE_BACKTRACE
        fprintf(stderr,"  allocated at");
        debugDumpStackTrace(stderr,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugResourceNode->stackTrace,debugResourceNode->stackTraceSize,0);
        fprintf(stderr,"  deleted at");
        debugDumpStackTrace(stderr,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugResourceNode->deleteStackTrace,debugResourceNode->deleteStackTraceSize,0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("remove resource trace fail");
    }

    // remove resource from allocated list, add resource to free-list, shorten free-list
    debugResourceNode = LIST_FIND(&debugResourceAllocList,
                                  debugResourceNode,
                                     (debugResourceNode->resource == resource)
                                  && stringEquals(debugResourceNode->typeName,typeName)
                                 );
    if (debugResourceNode != NULL)
    {
      // remove from allocated list
      List_remove(&debugResourceAllocList,debugResourceNode);

      // add to free list
      debugResourceNode->freeFileName = __fileName__;
      debugResourceNode->freeLineNb   = __lineNb__;
      #ifdef HAVE_BACKTRACE
        debugResourceNode->deleteStackTraceSize = getStackTrace(debugResourceNode->deleteStackTrace,SIZE_OF_ARRAY(debugResourceNode->deleteStackTrace));
      #endif /* HAVE_BACKTRACE */
      List_append(&debugResourceFreeList,debugResourceNode);

      // shorten free list
      while (debugResourceFreeList.count > DEBUG_MAX_FREE_LIST)
      {
        debugResourceNode = (DebugResourceNode*)List_removeFirst(&debugResourceFreeList);
        LIST_DELETE_NODE(debugResourceNode);
      }
    }
    else
    {
      fprintf(stderr,"DEBUG ERROR: resource %s '%s', 0x%016"PRIxPTR" not found in debug list at %s, line %lu\n",
              typeName,
              variableName,
              (uintptr_t)resource,
              __fileName__,
              __lineNb__
             );
      #ifdef HAVE_BACKTRACE
        debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
      #endif /* HAVE_BACKTRACE */
      HALT_INTERNAL_ERROR("remove resource trace fail");
    }
  }
  pthread_mutex_unlock(&debugResourceLock);
}

void debugCheckResourceTrace(const char *__fileName__,
                             ulong      __lineNb__,
                             const char *variableName,
                             const void *resource
                            )
{
  DebugResourceNode *debugResourceNode;

  pthread_once(&debugResourceInitFlag,debugResourceInit);

  pthread_mutex_lock(&debugResourceLock);
  {
    // find in allocate-list
    debugResourceNode = LIST_FIND(&debugResourceAllocList,debugResourceNode,debugResourceNode->resource == resource);
    if (debugResourceNode == NULL)
    {
      // find in free-list to check if already freed
      debugResourceNode = LIST_FIND(&debugResourceFreeList,debugResourceNode,debugResourceNode->resource == resource);
      if (debugResourceNode != NULL)
      {
        fprintf(stderr,"DEBUG ERROR: resource %s '%s', 0x%016"PRIxPTR" invalid at %s, %lu which was allocated at %s, %lu and freed at %s, %lu!\n",
                debugResourceNode->typeName,
                debugResourceNode->variableName,
                (uintptr_t)debugResourceNode->resource,
                __fileName__,
                __lineNb__,
                debugResourceNode->allocFileName,
                debugResourceNode->allocLineNb,
                debugResourceNode->freeFileName,
                debugResourceNode->freeLineNb
               );
        #ifdef HAVE_BACKTRACE
          debugDumpCurrentStackTrace(stderr,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
          fprintf(stderr,"  allocated at");
          debugDumpStackTrace(stderr,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugResourceNode->stackTrace,debugResourceNode->stackTraceSize,0);
          fprintf(stderr,"  deleted at");
          debugDumpStackTrace(stderr,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugResourceNode->deleteStackTrace,debugResourceNode->deleteStackTraceSize,0);
        #endif /* HAVE_BACKTRACE */
      }
      else
      {
        fprintf(stderr,"DEBUG ERROR: resource '%s' 0x%016"PRIxPTR" not found in debug list at %s, line %lu\n",
                variableName,
                (uintptr_t)resource,
                __fileName__,
                __lineNb__
               );
        #ifdef HAVE_BACKTRACE
          debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
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
    List_done(&debugResourceAllocList);
    List_done(&debugResourceFreeList);
  }
  pthread_mutex_unlock(&debugResourceLock);
}

void debugResourceDumpInfo(FILE                     *handle,
                           ResourceDumpInfoFunction resourceDumpInfoFunction,
                           void                     *resourceDumpInfoUserData,
                           uint                     resourceDumpInfoTypes
                          )
{
  typedef struct ResourceHistogramNode
  {
    LIST_NODE_HEADER(struct ResourceHistogramNode);

    const DebugResourceNode *debugResourceNode;
    uint                    count;

  } ResourceHistogramNode;
  typedef struct
  {
    LIST_HEADER(ResourceHistogramNode);
  } ResourceHistogramList;

  /***********************************************************************\
  * Name   : compareResourceHistogramNodes
  * Purpose: compare histogram nodes
  * Input  : node1,node2 - resource histogram nodes to compare
  * Output : -
  * Return : -1 iff node1->count > node2->count
  *           1 iff node1->count < node2->count
  *           0 iff node1->count == node2->count
  * Notes  : -
  \***********************************************************************/

  auto int compareResourceHistogramNodes(const ResourceHistogramNode *node1, const ResourceHistogramNode *node2, void *userData);
  int compareResourceHistogramNodes(const ResourceHistogramNode *node1, const ResourceHistogramNode *node2, void *userData)
  {
    assert(node1 != NULL);
    assert(node2 != NULL);

    UNUSED_VARIABLE(userData);

    if      (node1->count > node2->count) return -1;
    else if (node1->count < node2->count) return  1;
    else                                  return  0;
  }

  ulong                 n;
  ulong                 count;
  DebugResourceNode     *debugResourceNode;
  ResourceHistogramList resourceHistogramList;
  ResourceHistogramNode *resourceHistogramNode;
  char                  s[34+1];

  pthread_once(&debugResourceInitFlag,debugResourceInit);

  pthread_mutex_lock(&debugResourceLock);
  {
    // init variables
    List_init(&resourceHistogramList,CALLBACK_(NULL,NULL),CALLBACK_(NULL,NULL));
    n     = 0L;
    count = 0L;

    // collect histogram data
    if (IS_SET(resourceDumpInfoTypes,DUMP_INFO_TYPE_HISTOGRAM))
    {
      LIST_ITERATE(&debugResourceAllocList,debugResourceNode)
      {
        resourceHistogramNode = LIST_FIND(&resourceHistogramList,
                                          resourceHistogramNode,
                                             (resourceHistogramNode->debugResourceNode->allocFileName == debugResourceNode->allocFileName)
                                          && (resourceHistogramNode->debugResourceNode->allocLineNb   == debugResourceNode->allocLineNb)
                                         );
        if (resourceHistogramNode == NULL)
        {
          resourceHistogramNode = LIST_NEW_NODE(ResourceHistogramNode);
          if (resourceHistogramNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          resourceHistogramNode->debugResourceNode = debugResourceNode;
          resourceHistogramNode->count             = 0;
          List_append(&resourceHistogramList,resourceHistogramNode);
        }

        resourceHistogramNode->count++;
      }

      List_sort(&resourceHistogramList,(ListNodeCompareFunction)CALLBACK_(compareResourceHistogramNodes,NULL));
    }

    // get count
    if (IS_SET(resourceDumpInfoTypes,DUMP_INFO_TYPE_ALLOCATED))
    {
      count += List_count(&debugResourceAllocList);
    }
    if (IS_SET(resourceDumpInfoTypes,DUMP_INFO_TYPE_HISTOGRAM))
    {
      count += List_count(&resourceHistogramList);
    }

    // dump allocations
    if (IS_SET(resourceDumpInfoTypes,DUMP_INFO_TYPE_ALLOCATED))
    {
      LIST_ITERATE(&debugResourceAllocList,debugResourceNode)
      {
        fprintf(handle,"DEBUG: resource '%s' 0x%016"PRIxPTR" allocated at %s, line %lu\n",
                debugResourceNode->variableName,
                (uintptr_t)debugResourceNode->resource,
                debugResourceNode->allocFileName,
                debugResourceNode->allocLineNb
               );
        #ifdef HAVE_BACKTRACE
          fprintf(handle,"  allocated at\n");
          debugDumpStackTrace(handle,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugResourceNode->stackTrace,debugResourceNode->stackTraceSize,0);
        #endif /* HAVE_BACKTRACE */

        if (resourceDumpInfoFunction != NULL)
        {
          if (!resourceDumpInfoFunction(debugResourceNode->variableName,
                                        debugResourceNode->resource,
                                        debugResourceNode->allocFileName,
                                        debugResourceNode->allocLineNb,
                                        n,
                                        count,
                                        resourceDumpInfoUserData
                                       )
             )
          {
            break;
          }
        }

        n++;
      }
    }

    // dump histogram
    if (IS_SET(resourceDumpInfoTypes,DUMP_INFO_TYPE_HISTOGRAM))
    {
      LIST_ITERATE(&resourceHistogramList,resourceHistogramNode)
      {
        stringSet(s,sizeof(s),"'");
        stringAppend(s,sizeof(s),resourceHistogramNode->debugResourceNode->variableName);
        stringAppend(s,sizeof(s),"'");
        fprintf(handle,"DEBUG: resource %-32s 0x%016"PRIxPTR" allocated %u times at %s, line %lu\n",
                s,
                (uintptr_t)resourceHistogramNode->debugResourceNode->resource,
                resourceHistogramNode->count,
                resourceHistogramNode->debugResourceNode->allocFileName,
                resourceHistogramNode->debugResourceNode->allocLineNb
               );
        #ifdef HAVE_BACKTRACE
          fprintf(handle,"  allocated at least at\n");
          debugDumpStackTrace(handle,4,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,resourceHistogramNode->debugResourceNode->stackTrace,resourceHistogramNode->debugResourceNode->stackTraceSize,0);
        #endif /* HAVE_BACKTRACE */

        if (resourceDumpInfoFunction != NULL)
        {
          if (!resourceDumpInfoFunction(resourceHistogramNode->debugResourceNode->variableName,
                                        resourceHistogramNode->debugResourceNode->resource,
                                        resourceHistogramNode->debugResourceNode->allocFileName,
                                        resourceHistogramNode->debugResourceNode->allocLineNb,
                                        n,
                                        count,
                                        resourceDumpInfoUserData
                                       )
             )
          {
            break;
          }
        }

        n++;
      }
    }

    // free resources
    if (IS_SET(resourceDumpInfoTypes,DUMP_INFO_TYPE_HISTOGRAM))
    {
      List_done(&resourceHistogramList);
    }
  }
  pthread_mutex_unlock(&debugResourceLock);
}

void debugResourcePrintInfo(ResourceDumpInfoFunction resourceDumpInfoFunction,
                            void                     *resourceDumpInfoUserData,
                            uint                     resourceDumpInfoTypes
                           )
{
  debugResourceDumpInfo(stderr,resourceDumpInfoFunction,resourceDumpInfoUserData,resourceDumpInfoTypes);
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

void debugResourcePrintHistogram(void)
{
  debugResourceDumpInfo(stderr,CALLBACK_(NULL,NUL),DUMP_INFO_TYPE_HISTOGRAM);
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
        fprintf(stderr,"DEBUG: lost resource %s '%s' 0x%016"PRIxPTR" allocated at %s, line %lu\n",
                debugResourceNode->typeName,
                debugResourceNode->variableName,
                (uintptr_t)debugResourceNode->resource,
                debugResourceNode->allocFileName,
                debugResourceNode->allocLineNb
               );
        #ifdef HAVE_BACKTRACE
          debugDumpStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,debugResourceNode->stackTrace,debugResourceNode->stackTraceSize,0);
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

#ifdef HAVE_BFD_INIT
LOCAL void debugDumpStackTraceOutputSymbol(const void *address,
                                           const char *fileName,
                                           const char *symbolName,
                                           ulong      lineNb,
                                           void       *userData
                                          )
{
  StackTraceOutputInfo *stackTraceOutputInfo = (StackTraceOutputInfo*)userData;
  assert(stackTraceOutputInfo != NULL);

  // skip at least first one stack frame: this function
  if (stackTraceOutputInfo->count > stackTraceOutputInfo->skipFrameCount)
  {
    if (fileName   == NULL) fileName   = "<unknown file>";
    if (symbolName == NULL) symbolName = "<unknown symbol>";
    debugDumpStackTraceOutput(stackTraceOutputInfo->handle,
                              stackTraceOutputInfo->indent,
                              stackTraceOutputInfo->type,
                              "  [0x%016"PRIxPTR"] %s (%s:%lu)\n",
                              (uintptr_t)address,
                              symbolName,
                              fileName,
                              lineNb
                             );
  }
  stackTraceOutputInfo->count++;
}
#endif /* HAVE_BFD_INIT */

void debugDumpStackTraceAddOutput(DebugDumpStackTraceOutputTypes    type,
                                  DebugDumpStackTraceOutputFunction function,
                                  void                              *userData
                                 )
{
  assert(debugDumpStackTraceOutputHandlerCount < SIZE_OF_ARRAY(debugDumpStackTraceOutputHandlers));

  debugDumpStackTraceOutputHandlers[debugDumpStackTraceOutputHandlerCount].type     = type;
  debugDumpStackTraceOutputHandlers[debugDumpStackTraceOutputHandlerCount].function = function;
  debugDumpStackTraceOutputHandlers[debugDumpStackTraceOutputHandlerCount].userData = userData;
  debugDumpStackTraceOutputHandlerCount++;
}

void debugDumpStackTraceOutput(FILE                           *handle,
                               uint                           indent,
                               DebugDumpStackTraceOutputTypes type,
                               const char                     *format,
                               ...
                              )
{
  static va_list arguments;
  static uint    i;
  static char    buffer[1024];
  static uint    n;

  assert(indent < sizeof(buffer));
  assert(format != NULL);

  // get indention
  memset(buffer,' ',indent);

  // format string
  va_start(arguments,format);
  n = indent+(uint)vsnprintf(&buffer[indent],sizeof(buffer)-indent,format,arguments);
  va_end(arguments);

  // output
  fwrite(buffer,n,1,handle);
  for (i = 0; i < debugDumpStackTraceOutputHandlerCount; i++)
  {
    if (type >= debugDumpStackTraceOutputHandlers[i].type)
    {
      debugDumpStackTraceOutputHandlers[i].function(buffer,debugDumpStackTraceOutputHandlers[i].userData);
    }
  }
}

void debugDumpStackTrace(FILE                           *handle,
                         uint                           indent,
                         DebugDumpStackTraceOutputTypes type,
                         void const * const             stackTrace[],
                         uint                           stackTraceSize,
                         uint                           skipFrameCount
                        )
{
  #ifdef HAVE_BFD_INIT
    char                 executableName[PATH_MAX];
    ssize_t              n;
    StackTraceOutputInfo stackTraceOutputInfo;
  #elif HAVE_BACKTRACE_SYMBOLS
    const char **functionNames;
    uint       i;
  #else /* not HAVE_... */
  #endif /* HAVE_... */

  assert(handle != NULL);
  assert(stackTrace != NULL);

  #ifdef HAVE_BFD_INIT
    // get executable name
    n = readlink("/proc/self/exe",executableName,sizeof(executableName)-1);
    if ((n == -1) || ((size_t)n >= sizeof(executableName)))
    {
      return;
    }
    executableName[n] = '\0';

    // output stack trace
    stackTraceOutputInfo.handle         = handle;
    stackTraceOutputInfo.indent         = indent;
    stackTraceOutputInfo.type           = type;
    stackTraceOutputInfo.skipFrameCount = skipFrameCount;
    stackTraceOutputInfo.count          = 0;
    Stacktrace_getSymbolInfo(executableName,
                             stackTrace,
                             stackTraceSize,
                             debugDumpStackTraceOutputSymbol,
                             &stackTraceOutputInfo,
                             FALSE  // printErrorMessagesFlag
                            );
  #elif HAVE_BACKTRACE_SYMBOLS
    // get function names
    functionNames = (const char **)backtrace_symbols((void *const*)stackTrace,stackTraceSize);
    if (functionNames == NULL)
    {
      return;
    }

    // output stack trace
    for (i = 1+skipFrameCount; i < stackTraceSize; i++)
    {
      debugDumpStackTraceOutput(handle,indent,type,"  %2d 0x%016"PRIxPTR": %s\n",i,(uintptr_t)stackTrace[i],functionNames[i]);
    }
    free(functionNames);
  #else /* not HAVE_... */
    UNUSED_VARIABLE(stackTraceSize);
    UNUSED_VARIABLE(skipFrameCount);

    debugDumpStackTraceOutput(handle,indent,type,"  not available\n");
  #endif /* HAVE_... */
}

void debugDumpCurrentStackTrace(FILE                           *handle,
                                uint                           indent,
                                DebugDumpStackTraceOutputTypes type,
                                uint                           skipFrameCount
                               )
{
  #if defined(HAVE_BACKTRACE)
    const int MAX_STACK_TRACE_SIZE = 256;

    void const** currentStackTrace;
    int          currentStackTraceSize;
  #else /* not defined(HAVE_BACKTRACE) */
    uint i;
  #endif /* defined(HAVE_BACKTRACE) */

  assert(handle != NULL);

  #if defined(HAVE_BACKTRACE)
    currentStackTrace = (void const**)malloc(sizeof(void*)*MAX_STACK_TRACE_SIZE);
    if (currentStackTrace == NULL) return;

    currentStackTraceSize = getStackTrace(currentStackTrace,MAX_STACK_TRACE_SIZE);
    debugDumpStackTrace(handle,indent,type,currentStackTrace,currentStackTraceSize,1+skipFrameCount);

    free(currentStackTrace);
  #else /* not defined(HAVE_BACKTRACE) */
    UNUSED_VARIABLE(skipFrameCount);

    for (i = 0; i < indent; i++) fputc(' ',handle);
    debugDumpStackTraceOutput(handle,indent,type,"  not available\n");
  #endif /* defined(HAVE_BACKTRACE) */
}

void debugPrintStackTrace(void)
{
  debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,0);
}

void debugDumpMemory(const void *address, uint length, bool printAddress)
{
  const byte *p;
  uint       z,j;

  assert(address != NULL);

  z = 0;
  while (z < length)
  {
    p = (const byte*)address+z;
    if (printAddress) fprintf(stderr,"%08lx:",(unsigned long)p);
    fprintf(stderr,"%08lx  ",(unsigned long)(p-(byte*)address));

    for (j = 0; j < 16; j++)
    {
      if ((z+j) < length)
      {
        p = (const byte*)address+z+j;
        fprintf(stderr,"%02x ",((uint)(*p)) & 0xFF);
      }
      else
      {
        fprintf(stderr,"   ");
      }
    }
    fprintf(stderr,"  ");

    for (j = 0; j < 16; j++)
    {
      if ((z+j) < length)
      {
        p = (const byte*)address+z+j;
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
