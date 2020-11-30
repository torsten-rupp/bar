/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
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
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

#include "common/global.h"
#include "common/autofree.h"
#include "common/cmdoptions.h"
#include "common/configvalues.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/arrays.h"
#include "common/threads.h"
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
#include "index.h"
#include "continuous.h"
#if HAVE_BREAKPAD
  #include "minidump.h"
#endif /* HAVE_BREAKPAD */

#include "bar_global.h"
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

#define __VERSION_TO_STRING(z) __VERSION_TO_STRING_TMP(z)
#define __VERSION_TO_STRING_TMP(z) #z
#define VERSION_MAJOR_STRING __VERSION_TO_STRING(VERSION_MAJOR)
#define VERSION_MINOR_STRING __VERSION_TO_STRING(VERSION_MINOR)
#define VERSION_REPOSITORY_STRING __VERSION_TO_STRING(VERSION_REPOSITORY)
#define VERSION_STRING VERSION_MAJOR_STRING "." VERSION_MINOR_STRING VERSION_PATCH " (rev. " VERSION_REPOSITORY_STRING ")"

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
String                   tmpDirectory;
Semaphore                consoleLock;
#ifdef HAVE_NEWLOCALE
  locale_t               POSIXLocale;
#endif /* HAVE_NEWLOCALE */

/*---------------------------------------------------------------------*/

LOCAL String             jobUUID;             // UUID of job to execute/convert
LOCAL MountedList        mountedList;         // list of current mounts

LOCAL Semaphore          logLock;
LOCAL FILE               *logFile = NULL;     // log file handle

LOCAL ThreadLocalStorage outputLineHandle;
LOCAL String             lastOutputLine;

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
* Purpose: open log file
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void openLog(void)
{
  SEMAPHORE_LOCKED_DO(&logLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    if (globalOptions.logFileName != NULL)
    {
      logFile = fopen(globalOptions.logFileName,"a");
      if (logFile == NULL) printWarning("Cannot open log file '%s' (error: %s)!",globalOptions.logFileName,strerror(errno));
    }
  }
}

/***********************************************************************\
* Name   : closeLog
* Purpose: close log file
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
    if (globalOptions.logFileName != NULL)
    {
      fclose(logFile);
      logFile = fopen(globalOptions.logFileName,"a");
      if (logFile == NULL) printWarning("Cannot re-open log file '%s' (error: %s)!",globalOptions.logFileName,strerror(errno));
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
    signalAction.sa_flags   = 0;
    signalAction.sa_handler = SIG_DFL;
    sigaction(SIGTERM,&signalAction,NULL);
    sigaction(SIGILL,&signalAction,NULL);
    sigaction(SIGFPE,&signalAction,NULL);
    sigaction(SIGSEGV,&signalAction,NULL);
    sigaction(SIGBUS,&signalAction,NULL);
  #else /* not HAVE_SIGACTION */
    signal(SIGTERM,SIG_DFL);
    signal(SIGILL,SIG_DFL);
    signal(SIGFPE,SIG_DFL);
    signal(SIGSEGV,SIG_DFL);
    #ifdef HAVE_SIGBUS
      signal(SIGBUS,SIG_DFL);
    #endif /* HAVE_SIGBUS */
  #endif /* HAVE_SIGACTION */

  // output error message
  fprintf(stderr,"INTERNAL ERROR: signal %d\n",signalNumber);
  #ifndef NDEBUG
    debugDumpCurrentStackTrace(stderr,0,DEBUG_DUMP_STACKTRACE_OUTPUT_TYPE_NONE,1);
  #endif /* NDEBUG */

  // delete pid file
  deletePIDFile();

  // delete temporary directory (Note: do a simple validity check in case something serious went wrong...)
  if (!String_isEmpty(tmpDirectory) && !String_equalsCString(tmpDirectory,"/"))
  {
    (void)File_delete(tmpDirectory,TRUE);
  }

  // Note: do not free resources to avoid further errors

  // exit with signal number
  exit(128+signalNumber);
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
* Name   : outputConsole
* Purpose: output string to console
* Input  : file   - output stream (stdout, stderr)
*          string - string
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void outputConsole(FILE *file, ConstString string)
{
  String outputLine;
  ulong  i;
  size_t bytesWritten;
  char   ch;

  assert(file != NULL);
  assert(Semaphore_isLocked(&consoleLock));

  outputLine = (String)Thread_getLocalVariable(&outputLineHandle);
  if (outputLine != NULL)
  {
    if (File_isTerminal(file))
    {
      // restore output line if different to current line
      if (outputLine != lastOutputLine)
      {
        // wipe-out last line
        if (lastOutputLine != NULL)
        {
          for (i = 0; i < String_length(lastOutputLine); i++)
          {
            bytesWritten = fwrite("\b",1,1,file);
          }
          for (i = 0; i < String_length(lastOutputLine); i++)
          {
            bytesWritten = fwrite(" ",1,1,file);
          }
          for (i = 0; i < String_length(lastOutputLine); i++)
          {
            bytesWritten = fwrite("\b",1,1,file);
          }
          fflush(file);
        }

        // restore line
        bytesWritten = fwrite(String_cString(outputLine),1,String_length(outputLine),file);
      }

      // output new string
      bytesWritten = fwrite(String_cString(string),1,String_length(string),file);

      // store output string
      STRING_CHAR_ITERATE(string,i,ch)
      {
        switch (ch)
        {
          case '\n':
            String_clear(outputLine);
            break;
          case '\b':
            String_remove(outputLine,STRING_END,1);
            break;
          default:
            String_appendChar(outputLine,ch);
            break;
        }
      }

      lastOutputLine = outputLine;
    }
    else
    {
      if (String_index(string,STRING_END) == '\n')
      {
        if (outputLine != NULL) bytesWritten = fwrite(String_cString(outputLine),1,String_length(outputLine),file);
        bytesWritten = fwrite(String_cString(string),1,String_length(string),file);
        String_clear(outputLine);
      }
      else
      {
        String_append(outputLine,string);
      }
    }
    fflush(file);
  }
  else
  {
    // no thread local vairable -> output string
    bytesWritten = fwrite(String_cString(string),1,String_length(string),file);
  }
  UNUSED_VARIABLE(bytesWritten);
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
  printf("               cd://[<device name>:]<file name>\n");
  printf("               dvd://[<device name>:]<file name>\n");
  printf("               bd://[<device name>:]<file name>\n");
  printf("               device://[<device name>:]<file name>\n");
  printf("\n");
  CmdOption_printHelp(stdout,
                      COMMAND_LINE_OPTIONS,
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
  #if HAVE_BREAKPAD
    if (!MiniDump_init())
    {
      (void)fprintf(stderr,"Warning: Cannot initialize crash dump handler. No crash dumps will be created.\n");
    }
  #endif /* HAVE_BREAKPAD */

  // install signal handlers
  #ifdef HAVE_SIGACTION
    sigfillset(&signalAction.sa_mask);
    signalAction.sa_flags     = SA_SIGINFO;
    signalAction.sa_sigaction = signalHandler;
    sigaction(SIGSEGV,&signalAction,NULL);
    sigaction(SIGFPE,&signalAction,NULL);
    sigaction(SIGILL,&signalAction,NULL);
    sigaction(SIGTERM,&signalAction,NULL);
    sigaction(SIGUSR1,&signalAction,NULL);
    sigaction(SIGBUS,&signalAction,NULL);
  #else /* not HAVE_SIGACTION */
    signal(SIGSEGV,signalHandler);
    signal(SIGTERM,signalHandler);
    signal(SIGILL,signalHandler);
    signal(SIGFPE,signalHandler);
    signal(SIGSEGV,signalHandler);
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
  tmpDirectory = String_new();
  Semaphore_init(&consoleLock,SEMAPHORE_TYPE_BINARY);
  DEBUG_TESTCODE() { Semaphore_done(&consoleLock); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }

  Configuration_initGlobalOptions();
  uuid                                   = String_new();

  #ifdef HAVE_NEWLOCALE
    POSIXLocale                          = newlocale(LC_ALL,"POSIX",0);
  #endif /* HAVE_NEWLOCALE */

  Semaphore_init(&mountedList.lock,SEMAPHORE_TYPE_BINARY);
  List_init(&mountedList);

  jobUUID                                = String_new();

  Semaphore_init(&logLock,SEMAPHORE_TYPE_BINARY);
  logFile                                = NULL;

  Thread_initLocalVariable(&outputLineHandle,outputLineInit,NULL);
  lastOutputLine                         = NULL;

  AUTOFREE_ADD(&autoFreeList,tmpDirectory,{ String_delete(tmpDirectory); });
  AUTOFREE_ADD(&autoFreeList,&consoleLock,{ Semaphore_done(&consoleLock); });
  AUTOFREE_ADD(&autoFreeList,&globalOptions,{ Configuration_doneGlobalOptions(); });
  AUTOFREE_ADD(&autoFreeList,uuid,{ String_delete(uuid); });
  AUTOFREE_ADD(&autoFreeList,&mountedList,{ List_done(&mountedList,CALLBACK_((ListNodeFreeFunction)freeMountedNode,NULL)); });
  AUTOFREE_ADD(&autoFreeList,&mountedList.lock,{ Semaphore_done(&mountedList.lock); });
  AUTOFREE_ADD(&autoFreeList,jobUUID,{ String_delete(jobUUID); });
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
  error = Thread_initAll();
  if (error != ERROR_NONE)
  {
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  DEBUG_TESTCODE() { Thread_doneAll(); AutoFree_cleanup(&autoFreeList); return DEBUG_TESTCODE_ERROR(); }
  AUTOFREE_ADD(&autoFreeList,Thread_initAll,{ Thread_doneAll(); });

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

  // initialize command line options and config values
  ConfigValue_init(CONFIG_VALUES);
  CmdOption_init(COMMAND_LINE_OPTIONS);

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
  CmdOption_done(COMMAND_LINE_OPTIONS);
  ConfigValue_done(CONFIG_VALUES);

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
  Thread_doneAll();

  // deinitialize variables
  #ifdef HAVE_NEWLOCALE
    freelocale(POSIXLocale);
  #endif /* HAVE_NEWLOCALE */
  Semaphore_done(&logLock);

  Thread_doneLocalVariable(&outputLineHandle,outputLineDone,NULL);
  String_delete(jobUUID);

  List_done(&mountedList,CALLBACK_((ListNodeFreeFunction)freeMountedNode,NULL));
  Semaphore_done(&mountedList.lock);

  String_delete(uuid);
  Configuration_doneGlobalOptions();

  Semaphore_done(&consoleLock);
  String_delete(tmpDirectory);

  // done secure memory
  doneSecure();

  // deinstall signal handlers
  #ifdef HAVE_SIGACTION
    sigfillset(&signalAction.sa_mask);
    signalAction.sa_flags   = 0;
    signalAction.sa_handler = SIG_DFL;
    sigaction(SIGUSR1,&signalAction,NULL);
    sigaction(SIGTERM,&signalAction,NULL);
    sigaction(SIGILL,&signalAction,NULL);
    sigaction(SIGFPE,&signalAction,NULL);
    sigaction(SIGSEGV,&signalAction,NULL);
    sigaction(SIGBUS,&signalAction,NULL);
  #else /* not HAVE_SIGACTION */
    signal(SIGTERM,SIG_DFL);
    signal(SIGILL,SIG_DFL);
    signal(SIGFPE,SIG_DFL);
    signal(SIGSEGV,SIG_DFL);
    #ifdef HAVE_SIGBUS
      signal(SIGBUS,SIG_DFL);
    #endif /* HAVE_SIGBUS */
  #endif /* HAVE_SIGACTION */

  // deinitialize crash dump handler
  #if HAVE_BREAKPAD
    MiniDump_done();
  #endif /* HAVE_BREAKPAD */
}

/*---------------------------------------------------------------------*/

const char *getPasswordTypeText(PasswordTypes passwordType)
{
  const char *text;

  text = NULL;
  switch (passwordType)
  {
    case PASSWORD_TYPE_CRYPT:  text = "crypt";  break;
    case PASSWORD_TYPE_FTP:    text = "FTP";    break;
    case PASSWORD_TYPE_SSH:    text = "SSH";    break;
    case PASSWORD_TYPE_WEBDAV: text = "webDAV"; break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break;
  }

  return text;
}

void vprintInfo(uint verboseLevel, const char *prefix, const char *format, va_list arguments)
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
  ulong  i;
  size_t bytesWritten;

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
        bytesWritten = fwrite("\b",1,1,file);
      }
      for (i = 0; i < String_length(lastOutputLine); i++)
      {
        bytesWritten = fwrite(" ",1,1,file);
      }
      for (i = 0; i < String_length(lastOutputLine); i++)
      {
        bytesWritten = fwrite("\b",1,1,file);
      }
      fflush(file);
    }

    // save last line
    String_set(*saveLine,lastOutputLine);
  }

  UNUSED_VARIABLE(bytesWritten);
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
  String  saveLine;
  size_t  bytesWritten;

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
    saveConsole(stderr,&saveLine);
    bytesWritten = fwrite(String_cString(line),1,String_length(line),stderr);
    restoreConsole(stderr,&saveLine);
    UNUSED_VARIABLE(bytesWritten);
  }
  String_delete(line);
}

void printError(const char *text, ...)
{
  va_list arguments;
  String  saveLine;
  String  line;
  size_t  bytesWritten;

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
    saveConsole(stderr,&saveLine);
    bytesWritten = fwrite(String_cString(line),1,String_length(line),stderr);
    restoreConsole(stderr,&saveLine);
    UNUSED_VARIABLE(bytesWritten);
  }
  String_delete(line);
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
        dateTime = Misc_formatDateTime(String_new(),Misc_getCurrentDateTime(),globalOptions.logFormat);

        // log to session log file
        if (logHandle != NULL)
        {
          // append to job log file (if possible)
          if (logHandle->logFile != NULL)
          {
            (void)fprintf(logHandle->logFile,"%s> ",String_cString(dateTime));
            if (prefix != NULL)
            {
              (void)fputs(prefix,logHandle->logFile);
              (void)fprintf(logHandle->logFile,": ");
            }
            va_copy(tmpArguments,arguments);
            (void)vfprintf(logHandle->logFile,text,tmpArguments);
            va_end(tmpArguments);
            fputc('\n',logHandle->logFile);
          }
        }

        // log to global log file
        if (logFile != NULL)
        {
          // re-open log for log-rotation
          nowTimestamp = Misc_getTimestamp();
          if (nowTimestamp > (lastReopenTimestamp+30LL*US_PER_SECOND))
          {
            reopenLog();
            lastReopenTimestamp = nowTimestamp;
          }

          // append to log file
          (void)fprintf(logFile,"%s> ",String_cString(dateTime));
          if (prefix != NULL)
          {
            (void)fputs(prefix,logFile);
            (void)fprintf(logFile,": ");
          }
          va_copy(tmpArguments,arguments);
          (void)vfprintf(logFile,text,tmpArguments);
          va_end(tmpArguments);
          fputc('\n',logFile);
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

  if (globalOptions.logFileName != NULL)
  {
    // try to open log file if not already open
    if (logFile == NULL)
    {
      logFile = fopen(globalOptions.logFileName,"a");
      if (logFile == NULL) printWarning("Cannot re-open log file '%s' (error: %s)!",globalOptions.logFileName,strerror(errno));
    }

    if (logFile != NULL)
    {
      dateTime = Misc_formatDateTime(String_new(),Misc_getCurrentDateTime(),globalOptions.logFormat);

      // append to log file
      (void)fprintf(logFile,"%s> ",String_cString(dateTime));
      (void)fputs("FATAL: ",logFile);
      (void)fputs(text,logFile);
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

void templateInit(TemplateHandle   *templateHandle,
                  const char       *templateString,
                  ExpandMacroModes expandMacroMode,
                  uint64           dateTime
                 )
{
  assert(templateHandle != NULL);

  // init variables
  templateHandle->templateString  = templateString;
  templateHandle->expandMacroMode = expandMacroMode;
  templateHandle->dateTime        = dateTime;
  templateHandle->textMacros      = NULL;
  templateHandle->textMacroCount  = 0;
}

void templateMacros(TemplateHandle   *templateHandle,
                    const TextMacro  textMacros[],
                    uint             textMacroCount
                   )
{
  TextMacro *newTextMacros;
  uint      newTextMacroCount;

  assert(templateHandle != NULL);

  // add macros
  newTextMacroCount = templateHandle->textMacroCount+textMacroCount;
  newTextMacros = (TextMacro*)realloc((void*)templateHandle->textMacros,newTextMacroCount*sizeof(TextMacro));
  if (newTextMacros == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  memcpy(&newTextMacros[templateHandle->textMacroCount],textMacros,textMacroCount*sizeof(TextMacro));
  templateHandle->textMacros     = newTextMacros;
  templateHandle->textMacroCount = newTextMacroCount;
}

String templateDone(TemplateHandle *templateHandle,
                    String         string
                   )
{
  TextMacro *newTextMacros;
  uint      newTextMacroCount;
  #ifdef HAVE_LOCALTIME_R
    struct tm tmBuffer;
  #endif /* HAVE_LOCALTIME_R */
  struct tm *tm;
  char      buffer[256];
  uint      weekNumberU,weekNumberW;
  ulong     i;
  char      format[4];
  size_t    length;
  uint      z;

  assert(templateHandle != NULL);

  // init variables
  if (string == NULL) string = String_new();

  // get local time
  #ifdef HAVE_LOCALTIME_R
    tm = localtime_r((const time_t*)&templateHandle->dateTime,&tmBuffer);
  #else /* not HAVE_LOCALTIME_R */
    tm = localtime(&templateHandle->dateTime);
  #endif /* HAVE_LOCALTIME_R */
  assert(tm != NULL);

  // get week numbers
  strftime(buffer,sizeof(buffer)-1,"%U",tm); buffer[sizeof(buffer)-1] = '\0';
  weekNumberU = (uint)atoi(buffer);
  strftime(buffer,sizeof(buffer)-1,"%W",tm); buffer[sizeof(buffer)-1] = '\0';
  weekNumberW = (uint)atoi(buffer);

  // add week macros
  newTextMacroCount = templateHandle->textMacroCount+4;
  newTextMacros = (TextMacro*)realloc((void*)templateHandle->textMacros,newTextMacroCount*sizeof(TextMacro));
  if (newTextMacros == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+0],"%U2",(weekNumberU%2)+1,"[12]"  );
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+1],"%U4",(weekNumberU%4)+1,"[1234]");
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+2],"%W2",(weekNumberW%2)+1,"[12]"  );
  TEXT_MACRO_N_INTEGER(newTextMacros[templateHandle->textMacroCount+3],"%W4",(weekNumberW%4)+1,"[1234]");
  templateHandle->textMacros     = newTextMacros;
  templateHandle->textMacroCount = newTextMacroCount;

  // expand macros
  Misc_expandMacros(string,
                    templateHandle->templateString,
                    templateHandle->expandMacroMode,
                    templateHandle->textMacros,
                    templateHandle->textMacroCount,
                    FALSE
                   );

  // expand date/time macros, replace %% -> %
  i = 0L;
  while (i < String_length(string))
  {
    switch (String_index(string,i))
    {
      case '%':
        if ((i+1) < String_length(string))
        {
          switch (String_index(string,i+1))
          {
            case '%':
              // %% -> %
              String_remove(string,i,1);
              i += 1L;
              break;
            case 'a':
            case 'A':
            case 'b':
            case 'B':
            case 'c':
            case 'C':
            case 'd':
            case 'D':
            case 'e':
            case 'E':
            case 'F':
            case 'g':
            case 'G':
            case 'h':
            case 'H':
            case 'I':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'M':
            case 'n':
            case 'O':
            case 'p':
            case 'P':
            case 'r':
            case 'R':
            case 's':
            case 'S':
            case 't':
            case 'T':
            case 'u':
            case 'U':
            case 'V':
            case 'w':
            case 'W':
            case 'x':
            case 'X':
            case 'y':
            case 'Y':
            case 'z':
            case 'Z':
            case '+':
              // format date/time part
              switch (String_index(string,i+1))
              {
                case 'E':
                case 'O':
                  // %Ex, %Ox: extended date/time macros
                  format[0] = '%';
                  format[1] = String_index(string,i+1);
                  format[2] = String_index(string,i+2);
                  format[3] = '\0';

                  String_remove(string,i,3);
                  break;
                default:
                  // %x: date/time macros
                  format[0] = '%';
                  format[1] = String_index(string,i+1);
                  format[2] = '\0';

                  String_remove(string,i,2);
                  break;
              }
              length = strftime(buffer,sizeof(buffer)-1,format,tm); buffer[sizeof(buffer)-1] = '\0';

              // insert into string
              switch (templateHandle->expandMacroMode)
              {
                case EXPAND_MACRO_MODE_STRING:
                  String_insertBuffer(string,i,buffer,length);
                  i += length;
                  break;
                case EXPAND_MACRO_MODE_PATTERN:
                  for (z = 0 ; z < length; z++)
                  {
                    if (strchr("*+?{}():[].^$|",buffer[z]) != NULL)
                    {
                      String_insertChar(string,i,'\\');
                      i += 1L;
                    }
                    String_insertChar(string,i,buffer[z]);
                    i += 1L;
                  }
                  break;
                #ifndef NDEBUG
                  default:
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                    break; /* not reached */
                  #endif /* NDEBUG */
              }
              break;
            default:
              // keep %x
              i += 2L;
              break;
          }
        }
        else
        {
          // keep % at end of string
          i += 1L;
        }
        break;
      default:
        i += 1L;
        break;
    }
  }

  // free resources
  free((void*)templateHandle->textMacros);

  return string;
}

String expandTemplate(const char       *templateString,
                      ExpandMacroModes expandMacroMode,
                      time_t           timestamp,
                      const TextMacro  textMacros[],
                      uint             textMacroCount
                     )
{
  TemplateHandle templateHandle;

  templateInit(&templateHandle,
               templateString,
               expandMacroMode,
               timestamp
              );
  templateMacros(&templateHandle,
                 textMacros,
                 textMacroCount
                );

  return templateDone(&templateHandle,
                      NULL  // string
                     );
}

Errors executeTemplate(const char       *templateString,
                       time_t           timestamp,
                       const TextMacro  textMacros[],
                       uint             textMacroCount
                      )
{
  String script;
  Errors error;

  if (!stringIsEmpty(templateString))
  {
    script = expandTemplate(templateString,
                            EXPAND_MACRO_MODE_STRING,
                            timestamp,
                            textMacros,
                            textMacroCount
                           );
    if (!String_isEmpty(script))
    {
      // execute script
      error = Misc_executeScript(String_cString(script),
                                 CALLBACK_(executeIOOutput,NULL),
                                 CALLBACK_(executeIOOutput,NULL)
                                );
      String_delete(script);
    }
    else
    {
      error = ERROR_EXPAND_TEMPLATE;
    }
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
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
                    StorageFlags     storageFlags,
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

  assert(logHandle != NULL);
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
          TEXT_MACRO_X_STRING ("%file",   logHandle->logFileName,                            TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_STRING ("%name",   jobName,                                           TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_CSTRING("%type",   Archive_archiveTypeToString(archiveType),TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_CSTRING("%T",      Archive_archiveTypeToShortString(archiveType), ".");
          TEXT_MACRO_X_STRING ("%text",   scheduleCustomText,                                TEXT_MACRO_PATTERN_STRING);
          TEXT_MACRO_X_CSTRING("%state",  Job_getStateText(jobState,storageFlags),           NULL);
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
                                    CALLBACK_(NULL,NULL),
                                    CALLBACK_(executeIOlogPostProcess,&stderrList)
                                   );
        if (error != ERROR_NONE)
        {
          printError(_("Cannot post-process log file (error: %s)"),Error_getText(error));
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
          printWarning("Cannot re-open log file '%s' (error: %s)",String_cString(logHandle->logFileName),strerror(errno));
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
      serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,serverNode->id == serverId);
      if (serverNode == NULL)
      {
        Semaphore_unlock(&globalOptions.deviceList.lock);
        return FALSE;
      }

      // get max. number of allowed concurrent connections
      if (serverNode->maxConnectionCount != 0)
      {
        maxConnectionCount = serverNode->maxConnectionCount;
      }
      else
      {
        maxConnectionCount = 0;
        switch (serverNode->type)
        {
          case SERVER_TYPE_FILE:
            maxConnectionCount = MAX_UINT;
            break;
          case SERVER_TYPE_FTP:
            maxConnectionCount =globalOptions.defaultFTPServer.maxConnectionCount;
            break;
          case SERVER_TYPE_SSH:
            maxConnectionCount = globalOptions.defaultSSHServer.maxConnectionCount;
            break;
          case SERVER_TYPE_WEBDAV:
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
              && (serverNode->connection.count >= maxConnectionCount)
             )
          {
            // request low priority connection
            serverNode->connection.lowPriorityRequestCount++;
            Semaphore_signalModified(&globalOptions.serverList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

            // wait for free connection
            while (serverNode->connection.count >= maxConnectionCount)
            {
              if (!Semaphore_waitModified(&globalOptions.serverList.lock,timeout))
              {
                Semaphore_unlock(&globalOptions.serverList.lock);
                return FALSE;
              }
            }

            // low priority request done
            assert(serverNode->connection.lowPriorityRequestCount > 0);
            serverNode->connection.lowPriorityRequestCount--;
          }
          break;
        case SERVER_CONNECTION_PRIORITY_HIGH:
          if (   (maxConnectionCount != 0)
              && (serverNode->connection.count >= maxConnectionCount)
             )
          {
            // request high priority connection
            serverNode->connection.highPriorityRequestCount++;
            Semaphore_signalModified(&globalOptions.serverList.lock,SEMAPHORE_SIGNAL_MODIFY_ALL);

            // wait for free connection
            while (serverNode->connection.count >= maxConnectionCount)
            {
              if (!Semaphore_waitModified(&globalOptions.serverList.lock,timeout))
              {
                Semaphore_unlock(&globalOptions.serverList.lock);
                return FALSE;
              }
            }

            // high priority request done
            assert(serverNode->connection.highPriorityRequestCount > 0);
            serverNode->connection.highPriorityRequestCount--;
          }
          break;
        #ifndef NDEBUG
          default:
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
            break;
        #endif /* NDEBUG */
      }

      // allocated connection
      serverNode->connection.count++;
    }
  }

  return TRUE;
}

void freeServer(uint serverId)
{
  ServerNode *serverNode;

  if (serverId != 0)
  {
    SEMAPHORE_LOCKED_DO(&globalOptions.deviceList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
    {
      // find server
      serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,serverNode->id == serverId);
      if (serverNode != NULL)
      {
        assert(serverNode->connection.count > 0);

        // free connection
        serverNode->connection.count--;
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
      serverNode = (ServerNode*)LIST_FIND(&globalOptions.serverList,serverNode,serverNode->id == serverId);
      if (serverNode != NULL)
      {
        pendingFlag = (serverNode->connection.highPriorityRequestCount > 0);
      }
    }
  }

  return pendingFlag;
}

MountNode *newMountNode(ConstString mountName, ConstString deviceName)
{
  assert(mountName != NULL);

  return newMountNodeCString(String_cString(mountName),
                             String_cString(deviceName)
                            );
}

MountNode *newMountNodeCString(const char *mountName, const char *deviceName)
{
  MountNode *mountNode;

  assert(mountName != NULL);
  assert(!stringIsEmpty(mountName));

  // allocate mount node
  mountNode = LIST_NEW_NODE(MountNode);
  if (mountNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  mountNode->id            = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  mountNode->name          = String_newCString(mountName);
  mountNode->device        = String_newCString(deviceName);
  mountNode->mounted       = FALSE;
  mountNode->mountCount    = 0;

  return mountNode;
}

MountNode *duplicateMountNode(MountNode *fromMountNode,
                              void      *userData
                             )
{
  MountNode *mountNode;

  assert(fromMountNode != NULL);

  UNUSED_VARIABLE(userData);

  mountNode = newMountNode(fromMountNode->name,
                           fromMountNode->device
                          );
  assert(mountNode != NULL);

  return mountNode;
}

void deleteMountNode(MountNode *mountNode)
{
  assert(mountNode != NULL);

  freeMountNode(mountNode,NULL);
  LIST_DELETE_NODE(mountNode);
}

void freeMountNode(MountNode *mountNode, void *userData)
{
  assert(mountNode != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(mountNode->device);
  String_delete(mountNode->name);
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

      printError("Cannot mount '%s' (error: %s)",
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

            List_removeAndFree(&mountedList,mountedNode,CALLBACK_((ListNodeFreeFunction)freeMountedNode,NULL));
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

void purgeMounts(void)
{
  MountedNode *mountedNode;
  Errors      error;

  SEMAPHORE_LOCKED_DO(&mountedList.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    mountedNode = mountedList.head;
    while (mountedNode != NULL)
    {
      if (   (mountedNode->mountCount == 0)
          && (Misc_getTimestamp() > (mountedNode->lastMountTimestamp+MOUNT_TIMEOUT*US_PER_MS))
         )
      {
        if (Device_isMounted(mountedNode->name))
        {
          error = Device_umount(globalOptions.unmountCommand,mountedNode->name);
          if (error != ERROR_NONE)
          {
            printWarning("Cannot unmount '%s' (error: %s)",
                         String_cString(mountedNode->name),
                         Error_getText(error)
                        );
          }
        }
        mountedNode = List_removeAndFree(&mountedList,mountedNode,CALLBACK_((ListNodeFreeFunction)freeMountedNode,NULL));
      }
      else
      {
        mountedNode = mountedNode->next;
      }
    }
  }
}

Errors getCryptPasswordFromConsole(String        name,
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
        String title;
        String saveLine;

        SEMAPHORE_LOCKED_DO(&consoleLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          saveConsole(stdout,&saveLine);

          // input password
          title = String_new();
          switch (passwordType)
          {
            case PASSWORD_TYPE_CRYPT : String_format(title,"Crypt password"); break;
            case PASSWORD_TYPE_FTP   : String_format(title,"FTP password"); break;
            case PASSWORD_TYPE_SSH   : String_format(title,"SSH password"); break;
            case PASSWORD_TYPE_WEBDAV: String_format(title,"WebDAV password"); break;
          }
          if (!stringIsEmpty(text))
          {
            String_appendFormat(title,_(" for '%s'"),text);
          }
          if (!Password_input(password,String_cString(title),PASSWORD_INPUT_MODE_ANY) || (Password_length(password) <= 0))
          {
            restoreConsole(stdout,&saveLine);
            String_delete(title);
            Semaphore_unlock(&consoleLock);
            error = ERROR_NO_CRYPT_PASSWORD;
            break;
          }
          if (validateFlag)
          {
            // verify input password
            if ((text != NULL) && !stringIsEmpty(text))
            {
              String_format(title,_("Verify password for '%s'"),text);
            }
            else
            {
              String_setCString(title,"Verify password");
            }
            if (Password_inputVerify(password,String_cString(title),PASSWORD_INPUT_MODE_ANY))
            {
              error = ERROR_NONE;
            }
            else
            {
              printError(_("%s passwords are not equal!"),title);
              restoreConsole(stdout,&saveLine);
              String_delete(title);
              Semaphore_unlock(&consoleLock);
              error = ERROR_CRYPT_PASSWORDS_MISMATCH;
              break;
            }
          }
          else
          {
            error = ERROR_NONE;
          }
          String_delete(title);

          if (weakCheckFlag)
          {
            // check password quality
            if (Password_getQualityLevel(password) < MIN_PASSWORD_QUALITY_LEVEL)
            {
              printWarning(_("Low password quality!"));
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

void initStatusInfo(StatusInfo *statusInfo)
{
  assert(statusInfo != NULL);

  statusInfo->done.count          = 0L;
  statusInfo->done.size           = 0LL;
  statusInfo->total.count         = 0L;
  statusInfo->total.size          = 0LL;
  statusInfo->collectTotalSumDone = FALSE;
  statusInfo->skipped.count       = 0L;
  statusInfo->skipped.size        = 0LL;
  statusInfo->error.count         = 0L;
  statusInfo->error.size          = 0LL;
  statusInfo->archiveSize         = 0LL;
  statusInfo->compressionRatio    = 0.0;
  statusInfo->entry.name          = String_new();
  statusInfo->entry.doneSize      = 0LL;
  statusInfo->entry.totalSize     = 0LL;
  statusInfo->storage.name        = String_new();
  statusInfo->storage.doneSize    = 0LL;
  statusInfo->storage.totalSize   = 0LL;
  statusInfo->volume.number       = 0;
  statusInfo->volume.progress     = 0.0;
  statusInfo->message             = String_new();
}

void doneStatusInfo(StatusInfo *statusInfo)
{
  assert(statusInfo != NULL);

  String_delete(statusInfo->message);
  String_delete(statusInfo->storage.name);
  String_delete(statusInfo->entry.name);
}

void setStatusInfo(StatusInfo *statusInfo, const StatusInfo *fromStatusInfo)
{
  assert(statusInfo != NULL);

  statusInfo->done.count           = fromStatusInfo->done.count;
  statusInfo->done.size            = fromStatusInfo->done.size;
  statusInfo->total.count          = fromStatusInfo->total.count;
  statusInfo->total.size           = fromStatusInfo->total.size;
  statusInfo->collectTotalSumDone  = fromStatusInfo->collectTotalSumDone;
  statusInfo->skipped.count        = fromStatusInfo->skipped.count;
  statusInfo->skipped.size         = fromStatusInfo->skipped.size;
  statusInfo->error.count          = fromStatusInfo->error.count;
  statusInfo->error.size           = fromStatusInfo->error.size;
  statusInfo->archiveSize          = fromStatusInfo->archiveSize;
  statusInfo->compressionRatio     = fromStatusInfo->compressionRatio;
  String_set(statusInfo->entry.name,fromStatusInfo->entry.name);
  statusInfo->entry.doneSize       = fromStatusInfo->entry.doneSize;
  statusInfo->entry.totalSize      = fromStatusInfo->entry.totalSize;
  String_set(statusInfo->storage.name,fromStatusInfo->storage.name);
  statusInfo->storage.doneSize     = fromStatusInfo->storage.doneSize;
  statusInfo->storage.totalSize    = fromStatusInfo->storage.totalSize;
  statusInfo->volume.number        = fromStatusInfo->volume.number;
  statusInfo->volume.progress      = fromStatusInfo->volume.progress;
  String_set(statusInfo->message,fromStatusInfo->message);
}

void resetStatusInfo(StatusInfo *statusInfo)
{
  assert(statusInfo != NULL);

  statusInfo->done.count             = 0L;
  statusInfo->done.size              = 0LL;
  statusInfo->total.count            = 0L;
  statusInfo->total.size             = 0LL;
  statusInfo->collectTotalSumDone    = FALSE;
  statusInfo->skipped.count          = 0L;
  statusInfo->skipped.size           = 0LL;
  statusInfo->error.count            = 0L;
  statusInfo->error.size             = 0LL;
  statusInfo->archiveSize            = 0LL;
  statusInfo->compressionRatio       = 0.0;
  String_clear(statusInfo->entry.name);
  statusInfo->entry.doneSize         = 0LL;
  statusInfo->entry.totalSize        = 0LL;
  String_clear(statusInfo->storage.name);
  statusInfo->storage.doneSize       = 0LL;
  statusInfo->storage.totalSize      = 0LL;
  statusInfo->volume.number          = 0;
  statusInfo->volume.progress        = 0.0;
  String_clear(statusInfo->message);
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
                             CALLBACK_(NULL,NULL)
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
                             CALLBACK_(NULL,NULL)
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
                             CALLBACK_(NULL,NULL)
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

  return Pattern_match(&includeEntryNode->pattern,name,PATTERN_MATCH_MODE_BEGIN);
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
    if (Pattern_match(&entryNode->pattern,name,PATTERN_MATCH_MODE_BEGIN))
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

  assert(fileName != NULL);

  // open file
  error = File_open(&fileHandle,fileName,FILE_OPEN_READ);
  if (error != ERROR_NONE)
  {
    printError(_("Cannot open job file '%s' (error: %s)!"),
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
      (void)ConfigValue_parse(String_cString(name),
                              String_cString(value),
                              JOB_CONFIG_VALUES,
                              NULL, // sectionName,
                              LAMBDA(void,(const char *errorMessage, void *userData),
                              {
                                UNUSED_VARIABLE(userData);

                                if (printInfoFlag) printf("FAIL!\n");
                                printError("%s in %s, line %ld",errorMessage,String_cString(fileName),lineNb);
                                failFlag = TRUE;
                              }),NULL,
                              LAMBDA(void,(const char *warningMessage, void *userData),
                              {
                                UNUSED_VARIABLE(userData);

                                if (printInfoFlag) printf("FAIL!\n");
                                printWarning("%s in %s, line %ld",warningMessage,String_cString(fileName),lineNb);
                              }),NULL,
                              NULL  // variable
                             );
    }
    else
    {
      printError(_("Syntax error in '%s', line %ld: '%s' - skipped"),
                 String_cString(fileName),
                 lineNb,
                 String_cString(line)
                );
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
      printError(_("Cannot create process id file '%s' (error: %s)"),globalOptions.pidFileName,Error_getText(error));
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
  size_t   bytesWritten;

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
      printError(_("Public key file '%s' already exists!"),String_cString(publicKeyFileName));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
    if (File_exists(privateKeyFileName))
    {
      printError(_("Private key file '%s' already exists!"),String_cString(privateKeyFileName));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
  }

  // get crypt password for private key encryption
  if (Password_isEmpty(cryptPassword))
  {
    error = getCryptPasswordFromConsole(NULL,  // name
                                        cryptPassword,
                                        PASSWORD_TYPE_CRYPT,
                                        String_cString(privateKeyFileName),
                                        TRUE,  // validateFlag
                                        FALSE, // weakCheckFlag
                                        NULL  // userData
                                       );
    if (error != ERROR_NONE)
    {
      printError(_("No password given for private key!"));
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
    printError(_("Cannot create encryption key pair (error: %s)!"),Error_getText(error));
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
        error = File_makeDirectory(directoryName,FILE_DEFAULT_USER_ID,FILE_DEFAULT_GROUP_ID,FILE_DEFAULT_PERMISSION);
        if (error != ERROR_NONE)
        {
          printError(_("Cannot create directory '%s' (error: %s)!"),String_cString(directoryName),Error_getText(error));
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
      printError(_("Cannot write encryption public key file (error: %s)!"),Error_getText(error));
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
      printError(_("Cannot write encryption private key file (error: %s)!"),Error_getText(error));
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
      printError(_("Cannot get encryption public key (error: %s)!"),Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("crypt-public-key = base64:");
    bytesWritten = fwrite(data,1,dataLength,stdout);
    printf("\n");
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
      printError(_("Cannot get encryption private key (error: %s)!"),Error_getText(error));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("crypt-private-key = base64:");
    bytesWritten = fwrite(data,1,dataLength,stdout);
    printf("\n");
    freeSecure(data);
  }
  Crypt_doneKey(&privateKey);
  Crypt_doneKey(&publicKey);

  // free resources
  String_delete(privateKeyFileName);
  String_delete(publicKeyFileName);

  UNUSED_VARIABLE(bytesWritten);

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
  size_t   bytesWritten;

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
      printError(_("Public key file '%s' already exists!"),String_cString(publicKeyFileName));
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return ERROR_FILE_EXISTS_;
    }
    if (File_exists(privateKeyFileName))
    {
      printError(_("Private key file '%s' already exists!"),String_cString(privateKeyFileName));
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
    printError(_("Cannot create signature key pair (error: %s)!"),Error_getText(error));
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
        error = File_makeDirectory(directoryName,FILE_DEFAULT_USER_ID,FILE_DEFAULT_GROUP_ID,FILE_DEFAULT_PERMISSION);
        if (error != ERROR_NONE)
        {
          printError(_("Cannot create directory '%s' (error: %s)!"),String_cString(directoryName),Error_getText(error));
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
      printError(_("Cannot write signature public key file!"));
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
      printError(_("Cannot write signature private key file!"));
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
      printError(_("Cannot get signature public key!"));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("signature-public-key = base64:");
    bytesWritten = fwrite(data,1,dataLength,stdout);
    printf("\n");
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
      printError(_("Cannot get signature private key!"));
      Crypt_doneKey(&privateKey);
      Crypt_doneKey(&publicKey);
      String_delete(privateKeyFileName);
      String_delete(publicKeyFileName);
      return error;
    }
    printf("signature-private-key = base64:");
    bytesWritten = fwrite(data,1,dataLength,stdout);
    printf("\n");
    freeSecure(data);
  }
  Crypt_doneKey(&privateKey);
  Crypt_doneKey(&publicKey);

  // free resources
  String_delete(privateKeyFileName);
  String_delete(publicKeyFileName);

  UNUSED_VARIABLE(bytesWritten);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : runDaemon
* Purpose: run as daemon
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors runDaemon(void)
{
  Errors error;

  // open log file
  openLog();

  // init UUID if needed (ignore errors)
  if (String_isEmpty(uuid))
  {
    Misc_getUUID(uuid);
    (void)Configuration_update();
  }

  // create pid file
  error = createPIDFile();
  if (error != ERROR_NONE)
  {
    closeLog();
    return error;
  }

  if (Continuous_isAvailable())
  {
      // init continuous
      error = Continuous_init(globalOptions.continuousDatabaseFileName);
      if (error != ERROR_NONE)
      {
        printError(_("Cannot initialise continuous (error: %s)!"),
                   Error_getText(error)
                  );
        deletePIDFile();
        closeLog();
        return error;
      }
  }

  // daemon mode -> run server with network
  globalOptions.runMode = RUN_MODE_SERVER;

  // daemon mode -> run server with sockets
  error = Server_socket();
  if (error != ERROR_NONE)
  {
    if (Continuous_isAvailable()) Continuous_done();
    deletePIDFile();
    closeLog();
    return error;
  }

  // update config
  if (Configuration_isModified())
  {
    (void)Configuration_update();
    Configuration_clearModified();
  }

  // done continouous
  if (Continuous_isAvailable()) Continuous_done();

  // delete pid file
  deletePIDFile();

  // close log file
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

  // batch mode
  globalOptions.runMode = RUN_MODE_BATCH;

  // batch mode -> run server with standard i/o
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
* Input  : jobUUID - UUID of job to execute
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors runJob(ConstString jobUUIDName)
{
  const JobNode *jobNode;
  ArchiveTypes  archiveType;
  StorageFlags  storageFlags;
  JobOptions    jobOptions;
  Errors        error;

  // get job to execute
  archiveType  = ARCHIVE_TYPE_NONE;
  storageFlags = STORAGE_FLAGS_NONE;
  JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,NO_WAIT)
  {
    // find job by name or UUID
    jobNode = NULL;
    if (jobNode == NULL) jobNode = Job_findByName(jobUUIDName);
    if (jobNode == NULL) jobNode = Job_findByUUID(jobUUIDName);
    if      (jobNode != NULL)
    {
//      String_set(jobUUID,jobNode->job.uuid);
    }
    else if (String_matchCString(jobUUIDName,STRING_BEGIN,"[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[-0-9a-fA-F]{12}",NULL,NULL))
    {
//      String_set(jobUUID,jobUUIDName);
    }
    else
    {
      printError(_("Cannot find job '%s'!"),
                 String_cString(jobUUIDName)
                );
      Job_listUnlock();
      return ERROR_CONFIG;
    }

    // get job data
    String_set(globalOptions.storageName,jobNode->job.storageName);
    EntryList_copy(&globalOptions.includeEntryList,&jobNode->job.includeEntryList,NULL,NULL);
    PatternList_copy(&globalOptions.excludePatternList,&jobNode->job.excludePatternList,NULL,NULL);
    archiveType  = jobNode->archiveType;
    storageFlags = jobNode->storageFlags;
    Job_duplicateOptions(&jobOptions,&jobNode->job.options);
  }

  // start job execution
  globalOptions.runMode = RUN_MODE_INTERACTIVE;

  // create archive
  error = Command_create(NULL, // masterIO
                         NULL, // job UUID
                         NULL, // schedule UUID
                         NULL, // scheduleTitle
                         NULL, // scheduleCustomText
                         globalOptions.storageName,
                         &globalOptions.includeEntryList,
                         &globalOptions.excludePatternList,
                         &jobOptions,
                         archiveType,
                         Misc_getCurrentDateTime(),
                         storageFlags,
                         CALLBACK_(getCryptPasswordFromConsole,NULL),
                         CALLBACK_(NULL,NULL),  // createStatusInfoFunction
                         CALLBACK_(NULL,NULL),  // storageRequestVolumeFunction
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
  StaticString (scheduleUUID,MISC_UUID_STRING_LENGTH);
  JobOptions   jobOptions;
  Errors       error;

  // get include/excluded entries from file list
  if (!String_isEmpty(globalOptions.includeFileListFileName))
  {
    error = addIncludeListFromFile(ENTRY_TYPE_FILE,&globalOptions.includeEntryList,String_cString(globalOptions.includeFileListFileName));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.includeImageListFileName))
  {
    error = addIncludeListFromFile(ENTRY_TYPE_IMAGE,&globalOptions.includeEntryList,String_cString(globalOptions.includeImageListFileName));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.excludeListFileName))
  {
    error = addExcludeListFromFile(&globalOptions.excludePatternList,String_cString(globalOptions.excludeListFileName));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get excluded list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }

  // get include/excluded entries from commands
  if (!String_isEmpty(globalOptions.includeFileCommand))
  {
    error = addIncludeListFromCommand(ENTRY_TYPE_FILE,&globalOptions.includeEntryList,String_cString(globalOptions.includeFileCommand));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.includeImageCommand))
  {
    error = addIncludeListFromCommand(ENTRY_TYPE_IMAGE,&globalOptions.includeEntryList,String_cString(globalOptions.includeImageCommand));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get included list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }
  if (!String_isEmpty(globalOptions.excludeCommand))
  {
    error = addExcludeListFromCommand(&globalOptions.excludePatternList,String_cString(globalOptions.excludeCommand));
    if (error != ERROR_NONE)
    {
      printError(_("Cannot get excluded list (error: %s)!"),
                 Error_getText(error)
                );
      return error;
    }
  }

  // interactive mode
  globalOptions.runMode = RUN_MODE_INTERACTIVE;

  // get new schedule UUID
  Misc_getUUID(scheduleUUID);

  // init job options
  Job_initOptions(&jobOptions);

  error = ERROR_NONE;
  switch (globalOptions.command)
  {
    case COMMAND_CREATE_FILES:
    case COMMAND_CREATE_IMAGES:
      {
        EntryTypes entryType;
        int        i;

        // get storage name
        if (argc > 1)
        {
          String_setCString(globalOptions.storageName,argv[1]);
        }
        else
        {
          printError(_("No archive file name given!"));
          error = ERROR_INVALID_ARGUMENT;
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

        if (error == ERROR_NONE)
        {
          // create archive
          error = Command_create(NULL, // masterIO
                                 NULL, // job UUID
                                 scheduleUUID,
                                 NULL, // scheduleTitle
                                 NULL, // scheduleCustomText
                                 globalOptions.storageName,
                                 &globalOptions.includeEntryList,
                                 &globalOptions.excludePatternList,
                                 &jobOptions,
                                 globalOptions.archiveType,
                                 Misc_getCurrentDateTime(),
                                 globalOptions.storageFlags,
                                 CALLBACK_(getCryptPasswordFromConsole,NULL),
                                 CALLBACK_(NULL,NULL),  // createStatusInfo
                                 CALLBACK_(NULL,NULL),  // storageRequestVolume
                                 CALLBACK_(NULL,NULL),  // isPauseCreate
                                 CALLBACK_(NULL,NULL),  // isPauseStorage
                                 CALLBACK_(NULL,NULL),  // isAborted
                                 NULL  // logHandle
                                );

        }

        // free resources
      }
      break;
    case COMMAND_NONE:
    case COMMAND_LIST:
    case COMMAND_TEST:
    case COMMAND_COMPARE:
    case COMMAND_RESTORE:
    case COMMAND_CONVERT:
      {
        StringList storageNameList;
        int        i;

        // get storage names
        StringList_init(&storageNameList);
        if (globalOptions.storageNameListStdin)
        {
          error = addStorageNameListFromFile(&storageNameList,NULL);
          if (error != ERROR_NONE)
          {
            printError(_("Cannot get storage names (error: %s)!"),
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
            printError(_("Cannot get storage names (error: %s)!"),
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
            printError(_("Cannot get storage names (error: %s)!"),
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
                                 CALLBACK_(getCryptPasswordFromConsole,NULL),
                                 NULL  // logHandle
                                );
            break;
          case COMMAND_LIST:
            error = Command_list(&storageNameList,
                                 &globalOptions.includeEntryList,
                                 &globalOptions.excludePatternList,
                                 TRUE,  // showEntriesFlag
                                 &jobOptions,
                                 CALLBACK_(getCryptPasswordFromConsole,NULL),
                                 NULL  // logHandle
                                );
            break;
          case COMMAND_TEST:
            error = Command_test(&storageNameList,
                                 &globalOptions.includeEntryList,
                                 &globalOptions.excludePatternList,
                                 &jobOptions,
                                 CALLBACK_(getCryptPasswordFromConsole,NULL),
                                 NULL  // logHandle
                                );
            break;
          case COMMAND_COMPARE:
            error = Command_compare(&storageNameList,
                                    &globalOptions.includeEntryList,
                                    &globalOptions.excludePatternList,
                                    &jobOptions,
                                    CALLBACK_(getCryptPasswordFromConsole,NULL),
                                    NULL  // logHandle
                                   );
            break;
          case COMMAND_RESTORE:
            error = Command_restore(&storageNameList,
                                    &globalOptions.includeEntryList,
                                    &globalOptions.excludePatternList,
                                    &jobOptions,
                                    globalOptions.storageFlags,
                                    CALLBACK_(NULL,NULL),  // restoreStatusInfo callback
                                    CALLBACK_(NULL,NULL),  // restoreError callback
                                    CALLBACK_(getCryptPasswordFromConsole,NULL),
                                    CALLBACK_(NULL,NULL),  // isPause callback
                                    CALLBACK_(NULL,NULL),  // isAborted callback
                                    NULL  // logHandle
                                   );
            break;
          case COMMAND_CONVERT:
            error = Command_convert(&storageNameList,
                                    jobUUID,
                                    scheduleUUID,
                                    0LL,  // newCreatedDateTime
                                    &jobOptions,
                                    CALLBACK_(getCryptPasswordFromConsole,NULL),
                                    NULL  // logHandle
                                   );
            break;
          default:
            break;
        }

        // free resources
        StringList_done(&storageNameList);
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
      printError(_("No command given!"));
      error = ERROR_INVALID_ARGUMENT;
      break;
  }
  Job_doneOptions(&jobOptions);

  return error;
}

//TODO: debug version only
#ifndef WERROR
#warning remove/revert
#endif
LOCAL Errors runDebug(void)
{
  AutoFreeList     autoFreeList;
  IndexHandle      *indexHandle;
  ulong            deletedStorageCount;
  Errors           error;
  JobOptions       jobOptions;
  StorageSpecifier storageSpecifier;
  IndexId          storageId;
  StorageInfo      storageInfo;
  ulong            totalEntryCount;
  uint64           totalEntrySize;

  // initialize variables
  AutoFree_init(&autoFreeList);
  indexHandle = NULL;

  // init index database
  if (stringIsEmpty(globalOptions.indexDatabaseFileName))
  {
    printError("No index database!");
    AutoFree_cleanup(&autoFreeList);
    return ERROR_DATABASE;
  }

  error = Index_init(globalOptions.indexDatabaseFileName,CALLBACK_(NULL,NULL));
  if (error != ERROR_NONE)
  {
    printError("Cannot init index database '%s' (error: %s)!",
               globalOptions.indexDatabaseFileName,
               Error_getText(error)
              );
    AutoFree_cleanup(&autoFreeList);
    return error;
  }
  AUTOFREE_ADD(&autoFreeList,globalOptions.indexDatabaseFileName,{ Index_done(); });

  // open index
  indexHandle = NULL;
  while (indexHandle == NULL)
  {
    indexHandle = Index_open(NULL,INDEX_TIMEOUT);
  }
  AUTOFREE_ADD(&autoFreeList,indexHandle,{ Index_close(indexHandle); });

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
    while (Index_hasDeletedStorages(indexHandle,&deletedStorageCount))
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
      printError("Cannot parse storage name '%s' (error: %s)!",
                 globalOptions.debug.indexRemoveStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // find storage
    if (!Index_findStorageByName(indexHandle,
                                 &storageSpecifier,
                                 NULL,  // archiveName
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
      printError("Cannot find storage '%s' (error: %s)!",
                 globalOptions.debug.indexRemoveStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return ERROR_ARCHIVE_NOT_FOUND;
    }

    // delete storage
    error = Index_deleteStorage(indexHandle,
                                storageId
                               );
    if (error != ERROR_NONE)
    {
      printError("Cannot delete storage '%s' (error: %s)!",
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
    Job_initOptions(&jobOptions);
    AUTOFREE_ADD(&autoFreeList,&jobOptions,{ Job_doneOptions(&jobOptions); });

    // parse storage name, get printable name
    error = Storage_parseName(&storageSpecifier,globalOptions.debug.indexAddStorage);
    if (error != ERROR_NONE)
    {
      printError("Cannot parse storage name '%s' (error: %s)!",
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
                         STORAGE_FLAGS_NONE,
                         CALLBACK_(NULL,NULL),  // updateStatusInfo
                         CALLBACK_(NULL,NULL),  // getNamePassword
                         CALLBACK_(NULL,NULL),  // requestVolume
                         CALLBACK_(NULL,NULL),  // isPause
                         CALLBACK_(NULL,NULL),  // isAborted
                         NULL  // logHandle
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot initialize storage '%s' (error: %s)!",
                 globalOptions.debug.indexAddStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&storageInfo,{ Storage_done(&storageInfo); });

    // create storage
    error = Index_newStorage(indexHandle,
                             INDEX_ID_NONE, // uuidId
                             INDEX_ID_NONE, // entityId
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
      printError("Cannot create new storage '%s' (error: %s)!",
                 globalOptions.debug.indexAddStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // set state 'update'
    Index_setStorageState(indexHandle,
                          storageId,
                          INDEX_STATE_UPDATE,
                          0LL,  // lastCheckedDateTime
                          NULL  // errorMessage
                         );

    // index update
    error = Archive_updateIndex(indexHandle,
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
      (void)Index_setStorageState(indexHandle,
                                  storageId,
                                  INDEX_STATE_OK,
                                  Misc_getCurrentDateTime(),
                                  NULL  // errorMessage
                                 );
    }
    else if (Error_getCode(error) == ERROR_CODE_INTERRUPTED)
    {
      // interrupt
      (void)Index_setStorageState(indexHandle,
                                  storageId,
                                  INDEX_STATE_UPDATE_REQUESTED,
                                  0LL,  // lastCheckedTimestamp
                                  NULL  // errorMessage
                                 );
    }
    else
    {
      // error
      (void)Index_setStorageState(indexHandle,
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
      printError("Cannot create new storage '%s' (error: %s)!",
                 globalOptions.debug.indexAddStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // done storage
    AUTOFREE_REMOVE(&autoFreeList,&storageInfo);
    (void)Storage_done(&storageInfo);

    AUTOFREE_REMOVE(&autoFreeList,&jobOptions);
    Job_doneOptions(&jobOptions);
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
      printError("Cannot parse storage name '%s' (error: %s)!",
                 globalOptions.debug.indexRefreshStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }

    // find storage
    if (!Index_findStorageByName(indexHandle,
                                 &storageSpecifier,
                                 NULL,  // archiveName
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
      printError("Cannot find storage '%s' (error: %s)!",
                 globalOptions.debug.indexRefreshStorage,
                 Error_getText(error)
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
                         STORAGE_FLAGS_NONE,
                         CALLBACK_(NULL,NULL),  // updateStatusInfo
                         CALLBACK_(NULL,NULL),  // getNamePassword
                         CALLBACK_(NULL,NULL),  // requestVolume
                         CALLBACK_(NULL,NULL),  // isPause
                         CALLBACK_(NULL,NULL),  // isAborted
                         NULL  // logHandle
                        );
    if (error != ERROR_NONE)
    {
      printError("Cannot initialzie storage '%s' (error: %s)!",
                 globalOptions.debug.indexRefreshStorage,
                 Error_getText(error)
                );
      AutoFree_cleanup(&autoFreeList);
      return error;
    }
    AUTOFREE_ADD(&autoFreeList,&storageInfo,{ Storage_done(&storageInfo); });

    // set state 'update'
    Index_setStorageState(indexHandle,
                          storageId,
                          INDEX_STATE_UPDATE,
                          0LL,  // lastCheckedDateTime
                          NULL  // errorMessage
                         );

    // index update
    error = Archive_updateIndex(indexHandle,
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
      (void)Index_setStorageState(indexHandle,
                                  storageId,
                                  INDEX_STATE_OK,
                                  Misc_getCurrentDateTime(),
                                  NULL  // errorMessage
                                 );
    }
    else if (Error_getCode(error) == ERROR_CODE_INTERRUPTED)
    {
      // interrupt
      (void)Index_setStorageState(indexHandle,
                                  storageId,
                                  INDEX_STATE_UPDATE_REQUESTED,
                                  0LL,  // lastCheckedTimestamp
                                  NULL  // errorMessage
                                 );
    }
    else
    {
      // error
      (void)Index_setStorageState(indexHandle,
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
      printError("Cannot refresh storage '%s' (error: %s)!",
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

  // done index
  if (Index_isAvailable())
  {
    Index_close(indexHandle);
  }

  // free resources
  Index_done();
  AutoFree_done(&autoFreeList);

  return error;
}

/***********************************************************************\
* Name   : bar
* Purpose: BAR main program
* Input  : argc - number of arguments
*          argv - arguments
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors bar(int argc, const char *argv[])
{
  String         fileName;
  Errors         error;
  const JobNode  *jobNode;
  bool           printInfoFlag;

  // parse command line: pre-options
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,
                       0,1,
                       globalOptionSet,
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
      printf("BAR version %s (debug)\n",VERSION_STRING);
    #else /* NDEBUG */
      printf("BAR version %s\n",VERSION_STRING);
    #endif /* not NDEBUG */

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

    // read default configuration from <CONFIG_DIR>/bar.cfg (ignore errors)
    File_setFileNameCString(fileName,CONFIG_DIR);
    File_appendFileNameCString(fileName,DEFAULT_CONFIG_FILE_NAME);
    if (File_isFile(fileName) && File_isReadable(fileName))
    {
      // add to config list
      Configuration_add(CONFIG_FILE_TYPE_AUTO,String_cString(fileName));
    }

    // read default configuration from $HOME/.bar/bar.cfg (if exists)
    File_setFileNameCString(fileName,getenv("HOME"));
    File_appendFileNameCString(fileName,".bar");
    File_appendFileNameCString(fileName,DEFAULT_CONFIG_FILE_NAME);
    if (File_isFile(fileName))
    {
      // add to config list
      Configuration_add(CONFIG_FILE_TYPE_AUTO,String_cString(fileName));
    }

    String_delete(fileName);
  }

  // parse command line: post-options
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,
                       2,2,
                       globalOptionSet,
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
  if (!globalOptions.daemonFlag && !globalOptions.batchFlag)
  {
    globalOptions.quietFlag    = FALSE;
    globalOptions.verboseLevel = DEFAULT_VERBOSE_LEVEL_INTERACTIVE;
  }

  // parse command line: pre+post-options
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,
                       0,2,
                       globalOptionSet,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    return ERROR_INVALID_ARGUMENT;
  }

  if (globalOptions.serverMode == SERVER_MODE_MASTER)
  {
    // read jobs (if possible)
    (void)Job_rereadAll(globalOptions.jobsDirectory);

    // get UUID of job to execute
    if (!String_isEmpty(globalOptions.jobUUIDOrName))
    {
      JOB_LIST_LOCKED_DO(SEMAPHORE_LOCK_TYPE_READ,NO_WAIT)
      {
        // find job by name or UUID
        jobNode = NULL;
        if (jobNode == NULL) jobNode = Job_findByName(globalOptions.jobUUIDOrName);
        if (jobNode == NULL) jobNode = Job_findByUUID(globalOptions.jobUUIDOrName);
        if      (jobNode != NULL)
        {
          String_set(jobUUID,jobNode->job.uuid);
        }
        else if (String_matchCString(globalOptions.jobUUIDOrName,STRING_BEGIN,"[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[-0-9a-fA-F]{12}",NULL,NULL))
        {
          String_set(jobUUID,globalOptions.jobUUIDOrName);
        }
        else
        {
          printError(_("Cannot find job '%s'!"),
                     String_cString(globalOptions.jobUUIDOrName)
                    );
          Job_listUnlock();
          return ERROR_CONFIG;
        }
      }
    }
  }

  // parse command line: all
  if (!CmdOption_parse(argv,&argc,
                       COMMAND_LINE_OPTIONS,
                       CMD_PRIORITY_ANY,CMD_PRIORITY_ANY,
                       globalOptionSet,
                       stderr,"ERROR: ","Warning: "
                      )
     )
  {
    return ERROR_INVALID_ARGUMENT;
  }

  // check parameters
  if (!Configuration_validate())
  {
    return ERROR_INVALID_ARGUMENT;
  }

  // save configuration
  if (globalOptions.saveConfigurationFileName != NULL)
  {
    String configFileName;

    configFileName = String_newCString(globalOptions.saveConfigurationFileName);
    if (isPrintInfo(2) || printInfoFlag) { printConsole(stdout,0,"Writing configuration file '%s'...",String_cString(configFileName)); }
    error = ConfigValue_writeConfigFile(configFileName,CONFIG_VALUES);
    if (error != ERROR_NONE)
    {
       if (isPrintInfo(2) || printInfoFlag) { printConsole(stdout,0,"FAIL!\n"); }
       return error;
    }
    if (isPrintInfo(2) || printInfoFlag) { printConsole(stdout,0,"OK\n"); }
    return ERROR_NONE;
  }

  // create temporary directory
  error = File_getTmpDirectoryName(tmpDirectory,"bar",globalOptions.tmpDirectory);
  if (error != ERROR_NONE)
  {
    printError(_("Cannot create temporary directory in '%s' (error: %s)!"),
               String_cString(tmpDirectory),
               Error_getText(error)
              );
    return error;
  }

  // run
  error = ERROR_NONE;
  if      (globalOptions.daemonFlag)
  {
    error = runDaemon();
  }
  else if (globalOptions.batchFlag)
  {
    error = runBatch();
  }
  else if (!String_isEmpty(jobUUID) && (globalOptions.command == COMMAND_NONE))
  {
    error = runJob(jobUUID);
  }
  else if (   globalOptions.debug.indexWaitOperationsFlag
           || globalOptions.debug.indexPurgeDeletedStoragesFlag
           || (globalOptions.debug.indexAddStorage     != NULL)
           || (globalOptions.debug.indexRemoveStorage  != NULL)
           || (globalOptions.debug.indexRefreshStorage != NULL)
          )
  {
    error = runDebug();
  }
  else
  {
    error = runInteractive(argc,argv);
  }

  // delete temporary directory
  (void)File_delete(tmpDirectory,TRUE);

  return error;
}

/*---------------------------------------------------------------------*/

int main(int argc, const char *argv[])
{
  Errors error;

  assert(argc >= 0);

//FragmentList_unitTests(); exit(1);

  // init
  error = initAll();
  if (error != ERROR_NONE)
  {
    (void)fprintf(stderr,"ERROR: Cannot initialize program resources (error: %s)\n",Error_getText(error));
    #ifndef NDEBUG
      debugResourceDone();
      File_debugDone();
      Array_debugDone();
      String_debugDone();
      List_debugDone();
    #endif /* not NDEBUG */
    return errorToExitcode(error);
  }

  // get executable name
  File_getAbsoluteFileNameCString(globalOptions.barExecutable,argv[0]);

  error = ERROR_NONE;

  // parse command line: pre-options
  if (error == ERROR_NONE)
  {
    if (!CmdOption_parse(argv,&argc,
                         COMMAND_LINE_OPTIONS,
                         0,0,
                         globalOptionSet,
                         stderr,"ERROR: ","Warning: "
                        )
       )
    {
      error = ERROR_INVALID_ARGUMENT;
    }
  }

  // read all server keys/certificates
  if (error == ERROR_NONE)
  {
    error = Configuration_readAllServerKeys();
    if (error != ERROR_NONE)
    {
      printError(_("Cannot read server keys/certificates (error: %s)!"),
                 Error_getText(error)
                );
    }
  }

  // change working directory
  if (error == ERROR_NONE)
  {
    if (!stringIsEmpty(globalOptions.changeToDirectory))
    {
      error = File_changeDirectoryCString(globalOptions.changeToDirectory);
      if (error != ERROR_NONE)
      {
        printError(_("Cannot change to directory '%s' (error: %s)!"),
                   globalOptions.changeToDirectory,
                   Error_getText(error)
                  );
      }
    }
  }

  // run bar
  if (error == ERROR_NONE)
  {
    if (   globalOptions.daemonFlag
        && !globalOptions.noDetachFlag
        && !globalOptions.versionFlag
        && !globalOptions.helpFlag
        && !globalOptions.xhelpFlag
        && !globalOptions.helpInternalFlag
       )
    {
      // run as daemon
      #if   defined(PLATFORM_LINUX)
        // Note: do not suppress stdin/out/err for GCOV version
        #ifdef GCOV
          #define DAEMON_NO_SUPPRESS_STDIO 1
        #else /* not GCOV */
          #define DAEMON_NO_SUPPRESS_STDIO 0
        #endif /* GCOV */
        if (daemon(1,DAEMON_NO_SUPPRESS_STDIO) == 0)
        {
          error = bar(argc,argv);
        }
        else
        {
          error = ERROR_DAEMON_FAIL;
        }
      #elif defined(PLATFORM_WINDOWS)
// NYI ???
error = ERROR_STILL_NOT_IMPLEMENTED;
      #endif /* PLATFORM_... */
    }
    else
    {
      // run normal
      error = bar(argc,argv);
    }
  }

  // free resources
  doneAll();
  #ifndef NDEBUG
    debugResourceDone();
    File_debugDone();
    Array_debugDone();
    String_debugDone();
    List_debugDone();
  #endif /* not NDEBUG */

  return errorToExitcode(error);
}

#ifdef __cplusplus
  }
#endif

/* end of file */
