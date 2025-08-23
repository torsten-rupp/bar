/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index entry functions
* Systems: all
*
\***********************************************************************/

#ifndef __INDEX_ENTRIES__
#define __INDEX_ENTRIES__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "common/progressinfo.h"
#include "errors.h"

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
* Name   : IndexEntry_addFile
* Purpose: add file entry
* Input  : indexHandle     - index handle
*          uuidId          - index id of UUID
*          entityId        - index id of entity
*          storageId       - index id of storage
*          name            - file name
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

Errors IndexEntry_addFile(IndexHandle *indexHandle,
                          IndexId     uuidId,
                          IndexId     entityId,
                          IndexId     storageId,
                          ConstString name,
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
* Name   : IndexEntry_addImage
* Purpose: add image entry
* Input  : indexHandle    - index handle
*          uuidId         - index id of UUID
*          entityId       - index id of entity
*          storageId      - index id of storage
*          name           - image name
*          fileSystemType - file system type
*          size           - size [bytes]
*          blockSize      - block size [bytes]
*          blockOffset    - block offset [blocks]
*          blockCount     - number of blocks
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_addImage(IndexHandle     *indexHandle,
                           IndexId         uuidId,
                           IndexId         entityId,
                           IndexId         storageId,
                           ConstString     name,
                           FileSystemTypes fileSystemType,
                           int64           size,
                           uint            blockSize,
                           uint64          blockOffset,
                           uint64          blockCount
                          );

/***********************************************************************\
* Name   : IndexEntry_addDirectory
* Purpose: add directory entry
* Input  : indexHandle     - index handle
*          uuidId          - index id of UUID
*          entityId        - index id of entity
*          storageId       - index id of storage
*          name            - directory name
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

Errors IndexEntry_addDirectory(IndexHandle *indexHandle,
                               IndexId     uuidId,
                               IndexId     entityId,
                               IndexId     storageId,
                               String      name,
                               uint64      timeLastAccess,
                               uint64      timeModified,
                               uint64      timeLastChanged,
                               uint32      userId,
                               uint32      groupId,
                               uint32      permission
                              );

/***********************************************************************\
* Name   : IndexEntry_addLink
* Purpose: add link entry
* Input  : indexHandle     - index handle
*          uuidId          - index id of UUID
*          entityId        - index id of entity
*          storageId       - index id of storage
*          linkName        - link name
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

Errors IndexEntry_addLink(IndexHandle *indexHandle,
                          IndexId     uuidId,
                          IndexId     entityId,
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
* Name   : IndexEntry_addHardlink
* Purpose: add hard link entry
* Input  : indexHandle     - index handle
*          uuidId          - index id of UUID
*          entityId        - index id of entity
*          storageId       - index id of storage
*          name            - hardlink name
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

Errors IndexEntry_addHardlink(IndexHandle *indexHandle,
                              IndexId     uuidId,
                              IndexId     entityId,
                              IndexId     storageId,
                              ConstString name,
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
* Name   : IndexEntry_addSpecial
* Purpose: add special entry
* Input  : indexHandle     - index handle
*          uuidId          - index id of UUID
*          entityId        - index id of entity
*          storageId       - index id of storage
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

Errors IndexEntry_addSpecial(IndexHandle      *indexHandle,
                             IndexId          uuidId,
                             IndexId          entityId,
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
* Name   : IndexEntry_initList
* Purpose: list entries
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          indexIds         - uuid/entity/storage ids or NULL
*          indexIdCount     - uuid/entity/storage id count or 0
*          entryIds         - entry ids or NULL
*          entryIdCount     - entry id count or 0
*          indexType        - index type or INDEX_TYPE_NONE
*          name             - name pattern (glob, can be NULL)
*          newestOnly       - TRUE for newest entries only
*          fragmentsCount   - TRUE to get fragments count
*          sortMode         - sort mode; see IndexStorageSortModes
*          ordering         - ordering
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_initList(IndexQueryHandle    *indexQueryHandle,
                           IndexHandle         *indexHandle,
                           const IndexId       indexIds[],
                           uint                indexIdCount,
                           const IndexId       entryIds[],
                           uint                entryIdCount,
                           IndexTypes          indexType,
                           ConstString         name,
                           bool                newestOnly,
                           bool                fragmentsCount,
                           IndexEntrySortModes sortMode,
                           DatabaseOrdering    ordering,
                           uint64              offset,
                           uint64              limit
                          );

/***********************************************************************\
* Name   : IndexEntry_getNext
* Purpose: get next entry
* Input  : indexQueryHandle - index query handle
* Output : uuidId          - index id of UUID (can be NULL)
*          jobUUID         - job UUID (can be NULL)
*          entityId        - index id of entry
*          entityUUID      - schedule UUID (can be NULL)
*          userName        - user name (can be NULL)
*          hostName        - host name (can be NULL)
*          archiveType     - archive type (can be NULL)
*          entryId         - index id of entry
*          entryName       - entry name
*          storageId       - index id of storage (for directory, link,
*                            special entries, can be NULL)
*          storageName     - storage name  (for directory, link,
*                            special entries, can be NULL)
*          destinationName - destination name (for link entries)
*          fileSystemType  - file system type (for image
*                            entries)
*          size            - file/image/hardlink size [bytes]
*                            or directory size [bytes]
*          timeModified    - modified date/time stamp [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
*          fragmentCount   - fragment count
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexEntry_getNext(IndexQueryHandle *indexQueryHandle,
                        IndexId          *uuidId,
                        String           jobUUID,
                        IndexId          *entityId,
                        String           entityUUID,
                        String           userName,
                        String           hostName,
                        ArchiveTypes     *archiveType,
                        IndexId          *entryId,
                        String           entryName,
                        IndexId          *storageId,
                        String           storageName,
                        uint64           *size,
//TODO: use timeLastChanged
                        uint64           *timeModified,
                        uint32           *userId,
                        uint32           *groupId,
                        uint32           *permission,
                        uint             *fragmentCount,
                        String           destinationName,
                        FileSystemTypes  *fileSystemType,
                        uint             *blockSize
                       );

/***********************************************************************\
* Name   : IndexEntry_initListFragments
* Purpose: list fragments of entry
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          entryId          - entry id
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_initListFragments(IndexQueryHandle *indexQueryHandle,
                                    IndexHandle      *indexHandle,
                                    IndexId          entryId,
                                    uint64           offset,
                                    uint64           limit
                                   );

/***********************************************************************\
* Name   : IndexEntry_getNextFrament
* Purpose: get next fragment of entry
* Input  : indexQueryHandle - index query handle
* Output : entryFragmentId - index id of entry fragment
*          storageId       - index id of storage (can be NULL)
*          storageName     - storage name (can be NULL)
*          storageDateTime - storage date/time stamp [s]
*          fragmentOffset  - fragment offset [bytes]
*          fragmentSize    - fragment size [bytes]
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexEntry_getNextFragment(IndexQueryHandle *indexQueryHandle,
                                IndexId          *entryFragmentId,
                                IndexId          *storageId,
                                String           storageName,
                                uint64           *storageDateTime,
                                uint64           *fragmentOffset,
                                uint64           *fragmentSize
                               );

// ---------------------------------------------------------------------

/***********************************************************************\
* Name   : IndexEntry_initListFiles
* Purpose: list file entries
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          entityIds        - entity ids or NULL
*          entityIdCount    - entity id count or 0
*          entryIds         - entry ids or NULL
*          entryIdCount     - entry id count or 0
*          name             - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_initListFiles(IndexQueryHandle *indexQueryHandle,
                                IndexHandle      *indexHandle,
                                const IndexId    entityIds[],
                                uint             entityIdCount,
                                const IndexId    entryIds[],
                                uint             entryIdCount,
                                ConstString      name
                               );

/***********************************************************************\
* Name   : IndexEntry_getNextFile
* Purpose: get next file entry
* Input  : indexQueryHandle - index query handle
* Output : indexId         - entry index id
*          createdDateTime - create date/time [s]
*          fileName        - name
*          size            - size [bytes]
*          timeModified    - modified date/time [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexEntry_getNextFile(IndexQueryHandle *indexQueryHandle,
                            IndexId          *indexId,
                            uint64           *createdDateTime,
                            String           fileName,
                            uint64           *size,
//TODO: use timeLastChanged
                            uint64           *timeModified,
                            uint32           *userId,
                            uint32           *groupId,
                            uint32           *permission
                           );

// ---------------------------------------------------------------------

/***********************************************************************\
* Name   : IndexEntry_initListImages
* Purpose: list image entries
* Input  : indexHandle    - index handle
*          entityIds      - entity ids or NULL
*          entityIdCount  - entity id count or 0
*          entryIds       - entry ids or NULL
*          entryIdCount   - entry id count or 0
*          name           - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_initListImages(IndexQueryHandle *indexQueryHandle,
                                 IndexHandle      *indexHandle,
                                 const IndexId    entityIds[],
                                 uint             entityIdCount,
                                 const IndexId    entryIds[],
                                 uint             entryIdCount,
                                 ConstString      name
                                );

/***********************************************************************\
* Name   : IndexEntry_getNextImage
* Purpose: get next image entry
* Input  : indexQueryHandle - index query handle
* Output : indexId         - entry index id
*          createdDateTime - create date/time [s]
*          imageName       - image name
*          blockSize       - block size [bytes]
*          size            - size [bytes]
*          blockOffset     - block offset [blocks]
*          blockCount      - number of blocks
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexEntry_getNextImage(IndexQueryHandle *indexQueryHandle,
                             IndexId          *indexId,
                             uint64           *createdDateTime,
                             String           imageName,
                             FileSystemTypes  *fileSystemType,
                             uint             *blockSize,
                             uint64           *size,
                             uint64           *blockOffset,
                             uint64           *blockCount
                            );

// ---------------------------------------------------------------------

/***********************************************************************\
* Name   : IndexEntry_initListDirectories
* Purpose: list directory entries
* Input  : indexHandle    - index handle
*          entityIds      - entity ids or NULL
*          entityIdCount  - entity id count or 0
*          entryIds       - entry ids or NULL
*          entryIdCount   - entry id count or 0
*          name           - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_initListDirectories(IndexQueryHandle *indexQueryHandle,
                                      IndexHandle      *indexHandle,
                                      const IndexId    entityIds[],
                                      uint             entityIdCount,
                                      const IndexId    entryIds[],
                                      uint             entryIdCount,
                                      ConstString      name
                                     );

/***********************************************************************\
* Name   : IndexEntry_getNextDirectory
* Purpose: get next directory entry
* Input  : indexQueryHandle - index query handle
* Output : indexId         - entry index id
*          createdDateTime - create date/time [s]
*          directoryName   - directory name
*          timeModified    - modified date/time [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexEntry_getNextDirectory(IndexQueryHandle *indexQueryHandle,
                                 IndexId          *indexId,
                                 uint64           *createdDateTime,
                                 String           directoryName,
//TODO: use timeLastChanged
                                 uint64           *timeModified,
                                 uint32           *userId,
                                 uint32           *groupId,
                                 uint32           *permission
                                );

// ---------------------------------------------------------------------

/***********************************************************************\
* Name   : IndexEntry_initListLinks
* Purpose: list link entries
* Input  : indexHandle    - index handle
*          entityIds      - entity ids or NULL
*          entityIdCount  - entity id count or 0
*          entryIds       - entry ids or NULL
*          entryIdCount   - entry id count or 0
*          name           - name pattern (glob, can be NULL)
* Output : indexQueryHandle - inxe query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_initListLinks(IndexQueryHandle *indexQueryHandle,
                                IndexHandle      *indexHandle,
                                const IndexId    entityIds[],
                                uint             entityIdCount,
                                const IndexId    entryIds[],
                                uint             entryIdCount,
                                ConstString      name
                               );

/***********************************************************************\
* Name   : IndexEntry_getNextLink
* Purpose: get next link entry
* Input  : indexQueryHandle - index query handle
* Output : indexId         - entry index id
*          createdDateTime - create date/time [s]
*          linkName        - link name
*          destinationName - destination name
*          timeModified    - modified date/time [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexEntry_getNextLink(IndexQueryHandle *indexQueryHandle,
                            IndexId          *indexId,
                            uint64           *createdDateTime,
                            String           name,
                            String           destinationName,
//TODO: use timeLastChanged
                            uint64           *timeModified,
                            uint32           *userId,
                            uint32           *groupId,
                            uint32           *permission
                           );

// ---------------------------------------------------------------------

/***********************************************************************\
* Name   : IndexEntry_initListHardLinks
* Purpose: list hard link entries
* Input  : indexHandle    - index handle
*          entityIds      - entity ids or NULL
*          entityIdCount  - entity id count or 0
*          entryIds       - entry ids or NULL
*          entryIdCount   - entry id count or 0
*          name           - name pattern (glob, can be NULL)
* Output : indexQueryHandle - indxe query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_initListHardLinks(IndexQueryHandle *indexQueryHandle,
                                    IndexHandle      *indexHandle,
                                    const IndexId    entityIds[],
                                    uint             entityIdCount,
                                    const IndexId    entryIds[],
                                    uint             entryIdCount,
                                    ConstString      name
                                    );

/***********************************************************************\
* Name   : IndexEntry_getNextHardLink
* Purpose: get next hard link entry
* Input  : indexQueryHandle - index query handle
* Output : indexId             - entry index id
*          createdDateTime     - create date/time [s]
*          fileName            - file name
*          destinationFileName - destination file name
*          size                - size [bytes]
*          timeModified        - modified date/time [s]
*          userId              - user id
*          groupId             - group id
*          permission          - permission flags
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexEntry_getNextHardLink(IndexQueryHandle *indexQueryHandle,
                                IndexId          *indexId,
                                uint64           *createdDateTime,
                                String           fileName,
                                uint64           *size,
//TODO: use timeLastChanged
                                uint64           *timeModified,
                                uint32           *userId,
                                uint32           *groupId,
                                uint32           *permission
                               );

// ---------------------------------------------------------------------

/***********************************************************************\
* Name   : IndexEntry_initListSpecial
* Purpose: list special entries
* Input  : indexHandle    - index handle
*          entityIds      - entity ids or NULL
*          entityIdCount  - entity id count or 0
*          entryIds       - entry ids or NULL
*          entryIdCount   - entry id count or 0
*          name           - name pattern (glob, can be NULL)
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_initListSpecial(IndexQueryHandle *indexQueryHandle,
                                  IndexHandle      *indexHandle,
                                  const IndexId    entityIds[],
                                  uint             entityIdCount,
                                  const IndexId    entryIds[],
                                  uint             entryIdCount,
                                  ConstString      name
                                 );

/***********************************************************************\
* Name   : IndexEntry_getNextSpecial
* Purpose: get next special entry
* Input  : indexQueryHandle - index query handle
* Output : indexId         - entry index id
*          createdDateTime - create date/time [s]
*          name            - name
*          timeModified    - modified date/time [s]
*          userId          - user id
*          groupId         - group id
*          permission      - permission flags
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexEntry_getNextSpecial(IndexQueryHandle *indexQueryHandle,
                               IndexId          *indexId,
                               uint64           *createdDateTime,
                               String           name,
//TODO: use timeLastChanged
                               uint64           *timeModified,
                               uint32           *userId,
                               uint32           *groupId,
                               uint32           *permission
                              );

/***********************************************************************\
* Name   : IndexEntry_initListSkipped
* Purpose: list skipped entries
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          indexIds         - uuid/entity/storage ids or NULL
*          indexIdCount     - uuid/entity/storage id count or 0
*          entryIds         - entry ids or NULL
*          entryIdCount     - entry id count or 0
*          indexType        - index type or INDEX_TYPE_NONE
*          name             - name pattern (glob, can be NULL)
*          offset           - offset or 0
*          limit            - numer of entries to list or
*                             INDEX_UNLIMITED
* Output : indexQueryHandle - index query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_initListSkipped(IndexQueryHandle *indexQueryHandle,
                                  IndexHandle      *indexHandle,
                                  const IndexId    indexIds[],
                                  uint             indexIdCount,
                                  const IndexId    entryIds[],
                                  uint             entryIdCount,
                                  IndexTypes       indexType,
                                  ConstString      name,
                                  DatabaseOrdering ordering,
                                  uint64           offset,
                                  uint64           limit
                                 );

/***********************************************************************\
* Name   : IndexEntry_getNextSkipped
* Purpose: get next skipped entry
* Input  : indexQueryHandle - index query handle
* Output : uuidId          - index id of UUID
*          jobUUID         - job UUID (can be NULL)
*          entityId        - index id of entity (can be NULL)
*          entityUUID      - schedule UUID (can be NULL)
*          archiveType     - archive type (can be NULL)
*          storageId       - index id of storage (can be NULL)
*          storageName     - storage name (can be NULL)
*          storageDateTime - storage date/time stamp [s]
*          entryId         - index id of entry
*          entryName       - entry name
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool IndexEntry_getNextSkipped(IndexQueryHandle *indexQueryHandle,
                               IndexId          *uuidId,
                               String           jobUUID,
                               IndexId          *entityId,
                               String           entityUUID,
                               ArchiveTypes     *archiveType,
                               IndexId          *storageId,
                               String           storageName,
                               uint64           *storageDateTime,
                               IndexId          *entryId,
                               String           entryName
                              );

/***********************************************************************\
* Name   : IndexEntry_addSkipped
* Purpose: add file entry
* Input  : indexHandle - index handle
*          entityId    - index id of entity
*          entryName   - name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_addSkipped(IndexHandle *indexHandle,
                             IndexId     entityId,
                             IndexTypes  type,
                             ConstString entryName
                            );

/***********************************************************************\
* Name   : IndexEntry_delete
* Purpose: delete entry
* Input  : indexHandle - index handle
*          entryId     - index id of entry
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_delete(IndexHandle *indexHandle,
                         IndexId     entryId
                        );

/***********************************************************************\
* Name   : IndexEntry_getInfo
* Purpose: get entries info
* Input  : indexQueryHandle - index query handle variable
*          indexHandle      - index handle
*          indexIds         - uuid/entity/storage ids or NULL
*          indexIdCount     - uuid/entity/storage id count or 0
*          entryIds         - entry ids or NULL
*          entryIdCount     - entry id count or 0
*          indexTypes       - index type or INDEX_TYPE_NONE
*          name             - name pattern (glob, can be NULL)
*          newestOnly       - TRUE for newest entries only
* Output : totalStorageCount - total storage count (can be NULL)
*          totalStorageSize  - total storage size [bytes] (can be NULL)
*          totalEntryCount   - total entry count (can be NULL)
*          totalEntrySize    - total entry size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_getInfo(IndexHandle   *indexHandle,
                          const IndexId indexIds[],
                          uint          indexIdCount,
                          const IndexId entryIds[],
                          uint          entryIdCount,
                          IndexTypes    indexType,
                          ConstString   name,
                          bool          newestOnly,
                          uint          *totalStorageCount,
                          uint64        *totalStorageSize,
                          uint          *totalEntryCount,
                          uint64        *totalEntrySize
                         );

/***********************************************************************\
* Name   : IndexEntry_collectIds
* Purpose: collect entry ids for storage
* Input  : entryIds     - entry ids array variable
*          indexHandle  - index handle
*          storageId    - storage id
*          progressInfo - progress info
* Output : entryIds     - entry ids array
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_collectIds(Array        *entryIds,
                             IndexHandle  *indexHandle,
                             IndexId      storageId,
                             ProgressInfo *progressInfo
                            );

/***********************************************************************\
* Name   : IndexEntry_deleteAllWithoutFragments
* Purpose: delete all entries without fragments
* Input  : indexHandle - index handle
*          doneFlag       - done flag variable (can be NULL)
*          deletedCounter - deleted entries count variable (can be NULL)
* Output : doneFlag       - TRUE if all done
*          deletedCounter - number of purged entries
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexEntry_deleteAllWithoutFragments(IndexHandle *indexHandle,
                                            bool        *doneFlag,
                                            ulong       *deletedCounter
                                           );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX_ENTRIES__ */

/* end of file */
