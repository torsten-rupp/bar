/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive functions
* Systems: all
*
\***********************************************************************/

#ifndef __ARCHIVE__
#define __ARCHIVE__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"
#include "files.h"
#include "devices.h"
#include "semaphores.h"

#include "errors.h"
#include "chunks.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"
#include "deltasources.h"
#include "archive_format.h"
#include "storage.h"
#include "index.h"
#include "bar_global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define ARCHIVE_PART_NUMBER_NONE -1

// archive I/O types
typedef enum
{
  ARCHIVE_IO_TYPE_FILE,
  ARCHIVE_IO_TYPE_STORAGE,
} ArchiveIOTypes;

/***************************** Datatypes *******************************/

// archive entry types
typedef enum
{
  ARCHIVE_ENTRY_TYPE_NONE,

  ARCHIVE_ENTRY_TYPE_FILE,
  ARCHIVE_ENTRY_TYPE_IMAGE,
  ARCHIVE_ENTRY_TYPE_DIRECTORY,
  ARCHIVE_ENTRY_TYPE_LINK,
  ARCHIVE_ENTRY_TYPE_HARDLINK,
  ARCHIVE_ENTRY_TYPE_SPECIAL,

  ARCHIVE_ENTRY_TYPE_META,
  ARCHIVE_ENTRY_TYPE_SIGNATURE,

  ARCHIVE_ENTRY_TYPE_UNKNOWN
} ArchiveEntryTypes;

/***********************************************************************\
* Name   : ArchiveInitFunction
* Purpose: call back before store archive file
* Input  : indexHandle  - index handle or NULL if no index
*          uuidId       - index UUID id
*          jobUUID      - job UUID or NULL
*          scheduleUUID - schedule UUID or NULL
*          entityId     - index entity id
*          archiveType  - archive type
*          storageId    - index id of storage
*          partNumber   - part number or ARCHIVE_PART_NUMBER_NONE for
*                         single part
*          userData     - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*ArchiveInitFunction)(IndexHandle  *indexHandle,
                                     IndexId      uuidId,
                                     ConstString  jobUUID,
                                     ConstString  scheduleUUID,
                                     IndexId      entityId,
                                     ArchiveTypes archiveType,
                                     IndexId      storageId,
                                     int          partNumber,
                                     void         *userData
                                    );

/***********************************************************************\
* Name   : ArchiveDoneFunction
* Purpose: call back after store archive file
* Input  : indexHandle  - index handle or NULL if no index
*          uuidId       - index UUID id
*          jobUUID      - job UUID or NULL
*          scheduleUUID - schedule UUID or NULL
*          entityId     - index entity id
*          archiveType  - archive type
*          storageId    - index id of storage
*          partNumber   - part number or ARCHIVE_PART_NUMBER_NONE for
*                         single part
*          userData     - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*ArchiveDoneFunction)(IndexHandle  *indexHandle,
                                     IndexId      uuidId,
                                     ConstString  jobUUID,
                                     ConstString  scheduleUUID,
                                     IndexId      entityId,
                                     ArchiveTypes archiveType,
                                     IndexId      storageId,
                                     int          partNumber,
                                     void         *userData
                                    );

/******************************************************************** ***\
* Name   : ArchiveGetSizeFunction
* Purpose: call back to get size of archive file
* Input  : indexHandle - index handle or NULL if no index
*          storageId   - index id of storage
*          partNumber  - part number or ARCHIVE_PART_NUMBER_NONE for
*                        single part
*          userData    - user data
* Output : -
* Return : archive size [bytes] or 0 if not exist
* Notes  : -
\***********************************************************************/

typedef uint64(*ArchiveGetSizeFunction)(IndexHandle *indexHandle,
                                        IndexId     storageId,
                                        int         partNumber,
                                        void        *userData
                                       );

/***********************************************************************\
* Name   : ArchiveStoreFunction
* Purpose: call back to store archive
* Input  : indexHandle          - index handle or NULL if no index
*          uuidId               - index UUID id
*          jobUUID              - job UUID or NULL
*          scheduleUUID         - schedule UUID or NULL
*          entityId             - index entity id
*          archiveType          - archive type
*          storageId            - index id of storage
*          partNumber           - part number or ARCHIVE_PART_NUMBER_NONE
*                                 for single part
*          intermediateFileName - intermediate archive file name
*          intermediateFileSize - intermediate archive size [bytes]
*          userData             - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*ArchiveStoreFunction)(IndexHandle  *indexHandle,
                                      IndexId      uuidId,
                                      ConstString  jobUUID,
                                      ConstString  scheduleUUID,
                                      IndexId      entityId,
                                      ArchiveTypes archiveType,
                                      IndexId      storageId,
                                      int          partNumber,
                                      ConstString  intermediateFileName,
                                      uint64       intermediateFileSize,
                                      void         *userData
                                     );

// archive handle
typedef struct
{
  String                   jobUUID;
  String                   scheduleUUID;
  DeltaSourceList          *deltaSourceList;                           // list with delta sources
  const JobOptions         *jobOptions;
  IndexHandle              *indexHandle;                               // index handle or NULL (owned by opener/creator of archive)
  IndexId                  uuidId;                                     // index UUID id
  IndexId                  entityId;                                   // index entity id
  ArchiveTypes             archiveType;
  ArchiveInitFunction      archiveInitFunction;                        // call back to initialize archive file
  void                     *archiveInitUserData;                       // user data for call back to initialize archive file
  ArchiveDoneFunction      archiveDoneFunction;                        // call back to deinitialize archive file
  void                     *archiveDoneUserData;                       // user data for call back to deinitialize archive file
  ArchiveGetSizeFunction   archiveGetSizeFunction;                     // call back to get archive size
  void                     *archiveGetSizeUserData;                    // user data for call back to get archive size
  ArchiveStoreFunction     archiveStoreFunction;                       // call back to store data info archive file
  void                     *archiveStoreUserData;                      // user data for call back to store data info archive file
  GetPasswordFunction      getPasswordFunction;                        // call back to get crypt password
  void                     *getPasswordUserData;                       // user data for call back to get crypt password
  LogHandle                *logHandle;                                 // log handle

  byte                     cryptSalt[CRYPT_SALT_LENGTH];               // crypt salt

  Semaphore                passwordLock;                               // input password lock
  CryptTypes               cryptType;                                  // crypt type (symmetric/asymmetric; see CryptTypes)
  Password                 *cryptPassword;                             // cryption password for encryption/decryption
  bool                     cryptPasswordReadFlag;                      // TRUE iff input callback for crypt password called
  CryptKey                 cryptKey;                                   // public/private key for encryption/decryption of random key used for asymmetric encryptio
//TODO: use Key
  void                     *cryptKeyData;                              // encrypted random key used for asymmetric encryption
  uint                     cryptKeyDataLength;                         // length of encrypted random key

  void                     *signatureKeyData;                          // signature random key
  uint                     signatureKeyDataLength;                     // length of signature random key

  uint                     blockLength;                                // block length for file entry/file data (depend on used crypt algorithm)

  ArchiveIOTypes           ioType;                                     // i/o type
  union
  {
    // local file         // delta compress info
      CompressInfo                    byteCompressInfo;
    struct
    {
      String               appendFileName;
      String               fileName;                                   // file name
      FileHandle           fileHandle;                                 // file handle
      bool                 openFlag;                                   // TRUE iff archive file is open
    } file;
    // local or remote storage
    struct
    {
      StorageSpecifier     storageSpecifier;                           // storage specifier structure
      String               storageFileName;                            // storage storage name
      StorageInfo          *storageInfo;                               // storage info
      StorageHandle        storageHandle;
    } storage;
  };
  String                   printableStorageName;                       // printable storage name
  Semaphore                chunkIOLock;                                // chunk i/o functions lock
  const ChunkIO            *chunkIO;                                   // chunk i/o functions
  void                     *chunkIOUserData;                           // chunk i/o functions data

  IndexId                  storageId;                                  // index id of storage

  uint64                   entries;                                    // number of entries
  uint64                   archiveFileSize;                            // size of current archive file part
  uint                     partNumber;                                 // current archive part number

  Errors                   pendingError;                               // pending error or ERROR_NONE
  bool                     nextChunkHeaderReadFlag;                    // TRUE iff next chunk header read
  ChunkHeader              nextChunkHeader;                            // next chunk header

  struct
  {
    bool                   openFlag;                                   // TRUE iff archive is open
    uint64                 offset;                                     // interrupt offset
  } interrupt;
} ArchiveHandle;

// archive entry info
typedef struct ArchiveEntryInfo
{
  LIST_NODE_HEADER(struct ArchiveEntryInfo);

  ArchiveHandle                       *archiveHandle;                  // archive handle
  IndexHandle                         *indexHandle;                    // index handle or NULL  (owned by entry opener/creator)

  enum
  {
    ARCHIVE_MODE_READ,
    ARCHIVE_MODE_WRITE,
  } mode;                                                              // read/write archive mode

  CryptAlgorithms                     cryptAlgorithms[4];              // crypt algorithms for entry
  uint                                blockLength;                     /* block length for file entry/file
                                                                          data (depend on used crypt
                                                                          algorithm)
                                                                       */

  ArchiveEntryTypes                   archiveEntryType;
  union
  {
    struct
    {
      const FileExtendedAttributeList *fileExtendedAttributeList;      // extended attribute list

      DeltaSourceHandle               deltaSourceHandle;               // delta source handle
      bool                            deltaSourceHandleInitFlag;       // TRUE if delta source is initialized

      CompressAlgorithms              deltaCompressAlgorithm;          // delta compression algorithm
      CompressAlgorithms              byteCompressAlgorithm;           // byte compression algorithm

      ChunkFile                       chunkFile;                       // base chunk
      ChunkFileEntry                  chunkFileEntry;                  // entry
      ChunkFileExtendedAttribute      chunkFileExtendedAttribute;      // extended attribute
      ChunkFileDelta                  chunkFileDelta;                  // delta
      ChunkFileData                   chunkFileData;                   // data

      CompressInfo                    deltaCompressInfo;               // delta compress info
      CompressInfo                    byteCompressInfo;                // byte compress info
      CryptInfo                       cryptInfo;                       // cryption info

      uint                            headerLength;                    // length of header
      bool                            headerWrittenFlag;               // TRUE iff header written

      FileHandle                      intermediateFileHandle;          // file handle for intermediate entry data
      byte                            *byteBuffer;                     // buffer for processing byte data
      ulong                           byteBufferSize;                  // size of byte buffer
      byte                            *deltaBuffer;                    // buffer for processing delta data
      ulong                           deltaBufferSize;                 // size of delta buffer
    } file;
    struct
    {
      const FileExtendedAttributeList *fileExtendedAttributeList;      // extended attribute list

      DeltaSourceHandle               deltaSourceHandle;               // delta source handle
      bool                            deltaSourceHandleInitFlag;       // TRUE if delta source is initialized

      uint                            blockSize;                       // block size of device

      CompressAlgorithms              deltaCompressAlgorithm;          // delta compression algorithm
      CompressAlgorithms              byteCompressAlgorithm;           // byte compression algorithm

      ChunkImage                      chunkImage;                      // base chunk
      ChunkImageEntry                 chunkImageEntry;                 // entry chunk
      ChunkImageDelta                 chunkImageDelta;                 // delta chunk
      ChunkImageData                  chunkImageData;                  // data chunk

      CompressInfo                    deltaCompressInfo;               // delta compress info
      CompressInfo                    byteCompressInfo;                // byte compress info
      CryptInfo                       cryptInfo;                       // cryption info

      uint                            headerLength;                    // length of header
      bool                            headerWrittenFlag;               // TRUE iff header written

      FileHandle                      intermediateFileHandle;          // file handle for intermediate entry data
      byte                            *byteBuffer;                     // buffer for processing byte data
      ulong                           byteBufferSize;                  // size of byte buffer
      byte                            *deltaBuffer;                    // buffer for processing delta data
      ulong                           deltaBufferSize;                 // size of delta buffer
    } image;
    struct
    {
      ChunkDirectory                  chunkDirectory;                  // base chunk
      ChunkDirectoryEntry             chunkDirectoryEntry;             // entry chunk
      ChunkDirectoryExtendedAttribute chunkDirectoryExtendedAttribute; // extended attribute chunk
    } directory;
    struct
    {
      const FileExtendedAttributeList *fileExtendedAttributeList;      // extended attribute list

      ChunkLink                       chunkLink;                       // base chunk
      ChunkLinkEntry                  chunkLinkEntry;                  // entry chunk
      ChunkLinkExtendedAttribute      chunkLinkExtendedAttribute;      // extended attribute chunk
    } link;
    struct
    {
      const StringList                *fileNameList;                   // list of hard link names
      const FileExtendedAttributeList *fileExtendedAttributeList;      // extended attribute list

      DeltaSourceHandle               deltaSourceHandle;               // delta source handle
      bool                            deltaSourceHandleInitFlag;       // TRUE if delta source is initialized

      CompressAlgorithms              deltaCompressAlgorithm;          // delta compression algorithm
      CompressAlgorithms              byteCompressAlgorithm;           // byte compression algorithm

      ChunkHardLink                   chunkHardLink;                   // base chunk
      ChunkHardLinkEntry              chunkHardLinkEntry;              // entry chunk
      ChunkHardLinkExtendedAttribute  chunkHardLinkExtendedAttribute;  // extended attribute chunk
      ChunkHardLinkName               chunkHardLinkName;               // name chunk
      ChunkHardLinkDelta              chunkHardLinkDelta;              // delta chunk
      ChunkHardLinkData               chunkHardLinkData;               // data chunk

      CompressInfo                    deltaCompressInfo;               // delta compress info
      CompressInfo                    byteCompressInfo;                // byte compress info
      CryptInfo                       cryptInfo;                       // cryption info

      uint                            headerLength;                    // length of header
      bool                            headerWrittenFlag;               // TRUE iff header written

      FileHandle                      intermediateFileHandle;          // file handle for intermediate entry data
      byte                            *byteBuffer;                     // buffer for processing byte data
      ulong                           byteBufferSize;                  // size of byte buffer
      byte                            *deltaBuffer;                    // buffer for processing delta data
      ulong                           deltaBufferSize;                 // size of delta buffer
    } hardLink;
    struct
    {
      const FileExtendedAttributeList *fileExtendedAttributeList;      // extended attribute list

      ChunkSpecial                    chunkSpecial;                    // base chunk
      ChunkSpecialEntry               chunkSpecialEntry;               // entry chunk
      ChunkHardLinkExtendedAttribute  chunkSpecialExtendedAttribute;   // extended attribute chunk
    } special;
  };
} ArchiveEntryInfo;

/***********************************************************************\
* Name   : ArchivePauseCallbackFunction
* Purpose: call back to check for pausing
* Input  : userData - user data
* Output : -
* Return : TRUE iff pause
* Notes  : -
\***********************************************************************/

typedef bool(*ArchivePauseCallbackFunction)(void *userData);

/***********************************************************************\
* Name   : ArchiveAbortCallbackFunction
* Purpose: call back to check for abort
* Input  : userData - user data
* Output : -
* Return : TRUE if aborted
* Notes  : -
\***********************************************************************/

typedef bool(*ArchiveAbortCallbackFunction)(void *userData);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Archive_create(...) __Archive_create(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_open(...)   __Archive_open  (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_close(...)  __Archive_close (__FILE__,__LINE__, ## __VA_ARGS__)

  #define Archive_newFileEntry(...)       __Archive_newFileEntry      (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_newImageEntry(...)      __Archive_newImageEntry     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_newDirectoryEntry(...)  __Archive_newDirectoryEntry (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_newLinkEntry(...)       __Archive_newLinkEntry      (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_newHardLinkEntry(...)   __Archive_newHardLinkEntry  (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_newSpecialEntry(...)    __Archive_newSpecialEntry   (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_readFileEntry(...)      __Archive_readFileEntry     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_readImageEntry(...)     __Archive_readImageEntry    (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_readDirectoryEntry(...) __Archive_readDirectoryEntry(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_readLinkEntry(...)      __Archive_readLinkEntry     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_readHardLinkEntry(...)  __Archive_readHardLinkEntry (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_readSpecialEntry(...)   __Archive_readSpecialEntry  (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_closeEntry(...)         __Archive_closeEntry        (__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Archive_initAll
* Purpose: init archive functions
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_initAll(void);

/***********************************************************************\
* Name   : Archive_doneAll
* Purpose: done archive functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Archive_doneAll(void);

/***********************************************************************\
* Name   : Archive_parseArchiveEntryType
* Purpose: get archive entry type
* Input  : name - name of archive entry type
* Output : archiveEntryType - archive entry type
* Return : TRUE iff parsed
* Notes  : -
\***********************************************************************/

bool Archive_parseArchiveEntryType(const char *name, ArchiveEntryTypes *archiveEntryType);

/***********************************************************************\
* Name   : Archive_isArchiveFile
* Purpose: check if archive file
* Input  : fileName - file name
* Output : -
* Return : TRUE if archive file, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Archive_isArchiveFile(ConstString fileName);

/***********************************************************************\
* Name   : Archive_clearCryptPasswords
* Purpose: clear decrypt passwords (except default passwords)
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Archive_clearDecryptPasswords(void);

/***********************************************************************\
* Name   : Archive_appendDecryptPassword
* Purpose: append password to decrypt password list
* Input  : password - decrypt password to append
* Output : -
* Return : appended password
* Notes  : -
\***********************************************************************/

const Password *Archive_appendDecryptPassword(const Password *password);

/***********************************************************************\
* Name   : Archive_waitDecryptPassword
* Purpose: wait for new decrypt password in list
* Input  : password - new decrypt password
*          timeout  - timeout [ms]
* Output : -
* Return : TRUE iff new password in list
* Notes  : -
\***********************************************************************/

bool Archive_waitDecryptPassword(Password *password, long timeout);

/***********************************************************************\
* Name   : Archive_create
* Purpose: create archive
* Input  : archiveHandle        - archive handle
*          entityId             - index UUID id or INDEX_ID_NONE
*          jobUUID              - unique job id or NULL
*          scheduleUUID         - unique schedule id or NULL
*          deltaSourceList      - delta source list or NULL
*          jobOptions           - job option settings
*          indexHandle          - index handle or NULL
*          entityId             - index entity id or INDEX_ID_NONE
*          archiveType          - archive type
*          archiveInitFunction  - call back to initialize archive file
*          archiveInitUserData  - user data for call back
*          archiveDoneFunction  - call back to deinitialize archive file
*          archiveDoneUserData  - user data for call back
*          archiveStoreFunction - call back to store archive file
*          archiveStoreUserData - user data for call back
*          getPasswordFunction  - get password call back (can be NULL)
*          getPasswordData      - user data for get password call back
*          logHandle            - log handle (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_create(ArchiveHandle          *archiveHandle,
                        IndexId                uuidId,
                        ConstString            jobUUID,
                        ConstString            scheduleUUID,
                        DeltaSourceList        *deltaSourceList,
                        const JobOptions       *jobOptions,
                        IndexHandle            *indexHandle,
                        IndexId                entityId,
                        ArchiveTypes           archiveType,
                        ArchiveInitFunction    archiveInitFunction,
                        void                   *archiveInitUserData,
                        ArchiveDoneFunction    archiveDoneFunction,
                        void                   *archiveDoneUserData,
                        ArchiveGetSizeFunction archiveGetSizeFunction,
                        void                   *archiveGetSizeUserData,
                        ArchiveStoreFunction   archiveStoreFunction,
                        void                   *archiveStoreUserData,
                        GetPasswordFunction    getPasswordFunction,
                        void                   *getPasswordUserData,
                        LogHandle              *logHandle
                       );
#else /* not NDEBUG */
  Errors __Archive_create(const char             *__fileName__,
                          ulong                  __lineNb__,
                          ArchiveHandle          *archiveHandle,
                          IndexId                uuidId,
                          ConstString            jobUUID,
                          ConstString            scheduleUUID,
                          DeltaSourceList        *deltaSourceList,
                          const JobOptions       *jobOptions,
                          IndexHandle            *indexHandle,
                          IndexId                entityId,
                          ArchiveTypes           archiveType,
                          ArchiveInitFunction    archiveInitFunction,
                          void                   *archiveInitUserData,
                          ArchiveDoneFunction    archiveDoneFunction,
                          void                   *archiveDoneUserData,
                          ArchiveGetSizeFunction archiveGetSizeFunction,
                          void                   *archiveGetSizeUserData,
                          ArchiveStoreFunction   archiveStoreFunction,
                          void                   *archiveStoreUserData,
                          GetPasswordFunction    getPasswordFunction,
                          void                   *getPasswordUserData,
                          LogHandle              *logHandle
                         );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_open
* Purpose: open archive
* Input  : archiveHandle       - archive handle
*          storageInfo         - storage info
*          fileName            - file name (can be NULL)
*          jobOptions          - option settings
*          getPasswordFunction - get password call back (can be NULL)
*          getPasswordUserData - user data for get password call back
*          logHandle           - log handle (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_open(ArchiveHandle          *archiveHandle,
                      StorageInfo            *storageInfo,
                      ConstString            fileName,
                      DeltaSourceList        *deltaSourceList,
                      const JobOptions       *jobOptions,
                      GetPasswordFunction    getPasswordFunction,
                      void                   *getPasswordUserData,
                      LogHandle              *logHandle
                     );
#else /* not NDEBUG */
  Errors __Archive_open(const char             *__fileName__,
                        ulong                  __lineNb__,
                        ArchiveHandle          *archiveHandle,
                        StorageInfo            *storageInfo,
                        ConstString            fileName,
                        DeltaSourceList        *deltaSourceList,
                        const JobOptions       *jobOptions,
                        GetPasswordFunction    getPasswordFunction,
                        void                   *getPasswordUserData,
                        LogHandle              *logHandle
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_close
* Purpose: close archive
* Input  : archiveHandle - archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_close(ArchiveHandle *archiveHandle);
#else /* not NDEBUG */
  Errors __Archive_close(const char    *__fileName__,
                         ulong         __lineNb__,
                         ArchiveHandle *archiveHandle
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_storageInterrupt
* Purpose: interrupt create archive and close storage
* Input  : archiveHandle - archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_storageInterrupt(ArchiveHandle *archiveHandle);

/***********************************************************************\
* Name   : Archive_storageContinue
* Purpose: continue interrupted archive and reopen storage
* Input  : archiveHandle - archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_storageContinue(ArchiveHandle *archiveHandle);

/***********************************************************************\
* Name   : Archive_eof
* Purpose: check if end-of-archive file
* Input  : archiveHandle          - archive handle
*          skipUnknownChunksFlag  - TRUE to skip unknown chunks
*          printUnknownChunksFlag - TRUE to print unknown chunks
* Output : -
* Return : TRUE if end-of-archive, FALSE otherwise
* Notes  : Note: on error store error and return error value in next
*          operation
\***********************************************************************/

bool Archive_eof(ArchiveHandle *archiveHandle,
                 bool          skipUnknownChunksFlag,
                 bool          printUnknownChunksFlag
                );

/***********************************************************************\
* Name   : Archive_newFileEntry
* Purpose: add new file to archive
* Input  : archiveEntryInfo          - archive file entry info variable
*          archiveHandle             - archive handle
*          fileName                  - file name
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
*          fragmentOffset            - fragment offset [bytes]
*          fragmentSize              - fragment size [bytes]
*          deltaCompressFlag         - TRUE for delta compression, FALSE
*                                      otherwise
*          byteCompressFlag          - TRUE for byte compression, FALSE
*                                      otherwise (e. g. file is to small
*                                      or already compressed)
* Output : archiveEntryInfo - archive file entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_newFileEntry(ArchiveEntryInfo                *archiveEntryInfo,
                              ArchiveHandle                   *archiveHandle,
                              IndexHandle                     *indexHandle,
                              ConstString                     fileName,
                              const FileInfo                  *fileInfo,
                              const FileExtendedAttributeList *fileExtendedAttributeList,
                              uint64                          fragmentOffset,
                              uint64                          fragmentSize,
                              const bool                      deltaCompressFlag,
                              const bool                      byteCompressFlag
                             );
#else /* not NDEBUG */
  Errors __Archive_newFileEntry(const char                      *__fileName__,
                                ulong                           __lineNb__,
                                ArchiveEntryInfo                *archiveEntryInfo,
                                ArchiveHandle                   *archiveHandle,
                                IndexHandle                     *indexHandle,
                                ConstString                     fileName,
                                const FileInfo                  *fileInfo,
                                const FileExtendedAttributeList *fileExtendedAttributeList,
                                uint64                          fragmentOffset,
                                uint64                          fragmentSize,
                                const bool                      deltaCompressFlag,
                                const bool                      byteCompressFlag
                               );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_newImageEntry
* Purpose: add new block device image to archive
* Input  : archiveEntryInfo  - archive image entry info variable
*          archiveHandle     - archive handle
*          deviceName        - special device name
*          deviceInfo        - device info
*          fragmentOffset    - fragment offset [blocks]
*          fragmentSize      - fragment size [blocks]
*          deltaCompressFlag - TRUE for delta compression, FALSE
*                              otherwise
*          byteCompressFlag  - TRUE for byte compression, FALSE
*                              otherwise (e. g. file is to small
*                              or already compressed)
* Output : archiveEntryInfo - archive image entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_newImageEntry(ArchiveEntryInfo *archiveEntryInfo,
                               ArchiveHandle    *archiveHandle,
                               IndexHandle      *indexHandle,
                               ConstString      deviceName,
                               const DeviceInfo *deviceInfo,
                               FileSystemTypes  fileSystemType,
                               uint64           fragmentOffset,
                               uint64           fragmentSize,
                               const bool       deltaCompressFlag,
                               const bool       byteCompressFlag
                              );
#else /* not NDEBUG */
  Errors __Archive_newImageEntry(const char       *__fileName__,
                                 ulong            __lineNb__,
                                 ArchiveEntryInfo *archiveEntryInfo,
                                 ArchiveHandle    *archiveHandle,
                                 IndexHandle      *indexHandle,
                                 ConstString      deviceName,
                                 const DeviceInfo *deviceInfo,
                                 FileSystemTypes  fileSystemType,
                                 uint64           fragmentOffset,
                                 uint64           fragmentSize,
                                 const bool       deltaCompressFlag,
                                 const bool       byteCompressFlag
                                );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_newDirectoryEntry
* Purpose: add new directory to archive
* Input  : archiveEntryInfo          - archive directory entry info
*                                      variable
*          archiveHandle             - archive handle
*          name                      - directory name
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
* Output : archiveEntryInfo - archive directory entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_newDirectoryEntry(ArchiveEntryInfo                *archiveEntryInfo,
                                   ArchiveHandle                   *archiveHandle,
                                   IndexHandle                     *indexHandle,
                                   ConstString                     directoryName,
                                   const FileInfo                  *fileInfo,
                                   const FileExtendedAttributeList *fileExtendedAttributeList
                                  );
#else /* not NDEBUG */
  Errors __Archive_newDirectoryEntry(const char                      *__fileName__,
                                     ulong                           __lineNb__,
                                     ArchiveEntryInfo                *archiveEntryInfo,
                                     ArchiveHandle                   *archiveHandle,
                                     IndexHandle                     *indexHandle,
                                     ConstString                     directoryName,
                                     const FileInfo                  *fileInfo,
                                     const FileExtendedAttributeList *fileExtendedAttributeList
                                    );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_newLinkEntry
* Purpose: add new link to archive
* Input  : archiveEntryInfo          - archive link entry variable
*          archiveHandle             - archive handle
*          fileName                  - link name
*          destinationName           - name of referenced file
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
* Output : archiveEntryInfo - archive link entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_newLinkEntry(ArchiveEntryInfo                *archiveEntryInfo,
                              ArchiveHandle                   *archiveHandle,
                              IndexHandle                     *indexHandle,
                              ConstString                     linkName,
                              ConstString                     destinationName,
                              const FileInfo                  *fileInfo,
                              const FileExtendedAttributeList *fileExtendedAttributeList
                             );
#else /* not NDEBUG */
  Errors __Archive_newLinkEntry(const char                      *__fileName__,
                                ulong                           __lineNb__,
                                ArchiveEntryInfo                *archiveEntryInfo,
                                ArchiveHandle                   *archiveHandle,
                                IndexHandle                     *indexHandle,
                                ConstString                     linkName,
                                ConstString                     destinationName,
                                const FileInfo                  *fileInfo,
                                const FileExtendedAttributeList *fileExtendedAttributeList
                               );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_newHardLinkEntry
* Purpose: add new hard link to archive
* Input  : archiveEntryInfo          - archive hardlink entry info
*                                      variable
*          archiveHandle             - archive handle
*          fileNameList              - list of file names
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
*          fragmentOffset            - fragment offset [bytes]
*          fragmentSize              - fragment size [bytes]
*          deltaCompressFlag         - TRUE for delta compression, FALSE
*                                      otherwise
*          byteCompressFlag          - TRUE for byte compression, FALSE
*                                      otherwise (e. g. file is to small
*                                      or already compressed)
* Output : archiveEntryInfo - archive hard link entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_newHardLinkEntry(ArchiveEntryInfo                *archiveEntryInfo,
                                  ArchiveHandle                   *archiveHandle,
                                  IndexHandle                     *indexHandle,
                                  const StringList                *fileNameList,
                                  const FileInfo                  *fileInfo,
                                  const FileExtendedAttributeList *fileExtendedAttributeList,
                                  uint64                          fragmentOffset,
                                  uint64                          fragmentSize,
                                  const bool                      deltaCompressFlag,
                                  const bool                      byteCompressFlag
                                 );
#else /* not NDEBUG */
  Errors __Archive_newHardLinkEntry(const char                      *__fileName__,
                                    ulong                           __lineNb__,
                                    ArchiveEntryInfo                *archiveEntryInfo,
                                    ArchiveHandle                   *archiveHandle,
                                    IndexHandle                     *indexHandle,
                                    const StringList                *fileNameList,
                                    const FileInfo                  *fileInfo,
                                    const FileExtendedAttributeList *fileExtendedAttributeList,
                                    uint64                          fragmentOffset,
                                    uint64                          fragmentSize,
                                    const bool                      deltaCompressFlag,
                                    const bool                      byteCompressFlag
                                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_newSpecialEntry
* Purpose: add new special entry to archive
* Input  : archiveEntryInfo          - archive special entry info
*                                      variable
*          archiveHandle             - archive handle
*          specialName               - special name
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
* Output : archiveEntryInfo - archive special entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_newSpecialEntry(ArchiveEntryInfo                *archiveEntryInfo,
                                 ArchiveHandle                   *archiveHandle,
                                 IndexHandle                     *indexHandle,
                                 ConstString                     specialName,
                                 const FileInfo                  *fileInfo,
                                 const FileExtendedAttributeList *fileExtendedAttributeList
                                );
#else /* not NDEBUG */
  Errors __Archive_newSpecialEntry(const char                      *__fileName__,
                                   ulong                           __lineNb__,
                                   ArchiveEntryInfo                *archiveEntryInfo,
                                   ArchiveHandle                   *archiveHandle,
                                   IndexHandle                     *indexHandle,
                                   ConstString                     specialName,
                                   const FileInfo                  *fileInfo,
                                   const FileExtendedAttributeList *fileExtendedAttributeList
                                  );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_getNextArchiveEntry
* Purpose: get next entry in archive
* Input  : archiveHandle          - archive handle
*          skipUnknownChunksFlag  - TRUE to skip unknown chunks
*          printUnknownChunksFlag - TRUE to print unknown chunks
* Output : archiveEntryType - archive entry type (can be NULL)
*          offset           - offset (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_getNextArchiveEntry(ArchiveHandle     *archiveHandle,
                                   ArchiveEntryTypes *archiveEntryType,
                                   uint64            *offset,
                                   bool              skipUnknownChunksFlag,
                                   bool              printUnknownChunksFlag
                                  );

/***********************************************************************\
* Name   : Archive_skipNextEntry
* Purpose: skip next entry in archive
* Input  : archiveHandle - archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_skipNextEntry(ArchiveHandle *archiveHandle);

/***********************************************************************\
* Name   : Archive_readMetaEntry
* Purpose: read meta info from archive
* Input  : archiveHandle - archive handle
* Output : userName        - user name (can be NULL)
*          hostName        - host name (can be NULL)
*          jobUUID         - job UUID (can be NULL)
*          scheduleUUID    - schedule UUID (can be NULL)
*          archiveType     - archive type (can be NULL)
*          createdDateTime - create date/time [s]
*          comment         - comment
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_readMetaEntry(ArchiveHandle *archiveHandle,
                             String        userName,
                             String        hostName,
                             String        jobUUID,
                             String        scheduleUUID,
                             ArchiveTypes  *archiveType,
                             uint64        *createdDateTime,
                             String        comment
                            );

/***********************************************************************\
* Name   : Archive_readFileEntry
* Purpose: read file info from archive
* Input  : archiveEntryInfo - archive file entry info
*          archiveHandle    - archive handle
* Output : deltaCompressAlgorithm    - used delta compression algorithm
*                                      (can be NULL)
*          byteCompressAlgorithm     - used byte compression algorithm
*                                      (can be NULL)
*          cryptAlgorithm            - used crypt algorithm (can be
*                                      NULL)
*          cryptType                 - used crypt type (can be NULL)
*          fileName                  - file name
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
*          deltaSourceName           - delta source name (can be NULL)
*          deltaSourceSize           - delta source size [bytes] (can be
*                                      NULL)
*          fragmentOffset            - fragment offset (can be NULL)
*          fragmentSize              - fragment size in bytes (can be
*                                      NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_readFileEntry(ArchiveEntryInfo          *archiveEntryInfo,
                               ArchiveHandle             *archiveHandle,
                               CompressAlgorithms        *deltaCompressAlgorithm,
                               CompressAlgorithms        *byteCompressAlgorithm,
                               CryptAlgorithms           *cryptAlgorithm,
                               CryptTypes                *cryptType,
                               String                    fileName,
                               FileInfo                  *fileInfo,
                               FileExtendedAttributeList *fileExtendedAttributeList,
                               String                    deltaSourceName,
                               uint64                    *deltaSourceSize,
                               uint64                    *fragmentOffset,
                               uint64                    *fragmentSize
                              );
#else /* not NDEBUG */
  Errors __Archive_readFileEntry(const char                *__fileName__,
                                 ulong                     __lineNb__,
                                 ArchiveEntryInfo          *archiveEntryInfo,
                                 ArchiveHandle             *archiveHandle,
                                 CompressAlgorithms        *deltaCompressAlgorithm,
                                 CompressAlgorithms        *byteCompressAlgorithm,
                                 CryptAlgorithms           *cryptAlgorithm,
                                 CryptTypes                *cryptType,
                                 String                    fileName,
                                 FileInfo                  *fileInfo,
                                 FileExtendedAttributeList *fileExtendedAttributeList,
                                 String                    deltaSourceName,
                                 uint64                    *deltaSourceSize,
                                 uint64                    *fragmentOffset,
                                 uint64                    *fragmentSize
                                );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_readImageEntry
* Purpose: read block device image info from archive
* Input  : archiveEntryInfo - archive image entry info
*          archiveHandle    - archive handle
* Output : deltaCompressAlgorithm - used delta compression algorithm
*                                   (can be NULL)
*          byteCompressAlgorithm  - used byte compression algorithm (can
*                                   be NULL)
*          cryptAlgorithm         - used crypt algorithm (can be NULL)
*          cryptType              - used crypt type (can be NULL)
*          deviceName             - image name
*          deviceInfo             - device info (can be NULL)
*          fileSystemType         - file system type (can be NULL)
*          deltaSourceName        - delta source name (can be NULL)
*          deltaSourceSize        - delta source size [bytes] (can be
*                                   NULL)
*          blockOffset            - block offset (0..n-1)
*          blockCount             - number of blocks
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_readImageEntry(ArchiveEntryInfo   *archiveEntryInfo,
                                ArchiveHandle      *archiveHandle,
                                CompressAlgorithms *deltaCompressAlgorithm,
                                CompressAlgorithms *byteCompressAlgorithm,
                                CryptAlgorithms    *cryptAlgorithm,
                                CryptTypes         *cryptType,
                                String             deviceName,
                                DeviceInfo         *deviceInfo,
                                FileSystemTypes    *fileSystemType,
                                String             deltaSourceName,
                                uint64             *deltaSourceSize,
                                uint64             *blockOffset,
                                uint64             *blockCount
                               );
#else /* not NDEBUG */
  Errors __Archive_readImageEntry(const char         *__fileName__,
                                  ulong              __lineNb__,
                                  ArchiveEntryInfo   *archiveEntryInfo,
                                  ArchiveHandle      *archiveHandle,
                                  CompressAlgorithms *deltaCompressAlgorithm,
                                  CompressAlgorithms *byteCompressAlgorithm,
                                  CryptAlgorithms    *cryptAlgorithm,
                                  CryptTypes         *cryptType,
                                  String             deviceName,
                                  DeviceInfo         *deviceInfo,
                                  FileSystemTypes    *fileSystemType,
                                  String             deltaSourceName,
                                  uint64             *deltaSourceSize,
                                  uint64             *blockOffset,
                                  uint64             *blockCount
                                 );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_readDirectoryEntry
* Purpose: read directory info from archive
* Input  : archiveEntryInfo - archive directory info
*          archiveHandle    - archive handle
* Output : cryptAlgorithm            - used crypt algorithm (can be NULL)
*          cryptType                 - used crypt type (can be NULL)
*          directoryName             - directory name
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_readDirectoryEntry(ArchiveEntryInfo          *archiveEntryInfo,
                                    ArchiveHandle             *archiveHandle,
                                    CryptAlgorithms           *cryptAlgorithm,
                                    CryptTypes                *cryptType,
                                    String                    directoryName,
                                    FileInfo                  *fileInfo,
                                    FileExtendedAttributeList *fileExtendedAttributeList
                                   );
#else /* not NDEBUG */
  Errors __Archive_readDirectoryEntry(const char                *__fileName__,
                                      ulong                     __lineNb__,
                                      ArchiveEntryInfo          *archiveEntryInfo,
                                      ArchiveHandle             *archiveHandle,
                                      CryptAlgorithms           *cryptAlgorithm,
                                      CryptTypes                *cryptType,
                                      String                    directoryName,
                                      FileInfo                  *fileInfo,
                                      FileExtendedAttributeList *fileExtendedAttributeList
                                     );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_readLinkEntry
* Purpose: read link info from archive
* Input  : archiveEntryInfo - archive link entry info
*          archiveHandle    - archive handle
* Output : cryptAlgorithm            - used crypt algorithm (can be NULL)
*          cryptType                 - used crypt type (can be NULL)
*          linkName                  - link name
*          destinationName           - name of referenced file
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_readLinkEntry(ArchiveEntryInfo          *archiveEntryInfo,
                               ArchiveHandle             *archiveHandle,
                               CryptAlgorithms           *cryptAlgorithm,
                               CryptTypes                *cryptType,
                               String                    linkName,
                               String                    destinationName,
                               FileInfo                  *fileInfo,
                               FileExtendedAttributeList *fileExtendedAttributeList
                              );
#else /* not NDEBUG */
  Errors __Archive_readLinkEntry(const char                *__fileName__,
                                 ulong                     __lineNb__,
                                 ArchiveEntryInfo          *archiveEntryInfo,
                                 ArchiveHandle             *archiveHandle,
                                 CryptAlgorithms           *cryptAlgorithm,
                                 CryptTypes                *cryptType,
                                 String                    linkName,
                                 String                    destinationName,
                                 FileInfo                  *fileInfo,
                                 FileExtendedAttributeList *fileExtendedAttributeList
                                );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_readHardLinkEntry
* Purpose: read hard link info from archive
* Input  : archiveEntryInfo - archive hard link entry info
*          archiveHandle    - archive handle
* Output : deltaCompressAlgorithm    - used delta compression algorithm
*                                      (can be NULL)
*          byteCompressAlgorithm     - used byte compression algorithm
*                                      (can be NULL)
*          cryptAlgorithm            - used crypt algorithm (can be NULL)
*          cryptType                 - used crypt type (can be NULL)
*          fileNameList              - list of file names
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
*          deltaSourceName           - delta source name (can be NULL)
*          deltaSourceSize           - delta source size [bytes] (can
*                                      be NULL)
*          fragmentOffset            - fragment offset (can be NULL)
*          fragmentSize              - fragment size in bytes (can be
*                                      NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_readHardLinkEntry(ArchiveEntryInfo          *archiveEntryInfo,
                                   ArchiveHandle             *archiveHandle,
                                   CompressAlgorithms        *deltaCompressAlgorithm,
                                   CompressAlgorithms        *byteCompressAlgorithm,
                                   CryptAlgorithms           *cryptAlgorithm,
                                   CryptTypes                *cryptType,
                                   StringList                *fileNameList,
                                   FileInfo                  *fileInfo,
                                   FileExtendedAttributeList *fileExtendedAttributeList,
                                   String                    deltaSourceName,
                                   uint64                    *deltaSourceSize,
                                   uint64                    *fragmentOffset,
                                   uint64                    *fragmentSize
                                  );
#else /* not NDEBUG */
  Errors __Archive_readHardLinkEntry(const char                *__fileName__,
                                     ulong                     __lineNb__,
                                     ArchiveEntryInfo          *archiveEntryInfo,
                                     ArchiveHandle             *archiveHandle,
                                     CompressAlgorithms        *deltaCompressAlgorithm,
                                     CompressAlgorithms        *byteCompressAlgorithm,
                                     CryptAlgorithms           *cryptAlgorithm,
                                     CryptTypes                *cryptType,
                                     StringList                *fileNameList,
                                     FileInfo                  *fileInfo,
                                     FileExtendedAttributeList *fileExtendedAttributeList,
                                     String                    deltaSourceName,
                                     uint64                    *deltaSourceSize,
                                     uint64                    *fragmentOffset,
                                     uint64                    *fragmentSize
                                    );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_readSpecialEntry
* Purpose: read special device info from archive
* Input  : archiveEntryInfo - archive special entry info
*          archiveHandle    - archive handle
* Output : cryptAlgorithm            - used crypt algorithm (can be NULL)
*          cryptType                 - used crypt type (can be NULL)
*          name                      - link name
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_readSpecialEntry(ArchiveEntryInfo          *archiveEntryInfo,
                                  ArchiveHandle             *archiveHandle,
                                  CryptAlgorithms           *cryptAlgorithm,
                                  CryptTypes                *cryptType,
                                  String                    specialName,
                                  FileInfo                  *fileInfo,
                                  FileExtendedAttributeList *fileExtendedAttributeList
                                 );
#else /* not NDEBUG */
  Errors __Archive_readSpecialEntry(const char                *__fileName__,
                                    ulong                     __lineNb__,
                                    ArchiveEntryInfo          *archiveEntryInfo,
                                    ArchiveHandle             *archiveHandle,
                                    CryptAlgorithms           *cryptAlgorithm,
                                    CryptTypes                *cryptType,
                                    String                    specialName,
                                    FileInfo                  *fileInfo,
                                    FileExtendedAttributeList *fileExtendedAttributeList
                                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_verifySignatureEntry
* Purpose: verify signatures of archive
* Input  : archiveHandle - archive handle
*          offset        - offset
*          cryptSignatureState - signature state; see
*                                CryptSignatureStates (can be NULL)
* Output : cryptSignatureState - signature state; see
*                                CryptSignatureStates
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_verifySignatureEntry(ArchiveHandle        *archiveHandle,
                                    uint64               offset,
                                    CryptSignatureStates *cryptSignatureState
                                   );

/***********************************************************************\
* Name   : Archive_closeEntry
* Purpose: clsoe file in archive
* Input  : archiveEntryInfo - archive entry info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_closeEntry(ArchiveEntryInfo *archiveEntryInfo);
#else /* not NDEBUG */
  Errors __Archive_closeEntry(const char       *__fileName__,
                              ulong            __lineNb__,
                              ArchiveEntryInfo *archiveEntryInfo
                             );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_writeData
* Purpose: write data to archive
* Input  : archiveEntryInfo - archive entry info
*          buffer           - data buffer
*          length           - length of data buffer (bytes)
*          elementSize      - size of single (not splitted) element to
*                             write to archive part (1..n)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : It is assured that a data parts of size elementSize are
*          not splitted
\***********************************************************************/

Errors Archive_writeData(ArchiveEntryInfo *archiveEntryInfo,
                         const void       *buffer,
                         ulong            length,
                         uint             elementSize
                        );

/***********************************************************************\
* Name   : Archive_readData
* Purpose: read data from archive
* Input  : archiveEntryInfo - archive entry info
*          buffer           - data buffer
*          length           - length of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_readData(ArchiveEntryInfo *archiveEntryInfo,
                        void             *buffer,
                        ulong            length
                       );

/***********************************************************************\
* Name   : Archive_readData
* Purpose: read data from archive
* Input  : archiveEntryInfo - archive entry info
*          buffer           - data buffer
*          length           - length of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

/***********************************************************************\
* Name   : Archive_eofData
* Purpose: check if end-of-archive data
* Input  : archiveEntryInfo - archive entry info
* Output : -
* Return : TRUE if end-of-archive data, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Archive_eofData(ArchiveEntryInfo *archiveEntryInfo);

/***********************************************************************\
* Name   : Archive_tell
* Purpose: get current read/write position in archive file
* Input  : archiveHandle - archive handle
* Output : -
* Return : current position in archive [bytes]
* Notes  : -
\***********************************************************************/

uint64 Archive_tell(ArchiveHandle *archiveHandle);

/***********************************************************************\
* Name   : Archive_seek
* Purpose: seek to position in archive file
* Input  : archiveHandle - archive handle
*          offset        - offset in archive
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_seek(ArchiveHandle *archiveHandle,
                    uint64        offset
                   );

/***********************************************************************\
* Name   : Archive_getEntries
* Purpose: get number of entries stored into archive file
* Input  : archiveHandle - archive handle
* Output : -
* Return : number of entries
* Notes  : -
\***********************************************************************/

INLINE uint64 Archive_getEntries(const ArchiveHandle *archiveHandle);
#if defined(NDEBUG) || defined(__ARCHIVE_IMPLEMENATION__)
INLINE uint64 Archive_getEntries(const ArchiveHandle *archiveHandle)
{
  assert(archiveHandle != NULL);

  return archiveHandle->entries;
}
#endif /* NDEBUG || __ARCHIVE_IMPLEMENATION__ */

/***********************************************************************\
* Name   : Archive_getSize
* Purpose: get size of archive file
* Input  : archiveHandle - archive handle
* Output : -
* Return : size of archive [bytes]
* Notes  : -
\***********************************************************************/

uint64 Archive_getSize(ArchiveHandle *archiveHandle);

/***********************************************************************\
* Name   : Archive_verifySignatures
* Purpose: verify signatures of archive
* Input  : storageInfo             - storage info
*          fileName                - file name (can be NULL)
*          jobOptions              - option settings
*          logHandle               - log handle (can be NULL)
*          allCryptSignaturesState - state of all signatures; see
*                                    CryptSignatureStates (can be NULL)
* Output : allCryptSignaturesState - state of all signatures; see
*                                    CryptSignatureStates
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_verifySignatures(StorageInfo          *storageInfo,
                                ConstString          fileName,
                                const JobOptions     *jobOptions,
                                LogHandle            *logHandle,
                                CryptSignatureStates *allCryptSignaturesState
                               );

/***********************************************************************\
* Name   : Archive_addToIndex
* Purpose: add storage index
* Input  : indexHandle - index handle
*          storageInfo - storage info
*          indexMode   - index mode
*          jobOptions  - job options
*          logHandle   - log handle (can be NULL)
* Output : totalTimeLastChanged - total last change time [s] (can be
*                                 NULL)
*          totalEntries         - total entries (can be NULL)
*          totalSize            - total size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_addToIndex(IndexHandle      *indexHandle,
                          StorageInfo      *storageInfo,
                          IndexModes       indexMode,
                          const JobOptions *jobOptions,
                          uint64           *totalTimeLastChanged,
                          uint64           *totalEntries,
                          uint64           *totalSize,
                          LogHandle        *logHandle
                         );

/***********************************************************************\
* Name   : Archive_updateIndex
* Purpose: update storage index
* Input  : indexHandle           - index handle
*          storageId             - index id of storage
*          storageInfo           - storage info
*          jobOptions            - job options
*          pauseCallbackFunction - pause check callback (can be NULL)
*          pauseCallbackUserData - pause user data
*          abortCallbackFunction - abort check callback (can be NULL)
*          abortCallbackUserData - abort user data
* Output : totalTimeLastChanged - total last change time [s] (can be
*                                 NULL)
*          totalEntries         - total entries (can be NULL)
*          totalSize            - total size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_updateIndex(IndexHandle                  *indexHandle,
                           IndexId                      storageId,
                           StorageInfo                  *storageInfo,
                           const JobOptions             *jobOptions,
                           uint64                       *totalTimeLastChanged,
                           uint64                       *totalEntries,
                           uint64                       *totalSize,
                           ArchivePauseCallbackFunction pauseCallbackFunction,
                           void                         *pauseCallbackUserData,
                           ArchiveAbortCallbackFunction abortCallbackFunction,
                           void                         *abortCallbackUserData,
                           LogHandle                    *logHandle
                          );

/***********************************************************************\
* Name   : Archive_remIndex
* Purpose: remove storage index
* Input  : indexHandle - index handle
*          storageId   - index id of storage
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_remIndex(IndexHandle *indexHandle,
                        IndexId     storageId
                       );

#if 0
// NYI
Errors Archive_copy(ArchiveHandle       *archiveHandle,
                    ConstString         storageName,
                    const JobOptions    *jobOptions,
                    GetPasswordFunction archiveGetCryptPassword,
                    void                *archiveGetCryptPasswordData,
                    ConstString         newStorageName
                   );
#endif /* 0 */

#ifdef __cplusplus
  }
#endif

#endif /* __ARCHIVE__ */

/* end of file */
