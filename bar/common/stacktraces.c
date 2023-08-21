/***********************************************************************\
*
* $Revision: 3843 $
* $Date: 2015-03-26 21:07:41 +0100 (Thu, 26 Mar 2015) $
* $Author: torsten $
* Contents: stacktrace functions
* Systems: Linux
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#ifdef HAVE_EXECINFO_H
  #include <execinfo.h>
#endif
#ifdef HAVE_BFD_H
  #include <bfd.h>
#endif
#if   defined(HAVE_DEMANGLE_H)
  #include <demangle.h>
#elif defined(HAVE_LIBIBERTY_DEMANGLE_H)
  #include <libiberty/demangle.h>
#endif
#ifdef HAVE_LINK_H
  #include <link.h>
#endif
#ifdef HAVE_ELF_H
  #include <elf.h>
#endif
#ifdef HAVE_DL_H
  #include <dlfcn.h>
#endif
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/cstrings.h"

#include "stacktraces.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define DEBUG_SYMBOL_FILE_EXTENSION ".sym"
#define MAX_STACKTRACE_SIZE         64
#define SKIP_STACK_FRAME_COUNT      2

/**************************** Datatypes ********************************/

#ifdef HAVE_BFD_H
// section info
typedef struct
{
  const asymbol * const *symbols;
  ulong                 symbolCount;
  bfd_vma               address;

  bool                  sectionFound;
  bool                  symbolFound;
  const char            *fileName;
  const char            *symbolName;
  uint                  lineNb;
} AddressInfo;

// file match info
typedef struct
{
  bool       found;
  const void *address;

  const char *fileName;
  void       *base;
  void       *hdr;
} FileMatchInfo;
#endif

/**************************** Variables ********************************/
#if   defined(PLATFORM_LINUX)
  LOCAL stack_t                 oldSignalHandlerStackInfo;
  LOCAL const SignalHandlerInfo *signalHandlerInfo;
  LOCAL uint                    signalHandlerInfoCount;
  LOCAL SignalHandlerFunction   signalHandlerFunction;
  LOCAL void                    *signalHandlerUserData;
  LOCAL void const              *stackTrace[MAX_STACKTRACE_SIZE+SKIP_STACK_FRAME_COUNT];
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

/****************************** Macros *********************************/

/**************************** Functions ********************************/

#if   defined(PLATFORM_LINUX)
#if defined(HAVE_BFD_INIT) && defined(HAVE_LINK_H)
/***********************************************************************\
* Name   : readSymbolTable
* Purpose: read symbol table from BFD
* Input  : abfd             - BFD handle
*          errorMessage     - error message variable (can be NULL)
*          errorMessageSize - max. size of error message
* Output : symbols      - array with symbols
*          symbolCount  - number of entries in array
*          errorMessage - error message
* Return : TRUE iff symbol table read
* Notes  : -
\***********************************************************************/

LOCAL bool readSymbolTable(bfd           *abfd,
                           const asymbol **symbols[],
                           ulong         *symbolCount,
                           char          *errorMessage,
                           uint          errorMessageSize
                          )
{
  uint size;
  long n;

  assert(symbols != NULL);
  assert(symbolCount != NULL);

  // check if symbols available
  if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
  {
    stringFormat(errorMessage,errorMessageSize,"no symbol table\n");
    return FALSE;
  }

  // read mini-symbols
  (*symbols) = NULL;
  n = bfd_read_minisymbols(abfd,
                           FALSE,  // not dynamic
                           (void**)symbols,
                           &size
                          );
  if (n == 0)
  {
    if ((*symbols) != NULL) free(*symbols);

    (*symbols) = NULL;
    n = bfd_read_minisymbols(abfd,
                             TRUE,  // dynamic
                             (void**)symbols,
                             &size
                            );
  }
  if      (n < 0)
  {
    stringFormat(errorMessage,errorMessageSize,"error reading symbols\n");
    return FALSE;
  }
  else if (n == 0)
  {
    stringFormat(errorMessage,errorMessageSize,"no symbols found\n");
    return FALSE;
  }
  (void)size;
  (*symbolCount) = (ulong)n;

  return TRUE;
}

/***********************************************************************\
* Name   : freeSymbolTable
* Purpose: free symbol table
* Input  : symbols     - symbol array
*          symbolCount - number of entries in symbol array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeSymbolTable(const asymbol *symbols[],
                           ulong         symbolCount
                          )
{
  assert(symbols != NULL);

  UNUSED_VARIABLE(symbolCount);

  free(symbols);
}

/***********************************************************************\
* Name   : demangleSymbolName
* Purpose: demangle C++ name
* Input  : symbolName              - symbol name to demangle
*          demangledSymbolNameSize - max. length of demangled symbol name
* Output : demangledSymbolName - demangled symbol name
* Return : TRUE iff name demangled
* Notes  : -
\***********************************************************************/

LOCAL bool demangleSymbolName(char       *demangledSymbolName,
                              uint       demangledSymbolNameSize,
                              const char *symbolName
                             )
{
#if defined(HAVE_LIBIBERTY_DEMANGLE_H)
  char *s;
#endif

  assert(symbolName != NULL);
  assert(demangledSymbolName != NULL);

#if defined(HAVE_LIBIBERTY_DEMANGLE_H)
  s = bfd_demangle(NULL,symbolName,DMGL_ANSI|DMGL_PARAMS);
  if (s != NULL)
  {
    stringSet(demangledSymbolName,demangledSymbolNameSize,s);
    free(s);

    return TRUE;
  }
  else
  {
    return FALSE;
  }
#else
  UNUSED_VARIABLE(symbolName);
  UNUSED_VARIABLE(demangledSymbolName);
  UNUSED_VARIABLE(demangledSymbolNameSize);

  return FALSE;
#endif
}

/***********************************************************************\
* Name   : findAddressInSection
* Purpose: callback for find symbol in section
* Input  : abfd     - BFD handle
*          section  - section
*          userData - callback user data
* Output : -
* Return : -
* Notes  : fills in data into AddressInfo structure
\***********************************************************************/

LOCAL void findAddressInSection(bfd *abfd, asection *section, void *data)
{
  AddressInfo   *addressInfo = (AddressInfo*)data;
  bfd_vma       vma;
  bfd_size_type size;

  assert(addressInfo != NULL);

  // check if already found
  if (addressInfo->symbolFound)
  {
    return;
  }

  // find section
  #ifdef HAVE_BFD_GET_SECTION_FLAGS
    if ((bfd_get_section_flags(abfd,section) & SEC_ALLOC) == 0)
    {
      return;
    }
  #else
    if ((bfd_section_flags(section) & SEC_ALLOC) == 0)
    {
      return;
    }
  #endif
  #if (BFD_SECTION_SIZE_ARGUMENTS_COUNT == 2)
    vma  = bfd_section_vma(abfd,section);
  #else
    vma  = bfd_section_vma(section);
  #endif
  #if (BFD_SECTION_VMA_ARGUMENTS_COUNT == 2)
    size = bfd_section_size(abfd,section);
  #else
    size = bfd_section_size(section);
  #endif
  if (   (addressInfo->address < vma)
      || (addressInfo->address >= vma + size)
     )
  {
    return;
  }
  addressInfo->sectionFound = TRUE;

  // find symbol
  addressInfo->symbolFound = bfd_find_nearest_line(abfd,
                                                   section,
                                                   (asymbol**)addressInfo->symbols,
                                                   addressInfo->address-vma,
                                                   &addressInfo->fileName,
                                                   &addressInfo->symbolName,
                                                   &addressInfo->lineNb
                                                  );
}

/***********************************************************************\
* Name   : addressToSymbolInfo
* Purpose: get symbol info for address
* Input  : abfd             - BFD handle
*          symbols          - symbol array
*          symbolCount      - number of entries in symbol array
*          address          - address
*          symbolFunction   - callback function for symbol
*          symbolUserData   - callback user data
*          errorMessage     - error message
*          errorMessageSize - max. size of error message
* Output : -
* Return : TRUE iff symbol information found
* Notes  : -
\***********************************************************************/

LOCAL bool addressToSymbolInfo(bfd                   *abfd,
                               const asymbol * const symbols[],
                               ulong                 symbolCount,
                               bfd_vma               address,
                               SymbolFunction        symbolFunction,
                               void                  *symbolUserData,
                               char                  *errorMessage,
                               uint                  errorMessageSize
                              )
{
  AddressInfo addressInfo;

  assert(symbolFunction != NULL);

  // initialize variables
  if (errorMessage != NULL)
  {
    stringClear(errorMessage);
  }

  // find symbol
  addressInfo.symbols      = symbols;
  addressInfo.symbolCount  = symbolCount;
  addressInfo.address      = address;
  addressInfo.sectionFound = FALSE;
  addressInfo.symbolFound  = FALSE;
  addressInfo.fileName     = NULL;
  addressInfo.symbolName   = NULL;
  addressInfo.lineNb       = 0;
  bfd_map_over_sections(abfd,findAddressInSection,&addressInfo);
  if (!addressInfo.sectionFound)
  {
    if (errorMessage != NULL)
    {
      stringFormat(errorMessage,errorMessageSize,"section not found for address 0x%016"PRIxPTR"\n",(uintptr_t)address);
    }
    return FALSE;
  }
  if (!addressInfo.symbolFound)
  {
    if (errorMessage != NULL)
    {
      stringFormat(errorMessage,errorMessageSize,"symbol not found for address 0x%016"PRIxPTR"\n",(uintptr_t)address);
    }
    return FALSE;
  }

  while (addressInfo.symbolFound)
  {
    char       buffer[256];
    const char *symbolName;
    const char *fileName;

    // get symbol data
    if ((addressInfo.symbolName != NULL) && ((*addressInfo.symbolName) != '\0'))
    {
      if (demangleSymbolName(buffer,sizeof(buffer),addressInfo.symbolName))
      {
        symbolName = buffer;
      }
      else
      {
        symbolName = addressInfo.symbolName;
      }
    }
    else
    {
      symbolName = NULL;
    }

    if (addressInfo.fileName != NULL)
    {
      fileName = addressInfo.fileName;
    }
    else
    {
      fileName = NULL;
    }

    // handle found symbol
    symbolFunction((void*)address,fileName,symbolName,addressInfo.lineNb,symbolUserData);

    // get next information
    addressInfo.symbolFound = bfd_find_inliner_info(abfd,
                                                    &addressInfo.fileName,
                                                    &addressInfo.symbolName,
                                                    &addressInfo.lineNb
                                                   );
  }

  return TRUE;
}

/***********************************************************************\
* Name   : openBFD
* Purpose: open BFD and read symbol table
* Input  : fileName         - file name
*          errorMessage     - error message variable (can be NULL)
*          errorMessageSize - max. size of error message
* Output : symbols      - symbol array
*          symbolCount  - number of entries in symbol array
*          errorMessage - error mesreadSymbolTablesage
* Return : TRUE iff BFD opened
* Notes  : -
\***********************************************************************/

LOCAL bfd* openBFD(const char    *fileName,
                   const asymbol **symbols[],
                   ulong         *symbolCount,
                   char          *errorMessage,
                   uint          errorMessageSize
                  )
{
  bfd  *abfd;
  char **matching;

  assert(fileName != NULL);
  assert(symbols != NULL);
  assert(symbolCount != NULL);

  abfd = bfd_openr(fileName,NULL);
  if (abfd == NULL)
  {
    return NULL;
  }

  if (bfd_check_format(abfd,bfd_archive))
  {
    stringFormat(errorMessage,errorMessageSize,"invalid format of file '%s' (error: %s)\n",fileName,strerror(errno));
    bfd_close(abfd);
    return NULL;
  }

  if (!bfd_check_format_matches(abfd,bfd_object,&matching))
  {
    if (bfd_get_error() == bfd_error_file_ambiguously_recognized)
    {
      free(matching);
    }
    stringFormat(errorMessage,errorMessageSize,"format does not match for file file '%s'\n",fileName);
    bfd_close(abfd);
    return NULL;
  }

  if (!readSymbolTable(abfd,symbols,symbolCount,errorMessage,errorMessageSize))
  {
    bfd_close(abfd);
    return NULL;
  }

  return abfd;
}

/***********************************************************************\
* Name   : closeBFD
* Purpose: close BFD
* Input  : abfd - BFD handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void closeBFD(bfd           *abfd,
                    const asymbol *symbols[],
                    ulong         symbolCount
                   )
{
  assert(abfd != NULL);

  freeSymbolTable(symbols,symbolCount);
  bfd_close(abfd);
}

/***********************************************************************\
* Name   : getSymbolInfoFromFile
* Purpose: get symbol information from file
* Input  : fileName         - file name
*          address          - address
*          symbolFunction   - callback function for symbol
*          symbolUserData   - callback user data
*          errorMessage     - error message variable (can be NULL)
*          errorMessageSize - max. size of error message
* Output : errorMessage - error message
* Return : TRUE iff symbol read
* Notes  : -
\***********************************************************************/

LOCAL bool getSymbolInfoFromFile(const char     *fileName,
                                 bfd_vma        address,
                                 SymbolFunction symbolFunction,
                                 void           *symbolUserData,
                                 char           *errorMessage,
                                 uint           errorMessageSize
                                )
{
  char          debugSymbolFileName[PATH_MAX];
  ssize_t       n;
  bfd*          abfd;
  const asymbol **symbols;
  ulong         symbolCount;
  bool          result;

  assert(fileName != NULL);
//TODO: open once
//fprintf(stderr,"%s, %d: %s %p\n",__FILE__,__LINE__,fileName,address);

  // open file.<debug symbol extension> or file
  abfd = NULL;
  if (abfd == NULL)
  {
    n = readlink(fileName,debugSymbolFileName,sizeof(debugSymbolFileName));
    if (n > 0)
    {
      debugSymbolFileName[n] = '\0';
      stringAppend(debugSymbolFileName,sizeof(debugSymbolFileName),DEBUG_SYMBOL_FILE_EXTENSION);
      abfd = openBFD(debugSymbolFileName,&symbols,&symbolCount,errorMessage,errorMessageSize);
    }
  }
  if (abfd == NULL)
  {
    abfd = openBFD(fileName,&symbols,&symbolCount,errorMessage,errorMessageSize);
  }
  if (abfd == NULL)
  {
    return FALSE;
  }

  // get symbol info
  result = addressToSymbolInfo(abfd,
                               symbols,
                               symbolCount,
                               address,
                               symbolFunction,
                               symbolUserData,
                               errorMessage,
                               errorMessageSize
                              );

  // close file
  closeBFD(abfd,symbols,symbolCount);

  return result;
}

/***********************************************************************\
* Name   : findMatchingFile
* Purpose: callback for find address in loaded shared libraries
* Input  : info     - dynamic object info
*          size     -
*          userData - callback user data
* Output : -
* Return : always 0 (not used)
* Notes  : fills in data into FileMatchInfo structure
\***********************************************************************/

LOCAL int findMatchingFile(struct dl_phdr_info *info,
                           size_t              infoSize,
                           void                *userData
                          )
{
  FileMatchInfo *fileMatchInfo = (FileMatchInfo*)userData;

  ElfW(Half) i;
  ElfW(Addr) vaddr;
  #ifdef HAVE_DL_H
    Dl_info    dlInfo;
  #endif

  assert(info != NULL);
  assert(fileMatchInfo != NULL);

  // unused
  (void)infoSize;

  // find in shared objects
  for (i = 0; i < info->dlpi_phnum; i++)
  {
    if (info->dlpi_phdr[i].p_type == PT_LOAD)
    {
      vaddr = info->dlpi_addr + info->dlpi_phdr[i].p_vaddr;
      if (   ((uintptr_t)fileMatchInfo->address >= vaddr)
          && ((uintptr_t)fileMatchInfo->address < vaddr + info->dlpi_phdr[i].p_memsz)
          && (info->dlpi_name != NULL)
          && (info->dlpi_name[0] != '\0')
         )
      {
        fileMatchInfo->found    = TRUE;
        fileMatchInfo->fileName = info->dlpi_name;
        fileMatchInfo->base     = (void*)(uintptr_t)info->dlpi_addr;
      }
    }
  }

  #ifdef HAVE_DL_H
    // find via loader
    if (!fileMatchInfo->found)
    {
      if (   (dladdr(fileMatchInfo->address, &dlInfo) != 0)
          && (dlInfo.dli_fname != NULL)
          && (dlInfo.dli_fname[0] != '\0')
         )
      {
        fileMatchInfo->found    = TRUE;
        fileMatchInfo->fileName = dlInfo.dli_fname;
        fileMatchInfo->base     = (void*)(uintptr_t)dlInfo.dli_fbase;
      }
    }
  #endif

  return 0; // return value not used
}
#endif // HAVE_BFD_INIT

LOCAL void sigActionHandler(int signalNumber, siginfo_t *sigInfo, void *context)
{
   int        stackTraceSize;
   uint       i;
   const char *signalName;

   assert(NULL != signalHandlerFunction);

   (void)sigInfo;
   (void)context;

   // done signal handlers
   Stacktrace_done();

   // get stacktrace
   stackTraceSize = getStackTrace(stackTrace, MAX_STACKTRACE_SIZE);

   // get signal name
   signalName = NULL;
   i          = 0;
   while ((i < signalHandlerInfoCount) && (NULL == signalName))
   {
      if (signalHandlerInfo[i].signalNumber == signalNumber)
      {
         signalName = signalHandlerInfo[i].signalName;
      }
      i++;
   }
   if (NULL == signalName)
   {
      signalName = "unknown";
   }

   // call signal handler
   if (stackTraceSize >= SKIP_STACK_FRAME_COUNT)
   {
      signalHandlerFunction(signalNumber,
                            signalName,
                            (void const**)&stackTrace[SKIP_STACK_FRAME_COUNT],
                            (uint)stackTraceSize-SKIP_STACK_FRAME_COUNT,
                            signalHandlerUserData
                           );
   }
}
#elif defined(PLATFORM_WINDOWS)
#endif /* PLATFORM_... */

// ---------------------------------------------------------------------

void Stacktrace_init(const SignalHandlerInfo *signalHandlerInfo,
                     uint                    signalHandlerInfoCount,
                     SignalHandlerFunction   signalHandlerFunction,
                     void                    *signalHandlerUserData
                    )
{
  #if   defined(PLATFORM_LINUX)
    static uint8_t signalHandlerStack[64*1024];

    stack_t          stackInfo;
    struct sigaction signalActionInfo;
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  assert(signalHandlerInfo != NULL);

  #if   defined(PLATFORM_LINUX)
    // initialize signal handler stack
    stackInfo.ss_sp    = (void*)signalHandlerStack;
    stackInfo.ss_size  = sizeof(signalHandlerStack)/sizeof(signalHandlerStack[0]);
    stackInfo.ss_flags = 0;
    if (sigaltstack(&stackInfo, &oldSignalHandlerStackInfo) == -1)
    {
      fprintf(stderr,"ERROR: cannot initialize signal handler stack (error: %s)\n", strerror(errno));
      exit(128);
    }

    // add signal handlers
    signalHandlerInfo      = signalHandlerInfo;
    signalHandlerInfoCount = signalHandlerInfoCount;
    signalHandlerFunction  = signalHandlerFunction;
    signalHandlerUserData  = signalHandlerUserData;
    for (uint i = 0; i < signalHandlerInfoCount; i++)
    {
       signalActionInfo.sa_handler   = NULL;
       signalActionInfo.sa_sigaction = sigActionHandler;
       sigfillset(&signalActionInfo.sa_mask);
       signalActionInfo.sa_flags     = SA_SIGINFO|SA_RESTART|SA_ONSTACK;
       signalActionInfo.sa_restorer  = NULL;
       sigaction(signalHandlerInfo[i].signalNumber, &signalActionInfo, NULL);
    }
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(signalHandlerInfo);
    UNUSED_VARIABLE(signalHandlerInfoCount);
    UNUSED_VARIABLE(signalHandlerFunction);
    UNUSED_VARIABLE(signalHandlerUserData);
  #endif /* PLATFORM_... */
}

void Stacktrace_done(void)
{
  #if   defined(PLATFORM_LINUX)
    struct sigaction signalActionInfo;
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  #if   defined(PLATFORM_LINUX)
    for (uint i = 0; i < signalHandlerInfoCount; i++)
    {
       signalActionInfo.sa_handler   = SIG_DFL;
       signalActionInfo.sa_sigaction = NULL;
       sigfillset(&signalActionInfo.sa_mask);
       signalActionInfo.sa_flags     = 0;
       signalActionInfo.sa_restorer  = NULL;
       sigaction(signalHandlerInfo[i].signalNumber, &signalActionInfo, NULL);
    }
    sigaltstack(&oldSignalHandlerStackInfo, NULL);
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */
}

void Stacktrace_getSymbols(const char         *executableFileName,
                           const void * const addresses[],
                           uint               addressCount,
                           SymbolInfo         *symbolInfo
                          )
{
  #if   defined(PLATFORM_LINUX)
    #if defined(HAVE_BFD_INIT) && defined(HAVE_LINK_H)
      uint          i;
      FileMatchInfo fileMatchInfo;
      char          errorMessage[128];
      bool          symbolFound;
    #endif // defined(HAVE_BFD_INIT) && defined(HAVE_LINK_H)
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  assert(executableFileName != NULL);
  assert(addresses != NULL);
  assert(symbolInfo != NULL);

  #if   defined(PLATFORM_LINUX)
    #if defined(HAVE_BFD_INIT) && defined(HAVE_LINK_H)
      for (i = 0; i < addressCount; i++)
      {
        fileMatchInfo.found   = FALSE;
        fileMatchInfo.address = addresses[i];
        dl_iterate_phdr(findMatchingFile,&fileMatchInfo);
        if (fileMatchInfo.found)
        {
          symbolFound = getSymbolInfoFromFile(fileMatchInfo.fileName,
                                              (bfd_vma)((uintptr_t)addresses[i]-(uintptr_t)fileMatchInfo.base),
                                              CALLBACK_INLINE(void,(const void *address,
                                                                    const char *fileName,
                                                                    const char *symbolName,
                                                                    ulong      lineNb,
                                                                    void       *userData),
                                              {
                                                SymbolInfo *symbolInfo = (SymbolInfo*)userData;
                                                assert(symbolInfo != NULL);

                                                UNUSED_VARIABLE(address);

                                                symbolInfo->symbolName = stringDuplicate(symbolName);
                                                symbolInfo->fileName   = stringDuplicate(fileName);
                                                symbolInfo->lineNb     = lineNb;
                                              },&symbolInfo[i]),
                                              errorMessage,
                                              sizeof(errorMessage)
                                             );
          if (!symbolFound)
          {
            symbolFound = getSymbolInfoFromFile(fileMatchInfo.fileName,
                                                (bfd_vma)addresses[i],
                                                CALLBACK_INLINE(void,(const void *address,
                                                                      const char *fileName,
                                                                      const char *symbolName,
                                                                      ulong      lineNb,
                                                                      void       *userData),
                                                {
                                                  SymbolInfo *symbolInfo = (SymbolInfo*)userData;
                                                  assert(symbolInfo != NULL);

                                                  UNUSED_VARIABLE(address);

                                                  symbolInfo->symbolName = stringDuplicate(symbolName);
                                                  symbolInfo->fileName   = stringDuplicate(fileName);
                                                  symbolInfo->lineNb     = lineNb;
                                                },&symbolInfo[i]),
                                                errorMessage,
                                                sizeof(errorMessage)
                                               );
          }
//fprintf(stderr,"%s, %d: load from %s: %d\n",__FILE__,__LINE__,fileMatchInfo.fileName,symbolFound);
        }
        else
        {
          symbolFound = getSymbolInfoFromFile(executableFileName,
                                              (bfd_vma)addresses[i],
                                              CALLBACK_INLINE(void,(const void *address,
                                                                    const char *fileName,
                                                                    const char *symbolName,
                                                                    ulong      lineNb,
                                                                    void       *userData),
                                              {
                                                SymbolInfo *symbolInfo = (SymbolInfo*)userData;
                                                assert(symbolInfo != NULL);

                                                UNUSED_VARIABLE(address);

                                                symbolInfo->symbolName = stringDuplicate(symbolName);
                                                symbolInfo->fileName   = stringDuplicate(fileName);
                                                symbolInfo->lineNb     = lineNb;
                                              },&symbolInfo[i]),
                                              errorMessage,
                                              sizeof(errorMessage)
                                             );
//fprintf(stderr,"%s, %d: load from %s: %d\n",__FILE__,__LINE__,executableFileName,symbolFound);
        }

        if (!symbolFound)
        {
          // use dladdr() as fallback
          Dl_info    info;
          char       buffer[256];
          const char *symbolName;
          const char *fileName;

          if (dladdr(addresses[i],&info))
          {
            if ((info.dli_sname != NULL) && ((*info.dli_sname) != '\0'))
            {
              if (demangleSymbolName(buffer,sizeof(buffer),info.dli_sname))
              {
                symbolName = buffer;
              }
              else
              {
                symbolName = info.dli_sname;
              }
            }
            else
            {
              symbolName = NULL;
            }
            fileName = info.dli_fname;

            symbolFound = TRUE;
//fprintf(stderr,"%s, %d: load via dladdr from %s\n",__FILE__,__LINE__,executableFileName);
          }
          else
          {
            symbolName = NULL;
            fileName   = NULL;
//fprintf(stderr,"%s, %d: not found %s\n",__FILE__,__LINE__,executableFileName);
          }

          symbolInfo[i].symbolName = stringDuplicate(symbolName);
          symbolInfo[i].fileName   = stringDuplicate(fileName);
          symbolInfo[i].lineNb     = 0; //  lineNb
        }
      }
    #else // not defined(HAVE_BFD_INIT) && defined(HAVE_LINK_H)
      UNUSED_VARIABLE(executableFileName);
      UNUSED_VARIABLE(addresses);
      UNUSED_VARIABLE(addressCount);
      UNUSED_VARIABLE(symbolInfo);
    #endif // defined(HAVE_BFD_INIT) && defined(HAVE_LINK_H)
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(executableFileName);
    UNUSED_VARIABLE(addresses);
    UNUSED_VARIABLE(addressCount);
    UNUSED_VARIABLE(symbolInfo);
  #endif /* PLATFORM_... */
}

void Stacktrace_freeSymbols(SymbolInfo *symbolInfo,
                            uint       symbolInfoCount
                           )
{
  uint i;

  assert(symbolInfo != NULL);

  for (i = 0; i < symbolInfoCount; i++)
  {
    stringDelete((char*)symbolInfo[i].fileName);
    stringDelete((char*)symbolInfo[i].symbolName);
  }
}

void Stacktrace_getSymbolInfo(const char         *executableFileName,
                              const void * const addresses[],
                              uint               addressCount,
                              SymbolFunction     symbolFunction,
                              void               *symbolUserData,
                              bool               printErrorMessagesFlag
                             )
{
  #if   defined(PLATFORM_LINUX)
    #if defined(HAVE_BFD_INIT) && defined(HAVE_LINK_H)
      uint          i;
      FileMatchInfo fileMatchInfo;
      char          errorMessage[128];
      bool          symbolFound;
    #endif // defined(HAVE_BFD_INIT) && defined(HAVE_LINK_H)
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  assert(executableFileName != NULL);
  assert(addresses != NULL);
  assert(symbolFunction != NULL);

  #if   defined(PLATFORM_LINUX)
    #if defined(HAVE_BFD_INIT) && defined(HAVE_LINK_H)
      for (i = 0; i < addressCount; i++)
      {
        fileMatchInfo.found   = FALSE;
        fileMatchInfo.address = addresses[i];
        dl_iterate_phdr(findMatchingFile,&fileMatchInfo);
        if (fileMatchInfo.found)
        {
          symbolFound = getSymbolInfoFromFile(fileMatchInfo.fileName,
                                              (bfd_vma)((uintptr_t)addresses[i]-(uintptr_t)fileMatchInfo.base),
                                              symbolFunction,
                                              symbolUserData,
                                              errorMessage,
                                              sizeof(errorMessage)
                                             );
          if (!symbolFound)
          {
            symbolFound = getSymbolInfoFromFile(fileMatchInfo.fileName,
                                                (bfd_vma)addresses[i],
                                                symbolFunction,
                                                symbolUserData,
                                                errorMessage,
                                                sizeof(errorMessage)
                                               );
          }
    //fprintf(stderr,"%s, %d: load from %s: %d\n",__FILE__,__LINE__,fileMatchInfo.fileName,symbolFound);
        }
        else
        {
          symbolFound = getSymbolInfoFromFile(executableFileName,
                                              (bfd_vma)addresses[i],
                                              symbolFunction,
                                              symbolUserData,
                                              errorMessage,
                                              sizeof(errorMessage)
                                             );
    //fprintf(stderr,"%s, %d: load from %s: %d\n",__FILE__,__LINE__,executableFileName,symbolFound);
        }

        if (!symbolFound)
        {
          // use dladdr() as fallback
          Dl_info    info;
          char       buffer[256];
          const char *symbolName;
          const char *fileName;

          if (dladdr(addresses[i],&info))
          {
            if ((info.dli_sname != NULL) && ((*info.dli_sname) != '\0'))
            {
              if (demangleSymbolName(buffer,sizeof(buffer),info.dli_sname))
              {
                symbolName = buffer;
              }
              else
              {
                symbolName = info.dli_sname;
              }
            }
            else
            {
              symbolName = NULL;
            }
            fileName = info.dli_fname;

            symbolFound = TRUE;
    //fprintf(stderr,"%s, %d: load via dladdr from %s\n",__FILE__,__LINE__,executableFileName);
          }
          else
          {
            symbolName = NULL;
            fileName   = NULL;
    //fprintf(stderr,"%s, %d: not found %s\n",__FILE__,__LINE__,executableFileName);
          }

          // handle line
          symbolFunction(addresses[i],fileName,symbolName,0,symbolUserData);
        }

        if (!symbolFound && printErrorMessagesFlag)
        {
          fprintf(stderr,"ERROR: %s\n",errorMessage);
        }
      }
    #else // not defined(HAVE_BFD_INIT) && defined(HAVE_LINK_H)
      UNUSED_VARIABLE(executableFileName);
      UNUSED_VARIABLE(addresses);
      UNUSED_VARIABLE(addressCount);
      UNUSED_VARIABLE(symbolFunction);
      UNUSED_VARIABLE(symbolUserData);
      UNUSED_VARIABLE(printErrorMessagesFlag);
    #endif // defined(HAVE_BFD_INIT) && defined(HAVE_LINK_H)
  #elif defined(PLATFORM_WINDOWS)
    UNUSED_VARIABLE(executableFileName);
    UNUSED_VARIABLE(addresses);
    UNUSED_VARIABLE(addressCount);
    UNUSED_VARIABLE(symbolFunction);
    UNUSED_VARIABLE(symbolUserData);
    UNUSED_VARIABLE(printErrorMessagesFlag);
  #endif /* PLATFORM_... */
}

/* end of file */
