/***********************************************************************\
*
* $Revision: 3543 $
* $Date: 2015-01-24 13:59:52 +0100 (Sat, 24 Jan 2015) $
* $Author: torsten $
* Contents: Backup ARchiver entry list functions
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

#include "bar_global.h"
#include "patterns.h"

#include "deltasourcelists.h"

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
* Name   : copyDeltaSourceNode
* Purpose: copy allocated delta source node
* Input  : deltaSourceNode - delta source node
* Output : -
* Return : copied delta source node
* Notes  : -
\***********************************************************************/

LOCAL DeltaSourceNode *copyDeltaSourceNode(DeltaSourceNode *deltaSourceNode,
                                           void            *userData
                                          )
{
  DeltaSourceNode *newDeltaSourceNode;

  assert(deltaSourceNode != NULL);

  UNUSED_VARIABLE(userData);

  // allocate pattern node
  newDeltaSourceNode = LIST_NEW_NODE(DeltaSourceNode);
  if (newDeltaSourceNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  newDeltaSourceNode->storageName = String_duplicate(deltaSourceNode->storageName);
  newDeltaSourceNode->patternType = deltaSourceNode->patternType;

  return newDeltaSourceNode;
}

/***********************************************************************\
* Name   : freeDeltaSourceNode
* Purpose: free allocated entry node
* Input  : deltaSourceNode - entry node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeDeltaSourceNode(DeltaSourceNode *deltaSourceNode,
                               void            *userData
                              )
{
  assert(deltaSourceNode != NULL);
  assert(deltaSourceNode->storageName != NULL);

  UNUSED_VARIABLE(userData);

  String_delete(deltaSourceNode->storageName);
}

/*---------------------------------------------------------------------*/

Errors DeltaSourceList_initAll(void)
{
  return ERROR_NONE;
}

void DeltaSourceList_doneAll(void)
{
}

void DeltaSourceList_init(DeltaSourceList *deltaSourceList)
{
  assert(deltaSourceList != NULL);

  List_init(deltaSourceList);
}

void DeltaSourceList_initDuplicate(DeltaSourceList       *deltaSourceList,
                                   const DeltaSourceList *fromDeltaSourceList,
                                   const DeltaSourceNode *fromDeltaSourceListFromNode,
                                   const DeltaSourceNode *fromDeltaSourceListToNode
                                  )
{
  assert(deltaSourceList != NULL);
  assert(fromDeltaSourceList != NULL);

  DeltaSourceList_init(deltaSourceList);
  DeltaSourceList_copy(fromDeltaSourceList,deltaSourceList,fromDeltaSourceListFromNode,fromDeltaSourceListToNode);
}

void DeltaSourceList_done(DeltaSourceList *deltaSourceList)
{
  assert(deltaSourceList != NULL);

  List_done(deltaSourceList,(ListNodeFreeFunction)freeDeltaSourceNode,NULL);
}

DeltaSourceList *DeltaSourceList_clear(DeltaSourceList *deltaSourceList)
{
  assert(deltaSourceList != NULL);

  return (DeltaSourceList*)List_clear(deltaSourceList,(ListNodeFreeFunction)freeDeltaSourceNode,NULL);
}

void DeltaSourceList_copy(const DeltaSourceList *fromDeltaSourceList,
                          DeltaSourceList       *toDeltaSourceList,
                          const DeltaSourceNode *fromDeltaSourceListFromNode,
                          const DeltaSourceNode *fromDeltaSourceListToNode
                         )
{
  assert(fromDeltaSourceList != NULL);
  assert(toDeltaSourceList != NULL);

  List_copy(fromDeltaSourceList,toDeltaSourceList,fromDeltaSourceListFromNode,fromDeltaSourceListToNode,NULL,(ListNodeCopyFunction)copyDeltaSourceNode,NULL);
}

Errors DeltaSourceList_append(DeltaSourceList *deltaSourceList,
                              ConstString     storageName,
                              PatternTypes    patternType
                             )
{
  assert(deltaSourceList != NULL);
  assert(storageName != NULL);

  return DeltaSourceList_appendCString(deltaSourceList,String_cString(storageName),patternType);
}

Errors DeltaSourceList_appendCString(DeltaSourceList *deltaSourceList,
                                     const char      *storageName,
                                     PatternTypes    patternType
                                    )
{
  DeltaSourceNode *deltaSourceNode;
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    String    string;
  #endif /* PLATFORM_... */

  assert(deltaSourceList != NULL);
  assert(storageName != NULL);

  // allocate entry node
  deltaSourceNode = LIST_NEW_NODE(DeltaSourceNode);
  if (deltaSourceNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  deltaSourceNode->storageName = String_newCString(storageName);
  deltaSourceNode->patternType = patternType;

  // add to list
  List_append(deltaSourceList,deltaSourceNode);

  return ERROR_NONE;
}

bool DeltaSourceList_match(const DeltaSourceList   *deltaSourceList,
                     ConstString      string,
                     PatternMatchModes patternMatchMode
                    )
{
  bool      matchFlag;
  DeltaSourceNode *deltaSourceNode;

  assert(deltaSourceList != NULL);
  assert(string != NULL);

  matchFlag = FALSE;
  deltaSourceNode = deltaSourceList->head;
  while ((deltaSourceNode != NULL) && !matchFlag)
  {
//    matchFlag = Pattern_match(&deltaSourceNode->pattern,string,patternMatchMode);
    deltaSourceNode = deltaSourceNode->next;
  }

  return matchFlag;
}

bool DeltaSourceList_matchStringList(const DeltaSourceList   *deltaSourceList,
                               const StringList  *stringList,
                               PatternMatchModes patternMatchMode
                              )
{
  bool       matchFlag;
  StringNode *stringNode;

  assert(deltaSourceList != NULL);
  assert(stringList != NULL);

  matchFlag  = FALSE;
  stringNode = stringList->head;
  while ((stringNode != NULL) && !matchFlag)
  {
    matchFlag = DeltaSourceList_match(deltaSourceList,stringNode->string,patternMatchMode);
    stringNode = stringNode->next;
  }

  return matchFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
