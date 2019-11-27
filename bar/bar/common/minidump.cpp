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
#ifdef HAVE_SYS_UTSNAME_H
  #include <sys/utsname.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#ifdef HAVE_BREAKPAD
  #if   defined(PLATFORM_LINUX)
    #include "client/linux/handler/exception_handler.h"
  #elif defined(PLATFORM_WINDOWS)
    #include "client/windows/handler/exception_handler.h"
  #endif
#endif /* HAVE_BREAKPAD */

#include "common/global.h"
#include "common/files.h"

#include "common/minidump.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define DUMP_FILENAME "bar.dump"

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
#ifdef HAVE_BREAKPAD
  LOCAL bool                                initFlag = FALSE;
  LOCAL char                                minidumpFileName[1024];
  LOCAL int                                 minidumpFileDescriptor;
  LOCAL google_breakpad::MinidumpDescriptor *minidumpDescriptor;
  LOCAL google_breakpad::ExceptionHandler   *exceptionHandler;
  LOCAL bool                                crashFlag = FALSE;
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
  size_t unused;

  UNUSED_VARIABLE(unused);

  unused = write(STDERR_FILENO,s,strlen(s));
}

LOCAL char tarBlock[512];

/***********************************************************************\
* Name   : writeTarHeader
* Purpose: write tar archive header
* Input  : tarHandle - tar file handle
*          name      - entry name
*          size      - size of data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void writeTarHeader(int tarHandle, const char *name, ulong size)
{
  // see http://www.gnu.org/software/tar/manual/tar.html#SEC183
  typedef struct
  {                              /* byte offset */
    char name[100];               /*   0 */
    char mode[8];                 /* 100 */
    char uid[8];                  /* 108 */
    char gid[8];                  /* 116 */
    char size[12];                /* 124 */
    char mtime[12];               /* 136 */
    char chksum[8];               /* 148 */
    char typeflag;                /* 156 */
    char linkname[100];           /* 157 */
    char magic[6];                /* 257 */
    char version[2];              /* 263 */
    char uname[32];               /* 265 */
    char gname[32];               /* 297 */
    char devmajor[8];             /* 329 */
    char devminor[8];             /* 337 */
    char prefix[155];             /* 345 */
    char pad[12];                 /* 500 */
  } TARHeader;

  size_t         unused;
  TARHeader      *tarHeader = (TARHeader*)tarBlock;
  struct timeval tv;
  uint           z;
  uint           chksum;

  UNUSED_VARIABLE(unused);

  // init header
  memset(tarHeader,0,sizeof(TARHeader));
  strncpy(tarHeader->name,name,sizeof(tarHeader->name));
  snprintf(tarHeader->mode,sizeof(tarHeader->mode),"%07o",0664);
  snprintf(tarHeader->size,sizeof(tarHeader->size),"%011lo",size);
  gettimeofday(&tv,NULL);
  snprintf(tarHeader->mtime,sizeof(tarHeader->mtime),"%011lo",tv.tv_sec);
  memcpy(&tarHeader->magic[0],"ustar ",sizeof(tarHeader->magic));
  tarHeader->version[0] = ' ';
  memcpy(&tarHeader->chksum[0],"        ",sizeof(tarHeader->chksum));
  tarHeader->typeflag = '0';

  // calculate checksum
  chksum = 0;
  for (z = 0; z < sizeof(tarBlock); z++)
  {
    chksum += (uint)tarBlock[z];
  }
  snprintf(tarHeader->chksum,sizeof(tarHeader->chksum),"%07o",chksum);

  // write header data
  unused = write(tarHandle,tarHeader,sizeof(TARHeader));
}

/***********************************************************************\
* Name   : addToTarArchive
* Purpose: add data block to tar archive
* Input  : handle - tar file handle
*          name   - entry name
*          buffer - data
*          size   - size of data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addToTarArchive(int tarHandle, const char *name, const void *buffer, ulong size)
{
  size_t unused;
  byte   *p;
  ulong  n;

  UNUSED_VARIABLE(unused);

  // write header
  writeTarHeader(tarHandle,name,size);

  // write data
  p = (byte*)buffer;
  while (size > 0)
  {
    n = (size > sizeof(tarBlock)) ? sizeof(tarBlock) : size;

    memcpy(&tarBlock[0],p,n);
    memset(&tarBlock[n],0,sizeof(tarBlock)-n);
    unused = write(tarHandle,tarBlock,sizeof(tarBlock));

    p += n;
    size -= n;
  }
}

/***********************************************************************\
* Name   : addFileHandleToTarArchive
* Purpose: add data from file handle to tar archive
* Input  : tarHandle - tar file handle
*          name      - entry name
*          handle    - file handle
*          size      - size of file
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addFileHandleToTarArchive(int tarHandle, const char *name, int handle, ulong size)
{
  size_t unused;
  ulong  n;

  UNUSED_VARIABLE(unused);

  // write header
  writeTarHeader(tarHandle,name,size);

  // write data
  while (size > 0)
  {
    n = (size > sizeof(tarBlock)) ? sizeof(tarBlock) : size;

    unused = read(handle,&tarBlock[0],n);
    memset(&tarBlock[n],0,sizeof(tarBlock)-n);
    unused = write(tarHandle,tarBlock,sizeof(tarBlock));

    size -= n;
  }
}

/***********************************************************************\
* Name   : addFileToTarArchive
* Purpose: add data from file to tar archive
* Input  : tarHandle - tar file handle
*          name      - entry name
*          handle    - file handle
*          fileName  - file name
* Output : -
* Return : -
* Notes  : max. 64kB
\***********************************************************************/

LOCAL void addFileToTarArchive(int tarHandle, const char *name, const char *fileName)
{
  static char buffer[64*1024];

  size_t     unused;
  int        handle;
  ulong      size;
  const char *p;
  ulong      n;

  UNUSED_VARIABLE(unused);

  // get file content
  size = 0;
  handle = open(fileName,O_RDONLY);
  if (handle == -1)
  {
    return;
  }
  size = (ulong)read(handle,buffer,sizeof(buffer));
  close(handle);

  // write header
  writeTarHeader(tarHandle,name,size);

  // write data
  p = buffer;
  while (size > 0)
  {
    n = (size > sizeof(tarBlock)) ? sizeof(tarBlock) : size;

    memcpy(&tarBlock[0],p,n);
    memset(&tarBlock[n],0,sizeof(tarBlock)-n);
    unused = write(tarHandle,tarBlock,sizeof(tarBlock));

    size -= n;
    p += n;
  }
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
  // linked in symbol file
  extern int _minidump_symbols_start __attribute__((weak));
  extern int _minidump_symbols_size __attribute__((weak));

  int    handle;
  off_t  n;
  struct utsname utsname;

  #define __TO_STRING(z) __TO_STRING_TMP(z)
  #define __TO_STRING_TMP(z) #z
  #define VERSION_MAJOR_STRING __TO_STRING(VERSION_MAJOR)
  #define VERSION_MINOR_STRING __TO_STRING(VERSION_MINOR)
  #define VERSION_REPOSITORY_STRING __TO_STRING(VERSION_REPOSITORY)
  #define VERSION_STRING VERSION_MAJOR_STRING "." VERSION_MINOR_STRING " (rev. " VERSION_REPOSITORY_STRING ")"

  UNUSED_VARIABLE(&minidumpDescriptor);
  UNUSED_VARIABLE(context);

  // Note: do not use fprintf; it does not work here

  // create crash dump file (tar archive)
  handle = open(minidumpFileName,O_CREAT|O_RDWR,0660);
  if (handle != -1)
  {
    // add mini dump file
    n = lseek(minidumpFileDescriptor,0,SEEK_END);
    lseek(minidumpFileDescriptor,0,SEEK_SET);
    addFileHandleToTarArchive(handle,"bar.mdmp",minidumpFileDescriptor,n);

    // add symbol file
    if ((&_minidump_symbols_start != NULL) && (((size_t)&_minidump_symbols_size) > 0))
    {
      addToTarArchive(handle,"bar.sym.bz2",&_minidump_symbols_start,(size_t)&_minidump_symbols_size);
    }

    // add version info
    addFileToTarArchive(handle,"osinfo.txt","/proc/version");

    // add cpu info
    addFileToTarArchive(handle,"cpuinfo.txt","/proc/cpuinfo");

    // close crash dump file
    close(handle);
  }

  // output info
  uname(&utsname);
  printString("+++ BAR CRASH DUMP ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
  printString("Dump file: "); printString(minidumpFileName); printString("\n");
  printString("Version  : "); printString(VERSION_STRING); printString("\n");
  printString("OS       : "); printString(utsname.sysname); printString(", "); printString(utsname.release); printString(", "); printString(utsname.machine);  printString("\n");
  printString("Please send the crash dump file and this information to torsten.rupp@gmx.net\n");
  printString("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

  // free resources
  close(minidumpFileDescriptor);

  // set crash flag
  crashFlag = TRUE;

//TODO?
//  if (IS_DEBUG_TESTCODE()) exit(EXITCODE_TESTCODE);
  exit(EXITCODE_TESTCODE);

  return succeeded;

  #undef __TO_STRING
  #undef __TO_STRING_TMP
  #undef VERSION_MAJOR_STRING
  #undef VERSION_MINOR_STRING
  #undef VERSION_REPOSITORY_STRING
  #undef VERSION_STRING
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
             DUMP_FILENAME
    );

    // open temporary minidump file
    minidumpFileDescriptor = open(minidumpFileName,O_CREAT|O_RDWR,0660);
    if (minidumpFileDescriptor == -1)
    {
      return FALSE;
    }
    unlink(minidumpFileName);

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

  DEBUG_TESTCODE() { volatile int *p = (int*)(NULL); (*p) = 1; }

// test crash
//{ volatile int *p = (int*)(NULL); (*p) = 1; }

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
