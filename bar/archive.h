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

#include "common/global.h"
#include "common/strings.h"
#include "common/lists.h"
#include "common/files.h"
#include "common/devices.h"
#include "common/semaphores.h"
#include "common/passwords.h"

#include "errors.h"
#include "chunks.h"
#include "compress.h"
#include "crypt.h"
#include "deltasources.h"
#include "archive_format.h"
#include "storage.h"
#include "index/index.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define ARCHIVE_PART_NUMBER_NONE -1

// archive I/O types
typedef enum
{
  ARCHIVE_MODE_CREATE,
  ARCHIVE_MODE_READ
} ArchiveModes;

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

// archive flags
#define ARCHIVE_FLAG_NONE 0
#define ARCHIVE_FLAG_TRY_DELTA_COMPRESS   (1 <<  0)   // try delta compression (create only)
#define ARCHIVE_FLAG_TRY_BYTE_COMPRESS    (1 <<  1)   // try byte compression (create only)
#define ARCHIVE_FLAG_KEEP_DELTA_COMPRESS  (1 <<  2)   // keep delta compression (read only)
#define ARCHIVE_FLAG_KEEP_BYTE_COMPRESS   (1 <<  3)   // keep byte compression (read only)
#define ARCHIVE_FLAG_KEEP_ENCRYPTION      (1 <<  4)   // keep encryption (read only)
#define ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS  (1 <<  5)   // skip unknown chunks (read only)
#define ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS (1 <<  6)   // print unknown chunks (read only)

/***************************** Datatypes *******************************/

// archive flag set (see ARCHIVE_FLAG_*)
typedef uint ArchiveFlags;

// archive crypt info
typedef struct
{
  CryptTypes          cryptType;                                       // crypt type (symmetric/asymmetric; see CRYPT_TYPE_...)
  CryptMode           cryptMode;                                       // crypt mode; see CRYPT_MODE_....
  CryptKeyDeriveTypes cryptKeyDeriveType;                              // key derive type; see CRYPT_KEY_DERIVE_...
  CryptSalt           cryptSalt;                                       // crypt salt
  CryptKey            cryptKey;                                        // crypt key when asymmetric encrypted
} ArchiveCryptInfo;

// archive crypt info list
typedef struct ArchiveCryptInfoNode
{
  LIST_NODE_HEADER(struct ArchiveCryptInfoNode);

  ArchiveCryptInfo archiveCryptInfo;
} ArchiveCryptInfoNode;

typedef struct
{
  LIST_HEADER(ArchiveCryptInfoNode);
  Semaphore lock;
} ArchiveCryptInfoList;

/***********************************************************************\
* Name   : ArchiveInitFunction
* Purpose: call back before store archive file
* Input  : storageInfo  - storage info
*          uuidId       - UUID index id
*          jobUUID      - job UUID or NULL
*          entityUUID   - entity UUID or NULL
*          entityId     - entity index id
*          archiveType  - archive type
*          storageId    - storage index id
*          partNumber   - part number or ARCHIVE_PART_NUMBER_NONE for
*                         single part
*          userData     - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*ArchiveInitFunction)(StorageInfo  *storageInfo,
                                     IndexId      uuidId,
                                     ConstString  jobUUID,
                                     ConstString  entityUUID,
                                     IndexId      entityId,
                                     ArchiveTypes archiveType,
                                     IndexId      storageId,
                                     int          partNumber,
                                     void         *userData
                                    );

/***********************************************************************\
* Name   : ArchiveDoneFunction
* Purpose: call back after store archive file
* Input  : storageInfo  - storage info
*          uuidId       - UUID index id
*          jobUUID      - job UUID or NULL
*          entityUUID   - entity UUID or NULL
*          entityId     - entity index id
*          archiveType  - archive type
*          storageId    - storage index id
*          partNumber   - part number or ARCHIVE_PART_NUMBER_NONE for
*                         single part
*          userData     - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*ArchiveDoneFunction)(StorageInfo  *storageInfo,
                                     IndexId      uuidId,
                                     ConstString  jobUUID,
                                     ConstString  entityUUID,
                                     IndexId      entityId,
                                     ArchiveTypes archiveType,
                                     IndexId      storageId,
                                     int          partNumber,
                                     void         *userData
                                    );

/******************************************************************** ***\
* Name   : ArchiveGetSizeFunction
* Purpose: call back to get size of archive file
* Input  : storageInfo - storage info
*          storageId   - storage index id
*          partNumber  - part number or ARCHIVE_PART_NUMBER_NONE for
*                        single part
*          userData    - user data
* Output : -
* Return : archive size [bytes] or 0 if not exist
* Notes  : -
\***********************************************************************/

typedef uint64(*ArchiveGetSizeFunction)(StorageInfo *storageInfo,
                                        IndexId     storageId,
                                        int         partNumber,
                                        void        *userData
                                       );

/***********************************************************************\
* Name   : ArchiveTestFunction
* Purpose: call back for simple test archive
* Input  : storageInfo          - storage info
*          jobUUID              - job UUID or NULL
*          entityUUID           - entity UUID or NULL
*          partNumber           - part number or ARCHIVE_PART_NUMBER_NONE
*                                 for single part
*          intermediateFileName - intermediate archive file name
*          intermediateFileSize - intermediate archive size [bytes]
*          userData             - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*ArchiveTestFunction)(StorageInfo  *storageInfo,
                                     ConstString  jobUUID,
                                     ConstString  entityUUID,
                                     int          partNumber,
                                     ConstString  intermediateFileName,
                                     uint64       intermediateFileSize,
                                     void         *userData
                                    );

/***********************************************************************\
* Name   : ArchiveStoreFunction
* Purpose: call back to store archive
* Input  : storageInfo          - storage info
*          uuidId               - UUID index id
*          jobUUID              - job UUID or NULL
*          entityUUID           - entity UUID or NULL
*          entityId             - entity index id
*          storageId            - storage index id
*          partNumber           - part number or ARCHIVE_PART_NUMBER_NONE
*                                 for single part
*          intermediateFileName - intermediate archive file name
*          intermediateFileSize - intermediate archive size [bytes]
*          userData             - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*ArchiveStoreFunction)(StorageInfo  *storageInfo,
                                      IndexId      uuidId,
                                      ConstString  jobUUID,
                                      ConstString  entityUUID,
                                      IndexId      entityId,
                                      IndexId      storageId,
                                      int          partNumber,
                                      ConstString  intermediateFileName,
                                      uint64       intermediateFileSize,
                                      void         *userData
                                     );

// archive index cache list
struct ArchiveIndexNode;
typedef struct
{
  LIST_HEADER(struct ArchiveIndexNode);
  Semaphore lock;
} ArchiveIndexList;

// archive handle
typedef struct
{
  String                   hostName;                                   // host name or NULL
  String                   userName;                                   // user name or NULL
  StorageInfo              *storageInfo;
  IndexId                  uuidId;                                     // UUID index id
  IndexId                  entityId;                                   // entity index id

// TODO: use datatype UUID
  String                   jobUUID;
  String                   entityUUID;

  DeltaSourceList          *deltaSourceList;                           // list with delta sources
  ArchiveTypes             archiveType;
  bool                     dryRun;                                     // TRUE for dry-run only
  uint64                   createdDateTime;
  bool                     createMeta;                                 // TRUE to create meta chunks

  ArchiveInitFunction      archiveInitFunction;                        // call back to initialize archive file
  void                     *archiveInitUserData;                       // user data for call back to initialize archive file
  ArchiveDoneFunction      archiveDoneFunction;                        // call back to deinitialize archive file
  void                     *archiveDoneUserData;                       // user data for call back to deinitialize archive file
  ArchiveGetSizeFunction   archiveGetSizeFunction;                     // call back to get archive size
  void                     *archiveGetSizeUserData;                    // user data for call back to get archive size
  ArchiveTestFunction      archiveTestFunction;                        // call back to test archive file
  void                     *archiveTestUserData;                       // user data for test archive file
  ArchiveStoreFunction     archiveStoreFunction;                       // call back to store data info archive file
  void                     *archiveStoreUserData;                      // user data for call back to store data info archive file
  GetNamePasswordFunction  getNamePasswordFunction;                    // call back to get crypt password
  void                     *getNamePasswordUserData;                   // user data for call back to get crypt password
  LogHandle                *logHandle;                                 // log handle

  ArchiveCryptInfoList     archiveCryptInfoList;                       // crypt info list
  const ArchiveCryptInfo   *archiveCryptInfo;                          // current crypt info

  Semaphore                passwordLock;                               // input password lock
  Password                 *cryptPassword;                             // crypt password for encryption/decryption
  bool                     cryptPasswordReadFlag;                      // TRUE iff input callback for crypt password called
//  CryptKey                 cryptPublicKey;                             // public key for encryption/decryption of random key used for asymmetric encryptio
//TODO: use Key
  void                     *encryptedKeyData;                          // encrypted random key used for asymmetric encryption
  uint                     encryptedKeyDataLength;                     // length of encrypted random key

  void                     *signatureKeyData;                          // signature key
  uint                     signatureKeyDataLength;                     // length of signature key

  uint                     blockLength;                                // block length for crypt algorithm

// TODO: also in storageInfo->storageSpecifier->archiveName?
  ConstString              archiveName;                                // archive file name
  ConstString              printableStorageName;                       // printable storage name

  Semaphore                lock;
  ArchiveModes             mode;                                       // archive mode
  union
  {
    // create (local file)
    struct
    {
      String               tmpFileName;                                // temporary archive file name
      FileHandle           tmpFileHandle;                              // temporary file handle
      bool                 openFlag;                                   // TRUE iff temporary archive file is open
    } create;
    // read (local or remote storage)
    struct
    {
      String               storageFileName;                            // storage name
      StorageHandle        storageHandle;
    } read;
  };
  const ChunkIO            *chunkIO;                                   // chunk i/o functions
  void                     *chunkIOUserData;                           // chunk i/o functions data

  Semaphore                indexLock;
  IndexHandle              indexHandle;
  IndexId                  storageId;                                  // storage index id
  ArchiveIndexList         archiveIndexList;

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
      const FileExtendedAttributeList *fileExtendedAttributeList;      // extended attribute list

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
    struct
    {
      ChunkMeta                       chunkMeta;                       // base chunk
      ChunkMetaEntry                  chunkMetaEntry;                  // entry chunk
    } meta;
  };
} ArchiveEntryInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
  #define Archive_create(...)             __Archive_create            (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_open(...)               __Archive_open              (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_openHandle(...)         __Archive_openHandle        (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_close(...)              __Archive_close             (__FILE__,__LINE__, ## __VA_ARGS__)

  #define Archive_newMetaEntry(...)       __Archive_newMetaEntry      (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_newFileEntry(...)       __Archive_newFileEntry      (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_newImageEntry(...)      __Archive_newImageEntry     (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_newDirectoryEntry(...)  __Archive_newDirectoryEntry (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_newLinkEntry(...)       __Archive_newLinkEntry      (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_newHardLinkEntry(...)   __Archive_newHardLinkEntry  (__FILE__,__LINE__, ## __VA_ARGS__)
  #define Archive_newSpecialEntry(...)    __Archive_newSpecialEntry   (__FILE__,__LINE__, ## __VA_ARGS__)

  #define Archive_readMetaEntry(...)      __Archive_readMetaEntry     (__FILE__,__LINE__, ## __VA_ARGS__)
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
* Name   : Archive_parseType
* Purpose: parse archive type
* Input  : name     - name of archive type
*          userData - user data (not used)
* Output : archiveType - archive type
* Return : TRUE if parsed
* Notes  : -
\***********************************************************************/

bool Archive_parseType(const char *name, ArchiveTypes *archiveType, void *userData);

/***********************************************************************\
* Name   : Archive_archiveTypeToString
* Purpose: get name of archive type
* Input  : archiveType - archive type
* Output : -
* Return : archive type string
* Notes  : -
\***********************************************************************/

const char *Archive_archiveTypeToString(ArchiveTypes archiveType);

/***********************************************************************\
* Name   : Archive_archiveTypeToShortString
* Purpose: get short name of archive type
* Input  : archiveType - archive type
* Output : -
* Return : archive type short string
* Notes  : -
\***********************************************************************/

const char *Archive_archiveTypeToShortString(ArchiveTypes archiveType);

/***********************************************************************\
* Name   : Archive_archiveEntryTypeToString
* Purpose: get name of archive entry type
* Input  : archiveEntryType - archive entry type
* Output : -
* Return : archive entry type string
* Notes  : -
\***********************************************************************/

const char *Archive_archiveEntryTypeToString(ArchiveEntryTypes archiveEntryType);

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
* Name   : Archive_formatName
* Purpose: format archive name
* Input  : name               - name variable
*          templateFileName   - template file name
*          expandMacroMode    - expand macro mode; see
*                               EXPAND_MACRO_MODE_*
*          archiveType        - archive type; see ARCHIVE_TYPE_*
*          scheduleTitle      - schedule title or NULL
*          scheduleCustomText - schedule custom text or NULL
*          dateTime           - date/time
*          partNumber         - part number (>=0 for parts,
*                               ARCHIVE_PART_NUMBER_NONE for single
*                               part archive)
* Output : name - formated name
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_formatName(String           name,
                          ConstString      templateFileName,
                          ExpandMacroModes expandMacroMode,
                          ArchiveTypes     archiveType,
                          const char       *scheduleTitle,
                          const char       *scheduleCustomText,
                          uint64           dateTime,
                          int              partNumber
                         );

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
* Name   : Archive_clearDecryptKeys
* Purpose: clear decrypt keys (except default keys)
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Archive_clearDecryptKeys(void);

/***********************************************************************\
* Name   : Archive_appendDecryptKey
* Purpose: create key from password and append key to decrypt key list
* Input  : password - decrypt password to append
* Output : -
* Return : appended password
* Notes  : -
\***********************************************************************/

const CryptKey *Archive_appendDecryptKey(const Password *password);

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
* Input  : archiveHandle           - archive handle
*          hostName                - host name (can be NULL)
*          userName                - user name (can be NULL)
*          storageInfo             - storage info
*          archiveName             - archive name (can be NULL)
*          uuidId                  - UUID index id or INDEX_ID_NONE
*          entityId                - entity index id or INDEX_ID_NONE
*          jobUUID                 - job UUID or NULL
*          entityUUID              - entity UUID or NULL
*          deltaSourceList         - delta source list or NULL
*          archiveType             - archive type
*          createdDateTime         - date/time created [s]
*          cryptType               - crypt type; see CRYPT_TYPE_...
*          cryptAlgorithms         - crypt algorithm; see CRYPT_ALGORIHTM_...
*          cryptPassword           - crypt password
*          createMeta              - TRUE to create meta-chunks
*          storageFlags            - storage flags
*          archiveInitFunction     - call back to initialize archive
*                                    file
*          archiveInitUserData     - user data for call back
*          archiveDoneFunction     - call back to deinitialize archive
*                                    file
*          archiveDoneUserData     - user data for call back
*          archiveStoreFunction    - call back to store archive file
*          archiveStoreUserData    - user data for call back
*          getNamePasswordFunction - get password call back (can be
*                                    NULL)
*          getNamePasswordUserData - user data for get password call back
*          logHandle               - log handle (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_create(ArchiveHandle           *archiveHandle,
                        const char              *hostName,
                        const char              *userName,
                        StorageInfo             *storageInfo,
                        const char              *archiveName,
                        IndexId                 uuidId,
                        IndexId                 entityId,
                        const char              *jobUUID,
                        const char              *entityUUID,
                        DeltaSourceList         *deltaSourceList,
                        ArchiveTypes            archiveType,
                        bool                    dryRun,
                        uint64                  createdDateTime,
                        bool                    createMeta,
                        const Password          *cryptPassword,
                        ArchiveInitFunction     archiveInitFunction,
                        void                    *archiveInitUserData,
                        ArchiveDoneFunction     archiveDoneFunction,
                        void                    *archiveDoneUserData,
                        ArchiveGetSizeFunction  archiveGetSizeFunction,
                        void                    *archiveGetSizeUserData,
                        ArchiveTestFunction     archiveTestFunction,
                        void                    *archiveTestUserData,
                        ArchiveStoreFunction    archiveStoreFunction,
                        void                    *archiveStoreUserData,
                        GetNamePasswordFunction getNamePasswordFunction,
                        void                    *getNamePasswordUserData,
                        LogHandle               *logHandle
                       );
#else /* not NDEBUG */
  Errors __Archive_create(const char              *__fileName__,
                          ulong                   __lineNb__,
                          ArchiveHandle           *archiveHandle,
                          const char              *hostName,
                          const char              *userName,
                          StorageInfo             *storageInfo,
                          const char              *archiveName,
                          IndexId                 uuidId,
                          IndexId                 entityId,
                          const char              *jobUUID,
                          const char              *entityUUID,
                          DeltaSourceList         *deltaSourceList,
                          ArchiveTypes            archiveType,
                          bool                    dryRun,
                          uint64                  createdDateTime,
                          bool                    createMeta,
                          const Password          *cryptPassword,
                          ArchiveInitFunction     archiveInitFunction,
                          void                    *archiveInitUserData,
                          ArchiveDoneFunction     archiveDoneFunction,
                          void                    *archiveDoneUserData,
                          ArchiveGetSizeFunction  archiveGetSizeFunction,
                          void                    *archiveGetSizeUserData,
                          ArchiveTestFunction     archiveTestFunction,
                          void                    *archiveTestUserData,
                          ArchiveStoreFunction    archiveStoreFunction,
                          void                    *archiveStoreUserData,
                          GetNamePasswordFunction getNamePasswordFunction,
                          void                    *getNamePasswordUserData,
                          LogHandle               *logHandle
                         );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_open
* Purpose: open archive
* Input  : archiveHandle           - archive handle
*          storageInfo             - storage info
*          archiveName             - archive name (can be NULL)
*          jobOptions              - option settings
*          getNamePasswordFunction - get password call back (can be NULL)
*          getNamePasswordUserData - user data for get password call back
*          logHandle               - log handle (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_open(ArchiveHandle           *archiveHandle,
                      StorageInfo             *storageInfo,
                      ConstString             archiveName,
                      DeltaSourceList         *deltaSourceList,
                      GetNamePasswordFunction getNamePasswordFunction,
                      void                    *getNamePasswordUserData,
                      LogHandle               *logHandle
                     );
#else /* not NDEBUG */
  Errors __Archive_open(const char              *__fileName__,
                        ulong                   __lineNb__,
                        ArchiveHandle           *archiveHandle,
                        StorageInfo             *storageInfo,
                        ConstString             archiveName,
                        DeltaSourceList         *deltaSourceList,
                        GetNamePasswordFunction getNamePasswordFunction,
                        void                    *getNamePasswordUserData,
                        LogHandle               *logHandle
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_openHandle
* Purpose: open archive from handle
* Input  : archiveHandle     - archive handle
*          fromArchiveHandle - from archive handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_openHandle(ArchiveHandle       *archiveHandle,
                            const ArchiveHandle *fromArchiveHandle
                           );
#else /* not NDEBUG */
  Errors __Archive_openHandle(const char          *__fileName__,
                              ulong               __lineNb__,
                              ArchiveHandle       *archiveHandle,
                              const ArchiveHandle *fromArchiveHandle
                             );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_close
* Purpose: close archive
* Input  : archiveHandle - archive handle
*          storeFlag     - TRUE to store archive (if in create mode)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_close(ArchiveHandle *archiveHandle,
                       bool          storeFlag
                      );
#else /* not NDEBUG */
  Errors __Archive_close(const char    *__fileName__,
                         ulong         __lineNb__,
                         ArchiveHandle *archiveHandle,
                         bool          storeFlag
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_getCryptInfo
* Purpose: get crypt info
* Input  : archiveHandle    - archive handle
*          archiveCryptInfo - archive crypt info variable
* Output : archiveCryptInfo - archive crypt info
* Return : -
* Notes  : -
\***********************************************************************/

const ArchiveCryptInfo *Archive_getCryptInfo(const ArchiveHandle *archiveHandle);

/***********************************************************************\
* Name   : Archive_setCryptInfo
* Purpose: set crypt info
* Input  : archiveHandle - archive handle
*          archiveCryptInfo - archive crypt info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Archive_setCryptInfo(ArchiveHandle          *archiveHandle,
                                 const ArchiveCryptInfo *archiveCryptInfo
                                );
#if defined(NDEBUG) || defined(__ARCHIVE_IMPLEMENTATION__)
INLINE void Archive_setCryptInfo(ArchiveHandle          *archiveHandle,
                                 const ArchiveCryptInfo *archiveCryptInfo
                                )
{
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);
  assert(archiveCryptInfo != NULL);

  archiveHandle->archiveCryptInfo = archiveCryptInfo;
}
#endif /* defined(NDEBUG) || defined(__ARCHIVE_IMPLEMENTATION__) */

// TODO:
#if 0
/***********************************************************************\
* Name   : Archive_getCryptAlgorithms
* Purpose: get crypt algorithms
* Input  : archiveHandle  - archive handle
* Output : -
* Return : crypt algorithm; see CRYPT_ALGORITHM_...
* Notes  : -
\***********************************************************************/

INLINE CryptAlgorithms Archive_getCryptAlgorithm(const ArchiveHandle *archiveHandle);
#if defined(NDEBUG) || defined(__ARCHIVE_IMPLEMENTATION__)
INLINE CryptAlgorithms Archive_getCryptAlgorithm(const ArchiveHandle *archiveHandle)
{
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);

// TODO: multi-crypt
  return archiveHandle->cryptAlgorithms[0];
}
#endif /* defined(NDEBUG) || defined(__ARCHIVE_IMPLEMENTATION__) */

/***********************************************************************\
* Name   : Archive_setCryptAlgorithms
* Purpose: set crypt algorithms
* Input  : archiveHandle  - archive handle
*          cryptAlgorithm - crypt algorithm; see CRYPT_ALGORITHM_...
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void Archive_setCryptAlgorithm(ArchiveHandle         *archiveHandle,
                                      const CryptAlgorithms cryptAlgorithm
                                     );
#if defined(NDEBUG) || defined(__ARCHIVE_IMPLEMENTATION__)
INLINE void Archive_setCryptAlgorithm(ArchiveHandle         *archiveHandle,
                                      const CryptAlgorithms cryptAlgorithm
                                     )
{
  assert(archiveHandle != NULL);
  DEBUG_CHECK_RESOURCE_TRACE(archiveHandle);

// TODO: multi-crypt
  archiveHandle->cryptAlgorithms[0] = cryptAlgorithm;
  archiveHandle->cryptAlgorithms[1] = CRYPT_ALGORITHM_NONE;
  archiveHandle->cryptAlgorithms[2] = CRYPT_ALGORITHM_NONE;
  archiveHandle->cryptAlgorithms[3] = CRYPT_ALGORITHM_NONE;
}
#endif /* defined(NDEBUG) || defined(__ARCHIVE_IMPLEMENTATION__) */
#endif

#if 0
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
#endif

/***********************************************************************\
* Name   : Archive_eof
* Purpose: check if end-of-archive file
* Input  : archiveHandle - archive handle
*          archiveFlags  - flags; see ARCHIVE_FLAG_...
* Output : -
* Return : TRUE if end-of-archive, FALSE otherwise
* Notes  : Note: on error store error and return error value in next
*          operation
\***********************************************************************/

bool Archive_eof(ArchiveHandle *archiveHandle,
                 ArchiveFlags  flags
                );

/***********************************************************************\
* Name   : Archive_newMetaEntry
* Purpose: add new meta to archive
* Input  : archiveEntryInfo - archive file entry info variable
*          archiveHandle    - archive handle
*          userName         - user name (can be NULL)
*          hostName         - host name (can be NULL)
*          jobUUID          - job UUID (can be NULL)
*          entityUUID       - entity UUID (can be NULL)
*          archiveType      - archive type (can be NULL)
*          createdDateTime  - create date/time [s]
*          comment          - comment
* Output : archiveEntryInfo - archive file entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_newMetaEntry(ArchiveEntryInfo *archiveEntryInfo,
                              ArchiveHandle    *archiveHandle,
                              CryptAlgorithms  cryptAlgorithm,
                              const char       *userName,
                              const char       *hostName,
                              const char       *jobUUID,
                              const char       *entityUUID,
                              ArchiveTypes     archiveType,
                              uint64           createdDateTime,
                              const char       *comment
                             );
#else /* not NDEBUG */
  Errors __Archive_newMetaEntry(const char       *__fileName__,
                                ulong            __lineNb__,
                                ArchiveEntryInfo *archiveEntryInfo,
                                ArchiveHandle    *archiveHandle,
                                CryptAlgorithms  cryptAlgorithm,
                                const char       *userName,
                                const char       *hostName,
                                const char       *jobUUID,
                                const char       *entityUUID,
                                ArchiveTypes     archiveType,
                                uint64           createdDateTime,
                                const char       *comment
                               );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_newFileEntry
* Purpose: add new file to archive
* Input  : archiveEntryInfo          - archive file entry info variable
*          archiveHandle             - archive handle
*          deltaCompressAlgorithm    - used delta compression algorithm
*          byteCompressAlgorithm     - used byte compression algorithm
*          fileName                  - file name
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
*          fragmentOffset            - fragment offset [bytes]
*          fragmentSize              - fragment size [bytes]
*          archiveFlags              - flags; see ARCHIVE_FLAG_...
* Output : archiveEntryInfo - archive file entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_newFileEntry(ArchiveEntryInfo                *archiveEntryInfo,
                              ArchiveHandle                   *archiveHandle,
                              CompressAlgorithms              deltaCompressAlgorithm,
                              CompressAlgorithms              byteCompressAlgorithm,
                              CryptAlgorithms                 cryptAlgorithm,
//TOOD: use archiveHandle
                              ConstString                     fileName,
                              const FileInfo                  *fileInfo,
                              const FileExtendedAttributeList *fileExtendedAttributeList,
                              uint64                          fragmentOffset,
                              uint64                          fragmentSize,
                              ArchiveFlags                    archiveFlags
                             );
#else /* not NDEBUG */
  Errors __Archive_newFileEntry(const char                      *__fileName__,
                                ulong                           __lineNb__,
                                ArchiveEntryInfo                *archiveEntryInfo,
                                ArchiveHandle                   *archiveHandle,
                                CompressAlgorithms              deltaCompressAlgorithm,
                                CompressAlgorithms              byteCompressAlgorithm,
                                CryptAlgorithms                 cryptAlgorithm,
                                ConstString                     fileName,
                                const FileInfo                  *fileInfo,
                                const FileExtendedAttributeList *fileExtendedAttributeList,
                                uint64                          fragmentOffset,
                                uint64                          fragmentSize,
                                ArchiveFlags                    archiveFlags
                               );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_newImageEntry
* Purpose: add new block device image to archive
* Input  : archiveEntryInfo       - archive image entry info variable
*          archiveHandle          - archive handle
*          deltaCompressAlgorithm - used delta compression algorithm
*          byteCompressAlgorithm  - used byte compression algorithm
*          deviceName             - special device name
*          deviceInfo             - device info
*          blockOffset            - block offset (0..n-1)
*          blockCount             - number of blocks
*          archiveFlags           - flags; see ARCHIVE_FLAG_...
* Output : archiveEntryInfo - archive image entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_newImageEntry(ArchiveEntryInfo   *archiveEntryInfo,
                               ArchiveHandle      *archiveHandle,
                               CompressAlgorithms deltaCompressAlgorithm,
                               CompressAlgorithms byteCompressAlgorithm,
                               CryptAlgorithms    cryptAlgorithm,
                               ConstString        deviceName,
                               const DeviceInfo   *deviceInfo,
                               FileSystemTypes    fileSystemType,
                               uint64             blockOffset,
                               uint64             blockCount,
                               ArchiveFlags       archiveFlags
                              );
#else /* not NDEBUG */
  Errors __Archive_newImageEntry(const char         *__fileName__,
                                 ulong              __lineNb__,
                                 ArchiveEntryInfo   *archiveEntryInfo,
                                 ArchiveHandle      *archiveHandle,
                                 CompressAlgorithms deltaCompressAlgorithm,
                                 CompressAlgorithms byteCompressAlgorithm,
                                 CryptAlgorithms    cryptAlgorithm,
                                 ConstString        deviceName,
                                 const DeviceInfo   *deviceInfo,
                                 FileSystemTypes    fileSystemType,
                                 uint64             blockOffset,
                                 uint64             blockCount,
                                 ArchiveFlags       archiveFlags
                                );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_newDirectoryEntry
* Purpose: add new directory to archive
* Input  : archiveEntryInfo          - archive directory entry info
*                                      variable
*          archiveHandle             - archive handle
*          cryptType                 - used crypt type
*          cryptAlgorithm            - used crypt algorithm
*          cryptPassword             - used crypt password (can be NULL)
*          directoryName             - directory name
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
                                   CryptAlgorithms                 cryptAlgorithm,
                                   ConstString                     directoryName,
                                   const FileInfo                  *fileInfo,
                                   const FileExtendedAttributeList *fileExtendedAttributeList
                                  );
#else /* not NDEBUG */
  Errors __Archive_newDirectoryEntry(const char                      *__fileName__,
                                     ulong                           __lineNb__,
                                     ArchiveEntryInfo                *archiveEntryInfo,
                                     ArchiveHandle                   *archiveHandle,
                                     CryptAlgorithms                 cryptAlgorithm,
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
                              CryptAlgorithms                 cryptAlgorithm,
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
                                CryptAlgorithms                 cryptAlgorithm,
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
*          deltaCompressAlgorithm    - used delta compression algorithm
*          byteCompressAlgorithm     - used byte compression algorithm
*          fileNameList              - list of file names
*          fileInfo                  - file info
*          fileExtendedAttributeList - file extended attribute list or
*                                      NULL
*          fragmentOffset            - fragment offset [bytes]
*          fragmentSize              - fragment size [bytes]
*          archiveFlags              - flags; see ARCHIVE_FLAG_...
* Output : archiveEntryInfo - archive hard link entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_newHardLinkEntry(ArchiveEntryInfo                *archiveEntryInfo,
                                  ArchiveHandle                   *archiveHandle,
                                  CompressAlgorithms              deltaCompressAlgorithm,
                                  CompressAlgorithms              byteCompressAlgorithm,
                                  CryptAlgorithms                 cryptAlgorithm,
                                  const StringList                *fileNameList,
                                  const FileInfo                  *fileInfo,
                                  const FileExtendedAttributeList *fileExtendedAttributeList,
                                  uint64                          fragmentOffset,
                                  uint64                          fragmentSize,
                                  ArchiveFlags                    archiveFlags
                                 );
#else /* not NDEBUG */
  Errors __Archive_newHardLinkEntry(const char                      *__fileName__,
                                    ulong                           __lineNb__,
                                    ArchiveEntryInfo                *archiveEntryInfo,
                                    ArchiveHandle                   *archiveHandle,
                                    CompressAlgorithms              deltaCompressAlgorithm,
                                    CompressAlgorithms              byteCompressAlgorithm,
                                    CryptAlgorithms                 cryptAlgorithm,
                                    const StringList                *fileNameList,
                                    const FileInfo                  *fileInfo,
                                    const FileExtendedAttributeList *fileExtendedAttributeList,
                                    uint64                          fragmentOffset,
                                    uint64                          fragmentSize,
                                    ArchiveFlags                    archiveFlags
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
                                 CryptAlgorithms                 cryptAlgorithm,
                                 ConstString                     specialName,
                                 const FileInfo                  *fileInfo,
                                 const FileExtendedAttributeList *fileExtendedAttributeList
                                );
#else /* not NDEBUG */
  Errors __Archive_newSpecialEntry(const char                      *__fileName__,
                                   ulong                           __lineNb__,
                                   ArchiveEntryInfo                *archiveEntryInfo,
                                   ArchiveHandle                   *archiveHandle,
                                   CryptAlgorithms                 cryptAlgorithm,
                                   ConstString                     specialName,
                                   const FileInfo                  *fileInfo,
                                   const FileExtendedAttributeList *fileExtendedAttributeList
                                  );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_getNextArchiveEntry
* Purpose: get next entry in archive
* Input  : archiveHandle - archive handle
*          archiveFlags  - flags; see ARCHIVE_FLAG_...
* Output : archiveEntryType - archive entry type (can be NULL)
*          archiveCryptInfo - crypt info (can be NULL)
*          offset           - offset (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_getNextArchiveEntry(ArchiveHandle          *archiveHandle,
                                   ArchiveEntryTypes      *archiveEntryType,
                                   const ArchiveCryptInfo **archiveCryptInfo,
                                   uint64                 *offset,
                                   ArchiveFlags           flags
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
* Name   : Archive_readFileEntry
* Purpose: read file info from archive
* Input  : archiveEntryInfo - archive file entry info
*          archiveHandle    - archive handle
* Output : deltaCompressAlgorithm    - used delta compression algorithm
*                                      (can be NULL)
*          byteCompressAlgorithm     - used byte compression algorithm
*                                      (can be NULL)
*          cryptType                 - used crypt type
*          cryptAlgorithm            - used crypt algorithm (can be
*                                      NULL)
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
                               CryptTypes                *cryptType,
                               CryptAlgorithms           *cryptAlgorithm,
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
                                 CryptTypes                *cryptType,
                                 CryptAlgorithms           *cryptAlgorithm,
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
*          cryptType              - used crypt type
*          cryptAlgorithm         - used crypt algorithm (can be NULL)
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
                                CryptTypes         *cryptType,
                                CryptAlgorithms    *cryptAlgorithm,
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
                                  CryptTypes         *cryptType,
                                  CryptAlgorithms    *cryptAlgorithm,
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
* Output : cryptType                 - used crypt type
*          cryptAlgorithm            - used crypt algorithm (can be NULL)
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
                                    CryptTypes                *cryptType,
                                    CryptAlgorithms           *cryptAlgorithm,
                                    String                    directoryName,
                                    FileInfo                  *fileInfo,
                                    FileExtendedAttributeList *fileExtendedAttributeList
                                   );
#else /* not NDEBUG */
  Errors __Archive_readDirectoryEntry(const char                *__fileName__,
                                      ulong                     __lineNb__,
                                      ArchiveEntryInfo          *archiveEntryInfo,
                                      ArchiveHandle             *archiveHandle,
                                      CryptTypes                *cryptType,
                                      CryptAlgorithms           *cryptAlgorithm,
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
* Output : cryptType                 - used crypt type
*          cryptAlgorithm            - used crypt algorithm (can be NULL)
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
                               CryptTypes                *cryptType,
                               CryptAlgorithms           *cryptAlgorithm,
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
                                 CryptTypes                *cryptType,
                                 CryptAlgorithms           *cryptAlgorithm,
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
*          cryptType                 - used crypt type
*          cryptAlgorithm            - used crypt algorithm (can be NULL)
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
                                   CryptTypes                *cryptType,
                                   CryptAlgorithms           *cryptAlgorithm,
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
                                     CryptTypes                *cryptType,
                                     CryptAlgorithms           *cryptAlgorithm,
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
* Output : cryptType                 - used crypt type
*          cryptAlgorithm            - used crypt algorithm (can be NULL)
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
                                  CryptTypes                *cryptType,
                                  CryptAlgorithms           *cryptAlgorithm,
                                  String                    specialName,
                                  FileInfo                  *fileInfo,
                                  FileExtendedAttributeList *fileExtendedAttributeList
                                 );
#else /* not NDEBUG */
  Errors __Archive_readSpecialEntry(const char                *__fileName__,
                                    ulong                     __lineNb__,
                                    ArchiveEntryInfo          *archiveEntryInfo,
                                    ArchiveHandle             *archiveHandle,
                                    CryptTypes                *cryptType,
                                    CryptAlgorithms           *cryptAlgorithm,
                                    String                    specialName,
                                    FileInfo                  *fileInfo,
                                    FileExtendedAttributeList *fileExtendedAttributeList
                                   );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_readMetaEntry
* Purpose: read meta info from archive
* Input  : archiveEntryInfo - archive file entry info
*          archiveHandle    - archive handle
* Output : cryptType       - used crypt type
*          cryptAlgorithm  - used crypt algorithm (can be NULL)
*          hostName        - host name (can be NULL)
*          userName        - user name (can be NULL)
*          jobUUID         - job UUID (can be NULL)
*          entityUUID      - entity UUID (can be NULL)
*          archiveType     - archive type (can be NULL)
*          createdDateTime - create date/time [s]
*          comment         - comment
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Archive_readMetaEntry(ArchiveEntryInfo *archiveEntryInfo,
                               ArchiveHandle    *archiveHandle,
                               CryptTypes       *cryptType,
                               CryptAlgorithms  *cryptAlgorithm,
                               String           hostName,
                               String           userName,
                               String           jobUUID,
                               String           entityUUID,
                               ArchiveTypes     *archiveType,
                               uint64           *createdDateTime,
                               String           comment
                              );
#else /* not NDEBUG */
  Errors __Archive_readMetaEntry(const char       *__fileName__,
                                 ulong            __lineNb__,
                                 ArchiveEntryInfo *archiveEntryInfo,
                                 ArchiveHandle    *archiveHandle,
                                 CryptTypes       *cryptType,
                                 CryptAlgorithms  *cryptAlgorithm,
                                 String           hostName,
                                 String           userName,
                                 String           jobUUID,
                                 String           entityUUID,
                                 ArchiveTypes     *archiveType,
                                 uint64           *createdDateTime,
                                 String           comment
                                );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Archive_verifySignatureEntry
* Purpose: read signature entry and verify signature of archive
* Input  : archiveHandle - archive handle
*          offset        - data start offset
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
* Notes  : only available when writing an archive
\***********************************************************************/

INLINE uint64 Archive_getEntries(const ArchiveHandle *archiveHandle);
#if defined(NDEBUG) || defined(__ARCHIVE_IMPLEMENTATION__)
INLINE uint64 Archive_getEntries(const ArchiveHandle *archiveHandle)
{
  assert(archiveHandle != NULL);
  assert(archiveHandle->mode == ARCHIVE_MODE_CREATE);

  return archiveHandle->entries;
}
#endif /* NDEBUG || __ARCHIVE_IMPLEMENTATION__ */

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
* Input  : archiveHandle           - archive handle
*          allCryptSignaturesState - state of all signatures variable;
*                                    see CryptSignatureStates (can be
*                                    NULL)
* Output : allCryptSignaturesState - state of all signatures; see
*                                    CryptSignatureStates
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_verifySignatures(ArchiveHandle        *archiveHandle,
                                CryptSignatureStates *allCryptSignaturesState
                               );

/***********************************************************************\
* Name   : Archive_addToIndex
* Purpose: add storage index
* Input  : indexHandle - index handle
*          uuidId      - index id of UUID
*          entityId    - entity index id
*          hostName    - host name
*          storageInfo - storage info
*          indexMode   - index mode
*          logHandle   - log handle (can be NULL)
* Output : totalEntryCount - total number of entries (can be NULL)
*          totalSize       - total size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_addToIndex(IndexHandle *indexHandle,
                          IndexId     uuidId,
                          IndexId     entityId,
                          ConstString hostName,
                          StorageInfo *storageInfo,
                          IndexModes  indexMode,
                          ulong       *totalEntryCount,
                          uint64      *totalSize,
                          LogHandle   *logHandle
                         );

/***********************************************************************\
* Name   : Archive_updateIndex
* Purpose: read archive content and update storage index
* Input  : indexHandle       - index handle
*          entityId          - entity index id or INDEX_ID_NONE
*          storageId         - storage index id
*          storageInfo       - storage info
*          isPauseFunction   - is pause check callback (can be NULL)
*          isPauseUserData   - is pause check user data
*          isAbortedFunction - is aborted check callback (can be NULL)
*          isAbortedUserData - is aborted check user data
* Output : totalEntryCount - total number of entries (can be NULL)
*          totalSize       - total size [bytes] (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_updateIndex(IndexHandle       *indexHandle,
                           IndexId           uuidId,
                           IndexId           entityId,
                           IndexId           storageId,
                           StorageInfo       *storageInfo,
                           ulong             *totalEntryCount,
                           uint64            *totalSize,
                           IsPauseFunction   isPauseFunction,
                           void              *isPauseUserData,
                           IsAbortedFunction isAbortedFunction,
                           void              *isAbortedUserData,
                           LogHandle         *logHandle
                          );

/***********************************************************************\
* Name   : Archive_removeIndex
* Purpose: remove storage index
* Input  : indexHandle - index handle
*          storageId   - storage index id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_removeIndex(IndexHandle *indexHandle,
                           IndexId     storageId
                          );

#if 0
// NYI
Errors Archive_copy(ArchiveHandle           *archiveHandle,
                    ConstString             storageName,
                    const JobOptions        *jobOptions,
                    GetNamePasswordFunction archiveGetCryptPassword,
                    void                    *archiveGetCryptPasswordData,
                    ConstString             newStorageName
                   );
#endif /* 0 */

#ifdef __cplusplus
  }
#endif

#endif /* __ARCHIVE__ */

/* end of file */
