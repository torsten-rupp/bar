/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/patterns.h,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: Backup ARchiver pattern functions
* Systems : all
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

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

typedef enum
{
  PATTERN_TYPE_GLOB,
  PATTERN_TYPE_BASIC,
  PATTERN_TYPE_EXTENDED
} PatternTypes;

typedef enum
{
  PATTERN_MATCH_MODE_BEGIN,
  PATTERN_MATCH_MODE_END,
  PATTERN_MATCH_MODE_EXACT,
} PatternMatchModes;

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
* Name   : Pattern_init
* Purpose: init patterns
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Pattern_init(void);

/***********************************************************************\
* Name   : Pattern_done
* Purpose: deinitialize patterns
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

void Pattern_done(void);

/***********************************************************************\
* Name   : Pattern_initList
* Purpose: init pattern list
* Input  : patternList - pattern list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Pattern_initList(PatternList *patternList);

/***********************************************************************\
* Name   : Pattern_doneList
* Purpose: done pattern list
* Input  : patternList - pattern list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Pattern_doneList(PatternList *patternList);

/***********************************************************************\
* Name   : Pattern_clearList
* Purpose: remove all patterns in list
* Input  : patternList - pattern list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Pattern_clearList(PatternList *patternList);

/***********************************************************************\
* Name   : Pattern_moveList
* Purpose: move all patterns from source list to destination list
* Input  : fromPatternList - from pattern list (source)
*          toPatternList   - to pattern list (destination)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Pattern_moveList(PatternList *fromPatternList, PatternList *toPatternList);

/***********************************************************************\
* Name   : Pattern_appendList
* Purpose: add pattern to pattern list
* Input  : patternList - pattern list
*          pattern     - pattern
*          patternType - pattern type; see PATTERN_TTYPE_*
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Pattern_appendList(PatternList  *patternList,
                          const char   *pattern,
                          PatternTypes patternType
                         );

/***********************************************************************\
* Name   : Pattern_match
* Purpose: patch string with single pattern
* Input  : patternNode      - pattern node
*          s                - string
*          patternMatchMode - pattern match mode; see PatternMatchModes
* Output : -
* Return : TRUE if pattern match, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Pattern_match(PatternNode       *patternNode,
                   String            s,
                   PatternMatchModes patternMatchMode
                  );

/***********************************************************************\
* Name   : Pattern_matchList
* Purpose: patch string with all patterns of list
* Input  : patternList      - pattern list
*          s                - string
*          patternMatchMode - pattern match mode; see PatternMatchModes
* Output : -
* Return : TRUE if pattern match, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Pattern_matchList(PatternList       *patternList,
                       String            s,
                       PatternMatchModes patternMatchMode
                      );

/***********************************************************************\
* Name   : Pattern_checkIsPattern
* Purpose: check if string is a pattern
* Input  : s - string
* Output : -
* Return : TRUE is s is a pattern, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Pattern_checkIsPattern(String s);

#ifdef __cplusplus
  }
#endif

#endif /* __PATTERNS__ */

/* end of file */
