/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver pattern functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
#include "stringlists.h"

#include "bar.h"
#include "patterns.h"

#include "patternlists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : copyPatternNode
* Purpose: copy allocated pattern node
* Input  : patternNode - pattern node
* Output : -
* Return : copied pattern node
* Notes  : -
\***********************************************************************/

LOCAL PatternNode *copyPatternNode(PatternNode *patternNode,
                                   void        *userData
                                  )
{
  PatternNode *newPatternNode;
  Errors      error;

  assert(patternNode != NULL);

  UNUSED_VARIABLE(userData);

  // allocate pattern node
  newPatternNode = LIST_NEW_NODE(PatternNode);
  if (newPatternNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  newPatternNode->string = String_duplicate(patternNode->string);

  // create pattern
  error = Pattern_init(&newPatternNode->pattern,
                       patternNode->string,
                       patternNode->pattern.type,
                       patternNode->pattern.flags
                      );
  if (error != ERROR_NONE)
  {
    String_delete(newPatternNode->string);
    LIST_DELETE_NODE(newPatternNode);
    return NULL;
  }

  return newPatternNode;
}

/***********************************************************************\
* Name   : freePatternNode
* Purpose: free allocated pattern node
* Input  : patternNode - pattern node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freePatternNode(PatternNode *patternNode,
                           void        *userData
                          )
{
  assert(patternNode != NULL);
  assert(patternNode->string != NULL);

  UNUSED_VARIABLE(userData);

  Pattern_done(&patternNode->pattern);
  String_delete(patternNode->string);
}

/*---------------------------------------------------------------------*/

Errors PatternList_initAll(void)
{
  return ERROR_NONE;
}

void PatternList_doneAll(void)
{
}

void PatternList_init(PatternList *patternList)
{
  assert(patternList != NULL);

  List_init(patternList);
}

void PatternList_initDuplicate(PatternList       *patternList,
                               const PatternList *fromPatternList,
                               const PatternNode *fromPatternListFromNode,
                               const PatternNode *fromPatternListToNode
                              )
{
  assert(patternList != NULL);
  assert(fromPatternList != NULL);

  PatternList_init(patternList);
  PatternList_copy(fromPatternList,patternList,fromPatternListFromNode,fromPatternListToNode);
}

void PatternList_done(PatternList *patternList)
{
  assert(patternList != NULL);

  List_done(patternList,(ListNodeFreeFunction)freePatternNode,NULL);
}

PatternList *PatternList_clear(PatternList *patternList)
{
  assert(patternList != NULL);

  return (PatternList*)List_clear(patternList,(ListNodeFreeFunction)freePatternNode,NULL);
}

void PatternList_copy(const PatternList *fromPatternList,
                      PatternList       *toPatternList,
                      const PatternNode *fromPatternListFromNode,
                      const PatternNode *fromPatternListToNode
                     )
{
  assert(fromPatternList != NULL);
  assert(toPatternList != NULL);

  List_copy(fromPatternList,toPatternList,fromPatternListFromNode,fromPatternListToNode,NULL,(ListNodeCopyFunction)copyPatternNode,NULL);
}

void PatternList_move(PatternList       *fromPatternList,
                      PatternList       *toPatternList,
                      const PatternNode *fromPatternListFromNode,
                      const PatternNode *fromPatternListToNode
                     )
{
  assert(fromPatternList != NULL);
  assert(toPatternList != NULL);

  List_move(fromPatternList,toPatternList,fromPatternListFromNode,fromPatternListToNode,NULL);
}

Errors PatternList_append(PatternList  *patternList,
                          const String pattern,
                          PatternTypes patternType
                         )
{
  assert(patternList != NULL);
  assert(pattern != NULL);

  return PatternList_appendCString(patternList,String_cString(pattern),patternType);
}

Errors PatternList_appendCString(PatternList  *patternList,
                                 const char   *pattern,
                                 PatternTypes patternType
                                )
{
  PatternNode *patternNode;
  Errors      error;

  assert(patternList != NULL);
  assert(pattern != NULL);

  // allocate pattern node
  patternNode = LIST_NEW_NODE(PatternNode);
  if (patternNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  patternNode->string = String_newCString(pattern);

  // init pattern
  error = Pattern_initCString(&patternNode->pattern,
                              pattern,
                              patternType,
                              PATTERN_FLAG_IGNORE_CASE
                             );
  if (error != ERROR_NONE)
  {
    String_delete(patternNode->string);
    LIST_DELETE_NODE(patternNode);
    return error;
  }

  // add to list
  List_append(patternList,patternNode);

  return ERROR_NONE;
}

bool PatternList_match(const PatternList *patternList,
                       const String      string,
                       PatternMatchModes patternMatchMode
                      )
{
  bool        matchFlag;
  PatternNode *patternNode;

  assert(patternList != NULL);
  assert(string != NULL);

  matchFlag = FALSE;
  patternNode = patternList->head;
  while ((patternNode != NULL) && !matchFlag)
  {
    matchFlag = Pattern_match(&patternNode->pattern,string,patternMatchMode);
    patternNode = patternNode->next;
  }

  return matchFlag;
}

bool PatternList_matchStringList(const PatternList *patternList,
                                 const StringList  *stringList,
                                 PatternMatchModes patternMatchMode
                                )
{
  bool       matchFlag;
  StringNode *stringNode;

  assert(patternList != NULL);
  assert(stringList != NULL);

  matchFlag  = FALSE;
  stringNode = stringList->head;
  while ((stringNode != NULL) && !matchFlag)
  {
    matchFlag = PatternList_match(patternList,stringNode->string,patternMatchMode);
    stringNode = stringNode->next;
  }

  return matchFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
