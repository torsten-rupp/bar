/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/archive.h,v $
* $Revision: 1.10 $
* $Author: torsten $
* Contents: Backup ARchiver archive functions
* Systems: all
*
\***********************************************************************/

#ifndef __ARCHIVE__
#define __ARCHIVE__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "errors.h"
#include "chunks.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"
#include "archive_format.h"
#include "files.h"
#include "devices.h"
#include "storage.h"
#include "index.h"

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
#define ARCHIVE_PART_NUMBER_NONE -1

typedef enum
{
  ARCHIVE_IO_TYPE_FILE,
  ARCHIVE_IO_TYPE_STORAGE_FILE,
} ArchiveIOTypes;

/***************************** Datatypes *******************************/

/* archive entry types */
typedef enum
{
  ARCHIVE_ENTRY_TYPE_NONE,

  ARCHIVE_ENTRY_TYPE_FILE,
  ARCHIVE_ENTRY_TYPE_IMAGE,
  ARCHIVE_ENTRY_TYPE_DIRECTORY,
  ARCHIVE_ENTRY_TYPE_LINK,
  ARCHIVE_ENTRY_TYPE_SPECIAL,

  ARCHIVE_ENTRY_TYPE_UNKNOWN
} ArchiveEntryTypes;

#if 0
/* archive entry node */
typedef struct ArchiveEntryNode
{
  LIST_NODE_HEADER(struct ArchiveEntryNode);

  ArchiveEntryTypes type;
  union
  {
    struct
    {
      String             fileName;
      FileInfo           fileInfo;
      uint64             size;
      uint64             timeModified;
      uint64             archiveSize;
      CompressAlgorithms compressAlgorithm;
      CryptAlgorithms    cryptAlgorithm;
      CryptTypes         cryptType;
      uint64             fragmentOffset;
      uint64             fragmentSize;
    } file;
    struct
    {
      String             imageName;
      uint64             size;
      uint64             archiveSize;
      CompressAlgorithms compressAlgorithm;
      CryptAlgorithms    cryptAlgorithm;
      CryptTypes         cryptType;
      uint               blockSize;
      uint64             blockOffset;
      uint64             blockCount;
    } image;
    struct
    {
      String          directoryName;
      FileInfo           fileInfo;
      CryptAlgorithms cryptAlgorithm;
      CryptTypes      cryptType;
    } directory;
    struct
    {
      String          linkName;
      String          destinationName;
      FileInfo           fileInfo;
      CryptAlgorithms cryptAlgorithm;
      CryptTypes      cryptType;
    } link;
    struct
    {
      String           fileName;
      FileInfo           fileInfo;
      CryptAlgorithms  cryptAlgorithm;
      CryptTypes       cryptType;
      FileSpecialTypes fileSpecialType;
      ulong            major;
      ulong            minor;
    } special;
  };
} ArchiveEntryNode;

/* archive entry list */
typedef struct
{
  LIST_HEADER(ArchiveEntryNode);
} ArchiveEntryList;
#endif /* 0 */

/***********************************************************************\
* Name   : ArchiveNewEntryFunction
* Purpose: call back when new archive entry is created/written
* Input  : userData     - user data
*          archiveEntry - archive entry
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

/*
typedef Errors(*ArchiveNewEntryFunction)(void               *userData,
                                         const ArchiveEntry *archiveEntry
                                        );
*/

/***********************************************************************\
* Name   : ArchiveNewFileFunction
* Purpose: call back when archive file is created/written
* Input  : userData       - user data
*          databaseHandle - database handle or NULL if no database
*          storageId      - database id of storage
*          fileName       - archive file name
*          partNumber     - part number or -1 if no parts
*          lastPartFlag   - TRUE iff last archive part, FALSE otherwise
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*ArchiveNewFileFunction)(void           *userData,
                                        DatabaseHandle *databaseHandle,
                                        int64          storageId,
                                        String         fileName,
                                        int            partNumber,
                                        bool           lastPartFlag
                                       );

/***********************************************************************\
* Name   : ArchiveGetCryptPasswordFunction
* Purpose: call back to get crypt password for archive file
* Input  : userData      - user data
*          password      - crypt password variable
*          fileName      - file name
*          validateFlag  - TRUE to validate input, FALSE otherwise
*          weakCheckFlag - TRUE for weak password checking, FALSE
*                          otherwise (print warning if password seems to
*                          be a weak password)
* Output : password - crypt password
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*ArchiveGetCryptPasswordFunction)(void         *userData,
                                                 Password     *password,
                                                 const String fileName,
                                                 bool         validateFlag,
                                                 bool         weakCheckFlag
                                                );


typedef struct
{
//  ArchiveNewEntryFunction         archiveNewEntryFunction;           // call back for new archive entry
//  void                            *archiveNewEntryUserData;          // user data for call back for new archive entry
  ArchiveNewFileFunction          archiveNewFileFunction;            // call back for new archive file
  void                            *archiveNewFileUserData;           // user data for call back for new archive file
  ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction;   // call back to get crypt password
  void                            *archiveGetCryptPasswordUserData;  // user data for call back to get crypt password
  JobOptions                      *jobOptions;

  CryptTypes                      cryptType;                         // crypt type (symmetric/asymmetric; see CryptTypes)
  Password                        *cryptPassword;                    // cryption password for encryption/decryption
  CryptKey                        cryptKey;                          // public/private key for encryption/decryption of random key used for asymmetric encryption
  void                            *cryptKeyData;                     // encrypted random key used for asymmetric encryption
  uint                            cryptKeyDataLength;                // length of encrypted random key

  uint                            blockLength;                       // block length for file entry/file data (depend on used crypt algorithm)

  String                          fileName;                          // file name
  ArchiveIOTypes                  ioType;                            // i/o type
  union
  {
    struct
    {
      FileHandle                  fileHandle;                        // file handle
      bool                        openFlag;                          // TRUE iff archive file is open
    } file;
    struct
    {
      StorageFileHandle           storageFileHandle;                 // storage file handle
    } storageFile;
  };

  DatabaseHandle                  *databaseHandle;
  int64                           storageId;

  const ChunkIO                   *chunkIO;                          // chunk i/o functions
  void                            *chunkIOUserData;                  // chunk i/o functions data

  uint                            partNumber;                        // file part number

  bool                            nextChunkHeaderReadFlag;           // TRUE iff next chunk header read
  ChunkHeader                     nextChunkHeader;                   // next chunk header
} ArchiveInfo;

typedef struct
{
  ArchiveInfo        *archiveInfo;

  /* read/write archive mode */
  enum
  {
    ARCHIVE_MODE_READ,
    ARCHIVE_MODE_WRITE,
  } mode;

  CryptAlgorithms    cryptAlgorithm;                 // crypt algorithm for entry
  uint               blockLength;                    /* block length for file entry/file
                                                        data (depend on used crypt
                                                        algorithm)
                                                     */

  ArchiveEntryTypes  archiveEntryType;
  union
  {
    struct
    {
      CompressAlgorithms  compressAlgorithm;         // compression algorithm for file entry

      ChunkInfo           chunkInfoFile;             // chunk info block for file
      ChunkFile           chunkFile;                 // file

      ChunkInfo           chunkInfoFileEntry;        // chunk info block for file entry
      ChunkFileEntry      chunkFileEntry;            // file entry
      CryptInfo           cryptInfoFileEntry;        // file entry cryption info (without data elements)

      ChunkInfo           chunkInfoFileData;         // chunk info block for file data
      ChunkFileData       chunkFileData;             // file data
      CryptInfo           cryptInfoFileData;         // file data cryption info (without data elements)

      CompressInfo        compressInfoData;          // data compress info
      CryptInfo           cryptInfoData;             // data cryption info

      bool                createdFlag;               // TRUE iff file created
      uint                headerLength;              // length of header
      bool                headerWrittenFlag;         // TRUE iff header written

      byte                *buffer;
      ulong               bufferLength;
    } file;
    struct
    {
      uint                blockSize;                 // block size of device
      CompressAlgorithms  compressAlgorithm;         // compression algorithm for image entry

      ChunkInfo           chunkInfoImage;            // chunk info block for image
      ChunkImage          chunkImage;                // image

      ChunkInfo           chunkInfoImageEntry;       // chunk info block for file entry
      ChunkImageEntry     chunkImageEntry;           // image entry
      CryptInfo           cryptInfoImageEntry;       // image entry cryption info (without data elements)

      ChunkInfo           chunkInfoImageData;        // chunk info block for image data
      ChunkImageData      chunkImageData;            // image data
      CryptInfo           cryptInfoImageData;        // image data cryption info (without data elements)

      CompressInfo        compressInfoData;          // data compress info
      CryptInfo           cryptInfoData;             // data cryption info

      bool                createdFlag;               // TRUE iff file created
      uint                headerLength;              // length of header
      bool                headerWrittenFlag;         // TRUE iff header written

      byte                *buffer;
      ulong               bufferLength;
    } image;
    struct
    {
      ChunkInfo           chunkInfoDirectory;        // chunk info block for directory
      ChunkDirectory      chunkDirectory;            // directory

      ChunkInfo           chunkInfoDirectoryEntry;   // chunk info block for directory entry
      ChunkDirectoryEntry chunkDirectoryEntry;       // directory entry
      CryptInfo           cryptInfoDirectoryEntry;   // directory entry cryption info
    } directory;
    struct
    {
      ChunkInfo           chunkInfoLink;             // chunk info block for link
      ChunkLink           chunkLink;                 // link

      ChunkInfo           chunkInfoLinkEntry;        // chunk info block for link entry
      ChunkLinkEntry      chunkLinkEntry;            // link entry
      CryptInfo           cryptInfoLinkEntry;        // link entry cryption info
    } link;
    struct
    {
      ChunkInfo           chunkInfoSpecial;          // chunk info block for special
      ChunkSpecial        chunkSpecial;              // special

      ChunkInfo           chunkInfoSpecialEntry;     // chunk info block for special entry
      ChunkSpecialEntry   chunkSpecialEntry;         // special entry
      CryptInfo           cryptInfoSpecialEntry;     // special entry cryption info
    } special;
  };
} ArchiveFileInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

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
* Input  : password - password
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Archive_appendDecryptPassword(const Password *password);

/***********************************************************************\
* Name   : Archive_freeArchiveEntryNode
* Purpose: free archive entry node
* Input  : archiveEntryNode - archive entry node to free
*          userData         - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

//void Archive_freeArchiveEntryNode(ArchiveEntryNode *archiveEntryNode, void *userData);

/***********************************************************************\
* Name   : Archive_copyArchiveEntryNode
* Purpose: copy archive entry node
* Input  : archiveEntryNode - archive entry node to copy
*          userData         - user data (not used)
* Output : -
* Return : copy of archive entry node
* Notes  : -
\***********************************************************************/

//ArchiveEntryNode *Archive_copyArchiveEntryNode(ArchiveEntryNode *archiveEntryNode, void *userData);

/***********************************************************************\
* Name   : Archive_create
* Purpose: create archive
* Input  : archiveInfo                 - archive info block
*          jobOptions                  - job option settings
*          archiveNewFileFunction      - call back for creating new
*                                        archive file
*          archiveNewFileUserData      - user data for call back
*          archiveGetCryptPassword     - get password call back
*          archiveGetCryptPasswordData - user data for get password call
*                                        back
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_create(ArchiveInfo                     *archiveInfo,
                      JobOptions                      *jobOptions,
//                      ArchiveNewEntryFunction         archiveNewEntryFunction,
//                      void                            *archiveNewEntryUserData,
                      ArchiveNewFileFunction          archiveNewFileFunction,
                      void                            *archiveNewFileUserData,
                      ArchiveGetCryptPasswordFunction archiveGetCryptPassword,
                      void                            *archiveGetCryptPasswordData,
                      DatabaseHandle                  *databaseHandle
                     );

/***********************************************************************\
* Name   : Archive_open
* Purpose: open archive
* Input  : archiveInfo                 - archive info block
*          storageName                 - storage name
*          jobOptions                  - option settings
*          archiveGetCryptPassword     - get password call back
*          archiveGetCryptPasswordData - user data for get password call
*                                        back
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_open(ArchiveInfo                     *archiveInfo,
                    const String                    storageName,
                    JobOptions                      *jobOptions,
                    ArchiveGetCryptPasswordFunction archiveGetCryptPassword,
                    void                            *archiveGetCryptPasswordData
                   );

/***********************************************************************\
* Name   : Archive_close
* Purpose: close archive
* Input  : archiveInfo - archive info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_close(ArchiveInfo *archiveInfo);

/***********************************************************************\
* Name   : Archive_eof
* Purpose: check if end-of-archive file
* Input  : archiveInfo - archive info block
* Output : -
* Return : TRUE if end-of-archive, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Archive_eof(ArchiveInfo *archiveInfo);

/***********************************************************************\
* Name   : Archive_newFile
* Purpose: add new file to archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
*          fileName        - file name
*          fileInfo        - file info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_newFileEntry(ArchiveInfo     *archiveInfo,
                            ArchiveFileInfo *archiveFileInfo,
                            const String    fileName,
                            const FileInfo  *fileInfo
                           );

/***********************************************************************\
* Name   : Archive_newImageEntry
* Purpose: add new block device image to archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
*          name            - special device name
*          fileInfo        - file info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_newImageEntry(ArchiveInfo     *archiveInfo,
                             ArchiveFileInfo *archiveFileInfo,
                             const String    deviceName,
                             DeviceInfo      *deviceInfo
                            );

/***********************************************************************\
* Name   : Archive_newDirectoryEntry
* Purpose: add new directory to archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
*          name            - directory name
*          fileInfo        - file info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_newDirectoryEntry(ArchiveInfo     *archiveInfo,
                                 ArchiveFileInfo *archiveFileInfo,
                                 const String    directoryName,
                                 FileInfo        *fileInfo
                                );

/***********************************************************************\
* Name   : Archive_newLinkEntry
* Purpose: add new link to archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
*          fileName        - link name
*          destinationName - name of referenced file
*          fileInfo        - file info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_newLinkEntry(ArchiveInfo     *archiveInfo,
                            ArchiveFileInfo *archiveFileInfo,
                            const String    linkName,
                            const String    destinationName,
                            FileInfo        *fileInfo
                           );

/***********************************************************************\
* Name   : Archive_newSpecialEntry
* Purpose: add new special entry to archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
*          name            - special device name
*          fileInfo        - file info
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_newSpecialEntry(ArchiveInfo     *archiveInfo,
                               ArchiveFileInfo *archiveFileInfo,
                               const String    specialName,
                               FileInfo        *fileInfo
                              );

/***********************************************************************\
* Name   : Archive_getNextArchiveEntryType
* Purpose: get type of next entry in archive
* Input  : archiveInfo     - archive info block
* Output : archiveEntryType - archive entry type
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_getNextArchiveEntryType(ArchiveInfo       *archiveInfo,
                                       ArchiveEntryTypes *archiveEntryType
                                      );

/***********************************************************************\
* Name   : Archive_readFileEntry
* Purpose: read file info from archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
* Output : compressAlgorithm - used compression algorithm (can be NULL)
*          fileName          - file name
*          fileInfo          - file info
*          cryptAlgorithm    - used crypt algorithm (can be NULL)
*          cryptType         - used crypt type (can be NULL)
*          fragmentOffset    - fragment offset (can be NULL)
*          fragmentSize      - fragment size in bytes (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_readFileEntry(ArchiveInfo        *archiveInfo,
                             ArchiveFileInfo    *archiveFileInfo,
                             CompressAlgorithms *compressAlgorithm,
                             CryptAlgorithms    *cryptAlgorithm,
                             CryptTypes         *cryptType,
                             String             fileName,
                             FileInfo           *fileInfo,
                             uint64             *fragmentOffset,
                             uint64             *fragmentSize
                            );

/***********************************************************************\
* Name   : Archive_readImageEntry
* Purpose: read block device image info from archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
* Output : cryptAlgorithm - used crypt algorithm (can be NULL)
*          cryptType      - used crypt type (can be NULL)
*          deviceName     - image name
*          deviceInfo     - device info
*          blockOffset    - block offset (0..n-1)
*          blockCount     - number of blocks
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/
Errors Archive_readImageEntry(ArchiveInfo        *archiveInfo,
                              ArchiveFileInfo    *archiveFileInfo,
                              CompressAlgorithms *compressAlgorithm,
                              CryptAlgorithms    *cryptAlgorithm,
                              CryptTypes         *cryptType,
                              String             deviceName,
                              DeviceInfo         *deviceInfo,
                              uint64             *blockOffset,
                              uint64             *blockCount
                             );

/***********************************************************************\
* Name   : Archive_readDirectoryEntry
* Purpose: read directory info from archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
* Output : cryptAlgorithm - used crypt algorithm (can be NULL)
*          cryptType      - used crypt type (can be NULL)
*          directoryName  - directory name
*          fileInfo       - file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_readDirectoryEntry(ArchiveInfo     *archiveInfo,
                                  ArchiveFileInfo *archiveFileInfo,
                                  CryptAlgorithms *cryptAlgorithm,
                                  CryptTypes      *cryptType,
                                  String          directoryName,
                                  FileInfo        *fileInfo
                                 );

/***********************************************************************\
* Name   : Archive_readLinkEntry
* Purpose: read link info from archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
* Output : cryptAlgorithm  - used crypt algorithm (can be NULL)
*          cryptType       - used crypt type (can be NULL)
*          linkName        - link name
*          destinationName - name of referenced file
*          fileInfo        - file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_readLinkEntry(ArchiveInfo     *archiveInfo,
                             ArchiveFileInfo *archiveFileInfo,
                             CryptAlgorithms *cryptAlgorithm,
                             CryptTypes      *cryptType,
                             String          linkName,
                             String          destinationName,
                             FileInfo        *fileInfo
                            );

/***********************************************************************\
* Name   : Archive_readSpecialEntry
* Purpose: read special device info from archive
* Input  : archiveInfo     - archive info block
*          archiveFileInfo - archive file info block
* Output : cryptAlgorithm - used crypt algorithm (can be NULL)
*          cryptType      - used crypt type (can be NULL)
*          name           - link name
*          fileInfo       - file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_readSpecialEntry(ArchiveInfo     *archiveInfo,
                                ArchiveFileInfo *archiveFileInfo,
                                CryptAlgorithms *cryptAlgorithm,
                                CryptTypes      *cryptType,
                                String          specialName,
                                FileInfo        *fileInfo
                               );

/***********************************************************************\
* Name   : Archive_closeEntry
* Purpose: clsoe file in archive
* Input  : archiveFileInfo - archive file info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_closeEntry(ArchiveFileInfo *archiveFileInfo);

/***********************************************************************\
* Name   : Archive_writeData
* Purpose: write data to archive
* Input  : archiveFileInfo - archive file info block
*          buffer          - data buffer
*          length          - length of data buffer (bytes)
*          elementSize     - size of single (not splitted) element to
*                            write to archive part (1..n)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : It is assured that a data parts of size elementSize are
*          not splitted
\***********************************************************************/

Errors Archive_writeData(ArchiveFileInfo *archiveFileInfo,
                         const void      *buffer,
                         ulong           length,
                         uint            elementSize
                        );

/***********************************************************************\
* Name   : Archive_readData
* Purpose: read data from archive
* Input  : archiveFileInfo - archive file info block
*          buffer          - data buffer
*          length          - length of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_readData(ArchiveFileInfo *archiveFileInfo,
                        void            *buffer,
                        ulong           length
                       );

/***********************************************************************\
* Name   : Archive_tell
* Purpose: get current read/write position in archive file
* Input  : archiveInfo - archive info block
* Output : -
* Return : current position in archive [bytes]
* Notes  : -
\***********************************************************************/

uint64 Archive_tell(ArchiveInfo *archiveInfo);

/***********************************************************************\
* Name   : Archive_getSize
* Purpose: get size of archive file
* Input  : archiveInfo - archive info block
* Output : -
* Return : size of archive [bytes]
* Notes  : -
\***********************************************************************/

uint64 Archive_getSize(ArchiveInfo *archiveInfo);

/***********************************************************************\
* Name   : Archive_addIndex
* Purpose: add storage index
* Input  : databaseHandle              - database handle
*          storageName                 - storage name
*          jobOptions                  - option settings
*          archiveGetCryptPassword     - get password call back
*          archiveGetCryptPasswordData - user data for get password call
*                                        back
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_addIndex(DatabaseHandle *databaseHandle,
                        const String   storageName,
                        Password       *cryptPassword,
                        String         cryptPrivateKeyFileName
                       );

/***********************************************************************\
* Name   : Archive_updateIndex
* Purpose: update storage index
* Input  : databaseHandle              - database handle
*          storageId      - storage id
*          storageName                 - storage name
*          jobOptions                  - option settings
*          archiveGetCryptPassword     - get password call back
*          archiveGetCryptPasswordData - user data for get password call
*                                        back
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_updateIndex(DatabaseHandle *databaseHandle,
                           int64          storageId,
                           const String   storageName,
                           Password       *cryptPassword,
                           String         cryptPrivateKeyFileName,
                           bool           *pauseFlag,
                           bool           *requestedAbortFlag
                          );

/***********************************************************************\
* Name   : Archive_remIndex
* Purpose: remove storage index
* Input  : databaseHandle - database handle
*          storageId      - storage id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_remIndex(DatabaseHandle *databaseHandle,
                        int64          storageId
                       );

#ifdef __cplusplus
  }
#endif

#endif /* __ARCHIVE__ */

/* end of file */
