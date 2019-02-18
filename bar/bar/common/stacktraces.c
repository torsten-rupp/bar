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
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#ifdef HAVE_BFD_H
  #include <bfd.h>
#endif
#ifdef HAVE_DEMANGLE_H
  #include <libiberty/demangle.h>
#endif
#ifdef HAVE_LINK_H
  #include <link.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"

#include "stacktraces.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define DEBUG_SYMBOL_FILE_EXTENSION ".sym"

/**************************** Datatypes ********************************/

/**************************** Variables ********************************/

/****************************** Macros *********************************/

/**************************** Functions ********************************/

#ifdef HAVE_BFD_INIT
/***********************************************************************\
* Name   : readSymbolTable
* Purpose: read symbol table from BFD
* Input  : abfd - BFD handle
* Output : symbols     - array with symbols
*          symbolCount - number of entries in array
* Return : TRUE iff symbol table read
* Notes  : -
\***********************************************************************/

LOCAL bool readSymbolTable(bfd           *abfd,
                           const asymbol **symbols[],
                           ulong         *symbolCount
                          )
{
  uint size;
  long n;

  assert(symbols != NULL);
  assert(symbolCount != NULL);

  // check if symbols available
  if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
  {
    fprintf(stderr,"ERROR: no symbol table\n");
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
                             TRUE /* dynamic */ ,
                             (void**)symbols,
                             &size
                            );
  }
  if      (n < 0)
  {
    fprintf(stderr,"ERROR: error reading symbols\n");
    return FALSE;
  }
  else if (n == 0)
  {
    fprintf(stderr,"ERROR: no symbols found\n");
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
* Input  : symbolName - symbolname
*          demangledSymbolName     - variable for demangled symbol name
*          demangledSymbolNameSize - max. length of demangled symbol name
* Output : -
* Return : TRUE iff name demangled
* Notes  : -
\***********************************************************************/

LOCAL bool demangleSymbolName(const char *symbolName,
                              char       *demangledSymbolName,
                              uint       demangledSymbolNameSize
                             )
{
  char *s;

  assert(symbolName != NULL);
  assert(demangledSymbolName != NULL);

  s = bfd_demangle(NULL,symbolName,DMGL_ANSI|DMGL_PARAMS);
  if (s != NULL)
  {
    strncpy(demangledSymbolName,s,demangledSymbolNameSize);
    free(s);

    return TRUE;
  }
  else
  {
    strncpy(demangledSymbolName,symbolName,demangledSymbolNameSize);

    return FALSE;
  }
}

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
  if ((bfd_get_section_flags(abfd,section) & SEC_ALLOC) == 0)
  {
    return;
  }
  vma  = bfd_get_section_vma(abfd,section);
  size = bfd_section_size(abfd,section);
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
* Input  : abfd           - BFD handle
*          symbols        - symbol array
*          symbolCount    - number of entries in symbol array
*          address        - address
*          symbolFunction - callback function for symbol
*          symbolUserData - callback user data
* Output : -
* Return : TRUE iff symbol information found
* Notes  : -
\***********************************************************************/

LOCAL bool addressToSymbolInfo(bfd                   *abfd,
                               const asymbol * const symbols[],
                               ulong                 symbolCount,
                               bfd_vma               address,
                               SymbolFunction        symbolFunction,
                               void                  *symbolUserData
                              )
{
  AddressInfo addressInfo;

  assert(symbolFunction != NULL);

  // find symbol
  addressInfo.symbols     = symbols;
  addressInfo.symbolCount = symbolCount;
  addressInfo.address     = address;
  addressInfo.symbolFound = FALSE;
  bfd_map_over_sections(abfd,findAddressInSection,(PTR)&addressInfo);
  if (!addressInfo.sectionFound)
  {
    fprintf(stderr,"ERROR: section not found for address %p\n",(void*)address);
    return FALSE;
  }
  if (!addressInfo.symbolFound)
  {
    fprintf(stderr,"ERROR: symbol not found for address %p\n",(void*)address);
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
      if (demangleSymbolName(addressInfo.symbolName,buffer,sizeof(buffer)))
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
* Input  : fileName - file name
* Output : symbols     - symbol array
*          symbolCount - number of entries in symbol array
* Return : TRUE iff BFD opened
* Notes  : -
\***********************************************************************/

LOCAL bfd* openBFD(const char    *fileName,
                   const asymbol **symbols[],
                   ulong         *symbolCount
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
    fprintf(stderr,"ERROR: invalid format of file '%s' (error: %s)\n",fileName,strerror(errno));
    bfd_close(abfd);
    return NULL;
  }

  if (!bfd_check_format_matches(abfd,bfd_object,&matching))
  {
    if (bfd_get_error() == bfd_error_file_ambiguously_recognized)
    {
      free(matching);
    }
    fprintf(stderr,"ERROR: format does not match for file file '%s'\n",fileName);
    bfd_close(abfd);
    return NULL;
  }

  if (!readSymbolTable(abfd,symbols,symbolCount))
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
* Input  : fileName       - file name
*          address        - address
*          symbolFunction - callback function for symbol
*          symbolUserData - callback user data
* Output : -
* Return : TRUE iff symbol read
* Notes  : -
\***********************************************************************/

LOCAL bool getSymbolInfoFromFile(const char     *fileName,
                                 bfd_vma        address,
                                 SymbolFunction symbolFunction,
                                 void           *symbolUserData
                                )
{
  char          debugSymbolFileName[PATH_MAX];
  ssize_t       n;
  bfd*          abfd;
  const asymbol **symbols;
  ulong         symbolCount;
  bool          result;

  assert(fileName != NULL);

  // open file.<debug symbol extension> or file
  abfd = NULL;
  if (abfd == NULL)
  {
    n = readlink(fileName,debugSymbolFileName,sizeof(debugSymbolFileName));
    if (n > 0)
    {
      debugSymbolFileName[n] = '\0';
      strncat(debugSymbolFileName,DEBUG_SYMBOL_FILE_EXTENSION,sizeof(debugSymbolFileName)-n);
      abfd = openBFD(debugSymbolFileName,&symbols,&symbolCount);
    }
  }
  if (abfd == NULL)
  {
    abfd = openBFD(fileName,&symbols,&symbolCount);
  }
  if (abfd == NULL)
  {
    return FALSE;
  }

  // get symbol info
  result = addressToSymbolInfo(abfd,symbols,symbolCount,address,symbolFunction,symbolUserData);

  // close file
  closeBFD(abfd,symbols,symbolCount);

  return result;
}

// file match info
typedef struct
{
  bool       found;
  const void *address;

  const char *fileName;
  void       *base;
  void       *hdr;
} FileMatchInfo;

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

  assert(info != NULL);
  assert(fileMatchInfo != NULL);

  // unused
  (void)infoSize;

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

  return 0; // return value not used
}
#endif // HAVE_BFD_INIT

/***********************************************************************\
* Name   : Stacktrace_getSymbolInfo
* Purpose: get symbol information
* Input  : executableFileName - executable name
*          addresses          - addresses
*          addressCount       - number of addresses
*          symbolFunction     - callback function for symbol
*          symbolUserData     - callback user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Stacktrace_getSymbolInfo(const char         *executableFileName,
                              const void * const addresses[],
                              uint               addressCount,
                              SymbolFunction     symbolFunction,
                              void               *symbolUserData
                             )
{
#if defined(HAVE_BFD_INIT) && defined(HAVE_LINK)
  uint          i;
  FileMatchInfo fileMatchInfo;
  bool          symbolInfoFromFile;

  assert(executableFileName != NULL);
  assert(addresses != NULL);
  assert(symbolFunction != NULL);

  for (i = 0; i < addressCount; i++)
  {
    fileMatchInfo.found   = FALSE;
    fileMatchInfo.address = addresses[i];
    dl_iterate_phdr(findMatchingFile,&fileMatchInfo);
    if (fileMatchInfo.found)
    {
//fprintf(stderr,"%s, %d: %s\n",__FILE__,__LINE__,fileMatchInfo.fileName);
      symbolInfoFromFile = getSymbolInfoFromFile(fileMatchInfo.fileName,
                                                 (bfd_vma)((uintptr_t)addresses[i]-(uintptr_t)fileMatchInfo.base),
                                                 symbolFunction,
                                                 symbolUserData
                                                );
    }
    else
    {
//fprintf(stderr,"%s, %d: load frm fi\n",__FILE__,__LINE__);
      symbolInfoFromFile = getSymbolInfoFromFile(executableFileName,
                                                 (bfd_vma)addresses[i],
                                                 symbolFunction,
                                                 symbolUserData
                                                );
    }

    if (!symbolInfoFromFile)
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
          if (!demangleSymbolName(info.dli_sname,buffer,sizeof(buffer)))
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
      }
      else
      {
        symbolName = NULL;
        fileName   = NULL;
      }

      // handle line
      symbolFunction(addresses[i],fileName,symbolName,0,symbolUserData);
    }
  }
#else // not defined(HAVE_BFD_INIT) && defined(HAVE_LINK)
  UNUSED_VARIABLE(executableFileName);
  UNUSED_VARIABLE(addresses);
  UNUSED_VARIABLE(addressCount);
  UNUSED_VARIABLE(symbolFunction);
  UNUSED_VARIABLE(symbolUserData);
#endif // defined(HAVE_BFD_INIT) && defined(HAVE_LINK)
}

/* end of file */
