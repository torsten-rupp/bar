/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/entrylists.h,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: Backup ARchiver entry list functions
* Systems: all
*
\***********************************************************************/

#ifndef __ENTRYLISTS__
#define __ENTRYLISTS__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include <assert.h>

#include "global.h"
#include "lists.h"
#include "strings.h"
#include "stringlists.h"
#include "patterns.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef enum
{
  ENTRY_TYPE_FILE,
  ENTRY_TYPE_IMAGE,
} EntryTypes;

typedef struct EntryNode
{
  LIST_NODE_HEADER(struct EntryNode);

  EntryTypes type;
  String     string;
  Pattern    pattern;
} EntryNode;

typedef struct
{
  LIST_HEADER(EntryNode);
} EntryList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : EntryList_initAll
* Purpose: init entry lists
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors EntryList_initAll(void);

/***********************************************************************\
* Name   : EntryList_doneAll
* Purpose: deinitialize entry lists
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

void EntryList_doneAll(void);

/***********************************************************************\
* Name   : EntryList_init
* Purpose: init entry list
* Input  : entryList - entry list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void EntryList_init(EntryList *entryList);

/***********************************************************************\
* Name   : EntryList_done
* Purpose: done entry list
* Input  : entryList - entry list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void EntryList_done(EntryList *entryList);

/***********************************************************************\
* Name   : EntryList_clear
* Purpose: remove all entrys in list
* Input  : entryList - entry list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void EntryList_clear(EntryList *entryList);

/***********************************************************************\
* Name   : Entry_copyList
* Purpose: copy all entrys from source list to destination list
* Input  : fromEntryList - from entry list (source)
*          toEntryList   - to entry list (destination)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void EntryList_copy(const EntryList *fromEntryList, EntryList *toEntryList);

/***********************************************************************\
* Name   : EntryList_move
* Purpose: move all entrys from source list to destination list
* Input  : fromEntryList - from entry list (source)
*          toEntryList   - to entry list (destination)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void EntryList_move(EntryList *fromEntryList, EntryList *toEntryList);

/***********************************************************************\
* Name   : EntryList_append, EntryList_appendCString
* Purpose: add entry to entry list
* Input  : entryList   - entry list
+          type        - entry type; see ENTRY_TYPE_*
*          pattern     - pattern
*          patternType - pattern type; see PATTERN_TYPE_*
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors EntryList_append(EntryList    *entryList,
                        EntryTypes   type,
                        const String pattern,
                        PatternTypes patternType
                       );
Errors EntryList_appendCString(EntryList    *entryList,
                               EntryTypes   type,
                               const char   *pattern,
                               PatternTypes patternType
                              );

/***********************************************************************\
* Name   : EntryList_match, EntryList_matchStringList
* Purpose: patch string/string list with all entrys of list
* Input  : entryList        - entry list
*          string           - string
*          stringList       - string list
*          patternMatchMode - pattern match mode; see PatternMatchModes
* Output : -
* Return : TRUE if entry match, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool EntryList_match(const EntryList   *entryList,
                     const String      string,
                     PatternMatchModes patternMatchMode
                    );
bool EntryList_matchStringList(const EntryList   *entryList,
                               const StringList  *stringList,
                               PatternMatchModes patternMatchMode
                              );

#ifdef __cplusplus
  }
#endif

#endif /* __PATTERNLISTS__ */

/* end of file */
