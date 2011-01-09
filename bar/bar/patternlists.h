/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/patternlists.h,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: Backup ARchiver pattern functions
* Systems: all
*
\***********************************************************************/

#ifndef __PATTERNLISTS__
#define __PATTERNLISTS__

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

typedef struct PatternNode
{
  LIST_NODE_HEADER(struct PatternNode);

  String  string;
  Pattern pattern;
} PatternNode;

typedef struct
{
  LIST_HEADER(PatternNode);
} PatternList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : PATTERNLIST_ITERATE
* Purpose: iterated over pattern list and execute block
* Input  : list     - list
*          variable - iterator variable (type PatternNode)
* Output : -
* Return : -
* Notes  : usage:
*            PATTERNLIST_ITERATE(list,variable)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define PATTERNLIST_ITERATE(list,variable) \
  for ((variable) = (list)->head; \
       (variable) != NULL; \
       (variable) = (variable)->next \
      )

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : PatternList_initAll
* Purpose: init pattern lists
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors PatternList_initAll(void);

/***********************************************************************\
* Name   : PatternList_doneAll
* Purpose: deinitialize pattern lists
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

void PatternList_doneAll(void);

/***********************************************************************\
* Name   : PatternList_init
* Purpose: init pattern list
* Input  : patternList - pattern list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void PatternList_init(PatternList *patternList);

/***********************************************************************\
* Name   : PatternList_done
* Purpose: done pattern list
* Input  : patternList - pattern list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void PatternList_done(PatternList *patternList);

/***********************************************************************\
* Name   : PatternList_clear
* Purpose: remove all patterns in list
* Input  : patternList - pattern list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void PatternList_clear(PatternList *patternList);

/***********************************************************************\
* Name   : Pattern_copyList
* Purpose: copy all patterns from source list to destination list
* Input  : fromPatternList - from pattern list (source)
*          toPatternList   - to pattern list (destination)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void PatternList_copy(const PatternList *fromPatternList, PatternList *toPatternList);

/***********************************************************************\
* Name   : PatternList_move
* Purpose: move all patterns from source list to destination list
* Input  : fromPatternList - from pattern list (source)
*          toPatternList   - to pattern list (destination)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void PatternList_move(PatternList *fromPatternList, PatternList *toPatternList);

/***********************************************************************\
* Name   : PatternList_append, PatternList_appendCString
* Purpose: add pattern to pattern list
* Input  : patternList - pattern list
*          pattern     - pattern
*          patternType - pattern type; see PATTERN_TTYPE_*
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors PatternList_append(PatternList  *patternList,
                          const String pattern,
                          PatternTypes patternType
                         );
Errors PatternList_appendCString(PatternList  *patternList,
                                 const char   *pattern,
                                 PatternTypes patternType
                                );

/***********************************************************************\
* Name   : PatternList_match, PatternList_matchStringList
* Purpose: patch string with all patterns of list
* Input  : patternList      - pattern list
*          string           - string
*          stringList       - string list
*          patternMatchMode - pattern match mode; see PatternMatchModes
* Output : -
* Return : TRUE if pattern match, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool PatternList_match(const PatternList *patternList,
                       const String      string,
                       PatternMatchModes patternMatchMode
                      );
bool PatternList_matchStringList(const PatternList *patternList,
                                 const StringList  *stringList,
                                 PatternMatchModes patternMatchMode
                                );

#ifdef __cplusplus
  }
#endif

#endif /* __PATTERNLISTS__ */

/* end of file */
