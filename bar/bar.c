/***********************************************************************\
*
* Contents: Backup ARchiver main program
* Systems: all
*
\***********************************************************************/

#define __BAR_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#ifdef HAVE_SYS_RESOURCE_H
  #include <sys/resource.h>
#endif
#ifdef HAVE_LIBINTL_H
  #include <libintl.h>
#endif
#include <signal.h>
#include <errno.h>
#include <locale.h>
#include <assert.h>

#if   defined(PLATFORM_LINUX)
#include <sys/utsname.h>
#elif defined(PLATFORM_WINDOWS)
#include <winsock2.h>  // Windows brain dead
#include <windows.h>
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/cstrings.h"
#include "common/autofree.h"
#include "common/cmdoptions.h"
#include "common/configvalues.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/arrays.h"
#include "common/threads.h"
#include "common/threadpools.h"
#include "common/files.h"
#include "common/patternlists.h"
#include "common/network.h"
#include "common/database.h"
#include "common/misc.h"
#include "common/passwords.h"

#include "errors.h"
#include "configuration.h"
#include "entrylists.h"
#include "deltasourcelists.h"
#include "compress.h"
#include "crypt.h"
#include "archive.h"
#include "storage.h"
#include "deltasources.h"
#include "index/index.h"
#include "index/index_storages.h"
#include "continuous.h"
#ifdef HAVE_BREAKPAD
  #include "minidump.h"
#endif /* HAVE_BREAKPAD */

#include "bar_common.h"
#include "configuration.h"
#include "commands_create.h"
#include "commands_list.h"
#include "commands_restore.h"
#include "commands_test.h"
#include "commands_compare.h"
#include "commands_convert.h"
#include "jobs.h"
#include "server.h"

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define MOUNT_TIMEOUT (1L*60L*MS_PER_SECOND)  // mount timeout [ms]

/***************************** Datatypes *******************************/

// mounted list
typedef struct MountedNode
{
  LIST_NODE_HEADER(struct MountedNode);

  String name;                                                // mount point
  String device;                                              // mount device (optional)
  uint   mountCount;                                          // mount count (unmount iff 0)
  uint64 lastMountTimestamp;
} MountedNode;

typedef struct
{
  LIST_HEADER(MountedNode);

  Semaphore lock;
} MountedList;

/***************************** Variables *******************************/
Semaphore  consoleLock;
#ifdef HAVE_NEWLOCALE
  locale_t POSIXLocale;
#endif /* HAVE_NEWLOCALE */

ThreadPool clientThreadPool;
ThreadPool workerThreadPool;

LOCAL ThreadLocalStorage outputLineHandle;
LOCAL String             lastOutputLine;

/*---------------------------------------------------------------------*/

LOCAL MountedList        mountedList;         // list of current mounts

LOCAL Semaphore          logLock;
LOCAL FILE               *logFile = NULL;     // log file handle

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

LOCAL void deletePIDFile(void);
LOCAL void doneAll(void);

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : openLog
* Purpose: open global log file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void openLog(void)
{
  SEMAPHORE_LOCKED_DO(&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (!String_isEmpty(globalOptions.logFileName))
    {
      logFile = fopen(String_cString(globalOptions.logFileName),"a");
      if (logFile == NULL) printWarning(_("cannot open log file '%s' (error: %s)!"),String_cString(globalOptions.logFileName),strerror(errno));
    }
  }
}

/***********************************************************************\
* Name   : closeLog
* Purpose: close global log file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void closeLog(void)
{
  SEMAPHORE_LOCKED_DO(&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (logFile != NULL)
    {
      fclose(logFile);
      logFile = NULL;
    }
  }
}

/***********************************************************************\
* Name   : reopenLog
* Purpose: re-open log file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void reopenLog(void)
{
  SEMAPHORE_LOCKED_DO(&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (!String_isEmpty(globalOptions.logFileName))
    {
      fclose(logFile);
      logFile = fopen(String_cString(globalOptions.logFileName),"a");
      if (logFile == NULL) printWarning(_("cannot re-open log file '%s' (error: %s)!"),String_cString(globalOptions.logFileName),strerror(errno));
    }
  }
}

/***********************************************************************\
* Name   : signalHandler
* Purpose: general signal handler
* Input  : signalNumber - signal number
*          siginfo      - signal info
*          context      - context
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef HAVE_SIGACTION
LOCAL void signalHandler(int signalNumber, siginfo_t *siginfo, void *context)
#else /* not HAVE_SIGACTION */
LOCAL void signalHandler(int signalNumber)
#endif /* HAVE_SIGACTION */
{
  #ifdef HAVE_SIGACTION
    struct sigaction signalAction;
  #endif /* HAVE_SIGACTION */

  #ifdef HAVE_SIGINFO_T
    UNUSED_VARIABLE(siginfo);
    UNUSED_VARIABLE(context);
  #endif /* HAVE_SIGINFO_T */

  // reopen log file
  #ifdef HAVE_SIGUSR1
    if (signalNumber == SIGUSR1)
    {
      reopenLog();
      return;
    }
  #endif /* HAVE_SIGUSR1 */

  // deinstall signal handlers
  #ifdef HAVE_SIGACTION
    sigfillset(&signalAction.sa_mask);
    signalAction.sa_handler = SIG_DFL;
    signalAction.sa_flags   = 0;
    sigaction(SIGTERM,&signalAction,NULL);
    sigaction(SIGSEGV,&signalAction,NULL);
    #ifdef HAVE_SIGUSR1
      sigaction(SIGUSR1,&signalAction,NULL);
    #endif
    sigaction(SIGFPE,&signalAction,NULL);
    #ifdef HAVE_SIGBUS
      sigaction(SIGBUS,&signalAction,NULL);
    #endif /* HAVE_SIGBUS */
    sigaction(SIGILL,&signalAction,NULL);
  #else /* not HAVE_SIGACTION */
    signal(SIGTERM,SIG_DFL);
    signal(SIGSEGV,SIG_DFL);
    #ifdef HAVE_SIGUSR1
      signal(SIGUSR1,SIG_DFL);
    #endif
    signal(SIGFPE,SIG_DFL);
    #ifdef HAVE_SIGBUS
      signal(SIGBUS,SIG_DFL);
    #endif /* HAVE_SIGBUS */
    signal(SIGILL,SIG_DFL);
  #endif /* HAVE_SIGACTION */

  // output error message
  if (signalNumber != SIGTERM)
  {
    static char    line[256];
    #if   defined(PLATFORM_LINUX)
    #elif defined(PLATFORM_WINDOWS)
    #endif /* PLATFORM_... */

    UNUSED_RESULT(fprintf(stderr,"FATAL ERROR:\n"));

    stringFormat(line,sizeof(line),"BAR version %s%s\n",
                 VERSION_STRING,
                 #ifndef NDEBUG
                   " debug"
                 #else
                   ""
                 #endif
                );
    UNUSED_RESULT(fwrite(line,1,stringLength(line),stderr));
    fatalLogMessage(line,NULL);

    stringClear(line);
    #if   defined(PLATFORM_LINUX)
      struct utsname utsname;
      uname(&utsname);
      stringFormat(line,sizeof(line),
                   "%s %s %s %s\n",
                   utsname.sysname,
                   utsname.release,
                   utsname.version,
                   utsname.machine
                  );
    #elif defined(PLATFORM_WINDOWS)
      String value = String_new();
      Misc_getRegistryString(value,HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion","ProductName");
      stringFormat(line,sizeof(line),
                   "%s",
                   String_cString(value)
                  );
      if (Misc_getRegistryString(value,HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion","ReleaseId"))
      {
        stringAppend(line,sizeof(line),", release ");
        stringAppendFormat(line,sizeof(line),
                           String_cString(value)
                          );
      }
      if (Misc_getRegistryString(value,HKEY_LOCAL_MACHINE,"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion","CurrentBuildNumber"))
      {
        stringAppend(line,sizeof(line),", build ");
        stringAppendFormat(line,sizeof(line),
                           String_cString(value)
                          );
      }
      stringAppendChar(line,sizeof(line),'\n');
      String_delete(value);
    #endif /* PLATFORM_... */
    UNUSED_RESULT(fwrite(line,1,stringLength(line),stderr));
    fatalLogMessage(line,NULL);

    stringFormat(line,sizeof(line),"Signal %d\n",signalNumber);
    UNUSED_RESULT(fwrite(line,1,stringLength(line),stderr));
    fatalLogMessage(line,NULL);

    #ifndef NDEBUG
      UNUSED_RESULT(fprintf(stderr,"Stack trace:\n"));
      fatalLogMessage("Stack trace:\n",NULL);
      debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_FATAL,1);
    #endif /* NDEBUG */
  }

  // delete pid file
  deletePIDFile();

  // delete temporary directory (Note: do a simple validity check in case something serious went wrong...)
  if (!String_isEmpty(tmpDirectory) && !String_equalsCString(tmpDirectory,"/"))
  {
    (void)File_delete(tmpDirectory,TRUE);
  }

  // Note: do not free resources to avoid further errors

  // exit with signal number or trigger signal
  #if !defined(NDEBUG) || !defined(HAVE_KILL)
    exit(128+signalNumber);
  #else
    kill(0,signalNumber);
  #endif
}

/***********************************************************************\
* Name   : outputLineInit
* Purpose: init output line variable instance callback
* Input  : userData - user data (not used)
* Output : -
* Return : output line variable instance
* Notes  : -
\***********************************************************************/

LOCAL void *outputLineInit(void *userData)
{
  UNUSED_VARIABLE(userData);

  return String_new();
}

/***********************************************************************\
* Name   : outputLineDone
* Purpose: done output line variable instance callback
* Input  : variable - output line variable instance
*          userData - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void outputLineDone(void *variable, void *userData)
{
  assert(variable != NULL);

  UNUSED_VARIABLE(userData);

  String_delete((String)variable);
}

/***********************************************************************\
* Name   : freeMountedNode
* Purpose: free mounted node
* Input  : mountedNode - mounted node
*          userData    - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeMountedNode(MountedNode *mountedNode, void *userData)
{
  assert(mountedNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(mountedNode->device);
  String_delete(mountedNode->name);
}

/***********************************************************************\
* Name       : printUsage
* Purpose    : print "usage" help
* Input      : programName - program name
*              level       - help level (0..1)
* Output     : -
* Return     : -
* Side-effect: unknown
* Notes      : -
\***********************************************************************/

LOCAL void printUsage(const char *programName, uint level)
{
  assert(programName != NULL);

  printf("Usage: %s [<options>] [--] <archive name> [<files>|<device>...]\n",programName);
  printf("       %s [<options>] --generate-keys|--generate-signature-keys [--] [<key file base name>]\n",programName);
  printf("\n");
  printf("Archive name:  <file name>\n");
  printf("               file://<file name>\n");
  printf("               ftp://[<login name>[:<password>]@]<host name>/<file name>\n");
  printf("               scp://[<login name>[:<password>]@]<host name>[:<port>]/<file name>\n");
  printf("               sftp://[<login name>[:<password>]@]<host name>[:<port>]/<file name>\n");
  printf("               webdav://[<login name>[:<password>]@]<host name>/<file name>\n");
  printf("               webdavs://[<login name>[:<password>]@]<host name>/<file name>\n");
  printf("               smb://[<login name>[:<password>]@]<host name>[:share>]/<file name>\n");
  printf("               cd://[<device name>:]<file name>\n");
  printf("               dvd://[<device name>:]<file name>\n");
  printf("               bd://[<device name>:]<file name>\n");
  printf("               device://[<device name>:]<file name>\n");
  printf("\n");
  CmdOption_printHelp(stdout,
                      BAR_COMMAND_LINE_OPTIONS,
                      level
                     );
}

/***********************************************************************\
* Name   : initAll
* Purpose: initialize
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors initAll(void)
{
  #ifdef HAVE_SIGACTION
    struct sigaction signalAction;
  #endif /* HAVE_SIGACTION */
  AutoFreeList     autoFreeList;
  Errors           error;
  #if defined(HAVE_SETLOCALE) && defined(HAVE_BINDTEXTDOMAIN) && defined(HAVE_TEXTDOMAIN)
    const char       *localePath;
  #endif /* defined(HAVE_SETLOCALE) && defined(HAVE_BINDTEXTDOMAIN) && defined(HAVE_TEXTDOMAIN) */

  // initialize fatal log handler, crash dump handler
  #ifndef NDEBUG
    debugDumpStackTraceAddOutput(DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_FATAL,fatalLogMessage,NULL);
  #endif /* not NDEBUG */
  #ifdef HAVE_BREAKPAD
    if (!MiniDump_init())
    {
      UNUSED_RESULT(fprintf(stderr,"Warning: Cannot initialize crash dump handler. No crash dumps will be created.\n"));
    }
  #endif /* HAVE_BREAKPAD */

  // install signal handlers
  #ifdef HAVE_SIGACTION
    sigfillset(&signalAction.sa_mask);
    signalAction.sa_flags     = SA_SIGINFO;
    signalAction.sa_sigaction = signalHandler;
    #ifdef HAVE_SIGUSR1
      sigaction(SIGUSR1,&signalAction,NULL);
    #endif

    sigaction(SIGSEGV,&signalAction,NULL);
    sigaction(SIGFPE,&signalAction,NULL);
    sigaction(SIGILL,&signalAction,NULL);
    sigaction(SIGTERM,&signalAction,NULL);
    #ifdef HAVE_SIGBUS
      sigaction(SIGBUS,&signalAction,NULL);
    #endif /* HAVE_SIGBUS */
  #else /* not HAVE_SIGACTION */
    #ifdef HAVE_SIGUSR1
      signal(SIGUSR1,signalHandler);
    #endif
    signal(SIGSEGV,signalHandler);
    signal(SIGFPE,signalHandler);
    signal(SIGILL,signalHandler);
    signal(SIGTERM,signalHandler);
    #ifdef HAVE_SIGBUS
      signal(SIGBUS,signalHandler);
    #endif /* HAVE_SIGBUS */
  #endif /* HAVE_SIGACTION */

  AutoFree_init(&autoFreeList);

  // init secure memory
  error = initSecure();
  if (error != ERROR_NONE)
  {
    return error;
  }
  DEBUG_TESTCODE() { doneSecure(); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,initSecure,{ doneSecure(); });

  // initialize variables
  Semaphore_init(&consoleLock,SEMAPHORE_TYPE_BINARY);
  DEBUG_TESTCODE() { Semaphore_done(&consoleLock); return DEBUG_TESTCODE_ERROR(); }
  #ifdef HAVE_NEWLOCALE
    POSIXLocale = newlocale(LC_ALL,"POSIX",0);
  #endif /* HAVE_NEWLOCALE */

  Semaphore_init(&mountedList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&mountedList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeMountedNode,NULL));

  Semaphore_init(&logLock,SEMAPHORE_TYPE_BINARY);
  logFile                                = NULL;

  Thread_initLocalVariable(&outputLineHandle,outputLineInit,NULL);
  lastOutputLine                         = NULL;

  AUTOFREE_ADD(&autoFreeList,&consoleLock,{ Semaphore_done(&consoleLock); });
  AUTOFREE_ADD(&autoFreeList,&mountedList,{ List_done(&mountedList); });
  AUTOFREE_ADD(&autoFreeList,&mountedList.lock,{ Semaphore_done(&mountedList.lock); });
  AUTOFREE_ADD(&autoFreeList,&logLock,{ Semaphore_done(&logLock); });
  AUTOFREE_ADD(&autoFreeList,&outputLineHandle,{ Thread_doneLocalVariable(&outputLineHandle,outputLineDone,NULL); });

  // initialize i18n
  #if defined(HAVE_SETLOCALE) && defined(HAVE_BINDTEXTDOMAIN) && defined(HAVE_TEXTDOMAIN)
    setlocale(LC_ALL,"");
    #ifdef HAVE_BINDTEXTDOMAIN
      localePath = getenv("__BAR_LOCALE__");
      if (localePath != NULL)
      {
        bindtextdomain("bar",localePath);
      }
    #endif /* HAVE_BINDTEXTDOMAIN */
    textdomain("bar");
  #endif /* HAVE_SETLOCAL && HAVE_TEXTDOMAIN */

  // initialize modules
  error = Common_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Common_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Common_initAll,{ Common_doneAll(); });

  error = Configuration_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Configuration_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Configuration_initAll,{ Configuration_doneAll(); });

  error = Thread_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Thread_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Thread_initAll,{ Thread_doneAll(); });

  error = ThreadPool_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { ThreadPool_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Thread_initAll,{ ThreadPool_doneAll(); });

  error = Password_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Password_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Password_initAll,{ Password_doneAll(); });

  error = Compress_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Compress_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Compress_initAll,{ Compress_doneAll(); });

  error = Crypt_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Crypt_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Crypt_initAll,{ Crypt_doneAll(); });

  error = EntryList_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { EntryList_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,EntryList_initAll,{ EntryList_doneAll(); });

  error = Pattern_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Pattern_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Pattern_initAll,{ Password_doneAll(); });

  error = PatternList_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { PatternList_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,PatternList_initAll,{ PatternList_doneAll(); });

  error = Chunk_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Chunk_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Chunk_initAll,{ Chunk_doneAll(); });

  error = DeltaSource_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { DeltaSource_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,DeltaSource_initAll,{ DeltaSource_doneAll(); });

  error = Archive_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Archive_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Archive_initAll,{ Archive_doneAll(); });

  error = Storage_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Storage_doneAll(), AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Storage_initAll,{ Storage_doneAll(); });

  error = Index_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Index_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Index_initAll,{ Index_doneAll(); });

  (void)Continuous_initAll();
  DEBUG_TESTCODE() { Continuous_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Continuous_initAll,{ Continuous_doneAll(); });

  error = Network_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Network_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Network_initAll,{ Network_doneAll(); });

  error = Job_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Job_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Job_initAll,{ Job_doneAll(); });

  error = Server_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Server_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Server_initAll,{ Server_doneAll(); });

  // initialize config values and command line options
  ConfigValue_init(BAR_CONFIG_VALUES);
  CmdOption_init(BAR_COMMAND_LINE_OPTIONS);

  // done resources
  AutoFree_done(&autoFreeList);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : doneAll
* Purpose: deinitialize
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void doneAll(void)
{
  #ifdef HAVE_SIGACTION
    struct sigaction signalAction;
  #endif /* HAVE_SIGACTION */

  // deinitialize command line options and config values
  CmdOption_done(BAR_COMMAND_LINE_OPTIONS);
  ConfigValue_done(BAR_CONFIG_VALUES);

  // deinitialize modules
  Server_doneAll();
  Job_doneAll();
  Network_doneAll();
  Continuous_doneAll();
  Index_doneAll();
  Storage_doneAll();
  Archive_doneAll();
  DeltaSource_doneAll();
  Chunk_doneAll();
  PatternList_doneAll();
  Pattern_doneAll();
  EntryList_doneAll();
  Crypt_doneAll();
  Compress_doneAll();
  Password_doneAll();
  ThreadPool_doneAll();
  Thread_doneAll();
  Configuration_doneAll();
  Common_doneAll();

  Thread_doneLocalVariable(&outputLineHandle,outputLineDone,NULL);

  // deinitialize variables
  #ifdef HAVE_NEWLOCALE
    freelocale(POSIXLocale);
  #endif /* HAVE_NEWLOCALE */
  Semaphore_done(&logLock);

  List_done(&mountedList);
  Semaphore_done(&mountedList.lock);

  Semaphore_done(&consoleLock);

  // done secure memory
  doneSecure();

  #ifndef NDEBUG
    // done debug
    File_debugDone();
    RingBuffer_debugDone();
    Array_debugDone();
    String_debugDone();
    debugResourceDone();
    List_debugDone();
 #endif

  // deinstall signal handlers
  #ifdef HAVE_SIGACTION
    sigfillset(&signalAction.sa_mask);
    signalAction.sa_handler = SIG_DFL;
    signalAction.sa_flags   = 0;
    sigaction(SIGTERM,&signalAction,NULL);
    sigaction(SIGSEGV,&signalAction,NULL);
    #ifdef HAVE_SIGUSR1
      sigaction(SIGUSR1,&signalAction,NULL);
    #endif
    sigaction(SIGFPE,&signalAction,NULL);
    #ifdef HAVE_SIGBUS
      sigaction(SIGBUS,&signalAction,NULL);
    #endif /* HAVE_SIGBUS */
    sigaction(SIGILL,&signalAction,NULL);
  #else /* not HAVE_SIGACTION */
    signal(SIGTERM,SIG_DFL);
    signal(SIGSEGV,SIG_DFL);
    #ifdef HAVE_SIGUSR1
      signal(SIGUSR1,SIG_DFL);
    #endif
    signal(SIGFPE,SIG_DFL);
    #ifdef HAVE_SIGBUS
      signal(SIGBUS,SIG_DFL);
    #endif /* HAVE_SIGBUS */
    signal(SIGILL,SIG_DFL);
  #endif /* HAVE_SIGACTION */

  // deinitialize crash dump handler
  #ifdef HAVE_BREAKPAD
    MiniDump_done();
  #endif /* HAVE_BREAKPAD */
}

/***********************************************************************\
* Name   : vprintInfo
* Purpose: output info to console
* Input  : verboseLevel - verbosity level
*          prefix       - prefix text
*          format       - format string (like printf)
*          arguments    - arguments
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void vprintInfo(uint verboseLevel, const char *prefix, const char *format, va_list arguments)
{
  String line;

  assert(format != NULL);

  if (isPrintInfo(verboseLevel))
  {
    line = String_new();

    // format line
    if (prefix != NULL) String_appendCString(line,prefix);
    String_appendVFormat(line,format,arguments);

    // output
    SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      outputConsole(stdout,line);
    }

    String_delete(line);
  }
}

/***********************************************************************\
* Name   : outputConsole
* Purpose: output string to console
* Input  : file   - output stream (stdout, stderr)
*          string - string
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void outputConsole(FILE *file, ConstString string)
{
  String outputLine;
  ulong  i;
  long   j;

  assert(file != NULL);
  assert(Semaphore_isLocked(&consoleLock));

  outputLine = (String)Thread_getLocalVariable(&outputLineHandle);
  if (outputLine != NULL)
  {
    if (File_isTerminal(file) || globalOptions.forceConsoleEncodingFlag)
    {
      // wipe out if new output line is different to last line
      if (outputLine != lastOutputLine)
      {
        // wipe-out last line
        if (lastOutputLine != NULL)
        {
          // get visible line output length
          uint           n = 0;
          StringIterator stringIterator;
          char           ch;
          STRING_CHAR_ITERATE(lastOutputLine,stringIterator,ch)
          {
            if (ch != '\b')
            {
              n++;
            }
            else
            {
              assert(n > 0);
              n--;
            }
          }

          // wipe out old line
          for (i = 0; i < n; i++)
          {
            UNUSED_RESULT(fwrite("\b",1,1,file));
          }
          for (i = 0; i < n; i++)
          {
            UNUSED_RESULT(fwrite(" ",1,1,file));
          }
          for (i = 0; i < n; i++)
          {
            UNUSED_RESULT(fwrite("\b",1,1,file));
          }
        }

        // get new output line
        i = 0;
        convertSystemToConsoleEncodingAppend(outputLine,string);
      }
      else
      {
        i = String_length(outputLine);
        convertSystemToConsoleEncodingAppend(outputLine,string);
      }

      // output line part
      UNUSED_RESULT(fwrite(String_cString(outputLine)+i,1,String_length(outputLine)-i,file));

      // store new output line
      j = String_findLastChar(outputLine,STRING_END,'\n');
      if (j >= 0)
      {
        String_remove(outputLine,STRING_BEGIN,(ulong)(j+1));
      }

      // save last output line
      lastOutputLine = outputLine;
    }
    else
    {
      if (String_index(string,STRING_END) == '\n')
      {
        if (outputLine != NULL) UNUSED_RESULT(fwrite(String_cString(outputLine),1,String_length(outputLine),file));
        UNUSED_RESULT(fwrite(String_cString(string),1,String_length(string),file));
        String_clear(outputLine);
      }
      else
      {
        String_append(outputLine,string);
      }
    }
  }
  else
  {
    // no thread local vairable -> output string
    UNUSED_RESULT(fwrite(String_cString(string),1,String_length(string),file));
  }
  fflush(file);
}

bool lockConsole(void)
{
  return Semaphore_lock(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER);
}

void unlockConsole(void)
{
  assert(Semaphore_isLocked(&consoleLock));

  Semaphore_unlock(&consoleLock);
}

void saveConsole(FILE *file, String *saveLine)
{
  ulong i;

  assert(file != NULL);
  assert(saveLine != NULL);
  assert(Semaphore_isLocked(&consoleLock));

  (*saveLine) = String_new();

  if (File_isTerminal(file))
  {
    // wipe-out last line
    if (lastOutputLine != NULL)
    {
      for (i = 0; i < String_length(lastOutputLine); i++)
      {
        UNUSED_RESULT(fwrite("\b",1,1,file));
      }
      for (i = 0; i < String_length(lastOutputLine); i++)
      {
        UNUSED_RESULT(fwrite(" ",1,1,file));
      }
      for (i = 0; i < String_length(lastOutputLine); i++)
      {
        UNUSED_RESULT(fwrite("\b",1,1,file));
      }
      fflush(file);
    }

    // save last line
    String_set(*saveLine,lastOutputLine);
  }
}

void restoreConsole(FILE *file, const String *saveLine)
{
  assert(file != NULL);
  assert(saveLine != NULL);
  assert(Semaphore_isLocked(&consoleLock));

  if (File_isTerminal(file))
  {
    // force restore of line on next output
    lastOutputLine = NULL;
  }

  String_delete(*saveLine);
}

void printConsole(FILE *file, uint width, const char *format, ...)
{
  String  line;
  va_list arguments;

  assert(file != NULL);
  assert(format != NULL);

  line = String_new();

  // format line
  va_start(arguments,format);
  String_vformat(line,format,arguments);
  va_end(arguments);
  if (width > 0)
  {
    if (String_length(line) < width)
    {
      String_padRight(line,width,' ');
    }
    else
    {
      String_truncate(line,STRING_BEGIN,width);
    }
  }

  // output
  SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    outputConsole(file,line);
  }

  String_delete(line);
}

void printWarning(const char *text, ...)
{
  va_list arguments;
  String  line;

  assert(text != NULL);

  // output log line
  va_start(arguments,text);
  vlogMessage(NULL,LOG_TYPE_WARNING,"Warning",text,arguments);
  va_end(arguments);

  // output line
  line = String_new();
  va_start(arguments,text);
  String_appendCString(line,"Warning: ");
  String_appendVFormat(line,text,arguments);
  String_appendChar(line,'\n');
  va_end(arguments);

  SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    outputConsole(stderr,line);
  }
  String_delete(line);
}

void printError(const char *text, ...)
{
  va_list arguments;
  String  line;

  assert(text != NULL);

  // output log line
  va_start(arguments,text);
  vlogMessage(NULL,LOG_TYPE_ERROR,"ERROR",text,arguments);
  va_end(arguments);

  // output line
  line = String_new();
  va_start(arguments,text);
  String_appendCString(line,"ERROR: ");
  String_appendVFormat(line,text,arguments);
  String_appendChar(line,'\n');
  va_end(arguments);

  SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    outputConsole(stderr,line);
  }
  String_delete(line);
}

void pprintInfo(uint verboseLevel, const char *prefix, const char *format, ...)
{
  va_list arguments;

  assert(format != NULL);

  va_start(arguments,format);
  vprintInfo(verboseLevel,prefix,format,arguments);
  va_end(arguments);
}

void printInfo(uint verboseLevel, const char *format, ...)
{
  va_list arguments;

  assert(format != NULL);

  va_start(arguments,format);
  vprintInfo(verboseLevel,NULL,format,arguments);
  va_end(arguments);
}

void executeIOOutput(ConstString line,
                     void        *userData
                    )
{
  StringList *stringList = (StringList*)userData;

  assert(line != NULL);

  printInfo(4,"%s\n",String_cString(line));
  if (stringList != NULL) StringList_append(stringList,line);
}

Errors initLog(LogHandle *logHandle)
{
  Errors error;

  assert(logHandle != NULL);

  logHandle->logFileName = String_new();
  error = File_getTmpFileNameCString(logHandle->logFileName,"bar-log",NULL /* directory */);
  if (error != ERROR_NONE)
  {
    String_delete(logHandle->logFileName); logHandle->logFileName = NULL;
    return error;
  }
  logHandle->logFile = fopen(String_cString(logHandle->logFileName),"w");
  if (logHandle->logFile == NULL)
  {
    error = ERRORX_(CREATE_FILE,errno,"%s",String_cString(logHandle->logFileName));
    (void)File_delete(logHandle->logFileName,FALSE);
    String_delete(logHandle->logFileName); logHandle->logFileName = NULL;
    return error;
  }

  return ERROR_NONE;
}

void doneLog(LogHandle *logHandle)
{
  assert(logHandle != NULL);

  if (logHandle->logFile != NULL)
  {
    fclose(logHandle->logFile); logHandle->logFile = NULL;
    (void)File_delete(logHandle->logFileName,FALSE);
    String_delete(logHandle->logFileName); logHandle->logFileName = NULL;
  }
}

void vlogMessage(LogHandle *logHandle, ulong logType, const char *prefix, const char *text, va_list arguments)
{
  static uint64 lastReopenTimestamp = 0LL;

  String  dateTime;
  va_list tmpArguments;
  uint64  nowTimestamp;

  assert(text != NULL);

  SEMAPHORE_LOCKED_DO(&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if ((logHandle != NULL) || (logFile != NULL))
    {
      if ((logType == LOG_TYPE_ALWAYS) || ((globalOptions.logTypes & logType) != 0))
      {
        dateTime = Misc_formatDateTime(String_new(),Misc_getCurrentDateTime(),TIME_TYPE_LOCAL,globalOptions.logFormat);

        // log to session log file
        if (logHandle != NULL)
        {
          // append to job log file (if possible)
          if (logHandle->logFile != NULL)
          {
            UNUSED_RESULT(fprintf(logHandle->logFile,"%s> ",String_cString(dateTime)));
            if (prefix != NULL)
            {
              UNUSED_RESULT(fputs(prefix,logHandle->logFile));
              UNUSED_RESULT(fprintf(logHandle->logFile,": "));
            }
            va_copy(tmpArguments,arguments);
            UNUSED_RESULT(vfprintf(logHandle->logFile,text,tmpArguments));
            va_end(tmpArguments);
            UNUSED_RESULT(fputc('\n',logHandle->logFile));
          }
        }

        // log to global log file
        if (logFile != NULL)
        {
          // re-open log for log-rotation
          nowTimestamp = Misc_getTimestamp();
          if (nowTimestamp > (lastReopenTimestamp+10LL*US_PER_MINUTE))
          {
            reopenLog();
            lastReopenTimestamp = nowTimestamp;
          }

          // append to log file
          UNUSED_RESULT(fprintf(logFile,"%s> ",String_cString(dateTime)));
          if (prefix != NULL)
          {
            UNUSED_RESULT(fputs(prefix,logFile));
            UNUSED_RESULT(fprintf(logFile,": "));
          }
          va_copy(tmpArguments,arguments);
          UNUSED_RESULT(vfprintf(logFile,text,tmpArguments));
          va_end(tmpArguments);
          UNUSED_RESULT(fputc('\n',logFile));
          fflush(logFile);
        }

        String_delete(dateTime);
      }
    }
  }
}

void plogMessage(LogHandle *logHandle, ulong logType, const char *prefix, const char *text, ...)
{
  va_list arguments;

  assert(text != NULL);

  va_start(arguments,text);
  vlogMessage(logHandle,logType,prefix,text,arguments);
  va_end(arguments);
}

void logMessage(LogHandle *logHandle, ulong logType, const char *text, ...)
{
  va_list arguments;

  assert(text != NULL);

  va_start(arguments,text);
  vlogMessage(logHandle,logType,NULL,text,arguments);
  va_end(arguments);
}

void logLines(LogHandle *logHandle, ulong logType, const char *prefix, const StringList *lines)
{
  StringNode *stringNode;
  String     line;

  assert(lines != NULL);

  STRINGLIST_ITERATE(lines,stringNode,line)
  {
    logMessage(logHandle,logType,"%s%s",prefix,String_cString(line));
  }
}

void fatalLogMessage(const char *text, void *userData)
{
  String dateTime;

  assert(text != NULL);

  UNUSED_VARIABLE(userData);

  if (!String_isEmpty(globalOptions.logFileName))
  {
    // try to open log file if not already open
    if (logFile == NULL)
    {
      logFile = fopen(String_cString(globalOptions.logFileName),"a");
    }

    if (logFile != NULL)
    {
      dateTime = Misc_formatDateTime(String_new(),Misc_getCurrentDateTime(),TIME_TYPE_LOCAL,globalOptions.logFormat);

      // append to log file
      UNUSED_RESULT(fprintf(logFile,"%s> ",String_cString(dateTime)));
      UNUSED_RESULT(fputs("FATAL: ",logFile));
      UNUSED_RESULT(fputs(text,logFile));
      fflush(logFile);

      String_delete(dateTime);
    }
  }
}

const char* getHumanSizeString(char *buffer, uint bufferSize, uint64 n)
{
  assert(buffer != NULL);

  if      (n > 1024LL*1024LL*1024LL*1024LL)
  {
    stringFormat(buffer,bufferSize,"%.1fG",(double)n/(double)(1024LL*1024LL*1024LL*1024LL));
  }
  else if (n >        1024LL*1024LL*1024LL)
  {
    stringFormat(buffer,bufferSize,"%.1fG",(double)n/(double)(1024LL*1024LL*1024LL));
  }
  else if (n >               1024LL*1024LL)
  {
    stringFormat(buffer,bufferSize,"%.1fM",(double)n/(double)(1024LL*1024LL));
  }
  else if (n >                      1024LL)
  {
    stringFormat(buffer,bufferSize,"%.1fK",(double)n/(double)(1024LL));
  }
  else
  {
    stringFormat(buffer,bufferSize,"%"PRIu64,n);
  }

  return buffer;
}

/***********************************************************************\
* Name   : executeIOlogPostProcess
* Purpose: process log-post command stderr output
* Input  : line     - line
*          userData - strerr string list
* Output : -
* Return : -
* Notes  : string list will be shortend to last 5 entries
\***********************************************************************/

LOCAL void executeIOlogPostProcess(ConstString line,
                                   void        *userData
                                  )
{
  StringList *stringList = (StringList*)userData;

  assert(stringList != NULL);
  assert(line != NULL);

  StringList_append(stringList,line);
  while (StringList_count(stringList) > 5)
  {
    String_delete(StringList_removeFirst(stringList,NULL));
  }
}

void logPostProcess(LogHandle        *logHandle,
                    const JobOptions *jobOptions,
                    ArchiveTypes     archiveType,
                    ConstString      scheduleCustomText,
                    ConstString      jobName,
                    JobStates        jobState,
                    bool             noStorage,
                    bool             dryRun,
                    ConstString      message
                   )
{
  String     command;
  TextMacros (textMacros,7);
  StringList stderrList;
  Errors     error;
  StringNode *stringNode;
  String     string;

  UNUSED_VARIABLE(jobOptions);

  assert(jobName != NULL);
  assert(jobOptions != NULL);

  if (!stringIsEmpty(globalOptions.logPostCommand))
  {
    if (logHandle != NULL)
    {
      // init variables
      command = String_new();

      if (logHandle->logFile != NULL)
      {
        assert(logHandle->logFileName != NULL);

        // close job log file
        fclose(logHandle->logFile);
        assert(logHandle->logFileName != NULL);

        // log post command for job log file
        TEXT_MACROS_INIT(textMacros)
        {
          TEXT_MACRO_X_STRING ("%file",   logHandle->logFileName,                       TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_STRING ("%name",   jobName,                                      TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_CSTRING("%type",   Archive_archiveTypeToString(archiveType),     TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_CSTRING("%T",      Archive_archiveTypeToShortString(archiveType),".");
          TEXT_MACRO_X_STRING ("%text",   scheduleCustomText,                           TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_CSTRING("%state",  Job_getStateText(jobState,noStorage,dryRun),  NULL);
          TEXT_MACRO_X_STRING ("%message",String_cString(message),NULL);
        }
//TODO: macro expanded 2x!
        Misc_expandMacros(command,
                          globalOptions.logPostCommand,
                          EXPAND_MACRO_MODE_STRING,
                          textMacros.data,
                          textMacros.count,
                          TRUE
                         );
        printInfo(2,"Log post process '%s'...\n",String_cString(command));
        assert(logHandle->logFileName != NULL);

        StringList_init(&stderrList);
        error = Misc_executeCommand(globalOptions.logPostCommand,
                                    textMacros.data,
                                    textMacros.count,
                                    NULL,  // commandLine
                                    CALLBACK_(NULL,NULL),
                                    CALLBACK_(executeIOlogPostProcess,&stderrList),
                                    (globalOptions.commandTimeout > 0) ? (long)globalOptions.commandTimeout : WAIT_FOREVER
                                   );
        if (error != ERROR_NONE)
        {
          printError(_("cannot post-process log file (error: %s)"),Error_getText(error));
          STRINGLIST_ITERATE(&stderrList,stringNode,string)
          {
            printError("  %s",String_cString(string));
          }
        }
        StringList_done(&stderrList);
      }

      // reset and reopen job log file
      if (logHandle->logFileName != NULL)
      {
        logHandle->logFile = fopen(String_cString(logHandle->logFileName),"w");
        if (logHandle->logFile == NULL)
        {
          printWarning(_("cannot re-open log file '%s' (error: %s)"),String_cString(logHandle->logFileName),strerror(errno));
        }
      }

      // free resources
      String_delete(command);
    }
  }
}

bool allocateServer(uint serverId, ServerConnectionPriorities priority, long timeout)
{
  ServerNode *serverNode;
  uint       maxConnectionCount;

  if (serverId != 0)
  {
    SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      // find server
      serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,serverNode->server.id == serverId);
      if (serverNode == NULL)
      {
        Semaphore_unlock(&globalOptions.serverList.lock);
        return TRUE;
      }

      // get max. number of allowed concurrent connections
      if (serverNode->server.maxConnectionCount != 0)
      {
        maxConnectionCount = serverNode->server.maxConnectionCount;
      }
      else
      {
        maxConnectionCount = 0;
        switch (serverNode->server.type)
        {
          case SERVER_TYPE_FILE:
            maxConnectionCount = MAX_UINT;
            break;
          case SERVER_TYPE_FTP:
            maxConnectionCount = globalOptions.defaultFTPServer.maxConnectionCount;
            break;
          case SERVER_TYPE_SSH:
            maxConnectionCount = globalOptions.defaultSSHServer.maxConnectionCount;
            break;
          case SERVER_TYPE_WEBDAV:
          case SERVER_TYPE_WEBDAVS:
            maxConnectionCount = globalOptions.defaultWebDAVServer.maxConnectionCount;
            break;
          default:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            #endif /* NDEBUG */
            break;
        }
      }

      // allocate server
      switch (priority)
      {
        case SERVER_CONNECTION_PRIORITY_LOW:
          if (   (maxConnectionCount != 0)
              && (serverNode->server.connection.count >= maxConnectionCount)
             )
          {
            // request low priority connection
            serverNode->server.connection.lowPriorityRequestCount++;
            Semaphore_signalModified(&globalOptions.serverList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

            // wait for free connection
            while (serverNode->server.connection.count >= maxConnectionCount)
            {
              if (!Semaphore_waitModified(&globalOptions.serverList.lock,timeout))
              {
                Semaphore_unlock(&globalOptions.serverList.lock);
                return FALSE;
              }
            }

            // low priority request done
            assert(serverNode->server.connection.lowPriorityRequestCount > 0);
            serverNode->server.connection.lowPriorityRequestCount--;
          }
          break;
        case SERVER_CONNECTION_PRIORITY_HIGH:
          if (   (maxConnectionCount != 0)
              && (serverNode->server.connection.count >= maxConnectionCount)
             )
          {
            // request high priority connection
            serverNode->server.connection.highPriorityRequestCount++;
            Semaphore_signalModified(&globalOptions.serverList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

            // wait for free connection
            while (serverNode->server.connection.count >= maxConnectionCount)
            {
              if (!Semaphore_waitModified(&globalOptions.serverList.lock,timeout))
              {
                Semaphore_unlock(&globalOptions.serverList.lock);
                return FALSE;
              }
            }

            // high priority request done
            assert(serverNode->server.connection.highPriorityRequestCount > 0);
            serverNode->server.connection.highPriorityRequestCount--;
          }
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }

      // allocated connection
      serverNode->server.connection.count++;
    }
  }

  return TRUE;
}

void freeServer(uint serverId)
{
  ServerNode *serverNode;

  if (serverId != 0)
  {
    SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      // find server
      serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,serverNode->server.id == serverId);
      if (serverNode != NULL)
      {
        assert(serverNode->server.connection.count > 0);

        // free connection
        serverNode->server.connection.count--;
      }
    }
  }
}

bool isServerAllocationPending(uint serverId)
{
  bool       pendingFlag;
  ServerNode *serverNode;

  pendingFlag = FALSE;

  if (serverId != 0)
  {
    SEMAPHORE_LOCKED_DO(&globalOptions.serverList.lock,SEMAPHORE_LOCK_TYPE_READ,WAIT_FOREVER)
    {
      // find server
      serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,serverNode->server.id == serverId);
      if (serverNode != NULL)
      {
        pendingFlag = (serverNode->server.connection.highPriorityRequestCount > 0);
      }
    }
  }

  return pendingFlag;
}

Errors mountAll(const MountList *mountList)
{
  const MountNode *mountNode;
  MountedNode     *mountedNode;
  Errors          error;

  assert(mountList != NULL);

  error = ERROR_NONE;

  SEMAPHORE_LOCKED_DO(&mountedList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    mountNode = LIST_HEAD(mountList);
    while (mountNode != NULL)
    {
      // find/add mounted node
      mountedNode = LIST_FIND(&mountedList,
                              mountedNode,
                                 String_equals(mountedNode->name,mountNode->name)
                              && String_equals(mountedNode->device,mountNode->device)
                             );
      if (mountedNode == NULL)
      {
        mountedNode = LIST_NEW_NODE(MountedNode);
        if (mountedNode == NULL)
        {
          HALT_INSUFFICIENT_MEMORY();
        }
        mountedNode->name       = String_duplicate(mountNode->name);
        mountedNode->device     = String_duplicate(mountNode->device);
        mountedNode->mountCount = Device_isMounted(mountNode->name) ? 1 : 0;

        List_append(&mountedList,mountedNode);
      }

      // mount
      if (mountedNode->mountCount == 0)
      {
        if (!Device_isMounted(mountedNode->name))
        {
          if (!String_isEmpty(mountedNode->device))
          {
            error = Device_mount(globalOptions.mountDeviceCommand,mountedNode->name,mountedNode->device);
          }
          else
          {
            error = Device_mount(globalOptions.mountCommand,mountedNode->name,NULL);
          }
        }
      }
      if (error != ERROR_NONE)
      {
        break;
      }
      mountedNode->mountCount++;
      mountedNode->lastMountTimestamp = Misc_getTimestamp();

      // next
      mountNode = mountNode->next;
    }
    assert((error != ERROR_NONE) || (mountNode == NULL));

    if (error != ERROR_NONE)
    {
      assert(mountNode != NULL);

      printError(_("cannot mount '%s' (error: %s)"),
                 String_cString(mountNode->name),
                 Error_getText(error)
                );

      // revert mounts
      mountNode = mountNode->prev;
      while (mountNode != NULL)
      {
        // find mounted node
        mountedNode = LIST_FIND(&mountedList,
                                mountedNode,
                                   String_equals(mountedNode->name,mountNode->name)
                                && String_equals(mountedNode->device,mountNode->device)
                               );
        if (mountedNode != NULL)
        {
          assert(mountedNode->mountCount > 0);
          mountedNode->mountCount--;
          if (mountedNode->mountCount == 0)
          {
            (void)Device_umount(globalOptions.unmountCommand,mountNode->name);

            List_removeAndFree(&mountedList,mountedNode);
          }
        }

        // previous
        mountNode = mountNode->prev;
      }
    }
  }

  return error;
}

Errors unmountAll(const MountList *mountList)
{
  MountNode   *mountNode;
  MountedNode *mountedNode;

  assert(mountList != NULL);

  SEMAPHORE_LOCKED_DO(&mountedList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    LIST_ITERATE(mountList,mountNode)
    {
      // find mounted node
      mountedNode = LIST_FIND(&mountedList,
                              mountedNode,
                                 String_equals(mountedNode->name,mountNode->name)
                              && String_equals(mountedNode->device,mountNode->device)
                             );
      if (mountedNode != NULL)
      {
        assert(mountedNode->mountCount > 0);
        if (mountedNode->mountCount > 0) mountedNode->mountCount--;
      }
    }
  }

  return ERROR_NONE;
}

void purgeMounts(bool forceFlag)
{
  MountedNode *mountedNode;
  Errors      error;

  SEMAPHORE_LOCKED_DO(&mountedList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    mountedNode = mountedList.head;
    while (mountedNode != NULL)
    {
      if (   (mountedNode->mountCount == 0)
          && (   forceFlag
              || (Misc_getTimestamp() > (mountedNode->lastMountTimestamp+MOUNT_TIMEOUT*US_PER_MS))
             )
         )
      {
        if (Device_isMounted(mountedNode->name))
        {
          error = Device_umount(globalOptions.unmountCommand,mountedNode->name);
          if (error != ERROR_NONE)
          {
            printWarning(_("cannot unmount '%s' (error: %s)"),
                         String_cString(mountedNode->name),
                         Error_getText(error)
                        );
          }
        }
        mountedNode = List_removeAndFree(&mountedList,mountedNode);
      }
      else
      {
        mountedNode = mountedNode->next;
      }
    }
  }
}

const char *getPasswordTypeText(PasswordTypes passwordType)
{
  const char *text;

  text = NULL;
  switch (passwordType)
  {
    case PASSWORD_TYPE_CRYPT:  text = "crypt";    break;
    case PASSWORD_TYPE_FTP:    text = "FTP";      break;
    case PASSWORD_TYPE_SSH:    text = "SSH";      break;
    case PASSWORD_TYPE_WEBDAV: text = "webDAV";   break;
    case PASSWORD_TYPE_SMB:    text = "SMB/CIFS"; break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return text;
}

Errors getPasswordFromConsole(String        name,
                              Password      *password,
                              PasswordTypes passwordType,
                              const char    *text,
                              bool          validateFlag,
                              bool          weakCheckFlag,
                              void          *userData
                             )
{
  Errors error;

  assert(name == NULL);
  assert(password != NULL);

  UNUSED_VARIABLE(name);
  UNUSED_VARIABLE(userData);

  error = ERROR_UNKNOWN;

  switch (globalOptions.runMode)
  {
    case RUN_MODE_INTERACTIVE:
      {
        String title1,title2;
        String saveLine;

        SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          saveConsole(stdout,&saveLine);

          // input password
          title1 = String_new();
          title2 = String_new();
          switch (passwordType)
          {
            case PASSWORD_TYPE_CRYPT   : String_format(title1,"Crypt password"); break;
            case PASSWORD_TYPE_FTP     : String_format(title1,"FTP password"); break;
            case PASSWORD_TYPE_SSH     : String_format(title1,"SSH password"); break;
            case PASSWORD_TYPE_WEBDAV  : String_format(title1,"WebDAV password"); break;
            case PASSWORD_TYPE_SMB     : String_format(title1,"SMB/CIFS password"); break;
            case PASSWORD_TYPE_DATABASE: String_format(title1,"Database password"); break;
          }
          if (!stringIsEmpty(text))
          {
            String_appendFormat(title1,_(" for '%s'"),text);
          }
          if (!Password_input(password,String_cString(title1),PASSWORD_INPUT_MODE_ANY) || (Password_length(password) <= 0))
          {
            restoreConsole(stdout,&saveLine);
            String_delete(title2);
            String_delete(title1);
            Semaphore_unlock(&consoleLock);
            error = ERROR_NO_CRYPT_PASSWORD;
            break;
          }
          if (validateFlag)
          {
            // verify input password
            if ((text != NULL) && !stringIsEmpty(text))
            {
              String_format(title2,_("Verify password for '%s'"),text);
            }
            else
            {
              String_setCString(title2,"Verify password");
            }
            if (Password_inputVerify(password,String_cString(title2),PASSWORD_INPUT_MODE_ANY))
            {
              error = ERROR_NONE;
            }
            else
            {
              printError(_("%s passwords are not equal!"),String_cString(title1));
              restoreConsole(stdout,&saveLine);
              String_delete(title2);
              String_delete(title1);
              Semaphore_unlock(&consoleLock);
              error = ERROR_CRYPT_PASSWORDS_MISMATCH;
              break;
            }
          }
          else
          {
            error = ERROR_NONE;
          }
          String_delete(title2);
          String_delete(title1);

          if (weakCheckFlag)
          {
            // check password quality
            if (Password_getQualityLevel(password) < MIN_PASSWORD_QUALITY_LEVEL)
            {
              printWarning(_("low password quality!"));
            }
          }

          restoreConsole(stdout,&saveLine);
        }
      }
      break;
    case RUN_MODE_BATCH:
      printf("PASSWORD\n"); fflush(stdout);
      if (Password_input(password,NULL,PASSWORD_INPUT_MODE_CONSOLE) || (Password_length(password) <= 0))
      {
        error = ERROR_NONE;
      }
      else
      {
        error = ERROR_NO_CRYPT_PASSWORD;
      }
      break;
    case RUN_MODE_SERVER:
      error = ERROR_NO_CRYPT_PASSWORD;
      break;
  }
  assert(error != ERROR_UNKNOWN);

  return error;
}

Errors initFilePattern(Pattern *pattern, ConstString fileName, PatternTypes patternType)
{
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    String    string;
  #endif /* PLATFORM_... */
  Errors error;

  assert(pattern != NULL);
  assert(fileName != NULL);

  // init pattern
  #if   defined(PLATFORM_LINUX)
    error = Pattern_init(pattern,
                         fileName,
                         patternType,
                         PATTERN_FLAG_NONE
                        );
  #elif defined(PLATFORM_WINDOWS)
    // escape all '\' by '\\'
    string = String_duplicate(fileName);
    String_replaceAllCString(string,STRING_BEGIN,"\\","\\\\");

    error = Pattern_init(pattern,
                         string,
                         patternType,
                         PATTERN_FLAG_IGNORE_CASE
                        );

    // free resources
    String_delete(string);
  #endif /* PLATFORM_... */

  return error;
}

Errors addStorageNameListFromFile(StringList *storageNameList, const char *fileName)
{
  Errors     error;
  FileHandle fileHandle;
  String     line;

  assert(storageNameList != NULL);

  // init variables
  line = String_new();

  // open file
  if ((fileName == NULL) || stringEquals(fileName,"-"))
  {
    error = File_openDescriptor(&fileHandle,FILE_DESCRIPTOR_STDIN,FILE_OPEN_READ|FILE_STREAM);
  }
  else
  {
    error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  }
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

  // read file
  while (!File_eof(&fileHandle))
  {
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE)
    {
      File_close(&fileHandle);
      String_delete(line);
      return error;
    }
    StringList_append(storageNameList,line);
  }

  // close file
  File_close(&fileHandle);

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

Errors addStorageNameListFromCommand(StringList *storageNameList, const char *template)
{
  String script;
  Errors error;

  assert(storageNameList != NULL);
  assert(template != NULL);

  // init variables
  script = String_new();

  // expand template
  Misc_expandMacros(script,
                    template,
                    EXPAND_MACRO_MODE_STRING,
                    NULL,  // textMacros
                    0,  // SIZE_OF_ARRAY(textMacros)
                    TRUE
                   );

  // execute script and collect output
  error = Misc_executeScript(String_cString(script),
                             CALLBACK_INLINE(void,(ConstString line, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               StringList_append(storageNameList,line);
                             },NULL),
                             CALLBACK_(NULL,NULL),
                             (globalOptions.commandTimeout > 0) ? (long)globalOptions.commandTimeout : WAIT_FOREVER
                            );
  if (error != ERROR_NONE)
  {
    String_delete(script);
    return error;
  }

  // free resources
  String_delete(script);

  return ERROR_NONE;
}

Errors addIncludeListFromFile(EntryTypes entryType, EntryList *entryList, const char *fileName)
{
  Errors     error;
  FileHandle fileHandle;
  String     line;

  assert(entryList != NULL);
  assert(fileName != NULL);

  // init variables
  line = String_new();

  // open file
  if (stringEquals(fileName,"-"))
  {
    error = File_openDescriptor(&fileHandle,FILE_DESCRIPTOR_STDIN,FILE_OPEN_READ|FILE_STREAM);
  }
  else
  {
    error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  }
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

  // read file
  while (!File_eof(&fileHandle))
  {
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE)
    {
      File_close(&fileHandle);
      String_delete(line);
      return error;
    }
    EntryList_append(entryList,entryType,line,PATTERN_TYPE_GLOB,NULL);
  }

  // close file
  File_close(&fileHandle);

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

Errors addIncludeListFromCommand(EntryTypes entryType, EntryList *entryList, const char *template)
{
  String script;
  Errors error;

  assert(entryList != NULL);
  assert(template != NULL);

  // init variables
  script = String_new();

  // expand template
  Misc_expandMacros(script,
                    template,
                    EXPAND_MACRO_MODE_STRING,
                    NULL,  // textMacros
                    0,  // SIZE_OF_ARRAY(textMacros)
                    TRUE
                   );

  // execute script and collect output
  error = Misc_executeScript(String_cString(script),
                             CALLBACK_INLINE(void,(ConstString line, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               EntryList_append(entryList,entryType,line,PATTERN_TYPE_GLOB,NULL);
                             },NULL),
                             CALLBACK_(NULL,NULL),
                             (globalOptions.commandTimeout > 0) ? (long)globalOptions.commandTimeout : WAIT_FOREVER
                            );
  if (error != ERROR_NONE)
  {
    String_delete(script);
    return error;
  }

  // free resources
  String_delete(script);

  return ERROR_NONE;
}

Errors addExcludeListFromFile(PatternList *patternList, const char *fileName)
{
  Errors     error;
  FileHandle fileHandle;
  String     line;

  assert(patternList != NULL);
  assert(fileName != NULL);

  // init variables
  line = String_new();

  // open file
  // open file
  if (stringEquals(fileName,"-"))
  {
    error = File_openDescriptor(&fileHandle,FILE_DESCRIPTOR_STDIN,FILE_OPEN_READ|FILE_STREAM);
  }
  else
  {
    error = File_openCString(&fileHandle,fileName,FILE_OPEN_READ);
  }
  if (error != ERROR_NONE)
  {
    String_delete(line);
    return error;
  }

  // read file
  while (!File_eof(&fileHandle))
  {
    error = File_readLine(&fileHandle,line);
    if (error != ERROR_NONE)
    {
      File_close(&fileHandle);
      String_delete(line);
      return error;
    }
    PatternList_append(patternList,line,PATTERN_TYPE_GLOB,NULL);
  }

  // close file
  File_close(&fileHandle);

  // free resources
  String_delete(line);

  return ERROR_NONE;
}

Errors addExcludeListFromCommand(PatternList *patternList, const char *template)
{
  String script;
  Errors error;

  assert(patternList != NULL);
  assert(template != NULL);

  // init variables
  script = String_new();

  // expand template
  Misc_expandMacros(script,
                    template,
                    EXPAND_MACRO_MODE_STRING,
                    NULL,  // textMacros
                    0,  // SIZE_OF_ARRAY(textMacros)
                    TRUE
                   );

  // execute script and collect output
  error = Misc_executeScript(String_cString(script),
                             CALLBACK_INLINE(void,(ConstString line, void *userData),
                             {
                               UNUSED_VARIABLE(userData);

                               PatternList_append(patternList,line,PATTERN_TYPE_GLOB,NULL);
                             },NULL),
                             CALLBACK_(NULL,NULL),
                             (globalOptions.commandTimeout > 0) ? (long)globalOptions.commandTimeout : WAIT_FOREVER
                            );
  if (error != ERROR_NONE)
  {
    String_delete(script);
    return error;
  }

  // free resources
  String_delete(script);

  return ERROR_NONE;
}

bool isIncluded(const EntryNode *includeEntryNode,
                ConstString     name
               )
{
  assert(includeEntryNode != NULL);
  assert(name != NULL);

  return Pattern_match(&includeEntryNode->pattern,name,STRING_BEGIN,PATTERN_MATCH_MODE_BEGIN,NULL,NULL);
}

bool isInIncludedList(const EntryList *includeEntryList,
                      ConstString     name
                     )
{
  const EntryNode *entryNode;

  assert(includeEntryList != NULL);
  assert(name != NULL);

  LIST_ITERATE(includeEntryList,entryNode)
  {
    if (Pattern_match(&entryNode->pattern,name,STRING_BEGIN,PATTERN_MATCH_MODE_BEGIN,NULL,NULL))
    {
      return TRUE;
    }
  }

  return FALSE;
}

bool isInExcludedList(const PatternList *excludePatternList,
                      ConstString       name
                     )
{
  assert(excludePatternList != NULL);
  assert(name != NULL);

  return PatternList_match(excludePatternList,name,PATTERN_MATCH_MODE_EXACT);
}

bool hasNoBackup(ConstString pathName)
{
  String fileName;
  bool   hasNoBackupFlag;

  assert(pathName != NULL);

  hasNoBackupFlag = FALSE;

  fileName = String_new();
  hasNoBackupFlag |= File_exists(File_appendFileNameCString(File_setFileName(fileName,pathName),".nobackup"));
  hasNoBackupFlag |= File_exists(File_appendFileNameCString(File_setFileName(fileName,pathName),".NOBACKUP"));
  String_delete(fileName);

  return hasNoBackupFlag;
}

bool hasNoDumpAttribute(ConstString name)
{
  bool     hasNoDumpAttributeFlag;
  FileInfo fileInfo;

  assert(name != NULL);

  hasNoDumpAttributeFlag = FALSE;

  if (File_getInfo(&fileInfo,name))
  {
    hasNoDumpAttributeFlag = File_hasAttributeNoDump(&fileInfo);
  }

  return hasNoDumpAttributeFlag;
}

// ----------------------------------------------------------------------

#if 0
/***********************************************************************\
* Name   : readFromJob
* Purpose: read options from job file
* Input  : fileName - file name
* Output : -
* Return : TRUE iff read
* Notes  : -
\***********************************************************************/

LOCAL bool readFromJob(ConstString fileName)
{
  Errors     error;
  FileHandle fileHandle;
  bool       failFlag;
  uint       lineNb;
  String     line;
  String     title;
  String     type;
  String     name,value;
  long       nextIndex;
  uint       i;

  assert(fileName != NULL);

  // open file
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printError(_("cannot open job file '%s' (error: %s)!"),
               String_cString(fileName),
               Error_getText(error)
              );
    return FALSE;
  }

  // parse file
  failFlag = FALSE;
  line     = String_new();
  lineNb   = 0;
  title    = String_new();
  type     = String_new();
  name     = String_new();
  value    = String_new();
  while (File_getLine(&fileHandle,line,&lineNb,"#") && !failFlag)
  {
    // parse line
    if      (String_parse(line,STRING_BEGIN,"[schedule %S]",NULL,title))
    {
      // skip schedule sections
      while (   File_getLine(&fileHandle,line,&lineNb,"#")
             && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
             && !failFlag
            )
      {
        // nothing to do
      }
      File_ungetLine(&fileHandle,line,&lineNb);
    }
    if      (String_parse(line,STRING_BEGIN,"[persistence %S]",NULL,type))
    {
      // skip persistence sections
      while (   File_getLine(&fileHandle,line,&lineNb,"#")
             && !String_matchCString(line,STRING_BEGIN,"^\\s*\\[",NULL,NULL,NULL)
             && !failFlag
            )
      {
        // nothing to do
      }
      File_ungetLine(&fileHandle,line,&lineNb);
    }
    else if (String_parse(line,STRING_BEGIN,"[global]",NULL))
    {
      // nothing to do
    }
    else if (String_parse(line,STRING_BEGIN,"[end]",NULL))
    {
      // nothing to do
    }
    else if (String_parse(line,STRING_BEGIN,"%S=% S",&nextIndex,name,value))
    {
      i = ConfigValue_find(JOB_CONFIG_VALUES,
                           i0,
                           i1,
                           String_cString(name)
                          );
      if (i != CONFIG_VALUE_INDEX_NONE)
      {
        ConfigValue_parse(JOB_CONFIG_VALUES,
                          &JOB_CONFIG_VALUES[i],
                          NULL, // sectionName,
                          String_cString(value),
                          CALLBACK_INLINE(void,(const char *errorMessage, void *userData),
                          {
                            UNUSED_VARIABLE(userData);

                            printError(_("%s in %s, line %ld"),errorMessage,String_cString(fileName),lineNb);
                            failFlag = TRUE;
                          },NULL),
                          CALLBACK_INLINE(void,(const char *warningMessage, void *userData),
                          {
                            UNUSED_VARIABLE(userData);

                            printWarning(_("%s in %s, line %ld"),warningMessage,String_cString(fileName),lineNb);
                          },NULL),
                          NULL  // variable
                         );
      }
      else
      {
        printError(_("unknown value '%s' in %s, line %ld"),String_cString(name),String_cString(fileName),lineNb);
        failFlag = TRUE;
      }
    }
    else
    {
      printError(_("syntax error in '%s', line %ld: '%s' - skipped"),
                 String_cString(fileName),
                 lineNb,
                 String_cString(line)
                );
      failFlag = TRUE;
    }
  }
  String_delete(value);
  String_delete(name);
  String_delete(type);
  String_delete(title);
  String_delete(line);

  // close file
  (void)File_close(&fileHandle);

  return !failFlag;
}
#endif

/***********************************************************************\
* Name   : createPIDFile
* Purpose: create pid file
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors createPIDFile(void)
{
  String     fileName;
  Errors     error;
  FileHandle fileHandle;

  if (!stringIsEmpty(globalOptions.pidFileName))
  {
    fileName = String_new();
    error = File_open(&fileHandle,File_setFileNameCString(fileName,globalOptions.pidFileName),FILE_OPEN_CREATE);
    if (error != ERROR_NONE)
    {
      String_delete(fileName);
      printError(_("cannot create process id file '%s' (error: %s)"),globalOptions.pidFileName,Error_getText(error));
      return error;
    }
    File_printLine(&fileHandle,"%d",(int)getpid());
    (void)File_close(&fileHandle);
    String_delete(fileName);
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : deletePIDFile
* Purpose: delete pid file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void deletePIDFile(void)
{
  if (globalOptions.pidFileName != NULL)
  {
    (void)File_deleteCString(globalOptions.pidFileName,FALSE);
  }
}

/***********************************************************************\
* Name   : errorToExitcode
* Purpose: map error to exitcode
* Input  : error - error
* Output : -
* Return : exitcode
* Notes  : -
\***********************************************************************/

LOCAL int errorToExitcode(Errors error)
{
  switch (Error_getCode(error))
  {
    case ERROR_CODE_NONE:
      return EXITCODE_OK;
      break;
    case ERROR_CODE_TESTCODE:
      return EXITCODE_TESTCODE;
      break;
    case ERROR_CODE_INVALID_ARGUMENT:
      return EXITCODE_INVALID_ARGUMENT;
      break;
    case ERROR_CODE_CONFIG:
      return EXITCODE_CONFIG_ERROR;
    case ERROR_CODE_FUNCTION_NOT_SUPPORTED:
      return EXITCODE_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      return EXITCODE_FAIL;
      break;
  }
}

/***********************************************************************\
* Name   : generateEncryptionKeys
* Purpose: generate key pairs for encryption
* Input  : keyFileBaseName - key base file name or NULL
*          cryptPassword   - crypt password for private key
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors generateEncryptionKeys(const char *keyFileBaseName,
                                    Password   *cryptPassword
                                   )
{
  String   publicKeyFileName,privateKeyFileName;
  void     *data;
  uint     dataLength;
  Errors   error;
  CryptKey publicKey,privateKey;
  String   directoryName;

  // initialize variables
  publicKeyFileName  = String_new();
  privateKeyFileName = String_new();

  if (keyFileBaseName != NULL)
  {
    // get file names of keys
    File_setFileNameCString(publicKeyFileName,keyFileBaseName);
    String_appendCString(publicKeyFileName,".public");
    File_setFileNameCString(privateKeyFileName,keyFileBaseName);
    String_appendCString(privateKeyFileName,".private");

    // check if key files already exists
    if (File_exists(publicKeyFileName))
    {
      printError(_("public key file '%s' already exists!"),String_cString(publicKeyFileName));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
    if (File_exists(privateKeyFileName))
    {
      printError(_("private key file '%s' already exists!"),String_cString(privateKeyFileName));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
  }

  // get crypt password for private key encryption
  if (Password_isEmpty(cryptPassword))
  {
    error = getPasswordFromConsole(NULL,  // name
                                        cryptPassword,
                                        PASSWORD_TYPE_CRYPT,
                                        String_cString(privateKeyFileName),
                                        TRUE,  // validateFlag
                                        FALSE, // weakCheckFlag
                                        NULL  // userData
                                       );
    if (error != ERROR_NONE)
    {
      printError(_("no password given for private key!"));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
  }

  // generate new key pair for encryption
  if (Misc_isStdoutTerminal()) printInfo(1,"Generate keys (collecting entropie)...");
  Crypt_initKey(&publicKey,CRYPT_PADDING_TYPE_NONE);
  Crypt_initKey(&privateKey,CRYPT_PADDING_TYPE_NONE);
  error = Crypt_createPublicPrivateKeyPair(&publicKey,&privateKey,globalOptions.generateKeyBits,globalOptions.generateKeyMode);
  if (error != ERROR_NONE)
  {
    printError(_("cannot create encryption key pair (error: %s)!"),Error_getText(error));
    Crypt_doneKey(&privateKey);
    Crypt_doneKey(&publicKey);
    String_delete(privateKeyFileName);
    String_delete(publicKeyFileName);
    return error;
  }
//fprintf(stderr,"%s, %d: public %d \n",__FILE__,__LINE__,publicKey.dataLength); debugDumpMemory(publicKey.data,publicKey.dataLength,0);
//fprintf(stderr,"%s, %d: private %d\n",__FILE__,__LINE__,privateKey.dataLength); debugDumpMemory(privateKey.data,privateKey.dataLength,0);
  if (Misc_isStdoutTerminal()) printInfo(1,"OK\n");

  // output keys
  if (keyFileBaseName != NULL)
  {
    // create directory if it does not exists
    directoryName = File_getDirectoryNameCString(String_new(),keyFileBaseName);
    if (!String_isEmpty(directoryName))
    {
      if      (!File_exists(directoryName))
      {
        error = File_makeDirectory(directoryName,FILE_DEFAULT_USER_ID,FILE_DEFAULT_GROUP_ID,FILE_DEFAULT_PERMISSIONS,FALSE);
        if (error != ERROR_NONE)
        {
          printError(_("cannot create directory '%s' (error: %s)!"),String_cString(directoryName),Error_getText(error));
          String_delete(directoryName);
          Crypt_doneKey(&privateKey);
          Crypt_doneKey(&publicKey);
          String_delete(privateKeyFileName);
          String_delete(publicKeyFileName);
          return error;
        }
      }
      else if (!File_isDirectory(directoryName))
      {
        printError(_("'%s' is not a directory!"),String_cString(directoryName));
        String_delete(directoryName);
        Crypt_doneKey(&privateKey);
        Crypt_doneKey(&publicKey);
        String_delete(privateKeyFileName);
        String_delete(publicKeyFileName);
        return error;
      }
    }
    String_delete(directoryName);

    // write encryption public key file
    error = Crypt_writePublicPrivateKeyFile(&publicKey,
                                            String_cString(publicKeyFileName),
                                            CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // cryptSalt
                                            NULL  // password
                                           );
    if (error != ERROR_NONE)
    {
      printError(_("cannot write encryption public key file (error: %s)!"),Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created public encryption key '%s'\n",String_cString(publicKeyFileName));

    // write encryption private key file
    error = Crypt_writePublicPrivateKeyFile(&privateKey,
                                            String_cString(privateKeyFileName),
                                            CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                            CRYPT_KEY_DERIVE_FUNCTION,
                                            NULL,  // cryptSalt
                                            cryptPassword
                                           );
    if (error != ERROR_NONE)
    {
      printError(_("cannot write encryption private key file (error: %s)!"),Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(data);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created private encryption key '%s'\n",String_cString(privateKeyFileName));
  }
  else
  {
    String base64Data;

    // output encryption public key to stdout
    error = Crypt_getPublicPrivateKeyData(&publicKey,
                                          &data,
                                          &dataLength,
                                          CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                          CRYPT_KEY_DERIVE_NONE,
                                          NULL,  // cryptSalt
                                          NULL  // password
                                         );
    if (error != ERROR_NONE)
    {
      printError(_("cannot get encryption public key (error: %s)!"),Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }

    base64Data = Misc_base64Encode(String_new(),data,dataLength);

    printf("crypt-public-key = base64:%s\n",String_cString(base64Data));

    String_delete(base64Data);
    freeSecure(data);

    // output encryption private key to stdout
    error = Crypt_getPublicPrivateKeyData(&privateKey,
                                          &data,
                                          &dataLength,
                                          CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                          CRYPT_KEY_DERIVE_NONE,
                                          NULL,  // cryptSalt
                                          NULL  // password
                                         );
    if (error != ERROR_NONE)
    {
      printError(_("cannot get encryption private key (error: %s)!"),Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }

    base64Data = Misc_base64Encode(String_new(),data,dataLength);

    printf("crypt-private-key = base64:%s\n",String_cString(base64Data));

    String_delete(base64Data);
    freeSecure(data);
  }
  Crypt_doneKey(&privateKey);
  Crypt_doneKey(&publicKey);

  // free resources
  String_delete(privateKeyFileName);
  String_delete(publicKeyFileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : generateSignatureKeys
* Purpose: generate key pairs for signature
* Input  : keyFileBaseName - key base file name or NULL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors generateSignatureKeys(const char *keyFileBaseName)
{
  String   publicKeyFileName,privateKeyFileName;
  void     *data;
  uint     dataLength;
  Errors   error;
  CryptKey publicKey,privateKey;
  String   directoryName;

  // initialize variables
  publicKeyFileName  = String_new();
  privateKeyFileName = String_new();

  if (keyFileBaseName != NULL)
  {
    // get file names of keys
    File_setFileNameCString(publicKeyFileName,keyFileBaseName);
    String_appendCString(publicKeyFileName,".public");
    File_setFileNameCString(privateKeyFileName,keyFileBaseName);
    String_appendCString(privateKeyFileName,".private");

    // check if key files already exists
    if (File_exists(publicKeyFileName))
    {
      printError(_("public key file '%s' already exists!"),String_cString(publicKeyFileName));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
    if (File_exists(privateKeyFileName))
    {
      printError(_("private key file '%s' already exists!"),String_cString(privateKeyFileName));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
  }

  // generate new key pair for signature
  if (Misc_isStdoutTerminal()) printInfo(1,"Generate signature keys (collecting entropie)...");
  Crypt_initKey(&publicKey,CRYPT_PADDING_TYPE_NONE);
  Crypt_initKey(&privateKey,CRYPT_PADDING_TYPE_NONE);
  error = Crypt_createPublicPrivateKeyPair(&publicKey,&privateKey,globalOptions.generateKeyBits,globalOptions.generateKeyMode);
  if (error != ERROR_NONE)
  {
    printError(_("cannot create signature key pair (error: %s)!"),Error_getText(error));
    Crypt_doneKey(&privateKey);
    Crypt_doneKey(&publicKey);
    String_delete(privateKeyFileName);
    String_delete(publicKeyFileName);
    return error;
  }
  if (Misc_isStdoutTerminal()) printInfo(1,"OK\n");

  // output keys
  if (keyFileBaseName != NULL)
  {
    // create directory if it does not exists
    directoryName = File_getDirectoryNameCString(String_new(),keyFileBaseName);
    if (!String_isEmpty(directoryName))
    {
      if      (!File_exists(directoryName))
      {
        error = File_makeDirectory(directoryName,FILE_DEFAULT_USER_ID,FILE_DEFAULT_GROUP_ID,FILE_DEFAULT_PERMISSIONS,FALSE);
        if (error != ERROR_NONE)
        {
          printError(_("cannot create directory '%s' (error: %s)!"),String_cString(directoryName),Error_getText(error));
          String_delete(directoryName);
          Crypt_doneKey(&privateKey);
          Crypt_doneKey(&publicKey);
          String_delete(privateKeyFileName);
          String_delete(publicKeyFileName);
          return error;
        }
      }
      else if (!File_isDirectory(directoryName))
      {
        printError(_("'%s' is not a directory!"),String_cString(directoryName));
        String_delete(directoryName);
        Crypt_doneKey(&privateKey);
        Crypt_doneKey(&publicKey);
        String_delete(privateKeyFileName);
        String_delete(publicKeyFileName);
        return error;
      }
    }
    String_delete(directoryName);

    // write signature public key
    error = Crypt_writePublicPrivateKeyFile(&publicKey,
                                            String_cString(publicKeyFileName),
                                            CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // cryptSalt
                                            NULL  // password
                                           );
    if (error != ERROR_NONE)
    {
      printError(_("cannot write signature public key file!"));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created public signature key '%s'\n",String_cString(publicKeyFileName));

    // write signature private key
    error = Crypt_writePublicPrivateKeyFile(&privateKey,
                                            String_cString(privateKeyFileName),
                                            CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                            CRYPT_KEY_DERIVE_NONE,
                                            NULL,  // cryptSalt
                                            NULL  // password,
                                           );
    if (error != ERROR_NONE)
    {
      printError(_("cannot write signature private key file!"));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("Created private signature key '%s'\n",String_cString(privateKeyFileName));
  }
  else
  {
    String base64Data;

    // output signature public key to stdout
    error = Crypt_getPublicPrivateKeyData(&publicKey,
                                          &data,
                                          &dataLength,
                                          CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                          CRYPT_KEY_DERIVE_NONE,
                                          NULL,  // cryptSalt
                                          NULL  // password,
                                         );
    if (error != ERROR_NONE)
    {
      printError(_("cannot get signature public key!"));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }

    base64Data = Misc_base64Encode(String_new(),data,dataLength);

    printf("signature-public-key = base64:%s\n",String_cString(base64Data));

    String_delete(base64Data);
    freeSecure(data);

    // output signature private key to stdout
    error = Crypt_getPublicPrivateKeyData(&privateKey,
                                          &data,
                                          &dataLength,
                                          CRYPT_MODE_CBC_|CRYPT_MODE_CTS_,
                                          CRYPT_KEY_DERIVE_NONE,
                                          NULL,  // cryptSalt
                                          NULL  // password
                                         );
    if (error != ERROR_NONE)
    {
      printError(_("cannot get signature private key!"));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }

    base64Data = Misc_base64Encode(String_new(),data,dataLength);

    printf("signature-private-key = base64:%s\n",String_cString(base64Data));

    String_delete(base64Data);
    freeSecure(data);
  }
  Crypt_doneKey(&privateKey);
  Crypt_doneKey(&publicKey);

  // free resources
  String_delete(privateKeyFileName);
  String_delete(publicKeyFileName);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : runServer
* Purpose: run as server
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors runServer(void)
{
  Errors error;

  // open log file
  openLog();

  // create pid file
  error = createPIDFile();
  if (error != ERROR_NONE)
  {
    printError(_("cannot create PID file"),Error_getText(error));
    closeLog();
    return error;
  }

  // read all jobs (if master)
  if (globalOptions.serverMode == SERVER_MODE_MASTER)
  {
    error = Job_rereadAll(globalOptions.jobsDirectory);
    if (error != ERROR_NONE)
    {
      printError(_("cannot read jobs from '%s' (error: %s)"),
                 String_cString(globalOptions.jobsDirectory),
                 Error_getText(error)
                );
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 _("cannot read jobs from '%s' (error: %s)!"),
                 String_cString(globalOptions.jobsDirectory),
                 Error_getText(error)
                );
      deletePIDFile();
      closeLog();
      return error;
    }
  }

  // init continuous
  error = Continuous_init(globalOptions.continuousDatabaseFileName);
  if (error != ERROR_NONE)
  {
    printWarning(_("continuous support is not available (reason: %s)"),Error_getText(error));
  }
  Job_updateAllNotifies();

  // init UUID if needed (ignore errors)
  if (String_isEmpty(instanceUUID))
  {
    Misc_getUUID(instanceUUID);
    error = Configuration_update();
    if (error != ERROR_NONE)
    {
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 "Update configuration file fail (error: %s)",
                 Error_getText(error)
                );
    }
  }

  // server mode -> run server with network
  globalOptions.runMode = RUN_MODE_SERVER;
  error = Server_socket();
  if (error != ERROR_NONE)
  {
    Continuous_done();
    deletePIDFile();
    closeLog();
    return error;
  }

  // update config
  if (Configuration_isModified())
  {
    error = Configuration_update();
    if (error == ERROR_NONE)
    {
      Configuration_clearModified();
    }
    else
    {
      logMessage(NULL,  // logHandle,
                 LOG_TYPE_ALWAYS,
                 "Update configuration file fail (error: %s)",
                 Error_getText(error)
                );
    }
  }

  // free resources
  Continuous_done();
  deletePIDFile();
  closeLog();

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : runBatch
* Purpose: run batch mode
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors runBatch(void)
{
  Errors error;

  // read all jobs
  error = Job_rereadAll(globalOptions.jobsDirectory);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // batch mode -> run server with standard i/o
  globalOptions.runMode = RUN_MODE_BATCH;
  error = Server_batch(STDIN_FILENO,STDOUT_FILENO);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : runJob
* Purpose: run job
* Input  : jobUUIDOrName - UUID or name of job to execute
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors runJob(ConstString jobUUIDOrName)
{
  StaticString  (jobUUID,MISC_UUID_STRING_LENGTH);
  const JobNode *jobNode;
  ArchiveTypes  archiveType;
  JobOptions    jobOptions;
  StaticString  (entityUUID,MISC_UUID_STRING_LENGTH);
  Errors        error;

  // read all jobs
  error = Job_rereadAll(globalOptions.jobsDirectory);
  if (error != ERROR_NONE)
  {
    printError(_("cannot read jobs from '%s' (error: %s)!"),
               String_cString(globalOptions.jobsDirectory),
               Error_getText(error)
              );
    return error;
  }

  // get job to execute
  String_clear(jobUUID);
  archiveType = ARCHIVE_TYPE_NONE;
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,NO_WAIT)
  {
    // find job by name or UUID
    jobNode = NULL;
    if (jobNode == NULL) jobNode = Job_findByName(jobUUIDOrName);
    if (jobNode == NULL) jobNode = Job_findByUUID(jobUUIDOrName);
    if      (jobNode == NULL)
    {
      printError(_("cannot find job '%s'!"),
                 String_cString(jobUUIDOrName)
                );
      Job_listUnlock();
      return ERROR_JOB_NOT_FOUND;
    }

    // get job data
    String_set(jobUUID,jobNode->job.uuid);
    String_set(globalOptions.storageName,jobNode->job.storageName);
    EntryList_copy(&globalOptions.includeEntryList,&jobNode->job.includeEntryList,NULL,NULL);
    PatternList_copy(&globalOptions.excludePatternList,&jobNode->job.excludePatternList,NULL,NULL);
    Job_copyOptions(&jobOptions,&jobNode->job.options);
    archiveType  = jobNode->archiveType;
    jobOptions.testCreatedArchivesFlag = jobNode->testCreatedArchives;
    jobOptions.noStorage               = jobNode->noStorage;
    jobOptions.dryRun                  = jobNode->dryRun;
  }

  // start job execution
  globalOptions.runMode = RUN_MODE_INTERACTIVE;

  // create new entity UUID
  Misc_getUUID(entityUUID);

  // create archive
  error = Command_create(NULL, // masterIO
                         #ifndef NDEBUG
                           Configuration_isCommandLineOptionSet(&globalOptions.debug.indexUUID) ? String_cString(globalOptions.debug.indexUUID) : String_cString(jobUUID),
                         #else
                           String_cString(jobUUID),
                         #endif
                         NULL,  // scheduleUUID
                         NULL,  // scheduleTitle
                         #ifndef NDEBUG
                           Configuration_isCommandLineOptionSet(&globalOptions.debug.indexUUID) ? String_cString(globalOptions.debug.indexUUID) : String_cString(entityUUID),
                         #else
                           String_cString(entityUUID),
                         #endif
                         archiveType,
                         globalOptions.storageName,
                         &globalOptions.includeEntryList,
                         &globalOptions.excludePatternList,
                         NULL,  // scheduleCustomText
                         &jobOptions,
                         Misc_getCurrentDateTime(),
                         CALLBACK_(getPasswordFromConsole,NULL),
                         CALLBACK_(NULL,NULL),  // runningInfo
                         CALLBACK_(NULL,NULL),  // storageRequestVolume
                         CALLBACK_(NULL,NULL),  // isPauseCreate
                         CALLBACK_(NULL,NULL),  // isPauseStorage
                         CALLBACK_(NULL,NULL),  // isAborted
                         NULL  // logHandle
                        );
  if (error != ERROR_NONE)
  {
    Job_doneOptions(&jobOptions);
    return error;
  }
  Job_doneOptions(&jobOptions);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : runInteractive
* Purpose: run interactive
* Input  : argc - number of arguments
*          argv - arguments
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors runInteractive(int argc, const char *argv[])
{
  StaticString (entityUUID,MISC_UUID_STRING_LENGTH);
  JobOptions   jobOptions;
  Errors       error;

  if (Configuration_isCommandLineOptionSet(&globalOptions.logFileName))
  {
    // open log file
    openLog();
  }

  // get include/excluded entries from file list
  if (!String_isEmpty(globalOptions.includeFileListFileName))
  {
    error = addIncludeListFromFile(ENTRY_TYPE_FILE,&globalOptions.includeEntryList,String_cString(globalOptions.includeFileListFileName));
    if (error != ERROR_NONE)
    {
      printError(_("cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      if (Configuration_isCommandLineOptionSet(&globalOptions.logFileName)) closeLog();
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.includeImageListFileName))
  {
    error = addIncludeListFromFile(ENTRY_TYPE_IMAGE,&globalOptions.includeEntryList,String_cString(globalOptions.includeImageListFileName));
    if (error != ERROR_NONE)
    {
      printError(_("cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      if (Configuration_isCommandLineOptionSet(&globalOptions.logFileName)) closeLog();
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.excludeListFileName))
  {
    error = addExcludeListFromFile(&globalOptions.excludePatternList,String_cString(globalOptions.excludeListFileName));
    if (error != ERROR_NONE)
    {
      printError(_("cannot get excluded list (error: %s)!"),
                 Error_getText(error)
                );
      if (Configuration_isCommandLineOptionSet(&globalOptions.logFileName)) closeLog();
      return error;
    }
  }

  // get include/excluded entries from commands
  if (!String_isEmpty(globalOptions.includeFileCommand))
  {
    error = addIncludeListFromCommand(ENTRY_TYPE_FILE,&globalOptions.includeEntryList,String_cString(globalOptions.includeFileCommand));
    if (error != ERROR_NONE)
    {
      printError(_("cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      if (Configuration_isCommandLineOptionSet(&globalOptions.logFileName)) closeLog();
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.includeImageCommand))
  {
    error = addIncludeListFromCommand(ENTRY_TYPE_IMAGE,&globalOptions.includeEntryList,String_cString(globalOptions.includeImageCommand));
    if (error != ERROR_NONE)
    {
      printError(_("cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      if (Configuration_isCommandLineOptionSet(globalOptions.logFileName)) closeLog();
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.excludeCommand))
  {
    error = addExcludeListFromCommand(&globalOptions.excludePatternList,String_cString(globalOptions.excludeCommand));
    if (error != ERROR_NONE)
    {
      printError(_("cannot get excluded list (error: %s)!"),
                 Error_getText(error)
                );
      if (Configuration_isCommandLineOptionSet(&globalOptions.logFileName)) closeLog();
      return error;
    }
  }

  // interactive mode
  globalOptions.runMode = RUN_MODE_INTERACTIVE;

  // init job options
  Job_initOptions(&jobOptions);

  error = ERROR_NONE;
  switch (globalOptions.command)
  {
    case COMMAND_CREATE_FILES:
    case COMMAND_CREATE_IMAGES:
      {
        StorageSpecifier storageSpecifier;
        EntryTypes       entryType;
        int              i;

        // init varibales
        Storage_initSpecifier(&storageSpecifier);

        // get storage name
        if (argc > 1)
        {
          String_setCString(globalOptions.storageName,argv[1]);
        }
        else
        {
          printError(_("no storage name given!"));
          error = ERROR_NO_STORAGE_NAME;
        }

        // get include patterns
        if (error == ERROR_NONE)
        {
          switch (globalOptions.command)
          {
            case COMMAND_CREATE_FILES:  entryType = ENTRY_TYPE_FILE;  break;
            case COMMAND_CREATE_IMAGES: entryType = ENTRY_TYPE_IMAGE; break;
            default:                    entryType = ENTRY_TYPE_FILE;  break;
          }
          for (i = 2; i < argc; i++)
          {
            error = EntryList_appendCString(&globalOptions.includeEntryList,entryType,argv[i],globalOptions.patternType,NULL);
            if (error != ERROR_NONE)
            {
              break;
            }
          }
        }

        // parse storage name
        if (error == ERROR_NONE)
        {
          error = Storage_parseName(&storageSpecifier,globalOptions.storageName);
          if (error != ERROR_NONE)
          {
            printError(_("invalid storage '%s' (error: %s)"),
                       String_cString(globalOptions.storageName),
                       Error_getText(error)
                      );
          }
        }

        // pre-process command
        if (error == ERROR_NONE)
        {
          if (!String_isEmpty(globalOptions.preProcessScript))
          {
            TextMacros (textMacros,6);
            String     directory;

            directory = String_new();
            TEXT_MACROS_INIT(textMacros)
            {
              TEXT_MACRO_X_CSTRING ("%name",    "",                                                           NULL);
              TEXT_MACRO_X_STRING ("%archive",  globalOptions.storageName,                                    NULL);
              TEXT_MACRO_X_CSTRING("%type",     Archive_archiveTypeToString(globalOptions.archiveType),       NULL);
              TEXT_MACRO_X_CSTRING("%T",        Archive_archiveTypeToShortString(globalOptions.archiveType),  NULL);
              TEXT_MACRO_X_STRING ("%directory",File_getDirectoryName(directory,storageSpecifier.archiveName),NULL);
              TEXT_MACRO_X_STRING ("%file",     storageSpecifier.archiveName,                                 NULL);
            }
            error = executeTemplate(String_cString(globalOptions.preProcessScript),
                                    Misc_getCurrentDateTime(),
                                    textMacros.data,
                                    textMacros.count,
                                    CALLBACK_(NULL,NULL),
                                    globalOptions.commandTimeout
                                   );
            String_delete(directory);
          }
        }

        // create archive
        if (error == ERROR_NONE)
        {
          // create new entity UUID
          Misc_getUUID(entityUUID);

          // create archive
          error = Command_create(NULL, // masterIO
                                 #ifndef NDEBUG
                                   Configuration_isCommandLineOptionSet(&globalOptions.debug.indexUUID) ? String_cString(globalOptions.debug.indexUUID) : NULL,
                                 #else
                                   NULL, // job UUID
                                 #endif
                                 #ifndef NDEBUG
                                   Configuration_isCommandLineOptionSet(&globalOptions.debug.indexUUID) ? String_cString(globalOptions.debug.indexUUID) : NULL,
                                 #else
                                   NULL, // schedule UUID
                                 #endif
                                 NULL, // scheduleTitle
                                 String_cString(entityUUID),
                                 globalOptions.archiveType,
                                 globalOptions.storageName,
                                 &globalOptions.includeEntryList,
                                 &globalOptions.excludePatternList,
                                 NULL, // customText
                                 &jobOptions,
                                 Misc_getCurrentDateTime(),
                                 CALLBACK_(getPasswordFromConsole,NULL),
                                 CALLBACK_(NULL,NULL),  // runningInfo
                                 CALLBACK_(NULL,NULL),  // storageRequestVolume
                                 CALLBACK_(NULL,NULL),  // isPauseCreate
                                 CALLBACK_(NULL,NULL),  // isPauseStorage
                                 CALLBACK_(NULL,NULL),  // isAborted
                                 NULL  // logHandle
                                );
        }

        // post-process command
        if (error == ERROR_NONE)
        {
          if (!String_isEmpty(globalOptions.postProcessScript))
          {
            TextMacros (textMacros,9);
            String     directory;

            directory = String_new();
            TEXT_MACROS_INIT(textMacros)
            {
              TEXT_MACRO_X_CSTRING("%name",     "",                                                           NULL);
              TEXT_MACRO_X_STRING ("%archive",  globalOptions.storageName,                                    NULL);
              TEXT_MACRO_X_CSTRING("%type",     Archive_archiveTypeToString(globalOptions.archiveType),       NULL);
              TEXT_MACRO_X_CSTRING("%T",        Archive_archiveTypeToShortString(globalOptions.archiveType),  NULL);
              TEXT_MACRO_X_STRING ("%directory",File_getDirectoryName(directory,storageSpecifier.archiveName),NULL);
              TEXT_MACRO_X_STRING ("%file",     storageSpecifier.archiveName,                                 NULL);
              TEXT_MACRO_X_CSTRING("%state",    "",                                                           NULL);
              TEXT_MACRO_X_UINT   ("%error",    Error_getCode(error),                                         NULL);
              TEXT_MACRO_X_CSTRING("%message",  Error_getText(error),                                         NULL);
            }
            error = executeTemplate(String_cString(globalOptions.postProcessScript),
                                    Misc_getCurrentDateTime(),
                                    textMacros.data,
                                    textMacros.count,
                                    CALLBACK_(NULL,NULL),
                                    globalOptions.commandTimeout
                                   );
            String_delete(directory);
          }
        }

        // free resources
        Storage_doneSpecifier(&storageSpecifier);
      }
      break;
    case COMMAND_NONE:
    case COMMAND_LIST:
    case COMMAND_TEST:
    case COMMAND_COMPARE:
    case COMMAND_RESTORE:
    case COMMAND_CONVERT:
      {
        StorageSpecifier storageSpecifier;
        StringList       storageNameList;
        int              i;

        // init variables
        Storage_initSpecifier(&storageSpecifier);
        StringList_init(&storageNameList);

        // get storage names
        if (globalOptions.storageNameListStdin)
        {
          error = addStorageNameListFromFile(&storageNameList,NULL);
          if (error != ERROR_NONE)
          {
            printError(_("cannot get storage names (error: %s)!"),
                       Error_getText(error)
                      );
            StringList_done(&storageNameList);
            break;
          }
        }
        if (!String_isEmpty(globalOptions.storageNameListFileName))
        {
          error = addStorageNameListFromFile(&storageNameList,String_cString(globalOptions.storageNameListFileName));
          if (error != ERROR_NONE)
          {
            printError(_("cannot get storage names (error: %s)!"),
                       Error_getText(error)
                      );
            StringList_done(&storageNameList);
            break;
          }
        }
        if (!String_isEmpty(globalOptions.storageNameCommand))
        {
          error = addStorageNameListFromCommand(&storageNameList,String_cString(globalOptions.storageNameCommand));
          if (error != ERROR_NONE)
          {
            printError(_("cannot get storage names (error: %s)!"),
                       Error_getText(error)
                      );
            StringList_done(&storageNameList);
            break;
          }
        }
        for (i = 1; i < argc; i++)
        {
          StringList_appendCString(&storageNameList,argv[i]);
        }

        switch (globalOptions.command)
        {
          case COMMAND_NONE:
            // default: info/list content
            error = Command_list(&storageNameList,
                                 &globalOptions.includeEntryList,
                                 &globalOptions.excludePatternList,
                                 !globalOptions.metaInfoFlag,  // showEntriesFlag
                                 &jobOptions,
                                 CALLBACK_(getPasswordFromConsole,NULL),
                                 NULL  // logHandle
                                );
            break;
          case COMMAND_LIST:
            error = Command_list(&storageNameList,
                                 &globalOptions.includeEntryList,
                                 &globalOptions.excludePatternList,
                                 TRUE,  // showEntriesFlag
                                 &jobOptions,
                                 CALLBACK_(getPasswordFromConsole,NULL),
                                 NULL  // logHandle
                                );
            break;
          case COMMAND_TEST:
            error = Command_test(&storageNameList,
                                 &globalOptions.includeEntryList,
                                 &globalOptions.excludePatternList,
                                 &jobOptions,
                                 CALLBACK_(NULL,NULL),  // testUpdateRunningInfo
                                 CALLBACK_(getPasswordFromConsole,NULL),
                                 CALLBACK_(NULL,NULL),  // isAborted
                                 NULL  // logHandle
                                );
            break;
          case COMMAND_COMPARE:
            error = Command_compare(&storageNameList,
                                    &globalOptions.includeEntryList,
                                    &globalOptions.excludePatternList,
                                    &jobOptions,
                                    CALLBACK_(getPasswordFromConsole,NULL),
                                    NULL  // logHandle
                                   );
            break;
          case COMMAND_RESTORE:
            error = Command_restore(&storageNameList,
                                    &globalOptions.includeEntryList,
                                    &globalOptions.excludePatternList,
                                    &jobOptions,
                                    CALLBACK_(NULL,NULL),  // restoreRunningInfo
                                    CALLBACK_(NULL,NULL),  // restoreError
                                    CALLBACK_(getPasswordFromConsole,NULL),
                                    CALLBACK_(NULL,NULL),  // isPause
                                    CALLBACK_(NULL,NULL),  // isAborted
                                    NULL  // logHandle
                                   );
            break;
          case COMMAND_CONVERT:
            // create new entity UUID
            Misc_getUUID(entityUUID);

            error = Command_convert(&storageNameList,
                                    #ifndef NDEBUG
                                      Configuration_isCommandLineOptionSet(&globalOptions.jobUUIDOrName)
                                        ? String_cString(globalOptions.jobUUIDOrName)
                                        : (Configuration_isCommandLineOptionSet(&globalOptions.debug.indexUUID)
                                             ? String_cString(globalOptions.debug.indexUUID)
                                             : NULL
                                          ),
                                      Configuration_isCommandLineOptionSet(&globalOptions.newEntityUUID)
                                        ? String_cString(globalOptions.newEntityUUID)
                                        : (Configuration_isCommandLineOptionSet(&globalOptions.debug.indexUUID)
                                             ? String_cString(globalOptions.debug.indexUUID)
                                             : NULL
                                          ),
                                    #else
                                      Configuration_isCommandLineOptionSet(&globalOptions.jobUUIDOrName) ? String_cString(globalOptions.jobUUIDOrName) : NULL,
                                      Configuration_isCommandLineOptionSet(&globalOptions.newEntityUUID) ? String_cString(globalOptions.newEntityUUID) : NULL,
                                    #endif
                                    0LL,  // newCreatedDateTime
                                    &jobOptions,
                                    CALLBACK_(getPasswordFromConsole,NULL),
                                    NULL  // logHandle
                                   );
            break;
          default:
            break;
        }

        // free resources
        StringList_done(&storageNameList);
        Storage_doneSpecifier(&storageSpecifier);
      }
      break;
    case COMMAND_GENERATE_ENCRYPTION_KEYS:
      {
        // generate new key pair for asymmetric encryption
        const char *keyFileName;

        // get key file name
        keyFileName = (argc > 1) ? argv[1] : NULL;

        // generate encryption keys
        error = generateEncryptionKeys(keyFileName,&globalOptions.cryptPassword);
        if (error != ERROR_NONE)
        {
          break;
        }
      }
      break;
    case COMMAND_GENERATE_SIGNATURE_KEYS:
      {
        // generate new key pair for signature
        const char *keyFileName;

        // get key file name
        keyFileName = (argc > 1) ? argv[1] : NULL;

        // generate signature keys
        error = generateSignatureKeys(keyFileName);
        if (error != ERROR_NONE)
        {
          break;
        }
      }
      break;
    default:
      printError(_("no command given!"));
      error = ERROR_INVALID_ARGUMENT;
      break;
  }
  Job_doneOptions(&jobOptions);

  if (Configuration_isCommandLineOptionSet(&globalOptions.logFileName)) closeLog();

  return error;
}

#ifndef NDEBUG
LOCAL Errors runDebug(int argc, const char *argv[])
{
  AutoFreeList      autoFreeList;
  Errors            error;
  DatabaseSpecifier databaseSpecifier;
  String            printableDatabaseURI;
  IndexHandle       indexHandle;
  DatabaseHandle    continuousDatabaseHandle;
  uint              deletedStorageCount;
  JobOptions        jobOptions;
  StorageSpecifier  storageSpecifier;
  IndexId           entityId,storageId;
  StorageInfo       storageInfo;
  ulong             totalEntryCount;
  uint64            totalEntrySize;

  // initialize variables
  AutoFree_init(&autoFreeList);
  error = ERROR_NONE;

#if 0
  // read all jobs
  error = Job_rereadAll(globalOptions.jobsDirectory);
  if (error != ERROR_NONE)
  {
    printError(_("cannot read jobs from '%s' (error: %s)!"),
               String_cString(globalOptions.jobsDirectory),
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
#endif

  if (   globalOptions.debugFlag
      && (   !StringList_isEmpty(&globalOptions.debug.continuousNameList)
          || globalOptions.debug.showChunkIdsFlag
          || globalOptions.debug.indexWaitOperationsFlag
          || globalOptions.debug.indexPurgeDeletedStoragesFlag
          || (globalOptions.debug.indexRemoveStorage != NULL)
          || (globalOptions.debug.indexAddStorage != NULL)
          || (globalOptions.debug.indexRefreshStorage != NULL)
         )
     )
  {
    // init index database
    if (String_isEmpty(globalOptions.indexDatabaseURI))
    {
      printError("no index database!");
      AutoFree_cleanup(&autoFreeList);
      return ERROR_DATABASE;
    }
    error = Database_parseSpecifier(&databaseSpecifier,String_cString(globalOptions.indexDatabaseURI),INDEX_DEFAULT_DATABASE_NAME);
    if (error != ERROR_NONE)
    {
      printError(_("no valid database URI '%s'"),String_cString(globalOptions.indexDatabaseURI));
      AutoFree_cleanup(&autoFreeList);
      return ERROR_DATABASE;
    }
    printableDatabaseURI = Database_getPrintableName(String_new(),&databaseSpecifier,NULL);
    error = Index_init(&databaseSpecifier,CALLBACK_(NULL,NULL));
    if (error != ERROR_NONE)
    {
      printError(_("cannot init index database '%s' (error: %s)!"),
                 String_cString(printableDatabaseURI),
                 Error_getText(error)
                );
      Database_doneSpecifier(&databaseSpecifier);
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&databaseSpecifier,{ Index_done(); String_delete(printableDatabaseURI); Database_doneSpecifier(&databaseSpecifier); });

    // init continuous database
    error = Continuous_init(globalOptions.continuousDatabaseFileName);
    if (error != ERROR_NONE)
    {
      printWarning(_("continuous support is not available (reason: %s)"),Error_getText(error));
    }
    AUTOFREE_ADD(&autoFreeList,globalOptions.continuousDatabaseFileName,{ Continuous_done(); });

    // open index
    error = Index_open(&indexHandle,NULL,INDEX_TIMEOUT);
    if (error != ERROR_NONE)
    {
      printError(_("cannot open index database '%s' (error: %s)!"),
                 String_cString(printableDatabaseURI),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&indexHandle,{ Index_close(&indexHandle); });

    // open continuous database
    if (Continuous_isAvailable())
    {
      Errors error = Continuous_open(&continuousDatabaseHandle);
      if (error != ERROR_NONE)
      {
        printError(_("cannot initialize continuous database (error: %s)!"),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
      AUTOFREE_ADD(&autoFreeList,&continuousDatabaseHandle,{ Continuous_close(&continuousDatabaseHandle); });
    }

    // init job options
    Job_initOptions(&jobOptions);
    AUTOFREE_ADD(&autoFreeList,&jobOptions,{ Job_doneOptions(&jobOptions); });
  }

  if (!StringList_isEmpty(&globalOptions.debug.continuousNameList))
  {
    if (argc > 1)
    {
      String_setCString(globalOptions.storageName,argv[1]);
    }
    else
    {
      printError(_("no storage name given!"));
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_STORAGE_NAME;
    }
    error = ERROR_NONE;
    for (int i = 2; i < argc; i++)
    {
      error = EntryList_appendCString(&globalOptions.includeEntryList,ENTRY_TYPE_FILE,argv[i],globalOptions.patternType,NULL);
      if (error != ERROR_NONE)
      {
        break;
      }
    }
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return ERROR_NO_STORAGE_NAME;
    }

    StringNode *stringNode;
    String     name;
    STRINGLIST_ITERATEX(&globalOptions.debug.continuousNameList,stringNode,name,error == ERROR_NONE)
    {
      error = Continuous_addEntry(&continuousDatabaseHandle,
                                  MISC_UUID_NONE,  // jobUUID,
                                  MISC_UUID_NONE,  // scheduleUUID,
                                  NULL,  // beginTime,
                                  NULL,  // endTime,
                                  name
                                );
    }
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    error = Command_create(NULL,  // masterIO
                           MISC_UUID_NONE,  // jobUUID
                           MISC_UUID_NONE,  // scheduleUUID
                           NULL,  // scheduleTitle
                           MISC_UUID_NONE,  // entityUUID
                           ARCHIVE_TYPE_CONTINUOUS,
                           globalOptions.storageName,
                           &globalOptions.includeEntryList,
                           &globalOptions.excludePatternList,
                           NULL,  // scheduleCustomText
                           &jobOptions,
                           Misc_getCurrentDateTime(),
                           CALLBACK_(NULL,NULL),  // getPasswordFromConsole
                           CALLBACK_(NULL,NULL),  // runningInfo
                           CALLBACK_(NULL,NULL),  // storageRequestVolume
                           CALLBACK_(NULL,NULL),  // isPauseCreate
                           CALLBACK_(NULL,NULL),  // isPauseStorage
                           CALLBACK_(NULL,NULL),  // isAborted
                           NULL  // logHandle
                          );
    if (error != ERROR_NONE)
    {
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
  }

  if (globalOptions.debug.showChunkIdsFlag)
  {
    const ChunkIO CHUNK_IO_FILE =
    {
      (bool(*)(void*))File_eof,
      (Errors(*)(void*,void*,ulong,ulong*))File_read,
      (Errors(*)(void*,const void*,ulong))File_write,
      (Errors(*)(void*,uint64*))File_tell,
      (Errors(*)(void*,uint64))File_seek,
      (int64(*)(void*))File_getSize
    };

    int         i;
    FileHandle  fileHandle;
    ChunkHeader chunkHeader;

    for (i = 1; i < argc; i++)
    {
      error = File_openCString(&fileHandle,argv[i],FILE_OPEN_READ);
      if (error != ERROR_NONE)
      {
        printError(_("cannot open file '%s' (error: %s)!"),
                   argv[i],
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }

      error = ERROR_NONE;
      while (   !Chunk_eof(&CHUNK_IO_FILE,&fileHandle)
             && (error == ERROR_NONE)
            )
      {
        error = Chunk_next(&CHUNK_IO_FILE,&fileHandle,&chunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }
        printf("%s: %c%c%c%c %16"PRIu64" %8"PRIu64"\n",
               argv[i],
               chunkHeader.idChars[3],
               chunkHeader.idChars[2],
               chunkHeader.idChars[1],
               chunkHeader.idChars[0],
               chunkHeader.offset,
               chunkHeader.size
              );
        error = Chunk_skip(&CHUNK_IO_FILE,&fileHandle,&chunkHeader);
        if (error != ERROR_NONE)
        {
          break;
        }
      }
      if (error != ERROR_NONE)
      {
        printError(_("cannot list chunks of file '%s' (error: %s)!"),
                   argv[i],
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }

      (void)File_close(&fileHandle);
    }
  }

  if (globalOptions.debug.indexWaitOperationsFlag)
  {
    // wait for index opertions
    printInfo(1,"Wait index operations...");
    while (!Index_isInitialized())
    {
      Misc_udelay(1*US_PER_SECOND);
    }
    printInfo(1,"OK\n");
  }

  if (globalOptions.debug.indexPurgeDeletedStoragesFlag)
  {
    // wait until all deleted storages are purged
    printInfo(1,"Wait purge deleted storages...");
    while (Index_hasDeletedStorages(&indexHandle,&deletedStorageCount))
    {
      printInfo(1,"%5lu\b\b\b\b\b",deletedStorageCount);
      Misc_udelay(60*US_PER_SECOND);
    }
    printInfo(1,"OK   \n");
  }

  if (globalOptions.debug.indexRemoveStorage != NULL)
  {
    // remove storage from index
    Storage_initSpecifier(&storageSpecifier);
    AUTOFREE_ADD(&autoFreeList,&storageSpecifier,{ Storage_doneSpecifier(&storageSpecifier); });

    // parse storage name, get printable name
    error = Storage_parseName(&storageSpecifier,globalOptions.debug.indexRemoveStorage);
    if (error != ERROR_NONE)
    {
      printError(_("cannot parse storage name '%s' (error: %s)!"),
                 globalOptions.debug.indexRemoveStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // find storage
    if (!Index_findStorageByName(&indexHandle,
                                 &storageSpecifier,
                                 NULL,  // findArchiveName
                                 NULL,  // uuidId
                                 NULL,  // entityId
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID
                                 &storageId,
                                 NULL,  // createdDateTime
                                 NULL,  // size
                                 NULL,  // indexState,
                                 NULL,  // indexMode
                                 NULL,  // lastCheckedDateTime,
                                 NULL,  // errorMessage
                                 NULL,  // totalEntryCount
                                 NULL  // totalEntrySize
                               )
       )
    {
      printError(_("cannot find storage '%S'!"),
                 globalOptions.debug.indexRemoveStorage
                );
      AutoFree_cleanup(&autoFreeList);
      return ERROR_ARCHIVE_NOT_FOUND;
    }

    // purge storage
    error = IndexStorage_purge(&indexHandle,
                               storageId,
                               NULL  // progressInfo
                              );
    if (error != ERROR_NONE)
    {
      printError(_("cannot delete storage '%S' (error: %s)!"),
                 globalOptions.debug.indexRemoveStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    AUTOFREE_REMOVE(&autoFreeList,&storageSpecifier);
    Storage_doneSpecifier(&storageSpecifier);
  }

  if (globalOptions.debug.indexAddStorage != NULL)
  {
    // add storage to index
    Storage_initSpecifier(&storageSpecifier);
    AUTOFREE_ADD(&autoFreeList,&storageSpecifier,{ Storage_doneSpecifier(&storageSpecifier); });

    // parse storage name, get printable name
    error = Storage_parseName(&storageSpecifier,globalOptions.debug.indexAddStorage);
    if (error != ERROR_NONE)
    {
      printError(_("cannot parse storage name '%s' (error: %s)!"),
                 globalOptions.debug.indexAddStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // init storage
    error = Storage_init(&storageInfo,
                         NULL,  // masterIO
                         &storageSpecifier,
                         &jobOptions,
                         &globalOptions.indexDatabaseMaxBandWidthList,
                         SERVER_CONNECTION_PRIORITY_LOW,
                         CALLBACK_(NULL,NULL),  // storageUpdateProgress
                         CALLBACK_(NULL,NULL),  // getNamePassword
                         CALLBACK_(NULL,NULL),  // requestVolume
                         CALLBACK_(NULL,NULL),  // isPause
                         CALLBACK_(NULL,NULL),  // isAborted
                         NULL  // logHandle
                        );
    if (error != ERROR_NONE)
    {
      printError(_("cannot initialize storage '%s' (error: %s)!"),
                 globalOptions.debug.indexAddStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&storageInfo,{ Storage_done(&storageInfo); });

    // purge storage if it exists
    if (   (Index_findStorageByName(&indexHandle,
                                    &storageSpecifier,
                                    globalOptions.debug.indexAddStorage,
                                    NULL,  // uuidId,
                                    &entityId,
                                    NULL,  // jobUUID,
                                    NULL,  // scheduleUUID,
                                    &storageId,
                                    NULL,  // dateTime,
                                    NULL,  // size,
                                    NULL,  // indexState,
                                    NULL,  // indexMode,
                                    NULL,  // lastCheckedDateTime,
                                    NULL,  // errorMessage,
                                    NULL,  // totalEntryCount,
                                    NULL  // totalEntrySize
                                   )
           )
        && (INDEX_ID_EQUALS(entityId,INDEX_ID_ENTITY(globalOptions.debug.indexEntityId)))
       )
    {
      error = IndexStorage_purge(&indexHandle,
                                 storageId,
                                 NULL  // progressInfo
                                );
      if (error != ERROR_NONE)
      {
        printError(_("cannot delete storage '%s' (error: %s)!"),
                   String_cString(globalOptions.debug.indexAddStorage),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    // create entity
    if (globalOptions.debug.indexEntityId != DATABASE_ID_NONE)
    {
      if (!Index_findEntity(&indexHandle,
                            INDEX_ID_ENTITY(globalOptions.debug.indexEntityId),
                            NULL,  // findJobUUID
                            NULL,  // findScheduleUUID
                            NULL,  // findHostName
                            ARCHIVE_TYPE_ANY,
                            0LL,  // findCreatedDate
                            0L,  // findCreatedTime
                            NULL,  // jobUUID
                            NULL,  // scheduleUUID
                            NULL,  // uuidId
                            NULL,  // entityId
                            NULL,  // archiveType
                            NULL,  // createdDateTime
                            NULL,  // lastErrorMessage
                            NULL,  // totalEntryCount
                            NULL  // totalEntrySize
                           )
         )
      {
        // Note: cannot use Index_newEntity(); specific id is required
        error = Database_insert(&indexHandle.databaseHandle,
                                NULL,  // insertRowId
                                "entities",
                                DATABASE_FLAG_NONE,
                                DATABASE_VALUES
                                (
                                  DATABASE_VALUE_KEY     ("id",           globalOptions.debug.indexEntityId),
                                  DATABASE_VALUE_KEY     ("uuidId",       1),
                                  DATABASE_VALUE_CSTRING ("jobUUID",      MISC_UUID_NONE),
                                  DATABASE_VALUE_DATETIME("created",      0LL),
                                  DATABASE_VALUE_UINT    ("type",         ARCHIVE_TYPE_NORMAL),
                                  DATABASE_VALUE_UINT    ("lockedCount",  FALSE)
                                ),
                                DATABASE_COLUMNS_NONE,
                                DATABASE_FILTERS_NONE
                               );
        if (error != ERROR_NONE)
        {
          printError(_("cannot create new entity for storage '%s' (error: %s)!"),
                     String_cString(globalOptions.debug.indexAddStorage),
                     Error_getText(error)
                    );
          AutoFree_cleanup(&autoFreeList);
          return error;
        }
      }
      else if (error != ERROR_NONE)
      {
        printError(_("cannot create new entity for storage '%s' (error: %s)!"),
                   String_cString(globalOptions.debug.indexAddStorage),
                   Error_getText(error)
                  );
        AutoFree_cleanup(&autoFreeList);
        return error;
      }
    }

    // create storage
    error = Index_newStorage(&indexHandle,
                             INDEX_ID_NONE, // uuidId
                             INDEX_ID_ENTITY(globalOptions.debug.indexEntityId),
                             NULL,  // hostName
                             NULL,  // userName
                             globalOptions.debug.indexAddStorage,
                             0LL,  // createdDateTime
                             0LL,  // size
                             INDEX_STATE_UPDATE_REQUESTED,
                             INDEX_MODE_AUTO,
                             &storageId
                            );
    if (error != ERROR_NONE)
    {
      printError(_("cannot create new storage for '%s' (error: %s)!"),
                 String_cString(globalOptions.debug.indexAddStorage),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // set state 'update'
    Index_setStorageState(&indexHandle,
                          storageId,
                          INDEX_STATE_UPDATE,
                          0LL,  // lastCheckedDateTime
                          NULL  // errorMessage
                         );

    // index update
    error = Archive_updateIndex(&indexHandle,
                                INDEX_ID_NONE,
                                INDEX_ID_ENTITY(globalOptions.debug.indexEntityId),
                                storageId,
                                &storageInfo,
                                &totalEntryCount,
                                &totalEntrySize,
                                CALLBACK_(NULL,NULL),
                                CALLBACK_(NULL,NULL),
                                NULL  // logHandle
                               );

    // set index state
    if      (error == ERROR_NONE)
    {
      // done
      error = Index_setStorageState(&indexHandle,
                                    storageId,
                                    INDEX_STATE_OK,
                                    Misc_getCurrentDateTime(),
                                    NULL  // errorMessage
                                   );
    }
    else if (Error_getCode(error) == ERROR_CODE_INTERRUPTED)
    {
      // interrupt
      error = Index_setStorageState(&indexHandle,
                                    storageId,
                                    INDEX_STATE_UPDATE_REQUESTED,
                                    0LL,  // lastCheckedTimestamp
                                    NULL  // errorMessage
                                   );
    }
    else
    {
      // error
      error = Index_setStorageState(&indexHandle,
                                    storageId,
                                    INDEX_STATE_ERROR,
                                    0LL,  // lastCheckedDateTime
                                    "%s (error code: %d)",
                                    Error_getText(error),
                                    Error_getCode(error)
                                   );
    }
    if (error != ERROR_NONE)
    {
      printError(_("cannot set state of storage '%s' (error: %s)!"),
                 String_cString(globalOptions.debug.indexAddStorage),
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // done storage
    AUTOFREE_REMOVE(&autoFreeList,&storageInfo);
    (void)Storage_done(&storageInfo);

    AUTOFREE_REMOVE(&autoFreeList,&storageSpecifier);
    Storage_doneSpecifier(&storageSpecifier);
  }

  if (globalOptions.debug.indexRefreshStorage != NULL)
  {
    // refresh storage in index
    Storage_initSpecifier(&storageSpecifier);
    AUTOFREE_ADD(&autoFreeList,&storageSpecifier,{ Storage_doneSpecifier(&storageSpecifier); });

    // parse storage name, get printable name
    error = Storage_parseName(&storageSpecifier,globalOptions.debug.indexRefreshStorage);
    if (error != ERROR_NONE)
    {
      printError(_("cannot parse storage name '%s' (error: %s)!"),
                 globalOptions.debug.indexRefreshStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // find storage
    if (!Index_findStorageByName(&indexHandle,
                                 &storageSpecifier,
                                 NULL,  // findArchiveName
                                 NULL,  // uuidId
                                 NULL,  // entityId
                                 NULL,  // jobUUID
                                 NULL,  // scheduleUUID
                                 &storageId,
                                 NULL,  // createdDateTime
                                 NULL,  // size
                                 NULL,  // indexState,
                                 NULL,  // indexMode
                                 NULL,  // lastCheckedDateTime,
                                 NULL,  // errorMessage
                                 NULL,  // totalEntryCount
                                 NULL  // totalEntrySize
                                )
       )
    {
      printError(_("cannot find storage '%S'!"),
                 globalOptions.debug.indexRefreshStorage
                );
      AutoFree_cleanup(&autoFreeList);
      return ERROR_ARCHIVE_NOT_FOUND;
    }

    // init storage
    error = Storage_init(&storageInfo,
                         NULL,  // masterIO
                         &storageSpecifier,
                         &jobOptions,
                         &globalOptions.indexDatabaseMaxBandWidthList,
                         SERVER_CONNECTION_PRIORITY_LOW,
                         CALLBACK_(NULL,NULL),  // storageUpdateProgress
                         CALLBACK_(NULL,NULL),  // getNamePassword
                         CALLBACK_(NULL,NULL),  // requestVolume
                         CALLBACK_(NULL,NULL),  // isPause
                         CALLBACK_(NULL,NULL),  // isAborted
                         NULL  // logHandle
                        );
    if (error != ERROR_NONE)
    {
      printError(_("cannot initialize storage '%S' (error: %s)!"),
                 globalOptions.debug.indexRefreshStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&storageInfo,{ Storage_done(&storageInfo); });

    // set state 'update'
    Index_setStorageState(&indexHandle,
                          storageId,
                          INDEX_STATE_UPDATE,
                          0LL,  // lastCheckedDateTime
                          NULL  // errorMessage
                         );

    // index update
    error = Archive_updateIndex(&indexHandle,
                                INDEX_ID_NONE,
                                INDEX_ID_NONE,
                                storageId,
                                &storageInfo,
                                &totalEntryCount,
                                &totalEntrySize,
                                CALLBACK_(NULL,NULL),
                                CALLBACK_(NULL,NULL),
                                NULL  // logHandle
                               );

    // set index state
    if      (error == ERROR_NONE)
    {
      // done
      (void)Index_setStorageState(&indexHandle,
                                  storageId,
                                  INDEX_STATE_OK,
                                  Misc_getCurrentDateTime(),
                                  NULL  // errorMessage
                                 );
    }
    else if (Error_getCode(error) == ERROR_CODE_INTERRUPTED)
    {
      // interrupt
      (void)Index_setStorageState(&indexHandle,
                                  storageId,
                                  INDEX_STATE_UPDATE_REQUESTED,
                                  0LL,  // lastCheckedTimestamp
                                  NULL  // errorMessage
                                 );
    }
    else
    {
      // error
      (void)Index_setStorageState(&indexHandle,
                                  storageId,
                                  INDEX_STATE_ERROR,
                                  0LL,  // lastCheckedDateTime
                                  "%s (error code: %d)",
                                  Error_getText(error),
                                  Error_getCode(error)
                                 );
    }
    if (error != ERROR_NONE)
    {
      printError(_("cannot refresh storage '%S' (error: %s)!"),
                 globalOptions.debug.indexRefreshStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // done storage
    AUTOFREE_REMOVE(&autoFreeList,&storageInfo);
    (void)Storage_done(&storageInfo);

    AUTOFREE_REMOVE(&autoFreeList,&storageSpecifier);
    Storage_doneSpecifier(&storageSpecifier);
  }

  if (   globalOptions.debugFlag
      && (   !StringList_isEmpty(&globalOptions.debug.continuousNameList)
          || globalOptions.debug.showChunkIdsFlag
          || globalOptions.debug.indexWaitOperationsFlag
          || globalOptions.debug.indexPurgeDeletedStoragesFlag
          || (globalOptions.debug.indexRemoveStorage != NULL)
          || (globalOptions.debug.indexAddStorage != NULL)
          || (globalOptions.debug.indexRefreshStorage != NULL)
         )
     )
  {
    AUTOFREE_REMOVE(&autoFreeList,&jobOptions);
    Job_doneOptions(&jobOptions);
    AUTOFREE_REMOVE(&autoFreeList,&continuousDatabaseHandle);
    Continuous_close(&continuousDatabaseHandle);
    AUTOFREE_REMOVE(&autoFreeList,&indexHandle);
    Index_close(&indexHandle);
    AUTOFREE_REMOVE(&autoFreeList,globalOptions.continuousDatabaseFileName);
    Continuous_done();
    AUTOFREE_REMOVE(&autoFreeList,&databaseSpecifier);
    Index_done();
    String_delete(printableDatabaseURI);
    Database_doneSpecifier(&databaseSpecifier);
  }

  // free resources
  AutoFree_done(&autoFreeList);

  return error;
}
#endif /* NDEBUG */

/***********************************************************************\
* Name   : bar
* Purpose: BAR main program
* Input  : argc - number of arguments
*          argv - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "unicode/uclean.h"

#include "unicode/ucnv.h"
#include "unicode/udat.h"
#include "unicode/ucal.h"
LOCAL Errors bar(int argc, const char *argv[])
{
  String fileName;
  Errors error;
  bool   printInfoFlag;

  // parse command line: pre-options
  if (!CmdOption_parse(argv,&argc,
                       BAR_COMMAND_LINE_OPTIONS,
                       0,1,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    return ERROR_INVALID_ARGUMENT;
  }

  // output version, help
  if (globalOptions.versionFlag)
  {
    #ifndef NDEBUG
      printf("BAR version %s (debug)\n",VERSION_REVISION_STRING);
    #else /* NDEBUG */
      printf("BAR version %s\n",VERSION_REVISION_STRING);
    #endif /* not NDEBUG */
    printf("\n");
    printf("Components:\n");
    printf("  ICU        %s\n",!stringIsEmpty(VERSION_ICU       ) ? VERSION_ICU        : "(not included)");
    printf("  zip        %s\n",VERSION_Z);
    printf("  bzip2      %s\n",!stringIsEmpty(VERSION_BZ2       ) ? VERSION_BZ2        : "(not included)");
    printf("  lzma       %s\n",!stringIsEmpty(VERSION_LZMA      ) ? VERSION_LZMA       : "(not included)");
    printf("  lzo        %s\n",!stringIsEmpty(VERSION_LZO       ) ? VERSION_LZO        : "(not included)");
    printf("  lz4        %s\n",!stringIsEmpty(VERSION_LZ4       ) ? VERSION_LZ4        : "(not included)");
    printf("  zstd       %s\n",!stringIsEmpty(VERSION_ZSTD      ) ? VERSION_ZSTD       : "(not included)");
    printf("  xdelta3    %s\n",!stringIsEmpty(VERSION_XDELTA3   ) ? VERSION_XDELTA3    : "(not included)");
    printf("  gcrypt     %s\n",!stringIsEmpty(VERSION_GCRYPT    ) ? VERSION_GCRYPT     : "(not included)");
    printf("  gmp        %s\n",!stringIsEmpty(VERSION_GMP       ) ? VERSION_GMP        : "(not included)");
    printf("  gnuTLS     %s\n",!stringIsEmpty(VERSION_GNUTLS    ) ? VERSION_GNUTLS     : "(not included)");
    printf("  OpenSSL    %s\n",!stringIsEmpty(VERSION_OPENSSL   ) ? VERSION_OPENSSL    : "(not included)");
    printf("  libssh2    %s\n",!stringIsEmpty(VERSION_LIBSSH2   ) ? VERSION_LIBSSH2    : "(not included)");
    printf("  curl       %s\n",!stringIsEmpty(VERSION_CURL      ) ? VERSION_CURL       : "(not included)");
    printf("  libsmb2    %s\n",!stringIsEmpty(VERSION_LIBSMB2   ) ? VERSION_LIBSMB2    : "(not included)");
    printf("  cdio       %s\n",!stringIsEmpty(VERSION_CDIO      ) ? VERSION_CDIO       : "(not included)");
    printf("  PCRE       %s\n",!stringIsEmpty(VERSION_PCRE      ) ? VERSION_PCRE       : "(not included)");
    printf("  SQLite     %s\n",!stringIsEmpty(VERSION_SQLITE    ) ? VERSION_SQLITE     : "(not included)");
    printf("  MariaDB    %s\n",!stringIsEmpty(VERSION_MARIADB   ) ? VERSION_MARIADB    : "(not included)");
    printf("  PostgreSQL %s\n",!stringIsEmpty(VERSION_POSTGRESQL) ? VERSION_POSTGRESQL : "(not included)");
    printf("  PAR2       %s\n",!stringIsEmpty(VERSION_PAR2      ) ? VERSION_PAR2       : "(not included)");
    printf("  isofs      %s\n",!stringIsEmpty(VERSION_ISOFS     ) ? VERSION_ISOFS      : "(not included)");
    printf("  burn       %s\n",!stringIsEmpty(VERSION_BURN      ) ? VERSION_BURN       : "(not included)");
    printf("  mount      %s\n",!stringIsEmpty(VERSION_MOUNT     ) ? VERSION_MOUNT      : "(not included)");

    return ERROR_NONE;
  }
  if (globalOptions.helpFlag || globalOptions.xhelpFlag || globalOptions.helpInternalFlag)
  {
    if      (globalOptions.helpInternalFlag) printUsage(argv[0],2);
    else if (globalOptions.xhelpFlag       ) printUsage(argv[0],1);
    else                                     printUsage(argv[0],0);

    return ERROR_NONE;
  }

  if (!globalOptions.noDefaultConfigFlag)
  {
    fileName = String_new();

    // read default global configuration from <CONFIG_DIR>/<CONFIG_SUB_DIR>/bar.cfg (ignore errors)
    File_getSystemDirectoryCString(fileName,FILE_SYSTEM_PATH_CONFIGURATION,CONFIG_SUB_DIR FILE_SEPARATOR_STRING DEFAULT_CONFIG_FILE_NAME);
    if (File_isFile(fileName) && File_isReadable(fileName))
    {
      // add to config list
      Configuration_add(CONFIG_FILE_TYPE_AUTO,String_cString(fileName));
    }

    // read default user configuration from $HOME/.bar/bar.cfg (if exists)
    File_getSystemDirectoryCString(fileName,FILE_SYSTEM_PATH_USER_CONFIGURATION,".bar" FILE_SEPARATOR_STRING DEFAULT_CONFIG_FILE_NAME);
    if (File_isFile(fileName))
    {
      // add to config list
      Configuration_add(CONFIG_FILE_TYPE_AUTO,String_cString(fileName));
    }

    String_delete(fileName);
  }

  // parse command line: post-options
  if (!CmdOption_parse(argv,&argc,
                       BAR_COMMAND_LINE_OPTIONS,
                       2,2,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    return ERROR_INVALID_ARGUMENT;
  }

  // if daemon: print info
  printInfoFlag = !globalOptions.quietFlag && globalOptions.daemonFlag;

  // read all configuration files
  error = Configuration_readAll(isPrintInfo(2) || printInfoFlag);
  if (error != ERROR_NONE)
  {
    return error;
  }

  // special case: set verbose level/quiet flag in interactive mode
  if (!globalOptions.serverFlag && !globalOptions.daemonFlag && !globalOptions.batchFlag)
  {
    globalOptions.quietFlag    = FALSE;
    globalOptions.verboseLevel = DEFAULT_VERBOSE_LEVEL_INTERACTIVE;
  }

  // parse command line: pre+post-options
  if (!CmdOption_parse(argv,&argc,
                       BAR_COMMAND_LINE_OPTIONS,
                       0,2,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    return ERROR_INVALID_ARGUMENT;
  }

  // parse command line: all
  if (!CmdOption_parse(argv,&argc,
                       BAR_COMMAND_LINE_OPTIONS,
                       CMD_PRIORITY_ANY,CMD_PRIORITY_ANY,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    return ERROR_INVALID_ARGUMENT;
  }

  // read all server keys/certificates
  if (error == ERROR_NONE)
  {
    error = Configuration_readAllServerKeysCertificates();
    if (error != ERROR_NONE)
    {
      printError(_("cannot read server keys/certificates (error: %s)!"),
                 Error_getText(error)
                );
    }
  }

  // check parameters
  if (!Configuration_validate())
  {
    return ERROR_INVALID_ARGUMENT;
  }
  if (globalOptions.permissions != FILE_DEFAULT_PERMISSIONS)
  {
    char buffer[320];
    printWarning(_("non default restore permissions '%s' set!"),File_permissionToString(buffer,sizeof(buffer),globalOptions.permissions,TRUE));
  }

  // save configuration
  if (globalOptions.saveConfigurationFileName != NULL)
  {
    String configFileName;

    configFileName = String_newCString(globalOptions.saveConfigurationFileName);
    if (isPrintInfo(2) || printInfoFlag) { printConsole(stdout,0,"Writing configuration file '%s'...",String_cString(configFileName)); }
    error = ConfigValue_writeConfigFile(configFileName,
                                        BAR_CONFIG_VALUES,
                                        NULL,
                                        globalOptions.cleanConfigurationComments
                                       );
    if (error != ERROR_NONE)
    {
       if (isPrintInfo(2) || printInfoFlag) { printConsole(stdout,0,"FAIL!\n"); }
       String_delete(configFileName);
       return error;
    }
    if (isPrintInfo(2) || printInfoFlag) { printConsole(stdout,0,"OK\n"); }
    String_delete(configFileName);

    return ERROR_NONE;
  }

  // create temporary directory
  error = File_getTmpDirectoryName(tmpDirectory,"bar",globalOptions.tmpDirectory);
  if (error != ERROR_NONE)
  {
    printError(_("cannot create temporary directory in '%s' (error: %s)!"),
               String_cString(globalOptions.tmpDirectory),
               Error_getText(error)
              );
    return error;
  }

  // create client+worker thread pools
  if (!ThreadPool_init(&clientThreadPool,
                       "BAR client",
                       globalOptions.niceLevel,
                       4,
                       32
                      )
     )
  {
    printError(_("cannot initialize client thread pool!"));
    (void)File_delete(tmpDirectory,TRUE);
    return ERROR_INIT;
  }
  if (!ThreadPool_init(&workerThreadPool,
                       "BAR worker",
                       globalOptions.niceLevel,
                       ((globalOptions.maxThreads != 0) ? globalOptions.maxThreads : Thread_getNumberOfCores())+3,
                       MAX_UINT
                      )
     )
  {
    printError(_("cannot initialize worker thread pool!"));
    ThreadPool_done(&clientThreadPool);
    (void)File_delete(tmpDirectory,TRUE);
    return ERROR_INIT;
  }

  // debug options
  #ifndef NDEBUG
    if (globalOptions.debug.printConfigurationSHA256)
    {
      byte buffer[20];
      uint i;

      ConfigValue_debugSHA256(BAR_CONFIG_VALUES,buffer,sizeof(buffer));
      for (i = 0; i < sizeof(buffer); i++)
      {
        printf("%02x",buffer[i]);
      }
      printf("\n");

      ThreadPool_done(&clientThreadPool);
      (void)File_delete(tmpDirectory,TRUE);

      return ERROR_NONE;
    }

    if (globalOptions.debug.createSignal != 0)
    {
      #ifdef HAVE_KILL
        kill(getpid(),globalOptions.debug.createSignal);
      #endif
    }
  #endif /* NDEBUG */

  // run
  error = ERROR_NONE;
  if      (globalOptions.serverFlag || globalOptions.daemonFlag)
  {
    error = runServer();
  }
  else if (globalOptions.batchFlag)
  {
    error = runBatch();
  }
  else if (!String_isEmpty(globalOptions.jobUUIDOrName) && (globalOptions.command == COMMAND_NONE))
  {
    error = runJob(globalOptions.jobUUIDOrName);
  }
  #ifndef NDEBUG
  else if (globalOptions.debugFlag)
  {
    error = runDebug(argc,argv);
  }
  #endif /* NDEBUG */
  else
  {
    error = runInteractive(argc,argv);
  }

  // done thread pools
  ThreadPool_done(&workerThreadPool);
  ThreadPool_done(&clientThreadPool);

  // umounts
  purgeMounts(TRUE);

  // delete temporary directory
  (void)File_delete(tmpDirectory,TRUE);

  // free resources

  return error;
}

/*---------------------------------------------------------------------*/

int main(int argc, const char *argv[])
{
  Errors error;

  assert(argc >= 0);

  // init
  error = initAll();
  if (error != ERROR_NONE)
  {
    UNUSED_RESULT(fprintf(stderr,"ERROR: Cannot initialize program resources (error: %s)\n",Error_getText(error)));
    return errorToExitcode(error);
  }

  // get executable name
  File_getAbsoluteFileNameCString(globalOptions.barExecutable,argv[0]);

  // parse command line: pre-options
  if (!CmdOption_parse(argv,&argc,
                       BAR_COMMAND_LINE_OPTIONS,
                       0,0,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    doneAll();
    return EXITCODE_INVALID_ARGUMENT;
  }

  // init encoding converter
  error = initEncodingConverter(globalOptions.systemEncoding,globalOptions.consoleEncoding);
  if (error != ERROR_NONE)
  {
    printWarning(_("cannot initialize encoding (error: %s)!"),
                 Error_getText(error)
                );
  }

  // change working directory
  if (!stringIsEmpty(globalOptions.changeToDirectory))
  {
    error = File_changeDirectoryCString(globalOptions.changeToDirectory);
    if (error != ERROR_NONE)
    {
      printError(_("cannot change to directory '%s' (error: %s)!"),
                 globalOptions.changeToDirectory,
                 Error_getText(error)
                );
      doneEncodingConverter();
      doneAll();
      return errorToExitcode(error);
    }
  }

  // run bar
  if (   globalOptions.daemonFlag
      && !globalOptions.noDetachFlag
      && !globalOptions.versionFlag
      && !globalOptions.helpFlag
      && !globalOptions.xhelpFlag
      && !globalOptions.helpInternalFlag
     )
  {
    // run as service
    error = Misc_runService(bar,argc,argv);
  }
  else
  {
    // run normal
    error = bar(argc,argv);
  }

  // free resources
  doneEncodingConverter();
  doneAll();
  #ifndef NDEBUG
    UNUSED_RESULT(fprintf(stderr,
                          "DEBUG: %s exitcode %d\n",
                          (globalOptions.serverMode == SERVER_MODE_MASTER)
                            ? "master"
                            : "slave",
                          errorToExitcode(error)
                         )
                 );
  #endif /* not NDEBUG */

  return errorToExitcode(error);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
