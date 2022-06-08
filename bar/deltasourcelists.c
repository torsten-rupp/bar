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

#include "common/global.h"
#include "common/lists.h"
#include "common/strings.h"
#include "common/stringlists.h"

// TODO: remove bar.h
#include "bar.h"
#include "bar_common.h"
#include "common/patterns.h"
#include "storage.h"

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
* Name   : duplicateDeltaSourceNode
* Purpose: duplicate delta source node
* Input  : deltaSourceNode - delta source node
* Output : -
* Return : copied delta source node
* Notes  : -
\***********************************************************************/

LOCAL DeltaSourceNode *duplicateDeltaSourceNode(const DeltaSourceNode *deltaSourceNode,
                                                void                  *userData
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
  #ifndef NDEBUG
    newDeltaSourceNode->id        = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
  #else
    newDeltaSourceNode->id        = Misc_getId();
  #endif
  newDeltaSourceNode->storageName = String_duplicate(deltaSourceNode->storageName);
  newDeltaSourceNode->patternType = deltaSourceNode->patternType;
  newDeltaSourceNode->locked      = FALSE;

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

#ifdef NDEBUG
  void DeltaSourceList_init(DeltaSourceList *deltaSourceList)
#else /* not NDEBUG */
  void __DeltaSourceList_init(const char      *__fileName__,
                              ulong           __lineNb__,
                              DeltaSourceList *deltaSourceList
                             )
#endif /* NDEBUG */
{
  assert(deltaSourceList != NULL);

  List_init(deltaSourceList,
            CALLBACK_((ListNodeDuplicateFunction)duplicateDeltaSourceNode,NULL),
            CALLBACK_((ListNodeFreeFunction)freeDeltaSourceNode,NULL)
           );
  Semaphore_init(&deltaSourceList->lock,SEMAPHORE_TYPE_BINARY);

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(deltaSourceList,DeltaSourceList);
  #else /* not NDEBUG */
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,deltaSourceList,DeltaSourceList);
  #endif /* NDEBUG */
}

#ifdef NDEBUG
  void DeltaSourceList_initDuplicate(DeltaSourceList       *deltaSourceList,
                                     const DeltaSourceList *fromDeltaSourceList,
                                     const DeltaSourceNode *fromDeltaSourceListFromNode,
                                     const DeltaSourceNode *fromDeltaSourceListToNode
                                    )
#else /* not NDEBUG */
  void __DeltaSourceList_initDuplicate(const char        *__fileName__,
                                       ulong             __lineNb__,
                                       DeltaSourceList       *deltaSourceList,
                                       const DeltaSourceList *fromDeltaSourceList,
                                       const DeltaSourceNode *fromDeltaSourceListFromNode,
                                       const DeltaSourceNode *fromDeltaSourceListToNode
                                      )
#endif /* NDEBUG */
{
  assert(deltaSourceList != NULL);
  assert(fromDeltaSourceList != NULL);

  #ifdef NDEBUG
    DeltaSourceList_init(deltaSourceList);
  #else /* not NDEBUG */
    __DeltaSourceList_init(__fileName__,__lineNb__,deltaSourceList);
  #endif /* NDEBUG */
  DeltaSourceList_copy(deltaSourceList,fromDeltaSourceList,fromDeltaSourceListFromNode,fromDeltaSourceListToNode);
}

#ifdef NDEBUG
  void DeltaSourceList_done(DeltaSourceList *deltaSourceList)
#else /* not NDEBUG */
  void __DeltaSourceList_done(const char      *__fileName__,
                              ulong           __lineNb__,
                              DeltaSourceList *deltaSourceList
                             )
#endif /* NDEBUG */
{
  assert(deltaSourceList != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(deltaSourceList,DeltaSourceList);
  #else /* not NDEBUG */
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,deltaSourceList,DeltaSourceList);
  #endif /* NDEBUG */

  Semaphore_done(&deltaSourceList->lock);
  List_done(deltaSourceList);
}

DeltaSourceList *DeltaSourceList_clear(DeltaSourceList *deltaSourceList)
{
  assert(deltaSourceList != NULL);

  return (DeltaSourceList*)List_clear(deltaSourceList);
}

void DeltaSourceList_copy(DeltaSourceList       *toDeltaSourceList,
                          const DeltaSourceList *fromDeltaSourceList,
                          const DeltaSourceNode *fromDeltaSourceListFromNode,
                          const DeltaSourceNode *fromDeltaSourceListToNode
                         )
{
  assert(toDeltaSourceList != NULL);
  assert(fromDeltaSourceList != NULL);

  List_copy(toDeltaSourceList,NULL,fromDeltaSourceList,fromDeltaSourceListFromNode,fromDeltaSourceListToNode);
}

Errors DeltaSourceList_append(DeltaSourceList *deltaSourceList,
                              ConstString     storageName,
                              PatternTypes    patternType,
                              uint            *id
                             )
{
  String                     printableStorageName;
  StorageSpecifier           storageSpecifier;
  Errors                     error;
  DeltaSourceNode            *deltaSourceNode;
  JobOptions                 jobOptions;
  StorageDirectoryListHandle storageDirectoryListHandle;
  Pattern                    pattern;
  String                     fileName;
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  assert(deltaSourceList != NULL);
  assert(storageName != NULL);

  // init variables
  printableStorageName = String_new();

  // parse storage name
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }

  // get printable storage name
  Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);

  deltaSourceNode = NULL;

  if (String_isEmpty(storageSpecifier.archivePatternString))
  {
    // add file entry
    deltaSourceNode = LIST_NEW_NODE(DeltaSourceNode);
    if (deltaSourceNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    #ifndef NDEBUG
      deltaSourceNode->id          = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
    #else
      deltaSourceNode->id          = Misc_getId();
    #endif
    deltaSourceNode->storageName = String_duplicate(storageName);
    deltaSourceNode->patternType = patternType;
    deltaSourceNode->locked      = FALSE;

    // add to list
    List_append(deltaSourceList,deltaSourceNode);
  }
  else
  {
    // add matching files
    Job_initOptions(&jobOptions);

    //open directory list
    error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                      &storageSpecifier,
                                      NULL,  // archiveName
                                      &jobOptions,
                                      SERVER_CONNECTION_PRIORITY_LOW
                                     );
    if (error == ERROR_NONE)
    {
      error = Pattern_init(&pattern,
                           storageSpecifier.archivePatternString,
                           patternType,
                           PATTERN_FLAG_NONE
                          );
      if (error == ERROR_NONE)
      {
        fileName = String_new();
        while (!Storage_endOfDirectoryList(&storageDirectoryListHandle) && (error == ERROR_NONE))
        {
          // read next directory entry
          error = Storage_readDirectoryList(&storageDirectoryListHandle,fileName,NULL);
          if (error != ERROR_NONE)
          {
            continue;
          }

         // match pattern
          if (!Pattern_match(&pattern,fileName,STRING_BEGIN,PATTERN_MATCH_MODE_EXACT,NULL,NULL))
          {
            continue;
          }

          // add file entry
          deltaSourceNode = LIST_NEW_NODE(DeltaSourceNode);
          if (deltaSourceNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          #ifndef NDEBUG
            deltaSourceNode->id          = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
          #else
            deltaSourceNode->id          = Misc_getId();
          #endif
          deltaSourceNode->storageName = String_duplicate(fileName);
          deltaSourceNode->patternType = patternType;
          deltaSourceNode->locked      = FALSE;

          List_append(deltaSourceList,deltaSourceNode);
        }
        String_delete(fileName);
        Pattern_done(&pattern);
      }
      Storage_closeDirectoryList(&storageDirectoryListHandle);
    }
    Job_doneOptions(&jobOptions);
  }

  // add file entry directly if no matching entry found in directory
  if (deltaSourceNode == NULL)
  {
    printWarning("no matching entry for delta source '%s' found",
                 String_cString(printableStorageName)
                );

    deltaSourceNode = LIST_NEW_NODE(DeltaSourceNode);
    if (deltaSourceNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    #ifndef NDEBUG
      deltaSourceNode->id          = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
    #else
      deltaSourceNode->id          = Misc_getId();
    #endif
    deltaSourceNode->storageName = String_duplicate(storageName);
    deltaSourceNode->patternType = patternType;
    deltaSourceNode->locked      = FALSE;

    List_append(deltaSourceList,deltaSourceNode);
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(printableStorageName);

  if (id != NULL) (*id) = deltaSourceNode->id;

  return ERROR_NONE;
}

Errors DeltaSourceList_update(DeltaSourceList *deltaSourceList,
                              uint            id,
                              ConstString     storageName,
                              PatternTypes    patternType
                             )
{
  String                     printableStorageName;
  StorageSpecifier           storageSpecifier;
  Errors                     error;
  DeltaSourceNode            *deltaSourceNode;
  JobOptions                 jobOptions;
  StorageDirectoryListHandle storageDirectoryListHandle;
  Pattern                    pattern;
  String                     fileName;
  #if   defined(PLATFORM_LINUX)
  #elif defined(PLATFORM_WINDOWS)
  #endif /* PLATFORM_... */

  assert(deltaSourceList != NULL);
  assert(storageName != NULL);

HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
UNUSED_VARIABLE(id);

  // init variables
  printableStorageName = String_new();

  // parse storage name
  Storage_initSpecifier(&storageSpecifier);
  error = Storage_parseName(&storageSpecifier,storageName);
  if (error != ERROR_NONE)
  {
    Storage_doneSpecifier(&storageSpecifier);
    return error;
  }

  // get printable storage name
  Storage_getPrintableName(printableStorageName,&storageSpecifier,NULL);

  deltaSourceNode = NULL;

  if (String_isEmpty(storageSpecifier.archivePatternString))
  {
    // add file entry
    deltaSourceNode = LIST_NEW_NODE(DeltaSourceNode);
    if (deltaSourceNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    #ifndef NDEBUG
      deltaSourceNode->id          = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
    #else
      deltaSourceNode->id          = Misc_getId();
    #endif
    deltaSourceNode->storageName = String_duplicate(storageName);
    deltaSourceNode->patternType = patternType;
    deltaSourceNode->locked      = FALSE;

    // add to list
    List_append(deltaSourceList,deltaSourceNode);
  }
  else
  {
    // add matching files
    Job_initOptions(&jobOptions);

    //open directory list
    error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                      &storageSpecifier,
                                      NULL,  // archiveName
                                      &jobOptions,
                                      SERVER_CONNECTION_PRIORITY_LOW
                                     );
    if (error == ERROR_NONE)
    {
      error = Pattern_init(&pattern,
                           storageSpecifier.archivePatternString,
                           patternType,
                           PATTERN_FLAG_NONE
                          );
      if (error == ERROR_NONE)
      {
        fileName = String_new();
        while (!Storage_endOfDirectoryList(&storageDirectoryListHandle) && (error == ERROR_NONE))
        {
          // read next directory entry
          error = Storage_readDirectoryList(&storageDirectoryListHandle,fileName,NULL);
          if (error != ERROR_NONE)
          {
            continue;
          }

         // match pattern
          if (!Pattern_match(&pattern,fileName,STRING_BEGIN,PATTERN_MATCH_MODE_EXACT,NULL,NULL))
          {
            continue;
          }

          // add file entry
          deltaSourceNode = LIST_NEW_NODE(DeltaSourceNode);
          if (deltaSourceNode == NULL)
          {
            HALT_INSUFFICIENT_MEMORY();
          }
          #ifndef NDEBUG
            deltaSourceNode->id          = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
          #else
            deltaSourceNode->id          = Misc_getId();
          #endif
          deltaSourceNode->storageName = String_duplicate(fileName);
          deltaSourceNode->patternType = patternType;
          deltaSourceNode->locked      = FALSE;

          List_append(deltaSourceList,deltaSourceNode);
        }
        String_delete(fileName);
        Pattern_done(&pattern);
      }
      Storage_closeDirectoryList(&storageDirectoryListHandle);
    }
    Job_doneOptions(&jobOptions);
  }

  // add file entry directly if no matching entry found in directory
  if (deltaSourceNode == NULL)
  {
    printWarning("no matching entry for delta source '%s' found",
                 String_cString(printableStorageName)
                );

    deltaSourceNode = LIST_NEW_NODE(DeltaSourceNode);
    if (deltaSourceNode == NULL)
    {
      HALT_INSUFFICIENT_MEMORY();
    }
    #ifndef NDEBUG
      deltaSourceNode->id          = !globalOptions.debug.serverFixedIdsFlag ? Misc_getId() : 1;
    #else
      deltaSourceNode->id          = Misc_getId();
    #endif
    deltaSourceNode->storageName = String_duplicate(storageName);
    deltaSourceNode->patternType = patternType;
    deltaSourceNode->locked      = FALSE;

    List_append(deltaSourceList,deltaSourceNode);
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(printableStorageName);

  return ERROR_NONE;
}

bool DeltaSourceList_remove(DeltaSourceList *deltaSourceList,
                            uint            id
                           )
{
  DeltaSourceNode *deltaSourceNode;

  assert(deltaSourceList != NULL);

  deltaSourceNode = (DeltaSourceNode*)LIST_FIND(deltaSourceList,deltaSourceNode,deltaSourceNode->id == id);
  if (deltaSourceNode != NULL)
  {
    List_removeAndFree(deltaSourceList,deltaSourceNode);
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
