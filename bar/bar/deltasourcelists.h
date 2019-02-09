/***********************************************************************\
*
* $Revision: 3620 $
* $Date: 2015-02-06 20:57:40 +0100 (Fri, 06 Feb 2015) $
* $Author: torsten $
* Contents: Backup ARchiver delta source list functions
* Systems: all
*
\***********************************************************************/

#ifndef __DELTA_SOURCE_LISTS__
#define __DELTA_SOURCE_LISTS__

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
#include "common/patterns.h"
#include "common/semaphores.h"

#include "errors.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct DeltaSourceNode
{
  LIST_NODE_HEADER(struct DeltaSourceNode);

  uint         id;                  // unique node id
  String       storageName;         // storage name
  PatternTypes patternType;         // pattern type
  bool         locked;              // TRUE iff locked
} DeltaSourceNode;

typedef struct
{
  LIST_HEADER(DeltaSourceNode);
  Semaphore lock;
} DeltaSourceList;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define DeltaSourceList_init(...)          __DeltaSourceList_init(__FILE__,__LINE__, ## __VA_ARGS__)
  #define DeltaSourceList_initDuplicate(...) __DeltaSourceList_initDuplicate(__FILE__,__LINE__, ## __VA_ARGS__)
  #define DeltaSourceList_done(...)          __DeltaSourceList_done(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : DeltaSourceList_initAll
* Purpose: init delta source lists
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors DeltaSourceList_initAll(void);

/***********************************************************************\
* Name   : DeltaSourceList_doneAll
* Purpose: deinitialize delta source lists
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

void DeltaSourceList_doneAll(void);

/***********************************************************************\
* Name   : DeltaSourceList_init
* Purpose: init delta source list
* Input  : deltaSourceList - delta source list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void DeltaSourceList_init(DeltaSourceList *deltaSourceList);
#else /* not NDEBUG */
  void __DeltaSourceList_init(const char      *__fileName__,
                              ulong           __lineNb__,
                              DeltaSourceList *deltaSourceList
                             );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : DeltaSourceList_initDuplicate
* Purpose: init duplicated delta source list
* Input  : deltaSourceList             - delta source list
*          fromDeltaSourceList         - from delta source list (source)
*          fromDeltaSourceListFromNode - from node (could be NULL)
*          fromDeltaSourceListToNode   - to node (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void DeltaSourceList_initDuplicate(DeltaSourceList       *deltaSourceList,
                                     const DeltaSourceList *fromDeltaSourceList,
                                     const DeltaSourceNode *fromDeltaSourceListFromNode,
                                     const DeltaSourceNode *fromDeltaSourceListToNode
                                    );
#else /* not NDEBUG */
  void __DeltaSourceList_initDuplicate(const char            *__fileName__,
                                       ulong                 __lineNb__,
                                       DeltaSourceList       *deltaSourceList,
                                       const DeltaSourceList *fromDeltaSourceList,
                                       const DeltaSourceNode *fromDeltaSourceListFromNode,
                                       const DeltaSourceNode *fromDeltaSourceListToNode
                                      );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : DeltaSourceList_done
* Purpose: done delta source list
* Input  : deltaSourceList - delta source list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void DeltaSourceList_done(DeltaSourceList *deltaSourceList);
#else /* not NDEBUG */
  void __DeltaSourceList_done(const char      *__fileName__,
                              ulong           __lineNb__,
                              DeltaSourceList *deltaSourceList
                             );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : DeltaSourceList_clear
* Purpose: remove all in list
* Input  : deltaSourceList - delta source list
* Output : -
* Return : delta source list
* Notes  : -
\***********************************************************************/

DeltaSourceList *DeltaSourceList_clear(DeltaSourceList *deltaSourceList);

/***********************************************************************\
* Name   : DeltaSourceList_copy
* Purpose: copy all delta sources from source list to destination list
* Input  : toDeltaSourceList           - to delta source list (destination)
*          fromDeltaSourceList         - from delta source list (source)
*          fromDeltaSourceListFromNode - from node (could be NULL)
*          fromDeltaSourceListToNode   - to node (could be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void DeltaSourceList_copy(DeltaSourceList       *toDeltaSourceList,
                          const DeltaSourceList *fromDeltaSourceList,
                          const DeltaSourceNode *fromDeltaSourceListFromNode,
                          const DeltaSourceNode *fromDeltaSourceListToNode
                         );

/***********************************************************************\
* Name   : DeltaSourceList_append
* Purpose: add entry to delta source list
* Input  : entryList   - delta source list
+          type        - entry type; see ENTRY_TYPE_*
*          pattern     - pattern
*          patternType - pattern type; see PATTERN_TYPE_*
* Output : id - id (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors DeltaSourceList_append(DeltaSourceList *deltaSourceList,
                              ConstString     storageName,
                              PatternTypes    patternType,
                              uint            *id
                             );

/***********************************************************************\
* Name   : DeltaSourceList_append
* Purpose: add entry to delta source list
* Input  : entryList   - delta source list
+          type        - entry type; see ENTRY_TYPE_*
*          pattern     - pattern
*          patternType - pattern type; see PATTERN_TYPE_*
* Output : id - id (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors DeltaSourceList_update(DeltaSourceList *deltaSourceList,
                              uint            id,
                              ConstString     storageName,
                              PatternTypes    patternType
                             );

/***********************************************************************\
* Name   : DeltaSourceList_remove
* Purpose: remove entry from delta source list
* Input  : entryList   - delta source list
+          type        - entry type; see ENTRY_TYPE_*
*          pattern     - pattern
*          patternType - pattern type; see PATTERN_TYPE_*
* Output : id - id (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

bool DeltaSourceList_remove(DeltaSourceList *deltaSourceList,
                            uint            id
                           );

/***********************************************************************\
* Name   : DeltaSourceList_match, DeltaSourceList_matchStringList
* Purpose: patch string/string list with all entrys of list
* Input  : entryList        - delta source list
*          string           - string
*          stringList       - string list
*          patternMatchMode - pattern match mode; see PatternMatchModes
* Output : -
* Return : TRUE if entry match, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool DeltaSourceList_match(const DeltaSourceList *entryList,
                           ConstString           string,
                           PatternMatchModes     patternMatchMode
                         );

#ifdef __cplusplus
  }
#endif

#endif /* __DELTA_SOURCE_LISTS__ */

/* end of file */
