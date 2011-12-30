/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/compress.h,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: Backup ARchiver delta compression source functions
* Systems : all
*
\***********************************************************************/

#ifndef __SOURCES__
#define __SOURCES__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
#include <assert.h>

#include "global.h"
#include "lists.h"
#include "strings.h"
#include "patternlists.h"

#include "bar.h"
#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

typedef enum
{
  SOURCE_ENTRY_TYPE_FILE,
  SOURCE_ENTRY_TYPE_IMAGE,
  SOURCE_ENTRY_TYPE_HARDLINK,
} SourceTypes;

/***************************** Datatypes *******************************/

/* source node */
typedef struct SourceNode
{
  LIST_NODE_HEADER(struct SourceNode);

  String storageName;          // storage archive name
  String localStorageName;     // local storage archive name
//  bool   tmpLocalStorageFlag;  // TRUE if located storage created
} SourceNode;

typedef struct
{
  LIST_HEADER(SourceNode);
} SourceList;

/* source entry info block */
typedef struct
{
  SourceNode *sourceNode;
  String     tmpFileName;
  FileHandle tmpFileHandle;
  bool       tmpFileOpenFlag;
} SourceEntryInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Source_initAll
* Purpose: initialize source handle
* Input  : sourcePatternList - source pattern list
* ???
* Output : sourceInfo - initialized source info block
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Source_initAll(void);

/***********************************************************************\
* Name   : Source_doneAll
* Purpose: deinitialize source handle
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

void Source_doneAll();

/***********************************************************************\
* Name   :
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Source_addSource(const String sourcePattern,
                      JobOptions   *jobOptions
                     );

/***********************************************************************\
* Name   :
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Source_addSourceList(const PatternList *sourcePatternList,
                            JobOptions        *jobOptions
                           );

/***********************************************************************\
* Name   :
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Source_openEntry(SourceEntryInfo  *sourceEntryInfo,
                        const String     sourceStorageName,
                        JobOptions       *jobOptions,
                        const String     name
                       );

/***********************************************************************\
* Name   :
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Source_closeEntry(SourceEntryInfo *sourceEntryInfo);

/***********************************************************************\
* Name   :
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Source_getEntryDataBlock(void   *userData,
                                void   *buffer,
                                uint64 offset,
                                ulong  length,
                                ulong  *bytesRead
                               );

#ifdef __cplusplus
  }
#endif

#endif /* __SOURCES__ */

/* end of file */
