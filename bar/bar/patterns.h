/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/patterns.h,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: Backup ARchiver pattern functions
* Systems: all
*
\***********************************************************************/

#ifndef __PATTERNS__
#define __PATTERNS__

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

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define PATTERN_CHAR_SET_GLOB           "?*"
#define PATTERN_CHAR_SET_REGEX          "*+?{}():[].^$|"
#define PATTERN_CHAR_SET_EXTENDED_REGEX "*+?{}():[].^$|"

/* pattern types */
typedef enum
{
  PATTERN_TYPE_GLOB,                    // * and ?
  PATTERN_TYPE_REGEX,                   // regular expressions
  PATTERN_TYPE_EXTENDED_REGEX           // extended regular expressions
} PatternTypes;

/* match modes */
typedef enum
{
  PATTERN_MATCH_MODE_BEGIN,
  PATTERN_MATCH_MODE_END,
  PATTERN_MATCH_MODE_EXACT,
} PatternMatchModes;

/***************************** Datatypes *******************************/

typedef struct PatternNode
{
  LIST_NODE_HEADER(struct PatternNode);

  PatternTypes type;
  String       pattern;
  regex_t      regexBegin;              // regular expression for matching begin
  regex_t      regexEnd;                // regular expression for matching end
  regex_t      regexExact;              // regular expression for matching exact
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
* Name   : Pattern_initAll
* Purpose: init patterns
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Pattern_initAll(void);

/***********************************************************************\
* Name   : Pattern_doneAll
* Purpose: deinitialize patterns
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

void Pattern_doneAll(void);

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
* Name   : Pattern_copyList
* Purpose: copy all patterns from source list to destination list
* Input  : fromPatternList - from pattern list (source)
*          toPatternList   - to pattern list (destination)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Pattern_copyList(const PatternList *fromPatternList, PatternList *toPatternList);

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
* Return : ERROR_NONE or error code
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
