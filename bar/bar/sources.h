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
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "lists.h"
#include "strings.h"
#include "files.h"
#include "patternlists.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

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
  String     storageName;      // storage name
  String     tmpFileName;      // temporary file name
  FileHandle tmpFileHandle;    // temporary file handle
  bool       tmpFileOpenFlag;  // TRUE iff temporary file is open
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
* Return : -
* Notes  : -
\***********************************************************************/

void Source_addSource(const String sourcePattern);

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
* Input  : sourceStorageName - storage name
*          name              - entry name (file, image, hard link)
* Output : sourceEntryInfo - source entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Source_openEntry(SourceHandle *sourceHandle,
                        const String sourceStorageName,
                        const String name
                       );

/***********************************************************************\
* Name   : Source_closeEntry
* Purpose: close source entry
* Input  : sourceEntryInfo - source entry info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Source_closeEntry(SourceHandle *sourceHandle);

/***********************************************************************\
* Name   : Source_getEntryDataBlock
* Purpose: get source entry data block
* Input  : sourceEntryInfo - source entry info
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
