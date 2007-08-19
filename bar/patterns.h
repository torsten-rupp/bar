/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/patterns.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver patterns
* Systems :
*
\***********************************************************************/

#ifndef __PATTERNS__
#define __PATTERNS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include <assert.h>

#include "global.h"
#include "lists.h"
#include "strings.h"

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

typedef enum
{
  PATTERN_TYPE_GLOB,
  PATTERN_TYPE_BASIC,
  PATTERN_TYPE_EXTENDED
} PatternTypes;

/***************************** Datatypes *******************************/

typedef struct PatternNode
{
  NODE_HEADER(struct PatternNode);

  String pattern;      // file/path name pattern
  String matchString;  // pattern match string
  int    matchFlags;
} PatternNode;

typedef struct
{
  LIST_HEADER(PatternNode);
} PatternList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Patterns_init
* Purpose: init patterns
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Patterns_init(void);

/***********************************************************************\
* Name   : Patterns_done
* Purpose: deinitialize patterns
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

void Patterns_done(void);

/***********************************************************************\
* Name   : Patterns_initList
* Purpose: init pattern list
* Input  : patternList - pattern list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Patterns_newList(PatternList *patternList);

/***********************************************************************\
* Name   : Patterns_doneList
* Purpose: done pattern list
* Input  : patternList - pattern list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Patterns_deleteList(PatternList *patternList);

/***********************************************************************\
* Name   : Patterns_addList
* Purpose: add pattern to pattern list
* Input  : patternList - pattern list
*          pattern     - pattern
*          patternType - pattern type; see PATTERN_TTYPE_*
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Patterns_addList(PatternList  *patternList,
                        const char   *pattern,
                        PatternTypes patternType
                       );

/***********************************************************************\
* Name   : Patterns_match
* Purpose: patch string with pattern
* Input  : patternNode - pattern node
*          s           - string
* Output : -
* Return : TRUE if pattern match, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Patterns_match(PatternNode *patternNode,
                    String      s
                   );

/***********************************************************************\
* Name   : Patterns_matchList
* Purpose: patch string with pattern list
* Input  : patternList - pattern list
*          s           - string
* Output : -
* Return : TRUE if pattern match, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Patterns_matchList(PatternList *patternList,
                        String      s
                       );

/***********************************************************************\
* Name   : Patterns_checkIsPattern
* Purpose: check if string is a pattern
* Input  : s - string
* Output : -
* Return : TRUE is s is a pattern, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Patterns_checkIsPattern(String s);

#ifdef __cplusplus
  }
#endif

#endif /* __PATTERNS__ */

/* end of file */
