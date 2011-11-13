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

/* source info block */
typedef struct
{
  SourceList       sourceList;
  const JobOptions *jobOptions;
} SourceInfo;

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
* Name   : Source_new
* Purpose: initialize source handle
* Input  : sourceInfo        - source info block
*          sourcePatternList - source pattern list
* Output : sourceInfo - initialized source info block
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Source_init(SourceInfo        *sourceInfo,
                   const PatternList *sourcePatternList,
                   JobOptions        *jobOptions
                  );

/***********************************************************************\
* Name   : Source_delete
* Purpose: deinitialize source handle
* Input  : sourceInfo - source info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

void Source_done(SourceInfo *sourceInfo);

Errors Source_openEntry(SourceEntryInfo *sourceEntryInfo,
                        SourceInfo      *sourceInfo,
                        const String    name
                       );

void Source_closeEntry(SourceEntryInfo *sourceEntryInfo);

Errors Source_getEntryDataBlock(SourceEntryInfo *sourceEntryInfo,
                                void            *buffer,
                                uint64          offset,
                                ulong           length
                               );

#ifdef __cplusplus
  }
#endif

#endif /* __SOURCES__ */

/* end of file */
