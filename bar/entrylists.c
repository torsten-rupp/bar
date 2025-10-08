/***********************************************************************\
*
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
  const char      *name;
  EntryStoreTypes entryStoreType;
} ENTRY_STORE_TYPES[] =
{
  { "file",  ENTRY_STORE_TYPE_FILE  },
  { "image", ENTRY_STORE_TYPE_IMAGE }
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
  Errors error;

  assert(entryNode != NULL);

  UNUSED_VARIABLE(userData);

  // allocate entry node
  EntryNode *newEntryNode = LIST_NEW_NODE(EntryNode);
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
  newEntryNode->storeType   = entryNode->storeType;
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
    String string = String_duplicate(entryNode->string);
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

const char *EntryList_entryStoreTypeToString(EntryStoreTypes entryStoreType, const char *defaultValue)
{
  uint i = 0;
  while (   (i < SIZE_OF_ARRAY(ENTRY_STORE_TYPES))
         && (ENTRY_STORE_TYPES[i].entryStoreType != entryStoreType)
        )
  {
    i++;
  }
  const char *name;
  if (i < SIZE_OF_ARRAY(ENTRY_STORE_TYPES))
  {
    name = ENTRY_STORE_TYPES[i].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool EntryList_parseEntryStoreType(const char *name, EntryStoreTypes *entryStoreType, void *userData)
{
  assert(name != NULL);
  assert(entryStoreType != NULL);

  UNUSED_VARIABLE(userData);

  uint i = 0;
  while (   (i < SIZE_OF_ARRAY(ENTRY_STORE_TYPES))
         && !stringEqualsIgnoreCase(ENTRY_STORE_TYPES[i].name,name)
        )
  {
    i++;
  }
  if (i < SIZE_OF_ARRAY(ENTRY_STORE_TYPES))
  {
    (*entryStoreType) = ENTRY_STORE_TYPES[i].entryStoreType;
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

  List_init(entryList,CALLBACK_((ListNodeDuplicateFunction)duplicateEntryNode,NULL),CALLBACK_((ListNodeFreeFunction)freeEntryNode,NULL));
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

  List_done(entryList);
}

EntryList *EntryList_clear(EntryList *entryList)
{
  assert(entryList != NULL);

  return (EntryList*)List_clear(entryList);
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
            fromEntryListToNode
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

bool EntryList_contains(EntryList       *entryList,
                        EntryStoreTypes entryStoreType,
                        ConstString     string,
                        PatternTypes    patternType
                       )
{
  assert(entryList != NULL);
  assert(string != NULL);

  EntryNode *entryNode;
  return LIST_CONTAINS(entryList,
                       entryNode,
                          (entryNode->storeType == entryStoreType)
                       && (entryNode->patternType == patternType)
                       && String_equals(entryNode->string,string)
                      );
}

Errors EntryList_append(EntryList       *entryList,
                        EntryStoreTypes entryStoreType,
                        ConstString     string,
                        PatternTypes    patternType,
                        uint            *id
                       )
{
  assert(entryList != NULL);
  assert(string != NULL);

  return EntryList_appendCString(entryList,entryStoreType,String_cString(string),patternType,id);
}

Errors EntryList_appendCString(EntryList       *entryList,
                               EntryStoreTypes entryStoreType,
                               const char      *string,
                               PatternTypes    patternType,
                               uint            *id
                              )
{
  Errors error;

  assert(entryList != NULL);
  assert(string != NULL);

  // allocate entry node
  EntryNode *entryNode = LIST_NEW_NODE(EntryNode);
  if (entryNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  #ifndef NDEBUG
    entryNode->id        = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  #else
    entryNode->id        = Misc_getId();
  #endif
  entryNode->storeType   = entryStoreType;
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
    String escapedString = String_newCString(string);
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

Errors EntryList_update(EntryList       *entryList,
                        uint            id,
                        EntryStoreTypes entryStoreType,
                        ConstString     string,
                        PatternTypes    patternType
                       )
{
  assert(entryList != NULL);
  assert(string != NULL);

  return EntryList_updateCString(entryList,id,entryStoreType,String_cString(string),patternType);
}

Errors EntryList_updateCString(EntryList       *entryList,
                               uint            id,
                               EntryStoreTypes entryStoreType,
                               const char      *string,
                               PatternTypes    patternType
                              )
{
  Errors error;

  assert(entryList != NULL);
  assert(string != NULL);

  // find pattern node
  EntryNode *entryNode = (EntryNode*)LIST_FIND(entryList,entryNode,entryNode->id == id);
  if (entryNode != NULL)
  {
    // init pattern
    Pattern pattern;
    #if   defined(PLATFORM_LINUX)
      error = Pattern_initCString(&pattern,
                                  string,
                                  patternType,
                                  PATTERN_FLAG_NONE
                                 );
    #elif defined(PLATFORM_WINDOWS)
      // escape all '\' by '\\'
      String escapedString = String_newCString(string);
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
    entryNode->storeType   = entryStoreType;
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
  assert(entryList != NULL);

  EntryNode *entryNode = (EntryNode*)LIST_FIND(entryList,entryNode,entryNode->id == id);
  if (entryNode != NULL)
  {
    List_removeAndFree(entryList,entryNode);
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

  assert(entryList != NULL);
  assert(string != NULL);

  bool      matchFlag  = FALSE;
  EntryNode *entryNode = entryList->head;
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
  assert(entryList != NULL);
  assert(stringList != NULL);

  bool       matchFlag   = FALSE;
  StringNode *stringNode = stringList->head;
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
