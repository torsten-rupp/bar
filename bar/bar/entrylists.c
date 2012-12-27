/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver entry list functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

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

#include "entrylists.h"

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
* Name   : copyEntryNode
* Purpose: copy allocated entry node
* Input  : entryNode - entry node
* Output : -
* Return : copied entry node
* Notes  : -
\***********************************************************************/

LOCAL EntryNode *copyEntryNode(EntryNode *entryNode,
                               void      *userData
                              )
{
  EntryNode *newEntryNode;
  Errors      error;

  assert(entryNode != NULL);

  UNUSED_VARIABLE(userData);

  // allocate entry node
  newEntryNode = LIST_NEW_NODE(EntryNode);
  if (newEntryNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // create entry
  newEntryNode->type   = entryNode->type;
  newEntryNode->string = String_duplicate(entryNode->string);
  error = Pattern_init(&newEntryNode->pattern,
                       entryNode->string,
                       entryNode->pattern.type
                      );
  if (error != ERROR_NONE)
  {
    String_delete(newEntryNode->string);
    LIST_DELETE_NODE(newEntryNode);
    return NULL;
  }

  return newEntryNode;
}

/***********************************************************************\
* Name   : freeEntryNode
* Purpose: free allocated entry node
* Input  : entryNode - entry node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeEntryNode(EntryNode *entryNode,
                         void      *userData
                        )
{
  assert(entryNode != NULL);
  assert(entryNode->string != NULL);

  UNUSED_VARIABLE(userData);

  Pattern_done(&entryNode->pattern);
  String_delete(entryNode->string);
}

/*---------------------------------------------------------------------*/

Errors EntryList_initAll(void)
{
  return ERROR_NONE;
}

void EntryList_doneAll(void)
{
}

void EntryList_init(EntryList *entryList)
{
  assert(entryList != NULL);

  List_init(entryList);
}

void EntryList_done(EntryList *entryList)
{
  assert(entryList != NULL);

  List_done(entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
}

void EntryList_clear(EntryList *entryList)
{
  assert(entryList != NULL);

  List_clear(entryList,(ListNodeFreeFunction)freeEntryNode,NULL);
}

void EntryList_copy(const EntryList *fromEntryList, EntryList *toEntryList)
{
  assert(fromEntryList != NULL);
  assert(toEntryList != NULL);

  List_copy(fromEntryList,toEntryList,NULL,NULL,NULL,(ListNodeCopyFunction)copyEntryNode,NULL);
}

void EntryList_move(EntryList *fromEntryList, EntryList *toEntryList)
{
  assert(fromEntryList != NULL);
  assert(toEntryList != NULL);

  List_move(fromEntryList,toEntryList,NULL,NULL,NULL);
}

Errors EntryList_append(EntryList    *entryList,
                        EntryTypes   type,
                        const String pattern,
                        PatternTypes patternType
                       )
{
  assert(entryList != NULL);
  assert(pattern != NULL);

  return EntryList_appendCString(entryList,type,String_cString(pattern),patternType);
}

Errors EntryList_appendCString(EntryList    *entryList,
                               EntryTypes   type,
                               const char   *pattern,
                               PatternTypes patternType
                              )
{
  EntryNode *entryNode;
  Errors      error;

  assert(entryList != NULL);
  assert(pattern != NULL);

  // allocate entry node
  entryNode = LIST_NEW_NODE(EntryNode);
  if (entryNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  entryNode->type   = type;
  entryNode->string = String_newCString(pattern);

  // init pattern
  error = Pattern_initCString(&entryNode->pattern,
                              pattern,
                              patternType
                             );
  if (error != ERROR_NONE)
  {
    String_delete(entryNode->string);
    LIST_DELETE_NODE(entryNode);
    return error;
  }

  // add to list
  List_append(entryList,entryNode);

  return ERROR_NONE;
}

bool EntryList_match(const EntryList   *entryList,
                     const String      string,
                     PatternMatchModes patternMatchMode
                    )
{
  bool      matchFlag;
  EntryNode *entryNode;

  assert(entryList != NULL);
  assert(string != NULL);

  matchFlag = FALSE;
  entryNode = entryList->head;
  while ((entryNode != NULL) && !matchFlag)
  {
    matchFlag = Pattern_match(&entryNode->pattern,string,patternMatchMode);
    entryNode = entryNode->next;
  }

  return matchFlag;
}

bool EntryList_matchStringList(const EntryList   *entryList,
                               const StringList  *stringList,
                               PatternMatchModes patternMatchMode
                              )
{
  bool       matchFlag;
  StringNode *stringNode;

  assert(entryList != NULL);
  assert(stringList != NULL);

  matchFlag  = FALSE;
  stringNode = stringList->head;
  while ((stringNode != NULL) && !matchFlag)
  {
    matchFlag = EntryList_match(entryList,stringNode->string,patternMatchMode);
    stringNode = stringNode->next;
  }

  return matchFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
