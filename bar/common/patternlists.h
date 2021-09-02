/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver pattern list functions
* Systems: all
*
\***********************************************************************/

#ifndef __PATTERNLISTS__
#define __PATTERNLISTS__

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

#include "common/global.h"
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

  uint         id;               // unique node id
  String       string;           // pattern string
  Pattern      pattern;          // pattern
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

/***********************************************************************\
* Name   : PATTERNLIST_ITERATEX
* Purpose: iterated over pattern list and execute block
* Input  : list      - list
*          variable  - iterator variable (type PatternNode)
*          condition - additional condition
* Output : -
* Return : -
* Notes  : usage:
*            PATTERNLIST_ITERATEX(list,variable,TRUE)
*            {
*              ... = variable->...
*            }
\***********************************************************************/

#define PATTERNLIST_ITERATEX(list,variable,condition) \
  for ((variable) = (list)->head; \
       ((variable) != NULL) && (condition); \
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
* Name   : PatternList_initDuplicate
* Purpose: init duplicated pattern list
* Input  : patternList             - pattern list
*          fromPatternList         - from pattern list (source)
*          fromPatternListFromNode - from node (could be NULL)
*          fromPatternListToNode   - to node (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void PatternList_initDuplicate(PatternList       *patternList,
                               const PatternList *fromPatternList,
                               const PatternNode *fromPatternListFromNode,
                               const PatternNode *fromPatternListToNode
                              );

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
* Return : pattern list
* Notes  : -
\***********************************************************************/

PatternList *PatternList_clear(PatternList *patternList);

/***********************************************************************\
* Name   : Pattern_copyList
* Purpose: copy all patterns from source list to destination list
* Input  : toPatternList           - to pattern list (destination)
*          fromPatternList         - from pattern list (source)
*          fromPatternListFromNode - from node (could be NULL)
*          fromPatternListToNode   - to node (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void PatternList_copy(PatternList       *toPatternList,
                      const PatternList *fromPatternList,
                      const PatternNode *fromPatternListFromNode,
                      const PatternNode *fromPatternListToNode
                     );

/***********************************************************************\
* Name   : PatternList_move
* Purpose: move all patterns from source list to destination list
* Input  : toPatternList           - to pattern list (destination)
*          fromPatternList         - from pattern list (source)
*          fromPatternListFromNode - from node (could be NULL)
*          fromPatternListToNode   - to node (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void PatternList_move(PatternList       *toPatternList,
                      PatternList       *fromPatternList,
                      const PatternNode *fromPatternListFromNode,
                      const PatternNode *fromPatternListToNode
                     );

/***********************************************************************\
* Name   : PatternList_append, PatternList_appendCString
* Purpose: add pattern to pattern list
* Input  : patternList - pattern list
*          string      - pattern
*          patternType - pattern type; see PATTERN_TTYPE_*
* Output : id - pattern node id (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors PatternList_append(PatternList  *patternList,
                          ConstString  string,
                          PatternTypes patternType,
                          uint         *id
                         );
Errors PatternList_appendCString(PatternList  *patternList,
                                 const char   *string,
                                 PatternTypes patternType,
                                 uint         *id
                                );

/***********************************************************************\
* Name   : PatternList_update, PatternList_updateCString
* Purpose: update pattern in pattern list
* Input  : patternList - pattern list
*          id          - pattern node id
*          string      - pattern
*          patternType - pattern type; see PATTERN_TTYPE_*
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors PatternList_update(PatternList  *patternList,
                          uint         id,
                          ConstString  string,
                          PatternTypes patternType
                         );
Errors PatternList_updateCString(PatternList  *patternList,
                                 uint         id,
                                 const char   *string,
                                 PatternTypes patternType
                                );

/***********************************************************************\
* Name   : PatternList_remove
* Purpose: remove pattern from pattern list
* Input  : patternList - pattern list
*          id          - pattern node id
* Output : -
* Return : TRUE iff removed
* Notes  : -
\***********************************************************************/

bool PatternList_remove(PatternList *patternList,
                        uint        id
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
                       ConstString       string,
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
