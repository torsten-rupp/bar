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

#include "common/global.h"
#include "common/lists.h"
#include "common/misc.h"
#include "common/strings.h"
#include "common/stringlists.h"

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "common/patterns.h"

#include "entrylists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
LOCAL const struct
{
  const char *name;
  EntryTypes entryType;
} ENTRY_TYPES[] =
{
  { "file",  ENTRY_TYPE_FILE  },
  { "image", ENTRY_TYPE_IMAGE }
};

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : duplicateEntryNode
* Purpose: duplicate entry node
* Input  : entryNode - entry node
* Output : -
* Return : copied entry node
* Notes  : -
\***********************************************************************/

LOCAL EntryNode *duplicateEntryNode(EntryNode *entryNode,
                                    void      *userData
                                   )
{
  EntryNode *newEntryNode;
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    String    string;
  #endif /* PLATFORM_... */
  Errors    error;

  assert(entryNode != NULL);

  UNUSED_VARIABLE(userData);

  // allocate entry node
  newEntryNode = LIST_NEW_NODE(EntryNode);
  if (newEntryNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // create entry
  #ifndef NDEBUG
    newEntryNode->id        = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  #else
    newEntryNode->id        = Misc_getId();
  #endif
  newEntryNode->type        = entryNode->type;
  newEntryNode->string      = String_duplicate(entryNode->string);
  newEntryNode->patternType = entryNode->patternType;
  #if   defined(PLATFORM_LINUX)
    error = Pattern_init(&newEntryNode->pattern,
                         entryNode->string,
                         entryNode->pattern.type,
                         PATTERN_FLAG_NONE
                        );
  #elif defined(PLATFORM_WINDOWS)
    // escape all '\' by '\\'
    string = String_duplicate(entryNode->string);
    String_replaceAllCString(string,STRING_BEGIN,"\\","\\\\");

    error = Pattern_init(&newEntryNode->pattern,
                         string,
                         entryNode->pattern.type,
                         PATTERN_FLAG_IGNORE_CASE
                        );

    // free resources
    String_delete(string);
  #endif /* PLATFORM_... */
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

const char *EntryList_entryTypeToString(EntryTypes entryType, const char *defaultValue)
{
  uint       i;
  const char *name;

  i = 0;
  while (   (i < SIZE_OF_ARRAY(ENTRY_TYPES))
         && (ENTRY_TYPES[i].entryType != entryType)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(ENTRY_TYPES))
  {
    name = ENTRY_TYPES[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool EntryList_parseEntryType(const char *name, EntryTypes *entryType, void *userData)
{
  uint i;

  assert(name != NULL);
  assert(entryType != NULL);

  UNUSED_VARIABLE(userData);

  i = 0;
  while (   (i < SIZE_OF_ARRAY(ENTRY_TYPES))
         && !stringEqualsIgnoreCase(ENTRY_TYPES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(ENTRY_TYPES))
  {
    (*entryType) = ENTRY_TYPES[i].entryType;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

void EntryList_init(EntryList *entryList)
{
  assert(entryList != NULL);

  List_init(entryList);
}

void EntryList_initDuplicate(EntryList       *entryList,
                             const EntryList *fromEntryList,
                             const EntryNode *fromEntryListFromNode,
                             const EntryNode *fromEntryListToNode
                            )
{
  assert(entryList != NULL);

  EntryList_init(entryList);
  EntryList_copy(entryList,
                 fromEntryList,
                 fromEntryListFromNode,
                 fromEntryListToNode
                );
}

void EntryList_done(EntryList *entryList)
{
  assert(entryList != NULL);

  List_done(entryList,CALLBACK_((ListNodeFreeFunction)freeEntryNode,NULL));
}

EntryList *EntryList_clear(EntryList *entryList)
{
  assert(entryList != NULL);

  return (EntryList*)List_clear(entryList,CALLBACK_((ListNodeFreeFunction)freeEntryNode,NULL));
}

void EntryList_copy(EntryList       *toEntryList,
                    const EntryList *fromEntryList,
                    const EntryNode *fromEntryListFromNode,
                    const EntryNode *fromEntryListToNode
                   )
{
  assert(fromEntryList != NULL);
  assert(toEntryList != NULL);

  List_copy(toEntryList,
            NULL,  // toEntryNode
            fromEntryList,
            fromEntryListFromNode,
            fromEntryListToNode,
            CALLBACK_((ListNodeDuplicateFunction)duplicateEntryNode,NULL)
           );
}

void EntryList_move(EntryList       *toEntryList,
                    EntryList       *fromEntryList,
                    const EntryNode *fromEntryListFromNode,
                    const EntryNode *fromEntryListToNode
                   )
{
  assert(toEntryList != NULL);
  assert(fromEntryList != NULL);

  List_move(toEntryList,NULL,fromEntryList,fromEntryListFromNode,fromEntryListToNode);
}

Errors EntryList_append(EntryList    *entryList,
                        EntryTypes   type,
                        ConstString  string,
                        PatternTypes patternType,
                        uint         *id
                       )
{
  assert(entryList != NULL);
  assert(string != NULL);

  return EntryList_appendCString(entryList,type,String_cString(string),patternType,id);
}

Errors EntryList_appendCString(EntryList    *entryList,
                               EntryTypes   type,
                               const char   *string,
                               PatternTypes patternType,
                               uint         *id
                              )
{
  EntryNode *entryNode;
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    String    escapedString;
  #endif /* PLATFORM_... */
  Errors    error;

  assert(entryList != NULL);
  assert(string != NULL);

  // allocate entry node
  entryNode = LIST_NEW_NODE(EntryNode);
  if (entryNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  #ifndef NDEBUG
    entryNode->id        = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  #else
    entryNode->id        = Misc_getId();
  #endif
  entryNode->type        = type;
  entryNode->string      = String_newCString(string);
  entryNode->patternType = patternType;

  // init pattern
  #if   defined(PLATFORM_LINUX)
    error = Pattern_initCString(&entryNode->pattern,
                                string,
                                patternType,
                                PATTERN_FLAG_NONE
                               );
  #elif defined(PLATFORM_WINDOWS)
    // escape all '\' by '\\'
    escapedString = String_newCString(string);
    String_replaceAllCString(escapedString,STRING_BEGIN,"\\","\\\\");

    error = Pattern_init(&entryNode->pattern,
                         escapedString,
                         patternType,
                         PATTERN_FLAG_IGNORE_CASE
                        );

    // free resources
    String_delete(escapedString);
  #endif /* PLATFORM_... */
  if (error != ERROR_NONE)
  {
    String_delete(entryNode->string);
    LIST_DELETE_NODE(entryNode);
    return error;
  }

  // add to list
  List_append(entryList,entryNode);

  if (id != NULL) (*id) = entryNode->id;

  return ERROR_NONE;
}

Errors EntryList_update(EntryList    *entryList,
                        uint         id,
                        EntryTypes   type,
                        ConstString  string,
                        PatternTypes patternType
                       )
{
  assert(entryList != NULL);
  assert(string != NULL);

  return EntryList_updateCString(entryList,id,type,String_cString(string),patternType);
}

Errors EntryList_updateCString(EntryList    *entryList,
                               uint         id,
                               EntryTypes   type,
                               const char   *string,
                               PatternTypes patternType
                              )
{
  EntryNode *entryNode;
  Pattern   pattern;
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
    String    escapedString;
  #endif /* PLATFORM_... */
  Errors    error;

  assert(entryList != NULL);
  assert(string != NULL);

  // find pattern node
  entryNode = (EntryNode*)LIST_FIND(entryList,entryNode,entryNode->id == id);
  if (entryNode != NULL)
  {
    // init pattern
    #if   defined(PLATFORM_LINUX)
      error = Pattern_initCString(&pattern,
                                  string,
                                  patternType,
                                  PATTERN_FLAG_NONE
                                 );
    #elif defined(PLATFORM_WINDOWS)
      // escape all '\' by '\\'
      escapedString = String_newCString(string);
      String_replaceAllCString(escapedString,STRING_BEGIN,"\\","\\\\");

      error = Pattern_init(&pattern,
                           escapedString,
                           patternType,
                           PATTERN_FLAG_IGNORE_CASE
                          );

      // free resources
      String_delete(escapedString);
    #endif /* PLATFORM_... */
    if (error != ERROR_NONE)
    {
      return error;
    }

    // store
    entryNode->type        = type;
    String_setCString(entryNode->string,string);
    entryNode->patternType = patternType;
    Pattern_done(&entryNode->pattern);
    entryNode->pattern     = pattern;
  }

  return ERROR_NONE;
}

bool EntryList_remove(EntryList *entryList,
                      uint      id
                     )
{
  EntryNode *entryNode;

  assert(entryList != NULL);

  entryNode = (EntryNode*)LIST_FIND(entryList,entryNode,entryNode->id == id);
  if (entryNode != NULL)
  {
    List_removeAndFree(entryList,entryNode,(ListNodeFreeFunction)freeEntryNode,NULL);
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

bool EntryList_match(const EntryList   *entryList,
                     ConstString       string,
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
    matchFlag = Pattern_match(&entryNode->pattern,string,STRING_BEGIN,patternMatchMode,NULL,NULL);
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
