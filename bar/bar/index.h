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

#include "global.h"
#include "strings.h"
#include "database.h"
#include "files.h"
#include "filesystems.h"
#include "errors.h"
#include "index_definition.h"

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
// index version
#define INDEX_VERSION INDEX_CONST_VERSION

// index priorities
#define INDEX_PRIORITY_HIGH   DATABASE_PRIORITY_HIGH
#define INDEX_PRIORITY_MEDIUM DATABASE_PRIORITY_MEDIUM
#define INDEX_PRIORITY_LOW    DATABASE_PRIORITY_LOW

// max. limit value
#define INDEX_UNLIMITED 9223372036854775807LL

// special index ids
#define INDEX_ID_MASK_TYPE         0x0000000F
#define INDEX_ID_SHIFT_TYPE        0
#define INDEX_ID_MASK_DATABASE_ID  0xFFFFFFF0
#define INDEX_ID_SHIFT_DATABASE_ID 4

#define INDEX_ID_NONE  0LL
#define INDEX_ID_ANY  -1LL

#define INDEX_ID_UUID_NONE     INDEX_ID_UUID     (DATABASE_ID_NONE)
#define INDEX_ID_ENTITY_NONE   INDEX_ID_ENTITY   (DATABASE_ID_NONE)
#define INDEX_ID_STORAGE_NONE  INDEX_ID_STORAGE  (DATABASE_ID_NONE)
#define INDEX_ID_ENTRY_NONE    INDEX_ID_ENTRY    (DATABASE_ID_NONE)
#define INDEX_ID_FILE_NONE     INDEX_ID_FILE     (DATABASE_ID_NONE)
#define INDEX_ID_IMAGE_NONE    INDEX_ID_IMAGE    (DATABASE_ID_NONE)
#define INDEX_ID_DIRECTORY_NONEINDEX_ID_DIRECTORY(DATABASE_ID_NONE)
#define INDEX_ID_LINK_NONE     INDEX_ID_LINK     (DATABASE_ID_NONE)
#define INDEX_ID_HARDLINK_NONE INDEX_ID_HARDLINK (DATABASE_ID_NONE)
#define INDEX_ID_SPECIAL_NONE  INDEX_ID_SPECIAL  (DATABASE_ID_NONE)
#define INDEX_ID_HISTORY_NONE  INDEX_ID_HISTORY  (DATABASE_ID_NONE)

/***************************** Datatypes *******************************/

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
#define INDEX_STATE_SET_ALL  (SET_VALUE(INDEX_STATE_NONE) |\
                              SET_VALUE(INDEX_STATE_OK) |\
                              SET_VALUE(INDEX_STATE_CREATE) |\
                              SET_VALUE(INDEX_STATE_UPDATE_REQUESTED) |\
                              SET_VALUE(INDEX_STATE_UPDATE) |\
                              SET_VALUE(INDEX_STATE_ERROR) \
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

// index handle
typedef struct
{
  const char     *databaseFileName;
  DatabaseHandle databaseHandle;
  Errors         upgradeError;
  bool           quitFlag;
  #ifndef NDEBUG
    pthread_t threadId;
  #endif /* not NDEBUG */
} IndexHandle;

// index query handle
typedef struct
{
  IndexHandle         *indexHandle;
  DatabaseQueryHandle databaseQueryHandle;
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
} IndexTypes;

#define INDEX_TYPE_MIN INDEX_TYPE_UUID
#define INDEX_TYPE_MAX INDEX_TYPE_HISTORY

#define INDEX_TYPE_SET_NONE 0
#define INDEX_TYPE_SET_ANY \
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
#define INDEX_TYPE_SET_ANY_ENTRY \
  (  SET_VALUE(INDEX_TYPE_FILE) \
   | SET_VALUE(INDEX_TYPE_IMAGE) \
   | SET_VALUE(INDEX_TYPE_DIRECTORY) \
   | SET_VALUE(INDEX_TYPE_LINK) \
   | SET_VALUE(INDEX_TYPE_HARDLINK) \
   | SET_VALUE(INDEX_TYPE_SPECIAL) \
  )

typedef ulong IndexTypeSet;

// index id
typedef int64 IndexId;

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

// create index state set value
#define INDEX_STATE_SET(indexState) (1U << indexState)

// create index mode set value
#define INDEX_MODE_SET(indexMode) (1U << indexMode)

// get type, database id from index id
#define INDEX_TYPE_(indexId)        ((IndexTypes)(((indexId) & INDEX_ID_MASK_TYPE       ) >> INDEX_ID_SHIFT_TYPE       ))
#define INDEX_DATABASE_ID_(indexId) ((DatabaseId)(((indexId) & INDEX_ID_MASK_DATABASE_ID) >> INDEX_ID_SHIFT_DATABASE_ID))

// create index id
#define INDEX_ID_(indexType,databaseId) (IndexId)(  ((uint64)(indexType ) << INDEX_ID_SHIFT_TYPE       ) \
                                                  | ((uint64)(databaseId) << INDEX_ID_SHIFT_DATABASE_ID) \
                                                 )
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

#ifndef NDEBUG
  #define Index_open(...) __Index_open(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

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
* Input  : name - name
* Output : indexState - index state
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Index_parseState(const char *name, IndexStates *indexState);

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
* Input  : name - name
* Output : indexMode - index mode
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Index_parseMode(const char *name, IndexModes *indexMode);

/***********************************************************************\
* Name   : Index_parseType
* Purpose: parse index type string
* Input  : name - name
* Output : indexType - index type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Index_parseType(const char *name, IndexTypes *indexType);

/***********************************************************************\
* Name   : Index_init
* Purpose: initialize index database
* Input  : fileName - database file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_init(const char *fileName);

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

/***********************************************************************\
* Name   : Index_open
* Purpose: open index database
* Input  : indexHandle      - index handle variable
*          databaseFileName - database file name
*          priority         - priority (0=highest)
*          timeout          - timeout [ms]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
IndexHandle *Index_open(uint priority,
                        long timeout
                       );
#else /* not NDEBUG */
IndexHandle *__Index_open(const char *__fileName__,
                          uint       __lineNb__,
                          uint        priority,
                          long       timeout
                         );
#endif /* NDEBUG */

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
* Name   : Index_beginTransaction
* Purpose: begin transaction
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_beginTransaction(IndexHandle *indexHandle);

/***********************************************************************\
* Name   : Index_endTransaction
* Purpose: end transaction (commit)
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_endTransaction(IndexHandle *indexHandle);

/***********************************************************************\
* Name   : Index_rollbackTransaction
* Purpose: rollback transcation (discard)
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_rollbackTransaction(IndexHandle *indexHandle);

/***********************************************************************\
* Name   : Index_getType
* Purpose: get index type
* Input  : indexId - index id
* Output : -
* Return : index type
* Notes  : -
\***********************************************************************/

INLINE IndexTypes Index_getType(IndexId indexId);
#if defined(NDEBUG) || defined(__INDEX_IMPLEMENATION__)
INLINE IndexTypes Index_getType(IndexId indexId)
{
  return INDEX_TYPE_(indexId);
}
#endif /* NDEBUG || __INDEX_IMPLEMENATION__ */

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

/***********************************************************************\
* Name   : Index_getDatabaseId
* Purpose: get database id
* Input  : indexId - index id
* Output : -
* Return : database id
* Notes  : -
\***********************************************************************/

INLINE DatabaseId Index_getDatabaseId(IndexId indexId);
#if defined(NDEBUG) || defined(__INDEX_IMPLEMENATION__)
INLINE DatabaseId Index_getDatabaseId(IndexId indexId)
{
  return INDEX_DATABASE_ID_(indexId);
}
#endif /* NDEBUG || __INDEX_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Index_findUUIDByJobUUID
* Purpose: find uuid info by job UUID
* Input  : indexHandle - index handle
*          jobUUID     - unique job id
* Output : uuidId              - index id of UUID entry (can be NULL)
*          lastCreatedDateTime - created date/time stamp [s] (can be
*                                NULL)
*          lastErrorMessage    - last error message (can be NULL)
*          executionCount      - number job execution (can be NULL)
*          averageDuration     - average execution time [s] (entries
*                                without error, can be NULL)
*          totalEntityCount    - total number of entities (can be NULL)
*          totalStorageCount   - total number of storage archives (can be
*                                NULL)
*          totalStorageSize    - total size of storage archives [bytes]
*                                (can be NULL)
*          totalEntryCount     - total number of entries (can be NULL)
*          totalEntrySize      - total size [bytes] (can be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findUUIDByJobUUID(IndexHandle  *indexHandle,
                             ConstString  jobUUID,
                             IndexId      *uuidId,
                             uint64       *lastCreatedDateTime,
                             String       lastErrorMessage,
                             ulong        *executionCount,
                             uint64       *averageDuration,
                             ulong        *totalEntityCount,
                             ulong        *totalStorageCount,
                             uint64       *totalStorageSize,
                             ulong        *totalEntryCount,
                             uint64       *totalEntrySize
                            );

/***********************************************************************\
* Name   : Index_findEntityByJobUUID
* Purpose: find entity info by job UUID
* Input  : indexHandle      - index handle
*          jobUUID          - unique job id
*          scheduleUUID     - unique schedule id (can be NULL)
* Output : uuidId           - index id of UUID entry (can be NULL)
*          entityId         - index id of entity entry (can be NULL)
*          archiveType      - archive type (can be NULL)
*          createdDateTime  - created date/time stamp [s] (can be NULL)
*          lastErrorMessage - last error message (can be NULL)
*          totalEntryCount  - total number of entries (can be NULL)
*          totalEntrySize   - total size [bytes] (can be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findEntityByJobUUID(IndexHandle  *indexHandle,
                               ConstString  jobUUID,
                               IndexId      *uuidId,
                               IndexId      *entityId,
                               ConstString  scheduleUUID,
                               ArchiveTypes *archiveType,
                               uint64       *createdDateTime,
                               String       lastErrorMessage,
                               ulong        *totalEntryCount,
                               uint64       *totalEntrySize
                              );

/***********************************************************************\
* Name   : Index_findByStorageId
* Purpose: find info by storage id
* Input  : indexHandle - index handle
*          storageId   - index id of storage
* Output : jobUUID             - unique job id (can be NULL)
*          scheduleUUID        - unique schedule id (can be NULL)
*          uuidId              - index id of UUID entry (can be NULL)
*          entityId            - index id of entity entry (can be NULL)
*          storageName         - storage name (can be NULL)
*          indexState          - index state (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can
*                                be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findStorageById(IndexHandle *indexHandle,
                           IndexId     storageId,
                           String      jobUUID,
                           String      scheduleUUID,
                           IndexId     *uuidId,
                           IndexId     *entityId,
                           String      storageName,
                           IndexStates *indexState,
                           uint64      *lastCheckedDateTime
                          );

/***********************************************************************\
* Name   : Index_findByStorageName
* Purpose: find info by storage name
* Input  : indexHandle      - index handle
*          storageSpecifier - storage specifier
*          archiveName      - archive name or NULL
* Output : entityId            - index id of entity (can be NULL)
*          jobUUID             - unique job id (can be NULL)
*          scheduleUUID        - unique schedule id (can be NULL)
*          uuidId              - index id of UUID entry (can be NULL)
*          entityId            - index id of entity entry (can be NULL)
*          storageId           - index id of storage entry (can be NULL)
*          indexState          - index state (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can
*                                be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findStorageByName(IndexHandle            *indexHandle,
                             const StorageSpecifier *storageSpecifier,
                             ConstString            archiveName,
                             IndexId                *uuidId,
                             IndexId                *entityId,
                             String                 jobUUID,
                             String                 scheduleUUID,
                             IndexId                *storageId,
                             IndexStates            *indexState,
                             uint64                 *lastCheckedDateTime
                            );

/***********************************************************************\
* Name   : Index_findStorageByState
* Purpose: find storage info by state
* Input  : indexHandle   - index handle
*          indexStateSet - index state set
* Output : jobUUID             - unique job id (can be NULL)
*          scheduleUUID        - unique schedule id (can be NULL)
*          entityId            - index id of entity (can be NULL)
*          storageId           - index id of storage (can be NULL)
*          storageName         - storage name (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can
*                                be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findStorageByState(IndexHandle   *indexHandle,
                              IndexStateSet indexStateSet,
                              IndexId       *uuidId,
                              String        jobUUID,
                              IndexId       *entityId,
                              String        scheduleUUID,
                              IndexId       *storageId,
                              String        storageName,
                              uint64        *lastCheckedDateTime
                             );

/***********************************************************************\
* Name   : Index_getState
* Purpose: get index state
* Input  : indexHandle - index handle
*          storageId   - index id of storage
* Output : indexState          - index state; see IndexStates
*          lastCheckedDateTime - last checked date/time stamp [s] (can
*                                be NULL)
*          errorMessage        - error message (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_getState(IndexHandle  *indexHandle,
                      IndexId      storageId,
                      IndexStates  *indexState,
                      uint64       *lastCheckedDateTime,
                      String       errorMessage
                     );

/***********************************************************************\
* Name   : Index_setState
* Purpose: set storage index state
* Input  : indexHandle         - index handle
*          storageId           - index id of storage
*          indexState          - index state; see IndexStates
*          lastCheckedDateTime - last checked date/time stamp [s] (can
*                                be 0LL)
*          errorMessage        - error message (can be NULL)
*          ...                 - optional arguments for error message
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_setState(IndexHandle *indexHandle,
                      IndexId     storageId,
                      IndexStates indexState,
                      uint64      lastCheckedDateTime,
                      const char  *errorMessage,
                      ...
                     );

/***********************************************************************\
* Name   : Index_countState
* Purpose: get number of storage entries with specific state
* Input  : indexHandle - index handle
*          indexState  - index state; see IndexStates
* Output : -
* Return : number of entries or -1
* Notes  : -
\***********************************************************************/

long Index_countState(IndexHandle *indexHandle,
                      IndexStates indexState
                     );

/***********************************************************************\
* Name   : Index_initListHistory
* Purpose: list history
* Input  : IndexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          jobUUID          - unique job id (can be NULL)
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListHistory(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             IndexId          uuidId,
                             ConstString      jobUUID,
                             uint64           offset,
                             uint64           limit
                            );

/***********************************************************************\
* Name   : Index_getNextHistory
* Purpose: get next history entry
* Input  : IndexQueryHandle - index query handle
* Output : historyId         - index history id
*          uuidId            - index UUID id
*          jobUUID           - unique job id (can be NULL)
*          scheduleUUID      - unique schedule id (can be NULL)
*          hostName          - host name (can be NULL)
*          createdDateTime   - create date/time stamp [s] (can be NULL)
*          errorMessage      - last storage error message (can be NULL)
*          duration          - duration [s]
*          totalEntryCount   - total number of entries (can be NULL)
*          totalEntrySize    - total storage size [bytes] (can be NULL)
*          skippedEntryCount - number of skipped entries (can be NULL)
*          skippedEntrySize  - size of skipped entries [bytes] (can be
*                              NULL)
*          errorEntryCount   - number of error entries (can be NULL)
*          errorEntrySize    - size of error entries [bytes] (can be
*                              NULL)
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextHistory(IndexQueryHandle *indexQueryHandle,
                          IndexId          *historyId,
                          IndexId          *uuidId,
                          String           jobUUID,
                          String           scheduleUUID,
//TODO: entityId?
                          String           hostName,
                          uint64           *createdDateTime,
                          String           errorMessage,
                          uint64           *duration,
                          ulong            *totalEntryCount,
                          uint64           *totalEntrySize,
                          ulong            *skippedEntryCount,
                          uint64           *skippedEntrySize,
                          ulong            *errorEntryCount,
                          uint64           *errorEntrySize
                         );

/***********************************************************************\
* Name   : Index_newHistory
* Purpose: create new history entry
* Input  : indexHandle  - index handle
*          jobUUID           - unique job id
*          scheduleUUID      - unique schedule id (can be NULL)
*          hostName          - hostname (can be NULL)
*          archiveType       - archive type
*          createdDateTime   - create date/time stamp [s]
*          errorMessage      - last storage error message (can be NULL)
*          duration          - duration [s]
*          totalEntryCount   - total number of entries
*          totalEntrySize    - total storage size [bytes]
*          skippedEntryCount - number of skipped entries
*          skippedEntrySize  - size of skipped entries [bytes]
*          errorEntryCount   - number of error entries
*          errorEntrySize    - size of error entries [bytes]
* Output : uuidId - index id of new UUID entry (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_newHistory(IndexHandle  *indexHandle,
                        ConstString  jobUUID,
                        ConstString  scheduleUUID,
                        ConstString  hostName,
//TODO: entityId?
                        ArchiveTypes archiveType,
                        uint64       createdDateTime,
                        const char   *errorMessage,
                        uint64       duration,
                        ulong        totalEntryCount,
                        uint64       totalEntrySize,
                        ulong        skippedEntryCount,
                        uint64       skippedEntrySize,
                        ulong        errorEntryCount,
                        uint64       errorEntrySize,
                        IndexId      *historyId
                       );

/***********************************************************************\
* Name   : Index_deleteHistory
* Purpose: delete history entry
* Input  : indexQueryHandle - index query handle
*          jobUUID          - unique job id
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteHistory(IndexHandle *indexHandle,
                           IndexId     historyId
                          );

/***********************************************************************\
* Name   : Index_initListUUIDs
* Purpose: list uuid entries and aggregated data of entities
* Input  : IndexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          name             - storage name pattern (glob, can be NULL)
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListUUIDs(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           ConstString      name,
                           uint64           offset,
                           uint64           limit
                          );

/***********************************************************************\
* Name   : Index_getNextUUID
* Purpose: get next index uuid entry
* Input  : IndexQueryHandle - index query handle
* Output : uuidId              - index id of UUID entry
*          jobUUID             - unique job id (can be NULL)
*          lastCreatedDateTime - last storage date/time stamp [s] (can be NULL)
*          lastErrorMessage    - last storage error message (can be NULL)
*          totalEntryCount     - total number of entries (can be NULL)
*          totalEntrySize      - total storage size [bytes] (can be NULL)
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextUUID(IndexQueryHandle *indexQueryHandle,
                       IndexId          *uuidId,
                       String           jobUUID,
                       uint64           *lastCreatedDateTime,
                       String           lastErrorMessage,
                       ulong            *totalEntryCount,
                       uint64           *totalEntrySize
                      );

/***********************************************************************\
* Name   : Index_newUUID
* Purpose: create new UUID index
* Input  : indexHandle  - index handle
*          jobUUID      - unique job id (can be NULL)
* Output : uuidId - index id of new UUID entry (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_newUUID(IndexHandle *indexHandle,
                     ConstString jobUUID,
                     IndexId     *uuidId
                    );

/***********************************************************************\
* Name   : Index_deleteUUID
* Purpose: delete job UUID index including all attached index entries
*          (entities, entries)
* Input  : indexQueryHandle - index query handle
*          uuidId           - uuid id
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteUUID(IndexHandle *indexHandle,
                        IndexId     uuidId
                       );

/***********************************************************************\
* Name   : Index_isEmptyUUID
* Purpose: check if UUID entry is empty (no entities)
* Input  : indexQueryHandle - index query handle
*          uuidId           - uuid id
* Return : TRUE iff empty
* Notes  : -
\***********************************************************************/

Errors Index_isEmptyUUID(IndexHandle *indexHandle,
                         IndexId     uuidId
                        );

/***********************************************************************\
* Name   : Index_initListEntities
* Purpose: list entity entries and aggregated data of storage
* Input  : IndexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          uuidId           - index uuid id
*          jobUUID          - unique job id (can be NULL)
*          scheduleUUID     - unique schedule id (can be NULL)
*          name             - storage name pattern (glob, can be NULL)
*          ordering         - ordering mode
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListEntities(IndexQueryHandle *indexQueryHandle,
                              IndexHandle      *indexHandle,
                              IndexId          uuidId,
//TODO: remove?
                              ConstString      jobUUID,
                              ConstString      scheduleUUID,
                              ConstString      name,
                              DatabaseOrdering ordering,
                              ulong            offset,
                              uint64           limit
                             );

/***********************************************************************\
* Name   : Index_getNextEntity
* Purpose: get next index entity entry
* Input  : IndexQueryHandle - index query handle
* Output : uuidId           - index id of UUID entry (can be NULL)
*          jobUUID          - unique job id (can be NULL)
*          scheduleUUID     - unique schedule id (can be NULL)
*          entityId         - index id of entity entry
*          archiveType      - archive type (can be NULL)
*          createdDateTime  - created date/time stamp [s] (can be NULL)
*          lastErrorMessage - last storage error message (can be NULL)
*          totalEntryCount  - total number of entries (can be NULL)
*          totalEntrySize   - total storage size [bytes] (can be NULL)
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextEntity(IndexQueryHandle *indexQueryHandle,
                         IndexId          *uuidId,
                         String           jobUUID,
                         String           scheduleUUID,
                         IndexId          *entityId,
                         ArchiveTypes     *archiveType,
                         uint64           *createdDateTime,
                         String           lastErrorMessage,
                         ulong            *totalEntryCount,
                         uint64           *totalEntrySize
                        );

/***********************************************************************\
* Name   : Index_newEntity
* Purpose: create new uuid index (if need) and new entity index
* Input  : indexHandle     - index handle
*          jobUUID         - unique job id (can be NULL)
*          scheduleUUID    - unique schedule id (can be NULL)
*          archiveType     - archive type
*          createdDateTime - created date/time stamp or 0
* Output : entityId - index id of new entity index
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_newEntity(IndexHandle  *indexHandle,
//TODO: uuidId?
                       ConstString  jobUUID,
                       ConstString  scheduleUUID,
                       ArchiveTypes archiveType,
                       uint64       createdDateTime,
                       IndexId      *entityId
                      );

/***********************************************************************\
* Name   : Index_deleteEntity
* Purpose: delete entity index including all attached entries
* Input  : indexQueryHandle - index query handle
*          entityId         - index id of entity index
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         );

/***********************************************************************\
* Name   : Index_isEmptyEntity
* Purpose: check if entity entry is empty (no storages)
* Input  : indexQueryHandle - index query handle
*          entityId         - entity id
* Return : TRUE iff empty
* Notes  : -
\***********************************************************************/

Errors Index_isEmptyEntity(IndexHandle *indexHandle,
                           IndexId     entityId
                          );

/***********************************************************************\
* Name   : Index_getStoragesInfo
* Purpose: get storages info
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          uuidId           - index id of UUID entry or INDEX_ID_ANY
*          entityId         - index id of entity entry id or INDEX_ID_ANY
*          jobUUID          - unique job id or NULL
*          indexIds         - index ids or NULL
*          indexIdCount     - index id count or 0
*          indexStateSet    - index state set or INDEX_STATE_SET_ANY
*          IndexModeSet     - index mode set
*          name             - name pattern (glob, can be NULL)
* Output : storageCount          - number of storages (can be NULL)
*          totalEntryCount       - total entry count (can be NULL)
*          totalEntrySize        - total size [bytes] (can be NULL)
*          totalEntryContentSize - total size including directory
*                                  content [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_getStoragesInfo(IndexHandle   *indexHandle,
                             IndexId       uuidId,
                             IndexId       entityId,
//TODO: remove?
                             ConstString   jobUUID,
                             const IndexId indexIds[],
                             uint          indexIdCount,
                             IndexStateSet indexStateSet,
                             IndexModeSet  indexModeSet,
                             ConstString   name,
                             ulong         *storageCount,
                             ulong         *totalEntryCount,
                             uint64        *totalEntrySize,
                             uint64        *totalEntryContentSize
                            );

/***********************************************************************\
* Name   : Index_initListStorages
* Purpose: list storage entries
* Input  : IndexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          uuidId           - index id of UUID entry or INDEX_ID_ANY
*          entityId         - index id of entity entry id or INDEX_ID_ANY
*          jobUUID          - unique job id or NULL
*          indexIds         - index ids or NULL
*          indexIdCount     - index id count or 0
*          indexStateSet    - index state set
*          IndexModeSet     - index mode set
*          name             - name pattern (glob, can be NULL)
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListStorages(IndexQueryHandle *indexQueryHandle,
                              IndexHandle      *indexHandle,
                              IndexId          uuidId,
                              IndexId          entityId,
//TODO: remove?
                              ConstString      jobUUID,
                              const IndexId    indexIds[],
                              uint             indexIdCount,
                              IndexStateSet    indexStateSet,
                              IndexModeSet     indexModeSet,
                              ConstString      name,
                              DatabaseOrdering ordering,
                              uint64           offset,
                              uint64           limit
                             );

/***********************************************************************\
* Name   : Index_getNextStorage
* Purpose: get next index storage entry
* Input  : IndexQueryHandle    - index query handle
* Output : uuidId
*          jobUUID             - job UUID (can be NULL)
*          entityId            - index id of entity (can be NULL)
*          scheduleUUID        - schedule UUID (can be NULL)
*          archiveType         - archive type (can be NULL)
*          storageId           - index id of storage (can be NULL)
*          storageName         - storage name (can be NULL)
*          createdDateTime     - date/time stamp [s] (can be NULL)
*          totalEntryCount     - total number of entries (can be NULL)
*          totalEntrySize      - total size [bytes] (can be NULL)
*          indexState          - index state (can be NULL)
*          indexMode           - index mode (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can be
*                                NULL)
*          errorMessage        - last error message
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextStorage(IndexQueryHandle *indexQueryHandle,
                          IndexId          *uuidId,
                          String           jobUUID,
                          IndexId          *entityId,
                          String           scheduleUUID,
                          ArchiveTypes     *archiveType,
                          IndexId          *storageId,
                          String           storageName,
                          uint64           *createdDateTime,
                          ulong            *totalEntryCount,
                          uint64           *totalEntrySize,
                          IndexStates      *indexState,
                          IndexModes       *indexMode,
                          uint64           *lastCheckedDateTime,
                          String           errorMessage
                         );

/***********************************************************************\
* Name   : Index_newStorage
* Purpose: create new storage index
* Input  : indexHandle - index handle
*          entityId    - index id of entity
*          storageName - storage name
*          indexState  - index state
*          indexMode   - index mode
* Output : indexId - storageId id of new storage index
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_newStorage(IndexHandle *indexHandle,
                        IndexId     entityId,
                        ConstString storageName,
                        IndexStates indexState,
                        IndexModes  indexMode,
                        IndexId     *storageId
                       );

/***********************************************************************\
* Name   : Index_deleteStorage
* Purpose: delete storage index including all entries for attached files,
*          image, directories, link, hard link, special entries
* Input  : indexQueryHandle - index query handle
*          IndexId          - index id of storage index
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteStorage(IndexHandle *indexHandle,
                           IndexId     storageId
                          );

/***********************************************************************\
* Name   : Index_isEmptyStorage
* Purpose: check if storage entry is empty (no entries)
* Input  : indexQueryHandle - index query handle
*          storageId        - storage id
* Return : TRUE iff empty
* Notes  : -
\***********************************************************************/

Errors Index_isEmptyStorage(IndexHandle *indexHandle,
                            IndexId     storageId
                           );

/***********************************************************************\
* Name   : Index_clearStorage
* Purpose: clear index storage content
* Input  : indexHandle - index handle
*          storageId   - index id of storage index
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_clearStorage(IndexHandle *indexHandle,
                          IndexId    storageId
                         );

/***********************************************************************\
* Name   : Index_getStorage
* Purpose: get storage index name/entries/size
* Input  : indexHandle - index handle
*          storageId   - index id of storage index
* Output:  uuidId              - index if of uuid
*          jobUUID             - job UUID (can be NULL)
*          entityId            - index id of entity (can be NULL)
*          scheduleUUID        - schedule UUID (can be NULL)
*          archiveType         - archive type (can be NULL)
*          storageName         - storage name (can be NULL)
*          createdDateTime     - created date/time (can be NULL)
-*         size                - size of stroage (can be NULL)
*          totalEntryCount     - total number of entries (can be NULL)
*          totalEntrySize      - total size [bytes] (can be NULL)
*          indexState          - index state (can be NULL)
*          indexMode           - index mode (can be NULL)
*          lastCheckedDateTime - last checked date/time (can be NULL)
*          errorMessage        - last error message (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_getStorage(IndexHandle  *indexHandle,
                        IndexId      storageId,
                        IndexId      *uuidId,
                        String       jobUUID,
                        IndexId      *entityId,
                        String       scheduleUUID,
                        ArchiveTypes archiveType,
                        String       storageName,
                        uint64       *createdDateTime,
                        uint64       *size,
                        uint64       *totalEntryCount,
                        uint64       *totalEntrySize,
                        IndexStates  *indexState,
                        IndexModes   *indexMode,
                        uint64       *lastCheckedDateTime,
                        String       errorMessage
                       );

/***********************************************************************\
* Name   : Index_storageUpdate
* Purpose: update storage index name/entries/size
* Input  : indexHandle - index handle
*          storageId   - index id of storage index
*          storageName - storage name (can be NULL)
*          storageSize - storage size [bytes] (can be 0)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_storageUpdate(IndexHandle *indexHandle,
                           IndexId     storageId,
                           ConstString storageName,
                           uint64      storageSize
                          );

/***********************************************************************\
* Name   : Index_getEntriesInfo
* Purpose: get entries info
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          indexIds         - uuid/entity/storage ids or NULL
*          indexIdCount     - uuid/entity/storage id count or 0
*          entryIds         - entry ids or NULL
*          entryIdCount     - entry id count or 0
*          indexTypeSet     - index type set or INDEX_TYPE_SET_ANY
*          name             - name pattern (glob, can be NULL)
*          newestOnly       - TRUE for newest entries only
* Output : totalEntryCount       - total entry count (can be NULL)
*          totalEntrySize        - total size [bytes] (can be NULL)
*          totalEntryContentSize - total size including directory
*                                  content [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_getEntriesInfo(IndexHandle   *indexHandle,
                            const IndexId indexIds[],
                            uint          indexIdCount,
                            const IndexId entryIds[],
                            uint          entryIdCount,
                            IndexTypeSet  indexTypeSet,
                            ConstString   name,
                            bool          newestOnly,
                            ulong         *totalEntryCount,
                            uint64        *totalEntrySize,
                            uint64        *totalEntryContentSize
                           );

/***********************************************************************\
* Name   : Index_initListEntries
* Purpose: list entries
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          indexIds         - uuid/entity/storage ids or NULL
*          indexIdCount     - uuid/entity/storage id count or 0
*          entryIds         - entry ids or NULL
*          entryIdCount     - entry id count or 0
*          indexTypeSet     - index type set or INDEX_TYPE_SET_ANY
*          name             - name pattern (glob, can be NULL)
*          newestOnly       - TRUE for newest entries only
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListEntries(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const IndexId    indexIds[],
                             uint             indexIdCount,
                             const IndexId    entryIds[],
                             uint             entryIdCount,
                             IndexTypeSet     indexTypeSet,
                             ConstString      name,
                             DatabaseOrdering ordering,
                             bool             newestOnly,
                             uint64           offset,
                             uint64           limit
                            );

/***********************************************************************\
* Name   : Index_getNextEntry
* Purpose: get next entry
* Input  : indexQueryHandle - index query handle
* Output : uuidId                   - index if of uuid
*          jobUUID                  - job UUID (can be NULL)
*          entityId                 - index id of entity (can be NULL)
*          scheduleUUID             - schedule UUID (can be NULL)
*          archiveType              - archive type (can be NULL)
*          storageId                - index id of storage (can be NULL)
*          storageName              - storage name (can be NULL)
*          storageDateTime          - storage date/time stamp [s]
*          entryId                  - index id of entry
*          entryName                - entry name
*          destinationName          - destination name (for link entries)
*          fileSystemType           - file system type (for image
*                                     entries)
*          size                     - file/image/hardlink size [bytes]
*                                     or directory size [bytes]
*          timeModified             - modified date/time stamp [s]
*          userId                   - user id
*          groupId                  - group id
*          permission               - permission flags
*          fragmentOrBlockOffset    - fragment offset [bytes]/[blocks]
*          fragmentSizeOrBlockCount - fragment size [bytes]/[blocks]
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextEntry(IndexQueryHandle  *indexQueryHandle,
                        IndexId           *uuidId,
                        String            jobUUID,
                        IndexId           *entityId,
                        String            scheduleUUID,
                        ArchiveTypes      *archiveType,
                        IndexId           *storageId,
                        String            storageName,
                        uint64            *storageDateTime,
                        IndexId           *entryId,
                        String            entryName,
                        String            destinationName,
                        FileSystemTypes   *fileSystemType,
                        uint64            *size,
                        uint64            *timeModified,
                        uint32            *userId,
                        uint32            *groupId,
                        uint32            *permission,
                        uint64            *fragmentOrBlockOffset,
                        uint64            *fragmentSizeOrBlockCount
                       );

/***********************************************************************\
* Name   : Index_deleteEntry
* Purpose: delete entry
* Input  : indexHandle - index handle
*          entryId     - index id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteEntry(IndexHandle *indexHandle,
                         IndexId     entryId
                        );

/***********************************************************************\
* Name   : Index_initListFiles
* Purpose: list file entries
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          storageIds       - storage ids or NULL
*          storageIdCount   - storage id count or 0
*          entryIds         - entry ids or NULL
*          entryIdCount     - entry id count or 0
*          name             - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListFiles(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    storageIds[],
                           uint             storageIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      name
                          );

/***********************************************************************\
* Name   : Index_getNextFile
* Purpose: get next file entry
* Input  : indexQueryHandle - index query handle
* Output : indexId        - index id of entry
*          storageName    - storage name (can be NULL)
*          fileName       - name
*          size           - size [bytes]
*          timeModified   - modified date/time stamp [s]
*          userId         - user id
*          groupId        - group id
*          permission     - permission flags
*          fragmentOffset - fragment offset [bytes]
*          fragmentSize   - fragment size [bytes]
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextFile(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       String           storageName,
                       uint64           *storageDateTime,
                       String           fileName,
                       uint64           *size,
                       uint64           *timeModified,
                       uint32           *userId,
                       uint32           *groupId,
                       uint32           *permission,
                       uint64           *fragmentOffset,
                       uint64           *fragmentSize
                      );

/***********************************************************************\
* Name   : Index_deleteFile
* Purpose: delete file entry
* Input  : indexHandle - index handle
*          indexId     - index id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteFile(IndexHandle *indexHandle,
                        IndexId     indexId
                       );

/***********************************************************************\
* Name   : Index_initListImages
* Purpose: list image entries
* Input  : indexHandle    - index handle
*          storageIds     - storage ids or NULL
*          storageIdCount - storage id count or 0
*          entryIds       - entry ids or NULL
*          entryIdCount   - entry id count or 0
*          name           - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListImages(IndexQueryHandle *indexQueryHandle,
                            IndexHandle      *indexHandle,
                            const IndexId    storageIds[],
                            uint             storageIdCount,
                            const IndexId    entryIds[],
                            uint             entryIdCount,
                            ConstString      name
                           );

/***********************************************************************\
* Name   : Index_getNextImage
* Purpose: get next image entry
* Input  : indexQueryHandle - index query handle
* Output : indexId      - index id of entry
*          storageName  - storage name
*          imageName    - image name
*          size         - size [bytes]
*          blockOffset  - block offset [blocks]
*          blockCount   - number of blocks
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextImage(IndexQueryHandle *indexQueryHandle,
                        IndexId          *indexId,
                        String           storageName,
                        uint64           *storageDateTime,
                        String           imageName,
                        FileSystemTypes  *fileSystemType,
                        uint64           *size,
                        uint64           *blockOffset,
                        uint64           *blockCount
                       );

/***********************************************************************\
* Name   : Index_deleteImage
* Purpose: delete image entry
* Input  : indexHandle - index handle
*          indexId     - index id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteImage(IndexHandle *indexHandle,
                         IndexId     indexId
                        );

/***********************************************************************\
* Name   : Index_initListDirectories
* Purpose: list directory entries
* Input  : indexHandle    - index handle
*          storageIds     - storage ids or NULL
*          storageIdCount - storage id count or 0
*          entryIds       - entry ids or NULL
*          entryIdCount   - entry id count or 0
*          name           - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListDirectories(IndexQueryHandle *indexQueryHandle,
                                 IndexHandle      *indexHandle,
                                 const IndexId    storageIds[],
                                 uint             storageIdCount,
                                 const IndexId    entryIds[],
                                 uint             entryIdCount,
                                 ConstString      name
                                );

/***********************************************************************\
* Name   : Index_getNextDirectory
* Purpose: get next directory entry
* Input  : indexQueryHandle - index query handle
* Output : indexId       - index id of entry
*          storageName   - storage name
*          directoryName - directory name
*          timeModified  - modified date/time stamp [s]
*          userId        - user id
*          groupId       - group id
*          permission    - permission flags
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextDirectory(IndexQueryHandle *indexQueryHandle,
                            IndexId          *indexId,
                            String           storageName,
                            uint64           *storageDateTime,
                            String           directoryName,
                            uint64           *timeModified,
                            uint32           *userId,
                            uint32           *groupId,
                            uint32           *permission
                           );

/***********************************************************************\
* Name   : Index_deleteDirectory
* Purpose: delete directory entry
* Input  : indexHandle - index handle
*          indexId     - index id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteDirectory(IndexHandle *indexHandle,
                             IndexId     indexId
                            );

/***********************************************************************\
* Name   : Index_initListLinks
* Purpose: list link entries
* Input  : indexHandle    - index handle
*          storageIds     - storage ids or NULL
*          storageIdCount - storage id count or 0
*          entryIds       - entry ids or NULL
*          entryIdCount   - entry id count or 0
*          name           - name pattern (glob, can be NULL)
* Output : indexQueryHandle - inxe query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListLinks(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    storageIds[],
                           uint             storageIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           ConstString      name
                          );

/***********************************************************************\
* Name   : Index_getNextLink
* Purpose: get next link entry
* Input  : indexQueryHandle - index query handle
* Output : indexId         - index id of entry
*          storageName     - storage name
*          linkName        - link name
*          destinationName - destination name
*          timeModified    - modified date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextLink(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       String           storageName,
                       uint64           *storageDateTime,
                       String           name,
                       String           destinationName,
                       uint64           *timeModified,
                       uint32           *userId,
                       uint32           *groupId,
                       uint32           *permission
                      );

/***********************************************************************\
* Name   : Index_deleteLink
* Purpose: delete link entry
* Input  : indexHandle - index handle
*          indexId     - index id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteLink(IndexHandle *indexHandle,
                        IndexId     indexId
                       );

/***********************************************************************\
* Name   : Index_initListHardLinks
* Purpose: list hard link entries
* Input  : indexHandle    - index handle
*          storageIds     - storage ids or NULL
*          storageIdCount - storage id count or 0
*          entryIds       - entry ids or NULL
*          entryIdCount   - entry id count or 0
*          name           - name pattern (glob, can be NULL)
* Output : indexQueryHandle - indxe query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListHardLinks(IndexQueryHandle *indexQueryHandle,
                               IndexHandle      *indexHandle,
                               const IndexId    storageIds[],
                               uint             storageIdCount,
                               const IndexId    entryIds[],
                               uint             entryIdCount,
                               ConstString      name
                               );

/***********************************************************************\
* Name   : Index_getNextHardLink
* Purpose: get next hard link entry
* Input  : indexQueryHandle - index query handle
* Output : indexId             - index id of entry
*          storageName         - storage name
*          fileName            - file name
*          destinationFileName - destination file name
*          size                - size [bytes]
*          timeModified        - modified date/time stamp [s]
*          userId              - user id
*          groupId             - group id
*          permission          - permission flags
*          fragmentOffset      - fragment offset [bytes]
*          fragmentSize        - fragment size [bytes]
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextHardLink(IndexQueryHandle *indexQueryHandle,
                           IndexId          *indexId,
                           String           storageName,
                           uint64           *storageDateTime,
                           String           fileName,
                           uint64           *size,
                           uint64           *timeModified,
                           uint32           *userId,
                           uint32           *groupId,
                           uint32           *permission,
                           uint64           *fragmentOffset,
                           uint64           *fragmentSize
                          );

/***********************************************************************\
* Name   : Index_deleteHardLink
* Purpose: delete hard link entry
* Input  : indexHandle - index handle
*          indexId     - index id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteHardLink(IndexHandle *indexHandle,
                            IndexId     indexId
                           );

/***********************************************************************\
* Name   : Index_initListSpecial
* Purpose: list special entries
* Input  : indexHandle    - index handle
*          storageIds     - storage ids or NULL
*          storageIdCount - storage id count or 0
*          entryIds       - entry ids or NULL
*          entryIdCount   - entry id count or 0
*          name           - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListSpecial(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const IndexId    storageIds[],
                             uint             storageIdCount,
                             const IndexId    entryIds[],
                             uint             entryIdCount,
                             ConstString      name
                            );

/***********************************************************************\
* Name   : Index_getNextSpecial
* Purpose: get next special entry
* Input  : indexQueryHandle - index query handle
* Output : indexId      - index id of entry
*          storageName  - storage name
*          name         - name
*          timeModified - modified date/time stamp [s]
*          userId       - user id
*          groupId      - group id
*          permission   - permission flags
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextSpecial(IndexQueryHandle *indexQueryHandle,
                          IndexId          *indexId,
                          String           storageName,
                          uint64           *storageDateTime,
                          String           name,
                          uint64           *timeModified,
                          uint32           *userId,
                          uint32           *groupId,
                          uint32           *permission
                         );

/***********************************************************************\
* Name   : Index_deleteSpecial
* Purpose: delete special entry
* Input  : indexHandle - index handle
*          indexId     - index id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteSpecial(IndexHandle *indexHandle,
                           IndexId     indexId
                          );

/***********************************************************************\
* Name   : Index_doneList
* Purpose: done index list
* Input  : indexQueryHandle - index query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Index_doneList(IndexQueryHandle *indexQueryHandle);

/***********************************************************************\
* Name   : Index_addFile
* Purpose: add file entry
* Input  : indexHandle     - index handle
*          storageId       - index id of index
*          name            - name
*          size            - size [bytes]
*          timeLastAccess  - last access date/time stamp [s]
*          timeModified    - modified date/time stamp [s]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
*          fragmentOffset  - fragment offset [bytes]
*          fragmentSize    - fragment size [bytes]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_addFile(IndexHandle *indexHandle,
                     IndexId     storageId,
                     ConstString fileName,
                     uint64      size,
                     uint64      timeLastAccess,
                     uint64      timeModified,
                     uint64      timeLastChanged,
                     uint32      userId,
                     uint32      groupId,
                     uint32      permission,
                     uint64      fragmentOffset,
                     uint64      fragmentSize
                    );

/***********************************************************************\
* Name   : Index_addImage
* Purpose: add image entry
* Input  : indexHandle    - index handle
*          storageId      - index id of index
*          imageName      - image name
*          fileSystemType - file system type
*          size           - size [bytes]
*          blockSize      - block size [bytes]
*          blockOffset    - block offset [blocks]
*          blockCount     - number of blocks
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_addImage(IndexHandle     *indexHandle,
                      IndexId         storageId,
                      ConstString     imageName,
                      FileSystemTypes fileSystemType,
                      int64           size,
                      ulong           blockSize,
                      uint64          blockOffset,
                      uint64          blockCount
                     );

/***********************************************************************\
* Name   : Index_addDirectory
* Purpose: add directory entry
* Input  : indexHandle     - index handle
*          storageId       - index id of index
*          directoryName   - name
*          timeLastAccess  - last access date/time stamp [s]
*          timeModified    - modified date/time stamp [s]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_addDirectory(IndexHandle *indexHandle,
                          IndexId     storageId,
                          String      directoryName,
                          uint64      timeLastAccess,
                          uint64      timeModified,
                          uint64      timeLastChanged,
                          uint32      userId,
                          uint32      groupId,
                          uint32      permission
                         );

/***********************************************************************\
* Name   : Index_addLink
* Purpose: add link entry
* Input  : indexHandle     - index handle
*          storageId       - index id of index
*          name            - linkName
*          destinationName - destination name
*          timeLastAccess  - last access date/time stamp [s]
*          timeModified    - modified date/time stamp [s]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_addLink(IndexHandle *indexHandle,
                     IndexId     storageId,
                     ConstString linkName,
                     ConstString destinationName,
                     uint64      timeLastAccess,
                     uint64      timeModified,
                     uint64      timeLastChanged,
                     uint32      userId,
                     uint32      groupId,
                     uint32      permission
                    );

/***********************************************************************\
* Name   : Index_addHardlink
* Purpose: add hard link entry
* Input  : indexHandle     - index handle
*          storageId       - index id of index
*          name            - name
*          size            - size [bytes]
*          timeLastAccess  - last access date/time stamp [s]
*          timeModified    - modified date/time stamp [s]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
*          fragmentOffset  - fragment offset [bytes]
*          fragmentSize    - fragment size [bytes]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_addHardlink(IndexHandle *indexHandle,
                         IndexId     storageId,
                         ConstString fileName,
                         uint64      size,
                         uint64      timeLastAccess,
                         uint64      timeModified,
                         uint64      timeLastChanged,
                         uint32      userId,
                         uint32      groupId,
                         uint32      permission,
                         uint64      fragmentOffset,
                         uint64      fragmentSize
                        );

/***********************************************************************\
* Name   : Index_addSpecial
* Purpose: add special entry
* Input  : indexHandle     - index handle
*          storageId       - index id of index
*          name            - name
*          specialType     - special type; see FileSpecialTypes
*          timeLastAccess  - last access date/time stamp [s]
*          timeModified    - modified date/time stamp [s]
*          timeLastChanged - last changed date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
*          major,minor     - major,minor number
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_addSpecial(IndexHandle      *indexHandle,
                        IndexId          storageId,
                        ConstString      name,
                        FileSpecialTypes specialType,
                        uint64           timeLastAccess,
                        uint64           timeModified,
                        uint64           timeLastChanged,
                        uint32           userId,
                        uint32           groupId,
                        uint32           permission,
                        uint32           major,
                        uint32           minor
                       );

/***********************************************************************\
* Name   : Index_assignTo
* Purpose: assign job/entity/storage to other entity/storage
* Input  : indexHandle   - index handle
*          jobUUID       - job UUID (can be NULL)
*          entityId      - index id of entity index (can be
*                          INDEX_ID_NONE)
*          storageId     - index id of storage index (can be
*                          INDEX_ID_NONE)
*          toEntityId    - to entity id (can be INDEX_ID_NONE)
*          toArchiveType - to archive type (can be ARCHIVE_TYPE_NONE)
*          toStorageId   - to storage id (can be INDEX_ID_NONE)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_assignTo(IndexHandle  *indexHandle,
//TODO: uuidId?
                      ConstString  jobUUID,
                      IndexId      entityId,
                      IndexId      storageId,
                      IndexId      toEntityId,
                      ArchiveTypes toArchiveType,
                      IndexId      toStorageId
                     );

/***********************************************************************\
* Name   : Index_pruneUUID
* Purpose: delete uuid from index if not used anymore (empty)
* Input  : indexHandle - index handle
*          jobUUID     - job UUID (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_pruneUUID(IndexHandle *indexHandle,
                       IndexId     uuidId
                      );

/***********************************************************************\
* Name   : Index_pruneEntity
* Purpose: delete entity from index if not used anymore (empty)
* Input  : indexHandle - index handle
*          entityId    - index id of entity entry
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_pruneEntity(IndexHandle *indexHandle,
                         IndexId     entityId
                        );

/***********************************************************************\
* Name   : Index_pruneStorage
* Purpose: delete storage from index if not used anymore (empty)
* Input  : indexHandle - index handle
*          storageId   - storage id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_pruneStorage(IndexHandle *indexHandle,
                          IndexId     storageId
                         );

/***********************************************************************\
* Name   : Index_initListSkippedEntry
* Purpose: list skipped entries
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          indexIds         - uuid/entity/storage ids or NULL
*          indexIdCount     - uuid/entity/storage id count or 0
*          entryIds         - entry ids or NULL
*          entryIdCount     - entry id count or 0
*          indexTypeSet     - index type set or INDEX_TYPE_SET_ANY
*          name             - name pattern (glob, can be NULL)
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListSkippedEntry(IndexQueryHandle *indexQueryHandle,
                                  IndexHandle      *indexHandle,
                                  const IndexId    indexIds[],
                                  uint             indexIdCount,
                                  const IndexId    entryIds[],
                                  uint             entryIdCount,
                                  IndexTypeSet     indexTypeSet,
                                  ConstString      name,
                                  DatabaseOrdering ordering,
                                  uint64           offset,
                                  uint64           limit
                                 );

/***********************************************************************\
* Name   : Index_getNextSkippedEntry
* Purpose: get next skipped entry
* Input  : indexQueryHandle - index query handle
* Output : uuidId          - index if of uuid
*          jobUUID         - job UUID (can be NULL)
*          entityId        - index id of entity (can be NULL)
*          scheduleUUID    - schedule UUID (can be NULL)
*          archiveType     - archive type (can be NULL)
*          storageId       - index id of storage (can be NULL)
*          storageName     - storage name (can be NULL)
*          storageDateTime - storage date/time stamp [s]
*          entryId         - index id of entry
*          entryName       - entry name
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextSkippedEntry(IndexQueryHandle *indexQueryHandle,
                               IndexId          *uuidId,
                               String           jobUUID,
                               IndexId          *entityId,
                               String           scheduleUUID,
                               ArchiveTypes     *archiveType,
                               IndexId          *storageId,
                               String           storageName,
                               uint64           *storageDateTime,
                               IndexId          *entryId,
                               String           entryName
                              );

/***********************************************************************\
* Name   : Index_addFile
* Purpose: add file entry
* Input  : indexHandle - index handle
*          storageId   - index id of index
*          entryName   - name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_addSkippedEntry(IndexHandle *indexHandle,
                             IndexId     storageId,
                             IndexTypes  type,
                             ConstString entryName
                            );

/***********************************************************************\
* Name   : Index_deleteSkippedEntry
* Purpose: delete skipped entry
* Input  : indexHandle - index handle
*          entryId     - index id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteSkippedEntry(IndexHandle *indexHandle,
                                IndexId     entryId
                               );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX__ */

/* end of file */
