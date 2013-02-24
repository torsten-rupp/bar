/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
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
                                bar.h, because of circular dependency
                                in JobOptions
                             */

#include "global.h"
#include "lists.h"
#include "strings.h"
#include "files.h"
#include "patternlists.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define SOURCE_SIZE_UNKNOWN -1LL

/***************************** Datatypes *******************************/

// source node
typedef struct SourceNode
{
  LIST_NODE_HEADER(struct SourceNode);

  String storageName;          // storage archive name
} SourceNode;

typedef struct
{
  LIST_HEADER(SourceNode);
} SourceList;

// source handle
typedef struct
{
#warning is there a list of names required?
  String     name;             // source name
//  StringList nameList;
  uint64     size;             // size of source
  String     tmpFileName;      // temporary file name
  FileHandle tmpFileHandle;    // temporary file handle
  uint64     baseOffset;       // block read base offset in source
} SourceHandle;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Source_initAll
* Purpose: initialize source
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Source_initAll(void);

/***********************************************************************\
* Name   : Source_doneAll
* Purpose: deinitialize source
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

void Source_doneAll(void);

/***********************************************************************\
* Name   : Source_addSource
* Purpose: add source
* Input  : sourcePattern - source pattern
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Source_addSource(const String sourcePattern);

/***********************************************************************\
* Name   : Source_addSourceList
* Purpose: add source list
* Input  : sourcePatternList - source pattern list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Source_addSourceList(const PatternList *sourcePatternList);

/***********************************************************************\
* Name   : Source_openEntry
* Purpose: open source entry
* Input  : sourceHandle      - source handle variable
*          sourceStorageName - storage name
*          name              - entry name to open (file, image,
*                              hard link)
*          size              - size of entry [bytes]
*          jobOptions        - job option settings
* Output : sourceHandle - source handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Source_openEntry(SourceHandle     *sourceHandle,
                        const String     sourceStorageName,
                        const String     name,
                        int64            size,
                        const JobOptions *jobOptions
                       );

/***********************************************************************\
* Name   : Source_closeEntry
* Purpose: close source entry
* Input  : sourceHandle - source handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Source_closeEntry(SourceHandle *sourceHandle);

/***********************************************************************\
* Name   : Source_getName
* Purpose: get source name
* Input  : sourceHandle - source handle
* Output : -
* Return : source name
* Notes  : -
\***********************************************************************/

String Source_getName(SourceHandle *sourceHandle);

/***********************************************************************\
* Name   : Source_getSize
* Purpose: get source size
* Input  : sourceHandle - source handle
* Output : -
* Return : source size [bytes]
* Notes  : -
\***********************************************************************/

uint64 Source_getSize(SourceHandle *sourceHandle);

/***********************************************************************\
* Name   : Source_getName
* Purpose: get source name
* Input  : sourceHandle - source handle
*          offset       - base offset for read blocks
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Source_setBaseOffset(SourceHandle *sourceHandle, uint64 offset);

/***********************************************************************\
* Name   : Source_getEntryDataBlock
* Purpose: get source entry data block
* Input  : sourceHandle    - source handle
*          buffer          - buffer for data block
*          offset          - offset (0..n-1)
*          length          - length of data block to read
* Output : bytesRead - number of bytes read
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Source_getEntryDataBlock(SourceHandle *sourceHandle,
                                void         *buffer,
                                uint64       offset,
                                ulong        length,
                                ulong        *bytesRead
                               );

#ifdef __cplusplus
  }
#endif

#endif /* __SOURCES__ */

/* end of file */
