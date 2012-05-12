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
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "files.h"
#include "devices.h"

#include "errors.h"
#include "chunks.h"
#include "compress.h"
#include "passwords.h"
#include "crypt.h"
#include "sources.h"
#include "archive_format.h"
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
  ARCHIVE_ENTRY_TYPE_HARDLINK,
  ARCHIVE_ENTRY_TYPE_SPECIAL,

  ARCHIVE_ENTRY_TYPE_UNKNOWN
} ArchiveEntryTypes;

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
  const JobOptions                *jobOptions;
//  ArchiveNewEntryFunction         archiveNewEntryFunction;           // call back for new archive entry
//  void                            *archiveNewEntryUserData;          // user data for call back for new archive entry
  ArchiveNewFileFunction          archiveNewFileFunction;            // call back for new archive file
  void                            *archiveNewFileUserData;           // user data for call back for new archive file
  ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction;   // call back to get crypt password
  void                            *archiveGetCryptPasswordUserData;  // user data for call back to get crypt password

  CryptTypes                      cryptType;                         // crypt type (symmetric/asymmetric; see CryptTypes)
  Password                        *cryptPassword;                    // cryption password for encryption/decryption
  CryptKey                        cryptKey;                          // public/private key for encryption/decryption of random key used for asymmetric encryption
  void                            *cryptKeyData;                     // encrypted random key used for asymmetric encryption
  uint                            cryptKeyDataLength;                // length of encrypted random key

  uint                            blockLength;                       // block length for file entry/file data (depend on used crypt algorithm)

  ArchiveIOTypes                  ioType;                            // i/o type
  union
  {
    struct
    {
      String                      fileName;                          // file name
      FileHandle                  fileHandle;                        // file handle
      bool                        openFlag;                          // TRUE iff archive file is open
    } file;
    struct
    {
      String                      storageName;                       // storage name
      StorageFileHandle           storageFileHandle;                 // storage file handle
    } storage;
  };
  String                          printableName;                     // printable file/storage name (without password)

  DatabaseHandle                  *databaseHandle;
  int64                           storageId;

  const ChunkIO                   *chunkIO;                          // chunk i/o functions
  void                            *chunkIOUserData;                  // chunk i/o functions data

  uint                            partNumber;                        // file part number

  Errors                          pendingError;                      // pending error or ERROR_NONE
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

  CryptAlgorithms    cryptAlgorithm;                   // crypt algorithm for entry
  uint               blockLength;                      /* block length for file entry/file
                                                          data (depend on used crypt
                                                          algorithm)
                                                       */

  ArchiveEntryTypes  archiveEntryType;
  union
  {
    struct
    {
      bool                  deltaSourceInit;           // TRUE if delta source is initialized
      SourceHandle          sourceHandle;              // delta handle

      CompressAlgorithms    deltaCompressAlgorithm;    // delta compression algorithm
      CompressAlgorithms    byteCompressAlgorithm;     // byte compression algorithm

      ChunkFile             chunkFile;                 // file
      ChunkFileEntry        chunkFileEntry;            // file entry
      ChunkFileDelta        chunkFileDelta;            // file delta
      ChunkFileData         chunkFileData;             // file data

      CompressInfo          deltaCompressInfo;         // delta compress info
      CompressInfo          byteCompressInfo;          // byte compress info
      CryptInfo             cryptInfo;                 // cryption info

      uint                  headerLength;              // length of header
      bool                  headerWrittenFlag;         // TRUE iff header written

      byte                  *byteBuffer;               // buffer for processing byte data
      ulong                 byteBufferSize;            // size of byte buffer
      byte                  *deltaBuffer;              // buffer for processing delta data
      ulong                 deltaBufferSize;           // size of delta buffer
    } file;
    struct
    {
      bool                  deltaSourceInit;           // TRUE if delta source is initialized
      SourceHandle          sourceHandle;              // delta source info

      uint                  blockSize;                 // block size of device

      CompressAlgorithms    deltaCompressAlgorithm;    // delta compression algorithm
      CompressAlgorithms    byteCompressAlgorithm;     // byte compression algorithm

      ChunkImage            chunkImage;                // image
      ChunkImageEntry       chunkImageEntry;           // image entry
      ChunkImageDelta       chunkImageDelta;           // image delta
      ChunkImageData        chunkImageData;            // image data

      CompressInfo          deltaCompressInfo;         // delta compress info
      CompressInfo          byteCompressInfo;          // byte compress info
      CryptInfo             cryptInfo;                 // cryption info

      uint                  headerLength;              // length of header
      bool                  headerWrittenFlag;         // TRUE iff header written

      byte                  *byteBuffer;               // buffer for processing byte data
      ulong                 byteBufferSize;            // size of byte buffer
      byte                  *deltaBuffer;              // buffer for processing delta data
      ulong                 deltaBufferSize;           // size of delta buffer
    } image;
    struct
    {
      ChunkDirectory        chunkDirectory;            // directory
      ChunkDirectoryEntry   chunkDirectoryEntry;       // directory entry
    } directory;
    struct
    {
      ChunkLink             chunkLink;                 // link
      ChunkLinkEntry        chunkLinkEntry;            // link entry
    } link;
    struct
    {
      bool                  deltaSourceInit;           // TRUE if delta source is initialized
      SourceHandle          sourceHandle;              // delta source info

      CompressAlgorithms    deltaCompressAlgorithm;    // delta compression algorithm
      CompressAlgorithms    byteCompressAlgorithm;     // byte compression algorithm

      ChunkHardLink         chunkHardLink;             // hard link
      ChunkHardLinkEntry    chunkHardLinkEntry;        // hard link entry
      ChunkHardLinkNameList chunkHardLinkNameList;     // hard link name list
      ChunkHardLinkDelta    chunkHardLinkDelta;        // hard link delta
      ChunkHardLinkData     chunkHardLinkData;         // hard link data

      CompressInfo          deltaCompressInfo;         // delta compress info
      CompressInfo          byteCompressInfo;          // byte compress info
      CryptInfo             cryptInfo;                 // cryption info

      uint                  headerLength;              // length of header
      bool                  headerWrittenFlag;         // TRUE iff header written

      byte                  *byteBuffer;               // buffer for processing byte data
      ulong                 byteBufferSize;            // size of byte buffer
      byte                  *deltaBuffer;              // buffer for processing delta data
      ulong                 deltaBufferSize;           // size of delta buffer
    } hardLink;
    struct
    {
      ChunkSpecial          chunkSpecial;              // special
      ChunkSpecialEntry     chunkSpecialEntry;         // special entry
    } special;
  };
} ArchiveEntryInfo;

typedef bool(*ArchivePauseCallbackFunction)(void *userData);
typedef bool(*ArchiveAbortCallbackFunction)(void *userData);

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
* Name   :
* Purpose:
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Archive_isArchiveFile(const String fileName);

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
                      const JobOptions                *jobOptions,
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
*          sourcePatternList, ???
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
                    const JobOptions                *jobOptions,
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
* Input  : archiveInfo           - archive info block
*          skipUnknownChunksFlag - TRUE to skip unknown chunks
* Output : -
* Return : TRUE if end-of-archive, FALSE otherwise
* Notes  : Note: on error store error and return error value in next
*          operation
\***********************************************************************/

bool Archive_eof(ArchiveInfo *archiveInfo,
                 bool        skipUnknownChunksFlag
                );

/***********************************************************************\
* Name   : Archive_newFile
* Purpose: add new file to archive
* Input  : archiveInfo                     - archive info
*          fileName                        - file name
*          fileInfo                        - file info
*          deltaCompressFlag               - TRUE for delta compression,
*                                            FALSE otherwise
*          byteCompressFlag                - TRUE for byte compression,
*                                            FALSE otherwise (e. g. file
*                                            is to small or already
*                                            compressed)
* Output : archiveEntryInfo - archive file entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_newFileEntry(ArchiveInfo      *archiveInfo,
                            ArchiveEntryInfo *archiveEntryInfo,
                            const String     fileName,
                            const FileInfo   *fileInfo,
                            bool             deltaCompressFlag,
                            bool             byteCompressFlag
                           );

/***********************************************************************\
* Name   : Archive_newImageEntry
* Purpose: add new block device image to archive
* Input  : archiveInfo                     - archive info
*          deviceName                      - special device name
*          deviceInfo                      - device info
*          deltaCompressFlag               - TRUE for delta compression,
*                                            FALSE otherwise
*          byteCompressFlag                - TRUE for byte compression,
*                                            FALSE otherwise (e. g. file
*                                            is to small or already
*                                            compressed)
* Output : archiveEntryInfo - archive image entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_newImageEntry(ArchiveInfo      *archiveInfo,
                             ArchiveEntryInfo *archiveEntryInfo,
                             const String     deviceName,
                             const DeviceInfo *deviceInfo,
                             bool             deltaCompressFlag,
                             bool             byteCompressFlag
                            );

/***********************************************************************\
* Name   : Archive_newDirectoryEntry
* Purpose: add new directory to archive
* Input  : archiveInfo      - archive info
*          name             - directory name
*          fileInfo         - file info
* Output : archiveEntryInfo - archive directory entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_newDirectoryEntry(ArchiveInfo      *archiveInfo,
                                 ArchiveEntryInfo *archiveEntryInfo,
                                 const String     directoryName,
                                 const FileInfo   *fileInfo
                                );

/***********************************************************************\
* Name   : Archive_newLinkEntry
* Purpose: add new link to archive
* Input  : archiveInfo      - archive info variable
*          fileName         - link name
*          destinationName  - name of referenced file
*          fileInfo         - file info
* Output : archiveEntryInfo - archive link entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_newLinkEntry(ArchiveInfo      *archiveInfo,
                            ArchiveEntryInfo *archiveEntryInfo,
                            const String     linkName,
                            const String     destinationName,
                            const FileInfo   *fileInfo
                           );

/***********************************************************************\
* Name   : Archive_newHardLinkEntry
* Purpose: add new hard link to archive
* Input  : archiveInfo                     - archive info
*          fileNameList                    - list of file names
*          fileInfo                        - file info
*          deltaCompressFlag               - TRUE for delta compression,
*                                            FALSE otherwise
*          byteCompressFlag                - TRUE for byte compression,
*                                            FALSE otherwise (e. g. file
*                                            is to small or already
*                                            compressed)
* Output : archiveEntryInfo - archive hard link entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_newHardLinkEntry(ArchiveInfo      *archiveInfo,
                                ArchiveEntryInfo *archiveEntryInfo,
                                const StringList *fileNameList,
                                const FileInfo   *fileInfo,
                                bool             deltaCompressFlag,
                                bool             byteCompressFlag
                               );

/***********************************************************************\
* Name   : Archive_newSpecialEntry
* Purpose: add new special entry to archive
* Input  : archiveInfo      - archive info
*          specialName      - special name
*          fileInfo         - file info
* Output : archiveEntryInfo - archive special entry info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_newSpecialEntry(ArchiveInfo      *archiveInfo,
                               ArchiveEntryInfo *archiveEntryInfo,
                               const String     specialName,
                               const FileInfo   *fileInfo
                              );

/***********************************************************************\
* Name   : Archive_getNextArchiveEntryType
* Purpose: get type of next entry in archive
* Input  : archiveInfo           - archive info block
*          skipUnknownChunksFlag - TRUE to skip unknown chunks
* Output : archiveEntryType - archive entry type
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_getNextArchiveEntryType(ArchiveInfo       *archiveInfo,
                                       ArchiveEntryTypes *archiveEntryType,
                                       bool              skipUnknownChunksFlag
                                      );

/***********************************************************************\
* Name   : Archive_skipNextEntry
* Purpose: skip next entry in archive
* Input  : archiveInfo - archive info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_skipNextEntry(ArchiveInfo *archiveInfo);

/***********************************************************************\
* Name   : Archive_readFileEntry
* Purpose: read file info from archive
* Input  : archiveInfo      - archive info
*          archiveEntryInfo - archive file entry info
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
                             ArchiveEntryInfo   *archiveEntryInfo,
                             CompressAlgorithms *deltaCompressAlgorithm,
                             CompressAlgorithms *byteCompressAlgorithm,
                             CryptAlgorithms    *cryptAlgorithm,
                             CryptTypes         *cryptType,
                             String             fileName,
                             FileInfo           *fileInfo,
                             String             deltaSourceName,
                             uint64             *fragmentOffset,
                             uint64             *fragmentSize
                            );

/***********************************************************************\
* Name   : Archive_readImageEntry
* Purpose: read block device image info from archive
* Input  : archiveInfo      - archive info
*          archiveEntryInfo - archive image entry info
* Output : cryptAlgorithm - used crypt algorithm (can be NULL)
*          cryptType      - used crypt type (can be NULL)
*          deviceName     - image name
*          deviceInfo     - device info (can be NULL)
*          blockOffset    - block offset (0..n-1)
*          blockCount     - number of blocks
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/
Errors Archive_readImageEntry(ArchiveInfo        *archiveInfo,
                              ArchiveEntryInfo   *archiveEntryInfo,
                              CompressAlgorithms *deltaCompressAlgorithm,
                              CompressAlgorithms *byteCompressAlgorithm,
                              CryptAlgorithms    *cryptAlgorithm,
                              CryptTypes         *cryptType,
                              String             deviceName,
                              DeviceInfo         *deviceInfo,
                              String             deltaSourceName,
                              uint64             *blockOffset,
                              uint64             *blockCount
                             );

/***********************************************************************\
* Name   : Archive_readDirectoryEntry
* Purpose: read directory info from archive
* Input  : archiveInfo      - archive info
*          archiveEntryInfo - archive directory info
* Output : cryptAlgorithm - used crypt algorithm (can be NULL)
*          cryptType      - used crypt type (can be NULL)
*          directoryName  - directory name
*          fileInfo       - file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_readDirectoryEntry(ArchiveInfo      *archiveInfo,
                                  ArchiveEntryInfo *archiveEntryInfo,
                                  CryptAlgorithms  *cryptAlgorithm,
                                  CryptTypes       *cryptType,
                                  String           directoryName,
                                  FileInfo         *fileInfo
                                 );

/***********************************************************************\
* Name   : Archive_readLinkEntry
* Purpose: read link info from archive
* Input  : archiveInfo      - archive info
*          archiveEntryInfo - archive link entry info
* Output : cryptAlgorithm  - used crypt algorithm (can be NULL)
*          cryptType       - used crypt type (can be NULL)
*          linkName        - link name
*          destinationName - name of referenced file
*          fileInfo        - file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_readLinkEntry(ArchiveInfo      *archiveInfo,
                             ArchiveEntryInfo *archiveEntryInfo,
                             CryptAlgorithms  *cryptAlgorithm,
                             CryptTypes       *cryptType,
                             String           linkName,
                             String           destinationName,
                             FileInfo         *fileInfo
                            );

/***********************************************************************\
* Name   : Archive_readHardLinkEntry
* Purpose: read hard link info from archive
* Input  : archiveInfo      - archive info
*          archiveEntryInfo - archive hard link entry info
//????
*          sourceGetEntryDataBlock
*          sourceGetEntryDataBlockUserData
* Output : compressAlgorithm - used compression algorithm (can be NULL)
*          fileNameList      - list of file names
*          fileInfo          - file info
*          cryptAlgorithm    - used crypt algorithm (can be NULL)
*          cryptType         - used crypt type (can be NULL)
*          fragmentOffset    - fragment offset (can be NULL)
*          fragmentSize      - fragment size in bytes (can be NULL)
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_readHardLinkEntry(ArchiveInfo        *archiveInfo,
                                 ArchiveEntryInfo   *archiveEntryInfo,
                                 CompressAlgorithms *deltaCompressAlgorithm,
                                 CompressAlgorithms *byteCompressAlgorithm,
                                 CryptAlgorithms    *cryptAlgorithm,
                                 CryptTypes         *cryptType,
                                 StringList         *fileNameList,
                                 FileInfo           *fileInfo,
                                 String             deltaSourceName,
                                 uint64             *fragmentOffset,
                                 uint64             *fragmentSize
                                );

/***********************************************************************\
* Name   : Archive_readSpecialEntry
* Purpose: read special device info from archive
* Input  : archiveInfo      - archive info
*          archiveEntryInfo - archive special entry info
* Output : cryptAlgorithm - used crypt algorithm (can be NULL)
*          cryptType      - used crypt type (can be NULL)
*          name           - link name
*          fileInfo       - file info
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_readSpecialEntry(ArchiveInfo      *archiveInfo,
                                ArchiveEntryInfo *archiveEntryInfo,
                                CryptAlgorithms  *cryptAlgorithm,
                                CryptTypes       *cryptType,
                                String           specialName,
                                FileInfo         *fileInfo
                               );

/***********************************************************************\
* Name   : Archive_closeEntry
* Purpose: clsoe file in archive
* Input  : archiveEntryInfo - archive file info block
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_closeEntry(ArchiveEntryInfo *archiveEntryInfo);

/***********************************************************************\
* Name   : Archive_writeData
* Purpose: write data to archive
* Input  : archiveEntryInfo - archive file info block
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
* Input  : archiveEntryInfo - archive file info block
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
* Input  : archiveEntryInfo - archive file info block
*          buffer           - data buffer
*          length           - length of data buffer
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

/***********************************************************************\
* Name   : Archive_eofData
* Purpose: check if end-of-archive data
* Input  : archiveEntryInfo - archive file info block
* Output : -
* Return : TRUE if end-of-archive data, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Archive_eofData(ArchiveEntryInfo *archiveEntryInfo);

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
* Name   : Archive_seek
* Purpose: seek to position in archive file
* Input  : archiveInfo - archive info block
*          offset      - offset in archive
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_seek(ArchiveInfo *archiveInfo,
                    uint64      offset
                   );

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
*          indexMode                   - index mode
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
                        IndexModes     indexMode,
                        Password       *cryptPassword,
                        String         cryptPrivateKeyFileName
                       );

/***********************************************************************\
* Name   : Archive_updateIndex
* Purpose: update storage index
* Input  : databaseHandle          - database handle
*          storageId               - storage id
*          storageName             - storage name
*          cryptPassword           - encryption password
*          cryptPrivateKeyFileName - encryption private key file name
*          pauseCallback           - pause check callback (can be NULL)
*          pauseUserData           - pause user data
*          abortCallback           - abort check callback (can be NULL)
*          abortUserData           - abort user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Archive_updateIndex(DatabaseHandle               *databaseHandle,
                           int64                        storageId,
                           const String                 storageName,
                           Password                     *cryptPassword,
                           String                       cryptPrivateKeyFileName,
                           ArchivePauseCallbackFunction pauseCallback,
                           void                         *pauseUserData,
                           ArchiveAbortCallbackFunction abortCallback,
                           void                         *abortUserData
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

#if 0
// NYI
Errors Archive_copy(ArchiveInfo                     *archiveInfo,
                    const String                    storageName,
                    const JobOptions                *jobOptions,
                    ArchiveGetCryptPasswordFunction archiveGetCryptPassword,
                    void                            *archiveGetCryptPasswordData,
                    const String                    newStorageName
                   );
#endif /* 0 */

#ifdef __cplusplus
  }
#endif

#endif /* __ARCHIVE__ */

/* end of file */
