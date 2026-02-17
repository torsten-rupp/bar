/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index functions
* Systems: all
*
\***********************************************************************/

#ifndef __INDEX__
#define __INDEX__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/database.h"
#include "common/files.h"
#include "common/filesystems.h"
#include "errors.h"

#include "index_definition.h"
#include "storage.h"
#include "server_io.h"

/****************** Conditional compilation switches *******************/
// switch on for debugging only!
#define _INDEX_DEBUG_LOCKING
#define _INDEX_DEBUG_IMPORT_OLD_DATABASE

#ifndef NDEBUG
  #define _INDEX_DEBUG_LIST_INFO  // enable to output list info
  #define _INDEX_DEBUG_PURGE      // enable to output purge info
#endif

/***************************** Constants *******************************/
#define INDEX_DEFAULT_DATABASE_NAME "bar"

// index version
#define INDEX_VERSION INDEX_CONST_VERSION

// timeouts
#define INDEX_TIMEOUT       ( 5L*60L*MS_PER_SECOND)   // index timeout [ms]
#define INDEX_PURGE_TIMEOUT (    30L*MS_PER_SECOND)   // index purge timeout [ms]

// max. limit value
#define INDEX_UNLIMITED DATABASE_UNLIMITED

#define INDEX_ID_UUID_NONE      INDEX_ID_UUID     (DATABASE_ID_NONE)
#define INDEX_ID_ENTITY_NONE    INDEX_ID_ENTITY   (DATABASE_ID_NONE)
#define INDEX_ID_STORAGE_NONE   INDEX_ID_STORAGE  (DATABASE_ID_NONE)
#define INDEX_ID_ENTRY_NONE     INDEX_ID_ENTRY    (DATABASE_ID_NONE)
#define INDEX_ID_FILE_NONE      INDEX_ID_FILE     (DATABASE_ID_NONE)
#define INDEX_ID_IMAGE_NONE     INDEX_ID_IMAGE    (DATABASE_ID_NONE)
#define INDEX_ID_DIRECTORY_NONE INDEX_ID_DIRECTORY(DATABASE_ID_NONE)
#define INDEX_ID_LINK_NONE      INDEX_ID_LINK     (DATABASE_ID_NONE)
#define INDEX_ID_HARDLINK_NONE  INDEX_ID_HARDLINK (DATABASE_ID_NONE)
#define INDEX_ID_SPECIAL_NONE   INDEX_ID_SPECIAL  (DATABASE_ID_NONE)
#define INDEX_ID_HISTORY_NONE   INDEX_ID_HISTORY  (DATABASE_ID_NONE)

#define INDEX_DEFAULT_ENTITY_DATABASE_ID INDEX_CONST_DEFAULT_ENTITY_DATABASE_ID

#define PRIindexId PRIi64

/***************************** Datatypes *******************************/

/***********************************************************************\
* Name   : IndexIsMaintenanceTime
* Purpose: check if maintenance time callback
* Input  : userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef bool(*IndexIsMaintenanceTime)(uint64 dateTime, void *userData);

// index states
typedef enum
{
  INDEX_STATE_NONE,

  INDEX_STATE_OK               = INDEX_CONST_STATE_OK,
  INDEX_STATE_CREATE           = INDEX_CONST_STATE_CREATE,
  INDEX_STATE_UPDATE_REQUESTED = INDEX_CONST_STATE_UPDATE_REQUESTED,
  INDEX_STATE_UPDATE           = INDEX_CONST_STATE_UPDATE,
  INDEX_STATE_ERROR            = INDEX_CONST_STATE_ERROR,

  INDEX_STATE_UNKNOWN
} IndexStates;
typedef uint64 IndexStateSet;

#define INDEX_STATE_MIN INDEX_STATE_OK
#define INDEX_STATE_MAX INDEX_STATE_ERROR

#define INDEX_STATE_SET_NONE 0
#define INDEX_STATE_SET_ALL  (  SET_VALUE(INDEX_STATE_NONE) \
                              | SET_VALUE(INDEX_STATE_OK) \
                              | SET_VALUE(INDEX_STATE_CREATE) \
                              | SET_VALUE(INDEX_STATE_UPDATE_REQUESTED) \
                              | SET_VALUE(INDEX_STATE_UPDATE) \
                              | SET_VALUE(INDEX_STATE_ERROR) \
                             )

// index modes
typedef enum
{
  INDEX_MODE_MANUAL = INDEX_CONST_MODE_MANUAL,
  INDEX_MODE_AUTO   = INDEX_CONST_MODE_AUTO,

  INDEX_MODE_UNKNOWN
} IndexModes;
typedef uint64 IndexModeSet;

#define INDEX_MODE_MIN INDEX_MODE_MANUAL
#define INDEX_MODE_MAX INDEX_MODE_AUTO

#define INDEX_MODE_SET_NONE 0
#define INDEX_MODE_SET_ALL  (SET_VALUE(INDEX_MODE_MANUAL) | \
                             SET_VALUE(INDEX_MODE_AUTO) \
                            )

/***********************************************************************\
* Name   : IndexBusyHandlerFunction
* Purpose: index busy handler
* Input  : userData - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*IndexBusyHandlerFunction)(void *userData);

// index handle
typedef struct
{
  ServerIO                 *masterIO;
  const char               *uriString;
  DatabaseHandle           databaseHandle;
  IndexBusyHandlerFunction busyHandlerFunction;
  void                     *busyHandlerUserData;
  Errors                   upgradeError;
  bool                     quitFlag;
  #ifndef NDEBUG
    pthread_t threadId;
  #endif /* not NDEBUG */
} IndexHandle;

// index query handle
typedef struct
{
  IndexHandle             *indexHandle;
  DatabaseStatementHandle databaseStatementHandle;
} IndexQueryHandle;

// index types
typedef enum
{
  INDEX_TYPE_NONE      = 0,

  INDEX_TYPE_UUID      = INDEX_CONST_TYPE_UUID,
  INDEX_TYPE_ENTITY    = INDEX_CONST_TYPE_ENTITY,
  INDEX_TYPE_STORAGE   = INDEX_CONST_TYPE_STORAGE,
  INDEX_TYPE_ENTRY     = INDEX_CONST_TYPE_ENTRY,
  INDEX_TYPE_FILE      = INDEX_CONST_TYPE_FILE,
  INDEX_TYPE_IMAGE     = INDEX_CONST_TYPE_IMAGE,
  INDEX_TYPE_DIRECTORY = INDEX_CONST_TYPE_DIRECTORY,
  INDEX_TYPE_LINK      = INDEX_CONST_TYPE_LINK,
  INDEX_TYPE_HARDLINK  = INDEX_CONST_TYPE_HARDLINK,
  INDEX_TYPE_SPECIAL   = INDEX_CONST_TYPE_SPECIAL,
  INDEX_TYPE_HISTORY   = INDEX_CONST_TYPE_HISTORY,

  INDEX_TYPE_ANY       = 0xF
} IndexTypes;

#define INDEX_TYPEMIN INDEX_TYPE_UUID
#define INDEX_TYPEMAX INDEX_TYPEHISTORY

#define INDEX_TYPESET_NONE 0
#define INDEX_TYPESET_ALL \
  (  SET_VALUE(INDEX_TYPE_UUID) \
   | SET_VALUE(INDEX_TYPE_ENTITY) \
   | SET_VALUE(INDEX_TYPE_STORAGE) \
   | SET_VALUE(INDEX_TYPE_ENTITY) \
   | SET_VALUE(INDEX_TYPE_FILE) \
   | SET_VALUE(INDEX_TYPE_IMAGE) \
   | SET_VALUE(INDEX_TYPE_DIRECTORY) \
   | SET_VALUE(INDEX_TYPE_LINK) \
   | SET_VALUE(INDEX_TYPE_HARDLINK) \
   | SET_VALUE(INDEX_TYPE_SPECIAL) \
   | SET_VALUE(INDEX_TYPE_HISTORY) \
  )
#define INDEX_TYPESET_ALL_ENTRIES \
  (  SET_VALUE(INDEX_TYPE_FILE) \
   | SET_VALUE(INDEX_TYPE_IMAGE) \
   | SET_VALUE(INDEX_TYPE_DIRECTORY) \
   | SET_VALUE(INDEX_TYPE_LINK) \
   | SET_VALUE(INDEX_TYPE_HARDLINK) \
   | SET_VALUE(INDEX_TYPE_SPECIAL) \
  )

typedef uint64 IndexTypeSet;

// index id
typedef struct
{
  union
  {
    struct
    {
      IndexTypes type  :  4;
      DatabaseId value : 60;
    };
    uint64 data;
  };
} IndexId;

extern const IndexId INDEX_ID_NONE;
extern const IndexId INDEX_ID_ANY;

// sort modes
typedef enum
{
  INDEX_ENTITY_SORT_MODE_NONE,

  INDEX_ENTITY_SORT_MODE_JOB_UUID,
  INDEX_ENTITY_SORT_MODE_CREATED
} IndexEntitySortModes;

typedef enum
{
  INDEX_STORAGE_SORT_MODE_NONE,

  INDEX_STORAGE_SORT_MODE_USERNAME,
  INDEX_STORAGE_SORT_MODE_HOSTNAME,
  INDEX_STORAGE_SORT_MODE_NAME,
  INDEX_STORAGE_SORT_MODE_SIZE,
  INDEX_STORAGE_SORT_MODE_CREATED,
  INDEX_STORAGE_SORT_MODE_STATE
} IndexStorageSortModes;

typedef enum
{
  INDEX_ENTRY_SORT_MODE_NONE,

  INDEX_ENTRY_SORT_MODE_ARCHIVE,
  INDEX_ENTRY_SORT_MODE_NAME,
  INDEX_ENTRY_SORT_MODE_TYPE,
  INDEX_ENTRY_SORT_MODE_SIZE,
  INDEX_ENTRY_SORT_MODE_FRAGMENT,
  INDEX_ENTRY_SORT_MODE_LAST_CHANGED
} IndexEntrySortModes;

/***********************************************************************\
* Name   : IndexPauseCallbackFunction
* Purpose: call back to check for pausing
* Input  : userData - user data
* Output : -
* Return : TRUE iff pause
* Notes  : -
\***********************************************************************/

typedef bool(*IndexPauseCallbackFunction)(void *userData);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

//TODO: use type safe type
#define INDEX_ID_EQUALS(id1,id2) ((id1).data == (id2).data)
#define INDEX_ID_IS_DEFAULT_ENTITY(id) (   (INDEX_TYPE(id) == INDEX_TYPE_ENTITY) \
                                        && (INDEX_DATABASE_ID(id) == INDEX_CONST_DEFAULT_ENTITY_DATABASE_ID) \
                                       )
#define INDEX_ID_IS_NONE(id)           ((id).value == DATABASE_ID_NONE)
#define INDEX_ID_IS_ANY(id)            ((id).value == DATABASE_ID_ANY )

// create index state set value
#define INDEX_STATE_SET(indexState) (1U << indexState)

// create index mode set value
#define INDEX_MODE_SET(indexMode) (1U << indexMode)

// get type, database id from index id
#define INDEX_TYPE(indexId)        Index_getType(indexId)
#define INDEX_DATABASE_ID(indexId) Index_getDatabaseId(indexId)

// create index id
#define INDEX_ID_(indexType,databaseId) Index_getId(indexType,databaseId)
#define INDEX_ID_UUID(databaseId)      INDEX_ID_(INDEX_TYPE_UUID     ,databaseId)
#define INDEX_ID_ENTITY(databaseId)    INDEX_ID_(INDEX_TYPE_ENTITY   ,databaseId)
#define INDEX_ID_STORAGE(databaseId)   INDEX_ID_(INDEX_TYPE_STORAGE  ,databaseId)
#define INDEX_ID_ENTRY(databaseId)     INDEX_ID_(INDEX_TYPE_ENTRY    ,databaseId)
#define INDEX_ID_FILE(databaseId)      INDEX_ID_(INDEX_TYPE_FILE     ,databaseId)
#define INDEX_ID_IMAGE(databaseId)     INDEX_ID_(INDEX_TYPE_IMAGE    ,databaseId)
#define INDEX_ID_DIRECTORY(databaseId) INDEX_ID_(INDEX_TYPE_DIRECTORY,databaseId)
#define INDEX_ID_LINK(databaseId)      INDEX_ID_(INDEX_TYPE_LINK     ,databaseId)
#define INDEX_ID_HARDLINK(databaseId)  INDEX_ID_(INDEX_TYPE_HARDLINK ,databaseId)
#define INDEX_ID_SPECIAL(databaseId)   INDEX_ID_(INDEX_TYPE_SPECIAL  ,databaseId)
#define INDEX_ID_HISTORY(databaseId)   INDEX_ID_(INDEX_TYPE_HISTORY  ,databaseId)

#define INDEX_DEFAULT_ENTITY_ID INDEX_ID_ENTITY(INDEX_CONST_DEFAULT_ENTITY_DATABASE_ID)

#if !defined(NDEBUG) || (DATABASE_DEBUG_LOCK == DATABASE_DEBUG_LOCK_FULL)
//  #define Index_lock(...)             __Index_lock            (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Index_open(...)             __Index_open            (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Index_beginTransaction(...) __Index_beginTransaction(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* !defined(NDEBUG) || (DATABASE_DEBUG_LOCK == DATABASE_DEBUG_LOCK_FULL) */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Index_initAll
* Purpose: initialize index functions
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Index_initAll(void);

/***********************************************************************\
* Name   : Index_doneAll
* Purpose: deinitialize index functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Index_doneAll(void);

/***********************************************************************\
* Name   : Index_stateToStringindexStateSet
* Purpose: get name of index sIndex_donetateIndex_init
* Input  : indexState   - index state
*          defaultValue - default value
* Output : -
* Return : name
* Notes  : -
\***********************************************************************/

const char *Index_stateToString(IndexStates indexState, const char *defaultValue);

/***********************************************************************\
* Name   : Index_parseState
* Purpose: parse state string
* Input  : name     - name
*          userData - user data (not used)
* Output : indexState - index state
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Index_parseState(const char *name, IndexStates *indexState, void *userData);

/***********************************************************************\
* Name   : Index_modeToString
* Purpose: get name of index mode
* Input  : indexMode    - index mode
*          defaultValue - default value
* Output : -
* Return : name
* Notes  : -
\***********************************************************************/

const char *Index_modeToString(IndexModes indexMode, const char *defaultValue);

/***********************************************************************\
* Name   : Index_parseMode
* Purpose: parse mode string
* Input  : name     - name
*          userData - user data (not used)
* Output : indexMode - index mode
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Index_parseMode(const char *name, IndexModes *indexMode, void *userData);

/***********************************************************************\
* Name   : Index_typeToString
* Purpose: get name of index type
* Input  : indexType    - index type
*          defaultValue - default value
* Output : -
* Return : name
* Notes  : -
\***********************************************************************/

const char *Index_typeToString(IndexTypes indexType, const char *defaultValue);

/***********************************************************************\
* Name   : Index_parseType
* Purpose: parse index type string
* Input  : name     - name
*          userData - user data (not used)
* Output : indexType - index type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Index_parseType(const char *name, IndexTypes *indexType, void *userData);

/***********************************************************************\
* Name   : Index_parseEntitySortMode
* Purpose: parse index entity sort mode string
* Input  : name     - name
*          userData - user data (not used)
* Output : indexEntitySortMode - index entry sort mode
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Index_parseEntitySortMode(const char *name, IndexEntitySortModes *indexEntitySortMode, void *userData);

/***********************************************************************\
* Name   : Index_parseStorageSortMode
* Purpose: parse index storage sort mode string
* Input  : name     - name
*          userData - user data (not used)
* Output : indexStorageSortMode - index storage sort mode
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Index_parseStorageSortMode(const char *name, IndexStorageSortModes *indexStorageSortMode, void *userData);

/***********************************************************************\
* Name   : Index_parseEntrySortMode
* Purpose: parse index entry sort mode string
* Input  : name     - name
*          userData - user data (not used)
* Output : indexEntrySortMode - index entry sort mode
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Index_parseEntrySortMode(const char *name, IndexEntrySortModes *indexEntrySortMode, void *userData);

/***********************************************************************\
* Name   : Index_parseOrdering
* Purpose: parse index ordering string
* Input  : name     - name
*          userData - user data (not used)
* Output : databaseOrdering - database ordering
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Index_parseOrdering(const char *name, DatabaseOrdering *databaseOrdering, void *userData);

/***********************************************************************\
* Name   : Index_init
* Purpose: initialize index database
* Input  : databaseSpecifier         - database specifier
*          isMaintenanceTimeFunction - check if maintenance time
*                                      function or NULL
*          isMaintenanceTimeUserData - user data for check if
*                                      maintenance time function
* Output : -
* Return : ERROR_NONE or error code
* Notes  : Index_init must be called single-threaded _once_
\***********************************************************************/

Errors Index_init(const DatabaseSpecifier *databaseSpecifier,
                  IndexIsMaintenanceTime  isMaintenanceTimeFunction,
                  void                    *isMaintenanceTimeUserData
                 );

/***********************************************************************\
* Name   : Index_done
* Purpose: deinitialize index database
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

void Index_done(void);

/***********************************************************************\
* Name   : Index_isAvailable
* Purpose: check if index database is available
* Input  : -
* Output : -
* Return : TRUE iff index database is available
* Notes  : -
\***********************************************************************/

bool Index_isAvailable(void);

/***********************************************************************\
* Name   : Index_isInitialized
* Purpose: check if index database is initialized and ready to use
* Input  : -
* Output : -
* Return : TRUE iff index database is initialized and ready to use
* Notes  : -
\***********************************************************************/

bool Index_isInitialized(void);

/***********************************************************************\
* Name   : Index_setPauseCallback
* Purpose: set/reset pause callback
* Input  : pauseCallbackFunction - pause check callback (can be NULL)
*          pauseCallbackUserData - pause user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Index_setPauseCallback(IndexPauseCallbackFunction pauseCallbackFunction,
                            void                       *pauseCallbackUserData
                           );

#if 0
/***********************************************************************\
* Name   : Index_beginInUse, Index_endInUse
* Purpose: mark begin/end in-use
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Index_beginInUse(void);
void Index_endInUse(void);
#endif

/***********************************************************************\
* Name   : Index_isIndexInUse
* Purpose: check if index is in-use by some other thread
* Input  : -
* Output : -
* Return : TRUE iff in use by some other thread
* Notes  : -
\***********************************************************************/

bool Index_isIndexInUse(void);

/***********************************************************************\
* Name   : Index_open
* Purpose: open index database
* Input  : indexHandle      - index handle variable
*          databaseFileName - database file name
*          masterIO         - master I/O or NULL
*          timeout          - timeout [ms]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#if defined(NDEBUG) && (DATABASE_DEBUG_LOCK != DATABASE_DEBUG_LOCK_FULL)
Errors Index_open(IndexHandle *indexHandle,
                  ServerIO    *masterIO,
                  long        timeout
                 );
#else /* not defined(NDEBUG) && (DATABASE_DEBUG_LOCK != DATABASE_DEBUG_LOCK_FULL) */
Errors __Index_open(const char  *__fileName__,
                    ulong       __lineNb__,
                    IndexHandle *indexHandle,
                    ServerIO    *masterIO,
                    long        timeout
                   );
#endif /* defined(NDEBUG) && (DATABASE_DEBUG_LOCK != DATABASE_DEBUG_LOCK_FULL) */

/***********************************************************************\
* Name   : Index_close
* Purpose: close index database
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Index_close(IndexHandle *indexHandle);

/***********************************************************************\
* Name   : Index_setBusyHandler
* Purpose: set index busy handler
* Input  : databaseHandle      - database handle
*          busyHandlerFunction - busy handler function
*          busyHandlerUserData - user data for busy handler functions
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Index_setBusyHandler(IndexHandle              *indexHandle,
                          IndexBusyHandlerFunction busyHandlerFunction,
                          void                     *busyHandlerUserData
                         );

/***********************************************************************\
* Name   : Index_isLockPending
* Purpose: check if another thread is pending for index lock
* Input  : lockType - lock type; see DATABASE_LOCK_TYPE_*
* Output : -
* Return : TRUE iff another thread is pending for index lock, FALSE
*          otherwise
* Notes  : -
\***********************************************************************/

bool Index_isLockPending(IndexHandle *indexHandle, DatabaseLockTypes lockType);

/***********************************************************************\
* Name   : Index_interrupt
* Purpose: interrupt currently running index operation
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Index_interrupt(IndexHandle *indexHandle);

/***********************************************************************\
* Name   : Index_beginTransaction
* Purpose: begin transaction
* Input  : indexHandle - index handle
*          timeout     - timeout [ms]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#if defined(NDEBUG) && (DATABASE_DEBUG_LOCK != DATABASE_DEBUG_LOCK_FULL)
Errors Index_beginTransaction(IndexHandle *indexHandle, ulong timeout);
#else /* not defined(NDEBUG) && (DATABASE_DEBUG_LOCK != DATABASE_DEBUG_LOCK_FULL) */
Errors __Index_beginTransaction(const char  *__fileName__,
                                ulong       __lineNb__,
                                IndexHandle *indexHandle,
                                ulong       timeout
                               );
#endif /* defined(NDEBUG) && (DATABASE_DEBUG_LOCK != DATABASE_DEBUG_LOCK_FULL) */

/***********************************************************************\
* Name   : Index_endTransaction
* Purpose: end transaction (commit)
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_endTransaction(IndexHandle *indexHandle);

/***********************************************************************\
* Name   : Index_rollbackTransaction
* Purpose: rollback transcation (discard)
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_rollbackTransaction(IndexHandle *indexHandle);

/***********************************************************************\
* Name   : Index_flush
* Purpose: flush index data
* Input  : indexHandle - index handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_flush(IndexHandle *indexHandle);

/***********************************************************************\
* Name   : Index_getId
* Purpose: get index id
* Input  : indexType  - index id type
*          databaseId - database id
* Output : -
* Return : index id
* Notes  : -
\***********************************************************************/

static inline IndexId Index_getId(IndexTypes indexType, DatabaseId databaseId)
{
  IndexId indexId;

  indexId.type  = indexType;
  indexId.value = databaseId;

  return indexId;
}

/***********************************************************************\
* Name   : Index_getType
* Purpose: get index type
* Input  : indexId - index id
* Output : -
* Return : index type
* Notes  : -
\***********************************************************************/

INLINE IndexTypes Index_getType(const IndexId indexId);
#if defined(NDEBUG) || defined(__INDEX_IMPLEMENTATION__)
INLINE IndexTypes Index_getType(const IndexId indexId)
{
  return indexId.type;
}
#endif /* NDEBUG || __INDEX_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : xxxINDEX_DATABASE_ID
* Purpose: get database id
* Input  : indexId - index id
* Output : -
* Return : database id
* Notes  : -
\***********************************************************************/

static inline DatabaseId Index_getDatabaseId(const IndexId indexId)
{
  return indexId.value;
}

/***********************************************************************\
* Name   : Index_containsType
* Purpose: check if id set contains index type
* Input  : indexIds     - index ids
*          indexIdCount - index id count
*          indexType    - index type
* Output : -
* Return : true iff index ids contains type
* Notes  : -
\***********************************************************************/

bool Index_containsType(const IndexId indexIds[],
                        uint          indexIdCount,
                        IndexTypes    indexType
                       );

// ---------------------------------------------------------------------

/***********************************************************************\
* Name   : Index_getInfos
* Purpose: get infos
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
* Output : totalNormalEntityCount           - total number of normal entities (can be NULL)
*          totalFullEntityCount             - total number of full entities (can be NULL)
*          totalIncrementalEntityCount      - total number of incremental entities (can be NULL)
*          totalDifferentialEntityCount     - total number of differential entities (can be NULL)
*          totalContinuousEntityCount       - total number of continuous entities (can be NULL)
*          totalLockedEntityCount           - total number of locked entities (can be NULL)
*          totalDeletedEntityCount          - total number of deleted entities (can be NULL)
*
*          totalEntryCount                  - total entry count (can be NULL)
*          totalEntrySize                   - total entry size [bytes] (can be NULL)
*          totalFileCount                   - total file entry count (can be NULL)
*          totalFileSize                    - total file entry size [bytes] (can be NULL)
*          totalImageCount                  - total image entry count (can be NULL)
*          totalImageSize                   - total image entry size [bytes] (can be NULL)
*          totalDirectoryCount              - total directory entry count (can be NULL)
*          totalLinkCount                   - total link entry count (can be NULL)
*          totalHardlinkCount               - total hardlink entry count (can be NULL)
*          totalHardlinkSize                - total hardlink entry size [bytes] (can be NULL)
*          totalSpecialCount                - total special entry count (can be NULL)
*
*          totalEntryCountNewest            - total newest entry count (can be NULL)
*          totalEntrySizeNewest             - total newest entry size [bytes] (can be NULL)
*          totalFileCountNewest             - total newest file entry size [bytes] (can be NULL)
*          totalFileSizeNewest              - total newest file entry size [bytes] (can be NULL)
*          totalImageCountNewest            - total newest image entry count (can be NULL)
*          totalImageSizeNewest             - total newest image entry size [bytes] (can be NULL)
*          totalDirectoryCountNewest        - total newest directory entry count (can be NULL)
*          totalLinkCountNewest             - total newest link entry count (can be NULL)
*          totalHardlinkCountNewest         - total newest hardlink entry count (can be NULL)
*          totalHardlinkSizeNewest          - total newest hardlink entry content size [bytes] (can be NULL)
*          totalSpecialCountNewest          - total newest specialentry count (can be NULL)
*
*          totalSkippedEntryCount           - total skipped number of entries (can be NULL)
*          totalStorageCount                - total number of storages (can be NULL)
*          totalOKStorageCount              - total number of storages OK (can be NULL)
*          totalUpdateRequestedStorageCount - total number of storages update requested (can be NULL)
*          totalErrorStorageCount           - total number of storages with error (can be NULL)
*          totalStorageSize                 - total storages size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_getInfos(IndexHandle   *indexHandle,
                      uint          *totalNormalEntityCount,
                      uint          *totalFullEntityCount,
                      uint          *totalIncrementalEntityCount,
                      uint          *totalDifferentialEntityCount,
                      uint          *totalContinuousEntityCount,
                      uint          *totalLockedEntityCount,
                      uint          *totalDeletedEntityCount,

                      uint          *totalEntryCount,
                      uint64        *totalEntrySize,
                      uint          *totalFileCount,
                      uint64        *totalFileSize,
                      uint          *totalImageCount,
                      uint64        *totalImageSize,
                      uint          *totalDirectoryCount,
                      uint          *totalLinkCount,
                      uint          *totalHardlinkCount,
                      uint64        *totalHardlinkSize,
                      uint          *totalSpecialCount,

                      uint          *totalEntryCountNewest,
                      uint64        *totalEntrySizeNewest,
                      uint          *totalFileCountNewest,
                      uint64        *totalFileSizeNewest,
                      uint          *totalImageCountNewest,
                      uint64        *totalImageSizeNewest,
                      uint          *totalDirectoryCountNewest,
                      uint          *totalLinkCountNewest,
                      uint          *totalHardlinkCountNewest,
                      uint64        *totalHardlinkSizeNewest,
                      uint          *totalSpecialCountNewest,

                      uint          *totalSkippedEntryCount,

                      uint          *totalStorageCount,
                      uint64        *totalStorageSize,
                      uint          *totalOKStorageCount,
                      uint          *totalUpdateRequestedStorageCount,
                      uint          *totalErrorStorageCount,
                      uint          *totalDeletedStorageCount
                     );

// ---------------------------------------------------------------------

/***********************************************************************\
* Name   : Index_doneList
* Purpose: done index list
* Input  : indexQueryHandle - index query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Index_doneList(IndexQueryHandle *indexQueryHandle);

// ---------------------------------------------------------------------

// TODO: how to use?
/***********************************************************************\
* Name   : Index_getIndexId
* Purpose:
* Input  : stringMap    - string map
*          name         - value name
*          type         - index id type
*          defaultValue - default value
* Output : indexId - index id
* Return : TRUE if read, FALSE otherwise
* Notes  : -
\***********************************************************************/

INLINE bool Index_getIndexId(StringMap stringMap, const char *name, IndexId *indexId, IndexTypes type, IndexId defaultIndexId);
#if defined(NDEBUG) || defined(__INDEX_IMPLEMENTATION__)
INLINE bool Index_getIndexId(StringMap stringMap, const char *name, IndexId *indexId, IndexTypes type, IndexId defaultIndexId)
{
  uint64 n;

  assert(indexId != NULL);

  if (StringMap_getUInt64(stringMap,name,&n,INDEX_DATABASE_ID(defaultIndexId)))
  {
    (*indexId) = INDEX_ID_(type,n);
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}
#endif /* NDEBUG || __INDEX_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : StringMap_getIndexId
* Purpose: additional string map functions: get index id
* Input  : stringMap    - stringMap
*          name         - value name
*          indexType    - expected index id type; see INDEX_TYPE_...
*          defaultValue - value/default value
* Output : data - value or default value
* Return : TRUE if read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool StringMap_getIndexId(const StringMap stringMap, const char *name, IndexId *data, IndexTypes indexType, IndexId defaultValue);

#ifdef INDEX_DEBUG_LOCKING
/***********************************************************************\
* Name   : Index_debugPrintInUseInfo
* Purpose: print debug in-use info
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Index_debugPrintInUseInfo(void);
#endif /* INDEX_DEBUG_LOCKING */

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX__ */

/* end of file */
