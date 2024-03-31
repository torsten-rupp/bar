/***********************************************************************\
*
* Contents: Backup ARchiver delta compression source functions
* Systems: all
*
\***********************************************************************/

#ifndef __SOURCES__
#define __SOURCES__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "forward.h"         /* required for JobOptions. Do not include
                                bar_global.h, because of circular dependency
                                in JobOptions
                             */

#include "common/global.h"
#include "common/lists.h"
#include "common/strings.h"
#include "common/files.h"
//#include "common/patternlists.h"
#include "deltasourcelists.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define SOURCE_SIZE_UNKNOWN -1LL

/***************************** Datatypes *******************************/

// source handle
typedef struct
{
// NYI: is there a list of names required?
  ConstString name;             // source name
//  StringList nameList;
  uint64      size;             // size of source
  String      tmpFileName;      // temporary file name
  FileHandle  tmpFileHandle;    // temporary file handle
  uint64      baseOffset;       // block read base offset in source
} DeltaSourceHandle;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : DeltaSource_initAll
* Purpose: initialize source
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors DeltaSource_initAll(void);

/***********************************************************************\
* Name   : DeltaSource_doneAll
* Purpose: deinitialize source
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

void DeltaSource_doneAll(void);

/***********************************************************************\
* Name   : DeltaSource_addSource
* Purpose: add source
* Input  : sourcePattern - source pattern
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors DeltaSource_add(ConstString sourcePattern, PatternTypes patternType);

/***********************************************************************\
* Name   : DeltaSource_addSourceList
* Purpose: add source list
* Input  : sourcePatternList - source pattern list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

//Errors DeltaSource_addSourceList(const PatternList *sourcePatternList);

/***********************************************************************\
* Name   : DeltaSource_openEntry
* Purpose: open source entry
* Input  : sourceHandle      - source handle variable
*          deltaSourceList   - delta sources list
*          sourceStorageName - storage name
*          name              - entry name to open (file, image,
*                              hard link)
*          size              - size of entry [bytes]
*          jobOptions        - job option settings
* Output : sourceHandle - source handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors DeltaSource_openEntry(DeltaSourceHandle *sourceHandle,
                             DeltaSourceList   *deltaSourceList,
                             ConstString       sourceStorageName,
                             ConstString       name,
                             int64             size,
                             const JobOptions  *jobOptions
                            );

/***********************************************************************\
* Name   : DeltaSource_closeEntry
* Purpose: close source entry
* Input  : sourceHandle - source handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void DeltaSource_closeEntry(DeltaSourceHandle *sourceHandle);

/***********************************************************************\
* Name   : DeltaSource_getName
* Purpose: get source name
* Input  : sourceHandle - source handle
* Output : -
* Return : source name
* Notes  : -
\***********************************************************************/

ConstString DeltaSource_getName(const DeltaSourceHandle *sourceHandle);

/***********************************************************************\
* Name   : DeltaSource_getSize
* Purpose: get source size
* Input  : sourceHandle - source handle
* Output : -
* Return : source size [bytes]
* Notes  : -
\***********************************************************************/

uint64 DeltaSource_getSize(const DeltaSourceHandle *sourceHandle);

/***********************************************************************\
* Name   : DeltaSource_getName
* Purpose: get source name
* Input  : sourceHandle - source handle
*          offset       - base offset for read blocks
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void DeltaSource_setBaseOffset(DeltaSourceHandle *sourceHandle, uint64 offset);

/***********************************************************************\
* Name   : DeltaSource_getEntryDataBlock
* Purpose: get source entry data block
* Input  : sourceHandle    - source handle
*          buffer          - buffer for data block
*          offset          - offset (0..n-1)
*          length          - length of data block to read
* Output : bytesRead - number of bytes read
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors DeltaSource_getEntryDataBlock(DeltaSourceHandle *sourceHandle,
                                     void              *buffer,
                                     uint64            offset,
                                     ulong             length,
                                     ulong             *bytesRead
                                    );

#ifdef __cplusplus
  }
#endif

#endif /* __SOURCES__ */

/* end of file */
