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

#include "storage.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define INDEX_STORAGE_ID_NONE -1LL

// max. limit value
#define INDEX_UNLIMITED 9223372036854775807LL

/***************************** Datatypes *******************************/

// index states
typedef enum
{
  INDEX_STATE_NONE,

  INDEX_STATE_OK,
  INDEX_STATE_CREATE,
  INDEX_STATE_UPDATE_REQUESTED,
  INDEX_STATE_UPDATE,
  INDEX_STATE_ERROR,

  INDEX_STATE_UNKNOWN
} IndexStates;
typedef uint64 IndexStateSet;

#define INDEX_STATE_SET_NONE 0
#define INDEX_STATE_SET_ALL  (1 << INDEX_STATE_NONE|\
                              1 << INDEX_STATE_OK|\
                              1 << INDEX_STATE_CREATE|\
                              1 << INDEX_STATE_UPDATE_REQUESTED|\
                              1 << INDEX_STATE_UPDATE|\
                              1 << INDEX_STATE_ERROR\
                             )

// index modes
typedef enum
{
  INDEX_MODE_MANUAL,
  INDEX_MODE_AUTO,

  INDEX_MODE_UNKNOWN
} IndexModes;
typedef uint64 IndexModeSet;

#define INDEX_MODE_SET_NONE 0
#define INDEX_MODE_SET_ALL  (INDEX_MODE_MANUAL|INDEX_MODE_AUTO)

// index handle
typedef struct
{
  const char     *databaseFileName;
  DatabaseHandle databaseHandle;
  Errors         upgradeError;
  bool           quitFlag;
} IndexHandle;

// index query handle
typedef struct
{
  IndexHandle         *indexHandle;
  DatabaseQueryHandle databaseQueryHandle;
  struct
  {
    StorageTypes type;
    Pattern      *storageNamePattern;
    Pattern      *hostNamePattern;
    Pattern      *loginNamePattern;
    Pattern      *deviceNamePattern;
    Pattern      *fileNamePattern;
  } storage;
} IndexQueryHandle;

// index types
typedef enum
{
  INDEX_TYPE_NONE,

  INDEX_TYPE_UUID,
  INDEX_TYPE_ENTITY,
  INDEX_TYPE_STORAGE,
  INDEX_TYPE_FILE,
  INDEX_TYPE_IMAGE,
  INDEX_TYPE_DIRECTORY,
  INDEX_TYPE_LINK,
  INDEX_TYPE_HARDLINK,
  INDEX_TYPE_SPECIAL
} IndexTypes;

#define INDEX_TYPE_SET_NONE 0
#define INDEX_TYPE_SET_ANY \
  (  SET_VALUE(INDEX_TYPE_STORAGE) \
   | SET_VALUE(INDEX_TYPE_ENTITY) \
   | SET_VALUE(INDEX_TYPE_FILE) \
   | SET_VALUE(INDEX_TYPE_IMAGE) \
   | SET_VALUE(INDEX_TYPE_DIRECTORY) \
   | SET_VALUE(INDEX_TYPE_LINK) \
   | SET_VALUE(INDEX_TYPE_HARDLINK) \
   | SET_VALUE(INDEX_TYPE_SPECIAL) \
  )

typedef ulong IndexTypeSet;

// index id
typedef int64 IndexId;

// internal usage only!
typedef union
{
  struct
  {
    IndexTypes type : 4;
    DatabaseId databaseId : 60;
  };
  IndexId data;
} __IndexId;

// special index ids
#define INDEX_ID_NONE  0LL
#define INDEX_ID_ANY  -1LL

/***************************** Variables *******************************/

/****************************** Macros *********************************/

// create index state set value
#define INDEX_STATE_SET(indexState) (1 << indexState)

// get type, database id from index id
#define INDEX_TYPE_(indexId)             (((__IndexId)(indexId)).type                   )
#define INDEX_DATABASE_ID_(indexId)      (((__IndexId)(indexId)).databaseId             )

#ifndef NDEBUG
  #define Index_done(...) __Index_done(__FILE__,__LINE__,__VA_ARGS__)
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
* Input  : indexHandle      - index handle variable
*          databaseFileName - database file name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_init(IndexHandle *indexHandle,
                  const char  *databaseFileName
                 );

/***********************************************************************\
* Name   : Index_done
* Purpose: deinitialize index database
* Input  : indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
void Index_done(IndexHandle *indexHandle);
#else /* not NDEBUG */
void __Index_done(const char  *__fileName__,
                  uint        __lineNb__,
                  IndexHandle *indexHandle
                 );
#endif /* NDEBUG */

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
#endif /* NDEBUG || __ARCHIVE_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Index_findById
* Purpose: find storage by id
* Input  : indexHandle - index handle
*          storageId   - index id of storage
* Output : jobUUID             - unique job id (can be NULL)
*          scheduleUUID        - unique schedule id (can be NULL)
*          entityId            - index id of entity (can be NULL)
*          storageName         - storage name (can be NULL)
*          indexState          - index state (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can
*                                be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findById(IndexHandle *indexHandle,
                    IndexId     storageId,
                    String      jobUUID,
                    String      scheduleUUID,
                    IndexId     *entityId,
                    String      storageName,
                    IndexStates *indexState,
                    uint64      *lastCheckedDateTime
                   );

/***********************************************************************\
* Name   : Index_findByName
* Purpose: find storage by name
* Input  : indexHandle     - index handle
*          findStorageType - storage type to find or STORAGE_TYPE_ANY
*          findHostName    - host naem to find or NULL
*          findLoginName   - login name to find or NULL
*          findDeviceName  - device name to find or NULL
*          findFileName    - file name to find or NULL
* Output : jobUUID             - unique job id (can be NULL)
*          scheduleUUID        - unique schedule id (can be NULL)
*          entityId            - index id of entity (can be NULL)
*          storageId           - index id of storage (can be NULL)
*          indexState          - index state (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can
*                                be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findByName(IndexHandle  *indexHandle,
                      StorageTypes findStorageType,
                      ConstString  findHostName,
                      ConstString  findLoginName,
                      ConstString  findDeviceName,
                      ConstString  findFileName,
                      String       jobUUID,
                      String       scheduleUUID,
                      IndexId      *entityId,
                      IndexId      *storageId,
                      IndexStates  *indexState,
                      uint64       *lastCheckedDateTime
                     );

/***********************************************************************\
* Name   : Index_findByState
* Purpose: find index by state
* Input  : indexHandle - index handle
*          indexState  - index state
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

bool Index_findByState(IndexHandle   *indexHandle,
                       IndexStateSet indexStateSet,
                       String        jobUUID,
                       String        scheduleUUID,
                       IndexId       *entityId,
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
* Name   : Index_initListUUIDs
* Purpose: list uuid entries and aggregated data of entities
* Input  : IndexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          name             - name pattern (glob) or NULL
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListUUIDs(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle
                          );

/***********************************************************************\
* Name   : Index_getNextUUID
* Purpose: get next index uuid entry
* Input  : IndexQueryHandle - index query handle
* Output : indexId             - index id of UUID
*          jobUUID             - unique job id (can be NULL)
*          lastCreatedDateTime - last storage date/time stamp [s] (can be NULL)
*          totalEntries        - total number of entries (can be NULL)
*          totalSize           - total storage size [bytes] (can be NULL)
*          lastErrorMessage    - last storage error message (can be NULL)
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextUUID(IndexQueryHandle *indexQueryHandle,
                       IndexId          *indexId,
                       String           jobUUID,
                       uint64           *lastCreatedDateTime,
                       uint64           *totalEntries,
                       uint64           *totalSize,
                       String           lastErrorMessage
                      );

/***********************************************************************\
* Name   : Index_deleteUUID
* Purpose: delete job UUID index including entries for attached entities
* Input  : indexQueryHandle - index query handle
*          jobUUID          - unique job id
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteUUID(IndexHandle *indexHandle,
                        ConstString jobUUID
                       );

/***********************************************************************\
* Name   : Index_initListEntities
* Purpose: list entity entries and aggregated data of storage
* Input  : IndexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          jobUUID          - job UUID or NULL
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListEntities(IndexQueryHandle *indexQueryHandle,
                              IndexHandle      *indexHandle,
                              ConstString      jobUUID,
                              ConstString      scheduleUUID,
                              DatabaseOrdering ordering,
                              ulong            offset
                             );

/***********************************************************************\
* Name   : Index_getNextEntity
* Purpose: get next index entity entry
* Input  : IndexQueryHandle - index query handle
* Output : entityId         - index id of entity
*          jobUUID          - unique job id (can be NULL)
*          scheduleUUID     - unique schedule id (can be NULL)
*          createdDateTime  - created date/time stamp [s] (can be NULL)
*          archiveType      - archive type (can be NULL)
*          totalEntries     - total number of entries (can be NULL)
*          totalSize        - total storage size [bytes] (can be NULL)
*          lastErrorMessage - last storage error message (can be NULL)
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextEntity(IndexQueryHandle *indexQueryHandle,
                         IndexId          *entityId,
                         String           jobUUID,
                         String           scheduleUUID,
                         uint64           *createdDateTime,
                         ArchiveTypes     *archiveType,
                         uint64           *totalEntries,
                         uint64           *totalSize,
                         String           lastErrorMessage
                        );

/***********************************************************************\
* Name   : Index_newEntity
* Purpose: create new entity index
* Input  : indexHandle  - index handle
*          jobUUID      - unique job id (can be NULL)
*          scheduleUUID - unique schedule id (can be NULL)
*          archiveType  - archive type
* Output : entityId - index id of new entity index
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_newEntity(IndexHandle  *indexHandle,
                       ConstString  jobUUID,
                       ConstString  scheduleUUID,
                       ArchiveTypes archiveType,
                       IndexId      *entityId
                      );

/***********************************************************************\
* Name   : Index_deleteEntity
* Purpose: delete entity index including entries for attached storages
* Input  : indexQueryHandle - index query handle
*          entityId         - index id of entity index
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteEntity(IndexHandle *indexHandle,
                          IndexId     entityId
                         );

/***********************************************************************\
* Name   : Index_getStoragesInfo
* Purpose: get storages info
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          storageIds       - storage ids or NULL
*          storageIdCount   - storage id count or 0
*          indexStateSet    - index state set or INDEX_STATE_SET_ANY
*          pattern          - name pattern (glob, can be NULL)
* Output : count - entry count (can be NULL)
*          size  - size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_getStoragesInfo(IndexHandle   *indexHandle,
                             const IndexId storageIds[],
                             uint          storageIdCount,
                             IndexStateSet indexStateSet,
                             String        pattern,
                             ulong         *count,
                             uint64        *size
                            );

/***********************************************************************\
* Name   : Index_initListStorages
* Purpose: list storage entries
* Input  : IndexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          uuid             - unique job id or NULL
*          entityId         - entity id or INDEX_ID_ANY
*          storageType      - storage type to find or STORAGE_TYPE_ANY
*          storageName      - storage name pattern (glob) or NULL
*          hostName         - host name pattern (glob) or NULL
*          loginName        - login name pattern (glob) or NULL
*          deviceName       - device name pattern (glob) or NULL
*          fileName         - file name pattern (glob) or NULL
*          indexStateSet    - index state set
*          storageIds       - storage ids or NULL
*          storageIdCount   - storage id count or 0
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListStorages(IndexQueryHandle *indexQueryHandle,
                              IndexHandle      *indexHandle,
                              ConstString      uuid,
                              IndexId          entityId,
                              StorageTypes     storageType,
                              ConstString      storageName,
                              ConstString      hostName,
                              ConstString      loginName,
                              ConstString      deviceName,
                              ConstString      fileName,
                              IndexStateSet    indexStateSet,
                              const IndexId    storageIds[],
                              uint             storageIdCount,
                              uint64           offset,
                              uint64           limit
                             );

/***********************************************************************\
* Name   : Index_getNextStorage
* Purpose: get next index storage entry
* Input  : IndexQueryHandle    - index query handle
* Output : storageId           - index storage id of entry
*          entityId            - index entity id (can be NULL)
*          jobUUID             - unique job UUID (can be NULL)
*          scheduleUUID        - unique schedule UUID (can be NULL)
*          archiveType         - archive type (can be NULL)
*          storageName         - storage name (can be NULL)
*          createdDateTime     - date/time stamp [s] (can be NULL)
*          entries             - number of entries (can be NULL)
*          size                - size [bytes] (can be NULL)
*          indexState          - index state (can be NULL)
*          indexMode           - index mode (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can be NULL)
*          errorMessage        - last error message
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextStorage(IndexQueryHandle *indexQueryHandle,
                          IndexId          *storageId,
                          IndexId          *entityId,
                          String           jobUUID,
                          String           scheduleUUID,
                          ArchiveTypes     *archiveType,
                          String           storageName,
                          uint64           *createdDateTime,
                          uint64           *entries,
                          uint64           *size,
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
* Purpose: delete storage index including entries for attached files,
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
* Output:  storageName         - storage name (can be NULL)
*          createdDateTime     - created date/time (can be NULL)
*          entries             - number of entries (can be NULL)
*          size                - size [bytes] (can be NULL)
*          indexState          - index state (can be NULL)
*          indexMode           - index mode (can be NULL)
*          lastCheckedDateTime - last checked date/time (can be NULL)
*          errorMessage        - last error message (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_getStorage(IndexHandle *indexHandle,
                        IndexId     storageId,
                        String      storageName,
                        uint64      *createdDateTime,
                        uint64      *entries,
                        uint64      *size,
                        IndexStates *indexState,
                        IndexModes  *indexMode,
                        uint64      *lastCheckedDateTime,
                        String      errorMessage
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
*          storageIds       - storage ids or NULL
*          storageIdCount   - storage id count or 0
*          entryIds         - entry ids or NULL
*          entryIdCount     - entry id count or 0
*          indexTypeSet     - index type set or INDEX_TYPE_SET_ANY
*          pattern          - name pattern (glob, can be NULL)
* Output : count - entry count (can be NULL)
*          size  - size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_getEntriesInfo(IndexHandle   *indexHandle,
                            const IndexId storageIds[],
                            uint          storageIdCount,
                            const IndexId entryIds[],
                            uint          entryIdCount,
                            IndexTypeSet  indexTypeSet,
                            String        pattern,
                            ulong         *count,
                            uint64        *size
                           );

/***********************************************************************\
* Name   : Index_initListEntries
* Purpose: list entries
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          storageIds       - storage ids or NULL
*          storageIdCount   - storage id count or 0
*          entryIds         - entry ids or NULL
*          entryIdCount     - entry id count or 0
*          indexTypeSet     - index type set or INDEX_TYPE_SET_ANY
*          pattern          - name pattern (glob, can be NULL)
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListEntries(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const IndexId    storageIds[],
                             uint             storageIdCount,
                             const IndexId    entryIds[],
                             uint             entryIdCount,
                             IndexTypeSet     indexTypeSet,
                             String           pattern,
                             uint64           offset,
                             uint64           limit
                            );

/***********************************************************************\
* Name   : Index_getNext
* Purpose: get next entry
* Input  : indexQueryHandle - index query handle
* Output : indexId                     - index id of entry
*          storageName                 - storage name (can be NULL)
*          storageDateTime             - storage date/time stamp [s]
*          name                        - entry name
*          destinationName             - destination name (for link
*                                        entries)
*          fileSystemType              - file system type (for image
*                                        entries)
*          size                        - size [bytes]
*          timeModified                - modified date/time stamp [s]
*          userId                      - user id
*          groupId                     - group id
*          permission                  - permission flags
*          fragmentOffsetOrBlockOffset - fragment/block offset
*                                        [bytes/block]
*          fragmentSizeOrBlockSize     - fragment/block size [bytes]
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNext(IndexQueryHandle  *indexQueryHandle,
                   IndexId           *indexId,
                   String            storageName,
                   uint64            *storageDateTime,
                   String            name,
                   String            destinationName,
                   FileSystemTypes   *fileSystemType,
                   uint64            *size,
                   uint64            *timeModified,
                   uint32            *userId,
                   uint32            *groupId,
                   uint32            *permission,
                   uint64            *fragmentOrBlockOffset,
                   uint64            *fragmentOrBlockSize
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
*          pattern          - name pattern (glob, can be NULL)
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
                           String           pattern
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
*          pattern        - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListImages(IndexQueryHandle *indexQueryHandle,
                            IndexHandle      *indexHandle,
                            const IndexId    *storageIds,
                            uint             storageIdCount,
                            const IndexId    entryIds[],
                            uint             entryIdCount,
                            String           pattern
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
*          pattern        - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListDirectories(IndexQueryHandle *indexQueryHandle,
                                 IndexHandle      *indexHandle,
                                 const IndexId    *storageIds,
                                 uint             storageIdCount,
                                 const IndexId    entryIds[],
                                 uint             entryIdCount,
                                 String           pattern
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
*          pattern        - name pattern (glob, can be NULL)
* Output : indexQueryHandle - inxe query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListLinks(IndexQueryHandle *indexQueryHandle,
                           IndexHandle      *indexHandle,
                           const IndexId    *storageIds,
                           uint             storageIdCount,
                           const IndexId    entryIds[],
                           uint             entryIdCount,
                           String           pattern
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
*          pattern        - name pattern (glob, can be NULL)
* Output : indexQueryHandle - indxe query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListHardLinks(IndexQueryHandle *indexQueryHandle,
                               IndexHandle      *indexHandle,
                               const IndexId    *storageIds,
                               uint             storageIdCount,
                               const IndexId    entryIds[],
                               uint             entryIdCount,
                               String           pattern
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
*          pattern        - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListSpecial(IndexQueryHandle *indexQueryHandle,
                             IndexHandle      *indexHandle,
                             const IndexId    *storageIds,
                             uint             storageIdCount,
                             const IndexId    entryIds[],
                             uint             entryIdCount,
                             String           pattern
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
* Name   : Index_addHardLink
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

Errors Index_addHardLink(IndexHandle *indexHandle,
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
*          toStorageId   - to storage id (can be INDEX_ID_NONE)
*          toArchiveType - to archive type (can be ARCHIVE_TYPE_NONE)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_assignTo(IndexHandle  *indexHandle,
                      ConstString  jobUUID,
                      IndexId      entityId,
                      IndexId      storageId,
                      IndexId      toEntityId,
                      IndexId      toStorageId,
                      ArchiveTypes toArchiveType
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
* Name   : Index_pruneEntity
* Purpose: delete entity from index if not used anymore (empty)
* Input  : indexHandle - index handle
*          storageId   - storage id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_pruneEntity(IndexHandle *indexHandle,
                         IndexId     entityId
                        );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX__ */

/* end of file */
