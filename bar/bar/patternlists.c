/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver pattern list functions
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

#include "patterns.h"

#include "patternlists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
LOCAL uint id = 0;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getNewId
* Purpose: get new id
* Input  : -
* Output : -
* Return : id
* Notes  : -
\***********************************************************************/

LOCAL uint getNewId(void)
{
  id++;

  return id;
}

/***********************************************************************\
* Name   : duplicatePatternNode
* Purpose: duplicate pattern node
* Input  : patternNode - pattern node
* Output : -
* Return : copied pattern node
* Notes  : -
\***********************************************************************/

LOCAL PatternNode *duplicatePatternNode(PatternNode *patternNode,
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
  newPatternNode->id     = getNewId();
  newPatternNode->string = String_duplicate(patternNode->string);

  // create pattern
  error = Pattern_copy(&newPatternNode->pattern,
                       &patternNode->pattern
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

  List_copy(fromPatternList,toPatternList,fromPatternListFromNode,fromPatternListToNode,NULL,(ListNodeDuplicateFunction)duplicatePatternNode,NULL);
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
                          ConstString  string,
                          PatternTypes patternType,
                          uint         *id
                         )
{
  assert(patternList != NULL);
  assert(string != NULL);

  return PatternList_appendCString(patternList,String_cString(string),patternType,id);
}

Errors PatternList_appendCString(PatternList  *patternList,
                                 const char   *string,
                                 PatternTypes patternType,
                                 uint         *id
                                )
{
  PatternNode *patternNode;
  Errors      error;

  assert(patternList != NULL);
  assert(string != NULL);

  // allocate pattern node
  patternNode = LIST_NEW_NODE(PatternNode);
  if (patternNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  patternNode->id     = getNewId();
  patternNode->string = String_newCString(string);

  // init pattern
  error = Pattern_initCString(&patternNode->pattern,
                              string,
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

  if (id != NULL) (*id) = patternNode->id;

  return ERROR_NONE;
}

Errors PatternList_update(PatternList  *patternList,
                          uint         id,
                          ConstString  string,
                          PatternTypes patternType
                         )
{
  assert(patternList != NULL);
  assert(string != NULL);

  return PatternList_updateCString(patternList,id,String_cString(string),patternType);
}

Errors PatternList_updateCString(PatternList  *patternList,
                                 uint         id,
                                 const char   *string,
                                 PatternTypes patternType
                                )
{
  PatternNode *patternNode;
  Pattern     pattern;
  Errors      error;

  assert(patternList != NULL);
  assert(string != NULL);

  // find pattern node
  patternNode = (PatternNode*)LIST_FIND(patternList,patternNode,patternNode->id == id);
  if (patternNode != NULL)
  {
    // compile pattern
    error = Pattern_initCString(&pattern,
                                string,
                                patternType,
                                PATTERN_FLAG_IGNORE_CASE
                               );
    if (error != ERROR_NONE)
    {
      return error;
    }

    // store
    String_setCString(patternNode->string,string);
    Pattern_done(&patternNode->pattern);
    patternNode->pattern     = pattern;
  }

  return ERROR_NONE;
}

bool PatternList_remove(PatternList *patternList,
                        uint        id
                       )
{
  PatternNode *patternNode;

  assert(patternList != NULL);

  patternNode = (PatternNode*)LIST_FIND(patternList,patternNode,patternNode->id == id);
  if (patternNode != NULL)
  {
    List_removeAndFree(patternList,patternNode,(ListNodeFreeFunction)freePatternNode,NULL);
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool PatternList_match(const PatternList *patternList,
                       ConstString       string,
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
