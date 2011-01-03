/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/index.h,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: database index functions
* Systems: all
*
\***********************************************************************/

#ifndef __INDEX__
#define __INDEX__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "files.h"
#include "database.h"
#include "errors.h"

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define INDEX_STORAGE_ID_NONE -1LL

/* index states */
typedef enum
{
  INDEX_STATE_NONE,

  INDEX_STATE_OK,
  INDEX_STATE_CREATE,
  INDEX_STATE_UPDATE_REQUESTED,
  INDEX_STATE_UPDATE,
  INDEX_STATE_ERROR,

  INDEX_STATE_ALL,

  INDEX_STATE_UNKNOWN
} IndexStates;

extern const char* INDEX_STATE_STRINGS[8];

/* index modes */
typedef enum
{
  INDEX_MODE_MANUAL,
  INDEX_MODE_AUTO,

  INDEX_MODE_UNKNOWN  
} IndexModes;

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

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
* Name   : Index_stringToState
* Purpose: convert string to state
* Input  : string - string
* Output : -
* Return : state or INDEX_STATE_UNKNOWN if not known
* Notes  : -
\***********************************************************************/

IndexStates Index_stringToState(const String string);

/***********************************************************************\
* Name   : Index_init
* Purpose: initialize database index
* Input  : indexDatabaseHandle   - index database handle variable
*          indexDatabaseFileName - database file name
* Output : indexDatabaseHandle - index database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_init(DatabaseHandle *indexDatabaseHandle,
                  const char     *indexDatabaseFileName
                 );

/***********************************************************************\
* Name   : Index_done
* Purpose: deinitialize database index
* Input  : indexDatabaseHandle - index database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Index_done(DatabaseHandle *indexDatabaseHandle);

/***********************************************************************\
* Name   : Index_findByName
* Purpose: find index by name
* Input  : databaseHandle - database handle
*          name           - name
* Output : storageId   - database id of index
*          indexState  - index state (can be NULL)
*          lastChecked - last checked date/time stamp [s] (can be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findByName(DatabaseHandle *databaseHandle,
                      const String   name,
                      int64          *storageId,
                      IndexStates    *indexState,
                      uint64         *lastChecked
                     );

/***********************************************************************\
* Name   : Index_findByState
* Purpose: find index by state
* Input  : databaseHandle - database handle
*          indexState     - index state
* Output : storageId   - database id of index
*          name        - index name (can be NULL)
*          lastChecked - last checked date/time stamp [s] (can be NULL)
* Return : TRUE if index found, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_findByState(DatabaseHandle *databaseHandle,
                       IndexStates    indexState,
                       int64          *storageId,
                       String         name,
                       uint64         *lastChecked
                      );

/***********************************************************************\
* Name   : Index_create
* Purpose: create new index
* Input  : databaseHandle - database handle
*          name           - storage name
*          indexState     - index state
*          indexMode      - index mode
* Output : storageId - database id of index
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_create(DatabaseHandle *databaseHandle,
                    const String   name,
                    IndexStates    indexState,
                    IndexModes     indexMode,
                    int64          *storageId
                   );

/***********************************************************************\
* Name   : Index_delete
* Purpose: delete index
* Input  : databaseHandle - database handle
*          storageId      - database id of index
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_delete(DatabaseHandle *databaseHandle,
                    int64          storageId
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
                   int64          storageId
                  );

/***********************************************************************\
* Name   : Index_update
* Purpose: update index name/size
* Input  : databaseHandle - database handle
*          storageId      - database id of index
*          name           - name (can be NULL)
*          size           - size [bytes]
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_update(DatabaseHandle *databaseHandle,
                    int64          storageId,
                    String         name,
                    uint64         size
                   );

/***********************************************************************\
* Name   : Index_getState
* Purpose: get index state
* Input  : databaseHandle - database handle
*          storageId      - database id of index
* Output : indexState   - index state; see IndexStates
*          lastChecked  - last checked date/time stamp [s] (can be NULL)
*          errorMessage - error message (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_getState(DatabaseHandle *databaseHandle,
                      int64          storageId,
                      IndexStates    *indexState,
                      uint64         *lastChecked,
                      String         errorMessage
                     );

/***********************************************************************\
* Name   : Index_setState
* Purpose: set index state
* Input  : databaseHandle - database handle
*          storageId      - database id of index
*          indexState     - index state; see IndexStates
*          lastChecked    - last checked date/time stamp [s] (can be 0LL)
*          errorMessage   - error message (can be NULL)
*          ...            - optional arguments for error message
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_setState(DatabaseHandle *databaseHandle,
                      int64          storageId,
                      IndexStates    indexState,
                      uint64         lastChecked,
                      const char     *errorMessage,
                      ...
                     );

/***********************************************************************\
* Name   : Index_initListStorage
* Purpose: list storage entries
* Input  : databaseHandle - database handle
*          indexState     - index state
*          pattern        - name pattern (can be NULL)
* Output : databaseQueryHandle - database query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListStorage(DatabaseQueryHandle *databaseQueryHandle,
                             DatabaseHandle      *databaseHandle,
                             IndexStates         indexState,
                             String              pattern
                            );

/***********************************************************************\
* Name   : Index_getNextStorage
* Purpose: get next index storage entry
* Input  : databaseQueryHandle - database query handle
* Output : databaseId   - database id of entry
*          storageName  - storage name
*          dateTime     - date/time stamp [s]
*          size         - size [bytes]
*          indexState   - index state (can be NULL)
*          indexMode    - index mode (can be NULL)
*          lastChecked  - last checked date/time stamp [s] (can be NULL)
*          errorMessage - last error message
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextStorage(DatabaseQueryHandle *databaseQueryHandle,
                          DatabaseId          *databaseId,
                          String              storageName,
                          uint64              *dateTime,
                          uint64              *size,
                          IndexStates         *indexState,
                          IndexModes          *indexMode,
                          uint64              *lastChecked,
                          String              errorMessage
                         );

/***********************************************************************\
* Name   : Index_initListFiles
* Purpose: list file entries
* Input  : databaseHandle - database handle
*          pattern        - name pattern (can be NULL)
* Output : databaseQueryHandle - database query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListFiles(DatabaseQueryHandle *databaseQueryHandle,
                           DatabaseHandle      *databaseHandle,
                           String              pattern
                          );

/***********************************************************************\
* Name   : Index_getNextFile
* Purpose: get next file entry
* Input  : databaseQueryHandle - database query handle
* Output : databaseId     - database id of entry
*          storageName    - storage name
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

bool Index_getNextFile(DatabaseQueryHandle *databaseQueryHandle,
                       DatabaseId          *databaseId,
                       String              storageName,
                       uint64              *storageDateTime,
                       String              fileName,
                       uint64              *size,
                       uint64              *timeModified,
                       uint32              *userId,
                       uint32              *groupId,
                       uint32              *permission,
                       uint64              *fragmentOffset,
                       uint64              *fragmentSize
                      );

/***********************************************************************\
* Name   : Index_initListImages
* Purpose: list image entries
* Input  : databaseHandle - database handle
*          pattern        - name pattern (can be NULL)
* Output : databaseQueryHandle - database query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListImages(DatabaseQueryHandle *databaseQueryHandle,
                            DatabaseHandle      *databaseHandle,
                            String              pattern
                           );

/***********************************************************************\
* Name   : Index_getNextImage
* Purpose: get next image entry
* Input  : databaseQueryHandle - database query handle
* Output : databaseId   - database id of entry
*          storageName  - storage name
*          imageName    - image name
*          size         - size [bytes]
*          blockOffset  - block offset [blocks]
*          blockCount   - number of blocks
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Index_getNextImage(DatabaseQueryHandle *databaseQueryHandle,
                        DatabaseId          *databaseId,
                        String              storageName,
                        uint64              *storageDateTime,
                        String              imageName,
                        uint64              *size,
                        uint64              *blockOffset,
                        uint64              *blockCount
                       );

/***********************************************************************\
* Name   : Index_initListDirectories
* Purpose: list directory entries
* Input  : databaseHandle - database handle
*          pattern        - name pattern (can be NULL)
* Output : databaseQueryHandle - database query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListDirectories(DatabaseQueryHandle *databaseQueryHandle,
                                 DatabaseHandle      *databaseHandle,
                                 String              pattern
                                );

/***********************************************************************\
* Name   : Index_getNextDirectory
* Purpose: get next directory entry
* Input  : databaseQueryHandle - database query handle
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

bool Index_getNextDirectory(DatabaseQueryHandle *databaseQueryHandle,
                            DatabaseId          *databaseId,
                            String              storageName,
                            uint64              *storageDateTime,
                            String              directoryName,
                            uint64              *timeModified,
                            uint32              *userId,
                            uint32              *groupId,
                            uint32              *permission
                           );

/***********************************************************************\
* Name   : Index_initListLinks
* Purpose: list link entries
* Input  : databaseHandle - database handle
*          pattern        - name pattern (can be NULL)
* Output : databaseQueryHandle - database query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListLinks(DatabaseQueryHandle *databaseQueryHandle,
                           DatabaseHandle      *databaseHandle,
                           String              pattern
                          );

/***********************************************************************\
* Name   : Index_getNextLink
* Purpose: get next link entry
* Input  : databaseQueryHandle - database query handle
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

bool Index_getNextLink(DatabaseQueryHandle *databaseQueryHandle,
                       DatabaseId          *databaseId,
                       String              storageName,
                       uint64              *storageDateTime,
                       String              name,
                       String              destinationName,
                       uint64              *timeModified,
                       uint32              *userId,
                       uint32              *groupId,
                       uint32              *permission
                      );

/***********************************************************************\
* Name   : Index_initListHardLinks
* Purpose: list hard link entries
* Input  : databaseHandle - database handle
*          pattern        - name pattern (can be NULL)
* Output : databaseQueryHandle - database query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListHardLinks(DatabaseQueryHandle *databaseQueryHandle,
                               DatabaseHandle      *databaseHandle,
                               String              pattern
                               );

/***********************************************************************\
* Name   : Index_getNextHardLink
* Purpose: get next hard link entry
* Input  : databaseQueryHandle - database query handle
* Output : databaseId     - database id of entry
*          storageName    - storage name
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

bool Index_getNextHardLink(DatabaseQueryHandle *databaseQueryHandle,
                           DatabaseId          *databaseId,
                           String              storageName,
                           uint64              *storageDateTime,
                           String              fileName,
                           uint64              *size,
                           uint64              *timeModified,
                           uint32              *userId,
                           uint32              *groupId,
                           uint32              *permission,
                           uint64              *fragmentOffset,
                           uint64              *fragmentSize
                          );

/***********************************************************************\
* Name   : Index_initListSpecial
* Purpose: list special entries
* Input  : databaseHandle - database handle
*          pattern        - name pattern (can be NULL)
* Output : databaseQueryHandle - database query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_initListSpecial(DatabaseQueryHandle *databaseQueryHandle,
                             DatabaseHandle      *databaseHandle,
                             String              pattern
                            );

/***********************************************************************\
* Name   : Index_getNextSpecial
* Purpose: get next special entry
* Input  : databaseQueryHandle - database query handle
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

bool Index_getNextSpecial(DatabaseQueryHandle *databaseQueryHandle,
                          DatabaseId          *databaseId,
                          String              storageName,
                          uint64              *storageDateTime,
                          String              name,
                          uint64              *timeModified,
                          uint32              *userId,
                          uint32              *groupId,
                          uint32              *permission
                         );

/***********************************************************************\
* Name   : Index_doneList
* Purpose: done index list
* Input  : databaseQueryHandle - database query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Index_doneList(DatabaseQueryHandle *databaseQueryHandle);

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
                     int64          storageId,
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
*          size           - size [bytes]
*          blockSize      - block size [bytes]
*          blockOffset    - block offset [blocks]
*          blockCount     - number of blocks
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Index_addImage(DatabaseHandle *databaseHandle,
                      int64          storageId,
                      const String   imageName,
                      int64          size,
                      ulong          blockSize,
                      uint64         blockOffset,
                      uint64         blockCount
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
                          int64          storageId,
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
                     int64          storageId,
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
                         int64          storageId,
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
                        int64            storageId,
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
