/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver entry list functions
* Systems: all
*
\***********************************************************************/

#ifndef __ENTRY_LISTS__
#define __ENTRY_LISTS__

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
#include "common/lists.h"
#include "common/strings.h"
#include "common/stringlists.h"
#include "common/patterns.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef enum
{
  ENTRY_TYPE_FILE,                      // store matching entries as files
  ENTRY_TYPE_IMAGE,                     // store matching entries as block device images
  ENTRY_TYPE_UNKNOWN
} EntryTypes;

typedef struct EntryNode
{
  LIST_NODE_HEADER(struct EntryNode);

  uint         id;                      // unique id
  EntryTypes   type;                    // entry type; see ENTRY_TYPES_...
  String       string;                  // entry string pattern
  PatternTypes patternType;             // pattern type
  Pattern      pattern;                 // compiled entry pattern
} EntryNode;

typedef struct
{
  LIST_HEADER(EntryNode);
} EntryList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : EntryList_initAll
* Purpose: init entry lists
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors EntryList_initAll(void);

/***********************************************************************\
* Name   : EntryList_doneAll
* Purpose: deinitialize entry lists
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

void EntryList_doneAll(void);

/***********************************************************************\
* Name   : EntryList_entryTypeToString
* Purpose: get name of entry type
* Input  : entryType    - entry type
*          defaultValue - default value
* Output : -
* Return : name
* Notes  : -
\***********************************************************************/

const char *EntryList_entryTypeToString(EntryTypes entryType, const char* defaultValue);

/***********************************************************************\
* Name   : EntryList_parseEntryType
* Purpose: get entry type
* Input  : name     - name of entry type
*          userData - user data (not used)
* Output : entryType - entry type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool EntryList_parseEntryType(const char *name, EntryTypes *entryType, void *userData);

/***********************************************************************\
* Name   : EntryList_init
* Purpose: init entry list
* Input  : entryList - entry list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void EntryList_init(EntryList *entryList);

/***********************************************************************\
* Name   : EntryList_initDuplicaet
* Purpose: init duplicated entry list
* Input  : entryList             - entry list
*          fromEntryList         - from entry list (source)
*          fromEntryListFromNode - from node (could be NULL)
*          fromEntryListToNode   - to node (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void EntryList_initDuplicate(EntryList       *entryList,
                             const EntryList *fromEntryList,
                             const EntryNode *fromEntryListFromNode,
                             const EntryNode *fromEntryListToNode
                            );

/***********************************************************************\
* Name   : EntryList_done
* Purpose: done entry list
* Input  : entryList - entry list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void EntryList_done(EntryList *entryList);

/***********************************************************************\
* Name   : EntryList_clear
* Purpose: remove all entrys in list
* Input  : entryList - entry list
* Output : -
* Return : entry list
* Notes  : -
\***********************************************************************/

EntryList *EntryList_clear(EntryList *entryList);

/***********************************************************************\
* Name   : Entry_copyList
* Purpose: copy all entrys from source list to destination list
* Input  : toEntryList           - to entry list (destination)
*          fromEntryList         - from entry list (source)
*          fromEntryListFromNode - from node (could be NULL)
*          fromEntryListToNode   - to node (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void EntryList_copy(EntryList       *toEntryList,
                    const EntryList *fromEntryList,
                    const EntryNode *fromEntryListFromNode,
                    const EntryNode *fromEntryListToNode
                   );

/***********************************************************************\
* Name   : EntryList_move
* Purpose: move all entrys from source list to destination list
* Input  : toEntryList           - to entry list (destination)
*          fromEntryList         - from entry list (source)
*          fromEntryListFromNode - from node (could be NULL)
*          fromEntryListToNode   - to node (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void EntryList_move(EntryList       *toEntryList,
                    EntryList       *fromEntryList,
                    const EntryNode *fromEntryListFromNode,
                    const EntryNode *fromEntryListToNode
                   );

/***********************************************************************\
* Name   : EntryList_append, EntryList_appendCString
* Purpose: add entry to entry list
* Input  : entryList   - entry list
+          type        - entry type; see ENTRY_TYPE_*
*          string      - string
*          patternType - pattern type; see PATTERN_TYPE_*
* Output : id - entry node id (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors EntryList_append(EntryList    *entryList,
                        EntryTypes   type,
                        ConstString  string,
                        PatternTypes patternType,
                        uint         *id
                       );
Errors EntryList_appendCString(EntryList    *entryList,
                               EntryTypes   type,
                               const char   *string,
                               PatternTypes patternType,
                               uint         *id
                              );

/***********************************************************************\
* Name   : EntryList_update, EntryList_updateCString
* Purpose: update entry in entry list
* Input  : entryList   - entry list
+          id          - entry node id
+          type        - entry type; see ENTRY_TYPE_*
*          string      - string
*          patternType - pattern type; see PATTERN_TYPE_*
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors EntryList_update(EntryList    *entryList,
                        uint         id,
                        EntryTypes   type,
                        ConstString  string,
                        PatternTypes patternType
                       );
Errors EntryList_updateCString(EntryList    *entryList,
                               uint         id,
                               EntryTypes   type,
                               const char   *string,
                               PatternTypes patternType
                              );

/***********************************************************************\
* Name   : EntryList_remove
* Purpose: remove entry from entry list
* Input  : entryList - entry list
+          id        - entry node id
* Output : -
* Return : TRUE iff removed
* Notes  : -
\***********************************************************************/

bool EntryList_remove(EntryList *entryList,
                      uint      id
                     );

/***********************************************************************\
* Name   : EntryList_match, EntryList_matchStringList
* Purpose: patch string/string list with all entrys of list
* Input  : entryList        - entry list
*          string           - string
*          stringList       - string list
*          patternMatchMode - pattern match mode; see PatternMatchModes
* Output : -
* Return : TRUE if entry match, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool EntryList_match(const EntryList   *entryList,
                     ConstString       string,
                     PatternMatchModes patternMatchMode
                    );
bool EntryList_matchStringList(const EntryList   *entryList,
                               const StringList  *stringList,
                               PatternMatchModes patternMatchMode
                              );

#ifdef __cplusplus
  }
#endif

#endif /* __ENTRY_LISTS__ */

/* end of file */
