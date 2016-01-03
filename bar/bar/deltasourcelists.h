/***********************************************************************\
*
* $Revision: 3620 $
* $Date: 2015-02-06 20:57:40 +0100 (Fri, 06 Feb 2015) $
* $Author: torsten $
* Contents: Backup ARchiver delta source list functions
* Systems: all
*
\***********************************************************************/

#ifndef __DELTA_SOURCE_LISTS__
#define __DELTA_SOURCE_LISTS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#if defined(HAVE_PCRE)
  #include <pcreposix.h>
#elif defined(HAVE_REGEX_H)
  #include <regex.h>
#else
  #error No regular expression library available!
#endif /* HAVE_PCRE || HAVE_REGEX_H */
#include <assert.h>

#include "global.h"
#include "lists.h"
#include "strings.h"
#include "patterns.h"
#include "semaphores.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct DeltaSourceNode
{
  LIST_NODE_HEADER(struct DeltaSourceNode);

  uint         id;
  String       storageName;
  PatternTypes patternType;
  bool         locked;
} DeltaSourceNode;

typedef struct
{
  LIST_HEADER(DeltaSourceNode);
  Semaphore lock;
} DeltaSourceList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : DeltaSourceList_initAll
* Purpose: init delta source lists
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors DeltaSourceList_initAll(void);

/***********************************************************************\
* Name   : DeltaSourceList_doneAll
* Purpose: deinitialize delta source lists
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

void DeltaSourceList_doneAll(void);

/***********************************************************************\
* Name   : DeltaSourceList_init
* Purpose: init entry list
* Input  : entryList - entry list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void DeltaSourceList_init(DeltaSourceList *deltaSourceList);

/***********************************************************************\
* Name   : DeltaSourceList_initDuplicate
* Purpose: init duplicated delta source list
* Input  : deltaSourceList            - delta source list
*          fromDeltaSourceList         - from delta source list (source)
*          fromDeltaSourceListFromNode - from node (could be NULL)
*          fromDeltaSourceListToNode   - to node (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void DeltaSourceList_initDuplicate(DeltaSourceList       *deltaSourceList,
                                   const DeltaSourceList *fromDeltaSourceList,
                                   const DeltaSourceNode *fromDeltaSourceListFromNode,
                                   const DeltaSourceNode *fromDeltaSourceListToNode
                                  );

/***********************************************************************\
* Name   : DeltaSourceList_done
* Purpose: done delta source list
* Input  : deltaSourceList - delta source list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void DeltaSourceList_done(DeltaSourceList *deltaSourceList);

/***********************************************************************\
* Name   : DeltaSourceList_clear
* Purpose: remove all in list
* Input  : deltaSourceList - delta source list
* Output : -
* Return : delta source list
* Notes  : -
\***********************************************************************/

DeltaSourceList *DeltaSourceList_clear(DeltaSourceList *deltaSourceList);

/***********************************************************************\
* Name   : DeltaSourceList_copy
* Purpose: copy all delta sources from source list to destination list
* Input  : fromDeltaSourceList         - from delta source list (source)
*          toDeltaSourceList           - to delta source list (destination)
*          fromDeltaSourceListFromNode - from node (could be NULL)
*          fromDeltaSourceListToNode   - to node (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void DeltaSourceList_copy(const DeltaSourceList *fromDeltaSourceList,
                          DeltaSourceList       *toDeltaSourceList,
                          const DeltaSourceNode *fromDeltaSourceListFromNode,
                          const DeltaSourceNode *fromDeltaSourceListToNode
                         );

/***********************************************************************\
* Name   : DeltaSourceList_append
* Purpose: add entry to entry list
* Input  : entryList   - entry list
+          type        - entry type; see ENTRY_TYPE_*
*          pattern     - pattern
*          patternType - pattern type; see PATTERN_TYPE_*
* Output : id - id (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors DeltaSourceList_append(DeltaSourceList *deltaSourceList,
                              ConstString     storageName,
                              PatternTypes    patternType,
                              uint            *id
                             );

/***********************************************************************\
* Name   : DeltaSourceList_match, DeltaSourceList_matchStringList
* Purpose: patch string/string list with all entrys of list
* Input  : entryList        - entry list
*          string           - string
*          stringList       - string list
*          patternMatchMode - pattern match mode; see PatternMatchModes
* Output : -
* Return : TRUE if entry match, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool DeltaSourceList_match(const DeltaSourceList *entryList,
                           ConstString           string,
                           PatternMatchModes     patternMatchMode
                         );

#ifdef __cplusplus
  }
#endif

#endif /* __DELTA_SOURCE_LISTS__ */

/* end of file */
