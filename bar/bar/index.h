/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index database functions
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

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define INDEX_STORAGE_ID_NONE -1LL

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

#define INDEX_STATE_SET_ALL (1 << INDEX_STATE_NONE|\
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

#define INDEX_MODE_ALL (INDEX_MODE_MANUAL|INDEX_MODE_AUTO)

// index query handle
typedef struct
{
  DatabaseHandle      *databaseHandle;
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

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#define INDEX_STATE_SET(indexState) (1 << indexState)

#ifndef NDEBUG
  #define Index_init(...) __Index_init(__FILE__,__LINE__,__VA_ARGS__)
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
* Name   : Index_stateToString
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
* Name   : Index_parseMode
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
* Name   : Index_init
* Purpose: initialize index database
* Input  : indexDatabaseHandle   - index database handle variable
*          indexDatabaseFileName - database file name
* Output : indexDatabaseHandle - index database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Index_init(DatabaseHandle *indexDatabaseHandle,
                    const char     *indexDatabaseFileName
                   );
#else /* not NDEBUG */
  Errors __Index_init(const char     *__fileName__,
                      uint           __lineNb__,
                      DatabaseHandle *indexDatabaseHandle,
                      const char     *indexDatabaseFileName
                     );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Index_done
* Purpose: deinitialize index database
* Input  : indexDatabaseHandle - index database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Index_done(DatabaseHandle *indexDatabaseHandle);
#else /* not NDEBUG */
  void __Index_done(const char     *__fileName__,
                    uint           __lineNb__,
                    DatabaseHandle *indexDatabaseHandle
                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Index_findById
* Purpose: find index by id
* Input  : databaseHandle - database handle
*          storageId   - database id of index
* Output : storageName          - storage name
*          indexState           - index state (can be NULL)
*          lastCheckedTimestamp - last checked date/time stamp [s] (can
*                                 be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findById(DatabaseHandle *databaseHandle,
                    DatabaseId     storageId,
                    String         storageName,
                    IndexStates    *indexState,
                    uint64         *lastCheckedTimestamp
                   );

/***********************************************************************\
* Name   : Index_findByName
* Purpose: find index by name
* Input  : databaseHandle  - database handle
*          findStorageType - storage type to find or STORAGE_TYPE_ANY
*          findHostName    - host naem to find or NULL
*          findLoginName   - login name to find or NULL
*          findDeviceName  - device name to find or NULL
*          findFileName    - file name to find or NULL
* Output : storageId            - database id of index
*          uuid                 - unique id (can be NULL)
*          indexState           - index state (can be NULL)
*          lastCheckedTimestamp - last checked date/time stamp [s] (can
*                                 be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findByName(DatabaseHandle *databaseHandle,
                      StorageTypes   findStorageType,
                      const String   findHostName,
                      const String   findLoginName,
                      const String   findDeviceName,
                      const String   findFileName,
                      DatabaseId     *storageId,
                      String         uuid,
                      IndexStates    *indexState,
                      uint64         *lastCheckedTimestamp
                     );

/***********************************************************************\
* Name   : Index_findByState
* Purpose: find index by state
* Input  : databaseHandle - database handle
*          indexState     - index state
* Output : storageId            - database id of index
*          storageName          - storage name (can be NULL)
*          uuid                 - unique id (can be NULL)
*          lastCheckedTimestamp - last checked date/time stamp [s] (can
*                                 be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findByState(DatabaseHandle *databaseHandle,
                       IndexStateSet  indexStateSet,
                       DatabaseId     *storageId,
                       String         storageName,
                       String         uuid,
                       uint64         *lastCheckedTimestamp
                      );

/***********************************************************************\
* Name   : Index_clear
* Purpose: clear index content
* Input  : databaseHandle - database handle
*          storageId      - database id of index
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_clear(DatabaseHandle *databaseHandle,
                   DatabaseId     storageId
                  );

/***********************************************************************\
* Name   : Index_update
* Purpose: update index name/size
* Input  : databaseHandle - database handle
*          storageId      - database id of storage
*          storageName    - storage name (can be NULL)
*          size           - size [bytes]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_update(DatabaseHandle *databaseHandle,
                    DatabaseId     storageId,
                    const String   storageName,
                    uint64         size
                   );

/***********************************************************************\
* Name   : Index_getState
* Purpose: get index state
* Input  : databaseHandle - database handle
*          storageId      - database id of index
* Output : indexState           - index state; see IndexStates
*          lastCheckedTimestamp - last checked date/time stamp [s] (can
*                                 be NULL)
*          errorMessage         - error message (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_getState(DatabaseHandle *databaseHandle,
                      DatabaseId     storageId,
                      IndexStates    *indexState,
                      uint64         *lastCheckedTimestamp,
                      String         errorMessage
                     );

/***********************************************************************\
* Name   : Index_setState
* Purpose: set index state
* Input  : databaseHandle       - database handle
*          storageId            - database id of index
*          indexState           - index state; see IndexStates
*          lastCheckedTimestamp - last checked date/time stamp [s] (can
*                                 be 0LL)
*          errorMessage         - error message (can be NULL)
*          ...                  - optional arguments for error message
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_setState(DatabaseHandle *databaseHandle,
                      DatabaseId     storageId,
                      IndexStates    indexState,
                      uint64         lastCheckedTimestamp,
                      const char     *errorMessage,
                      ...
                     );

/***********************************************************************\
* Name   : Index_countState
* Purpose: get number of storage entries with specific state
* Input  : databaseHandle - database handle
*          indexState     - index state; see IndexStates
* Output : -
* Return : number of entries or -1
* Notes  : -
\***********************************************************************/

long Index_countState(DatabaseHandle *databaseHandle,
                      IndexStates    indexState
                     );

/***********************************************************************\
* Name   : Index_initListUUIDs
* Purpose: list uuid entries and aggregated data of entities
* Input  : IndexQueryHandle - index query handle variable
*          databaseHandle   - database handle
*          name             - name pattern (glob) or NULL
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListUUIDs(IndexQueryHandle *indexQueryHandle,
                           DatabaseHandle   *databaseHandle
                          );

/***********************************************************************\
* Name   : Index_getNextUUID
* Purpose: get next index uuid entry
* Input  : IndexQueryHandle - index query handle
* Output : jobUUID             - unique job id (can be NULL)
*          scheduleUUID        - unique schedule id (can be NULL)
*          lastCreatedDateTime - last storage date/time stamp [s] (can be NULL)
*          totalSize           - total storage size [bytes] (can be NULL)
*          lastErrorMessage    - last storage error message (can be NULL)
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextUUID(IndexQueryHandle *indexQueryHandle,
                       String           jobUUID,
                       String           scheduleUUID,
                       uint64           *lastCreatedDateTime,
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

Errors Index_deleteUUID(DatabaseHandle *databaseHandle,
                        const String   jobUUID
                       );

/***********************************************************************\
* Name   : Index_initListEntities
* Purpose: list entity entries and aggregated data of storage
* Input  : IndexQueryHandle - index query handle variable
*          databaseHandle   - database handle
*          jobUUID          - job UUID or NULL
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListEntities(IndexQueryHandle *indexQueryHandle,
                              DatabaseHandle   *databaseHandle,
                              const String     jobUUID
                             );

/***********************************************************************\
* Name   : Index_getNextEntity
* Purpose: get next index entity entry
* Input  : IndexQueryHandle - index query handle
* Output : databaseId          - database id of entry
*          jobUUID             - unique job id (can be NULL)
*          scheduleUUID        - unique schedule id (can be NULL)
*          lastCreatedDateTime - last storage date/time stamp [s] (can be NULL)
*          totalSize           - total storage size [bytes] (can be NULL)
*          lastErrorMessage    - last storage error message (can be NULL)
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextEntity(IndexQueryHandle *indexQueryHandle,
                         DatabaseId       *databaseId,
                         String           jobUUID,
                         String           scheduleUUID,
                         ArchiveTypes     *archiveType,
                         uint64           *lastCreatedDateTime,
                         uint64           *totalSize,
                         String           lastErrorMessage
                        );

/***********************************************************************\
* Name   : Index_newEntity
* Purpose: create new entity index
* Input  : databaseHandle - database handle
*          jobUUID             - unique job id (can be NULL)
*          scheduleUUID        - unique schedule id (can be NULL)
* Output : entityId - database id of new entity index
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_newEntity(DatabaseHandle *databaseHandle,
                       const String   jobUUID,
                       const String   scheduleUUID,
                       ArchiveTypes   archiveType,
                       DatabaseId     *entityId
                      );

/***********************************************************************\
* Name   : Index_deleteEntity
* Purpose: delete entity index including entries for attached storages
* Input  : indexQueryHandle - index query handle
*          entityId         - database id of entity index
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteEntity(DatabaseHandle *databaseHandle,
                          DatabaseId     entityId
                         );

/***********************************************************************\
* Name   : Index_initListStorage
* Purpose: list storage entries
* Input  : IndexQueryHandle - index query handle variable
*          databaseHandle   - database handle
*          uuid             - unique id or NULL
*          entityId         - entity id or DATABASE_ID_NONE
*          storageType      - storage type to find or STORAGE_TYPE_ANY
*          storageName      - storage name pattern (glob) or NULL
*          hostName         - host name pattern (glob) or NULL
*          loginName        - login name pattern (glob) or NULL
*          deviceName       - device name pattern (glob) or NULL
*          fileName         - file name pattern (glob) or NULL
*          indexState       - index state
* Output : IndexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListStorage(IndexQueryHandle *indexQueryHandle,
                             DatabaseHandle   *databaseHandle,
                             const String     uuid,
                             DatabaseId       entityId,
                             StorageTypes     storageType,
                             const String     storageName,
                             const String     hostName,
                             const String     loginName,
                             const String     deviceName,
                             const String     fileName,
                             IndexStateSet    indexStateSet
                            );

/***********************************************************************\
* Name   : Index_getNextStorage
* Purpose: get next index storage entry
* Input  : IndexQueryHandle    - index query handle
* Output : storageId           - database storage id of entry
*          entityId            - database entity id (can be NULL)
*          jobUUID             - unique job UUID (can be NULL)
*          scheduleUUID        - unique schedule UUID (can be NULL)
*          archiveType         - archive type (can be NULL)
*          storageName         - storage name (can be NULL)
*          createdDateTime     - date/time stamp [s]
*          size                - size [bytes]
*          indexState          - index state (can be NULL)
*          indexMode           - index mode (can be NULL)
*          lastCheckedDateTime - last checked date/time stamp [s] (can be NULL)
*          errorMessage        - last error message
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextStorage(IndexQueryHandle *indexQueryHandle,
                          DatabaseId       *storageId,
                          DatabaseId       *entityId,
                          String           jobUUID,
                          String           scheduleUUID,
                          ArchiveTypes     *archiveType,
                          String           storageName,
                          uint64           *createdDateTime,
                          uint64           *size,
                          IndexStates      *indexState,
                          IndexModes       *indexMode,
                          uint64           *lastCheckedDateTime,
                          String           errorMessage
                         );

/***********************************************************************\
* Name   : Index_newStorage
* Purpose: create new storage index
* Input  : databaseHandle - database handle
*          entityId       - database id of entity
*          storageName    - storage name
*          indexState     - index state
*          indexMode      - index mode
* Output : databaseId - storageId id of new storage index
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_newStorage(DatabaseHandle *databaseHandle,
                        DatabaseId     entityId,
                        const String   storageName,
                        IndexStates    indexState,
                        IndexModes     indexMode,
                        DatabaseId     *storageId
                       );

/***********************************************************************\
* Name   : Index_deleteStorage
* Purpose: delete storage index including entries for attached files,
*          image, directories, link, hard link, special entries
* Input  : indexQueryHandle - index query handle
*          databaseId       - database id of storage index
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteStorage(DatabaseHandle *databaseHandle,
                           DatabaseId     databaseId
                          );

/***********************************************************************\
* Name   : Index_initListFiles
* Purpose: list file entries
* Input  : indexQueryHandle - index query handle variable
*          databaseHandle   - database handle
*          pattern          - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListFiles(IndexQueryHandle *indexQueryHandle,
                           DatabaseHandle   *databaseHandle,
                           const DatabaseId storageIds[],
                           uint             storageIdCount,
                           String           pattern
                          );

/***********************************************************************\
* Name   : Index_getNextFile
* Purpose: get next file entry
* Input  : indexQueryHandle - index query handle
* Output : databaseId     - database id of entry
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
                       DatabaseId       *databaseId,
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
* Input  : databaseHandle - database handle
*          databaseId     - database id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteFile(DatabaseHandle *databaseHandle,
                        DatabaseId     databaseId
                       );

/***********************************************************************\
* Name   : Index_initListImages
* Purpose: list image entries
* Input  : databaseHandle - database handle
*          pattern        - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListImages(IndexQueryHandle *indexQueryHandle,
                            DatabaseHandle   *databaseHandle,
                            const DatabaseId *storageIds,
                            uint             storageIdCount,
                            String           pattern
                           );

/***********************************************************************\
* Name   : Index_getNextImage
* Purpose: get next image entry
* Input  : indexQueryHandle - index query handle
* Output : databaseId   - database id of entry
*          storageName  - storage name
*          imageName    - image name
*          size         - size [bytes]
*          blockOffset  - block offset [blocks]
*          blockCount   - number of blocks
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextImage(IndexQueryHandle *indexQueryHandle,
                        DatabaseId       *databaseId,
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
* Input  : databaseHandle - database handle
*          databaseId     - database id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteImage(DatabaseHandle *databaseHandle,
                         DatabaseId     databaseId
                        );

/***********************************************************************\
* Name   : Index_initListDirectories
* Purpose: list directory entries
* Input  : databaseHandle - database handle
*          pattern        - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListDirectories(IndexQueryHandle *indexQueryHandle,
                                 DatabaseHandle   *databaseHandle,
                                 const DatabaseId *storageIds,
                                 uint             storageIdCount,
                                 String           pattern
                                );

/***********************************************************************\
* Name   : Index_getNextDirectory
* Purpose: get next directory entry
* Input  : indexQueryHandle - index query handle
* Output : databaseId    - database id of entry
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
                            DatabaseId       *databaseId,
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
* Input  : databaseHandle - database handle
*          databaseId     - database id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteDirectory(DatabaseHandle *databaseHandle,
                             DatabaseId     databaseId
                            );

/***********************************************************************\
* Name   : Index_initListLinks
* Purpose: list link entries
* Input  : databaseHandle - database handle
*          pattern        - name pattern (glob, can be NULL)
* Output : indexQueryHandle - inxe query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListLinks(IndexQueryHandle *indexQueryHandle,
                           DatabaseHandle   *databaseHandle,
                           const DatabaseId *storageIds,
                           uint             storageIdCount,
                           String           pattern
                          );

/***********************************************************************\
* Name   : Index_getNextLink
* Purpose: get next link entry
* Input  : indexQueryHandle - index query handle
* Output : databaseId      - database id of entry
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
                       DatabaseId       *databaseId,
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
* Input  : databaseHandle - database handle
*          databaseId     - database id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteLink(DatabaseHandle *databaseHandle,
                        DatabaseId     databaseId
                       );

/***********************************************************************\
* Name   : Index_initListHardLinks
* Purpose: list hard link entries
* Input  : databaseHandle - database handle
*          pattern        - name pattern (glob, can be NULL)
* Output : indexQueryHandle - indxe query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListHardLinks(IndexQueryHandle *indexQueryHandle,
                               DatabaseHandle   *databaseHandle,
                               const DatabaseId *storageIds,
                               uint             storageIdCount,
                               String           pattern
                               );

/***********************************************************************\
* Name   : Index_getNextHardLink
* Purpose: get next hard link entry
* Input  : indexQueryHandle - index query handle
* Output : databaseId          - database id of entry
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
                           DatabaseId       *databaseId,
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
* Input  : databaseHandle - database handle
*          databaseId     - database id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteHardLink(DatabaseHandle *databaseHandle,
                            DatabaseId     databaseId
                           );

/***********************************************************************\
* Name   : Index_initListSpecial
* Purpose: list special entries
* Input  : databaseHandle - database handle
*          pattern        - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListSpecial(IndexQueryHandle *indexQueryHandle,
                             DatabaseHandle   *databaseHandle,
                             const DatabaseId *storageIds,
                             uint             storageIdCount,
                             String           pattern
                            );

/***********************************************************************\
* Name   : Index_getNextSpecial
* Purpose: get next special entry
* Input  : indexQueryHandle - index query handle
* Output : databaseId   - database id of entry
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
                          DatabaseId       *databaseId,
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
* Input  : databaseHandle - database handle
*          databaseId     - database id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_deleteSpecial(DatabaseHandle *databaseHandle,
                           DatabaseId     databaseId
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
* Input  : databaseHandle  - database handle
*          storageId       - database id of index
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

Errors Index_addFile(DatabaseHandle *databaseHandle,
                     DatabaseId     storageId,
                     const String   fileName,
                     uint64         size,
                     uint64         timeLastAccess,
                     uint64         timeModified,
                     uint64         timeLastChanged,
                     uint32         userId,
                     uint32         groupId,
                     uint32         permission,
                     uint64         fragmentOffset,
                     uint64         fragmentSize
                    );

/***********************************************************************\
* Name   : Index_addImage
* Purpose: add image entry
* Input  : databaseHandle - database handle
*          storageId      - database id of index
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

Errors Index_addImage(DatabaseHandle  *databaseHandle,
                      DatabaseId      storageId,
                      const String    imageName,
                      FileSystemTypes fileSystemType,
                      int64           size,
                      ulong           blockSize,
                      uint64          blockOffset,
                      uint64          blockCount
                     );

/***********************************************************************\
* Name   : Index_addDirectory
* Purpose: add directory entry
* Input  : databaseHandle  - database handle
*          storageId       - database id of index
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

Errors Index_addDirectory(DatabaseHandle *databaseHandle,
                          DatabaseId     storageId,
                          String         directoryName,
                          uint64         timeLastAccess,
                          uint64         timeModified,
                          uint64         timeLastChanged,
                          uint32         userId,
                          uint32         groupId,
                          uint32         permission
                         );

/***********************************************************************\
* Name   : Index_addLink
* Purpose: add link entry
* Input  : databaseHandle  - database handle
*          storageId       - database id of index
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

Errors Index_addLink(DatabaseHandle *databaseHandle,
                     DatabaseId     storageId,
                     const String   linkName,
                     const String   destinationName,
                     uint64         timeLastAccess,
                     uint64         timeModified,
                     uint64         timeLastChanged,
                     uint32         userId,
                     uint32         groupId,
                     uint32         permission
                    );

/***********************************************************************\
* Name   : Index_addHardLink
* Purpose: add hard link entry
* Input  : databaseHandle  - database handle
*          storageId       - database id of index
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

Errors Index_addHardLink(DatabaseHandle *databaseHandle,
                         DatabaseId     storageId,
                         const String   fileName,
                         uint64         size,
                         uint64         timeLastAccess,
                         uint64         timeModified,
                         uint64         timeLastChanged,
                         uint32         userId,
                         uint32         groupId,
                         uint32         permission,
                         uint64         fragmentOffset,
                         uint64         fragmentSize
                        );

/***********************************************************************\
* Name   : Index_addSpecial
* Purpose: add special entry
* Input  : databaseHandle  - database handle
*          storageId       - database id of index
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

Errors Index_addSpecial(DatabaseHandle   *databaseHandle,
                        DatabaseId       storageId,
                        const String     name,
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

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX__ */

/* end of file */
