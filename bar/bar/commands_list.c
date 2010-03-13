/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/commands_list.c,v $
* $Revision: 1.9 $
* $Author: torsten $
* Contents: Backup ARchiver archive list function
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"
#include "stringlists.h"
#include "arrays.h"

#include "bar.h"
#include "errors.h"
#include "misc.h"
#include "patterns.h"
#include "entrylists.h"
#include "patternlists.h"
#include "files.h"
#include "archive.h"
#include "storage.h"
#include "network.h"
#include "server.h"

#include "commands_list.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define DEFAULT_FORMAT_TITLE_NORMAL_LONG "%type:-5s %size:-10s %date:-25s %part:-22s %compress:-10s %ratio:-7s %crypt:-10s %name:s"
#define DEFAULT_FORMAT_TITLE_GROUP_LONG  "%archiveName:-20s %type:4s %size:-10s %date:-25s %part:-22s %compress:-10s %ratio:-7s %crypt:-10s %name:s"
#define DEFAULT_FORMAT_TITLE_NORMAL      "%type:-5s %size:-10s %date:-25s %name:s"
#define DEFAULT_FORMAT_TITLE_GROUP       "%archiveName:-20s %type:4s %size:-10s %date:-25s %name:s"

#define DEFAULT_FORMAT_NORMAL_LONG       "%type:-5s %size:-10s %date:-25s %part:-22s %compress:-10s %ratio:-7s %crypt:-10s %name:s"
#define DEFAULT_FORMAT_GROUP_LONG        "%archiveName:-20s %type:4s %size:-10s %date:-25s %part:-22s %compress:-10s %ratio:-7s %crypt:-10s %name:s"
#define DEFAULT_FORMAT_NORMAL            "%type:-5s %size:-10s %date:-25s %name:s"
#define DEFAULT_FORMAT_GROUP             "%archiveName:-20s %type:4s %size:-10s %date:-25s %name:s"

/***************************** Datatypes *******************************/

/* archive entry node */
typedef struct ArchiveEntryNode
{
  LIST_NODE_HEADER(struct ArchiveEntryNode);

  String            archiveFileName;
  ArchiveEntryTypes type;
  union
  {
    struct
    {
      String             fileName;
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
      CryptAlgorithms cryptAlgorithm;
      CryptTypes      cryptType;
    } directory;
    struct
    {
      String          linkName;
      String          destinationName;
      CryptAlgorithms cryptAlgorithm;
      CryptTypes      cryptType;
    } link;
    struct
    {
      String           fileName;
      CryptAlgorithms  cryptAlgorithm;
      CryptTypes       cryptType;
      FileSpecialTypes fileSpecialType;
      ulong            major;
      ulong            minor;
    } special;
  };

  String       name;
  SocketHandle socketHandle;
} ArchiveEntryNode;

/* archive entry list */
typedef struct
{
  LIST_HEADER(ArchiveEntryNode);
} ArchiveEntryList;

// obsolete?
typedef struct SSHSocketNode
{
  LIST_NODE_HEADER(struct SSHSocketNode);

  String       name;
  SocketHandle socketHandle;
} SSHSocketNode;

typedef struct
{
  LIST_HEADER(SSHSocketNode);
} SSHSocketList;

/***************************** Variables *******************************/
LOCAL ArchiveEntryList archiveEntryList;

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/*---------------------------------------------------------------------*/

/***********************************************************************\
* Name   : getHumanSizeString
* Purpose: get human readable size string
* Input  : buffer     - buffer to format string into
*          bufferSize - size of buffer
*          n          - size value
* Output : -
* Return : buffer with formated human string size
* Notes  : -
\***********************************************************************/

LOCAL const char* getHumanSizeString(char *buffer, uint bufferSize, uint64 n)
{
  if      (n > 1024L*1024L*1024L)
  {
    snprintf(buffer,bufferSize,"%.1fG",(double)n/(double)(1024L*1024L*1024L));
  }
  else if (n >       1024L*1024L)
  {
    snprintf(buffer,bufferSize,"%.1fM",(double)n/(double)(1024L*1024L));
  }
  else if (n >             1024L)
  {
    snprintf(buffer,bufferSize,"%.1fK",(double)n/(double)(1024L));
  }
  else
  {
    snprintf(buffer,bufferSize,"%llu",n);
  }

  return buffer;
}

/***********************************************************************\
* Name   : printHeader
* Purpose: print list header
* Input  : archiveFileName - archive file name or NULL if archive should
*                            not be printed
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printHeader(const String archiveFileName)
{
  const TextMacro MACROS[] =
  {
    TEXT_MACRO_CSTRING("%archiveName","Archive"  ),
    TEXT_MACRO_CSTRING("%type",       "Type"     ),
    TEXT_MACRO_CSTRING("%size",       "Size"     ),
    TEXT_MACRO_CSTRING("%date",       "Date/Time"),
    TEXT_MACRO_CSTRING("%part",       "Part"     ),
    TEXT_MACRO_CSTRING("%compress",   "Compress" ),
    TEXT_MACRO_CSTRING("%ratio",      "Ratio %"  ),
    TEXT_MACRO_CSTRING("%crypt",      "Crypt"    ),
    TEXT_MACRO_CSTRING("%name",       "Name"     ),
    TEXT_MACRO_CSTRING("%type",       "Type"     ),
    TEXT_MACRO_CSTRING("%type",       "Type"     ),
    TEXT_MACRO_CSTRING("%type",       "Type"     ),
    TEXT_MACRO_CSTRING("%type",       "Type"     ),
  };

  String     string;
  const char *template;

  string = String_new();

  if (!globalOptions.noHeaderFooterFlag)
  {
    /* header */
    if (archiveFileName != NULL)
    {
      printInfo(0,"List archive '%s':\n",String_cString(archiveFileName));
      printInfo(0,"\n");
    }

    /* title line */
    if (globalOptions.longFormatFlag)
    {
      template = (archiveFileName != NULL) ? DEFAULT_FORMAT_TITLE_NORMAL_LONG : DEFAULT_FORMAT_TITLE_GROUP_LONG;
    }
    else
    {
      template = (archiveFileName != NULL) ? DEFAULT_FORMAT_TITLE_NORMAL : DEFAULT_FORMAT_TITLE_GROUP;
    }
    Misc_expandMacros(String_clear(string),template,MACROS,SIZE_OF_ARRAY(MACROS));
    printInfo(0,"%s\n",String_cString(string));
    printInfo(0,"--------------------------------------------------------------------------------------------------------------\n");
  }

  String_delete(string);
}

/***********************************************************************\
* Name   : printFooter
* Purpose: print list footer
* Input  : fileCount - number of files listed
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printFooter(ulong fileCount)
{
  if (!globalOptions.noHeaderFooterFlag)
  {
    printInfo(0,"--------------------------------------------------------------------------------------------------------------\n");
    printInfo(0,"%lu %s\n",fileCount,(fileCount == 1)?"entry":"entries");
    printInfo(0,"\n");
  }
}

/***********************************************************************\
* Name   : printFileInfo
* Purpose: print file information
* Input  : archiveFileName   - archive name or NULL if archive name
*                              should not be printed
*          fileName          - file name
*          fileSize          - file size [bytes]
*          archiveSize       - archive size [bytes]
*          compressAlgorithm - used compress algorithm
*          cryptAlgorithm    - used crypt algorithm
*          cryptType         - crypt type; see CRYPT_TYPES
*          fragmentOffset    - fragment offset (0..n-1)
*          fragmentSize      - fragment length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printFileInfo(const String       archiveFileName,
                         const String       fileName,
                         uint64             size,
                         uint64             timeModified,
                         uint64             archiveSize,
                         CompressAlgorithms compressAlgorithm,
                         CryptAlgorithms    cryptAlgorithm,
                         CryptTypes         cryptType,
                         uint64             fragmentOffset,
                         uint64             fragmentSize
                        )
{
  String dateTime;
  double ratio;
  char   buffer[16];
  String cryptString;

  assert(fileName != NULL);

  dateTime = Misc_formatDateTime(String_new(),timeModified,NULL);

  if ((compressAlgorithm != COMPRESS_ALGORITHM_NONE) && (fragmentSize > 0))
  {
    ratio = 100.0-archiveSize*100.0/fragmentSize;
  }
  else
  {
    ratio = 0;
  }

  if (archiveFileName != NULL)
  {
    printf("%-20s ",String_cString(archiveFileName));
  }
  printf("FILE  ");
  if (globalOptions.humanFormatFlag)
  {
    printf("%10s",getHumanSizeString(buffer,sizeof(buffer),size));
  }
  else
  {
    printf("%10llu",size);
  }
  if (globalOptions.longFormatFlag)
  {
    cryptString = String_format(String_new(),"%s%c",Crypt_getAlgorithmName(cryptAlgorithm),(cryptType==CRYPT_TYPE_ASYMMETRIC)?'*':' ');
    printf(" %-25s %10llu..%10llu %-10s %6.1f%% %-10s %s\n",
           String_cString(dateTime),
           fragmentOffset,
           (fragmentSize > 0)?fragmentOffset+fragmentSize-1:fragmentOffset,
           Compress_getAlgorithmName(compressAlgorithm),
           ratio,
           String_cString(cryptString),
           String_cString(fileName)
          );
    String_delete(cryptString);
  }
  else
  {
    printf(" %-25s %s\n",
           String_cString(dateTime),
           String_cString(fileName)
          );
  }

  String_delete(dateTime);
}

/***********************************************************************\
* Name   : printImageInfo
* Purpose: print image information
* Input  : archiveFileName   - archive name or NULL if archive name
*                              should not be printed
*          iamgeName         - image name
*          size              - image size [bytes]
*          archiveSize       - archive size [bytes]
*          compressAlgorithm - used compress algorithm
*          cryptAlgorithm    - used crypt algorithm
*          cryptType         - crypt type; see CRYPT_TYPES
*          blockSize         - block size :bytes]
*          blockOffset       - block offset (0..n-1)
*          blockCount        - number of blocks
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printImageInfo(const String       archiveFileName,
                          const String       imageName,
                          uint64             size,
                          uint64             archiveSize,
                          CompressAlgorithms compressAlgorithm,
                          CryptAlgorithms    cryptAlgorithm,
                          CryptTypes         cryptType,
                          uint               blockSize,
                          uint64             blockOffset,
                          uint64             blockCount
                         )
{
  double ratio;
  char   buffer[16];
  String cryptString;

  assert(imageName != NULL);

  if ((compressAlgorithm != COMPRESS_ALGORITHM_NONE) && (blockCount > 0))
  {
    ratio = 100.0-archiveSize*100.0/(blockCount*(uint64)blockSize);
  }
  else
  {
    ratio = 0;
  }

  if (archiveFileName != NULL)
  {
    printf("%-20s ",String_cString(archiveFileName));
  }
  printf("IMAGE ");
  if (globalOptions.humanFormatFlag)
  {
    printf("%10s",getHumanSizeString(buffer,sizeof(buffer),size));
  }
  else
  {
    printf("%10llu",size);
  }
  if (globalOptions.longFormatFlag)
  {
    cryptString = String_format(String_new(),"%s%c",Crypt_getAlgorithmName(cryptAlgorithm),(cryptType==CRYPT_TYPE_ASYMMETRIC)?'*':' ');
    printf("                           %10llu..%10llu %-10s %6.1f%% %-10s %s\n",
           blockOffset*(uint64)blockSize,
           (blockOffset+blockCount)*(uint64)blockSize-((blockCount > 0)?1:0),
           Compress_getAlgorithmName(compressAlgorithm),
           ratio,
           String_cString(cryptString),
           String_cString(imageName)
          );
    String_delete(cryptString);
  }
  else
  {
    printf("                           %s\n",
           String_cString(imageName)
          );
  }
}

/***********************************************************************\
* Name   : printDirectoryInfo
* Purpose: print directory information
* Input  : archiveFileName - archive name or NULL if archive name should
*                            not be printed
*          directoryName   - directory name
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printDirectoryInfo(const String    archiveFileName,
                              const String    directoryName,
                              CryptAlgorithms cryptAlgorithm,
                              CryptTypes      cryptType
                             )
{
  String cryptString;

  assert(directoryName != NULL);

  if (archiveFileName != NULL)
  {
    printf("%-30s ",String_cString(archiveFileName));
  }
  if (globalOptions.longFormatFlag)
  {
    cryptString = String_format(String_new(),"%s%c",Crypt_getAlgorithmName(cryptAlgorithm),(cryptType==CRYPT_TYPE_ASYMMETRIC)?'*':' ');
    printf("DIR                                                                                  %-10s %s\n",
           String_cString(cryptString),
           String_cString(directoryName)
          );
    String_delete(cryptString);
  }
  else
  {
    printf("DIR                                        %s\n",
           String_cString(directoryName)
          );
  }
}

/***********************************************************************\
* Name   : printLinkInfo
* Purpose: print link information
* Input  : archiveFileName - archive name or NULL if archive name should
*                            not be printed
*          linkName        - link name
*          destinationName - name of referenced file
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printLinkInfo(const String    archiveFileName,
                         const String    linkName,
                         const String    destinationName,
                         CryptAlgorithms cryptAlgorithm,
                         CryptTypes      cryptType
                        )
{
  String cryptString;

  assert(linkName != NULL);
  assert(destinationName != NULL);

  if (archiveFileName != NULL)
  {
    printf("%-30s ",String_cString(archiveFileName));
  }
  if (globalOptions.longFormatFlag)
  {
    cryptString = String_format(String_new(),"%s%c",Crypt_getAlgorithmName(cryptAlgorithm),(cryptType==CRYPT_TYPE_ASYMMETRIC)?'*':' ');
    printf("LINK                                                                                 %-10s %s -> %s\n",
           String_cString(cryptString),
           String_cString(linkName),
           String_cString(destinationName)
          );
    String_delete(cryptString);
  }
  else
  {
    printf("LINK                                       %s -> %s\n",
           String_cString(linkName),
           String_cString(destinationName)
          );
  }
}

/***********************************************************************\
* Name   : printSpecialInfo
* Purpose: print special information
* Input  : archiveFileName - archive name or NULL if archive name should
*                            not be printed
*          fileName        - file name
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
*          fileSpecialType - special file type
*          major           - device major number
*          minor           - device minor number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printSpecialInfo(const String     archiveFileName,
                            const String     fileName,
                            CryptAlgorithms  cryptAlgorithm,
                            CryptTypes       cryptType,
                            FileSpecialTypes fileSpecialType,
                            ulong            major,
                            ulong            minor
                           )
{
  String cryptString;

  assert(fileName != NULL);

  if (archiveFileName != NULL)
  {
    printf("%-30s ",String_cString(archiveFileName));
  }
  switch (fileSpecialType)
  {
    case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
      if (globalOptions.longFormatFlag)
      {
        cryptString = String_format(String_new(),"%s%c",Crypt_getAlgorithmName(cryptAlgorithm),(cryptType==CRYPT_TYPE_ASYMMETRIC)?'*':' ');
        printf("CHAR                                                                                 %-10s %s, %lu %lu\n",
               String_cString(cryptString),
               String_cString(fileName),
               major,
               minor
              );
        String_delete(cryptString);
      }
      else
      {
        printf("CHAR                                       %s\n",
               String_cString(fileName)
              );
      }
      break;
    case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
      if (globalOptions.longFormatFlag)
      {
        cryptString = String_format(String_new(),"%s%c",Crypt_getAlgorithmName(cryptAlgorithm),(cryptType==CRYPT_TYPE_ASYMMETRIC)?'*':' ');
        printf("BLOCK                                                                               %-10s %s, %lu %lu\n",
               String_cString(cryptString),
               String_cString(fileName),
               major,
               minor
              );
        String_delete(cryptString);
      }
      else
      {
        printf("BLOCK                                      %s\n",
               String_cString(fileName)
              );
      }
      break;
    case FILE_SPECIAL_TYPE_FIFO:
      if (globalOptions.longFormatFlag)
      {
        cryptString = String_format(String_new(),"%s%c",Crypt_getAlgorithmName(cryptAlgorithm),(cryptType==CRYPT_TYPE_ASYMMETRIC)?'*':' ');
        printf("FIFO                                                                                 %-10s %s\n",
               String_cString(cryptString),
               String_cString(fileName)
              );
        String_delete(cryptString);
      }
      else
      {
        printf("FIFO                                       %s\n",
               String_cString(fileName)
              );
      }
      break;
    case FILE_SPECIAL_TYPE_SOCKET:
      if (globalOptions.longFormatFlag)
      {
        cryptString = String_format(String_new(),"%s%c",Crypt_getAlgorithmName(cryptAlgorithm),(cryptType==CRYPT_TYPE_ASYMMETRIC)?'*':' ');
        printf("SOCKET                                                                              %-10s %s\n",
               String_cString(cryptString),
               String_cString(fileName)
              );
        String_delete(cryptString);
      }
      else
      {
        printf("SOCKET                                     %s\n",
               String_cString(fileName)
              );
      }
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
}

/***********************************************************************\
* Name   : addListFileInfo
* Purpose: add file info to archive entry list
* Input  : fileName          - file name
*          fileSize          - file size [bytes]
*          archiveSize       - archive size [bytes]
*          compressAlgorithm - used compress algorithm
*          cryptAlgorithm    - used crypt algorithm
*          cryptType         - crypt type; see CRYPT_TYPES
*          fragmentOffset    - fragment offset (0..n-1)
*          fragmentSize      - fragment length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListFileInfo(const String       archiveFileName,
                           const String       fileName,
                           uint64             size,
                           uint64             timeModified,
                           uint64             archiveSize,
                           CompressAlgorithms compressAlgorithm,
                           CryptAlgorithms    cryptAlgorithm,
                           CryptTypes         cryptType,
                           uint64             fragmentOffset,
                           uint64             fragmentSize
                          )
{
  ArchiveEntryNode *archiveEntryNode;

  /* allocate node */
  archiveEntryNode = LIST_NEW_NODE(ArchiveEntryNode);
  if (archiveEntryNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init node */
  archiveEntryNode->archiveFileName        = String_duplicate(archiveFileName);
  archiveEntryNode->type                   = ARCHIVE_ENTRY_TYPE_FILE;
  archiveEntryNode->file.fileName          = String_duplicate(fileName);
  archiveEntryNode->file.size              = size;
  archiveEntryNode->file.timeModified      = timeModified;
  archiveEntryNode->file.archiveSize       = archiveSize;
  archiveEntryNode->file.compressAlgorithm = compressAlgorithm;
  archiveEntryNode->file.cryptAlgorithm    = cryptAlgorithm;
  archiveEntryNode->file.cryptType         = cryptType;
  archiveEntryNode->file.fragmentOffset    = fragmentOffset;
  archiveEntryNode->file.fragmentSize      = fragmentSize;

  /* append to list */
  List_append(&archiveEntryList,archiveEntryNode);
}

/***********************************************************************\
* Name   : addListImageInfo
* Purpose: add image info to archive entry list
* Input  : imageName         - image name
*          size              - iamge size [bytes]
*          archiveSize       - archive size [bytes]
*          compressAlgorithm - used compress algorithm
*          cryptAlgorithm    - used crypt algorithm
*          cryptType         - crypt type; see CRYPT_TYPES
*          blockSize         - block size
*          blockOffset       - block offset (0..n-1)
*          blockCount        - block count
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListImageInfo(const String       archiveFileName,
                            const String       imageName,
                            uint64             size,
                            uint64             archiveSize,
                            CompressAlgorithms compressAlgorithm,
                            CryptAlgorithms    cryptAlgorithm,
                            CryptTypes         cryptType,
                            uint               blockSize,
                            uint64             blockOffset,
                            uint64             blockCount
                           )
{
  ArchiveEntryNode *archiveEntryNode;

  /* allocate node */
  archiveEntryNode = LIST_NEW_NODE(ArchiveEntryNode);
  if (archiveEntryNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init node */
  archiveEntryNode->archiveFileName         = String_duplicate(archiveFileName);
  archiveEntryNode->type                    = ARCHIVE_ENTRY_TYPE_IMAGE;
  archiveEntryNode->image.imageName         = String_duplicate(imageName);
  archiveEntryNode->image.size              = size;
  archiveEntryNode->image.archiveSize       = archiveSize;
  archiveEntryNode->image.compressAlgorithm = compressAlgorithm;
  archiveEntryNode->image.cryptAlgorithm    = cryptAlgorithm;
  archiveEntryNode->image.cryptType         = cryptType;
  archiveEntryNode->image.blockSize         = blockSize;
  archiveEntryNode->image.blockOffset       = blockOffset;
  archiveEntryNode->image.blockCount        = blockCount;

  /* append to list */
  List_append(&archiveEntryList,archiveEntryNode);
}

/***********************************************************************\
* Name   : addListDirectoryInfo
* Purpose: add directory info to archive entry list
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListDirectoryInfo(const String    archiveFileName,
                                const String    directoryName,
                                CryptAlgorithms cryptAlgorithm,
                                CryptTypes      cryptType
                               )
{
  ArchiveEntryNode *archiveEntryNode;

  /* allocate node */
  archiveEntryNode = LIST_NEW_NODE(ArchiveEntryNode);
  if (archiveEntryNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init node */
  archiveEntryNode->archiveFileName          = String_duplicate(archiveFileName);
  archiveEntryNode->type                     = ARCHIVE_ENTRY_TYPE_DIRECTORY;
  archiveEntryNode->directory.directoryName  = String_duplicate(directoryName);
  archiveEntryNode->directory.cryptAlgorithm = cryptAlgorithm;
  archiveEntryNode->directory.cryptType      = cryptType;

  /* append to list */
  List_append(&archiveEntryList,archiveEntryNode);
}

/***********************************************************************\
* Name   : addListLinkInfo
* Purpose: add link info to archive entry list
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListLinkInfo(const String    archiveFileName,
                           const String    linkName,
                           const String    destinationName,
                           CryptAlgorithms cryptAlgorithm,
                           CryptTypes      cryptType
                          )
{
  ArchiveEntryNode *archiveEntryNode;

  /* allocate node */
  archiveEntryNode = LIST_NEW_NODE(ArchiveEntryNode);
  if (archiveEntryNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init node */
  archiveEntryNode->archiveFileName      = String_duplicate(archiveFileName);
  archiveEntryNode->type                 = ARCHIVE_ENTRY_TYPE_LINK;
  archiveEntryNode->link.linkName        = String_duplicate(linkName);
  archiveEntryNode->link.destinationName = String_duplicate(destinationName);
  archiveEntryNode->link.cryptAlgorithm  = cryptAlgorithm;
  archiveEntryNode->link.cryptType       = cryptType;

  /* append to list */
  List_append(&archiveEntryList,archiveEntryNode);
}

/***********************************************************************\
* Name   : addListSpecialInfo
* Purpose: add special info to archive entry list
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListSpecialInfo(const String     archiveFileName,
                              const String     fileName,
                              CryptAlgorithms  cryptAlgorithm,
                              CryptTypes       cryptType,
                              FileSpecialTypes fileSpecialType,
                              ulong            major,
                              ulong            minor
                             )
{
  ArchiveEntryNode *archiveEntryNode;

  /* allocate node */
  archiveEntryNode = LIST_NEW_NODE(ArchiveEntryNode);
  if (archiveEntryNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  /* init node */
  archiveEntryNode->archiveFileName         = String_duplicate(archiveFileName);
  archiveEntryNode->type                    = ARCHIVE_ENTRY_TYPE_SPECIAL;
  archiveEntryNode->special.fileName        = String_duplicate(fileName);
  archiveEntryNode->special.cryptAlgorithm  = cryptAlgorithm;
  archiveEntryNode->special.cryptType       = cryptType;
  archiveEntryNode->special.fileSpecialType = fileSpecialType;
  archiveEntryNode->special.major           = major;
  archiveEntryNode->special.minor           = minor;

  /* append to list */
  List_append(&archiveEntryList,archiveEntryNode);
}

/***********************************************************************\
* Name   : freeArchiveEntryNode
* Purpose: free archive entry node
* Input  : archiveEntryNode - node to free
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeArchiveEntryNode(ArchiveEntryNode *archiveEntryNode)
{
  assert(archiveEntryNode != NULL);

  switch (archiveEntryNode->type)
  {
    case ARCHIVE_ENTRY_TYPE_NONE:
      break;
    case ARCHIVE_ENTRY_TYPE_FILE:
      String_delete(archiveEntryNode->file.fileName);
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      String_delete(archiveEntryNode->directory.directoryName);
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      String_delete(archiveEntryNode->link.destinationName);
      String_delete(archiveEntryNode->link.linkName);
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      String_delete(archiveEntryNode->special.fileName);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  String_delete(archiveEntryNode->archiveFileName);
}

/***********************************************************************\
* Name   : compareArchiveEntryNode
* Purpose: compare archive entries
* Input  : archiveEntryNode1,archiveEntryNode2 - nodes to compare
* Output : -
* Return : -1 iff name1 < name2 or
*                 name1 == name2 && timeModified1 < timeModified2
*           1 iff name1 > name2 or
*                 name1 == name2 && timeModified1 > timeModified2
*           0 iff name1 == name2 && timeModified1 == timeModified2
* Notes  : -
\***********************************************************************/

LOCAL int compareArchiveEntryNode(ArchiveEntryNode *archiveEntryNode1, ArchiveEntryNode *archiveEntryNode2, void *dummy)
{
  String name1,name2;
  uint64 modifiedTime1,modifiedTime2;

  assert(archiveEntryNode1 != NULL);
  assert(archiveEntryNode2 != NULL);

  UNUSED_VARIABLE(dummy);

  /* get data */
  name1         = NULL;
  modifiedTime1 = 0LL;
  name2         = NULL;
  modifiedTime2 = 0LL;
  switch (archiveEntryNode1->type)
  {
    case ARCHIVE_ENTRY_TYPE_FILE:
      name1         = archiveEntryNode1->file.fileName;
      modifiedTime1 = archiveEntryNode1->file.timeModified;
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      name1         = archiveEntryNode1->directory.directoryName;
      modifiedTime1 = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      name1         = archiveEntryNode1->link.linkName;
      modifiedTime1 = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      name1         = archiveEntryNode1->special.fileName;
      modifiedTime1 = 0LL;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  switch (archiveEntryNode2->type)
  {
    case ARCHIVE_ENTRY_TYPE_FILE:
      name2         = archiveEntryNode2->file.fileName;
      modifiedTime2 = archiveEntryNode2->file.timeModified;
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      name2         = archiveEntryNode2->directory.directoryName;
      modifiedTime2 = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      name2         = archiveEntryNode2->link.linkName;
      modifiedTime2 = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      name2         = archiveEntryNode2->special.fileName;
      modifiedTime2 = 0LL;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  /* compare */
  switch (String_compare(name1,name2,NULL,NULL))
  {
    case -1:
      return -1;
      break;
    case  1:
      return 1;
      break;
    case  0:
      if      (modifiedTime1 > modifiedTime2) return -1;
      else if (modifiedTime1 < modifiedTime2) return  1;
      else                                    return  0;
      break;
  }

  return 0;
}

/***********************************************************************\
* Name   : printList
* Purpose: sort, group and print list with entries
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printList(void)
{
  ArchiveEntryNode  *archiveEntryNode;
  ArchiveEntryTypes prevArchiveEntryType;
  String            prevArchiveName;

  /* sort list */
  List_sort(&archiveEntryList,
            (ListNodeCompareFunction)compareArchiveEntryNode,
            NULL
           );

  /* output list */
  prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_NONE;
  prevArchiveName      = NULL;
  archiveEntryNode = archiveEntryList.head;
  while (archiveEntryNode != NULL)
  {
    /* output */
    switch (archiveEntryNode->type)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        if (   globalOptions.allFlag
            || (prevArchiveEntryType != ARCHIVE_ENTRY_TYPE_FILE)
            || !String_equals(prevArchiveName,archiveEntryNode->file.fileName)
           )
        {
          printFileInfo(archiveEntryNode->archiveFileName,
                        archiveEntryNode->file.fileName,
                        archiveEntryNode->file.size,
                        archiveEntryNode->file.timeModified,
                        archiveEntryNode->file.archiveSize,
                        archiveEntryNode->file.compressAlgorithm,
                        archiveEntryNode->file.cryptAlgorithm,
                        archiveEntryNode->file.cryptType,
                        archiveEntryNode->file.fragmentOffset,
                        archiveEntryNode->file.fragmentSize
                       );
          prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_FILE;
          prevArchiveName      = archiveEntryNode->file.fileName;
        }
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        if (   globalOptions.allFlag
            || (prevArchiveEntryType != ARCHIVE_ENTRY_TYPE_IMAGE)
            || !String_equals(prevArchiveName,archiveEntryNode->image.imageName)
           )
        {
          printImageInfo(archiveEntryNode->archiveFileName,
                         archiveEntryNode->image.imageName,
                         archiveEntryNode->image.size,
                         archiveEntryNode->image.archiveSize,
                         archiveEntryNode->image.compressAlgorithm,
                         archiveEntryNode->image.cryptAlgorithm,
                         archiveEntryNode->image.cryptType,
                         archiveEntryNode->image.blockSize,
                         archiveEntryNode->image.blockOffset,
                         archiveEntryNode->image.blockCount
                        );
          prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_IMAGE;
          prevArchiveName      = archiveEntryNode->image.imageName;
        }
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        if (   globalOptions.allFlag
            || (prevArchiveEntryType != ARCHIVE_ENTRY_TYPE_DIRECTORY)
            || !String_equals(prevArchiveName,archiveEntryNode->file.fileName)
           )
        {
          printDirectoryInfo(archiveEntryNode->archiveFileName,
                             archiveEntryNode->directory.directoryName,
                             archiveEntryNode->directory.cryptAlgorithm,
                             archiveEntryNode->directory.cryptType
                            );
          prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;
          prevArchiveName      = archiveEntryNode->directory.directoryName;
        }
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        if (   globalOptions.allFlag
            || (prevArchiveEntryType != ARCHIVE_ENTRY_TYPE_LINK)
            || !String_equals(prevArchiveName,archiveEntryNode->file.fileName)
           )
        {
          printLinkInfo(archiveEntryNode->archiveFileName,
                        archiveEntryNode->link.linkName,
                        archiveEntryNode->link.destinationName,
                        archiveEntryNode->link.cryptAlgorithm,
                        archiveEntryNode->link.cryptType
                       );
          prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;
          prevArchiveName      = archiveEntryNode->link.linkName;
        }
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        if (   globalOptions.allFlag
            || (prevArchiveEntryType != ARCHIVE_ENTRY_TYPE_SPECIAL)
            || !String_equals(prevArchiveName,archiveEntryNode->file.fileName)
           )
        {
          printSpecialInfo(archiveEntryNode->archiveFileName,
                           archiveEntryNode->special.fileName,
                           archiveEntryNode->special.cryptAlgorithm,
                           archiveEntryNode->special.cryptType,
                           archiveEntryNode->special.fileSpecialType,
                           archiveEntryNode->special.major,
                           archiveEntryNode->special.minor
                          );
          prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;
          prevArchiveName      = archiveEntryNode->special.fileName;
        }
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }

    /* next entry */
    archiveEntryNode = archiveEntryNode->next;
  }
}

/*---------------------------------------------------------------------*/

Errors Command_list(StringList                      *archiveFileNameList,
                    EntryList                       *includeEntryList,
                    PatternList                     *excludePatternList,
                    JobOptions                      *jobOptions,
                    ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                    void                            *archiveGetCryptPasswordUserData
                   )
{
  String       archiveFileName;
  String       storageSpecifier;
  bool         printedInfoFlag;
  ulong        fileCount;
  Errors       failError;
  bool         retryFlag;
bool         remoteBarFlag;
//  SSHSocketList sshSocketList;
//  SSHSocketNode *sshSocketNode;
  SocketHandle socketHandle;

  assert(archiveFileNameList != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

remoteBarFlag=FALSE;

  /* init variables */
  List_init(&archiveEntryList);
  archiveFileName  = String_new();
  storageSpecifier = String_new();

  /* list archive content */
  failError = ERROR_NONE;
  while (!StringList_empty(archiveFileNameList))
  {
    StringList_getFirst(archiveFileNameList,archiveFileName);
    printedInfoFlag = FALSE;
    fileCount       = 0;

    switch (Storage_getType(archiveFileName,storageSpecifier))
    {
      case STORAGE_TYPE_FILESYSTEM:
      case STORAGE_TYPE_FTP:
      case STORAGE_TYPE_SFTP:
        {
          Errors            error;
          ArchiveInfo       archiveInfo;
          ArchiveFileInfo   archiveFileInfo;
          ArchiveEntryTypes archiveEntryType;

          /* open archive */
          error = Archive_open(&archiveInfo,
                               archiveFileName,
                               jobOptions,
                               archiveGetCryptPasswordFunction,
                               archiveGetCryptPasswordUserData
                              );
          if (error != ERROR_NONE)
          {
            printError("Cannot open file '%s' (error: %s)!\n",
                       String_cString(archiveFileName),
                       Errors_getText(error)
                      );
            if (failError == ERROR_NONE) failError = error;
            continue;
          }

          /* list contents */
          while (   !Archive_eof(&archiveInfo)
                 && (failError == ERROR_NONE)
                )
          {
            /* get next archive entry type */
            error = Archive_getNextArchiveEntryType(&archiveInfo,
                                                    &archiveFileInfo,
                                                    &archiveEntryType
                                                   );
            if (error != ERROR_NONE)
            {
              printError("Cannot not read next entry in archive '%s' (error: %s)!\n",
                         String_cString(archiveFileName),
                         Errors_getText(error)
                        );
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            switch (archiveEntryType)
            {
              case ARCHIVE_ENTRY_TYPE_FILE:
                {
                  ArchiveFileInfo    archiveFileInfo;
                  CompressAlgorithms compressAlgorithm;
                  CryptAlgorithms    cryptAlgorithm;
                  CryptTypes         cryptType;
                  String             fileName;
                  FileInfo           fileInfo;
                  uint64             fragmentOffset,fragmentSize;

                  /* open archive file */
                  fileName = String_new();
                  error = Archive_readFileEntry(&archiveInfo,
                                                &archiveFileInfo,
                                                &compressAlgorithm,
                                                &cryptAlgorithm,
                                                &cryptType,
                                                fileName,
                                                &fileInfo,
                                                &fragmentOffset,
                                                &fragmentSize
                                               );
                  if (error != ERROR_NONE)
                  {
                    printError("Cannot not read 'file' content of archive '%s' (error: %s)!\n",
                               String_cString(archiveFileName),
                               Errors_getText(error)
                              );
                    String_delete(fileName);
                    if (failError == ERROR_NONE) failError = error;
                    break;
                  }

                  if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      /* add file info to list */
                      addListFileInfo(archiveFileName,
                                      fileName,
                                      fileInfo.size,
                                      fileInfo.timeModified,
                                      archiveFileInfo.file.chunkInfoFileData.size,
                                      compressAlgorithm,
                                      cryptAlgorithm,
                                      cryptType,
                                      fragmentOffset,
                                      fragmentSize
                                     );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printHeader(archiveFileName);
                        printedInfoFlag = TRUE;
                      }

                      /* output file info */
                      printFileInfo(NULL,
                                    fileName,
                                    fileInfo.size,
                                    fileInfo.timeModified,
                                    archiveFileInfo.file.chunkInfoFileData.size,
                                    compressAlgorithm,
                                    cryptAlgorithm,
                                    cryptType,
                                    fragmentOffset,
                                    fragmentSize
                                   );
                    }
                    fileCount++;
                  }

                  /* close archive file, free resources */
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                }
                break;
              case ARCHIVE_ENTRY_TYPE_IMAGE:
                {
                  ArchiveFileInfo    archiveFileInfo;
                  CompressAlgorithms compressAlgorithm;
                  CryptAlgorithms    cryptAlgorithm;
                  CryptTypes         cryptType;
                  String             imageName;
                  DeviceInfo         deviceInfo;
                  uint64             blockOffset,blockCount;

                  /* open archive file */
                  imageName = String_new();
                  error = Archive_readImageEntry(&archiveInfo,
                                                 &archiveFileInfo,
                                                 &compressAlgorithm,
                                                 &cryptAlgorithm,
                                                 &cryptType,
                                                 imageName,
                                                 &deviceInfo,
                                                 &blockOffset,
                                                 &blockCount
                                                );
                  if (error != ERROR_NONE)
                  {
                    printError("Cannot not read 'image' content of archive '%s' (error: %s)!\n",
                               String_cString(archiveFileName),
                               Errors_getText(error)
                              );
                    String_delete(imageName);
                    if (failError == ERROR_NONE) failError = error;
                    break;
                  }

                  if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,imageName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,imageName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      /* add image info to list */
                      addListImageInfo(archiveFileName,
                                       imageName,
                                       deviceInfo.size,
                                       archiveFileInfo.image.chunkInfoImageData.size,
                                       compressAlgorithm,
                                       cryptAlgorithm,
                                       cryptType,
                                       deviceInfo.blockSize,
                                       blockOffset,
                                       blockCount
                                      );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printHeader(archiveFileName);
                        printedInfoFlag = TRUE;
                      }

                      /* output file info */
                      printImageInfo(NULL,
                                     imageName,
                                     deviceInfo.size,
                                     archiveFileInfo.image.chunkInfoImageData.size,
                                     compressAlgorithm,
                                     cryptAlgorithm,
                                     cryptType,
                                     deviceInfo.blockSize,
                                     blockOffset,
                                     blockCount
                                    );
                    }
                    fileCount++;
                  }

                  /* close archive file, free resources */
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(imageName);
                }
                break;
              case ARCHIVE_ENTRY_TYPE_DIRECTORY:
                {
                  String          directoryName;
                  CryptAlgorithms cryptAlgorithm;
                  CryptTypes      cryptType;
                  FileInfo        fileInfo;

                  /* open archive lin */
                  directoryName = String_new();
                  error = Archive_readDirectoryEntry(&archiveInfo,
                                                     &archiveFileInfo,
                                                     &cryptAlgorithm,
                                                     &cryptType,
                                                     directoryName,
                                                     &fileInfo
                                                    );
                  if (error != ERROR_NONE)
                  {
                    printError("Cannot not read 'directory' content of archive '%s' (error: %s)!\n",
                               String_cString(archiveFileName),
                               Errors_getText(error)
                              );
                    String_delete(directoryName);
                    if (failError == ERROR_NONE) failError = error;
                    break;
                  }

                  if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      /* add directory info to list */
                      addListDirectoryInfo(archiveFileName,
                                           directoryName,
                                           cryptAlgorithm,
                                           cryptType
                                          );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printHeader(archiveFileName);
                        printedInfoFlag = TRUE;
                      }

                      /* output file info */
                      printDirectoryInfo(NULL,
                                         directoryName,
                                         cryptAlgorithm,
                                         cryptType
                                        );
                    }
                    fileCount++;
                  }

                  /* close archive file, free resources */
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(directoryName);
                }
                break;
              case ARCHIVE_ENTRY_TYPE_LINK:
                {
                  CryptAlgorithms cryptAlgorithm;
                  CryptTypes      cryptType;
                  String          linkName;
                  String          fileName;
                  FileInfo        fileInfo;

                  /* open archive lin */
                  linkName = String_new();
                  fileName = String_new();
                  error = Archive_readLinkEntry(&archiveInfo,
                                                &archiveFileInfo,
                                                &cryptAlgorithm,
                                                &cryptType,
                                                linkName,
                                                fileName,
                                                &fileInfo
                                               );
                  if (error != ERROR_NONE)
                  {
                    printError("Cannot not read 'link' content of archive '%s' (error: %s)!\n",
                               String_cString(archiveFileName),
                               Errors_getText(error)
                              );
                    String_delete(fileName);
                    String_delete(linkName);
                    if (failError == ERROR_NONE) failError = error;
                    break;
                  }

                  if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      /* add link info to list */
                      addListLinkInfo(archiveFileName,
                                      linkName,
                                      fileName,
                                      cryptAlgorithm,
                                      cryptType
                                     );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printHeader(archiveFileName);
                        printedInfoFlag = TRUE;
                      }

                      /* output file info */
                      printLinkInfo(NULL,
                                    linkName,
                                    fileName,
                                    cryptAlgorithm,
                                    cryptType
                                   );
                    }
                    fileCount++;
                  }

                  /* close archive file, free resources */
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                  String_delete(linkName);
                }
                break;
              case ARCHIVE_ENTRY_TYPE_SPECIAL:
                {
                  CryptAlgorithms cryptAlgorithm;
                  CryptTypes      cryptType;
                  String          fileName;
                  FileInfo        fileInfo;

                  /* open archive lin */
                  fileName = String_new();
                  error = Archive_readSpecialEntry(&archiveInfo,
                                                   &archiveFileInfo,
                                                   &cryptAlgorithm,
                                                   &cryptType,
                                                   fileName,
                                                   &fileInfo
                                                  );
                  if (error != ERROR_NONE)
                  {
                    printError("Cannot not read 'special' content of archive '%s' (error: %s)!\n",
                               String_cString(archiveFileName),
                               Errors_getText(error)
                              );
                    String_delete(fileName);
                    if (failError == ERROR_NONE) failError = error;
                    break;
                  }

                  if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      /* add special info to list */
                      addListSpecialInfo(archiveFileName,
                                         fileName,
                                         cryptAlgorithm,
                                         cryptType,
                                         fileInfo.specialType,
                                         fileInfo.major,
                                         fileInfo.minor
                                        );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printHeader(archiveFileName);
                        printedInfoFlag = TRUE;
                      }

                      /* output file info */
                      printSpecialInfo(NULL,
                                       fileName,
                                       cryptAlgorithm,
                                       cryptType,
                                       fileInfo.specialType,
                                       fileInfo.major,
                                       fileInfo.minor
                                      );
                    }
                    fileCount++;
                  }

                  /* close archive file, free resources */
                  Archive_closeEntry(&archiveFileInfo);
                  String_delete(fileName);
                }
                break;
              default:
                #ifndef NDEBUG
                  HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                #endif /* NDEBUG */
                break; /* not reached */
            }
          }

          /* close archive */
          Archive_close(&archiveInfo);
        }
        break;
      case STORAGE_TYPE_SSH:
      case STORAGE_TYPE_SCP:
        {
          String               loginName;
          String               hostName;
          uint                 hostPort;
          String               archiveFileName;
          SSHServer            sshServer;
          NetworkExecuteHandle networkExecuteHandle;
          String               line;
          uint                 majorVersion,minorVersion;
          Errors               error;
          int                  id,errorCode;
          bool                 completedFlag;
          String               fileName,directoryName,linkName;
          uint64               fileSize;
          uint64               timeModified;
          uint64               archiveFileSize;
          uint64               fragmentOffset,fragmentLength;
          CompressAlgorithms   compressAlgorithm;
          CryptAlgorithms      cryptAlgorithm;
          CryptTypes           cryptType;
          FileSpecialTypes     fileSpecialType;
          ulong                major;
          ulong                minor;
          int                  exitcode;

          /* parse storage string */
          loginName       = String_new();
          hostName        = String_new();
          hostPort        = 0;
          archiveFileName = String_new();
          if (!Storage_parseSSHSpecifier(storageSpecifier,
                                         loginName,
                                         hostName,
                                         &hostPort,
                                         archiveFileName
                                        )
             )
          {
            String_delete(archiveFileName);
            String_delete(hostName);
            String_delete(loginName);
            printError("Cannot not parse storage name '%s'!\n",
                       String_cString(storageSpecifier)
                      );
            if (failError == ERROR_NONE) failError = ERROR_INIT_TLS;
            break;
          }

          /* start remote BAR via SSH (if not already started) */
          if (!remoteBarFlag)
          {
            getSSHServer(hostName,jobOptions,&sshServer);
            if (String_empty(loginName)) String_set(loginName,sshServer.loginName);
            if (hostPort == 0) hostPort = sshServer.port;
            error = Network_connect(&socketHandle,
                                    SOCKET_TYPE_SSH,
                                    hostName,
                                    hostPort,
                                    loginName,
                                    sshServer.password,
                                    sshServer.publicKeyFileName,
                                    sshServer.privateKeyFileName,
                                    0
                                   );
            if (error != ERROR_NONE)
            {
              printError("Cannot not connecto to '%s:%d' (error: %s)!\n",
                         String_cString(hostName),
                         hostPort,
                         Errors_getText(error)
                        );
              String_delete(archiveFileName);
              String_delete(hostName);
              String_delete(loginName);
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            remoteBarFlag = TRUE;
          }

          /* start remote BAR in batch mode */
          line = String_format(String_new(),"%s --batch",!String_empty(globalOptions.remoteBARExecutable)?String_cString(globalOptions.remoteBARExecutable):"bar");
fprintf(stderr,"%s,%d: line=%s\n",__FILE__,__LINE__,String_cString(line));
          error = Network_execute(&networkExecuteHandle,
                                  &socketHandle,
                                  NETWORK_EXECUTE_IO_MASK_STDOUT|NETWORK_EXECUTE_IO_MASK_STDERR,
                                  String_cString(line)
                                 );
          if (error != ERROR_NONE)
          {
            printError("Cannot not execute remote BAR program '%s' (error: %s)!\n",
                       String_cString(line),
                       Errors_getText(error)
                      );
            String_delete(line);
            String_delete(archiveFileName);
            String_delete(hostName);
            String_delete(loginName);
            if (failError == ERROR_NONE) failError = error;
            break;
          }
          if (Network_executeEOF(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,60*1000))
          {
            Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,line,0);
            exitcode = Network_terminate(&networkExecuteHandle);
            printError("No response from remote BAR program (error: %s, exitcode %d)!\n",!String_empty(line)?String_cString(line):"unknown",exitcode);
            String_delete(line);
            String_delete(archiveFileName);
            String_delete(hostName);
            String_delete(loginName);
            if (failError == ERROR_NONE) failError = ERROR_NO_RESPONSE;
            break;
          }
          Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,line,0);
fprintf(stderr,"%s,%d: line=%s\n",__FILE__,__LINE__,String_cString(line));
          if (!String_parse(line,STRING_BEGIN,"BAR VERSION %d %d",NULL,&majorVersion,&minorVersion))
          {
            Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,line,0);
            exitcode = Network_terminate(&networkExecuteHandle);
            printError("Invalid response from remote BAR program (error: %s, exitcode %d)!\n",!String_empty(line)?String_cString(line):"unknown",exitcode);
            String_delete(line);
            String_delete(archiveFileName);
            String_delete(hostName);
            String_delete(loginName);
            if (failError == ERROR_NONE) failError = ERROR_INVALID_RESPONSE;
            break;
          }
          String_delete(line);

          /* read archive content */
          line          = String_new();
          fileName      = String_new();
          directoryName = String_new();
          linkName      = String_new();
          do
          {
            /* send list archive command */
            if (!Password_empty(jobOptions->cryptPassword))
            {
              String_format(String_clear(line),"1 SET crypt-password %'s",jobOptions->cryptPassword);
              Network_executeWriteLine(&networkExecuteHandle,line);
            }
            String_format(String_clear(line),"2 ARCHIVE_LIST %S",archiveFileName);
            Network_executeWriteLine(&networkExecuteHandle,line);
            Network_executeSendEOF(&networkExecuteHandle);

            /* list contents */
            while (!Network_executeEOF(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,60*1000))
            {
              /* read line */
              Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,line,60*1000);
fprintf(stderr,"%s,%d: line=%s\n",__FILE__,__LINE__,String_cString(line));

              /* parse and output list */
              if      (String_parse(line,
                                    STRING_BEGIN,
                                    "%d %d %y FILE %llu %llu %llu %llu %d %d %d %S",
                                    NULL,
                                    &id,
                                    &errorCode,
                                    &completedFlag,
                                    &fileSize,
                                    &timeModified,
                                    &archiveFileSize,
                                    &fragmentOffset,
                                    &fragmentLength,
                                    &compressAlgorithm,
                                    &cryptAlgorithm,
                                    &cryptType,
                                    fileName
                                   )
                      )
              {
                if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                    && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                   )
                {
                  if (globalOptions.groupFlag)
                  {
                      /* add file info to list */
                    addListFileInfo(archiveFileName,
                                    fileName,
                                    fileSize,
                                    timeModified,
                                    archiveFileSize,
                                    compressAlgorithm,
                                    cryptAlgorithm,
                                    cryptType,
                                    fragmentOffset,
                                    fragmentLength
                                   );
                  }
                  else
                  {
                    if (!printedInfoFlag)
                    {
                      printHeader(archiveFileName);
                      printedInfoFlag = TRUE;
                    }

                    /* output file info */
                    printFileInfo(NULL,
                                  fileName,
                                  fileSize,
                                  timeModified,
                                  archiveFileSize,
                                  compressAlgorithm,
                                  cryptAlgorithm,
                                  cryptType,
                                  fragmentOffset,
                                  fragmentLength
                                 );
                  }
                  fileCount++;
                }
              }
              else if (String_parse(line,
                                    STRING_BEGIN,
                                    "%d %d %d DIRECTORY %d %S",
                                    NULL,
                                    &id,
                                    &errorCode,
                                    &completedFlag,
                                    &cryptAlgorithm,
                                    &cryptType,
                                    directoryName
                                   )
                      )
              {
                if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                    && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
                   )
                {
                  if (globalOptions.groupFlag)
                  {
                    /* add directory info to list */
                    addListDirectoryInfo(archiveFileName,
                                         directoryName,
                                         cryptAlgorithm,
                                         cryptType
                                        );
                  }
                  else
                  {
                    if (!printedInfoFlag)
                    {
                      printHeader(archiveFileName);
                      printedInfoFlag = TRUE;
                    }

                    /* output file info */
                    printDirectoryInfo(NULL,
                                       directoryName,
                                       cryptAlgorithm,
                                       cryptType
                                      );
                  }
                  fileCount++;
                }
              }
              else if (String_parse(line,
                                    STRING_BEGIN,
                                    "%d %d %d LINK %d %S %S",
                                    NULL,
                                    &id,
                                    &errorCode,
                                    &completedFlag,
                                    &cryptAlgorithm,
                                    &cryptType,
                                    linkName,
                                    fileName
                                   )
                      )
              {
                if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                    && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
                   )
                {
                  if (globalOptions.groupFlag)
                  {
                    /* add linkinfo to list */
                    addListLinkInfo(archiveFileName,
                                    linkName,
                                    fileName,
                                    cryptAlgorithm,
                                    cryptType
                                   );
                  }
                  else
                  {
                    if (!printedInfoFlag)
                    {
                      printHeader(archiveFileName);
                      printedInfoFlag = TRUE;
                    }

                    /* output file info */
                    printLinkInfo(NULL,
                                  linkName,
                                  fileName,
                                  cryptAlgorithm,
                                  cryptType
                                 );
                  }
                  fileCount++;
                }
              }
              else if (String_parse(line,
                                    STRING_BEGIN,
                                    "%d %d %d SPECIAL %d %d %ld %ld %S",
                                    NULL,
                                    &id,
                                    &errorCode,
                                    &completedFlag,
                                    &cryptAlgorithm,
                                    &cryptType,
                                    &fileSpecialType,
                                    &major,
                                    &minor,
                                    fileName
                                   )
                      )
              {
                if (   (List_empty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                    && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
                   )
                {
                  if (globalOptions.groupFlag)
                  {
                    /* add special info to list */
                    addListSpecialInfo(archiveFileName,
                                       fileName,
                                       cryptAlgorithm,
                                       cryptType,
                                       fileSpecialType,
                                       major,
                                       minor
                                      );
                  }
                  else
                  {
                    if (!printedInfoFlag)
                    {
                      printHeader(archiveFileName);
                      printedInfoFlag = TRUE;
                    }

                    /* output file info */
                    printSpecialInfo(NULL,
                                     fileName,
                                     cryptAlgorithm,
                                     cryptType,
                                     fileSpecialType,
                                     major,
                                     minor
                                    );
                  }
                  fileCount++;
                }
              }
              else
              {
fprintf(stderr,"%s,%d: ERROR %s\n",__FILE__,__LINE__,String_cString(line));
              }
            }
            while (!Network_executeEOF(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,60*1000))
            {
Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,line,0);
if (String_length(line)>0) fprintf(stderr,"%s,%d: error=%s\n",__FILE__,__LINE__,String_cString(line));
            }

            retryFlag = FALSE;
#if 0
            if ((error == ERROR_CORRUPT_DATA) && !inputPasswordFlag)
            {
              inputCryptPassword(&jobOptions->cryptPassword);
              retryFlag         = TRUE;
              inputPasswordFlag = TRUE;
            }
#endif /* 0 */
          }
          while ((error != ERROR_NONE) && retryFlag);
          String_delete(linkName);
          String_delete(directoryName);
          String_delete(fileName);
          String_delete(line);

          /* close connection */
          exitcode = Network_terminate(&networkExecuteHandle);
          if (exitcode != 0)
          {
            printError("Remote BAR program return exitcode %d!\n",exitcode);
            if (failError == ERROR_NONE) failError = ERROR_NETWORK_EXECUTE_FAIL;
          }

          /* free resources */
          String_delete(archiveFileName);
          String_delete(hostName);
          String_delete(loginName);
        }
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }
    if (printedInfoFlag)
    {
      printFooter(fileCount);
    }
  }

  /* output grouped list */
  if (globalOptions.groupFlag)
  {
    printHeader(NULL);
    printList();
//    printFooter()
  }

  /* free resources */
  String_delete(storageSpecifier);
  String_delete(archiveFileName);
  List_done(&archiveEntryList,(ListNodeFreeFunction)freeArchiveEntryNode,NULL);

  return failError;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
