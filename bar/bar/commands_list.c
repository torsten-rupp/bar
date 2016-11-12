/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive list functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

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
#include "autofree.h"
#include "strings.h"
#include "stringmaps.h"
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

#define DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_NORMAL_LONG          "%type:-8s %user:-12s %group:-12s %permission:-10s %size:-12s %dateTime:-32s %part:-26s %compress:-15s %ratio:-7s  %crypt:-10s %name:s"
#define DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_GROUP_LONG           "%storageName:-20s" DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_NORMAL_LONG
#define DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_NORMAL               "%type:-8s %size:-12s %dateTime:-32s %name:s"
#define DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_GROUP                "%storageName:-20s" DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_NORMAL

#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_FILE_LONG           "%type:-8s %user:-12s %group:-12s %permission:-10s %size:12s %dateTime:-32S %partFrom:12llu..%partTo:12llu %compress:-15S %ratio:7.1f%% %crypt:-10S %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_IMAGE_LONG          "%type:-8s %user:-12s %group:-12s %permission:-10s %size:12s %        :-32s %partFrom:12llu..%partTo:12llu %compress:-15S %ratio:7.1f%% %crypt:-10S %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_DIR_LONG            "%type:-8s %user:-12s %group:-12s %permission:-10s %    :12s %dateTime:-32S %                         :26s %        :-15s %       :7s  %crypt:-10S %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_LINK_LONG           "%type:-8s %user:-12s %group:-12s %permission:-10s %    :12s %dateTime:-32S %                         :26s %        :-15s %       :7s  %crypt:-10S %name:S -> %destinationName:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_HARDLINK_LONG       "%type:-8s %user:-12s %group:-12s %permission:-10s %size:12s %dateTime:-32S %partFrom:12llu..%partTo:12llu %compress:-15S %ratio:7.1f%% %crypt:-10S %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_CHAR_LONG   "%type:-8s %user:-12s %group:-12s %permission:-10s %    :12s %        :-32s %                         :26s %        :-15s %       :7s  %crypt:-10S %name:S, %major:llu %minor:llu"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_BLOCK_LONG  "%type:-8s %user:-12s %group:-12s %permission:-10s %    :12s %        :-32s %                         :26s %        :-15s %       :7s  %crypt:-10S %name:S, %major:llu %minor:llu"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_FIFO_LONG   "%type:-8s %user:-12s %group:-12s %permission:-10s %    :12s %        :-32s %                         :26s %        :-15s %       :7s  %crypt:-10S %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_SOCKET_LONG "%type:-8s %user:-12s %group:-12s %permission:-10s %    :12s %        :-32s %                         :26s %        :-15s %       :7s  %crypt:-10S %name:S"

#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_FILE_LONG            "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_FILE_LONG
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_IMAGE_LONG           "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_IMAGE_LONG
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_DIR_LONG             "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_DIR_LONG
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_LINK_LONG            "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_LINK_LONG
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_HARDLINK_LONG        "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_HARDLINK_LONG
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_CHAR_LONG    "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_CHAR_LONG
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_BLOCK_LONG   "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_BLOCK_LONG
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_FIFO_LONG    "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_FIFO_LONG
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_SOCKET_LONG  "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_SOCKET_LONG

#define DEFAULT_ARCHIVE_LIST_FORMAT_SOURCE                     "                                                                     source: %deltaSource:S, %deltaSourceSize:s"

#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_FILE                "%type:-8s %size:12s %dateTime:-32S %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_IMAGE               "%type:-8s %size:12s %        :-32s %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_DIR                 "%type:-8s %    :12s %dateTime:-32S %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_LINK                "%type:-8s %    :12s %dateTime:-32S %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_HARDLINK            "%type:-8s %size:12s %dateTime:-32S %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_CHAR        "%type:-8s %    :12s %        :-32s %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_BLOCK       "%type:-8s %    :12s %        :-32s %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_FIFO        "%type:-8s %    :12s %        :-32s %name:S"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_SOCKET      "%type:-8s %    :12s %        :-32s %name:S"

#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_FILE                 "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_FILE
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_IMAGE                "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_IMAGE
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_DIR                  "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_DIR
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_LINK                 "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_LINK
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_HARDLINK             "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_HARDLINK
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_CHAR         "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_CHAR
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_BLOCK        "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_BLOCK
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_FIFO         "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_FIFO
#define DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_SOCKET       "%storageName:-20S" DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_SOCKET

#define DEFAULT_DIRECTORY_LIST_FORMAT_TITLE                    "%type:-8s %size:-12s %dateTime:-32s %name:s"

#define DEFAULT_DIRECTORY_LIST_FORMAT                          "%type:-8s %size:12s %dateTime:-32S %name:S"

/***************************** Datatypes *******************************/

// archive content node
typedef struct ArchiveContentNode
{
  LIST_NODE_HEADER(struct ArchiveContentNode);

  String            storageName;
  ArchiveEntryTypes type;
  union
  {
    struct
    {
      String             fileName;
      uint64             size;
      uint64             timeModified;
      uint32             userId;
      uint32             groupId;
      FilePermission     permission;
      uint64             archiveSize;
      CompressAlgorithms deltaCompressAlgorithm;
      CompressAlgorithms byteCompressAlgorithm;
      CryptAlgorithms    cryptAlgorithm;
      CryptTypes         cryptType;
      String             deltaSourceName;
      uint64             deltaSourceSize;
      uint64             fragmentOffset;
      uint64             fragmentSize;
    } file;
    struct
    {
      String             imageName;
      uint64             size;
      uint64             archiveSize;
      CompressAlgorithms deltaCompressAlgorithm;
      CompressAlgorithms byteCompressAlgorithm;
      CryptAlgorithms    cryptAlgorithm;
      CryptTypes         cryptType;
      String             deltaSourceName;
      uint64             deltaSourceSize;
      uint               blockSize;
      uint64             blockOffset;
      uint64             blockCount;
    } image;
    struct
    {
      String          directoryName;
      uint64          timeModified;
      uint32          userId;
      uint32          groupId;
      FilePermission  permission;
      CryptAlgorithms cryptAlgorithm;
      CryptTypes      cryptType;
    } directory;
    struct
    {
      String          linkName;
      String          destinationName;
      uint64          timeModified;
      uint32          userId;
      uint32          groupId;
      FilePermission  permission;
      CryptAlgorithms cryptAlgorithm;
      CryptTypes      cryptType;
    } link;
    struct
    {
      String             fileName;
      uint64             size;
      uint64             timeModified;
      uint32             userId;
      uint32             groupId;
      FilePermission     permission;
      uint64             archiveSize;
      CompressAlgorithms deltaCompressAlgorithm;
      CompressAlgorithms byteCompressAlgorithm;
      CryptAlgorithms    cryptAlgorithm;
      CryptTypes         cryptType;
      String             deltaSourceName;
      uint64             deltaSourceSize;
      uint64             fragmentOffset;
      uint64             fragmentSize;
    } hardLink;
    struct
    {
      String           fileName;
      uint32           userId;
      uint32           groupId;
      FilePermission   permission;
      CryptAlgorithms  cryptAlgorithm;
      CryptTypes       cryptType;
      FileSpecialTypes fileSpecialType;
      uint32           major;
      uint32           minor;
    } special;
  };

  String       name;
  SocketHandle socketHandle;
} ArchiveContentNode;

// archive content list
typedef struct
{
  LIST_HEADER(ArchiveContentNode);
} ArchiveContentList;

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
LOCAL ArchiveContentList archiveContentList;

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
* Name   : printArchiveListHeader
* Purpose: print archive list header
* Input  : storageName - storage file name or NULL if archive should not
*                        be printed
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printArchiveListHeader(ConstString storageName)
{
  const TextMacro MACROS[] =
  {
    TEXT_MACRO_CSTRING("%storageName","Storage",   NULL),
    TEXT_MACRO_CSTRING("%type",       "Type",      NULL),
    TEXT_MACRO_CSTRING("%size",       "Size",      NULL),
    TEXT_MACRO_CSTRING("%dateTime",   "Date/Time", NULL),
    TEXT_MACRO_CSTRING("%user",       "User",      NULL),
    TEXT_MACRO_CSTRING("%group",      "Group",     NULL),
    TEXT_MACRO_CSTRING("%permission", "Permission",NULL),
    TEXT_MACRO_CSTRING("%part",       "Part",      NULL),
    TEXT_MACRO_CSTRING("%compress",   "Compress",  NULL),
    TEXT_MACRO_CSTRING("%ratio",      "Ratio",     NULL),
    TEXT_MACRO_CSTRING("%crypt",      "Crypt",     NULL),
    TEXT_MACRO_CSTRING("%name",       "Name",      NULL)
  };

  String     line;
  const char *template;

  if (!globalOptions.noHeaderFooterFlag)
  {
    // init variables
    line = String_new();

    // header
    if (storageName != NULL)
    {
//TODO: printable storaeg name?
      printInfo(0,"List storage '%s':\n",String_cString(storageName));
      printInfo(0,"\n");
    }

    // title line
    if (globalOptions.longFormatFlag)
    {
      template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_GROUP_LONG : DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_NORMAL_LONG;
    }
    else
    {
      template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_GROUP : DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_NORMAL;
    }
    printInfo(0,
              "%s\n",
              String_cString(Misc_expandMacros(line,
                                               template,
                                               EXPAND_MACRO_MODE_STRING,
                                               MACROS,SIZE_OF_ARRAY(MACROS),
                                               TRUE
                                              )
                            )
             );
    printInfo(0,
              "%s\n",
              String_cString(String_fillChar(line,
                                             Misc_getConsoleColumns(),
                                             '-'
                                            )
                            )
             );

    // free resources
    String_delete(line);
  }
}

/***********************************************************************\
* Name   : printArchiveListFooter
* Purpose: print archive list footer
* Input  : fileCount - number of files listed
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printArchiveListFooter(ulong fileCount)
{
  String line;

  if (!globalOptions.noHeaderFooterFlag)
  {
    line = String_new();

    String_fillChar(line,Misc_getConsoleColumns(),'-');
    printInfo(0,"%s\n",String_cString(line));
    printInfo(0,"%lu %s\n",fileCount,(fileCount == 1) ? "entry" : "entries");
    printInfo(0,"\n");

    String_delete(line);
  }
}

/***********************************************************************\
* Name   : printFileInfo
* Purpose: print file information
* Input  : storageName            - storage name or NULL if storage name
*                                   should not be printed
*          fileName               - file name
*          size                   - file size [bytes]
*          timeModified           - file modified time
*          userId                 - user id
*          groupId                - group id
*          permission             - permissions
*          archiveSize            - archive size [bytes]
*          deltaCompressAlgorithm - used delta compress algorithm
*          byteCompressAlgorithm  - used data compress algorithm
*          cryptAlgorithm         - used crypt algorithm
*          cryptType              - crypt type; see CRYPT_TYPES
*          deltaSourceName        - delta source name
*          deltaSourceSize        - delta source size [bytes]
*          fragmentOffset         - fragment offset (0..n-1)
*          fragmentSize           - fragment length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printFileInfo(ConstString        storageName,
                         ConstString        fileName,
                         uint64             size,
                         uint64             timeModified,
                         uint32             userId,
                         uint32             groupId,
                         FilePermission     permission,
                         uint64             archiveSize,
                         CompressAlgorithms deltaCompressAlgorithm,
                         CompressAlgorithms byteCompressAlgorithm,
                         CryptAlgorithms    cryptAlgorithm,
                         CryptTypes         cryptType,
                         ConstString        deltaSourceName,
                         uint64             deltaSourceSize,
                         uint64             fragmentOffset,
                         uint64             fragmentSize
                        )
{
  String     dateTimeString;
  String     compressString;
  String     cryptString;
  String     line;
  double     ratio;
  char       sizeString[16];
  char       userName[12],groupName[12];
  char       permissionString[10];
  char       deltaSourceSizeString[16];
  const char *template;
  TextMacro  textMacros[15];

  assert(fileName != NULL);

  // init variables
  dateTimeString = String_new();
  compressString = String_new();
  cryptString    = String_new();
  line           = String_new();

  // format
  Misc_formatDateTime(dateTimeString,timeModified,NULL);

  if (globalOptions.humanFormatFlag)
  {
    getHumanSizeString(sizeString,sizeof(sizeString),size);
    getHumanSizeString(deltaSourceSizeString,sizeof(deltaSourceSizeString),deltaSourceSize);
  }
  else
  {
    snprintf(sizeString,sizeof(sizeString),"%llu",size);
    snprintf(deltaSourceSizeString,sizeof(deltaSourceSizeString),"%llu",deltaSourceSize);
  }

  if (globalOptions.numericUIDGIDFlag)
  {
    snprintf(userName,sizeof(userName),"%d",userId);
    snprintf(groupName,sizeof(groupName),"%d",groupId);
  }
  else
  {
    File_userIdToUserName(userName,sizeof(userName),userId);
    File_groupIdToGroupName(groupName,sizeof(groupName),groupId);
  }
  if (globalOptions.numericPermissionFlag)
  {
    snprintf(permissionString,sizeof(permissionString),"%4o",permission & FILE_PERMISSION_ALL);
  }
  else
  {
    File_permissionToString(permissionString,sizeof(permissionString),permission);
  }

  if (globalOptions.longFormatFlag)
  {
    template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_FILE_LONG : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_FILE_LONG;
    if      ((Compress_isCompressed(deltaCompressAlgorithm) && Compress_isCompressed(byteCompressAlgorithm)))
    {
      String_format(compressString,
                    "%s+%s",
                    Compress_algorithmToString(deltaCompressAlgorithm),
                    Compress_algorithmToString(byteCompressAlgorithm)
                   );
    }
    else if (Compress_isCompressed(deltaCompressAlgorithm))
    {
      String_format(compressString,
                    "%s",
                    Compress_algorithmToString(deltaCompressAlgorithm)
                   );
    }
    else if (Compress_isCompressed(byteCompressAlgorithm))
    {
      String_format(compressString,
                    "%s",
                    Compress_algorithmToString(byteCompressAlgorithm)
                   );
    }
    else
    {
      String_format(compressString,
                    "%s",
                    Compress_algorithmToString(deltaCompressAlgorithm)
                   );
    }

    String_format(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
  }
  else
  {
    template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_FILE : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_FILE;
  }

  if (   (   Compress_isCompressed(deltaCompressAlgorithm)
          || Compress_isCompressed(byteCompressAlgorithm)
         )
      && (fragmentSize > 0LL)
     )
  {
    ratio = 100.0-(double)archiveSize*100.0/(double)fragmentSize;
  }
  else
  {
    ratio = 0.0;
  }

  TEXT_MACRO_N_STRING   (textMacros[ 0],"%storageName",    storageName,                                                          NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 1],"%type",           "FILE",                                                               NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 2],"%size",           sizeString,                                                           NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 3],"%dateTime",       dateTimeString,                                                       NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 4],"%user",           userName,                                                             NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 5],"%group",          groupName,                                                            NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 6],"%permission",     permissionString,                                                     NULL);
  TEXT_MACRO_N_INTEGER64(textMacros[ 7],"%partFrom",       fragmentOffset,                                                       NULL);
  TEXT_MACRO_N_INTEGER64(textMacros[ 8],"%partTo",         (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1 : fragmentOffset,NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 9],"%compress",       compressString,                                                       NULL);
  TEXT_MACRO_N_DOUBLE   (textMacros[10],"%ratio",          ratio,                                                                NULL);
  TEXT_MACRO_N_STRING   (textMacros[11],"%crypt",          cryptString,                                                          NULL);
  TEXT_MACRO_N_STRING   (textMacros[12],"%name",           fileName,                                                             NULL);
  TEXT_MACRO_N_STRING   (textMacros[13],"%deltaSourceName",deltaSourceName,                                                      NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[14],"%deltaSourceSize",deltaSourceSizeString,                                                NULL);

  // print
  printInfo(0,
            "%s\n",
            String_cString(Misc_expandMacros(line,
                                             template,
                                             EXPAND_MACRO_MODE_STRING,
                                             textMacros,SIZE_OF_ARRAY(textMacros),
                                             TRUE
                                            )
                          )
           );
  if (Compress_isCompressed(deltaCompressAlgorithm) && isPrintInfo(2))
  {
    printInfo(0,
              "%s\n",
              String_cString(Misc_expandMacros(line,
                                               DEFAULT_ARCHIVE_LIST_FORMAT_SOURCE,
                                               EXPAND_MACRO_MODE_STRING,
                                               textMacros,SIZE_OF_ARRAY(textMacros),
                                               TRUE
                                              )
                            )
             );
  }

  // free resources
  String_delete(line);
  String_delete(cryptString);
  String_delete(compressString);
  String_delete(dateTimeString);
}

/***********************************************************************\
* Name   : printImageInfo
* Purpose: print image information
* Input  : storageName            - storage name or NULL if storage name
*                                   should not be printed
*          imageName              - image name
*          size                   - image size [bytes]
*          archiveSize            - archive size [bytes]
*          deltaCompressAlgorithm - used delta compress algorithm
*          byteCompressAlgorithm  - used data compress algorithm
*          cryptAlgorithm         - used crypt algorithm
*          cryptType              - crypt type; see CRYPT_TYPES
*          deltaSourceName        - delta source name
*          deltaSourceSize        - delta source size [bytes]
*          blockSize              - block size :bytes]
*          blockOffset            - block offset (0..n-1)
*          blockCount             - number of blocks
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printImageInfo(ConstString        storageName,
                          ConstString       imageName,
                          uint64             size,
                          uint64             archiveSize,
                          CompressAlgorithms deltaCompressAlgorithm,
                          CompressAlgorithms byteCompressAlgorithm,
                          CryptAlgorithms    cryptAlgorithm,
                          CryptTypes         cryptType,
                          ConstString        deltaSourceName,
                          uint64             deltaSourceSize,
                          uint               blockSize,
                          uint64             blockOffset,
                          uint64             blockCount
                         )
{
  String     compressString;
  String     cryptString;
  String     line;
  double     ratio;
  char       sizeString[16];
  char       deltaSourceSizeString[16];
  const char *template;
  TextMacro  textMacros[11];

  assert(imageName != NULL);

  // init variables
  compressString = String_new();
  cryptString    = String_new();
  line           = String_new();

  // format
  if (globalOptions.humanFormatFlag)
  {
    getHumanSizeString(sizeString,sizeof(sizeString),size);
    getHumanSizeString(deltaSourceSizeString,sizeof(deltaSourceSizeString),deltaSourceSize);
  }
  else
  {
    snprintf(sizeString,sizeof(sizeString),"%llu",size);
    snprintf(deltaSourceSizeString,sizeof(deltaSourceSizeString),"%llu",deltaSourceSize);
  }

  if (globalOptions.longFormatFlag)
  {
    template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_IMAGE_LONG : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_IMAGE_LONG;
    if      ((Compress_isCompressed(deltaCompressAlgorithm) && Compress_isCompressed(byteCompressAlgorithm)))
    {
      String_format(compressString,
                    "%s+%s",
                    Compress_algorithmToString(deltaCompressAlgorithm),
                    Compress_algorithmToString(byteCompressAlgorithm)
                   );
    }
    else if (Compress_isCompressed(deltaCompressAlgorithm))
    {
      String_format(compressString,
                    "%s",
                    Compress_algorithmToString(deltaCompressAlgorithm)
                   );
    }
    else if (Compress_isCompressed(byteCompressAlgorithm))
    {
      String_format(compressString,
                    "%s",
                    Compress_algorithmToString(byteCompressAlgorithm)
                   );
    }
    else
    {
      String_format(compressString,
                    "%s",
                    Compress_algorithmToString(deltaCompressAlgorithm)
                   );
    }

    String_format(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
  }
  else
  {
    template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_IMAGE : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_IMAGE;
  }

  if (   (   Compress_isCompressed(deltaCompressAlgorithm)
          || Compress_isCompressed(byteCompressAlgorithm)
         )
      && (blockCount > 0LL)
     )
  {
    ratio = 100.0-(double)archiveSize*100.0/(blockCount*(uint64)blockSize);
  }
  else
  {
    ratio = 0.0;
  }

  TEXT_MACRO_N_STRING   (textMacros[ 0],"%storageName",    storageName,                                                          NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 1],"%type",           "IMAGE",                                                              NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 2],"%size",           sizeString,                                                           NULL);
  TEXT_MACRO_N_INTEGER64(textMacros[ 3],"%partFrom",       blockOffset*(uint64)blockSize,                                        NULL);
  TEXT_MACRO_N_INTEGER64(textMacros[ 4],"%partTo",         (blockOffset+blockCount)*(uint64)blockSize-((blockCount > 0) ? 1 : 0),NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 5],"%compress",       compressString,                                                       NULL);
  TEXT_MACRO_N_DOUBLE   (textMacros[ 6],"%ratio",          ratio,                                                                NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 7],"%crypt",          cryptString,                                                          NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 8],"%name",           imageName,                                                            NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 9],"%deltaSourceName",deltaSourceName,                                                      NULL);
  TEXT_MACRO_N_STRING   (textMacros[10],"%deltaSourceSize",deltaSourceSizeString,                                                NULL);

  // print
  printInfo(0,
            "%s\n",
            String_cString(Misc_expandMacros(line,
                                             template,
                                             EXPAND_MACRO_MODE_STRING,
                                             textMacros,SIZE_OF_ARRAY(textMacros),
                                             TRUE
                                            )
                          )
           );
  if (Compress_isCompressed(deltaCompressAlgorithm) && isPrintInfo(2))
  {
    printInfo(0,
              "%s\n",
              String_cString(Misc_expandMacros(line,
                                               DEFAULT_ARCHIVE_LIST_FORMAT_SOURCE,
                                               EXPAND_MACRO_MODE_STRING,
                                               textMacros,SIZE_OF_ARRAY(textMacros),
                                               TRUE
                                              )
                            )
             );
  }

  // free resources
  String_delete(line);
  String_delete(cryptString);
  String_delete(compressString);
}

/***********************************************************************\
* Name   : printDirectoryInfo
* Purpose: print directory information
* Input  : storageName     - storage name or NULL if storage name should
*                            not be printed
*          directoryName   - directory name
*          timeModified    - file modified time
*          userId          - user id
*          groupId         - group id
*          permission      - permissions
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printDirectoryInfo(ConstString     storageName,
                              ConstString     directoryName,
                              uint64          timeModified,
                              uint32          userId,
                              uint32          groupId,
                              FilePermission  permission,
                              CryptAlgorithms cryptAlgorithm,
                              CryptTypes      cryptType
                             )
{
  String     dateTimeString;
  String     cryptString;
  String     line;
  char       userName[12],groupName[12];
  char       permissionString[10];
  const char *template;
  TextMacro  textMacros[8];

  assert(directoryName != NULL);

  // init variables
  dateTimeString = String_new();
  cryptString    = String_new();
  line           = String_new();

  // format
  Misc_formatDateTime(dateTimeString,timeModified,NULL);

  if (globalOptions.numericUIDGIDFlag)
  {
    snprintf(userName,sizeof(userName),"%d",userId);
    snprintf(groupName,sizeof(groupName),"%d",groupId);
  }
  else
  {
    File_userIdToUserName(userName,sizeof(userName),userId);
    File_groupIdToGroupName(groupName,sizeof(groupName),groupId);
  }
  if (globalOptions.numericPermissionFlag)
  {
    snprintf(permissionString,sizeof(permissionString),"%4o",permission & FILE_PERMISSION_ALL);
  }
  else
  {
    File_permissionToString(permissionString,sizeof(permissionString),permission);
  }

  if (globalOptions.longFormatFlag)
  {
    template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_DIR_LONG : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_DIR_LONG;
    String_format(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
  }
  else
  {
    template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_DIR : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_DIR;
  }

  TEXT_MACRO_N_STRING (textMacros[0],"%storageName",storageName,     NULL);
  TEXT_MACRO_N_CSTRING(textMacros[1],"%type",       "DIR",           NULL);
  TEXT_MACRO_N_STRING (textMacros[2],"%dateTime",   dateTimeString,  NULL);
  TEXT_MACRO_N_CSTRING(textMacros[3],"%user",       userName,        NULL);
  TEXT_MACRO_N_CSTRING(textMacros[4],"%group",      groupName,       NULL);
  TEXT_MACRO_N_CSTRING(textMacros[5],"%permission", permissionString,NULL);
  TEXT_MACRO_N_STRING (textMacros[6],"%crypt",      cryptString,     NULL);
  TEXT_MACRO_N_STRING (textMacros[7],"%name",       directoryName,   NULL);

  // print
  printInfo(0,"%s\n",
            String_cString(Misc_expandMacros(String_clear(line),
                                             template,
                                             EXPAND_MACRO_MODE_STRING,
                                             textMacros,SIZE_OF_ARRAY(textMacros),
                                             TRUE
                                            )
                          )
           );

  // free resources
  String_delete(line);
  String_delete(cryptString);
  String_delete(dateTimeString);
}

/***********************************************************************\
* Name   : printLinkInfo
* Purpose: print link information
* Input  : storageName     - storage name or NULL if storage name should
*                            not be printed
*          linkName        - link name
*          destinationName - name of referenced file
*          timeModified    - file modified time
*          userId          - user id
*          groupId         - group id
*          permission      - permissions
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printLinkInfo(ConstString     storageName,
                         ConstString     linkName,
                         ConstString     destinationName,
                         uint64          timeModified,
                         uint32          userId,
                         uint32          groupId,
                         FilePermission  permission,
                         CryptAlgorithms cryptAlgorithm,
                         CryptTypes      cryptType
                        )
{
  String     dateTimeString;
  String     cryptString;
  String     line;
  char       userName[12],groupName[12];
  char       permissionString[10];
  const char *template;
  TextMacro  textMacros[9];

  assert(linkName != NULL);
  assert(destinationName != NULL);

  // init variables
  dateTimeString = String_new();
  cryptString    = String_new();
  line           = String_new();

  // format
  Misc_formatDateTime(dateTimeString,timeModified,NULL);

  if (globalOptions.numericUIDGIDFlag)
  {
    snprintf(userName,sizeof(userName),"%d",userId);
    snprintf(groupName,sizeof(groupName),"%d",groupId);
  }
  else
  {
    File_userIdToUserName(userName,sizeof(userName),userId);
    File_groupIdToGroupName(groupName,sizeof(groupName),groupId);
  }
  if (globalOptions.numericPermissionFlag)
  {
    snprintf(permissionString,sizeof(permissionString),"%4o",permission & FILE_PERMISSION_ALL);
  }
  else
  {
    File_permissionToString(permissionString,sizeof(permissionString),permission);
  }

  if (globalOptions.longFormatFlag)
  {
    template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_LINK_LONG : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_LINK_LONG;
    String_format(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
  }
  else
  {
    template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_LINK : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_LINK;
  }

  TEXT_MACRO_N_STRING   (textMacros[ 0],"%storageName",    storageName,     NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 1],"%type",           "LINK",          NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 2],"%dateTime",       dateTimeString,  NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 3],"%user",           userName,        NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 4],"%group",          groupName,       NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 5],"%permission",     permissionString,NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 6],"%crypt",          cryptString,     NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 7],"%name",           linkName,        NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 8],"%destinationName",destinationName, NULL);

  // print
  printInfo(0,
            "%s\n",
            String_cString(Misc_expandMacros(line,
                                             template,
                                             EXPAND_MACRO_MODE_STRING,
                                             textMacros,SIZE_OF_ARRAY(textMacros),
                                             TRUE
                                            )
                          )
           );

  // free resources
  String_delete(line);
  String_delete(cryptString);
  String_delete(dateTimeString);
}

/***********************************************************************\
* Name   : printHardLinkInfo
* Purpose: print hard link information
* Input  : storageName            - storage name or NULL if storage name
*                                   should not be printed
*          fileName               - file name
*          size                   - file size [bytes]
*          timeModified           - file modified time
*          userId                 - user id
*          groupId                - group id
*          permission             - permissions
*          archiveSize            - archive size [bytes]
*          deltaCompressAlgorithm - used delta compress algorithm
*          byteCompressAlgorithm  - used data compress algorithm
*          cryptAlgorithm         - used crypt algorithm
*          cryptType              - crypt type; see CRYPT_TYPES
*          deltaSourceName        - delta source name
*          deltaSourceSize        - delta source size [bytes]
*          fragmentOffset         - fragment offset (0..n-1)
*          fragmentSize           - fragment length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printHardLinkInfo(ConstString        storageName,
                             ConstString        fileName,
                             uint64             size,
                             uint64             timeModified,
                             uint32             userId,
                             uint32             groupId,
                             FilePermission     permission,
                             uint64             archiveSize,
                             CompressAlgorithms deltaCompressAlgorithm,
                             CompressAlgorithms byteCompressAlgorithm,
                             CryptAlgorithms    cryptAlgorithm,
                             CryptTypes         cryptType,
                             ConstString        deltaSourceName,
                             uint64             deltaSourceSize,
                             uint64             fragmentOffset,
                             uint64             fragmentSize
                            )
{
  String     dateTimeString;
  String     compressString;
  String     cryptString;
  String     line;
  double     ratio;
  char       sizeString[16];
  char       userName[12],groupName[12];
  char       permissionString[10];
  char       deltaSourceSizeString[16];
  const char *template;
  TextMacro  textMacros[15];

  assert(fileName != NULL);

  // init variables
  dateTimeString = String_new();
  compressString = String_new();
  cryptString    = String_new();
  line           = String_new();

  // format
  Misc_formatDateTime(dateTimeString,timeModified,NULL);

  if (globalOptions.humanFormatFlag)
  {
    getHumanSizeString(sizeString,sizeof(sizeString),size);
    getHumanSizeString(deltaSourceSizeString,sizeof(deltaSourceSizeString),deltaSourceSize);
  }
  else
  {
    snprintf(sizeString,sizeof(sizeString),"%llu",size);
    snprintf(deltaSourceSizeString,sizeof(deltaSourceSizeString),"%llu",deltaSourceSize);
  }

  if (globalOptions.numericUIDGIDFlag)
  {
    snprintf(userName,sizeof(userName),"%d",userId);
    snprintf(groupName,sizeof(groupName),"%d",groupId);
  }
  else
  {
    File_userIdToUserName(userName,sizeof(userName),userId);
    File_groupIdToGroupName(groupName,sizeof(groupName),groupId);
  }
  if (globalOptions.numericPermissionFlag)
  {
    snprintf(permissionString,sizeof(permissionString),"%4o",permission & FILE_PERMISSION_ALL);
  }
  else
  {
    File_permissionToString(permissionString,sizeof(permissionString),permission);
  }

  if (globalOptions.longFormatFlag)
  {
    template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_HARDLINK_LONG : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_HARDLINK_LONG;
    if      ((Compress_isCompressed(deltaCompressAlgorithm) && Compress_isCompressed(byteCompressAlgorithm)))
    {
      String_format(compressString,
                    "%s+%s",
                    Compress_algorithmToString(deltaCompressAlgorithm),
                    Compress_algorithmToString(byteCompressAlgorithm)
                   );
    }
    else if (Compress_isCompressed(deltaCompressAlgorithm))
    {
      String_format(compressString,
                    "%s",
                    Compress_algorithmToString(deltaCompressAlgorithm)
                   );
    }
    else if (Compress_isCompressed(byteCompressAlgorithm))
    {
      String_format(compressString,
                    "%s",
                    Compress_algorithmToString(byteCompressAlgorithm)
                   );
    }
    else
    {
      String_format(compressString,
                    "%s",
                    Compress_algorithmToString(deltaCompressAlgorithm)
                   );
    }

    String_format(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
  }
  else
  {
    template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_HARDLINK : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_HARDLINK;
  }

  if (   (   Compress_isCompressed(deltaCompressAlgorithm)
          || Compress_isCompressed(byteCompressAlgorithm)
         )
      && (fragmentSize > 0LL)
     )
  {
    ratio = 100.0-(double)archiveSize*100.0/(double)fragmentSize;
  }
  else
  {
    ratio = 0.0;
  }

  TEXT_MACRO_N_STRING   (textMacros[ 0],"%storageName",    storageName,                                                          NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 1],"%type",           "HARDLINK",                                                           NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 2],"%size",           sizeString,                                                           NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 3],"%dateTime",       dateTimeString,                                                       NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 4],"%user",           userName,                                                             NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 5],"%group",          groupName,                                                            NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[ 6],"%permission",     permissionString,                                                     NULL);
  TEXT_MACRO_N_INTEGER64(textMacros[ 7],"%partFrom",       fragmentOffset,                                                       NULL);
  TEXT_MACRO_N_INTEGER64(textMacros[ 8],"%partTo",         (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1 : fragmentOffset,NULL);
  TEXT_MACRO_N_STRING   (textMacros[ 9],"%compress",       compressString,                                                       NULL);
  TEXT_MACRO_N_DOUBLE   (textMacros[10],"%ratio",          ratio,                                                                NULL);
  TEXT_MACRO_N_STRING   (textMacros[11],"%crypt",          cryptString,                                                          NULL);
  TEXT_MACRO_N_STRING   (textMacros[12],"%name",           fileName,                                                             NULL);
  TEXT_MACRO_N_STRING   (textMacros[13],"%deltaSourceName",deltaSourceName,                                                      NULL);
  TEXT_MACRO_N_CSTRING  (textMacros[14],"%deltaSourceSize",deltaSourceSizeString,                                                NULL);

  // print
  printInfo(0,"%s\n",
            String_cString(Misc_expandMacros(String_clear(line),
                                             template,
                                             EXPAND_MACRO_MODE_STRING,
                                             textMacros,SIZE_OF_ARRAY(textMacros),
                                             TRUE
                                            )
                          )
           );
  if (Compress_isCompressed(deltaCompressAlgorithm) && isPrintInfo(2))
  {
    printInfo(0,
              "%s\n",
              String_cString(Misc_expandMacros(line,
                                               DEFAULT_ARCHIVE_LIST_FORMAT_SOURCE,
                                               EXPAND_MACRO_MODE_STRING,
                                               textMacros,SIZE_OF_ARRAY(textMacros),
                                               TRUE
                                              )
                            )
             );
  }

  // free resources
  String_delete(line);
  String_delete(cryptString);
  String_delete(compressString);
  String_delete(dateTimeString);
}

/***********************************************************************\
* Name   : printSpecialInfo
* Purpose: print special information
* Input  : storageName     - storage name or NULL if storage name should
*                            not be printed
*          fileName        - file name
*          userId          - user id
*          groupId         - group id
*          permission      - permissions
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
*          fileSpecialType - special file type
*          major,minor     - special major/minor number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printSpecialInfo(ConstString      storageName,
                            ConstString      fileName,
                            uint32           userId,
                            uint32           groupId,
                            FilePermission   permission,
                            CryptAlgorithms  cryptAlgorithm,
                            CryptTypes       cryptType,
                            FileSpecialTypes fileSpecialType,
                            uint32           major,
                            uint32           minor
                           )
{
  String     cryptString;
  String     line;
  char       userName[12],groupName[12];
  char       permissionString[10];
  const char *template;
  const char *type;
  TextMacro  textMacros[9];

  assert(fileName != NULL);

  // init variables
  cryptString = String_new();
  line        = String_new();

  // format
  template = NULL;
  type     = NULL;
  switch (fileSpecialType)
  {
    case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
      if (globalOptions.longFormatFlag)
      {
        template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_CHAR_LONG : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_CHAR_LONG;
        String_format(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
      }
      else
      {
        template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_CHAR : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_CHAR;
      }
      type = "CHAR";
      break;
    case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
      if (globalOptions.longFormatFlag)
      {
        template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_BLOCK_LONG : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_BLOCK_LONG;
        String_format(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
      }
      else
      {
        template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_BLOCK : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_BLOCK;
      }
      type = "BLOCK";
      break;
    case FILE_SPECIAL_TYPE_FIFO:
      if (globalOptions.longFormatFlag)
      {
        template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_FIFO_LONG : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_FIFO_LONG;
        String_format(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
      }
      else
      {
        template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_FIFO : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_FIFO;
      }
      type = "FIFO";
      break;
    case FILE_SPECIAL_TYPE_SOCKET:
      if (globalOptions.longFormatFlag)
      {
        template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_SOCKET_LONG : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_SOCKET_LONG;
        String_format(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
      }
      else
      {
        template = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_GROUP_SPECIAL_SOCKET : DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_SOCKET;
      }
      type = "SOCKET";
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  if (globalOptions.numericUIDGIDFlag)
  {
    snprintf(userName,sizeof(userName),"%d",userId);
    snprintf(groupName,sizeof(groupName),"%d",groupId);
  }
  else
  {
    File_userIdToUserName(userName,sizeof(userName),userId);
    File_groupIdToGroupName(groupName,sizeof(groupName),groupId);
  }
  if (globalOptions.numericPermissionFlag)
  {
    snprintf(permissionString,sizeof(permissionString),"%4o",permission & FILE_PERMISSION_ALL);
  }
  else
  {
    File_permissionToString(permissionString,sizeof(permissionString),permission);
  }

  TEXT_MACRO_N_STRING (textMacros[ 0],"%storageName",storageName,     NULL);
  TEXT_MACRO_N_CSTRING(textMacros[ 1],"%type",       type,            NULL);
  TEXT_MACRO_N_CSTRING(textMacros[ 2],"%user",       userName,        NULL);
  TEXT_MACRO_N_CSTRING(textMacros[ 3],"%group",      groupName,       NULL);
  TEXT_MACRO_N_CSTRING(textMacros[ 4],"%permission", permissionString,NULL);
  TEXT_MACRO_N_STRING (textMacros[ 5],"%crypt",      cryptString,     NULL);
  TEXT_MACRO_N_STRING (textMacros[ 6],"%name",       fileName,        NULL);
  TEXT_MACRO_N_INTEGER(textMacros[ 7],"%major",      major,           NULL);
  TEXT_MACRO_N_INTEGER(textMacros[ 8],"%minor",      minor,           NULL);

  // print
  printInfo(0,"%s\n",
            String_cString(Misc_expandMacros(String_clear(line),
                                             template,
                                             EXPAND_MACRO_MODE_STRING,
                                             textMacros,SIZE_OF_ARRAY(textMacros),
                                             TRUE
                                            )
                          )
           );

  // free resources
  String_delete(line);
  String_delete(cryptString);
}

/***********************************************************************\
* Name   : addListFileInfo
* Purpose: add file info to archive entry list
* Input  : storageName            - storage name
*          fileName               - file name
*          fileSize               - file size [bytes]
*          timeModified           - file modified time
*          userId                 - user id
*          groupId                - group id
*          permission             - permissions
*          archiveSize            - archive size [bytes]
*          deltaCompressAlgorithm - used delta compress algorithm
*          byteCompressAlgorithm  - used data compress algorithm
*          cryptAlgorithm         - used crypt algorithm
*          cryptType              - crypt type; see CRYPT_TYPES
*          deltaSourceName        - delta source name
*          deltaSourceSize        - delta source size [bytes]
*          fragmentOffset         - fragment offset (0..n-1)
*          fragmentSize           - fragment length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListFileInfo(ConstString        storageName,
                           ConstString        fileName,
                           uint64             fileSize,
                           uint64             timeModified,
                           uint32             userId,
                           uint32             groupId,
                           FilePermission     permission,
                           uint64             archiveSize,
                           CompressAlgorithms deltaCompressAlgorithm,
                           CompressAlgorithms byteCompressAlgorithm,
                           CryptAlgorithms    cryptAlgorithm,
                           CryptTypes         cryptType,
                           ConstString        deltaSourceName,
                           uint64             deltaSourceSize,
                           uint64             fragmentOffset,
                           uint64             fragmentSize
                          )
{
  ArchiveContentNode *archiveContentNode;

  // allocate node
  archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
  if (archiveContentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init node
  archiveContentNode->storageName                 = String_duplicate(storageName);
  archiveContentNode->type                        = ARCHIVE_ENTRY_TYPE_FILE;
  archiveContentNode->file.fileName               = String_duplicate(fileName);
  archiveContentNode->file.size                   = fileSize;
  archiveContentNode->file.timeModified           = timeModified;
  archiveContentNode->file.userId                 = userId;
  archiveContentNode->file.groupId                = groupId;
  archiveContentNode->file.permission             = permission;
  archiveContentNode->file.archiveSize            = archiveSize;
  archiveContentNode->file.deltaCompressAlgorithm = deltaCompressAlgorithm;
  archiveContentNode->file.byteCompressAlgorithm  = byteCompressAlgorithm;
  archiveContentNode->file.cryptAlgorithm         = cryptAlgorithm;
  archiveContentNode->file.cryptType              = cryptType;
  archiveContentNode->file.deltaSourceName        = String_duplicate(deltaSourceName);
  archiveContentNode->file.deltaSourceSize        = deltaSourceSize;
  archiveContentNode->file.fragmentOffset         = fragmentOffset;
  archiveContentNode->file.fragmentSize           = fragmentSize;

  // append to list
  List_append(&archiveContentList,archiveContentNode);
}

/***********************************************************************\
* Name   : addListImageInfo
* Purpose: add image info to archive entry list
* Input  : storageName            - storage name
*          imageName              - image name
*          imageSize              - image size [bytes]
*          archiveSize            - archive size [bytes]
*          deltaCompressAlgorithm - used delta compress algorithm
*          byteCompressAlgorithm  - used data compress algorithm
*          cryptAlgorithm         - used crypt algorithm
*          cryptType              - crypt type; see CRYPT_TYPES
*          deltaSourceName        - delta source name
*          deltaSourceSize        - delta source size [bytes]
*          blockSize              - block size
*          blockOffset            - block offset (0..n-1)
*          blockCount             - block count
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListImageInfo(ConstString        storageName,
                            ConstString        imageName,
                            uint64             imageSize,
                            uint64             archiveSize,
                            CompressAlgorithms deltaCompressAlgorithm,
                            CompressAlgorithms byteCompressAlgorithm,
                            CryptAlgorithms    cryptAlgorithm,
                            CryptTypes         cryptType,
                            ConstString        deltaSourceName,
                            uint64             deltaSourceSize,
                            uint               blockSize,
                            uint64             blockOffset,
                            uint64             blockCount
                           )
{
  ArchiveContentNode *archiveContentNode;

  // allocate node
  archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
  if (archiveContentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init node
  archiveContentNode->storageName                  = String_duplicate(storageName);
  archiveContentNode->type                         = ARCHIVE_ENTRY_TYPE_IMAGE;
  archiveContentNode->image.imageName              = String_duplicate(imageName);
  archiveContentNode->image.size                   = imageSize;
  archiveContentNode->image.archiveSize            = archiveSize;
  archiveContentNode->image.deltaCompressAlgorithm = deltaCompressAlgorithm;
  archiveContentNode->image.byteCompressAlgorithm  = byteCompressAlgorithm;
  archiveContentNode->image.cryptAlgorithm         = cryptAlgorithm;
  archiveContentNode->image.cryptType              = cryptType;
  archiveContentNode->image.deltaSourceName        = String_duplicate(deltaSourceName);
  archiveContentNode->image.deltaSourceSize        = deltaSourceSize;
  archiveContentNode->image.blockSize              = blockSize;
  archiveContentNode->image.blockOffset            = blockOffset;
  archiveContentNode->image.blockCount             = blockCount;

  // append to list
  List_append(&archiveContentList,archiveContentNode);
}

/***********************************************************************\
* Name   : addListDirectoryInfo
* Purpose: add directory info to archive entry list
* Input  : storageName    - storage name
*          directoryName  - directory name
*          timeModified   - file modified time
*          userId         - user id
*          groupId        - group id
*          permission     - permissions
*          cryptAlgorithm - used crypt algorithm
*          cryptType      - crypt type; see CRYPT_TYPES
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListDirectoryInfo(ConstString     storageName,
                                ConstString     directoryName,
                                uint64          timeModified,
                                uint32          userId,
                                uint32          groupId,
                                FilePermission  permission,
                                CryptAlgorithms cryptAlgorithm,
                                CryptTypes      cryptType
                               )
{
  ArchiveContentNode *archiveContentNode;

  // allocate node
  archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
  if (archiveContentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init node
  archiveContentNode->storageName              = String_duplicate(storageName);
  archiveContentNode->type                     = ARCHIVE_ENTRY_TYPE_DIRECTORY;
  archiveContentNode->directory.directoryName  = String_duplicate(directoryName);
  archiveContentNode->directory.timeModified   = timeModified;
  archiveContentNode->directory.userId         = userId;
  archiveContentNode->directory.groupId        = groupId;
  archiveContentNode->directory.permission     = permission;
  archiveContentNode->directory.cryptAlgorithm = cryptAlgorithm;
  archiveContentNode->directory.cryptType      = cryptType;

  // append to list
  List_append(&archiveContentList,archiveContentNode);
}

/***********************************************************************\
* Name   : addListLinkInfo
* Purpose: add link info to archive entry list
* Input  : storageName     - storage name
*          linkName        - link name
*          destinationName - destination name
*          timeModified    - link modified time
*          userId          - user id
*          groupId         - group id
*          permission      - permissions
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListLinkInfo(ConstString     storageName,
                           ConstString     linkName,
                           ConstString     destinationName,
                           uint64          timeModified,
                           uint32          userId,
                           uint32          groupId,
                           FilePermission  permission,
                           CryptAlgorithms cryptAlgorithm,
                           CryptTypes      cryptType
                          )
{
  ArchiveContentNode *archiveContentNode;

  // allocate node
  archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
  if (archiveContentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init node
  archiveContentNode->storageName          = String_duplicate(storageName);
  archiveContentNode->type                 = ARCHIVE_ENTRY_TYPE_LINK;
  archiveContentNode->link.linkName        = String_duplicate(linkName);
  archiveContentNode->link.destinationName = String_duplicate(destinationName);
  archiveContentNode->link.timeModified    = timeModified;
  archiveContentNode->link.userId          = userId;
  archiveContentNode->link.groupId         = groupId;
  archiveContentNode->link.permission      = permission;
  archiveContentNode->link.cryptAlgorithm  = cryptAlgorithm;
  archiveContentNode->link.cryptType       = cryptType;

  // append to list
  List_append(&archiveContentList,archiveContentNode);
}

/***********************************************************************\
* Name   : addListHardLinkInfo
* Purpose: add hard link info to archive entry list
* Input  : storageName            - storage name
*          fileName               - file name
*          fileSize               - file size [bytes]
*          timeModified           - file modified time
*          userId                 - user id
*          groupId                - group id
*          permission             - permissions
*          archiveSize            - archive size [bytes]
*          deltaCompressAlgorithm - used delta compress algorithm
*          byteCompressAlgorithm  - used data compress algorithm
*          cryptAlgorithm         - used crypt algorithm
*          cryptType              - crypt type; see CRYPT_TYPES
*          deltaSourceName        - delta source name
*          deltaSourceSize        - delta source size [bytes]
*          fragmentOffset         - fragment offset (0..n-1)
*          fragmentSize           - fragment length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListHardLinkInfo(ConstString        storageName,
                               ConstString        fileName,
                               uint64             fileSize,
                               uint64             timeModified,
                               uint32             userId,
                               uint32             groupId,
                               FilePermission     permission,
                               uint64             archiveSize,
                               CompressAlgorithms deltaCompressAlgorithm,
                               CompressAlgorithms byteCompressAlgorithm,
                               CryptAlgorithms    cryptAlgorithm,
                               CryptTypes         cryptType,
                               ConstString        deltaSourceName,
                               uint64             deltaSourceSize,
                               uint64             fragmentOffset,
                               uint64             fragmentSize
                              )
{
  ArchiveContentNode *archiveContentNode;

  // allocate node
  archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
  if (archiveContentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init node
  archiveContentNode->storageName                     = String_duplicate(storageName);
  archiveContentNode->type                            = ARCHIVE_ENTRY_TYPE_HARDLINK;
  archiveContentNode->hardLink.fileName               = String_duplicate(fileName);
  archiveContentNode->hardLink.size                   = fileSize;
  archiveContentNode->hardLink.timeModified           = timeModified;
  archiveContentNode->hardLink.userId                 = userId;
  archiveContentNode->hardLink.groupId                = groupId;
  archiveContentNode->hardLink.permission             = permission;
  archiveContentNode->hardLink.archiveSize            = archiveSize;
  archiveContentNode->hardLink.deltaCompressAlgorithm = deltaCompressAlgorithm;
  archiveContentNode->hardLink.byteCompressAlgorithm  = byteCompressAlgorithm;
  archiveContentNode->hardLink.cryptAlgorithm         = cryptAlgorithm;
  archiveContentNode->hardLink.cryptType              = cryptType;
  archiveContentNode->hardLink.deltaSourceName        = String_duplicate(deltaSourceName);
  archiveContentNode->hardLink.deltaSourceSize        = deltaSourceSize;
  archiveContentNode->hardLink.fragmentOffset         = fragmentOffset;
  archiveContentNode->hardLink.fragmentSize           = fragmentSize;

  // append to list
  List_append(&archiveContentList,archiveContentNode);
}

/***********************************************************************\
* Name   : addListSpecialInfo
* Purpose: add special info to archive entry list
* Input  : storageName      - storage name
*          userId           - user id
*          groupId          - group id
*          permission       - permissions
*          cryptAlgorithm   - used crypt algorithm
*          cryptType        - crypt type; see CRYPT_TYPES
*          FileSpecialTypes - special type
*          major,minor      - special major/minor number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListSpecialInfo(ConstString      storageName,
                              ConstString      fileName,
                              uint32           userId,
                              uint32           groupId,
                              FilePermission   permission,
                              CryptAlgorithms  cryptAlgorithm,
                              CryptTypes       cryptType,
                              FileSpecialTypes fileSpecialType,
                              uint32           major,
                              uint32           minor
                             )
{
  ArchiveContentNode *archiveContentNode;

  // allocate node
  archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
  if (archiveContentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init node
  archiveContentNode->storageName             = String_duplicate(storageName);
  archiveContentNode->type                    = ARCHIVE_ENTRY_TYPE_SPECIAL;
  archiveContentNode->special.fileName        = String_duplicate(fileName);
  archiveContentNode->special.userId          = userId;
  archiveContentNode->special.groupId         = groupId;
  archiveContentNode->special.permission      = permission;
  archiveContentNode->special.cryptAlgorithm  = cryptAlgorithm;
  archiveContentNode->special.cryptType       = cryptType;
  archiveContentNode->special.fileSpecialType = fileSpecialType;
  archiveContentNode->special.major           = major;
  archiveContentNode->special.minor           = minor;

  // append to list
  List_append(&archiveContentList,archiveContentNode);
}

/***********************************************************************\
* Name   : freeArchiveContentNode
* Purpose: free archive entry node
* Input  : archiveContentNode - content node to free
*          userData           - user data (not used)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freeArchiveContentNode(ArchiveContentNode *archiveContentNode, void *userData)
{
  assert(archiveContentNode != NULL);

  UNUSED_VARIABLE(userData);

  switch (archiveContentNode->type)
  {
    case ARCHIVE_ENTRY_TYPE_NONE:
      break;
    case ARCHIVE_ENTRY_TYPE_FILE:
      String_delete(archiveContentNode->file.fileName);
      String_delete(archiveContentNode->file.deltaSourceName);
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      String_delete(archiveContentNode->image.imageName);
      String_delete(archiveContentNode->image.deltaSourceName);
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      String_delete(archiveContentNode->directory.directoryName);
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      String_delete(archiveContentNode->link.destinationName);
      String_delete(archiveContentNode->link.linkName);
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      String_delete(archiveContentNode->hardLink.fileName);
      String_delete(archiveContentNode->hardLink.deltaSourceName);
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      String_delete(archiveContentNode->special.fileName);
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  String_delete(archiveContentNode->storageName);
}

/***********************************************************************\
* Name   : compareArchiveContentNode
* Purpose: compare archive content entries
* Input  : archiveContentNode1,archiveContentNode2 - nodes to compare
*          userData                                - user data (not used)
* Output : -
* Return : -1 iff name1 < name2 or
*                 name1 == name2 && timeModified1 < timeModified2
*           1 iff name1 > name2 or
*                 name1 == name2 && timeModified1 > timeModified2
*           0 iff name1 == name2 && timeModified1 == timeModified2
* Notes  : -
\***********************************************************************/

LOCAL int compareArchiveContentNode(const ArchiveContentNode *archiveContentNode1, const ArchiveContentNode *archiveContentNode2, void *userData)
{
  String name1,name2;
  uint64 modifiedTime1,modifiedTime2;

  assert(archiveContentNode1 != NULL);
  assert(archiveContentNode2 != NULL);

  UNUSED_VARIABLE(userData);

  // get data
  name1         = NULL;
  modifiedTime1 = 0LL;
  name2         = NULL;
  modifiedTime2 = 0LL;
  switch (archiveContentNode1->type)
  {
    case ARCHIVE_ENTRY_TYPE_FILE:
      name1         = archiveContentNode1->file.fileName;
      modifiedTime1 = archiveContentNode1->file.timeModified;
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      name1         = archiveContentNode1->image.imageName;
      modifiedTime1 = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      name1         = archiveContentNode1->directory.directoryName;
      modifiedTime1 = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      name1         = archiveContentNode1->link.linkName;
      modifiedTime1 = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      name1         = archiveContentNode1->hardLink.fileName;
      modifiedTime1 = archiveContentNode1->hardLink.timeModified;
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      name1         = archiveContentNode1->special.fileName;
      modifiedTime1 = 0LL;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  switch (archiveContentNode2->type)
  {
    case ARCHIVE_ENTRY_TYPE_FILE:
      name2         = archiveContentNode2->file.fileName;
      modifiedTime2 = archiveContentNode2->file.timeModified;
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      name2         = archiveContentNode2->image.imageName;
      modifiedTime2 = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      name2         = archiveContentNode2->directory.directoryName;
      modifiedTime2 = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      name2         = archiveContentNode2->link.linkName;
      modifiedTime2 = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      name2         = archiveContentNode2->hardLink.fileName;
      modifiedTime2 = archiveContentNode2->hardLink.timeModified;
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      name2         = archiveContentNode2->special.fileName;
      modifiedTime2 = 0LL;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  // compare
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
* Name   : printrchiveList
* Purpose: sort, group and print list with archive entries
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printArchiveList(void)
{
  ArchiveContentNode *archiveContentNode;
  ArchiveEntryTypes  prevArchiveEntryType;
  String             prevArchiveName;

  // sort list
  List_sort(&archiveContentList,
            (ListNodeCompareFunction)compareArchiveContentNode,
            NULL
           );

  // output list
  prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_NONE;
  prevArchiveName      = NULL;
  archiveContentNode   = archiveContentList.head;
  while (archiveContentNode != NULL)
  {
    // output
    switch (archiveContentNode->type)
    {
      case ARCHIVE_ENTRY_TYPE_FILE:
        if (   globalOptions.allFlag
            || (prevArchiveEntryType != ARCHIVE_ENTRY_TYPE_FILE)
            || !String_equals(prevArchiveName,archiveContentNode->file.fileName)
           )
        {
          printFileInfo(archiveContentNode->storageName,
                        archiveContentNode->file.fileName,
                        archiveContentNode->file.size,
                        archiveContentNode->file.timeModified,
                        archiveContentNode->file.userId,
                        archiveContentNode->file.groupId,
                        archiveContentNode->file.permission,
                        archiveContentNode->file.archiveSize,
                        archiveContentNode->file.deltaCompressAlgorithm,
                        archiveContentNode->file.byteCompressAlgorithm,
                        archiveContentNode->file.cryptAlgorithm,
                        archiveContentNode->file.cryptType,
                        archiveContentNode->file.deltaSourceName,
                        archiveContentNode->file.deltaSourceSize,
                        archiveContentNode->file.fragmentOffset,
                        archiveContentNode->file.fragmentSize
                       );
          prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_FILE;
          prevArchiveName      = archiveContentNode->file.fileName;
        }
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        if (   globalOptions.allFlag
            || (prevArchiveEntryType != ARCHIVE_ENTRY_TYPE_IMAGE)
            || !String_equals(prevArchiveName,archiveContentNode->image.imageName)
           )
        {
          printImageInfo(archiveContentNode->storageName,
                         archiveContentNode->image.imageName,
                         archiveContentNode->image.size,
                         archiveContentNode->image.archiveSize,
                         archiveContentNode->image.deltaCompressAlgorithm,
                         archiveContentNode->image.byteCompressAlgorithm,
                         archiveContentNode->image.cryptAlgorithm,
                         archiveContentNode->image.cryptType,
                         archiveContentNode->image.deltaSourceName,
                         archiveContentNode->image.deltaSourceSize,
                         archiveContentNode->image.blockSize,
                         archiveContentNode->image.blockOffset,
                         archiveContentNode->image.blockCount
                        );
          prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_IMAGE;
          prevArchiveName      = archiveContentNode->image.imageName;
        }
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        if (   globalOptions.allFlag
            || (prevArchiveEntryType != ARCHIVE_ENTRY_TYPE_DIRECTORY)
            || !String_equals(prevArchiveName,archiveContentNode->file.fileName)
           )
        {
          printDirectoryInfo(archiveContentNode->storageName,
                             archiveContentNode->directory.directoryName,
                             archiveContentNode->directory.timeModified,
                             archiveContentNode->directory.userId,
                             archiveContentNode->directory.groupId,
                             archiveContentNode->directory.permission,
                             archiveContentNode->directory.cryptAlgorithm,
                             archiveContentNode->directory.cryptType
                            );
          prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_DIRECTORY;
          prevArchiveName      = archiveContentNode->directory.directoryName;
        }
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        if (   globalOptions.allFlag
            || (prevArchiveEntryType != ARCHIVE_ENTRY_TYPE_LINK)
            || !String_equals(prevArchiveName,archiveContentNode->file.fileName)
           )
        {
          printLinkInfo(archiveContentNode->storageName,
                        archiveContentNode->link.linkName,
                        archiveContentNode->link.destinationName,
                        archiveContentNode->link.timeModified,
                        archiveContentNode->link.userId,
                        archiveContentNode->link.groupId,
                        archiveContentNode->link.permission,
                        archiveContentNode->link.cryptAlgorithm,
                        archiveContentNode->link.cryptType
                       );
          prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_LINK;
          prevArchiveName      = archiveContentNode->link.linkName;
        }
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        if (   globalOptions.allFlag
            || (prevArchiveEntryType != ARCHIVE_ENTRY_TYPE_HARDLINK)
            || !String_equals(prevArchiveName,archiveContentNode->file.fileName)
           )
        {
          printHardLinkInfo(archiveContentNode->storageName,
                            archiveContentNode->hardLink.fileName,
                            archiveContentNode->hardLink.size,
                            archiveContentNode->hardLink.timeModified,
                            archiveContentNode->hardLink.userId,
                            archiveContentNode->hardLink.groupId,
                            archiveContentNode->hardLink.permission,
                            archiveContentNode->hardLink.archiveSize,
                            archiveContentNode->hardLink.deltaCompressAlgorithm,
                            archiveContentNode->hardLink.byteCompressAlgorithm,
                            archiveContentNode->hardLink.cryptAlgorithm,
                            archiveContentNode->hardLink.cryptType,
                            archiveContentNode->hardLink.deltaSourceName,
                            archiveContentNode->hardLink.deltaSourceSize,
                            archiveContentNode->hardLink.fragmentOffset,
                            archiveContentNode->hardLink.fragmentSize
                           );
          prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_HARDLINK;
          prevArchiveName      = archiveContentNode->hardLink.fileName;
        }
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        if (   globalOptions.allFlag
            || (prevArchiveEntryType != ARCHIVE_ENTRY_TYPE_SPECIAL)
            || !String_equals(prevArchiveName,archiveContentNode->file.fileName)
           )
        {
          printSpecialInfo(archiveContentNode->storageName,
                           archiveContentNode->special.fileName,
                           archiveContentNode->special.userId,
                           archiveContentNode->special.groupId,
                           archiveContentNode->special.permission,
                           archiveContentNode->special.cryptAlgorithm,
                           archiveContentNode->special.cryptType,
                           archiveContentNode->special.fileSpecialType,
                           archiveContentNode->special.major,
                           archiveContentNode->special.minor
                          );
          prevArchiveEntryType = ARCHIVE_ENTRY_TYPE_SPECIAL;
          prevArchiveName      = archiveContentNode->special.fileName;
        }
        break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }

    // next entry
    archiveContentNode = archiveContentNode->next;
  }
}

/***********************************************************************\
* Name   : listArchiveContent
* Purpose: list archive content
* Input  : storageSpecifier     - storage specifier
*          archiveName          - archive name
*          includeEntryList     - include entry list
*          excludePatternList   - exclude pattern list
*          jobOptions           - job options
*          printableStorageName - printable storage name
*          getPasswordFunction  - get password call back
*          getPasswordUserData  - user data for get password
*          logHandle            - log handle (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors listArchiveContent(StorageSpecifier    *storageSpecifier,
                                ConstString         archiveName,
                                const EntryList     *includeEntryList,
                                const PatternList   *excludePatternList,
                                JobOptions          *jobOptions,
                                GetPasswordFunction getPasswordFunction,
                                void                *getPasswordUserData,
                                LogHandle           *logHandle
                               )
{
  bool         printedInfoFlag;
  ulong        fileCount;
  Errors       error;
bool         remoteBarFlag;
//  SSHSocketList sshSocketList;
//  SSHSocketNode *sshSocketNode;
  SocketHandle     socketHandle;

  assert(storageSpecifier != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

// NYI ???
remoteBarFlag=FALSE;

  printedInfoFlag = FALSE;
  fileCount       = 0L;
  error           = ERROR_NONE;
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
    case STORAGE_TYPE_FTP:
    case STORAGE_TYPE_SFTP:
    case STORAGE_TYPE_WEBDAV:
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      {
        StorageInfo       storageInfo;
        ArchiveInfo       archiveInfo;
        ArchiveEntryInfo  archiveEntryInfo;
        ArchiveEntryTypes archiveEntryType;

        // init storage
        error = Storage_init(&storageInfo,
                             storageSpecifier,
                             jobOptions,
                             &globalOptions.maxBandWidthList,
                             SERVER_CONNECTION_PRIORITY_HIGH,
                             CALLBACK(NULL,NULL),  // updateStatusInfo
                             CALLBACK(NULL,NULL),  // getPassword
                             CALLBACK(NULL,NULL)  // requestVolume
                            );
        if (error != ERROR_NONE)
        {
          break;
        }

        // open archive
        error = Archive_open(&archiveInfo,
                             &storageInfo,
                             storageSpecifier,
                             archiveName,
                             NULL,  // deltaSourceList
                             jobOptions,
                             getPasswordFunction,
                             getPasswordUserData,
                             logHandle
                            );
        if (error != ERROR_NONE)
        {
          (void)Storage_done(&storageInfo);
          break;
        }

        // list contents
        while (   !Archive_eof(&archiveInfo,TRUE)
               && (error == ERROR_NONE)
              )
        {
          // get next archive entry type
          error = Archive_getNextArchiveEntryType(&archiveInfo,
                                                  &archiveEntryType,
                                                  TRUE
                                                 );
          if (error != ERROR_NONE)
          {
            printError("Cannot read next entry from storage '%s' (error: %s)!\n",
                       Storage_getPrintableNameCString(storageSpecifier,archiveName),
                       Error_getText(error)
                      );
            break;
          }

          switch (archiveEntryType)
          {
            case ARCHIVE_ENTRY_TYPE_FILE:
              {
                ArchiveEntryInfo   archiveEntryInfo;
                CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
                CryptAlgorithms    cryptAlgorithm;
                CryptTypes         cryptType;
                String             fileName;
                FileInfo           fileInfo;
                String             deltaSourceName;
                uint64             deltaSourceSize;
                uint64             fragmentOffset,fragmentSize;

                // read archive file
                fileName        = String_new();
                deltaSourceName = String_new();
                error = Archive_readFileEntry(&archiveEntryInfo,
                                              &archiveInfo,
                                              &deltaCompressAlgorithm,
                                              &byteCompressAlgorithm,
                                              &cryptAlgorithm,
                                              &cryptType,
                                              fileName,
                                              &fileInfo,
                                              NULL,  // fileExtendedAttributeList
                                              deltaSourceName,
                                              &deltaSourceSize,
                                              &fragmentOffset,
                                              &fragmentSize
                                             );
                if (error != ERROR_NONE)
                {
                  printError("Cannot read 'file' content from storage '%s' (error: %s)!\n",
                             Storage_getPrintableNameCString(storageSpecifier,archiveName),
                             Error_getText(error)
                            );
                  String_delete(deltaSourceName);
                  String_delete(fileName);
                  break;
                }

                if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                    && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                   )
                {
                  if (globalOptions.groupFlag)
                  {
                    // add file info to list
                    addListFileInfo(Storage_getName(storageSpecifier,NULL),
                                    fileName,
                                    fileInfo.size,
                                    fileInfo.timeModified,
                                    fileInfo.permission,
                                    fileInfo.userId,
                                    fileInfo.groupId,
                                    archiveEntryInfo.file.chunkFileData.info.size,
                                    deltaCompressAlgorithm,
                                    byteCompressAlgorithm,
                                    cryptAlgorithm,
                                    cryptType,
                                    deltaSourceName,
                                    deltaSourceSize,
                                    fragmentOffset,
                                    fragmentSize
                                   );
                  }
                  else
                  {
                    // output file info
                    if (!printedInfoFlag)
                    {
                      printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                      printedInfoFlag = TRUE;
                    }
                    printFileInfo(NULL,
                                  fileName,
                                  fileInfo.size,
                                  fileInfo.timeModified,
                                  fileInfo.userId,
                                  fileInfo.groupId,
                                  fileInfo.permission,
                                  archiveEntryInfo.file.chunkFileData.info.size,
                                  deltaCompressAlgorithm,
                                  byteCompressAlgorithm,
                                  cryptAlgorithm,
                                  cryptType,
                                  deltaSourceName,
                                  deltaSourceSize,
                                  fragmentOffset,
                                  fragmentSize
                                 );
                  }
                  fileCount++;
                }

                // close archive file, free resources
                error = Archive_closeEntry(&archiveEntryInfo);
                if (error != ERROR_NONE)
                {
                  printWarning("close 'file' entry fail (error: %s)\n",Error_getText(error));
                }

                // free resources
                String_delete(deltaSourceName);
                String_delete(fileName);
              }
              break;
            case ARCHIVE_ENTRY_TYPE_IMAGE:
              {
                ArchiveEntryInfo   archiveEntryInfo;
                CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
                CryptAlgorithms    cryptAlgorithm;
                CryptTypes         cryptType;
                String             deviceName;
                DeviceInfo         deviceInfo;
                FileSystemTypes    fileSystemType;
                String             deltaSourceName;
                uint64             deltaSourceSize;
                uint64             blockOffset,blockCount;

                // read archive image
                deviceName      = String_new();
                deltaSourceName = String_new();
                error = Archive_readImageEntry(&archiveEntryInfo,
                                               &archiveInfo,
                                               &deltaCompressAlgorithm,
                                               &byteCompressAlgorithm,
                                               &cryptAlgorithm,
                                               &cryptType,
                                               deviceName,
                                               &deviceInfo,
                                               &fileSystemType,
                                               deltaSourceName,
                                               &deltaSourceSize,
                                               &blockOffset,
                                               &blockCount
                                              );
                if (error != ERROR_NONE)
                {
                  printError("Cannot read 'image' content from storage '%s' (error: %s)!\n",
                             Storage_getPrintableNameCString(storageSpecifier,archiveName),
                             Error_getText(error)
                            );
                  String_delete(deltaSourceName);
                  String_delete(deviceName);
                  break;
                }

                if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,deviceName,PATTERN_MATCH_MODE_EXACT))
                    && !PatternList_match(excludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT)
                   )
                {
                  if (globalOptions.groupFlag)
                  {
                    // add image info to list
                    addListImageInfo(Storage_getName(storageSpecifier,NULL),
                                     deviceName,
                                     deviceInfo.size,
                                     archiveEntryInfo.image.chunkImageData.info.size,
                                     deltaCompressAlgorithm,
                                     byteCompressAlgorithm,
                                     cryptAlgorithm,
                                     cryptType,
                                     deltaSourceName,
                                     deltaSourceSize,
                                     deviceInfo.blockSize,
                                     blockOffset,
                                     blockCount
                                    );
                  }
                  else
                  {
                    // output file info
                    if (!printedInfoFlag)
                    {
                      printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                      printedInfoFlag = TRUE;
                    }
                    printImageInfo(NULL,
                                   deviceName,
                                   deviceInfo.size,
                                   archiveEntryInfo.image.chunkImageData.info.size,
                                   deltaCompressAlgorithm,
                                   byteCompressAlgorithm,
                                   cryptAlgorithm,
                                   cryptType,
                                   deltaSourceName,
                                   deltaSourceSize,
                                   deviceInfo.blockSize,
                                   blockOffset,
                                   blockCount
                                  );
                  }
                  fileCount++;
                }

                // close archive file, free resources
                error = Archive_closeEntry(&archiveEntryInfo);
                if (error != ERROR_NONE)
                {
                  printWarning("close 'image' entry fail (error: %s)\n",Error_getText(error));
                }

                // free resources
                String_delete(deltaSourceName);
                String_delete(deviceName);
              }
              break;
            case ARCHIVE_ENTRY_TYPE_DIRECTORY:
              {
                String          directoryName;
                CryptAlgorithms cryptAlgorithm;
                CryptTypes      cryptType;
                FileInfo        fileInfo;

                // read archive directory entry
                directoryName = String_new();
                error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                                   &archiveInfo,
                                                   &cryptAlgorithm,
                                                   &cryptType,
                                                   directoryName,
                                                   &fileInfo,
                                                   NULL   // fileExtendedAttributeList
                                                  );
                if (error != ERROR_NONE)
                {
                  printError("Cannot read 'directory' content from storage '%s' (error: %s)!\n",
                             Storage_getPrintableNameCString(storageSpecifier,archiveName),
                             Error_getText(error)
                            );
                  String_delete(directoryName);
                  break;
                }

                if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                    && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
                   )
                {
                  if (globalOptions.groupFlag)
                  {
                    // add directory info to list
                    addListDirectoryInfo(Storage_getName(storageSpecifier,NULL),
                                         directoryName,
                                         fileInfo.timeModified,
                                         fileInfo.userId,
                                         fileInfo.groupId,
                                         fileInfo.permission,
                                         cryptAlgorithm,
                                         cryptType
                                        );
                  }
                  else
                  {
                    // output file info
                    if (!printedInfoFlag)
                    {
                      printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                      printedInfoFlag = TRUE;
                    }
                    printDirectoryInfo(NULL,
                                       directoryName,
                                       fileInfo.timeModified,
                                       fileInfo.userId,
                                       fileInfo.groupId,
                                       fileInfo.permission,
                                       cryptAlgorithm,
                                       cryptType
                                      );
                  }
                  fileCount++;
                }

                // close archive file, free resources
                error = Archive_closeEntry(&archiveEntryInfo);
                if (error != ERROR_NONE)
                {
                  printWarning("close 'directory' entry fail (error: %s)\n",Error_getText(error));
                }

                // free resources
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

                // read archive link
                linkName = String_new();
                fileName = String_new();
                error = Archive_readLinkEntry(&archiveEntryInfo,
                                              &archiveInfo,
                                              &cryptAlgorithm,
                                              &cryptType,
                                              linkName,
                                              fileName,
                                              &fileInfo,
                                              NULL   // fileExtendedAttributeList
                                             );
                if (error != ERROR_NONE)
                {
                  printError("Cannot read 'link' content from storage '%s' (error: %s)!\n",
                             Storage_getPrintableNameCString(storageSpecifier,archiveName),
                             Error_getText(error)
                            );
                  String_delete(fileName);
                  String_delete(linkName);
                  break;
                }

                if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                    && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
                   )
                {
                  if (globalOptions.groupFlag)
                  {
                    // add link info to list
                    addListLinkInfo(Storage_getName(storageSpecifier,NULL),
                                    linkName,
                                    fileName,
                                    fileInfo.timeModified,
                                    fileInfo.userId,
                                    fileInfo.groupId,
                                    fileInfo.permission,
                                    cryptAlgorithm,
                                    cryptType
                                   );
                  }
                  else
                  {
                    // output file info
                    if (!printedInfoFlag)
                    {
                      printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                      printedInfoFlag = TRUE;
                    }
                    printLinkInfo(NULL,
                                  linkName,
                                  fileName,
                                  fileInfo.timeModified,
                                  fileInfo.userId,
                                  fileInfo.groupId,
                                  fileInfo.permission,
                                  cryptAlgorithm,
                                  cryptType
                                 );
                  }
                  fileCount++;
                }

                // close archive file, free resources
                error = Archive_closeEntry(&archiveEntryInfo);
                if (error != ERROR_NONE)
                {
                  printWarning("close 'link' entry fail (error: %s)\n",Error_getText(error));
                }

                // free resources
                String_delete(fileName);
                String_delete(linkName);
              }
              break;
            case ARCHIVE_ENTRY_TYPE_HARDLINK:
              {
                ArchiveEntryInfo   archiveEntryInfo;
                CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
                CryptAlgorithms    cryptAlgorithm;
                CryptTypes         cryptType;
                StringList         fileNameList;
                FileInfo           fileInfo;
                String             deltaSourceName;
                uint64             deltaSourceSize;
                uint64             fragmentOffset,fragmentSize;
                StringNode         *stringNode;
                String             fileName;

                // read archive hard link
                StringList_init(&fileNameList);
                deltaSourceName = String_new();
                error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                                  &archiveInfo,
                                                  &deltaCompressAlgorithm,
                                                  &byteCompressAlgorithm,
                                                  &cryptAlgorithm,
                                                  &cryptType,
                                                  &fileNameList,
                                                  &fileInfo,
                                                  NULL,  // fileExtendedAttributeList
                                                  deltaSourceName,
                                                  &deltaSourceSize,
                                                  &fragmentOffset,
                                                  &fragmentSize
                                                 );
                if (error != ERROR_NONE)
                {
                  printError("Cannot read 'hard link' content from storage '%s' (error: %s)!\n",
                             Storage_getPrintableNameCString(storageSpecifier,archiveName),
                             Error_getText(error)
                            );
                  String_delete(deltaSourceName);
                  StringList_done(&fileNameList);
                  break;
                }

                STRINGLIST_ITERATE(&fileNameList,stringNode,fileName)
                {
                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add file info to list
                      addListHardLinkInfo(Storage_getName(storageSpecifier,NULL),
                                          fileName,
                                          fileInfo.size,
                                          fileInfo.timeModified,
                                          fileInfo.userId,
                                          fileInfo.groupId,
                                          fileInfo.permission,
                                          archiveEntryInfo.hardLink.chunkHardLinkData.info.size,
                                          deltaCompressAlgorithm,
                                          byteCompressAlgorithm,
                                          cryptAlgorithm,
                                          cryptType,
                                          deltaSourceName,
                                          deltaSourceSize,
                                          fragmentOffset,
                                          fragmentSize
                                         );
                    }
                    else
                    {
                      // output file info
                      if (!printedInfoFlag)
                      {
                        printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                        printedInfoFlag = TRUE;
                      }
                      printHardLinkInfo(NULL,
                                        fileName,
                                        fileInfo.size,
                                        fileInfo.timeModified,
                                        fileInfo.userId,
                                        fileInfo.groupId,
                                        fileInfo.permission,
                                        archiveEntryInfo.hardLink.chunkHardLinkData.info.size,
                                        deltaCompressAlgorithm,
                                        byteCompressAlgorithm,
                                        cryptAlgorithm,
                                        cryptType,
                                        deltaSourceName,
                                        deltaSourceSize,
                                        fragmentOffset,
                                        fragmentSize
                                       );
                    }
                    fileCount++;
                  }
                }

                // close archive file, free resources
                error = Archive_closeEntry(&archiveEntryInfo);
                if (error != ERROR_NONE)
                {
                  printWarning("close 'hard link' entry fail (error: %s)\n",Error_getText(error));
                }

                // free resources
                String_delete(deltaSourceName);
                StringList_done(&fileNameList);
              }
              break;
            case ARCHIVE_ENTRY_TYPE_SPECIAL:
              {
                CryptAlgorithms cryptAlgorithm;
                CryptTypes      cryptType;
                String          fileName;
                FileInfo        fileInfo;

                // open archive lin
                fileName = String_new();
                error = Archive_readSpecialEntry(&archiveEntryInfo,
                                                 &archiveInfo,
                                                 &cryptAlgorithm,
                                                 &cryptType,
                                                 fileName,
                                                 &fileInfo,
                                                 NULL   // fileExtendedAttributeList
                                                );
                if (error != ERROR_NONE)
                {
                  printError("Cannot read 'special' content from storage '%s' (error: %s)!\n",
                             Storage_getPrintableNameCString(storageSpecifier,archiveName),
                             Error_getText(error)
                            );
                  String_delete(fileName);
                  break;
                }

                if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                    && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                   )
                {
                  if (globalOptions.groupFlag)
                  {
                    // add special info to list
                    addListSpecialInfo(Storage_getName(storageSpecifier,NULL),
                                       fileName,
                                       fileInfo.userId,
                                       fileInfo.groupId,
                                       fileInfo.permission,
                                       cryptAlgorithm,
                                       cryptType,
                                       fileInfo.specialType,
                                       fileInfo.major,
                                       fileInfo.minor
                                      );
                  }
                  else
                  {
                    // output file info
                    if (!printedInfoFlag)
                    {
                      printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                      printedInfoFlag = TRUE;
                    }
                    printSpecialInfo(NULL,
                                     fileName,
                                     fileInfo.userId,
                                     fileInfo.groupId,
                                     fileInfo.permission,
                                     cryptAlgorithm,
                                     cryptType,
                                     fileInfo.specialType,
                                     fileInfo.major,
                                     fileInfo.minor
                                    );
                  }
                  fileCount++;
                }

                // close archive file, free resources
                error = Archive_closeEntry(&archiveEntryInfo);
                if (error != ERROR_NONE)
                {
                  printWarning("close 'special' entry fail (error: %s)\n",Error_getText(error));
                }

                // free resources
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

        // close archive
        Archive_close(&archiveInfo);

        // done storage
        (void)Storage_done(&storageInfo);
      }
      break;
    case STORAGE_TYPE_SSH:
    case STORAGE_TYPE_SCP:
      {
        #define TIMEOUT (60*1000)

        SSHServer            sshServer;
        String               s;
        Password             sshPassword;
        NetworkExecuteHandle networkExecuteHandle;
        String               line;
        uint                 protocolVersionMajor,protocolVersionMinor;
        Errors               error;
        uint                 retryCount;
        int                  id,errorCode;
        bool                 completedFlag;
        String               arguments;
        StringMap            argumentMap;
        String               type;
        bool                 parseOK;
        int                  exitcode;

        // start remote BAR via SSH (if not already started)
        if (!remoteBarFlag)
        {

          // get SSH server settings
          getSSHServerSettings(storageSpecifier->hostName,jobOptions,&sshServer);
          if (String_isEmpty(storageSpecifier->loginName)) String_set(storageSpecifier->loginName,sshServer.loginName);
          if (String_isEmpty(storageSpecifier->loginName)) String_setCString(storageSpecifier->loginName,getenv("LOGNAME"));
          if (String_isEmpty(storageSpecifier->loginName)) String_setCString(storageSpecifier->loginName,getenv("USER"));
          if (storageSpecifier->hostPort == 0) storageSpecifier->hostPort = sshServer.port;
          if (String_isEmpty(storageSpecifier->hostName))
          {
            printError("No host name given!\n");
            error = ERROR_NO_HOST_NAME;
            break;
          }
          if (sshServer.publicKey.data == NULL)
          {
            printError("Cannot SSH public key given!\n");
            error = ERROR_NO_SSH_PUBLIC_KEY;
            break;
          }
          if (sshServer.privateKey.data == NULL)
          {
            printError("Cannot SSH private key given!\n");
            error = ERROR_NO_SSH_PRIVATE_KEY;
            break;
          }

          // connect to SSH server
          error = Network_connect(&socketHandle,
                                  SOCKET_TYPE_SSH,
                                  storageSpecifier->hostName,
                                  storageSpecifier->hostPort,
                                  storageSpecifier->loginName,
                                  sshServer.password,
                                  sshServer.publicKey.data,
                                  sshServer.publicKey.length,
                                  sshServer.privateKey.data,
                                  sshServer.privateKey.length,
                                  0
                                 );
          if (error != ERROR_NONE)
          {
            Password_init(&sshPassword);

            s = String_newCString("SSH login password for ");
            if (!String_isEmpty(storageSpecifier->loginName)) String_format(s,"%S@",storageSpecifier->loginName);
            String_format(s,"%S",storageSpecifier->hostName);
            Password_input(&sshPassword,String_cString(s),PASSWORD_INPUT_MODE_ANY);
            String_delete(s);

            error = Network_connect(&socketHandle,
                                    SOCKET_TYPE_SSH,
                                    storageSpecifier->hostName,
                                    storageSpecifier->hostPort,
                                    storageSpecifier->loginName,
                                    &sshPassword,
                                    sshServer.publicKey.data,
                                    sshServer.publicKey.length,
                                    sshServer.privateKey.data,
                                    sshServer.privateKey.length,
                                    0
                                   );

            Password_done(&sshPassword);
          }
          if (error != ERROR_NONE)
          {
            printError("Cannot connect to '%s:%d' (error: %s)!\n",
                       String_cString(storageSpecifier->hostName),
                       storageSpecifier->hostPort,
                       Error_getText(error)
                      );
            break;
          }

          remoteBarFlag = TRUE;
        }

        // start remote BAR in batch mode, check protocol version
        line = String_format(String_new(),"%s --batch",!String_isEmpty(globalOptions.remoteBARExecutable) ? String_cString(globalOptions.remoteBARExecutable) : "bar");
        error = Network_execute(&networkExecuteHandle,
                                &socketHandle,
                                NETWORK_EXECUTE_IO_MASK_STDOUT|NETWORK_EXECUTE_IO_MASK_STDERR,
                                String_cString(line)
                               );
        if (error != ERROR_NONE)
        {
          printError("Cannot execute remote BAR program '%s' (error: %s)!\n",
                     String_cString(line),
                     Error_getText(error)
                    );
          String_delete(line);
          break;
        }
        if (Network_executeEOF(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,TIMEOUT))
        {
          Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,line,0);
          exitcode = Network_terminate(&networkExecuteHandle);
          printError("No response from remote BAR program (error: %s, exitcode %d)!\n",!String_isEmpty(line) ? String_cString(line) : "unknown",exitcode);
          String_delete(line);
          break;
        }
        Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,line,TIMEOUT);
        if (!String_parse(line,STRING_BEGIN,"BAR VERSION %d %d",NULL,&protocolVersionMajor,&protocolVersionMinor))
        {
          Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,line,TIMEOUT);
          exitcode = Network_terminate(&networkExecuteHandle);
          printError("Invalid response from remote BAR program (error: %s, exitcode %d)!\n",!String_isEmpty(line) ? String_cString(line) : "unknown",exitcode);
          String_delete(line);
          break;
        }
        if (protocolVersionMajor != SERVER_PROTOCOL_VERSION_MAJOR)
        {
          (void)Network_terminate(&networkExecuteHandle);
          printError("Invalid BAR major protocol version (expected: %d, got %d)!\n",SERVER_PROTOCOL_VERSION_MAJOR,protocolVersionMajor);
          String_delete(line);
          break;
        }
        if (protocolVersionMinor != SERVER_PROTOCOL_VERSION_MINOR)
        {
          printWarning("Invalid BAR minor protocol version (expected: %d, got %d)!\n",SERVER_PROTOCOL_VERSION_MAJOR,protocolVersionMinor);
        }
        String_delete(line);

        // read archive content
        error       = ERROR_UNKNOWN;
        line        = String_new();
        arguments   = String_new();
        argumentMap = StringMap_new();
        type        = String_new();
        retryCount  = 3;
        do
        {
          // next retry
          retryCount--;
          if (retryCount <= 0) break;

          // send decrypt password
          if (!Password_isEmpty(jobOptions->cryptPassword))
          {
            String_format(String_clear(line),"1 DECRYPT_PASSWORD_ADD encryptType=none encryptedPassword=%'s",jobOptions->cryptPassword);
            Network_executeWriteLine(&networkExecuteHandle,line);
          }

          // send list archive command
          String_format(String_clear(line),"2 ARCHIVE_LIST name=%S",storageSpecifier->archiveName);
          Network_executeWriteLine(&networkExecuteHandle,line);
          Network_executeSendEOF(&networkExecuteHandle);

          // list contents
          while (!Network_executeEOF(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,TIMEOUT))
          {
            // read line
            Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,line,TIMEOUT);
//fprintf(stderr,"%s, %d: line=%s\n",__FILE__,__LINE__,String_cString(line));

            // parse
            if (!String_parse(line,
                                  STRING_BEGIN,
                                  "%d %d %y % S",
                                  NULL,
                                  &id,
                                  &errorCode,
                                  &completedFlag,
                                  arguments
                                 )
                    )
            {
              error = ERROR_INVALID_RESPONSE;
              break;
            }
//fprintf(stderr,"%s, %d: type=#%s# arguments=%s\n",__FILE__,__LINE__,type,String_cString(arguments));
            StringMap_clear(argumentMap);
            if (!StringMap_parse(argumentMap,arguments,STRINGMAP_ASSIGN,STRING_QUOTES,NULL,0,NULL))
            {
              error = ERROR_INVALID_RESPONSE;
              break;
            }

            if ((errorCode == ERROR_NONE) && !completedFlag)
            {
              // output list
              if (!StringMap_getString(argumentMap,"type",type,NULL))
              {
                error = ERROR_INVALID_RESPONSE;
                break;
              }

              if      (String_equalsCString(type,"FILE"))
              {
                String               fileName;
                uint64               fileSize;
                uint64               dateTime;
                uint32               userId,groupId;
                FilePermission       permission;
                uint64               archiveFileSize;
                CompressAlgorithms   deltaCompressAlgorithm;
                CompressAlgorithms   byteCompressAlgorithm;
                CryptAlgorithms      cryptAlgorithm;
                CryptTypes           cryptType;
                String               deltaSourceName;
                uint64               deltaSourceSize;
                uint64               fragmentOffset,fragmentSize;

                // initialize variables
                fileName        = String_new();
                deltaSourceName = String_new();

                // get values
                parseOK = TRUE;
                parseOK |= StringMap_getString(argumentMap,"name",fileName,NULL);
                parseOK |= StringMap_getUInt64(argumentMap,"size",&fileSize,0LL);
                parseOK |= StringMap_getUInt64(argumentMap,"dateTime",&dateTime,0LL);
                parseOK |= StringMap_getUInt(argumentMap,"userId",&userId,0);
                parseOK |= StringMap_getUInt(argumentMap,"groupId",&groupId,0);
                parseOK |= StringMap_getUInt(argumentMap,"permission",&permission,0);
                parseOK |= StringMap_getUInt64(argumentMap,"archiveFileSize",&archiveFileSize,0LL);
                parseOK |= StringMap_getEnum(argumentMap,"deltaCompressAlgorithm",&deltaCompressAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,COMPRESS_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"byteCompressAlgorithm",&byteCompressAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,COMPRESS_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"cryptAlgorithm",&cryptAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"cryptType",&cryptType,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_TYPE_NONE);
                parseOK |= StringMap_getString(argumentMap,"deltaSourceName",deltaSourceName,NULL);
                parseOK |= StringMap_getUInt64(argumentMap,"deltaSourceSize",&deltaSourceSize,0LL);
                parseOK |= StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL);
                parseOK |= StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL);

                // output
                if (parseOK)
                {
                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add file info to list
                      addListFileInfo(Storage_getName(storageSpecifier,NULL),
                                      fileName,
                                      fileSize,
                                      dateTime,
                                      permission,
                                      userId,
                                      groupId,
                                      archiveFileSize,
                                      deltaCompressAlgorithm,
                                      byteCompressAlgorithm,
                                      cryptAlgorithm,
                                      cryptType,
                                      deltaSourceName,
                                      deltaSourceSize,
                                      fragmentOffset,
                                      fragmentSize
                                     );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                        printedInfoFlag = TRUE;
                      }

                      // output file info
                      printFileInfo(NULL,
                                    fileName,
                                    fileSize,
                                    dateTime,
                                    userId,
                                    groupId,
                                    permission,
                                    archiveFileSize,
                                    deltaCompressAlgorithm,
                                    byteCompressAlgorithm,
                                    cryptAlgorithm,
                                    cryptType,
                                    deltaSourceName,
                                    deltaSourceSize,
                                    fragmentOffset,
                                    fragmentSize
                                   );
                    }
                    fileCount++;
                  }
                }
                else
                {
                  printWarning("Parse 'file' entry '%s' fail\n",String_cString(arguments));
                }

                // free resources
                String_delete(deltaSourceName);
                String_delete(fileName);
              }
              else if (String_equalsCString(type,"IMAGE"))
              {
                String             imageName;
                uint64             imageSize;
                uint64             archiveFileSize;
                CompressAlgorithms deltaCompressAlgorithm;
                CompressAlgorithms byteCompressAlgorithm;
                CryptAlgorithms    cryptAlgorithm;
                CryptTypes         cryptType;
                String             deltaSourceName;
                uint64             deltaSourceSize;
                uint               blockSize;
                uint64             blockOffset,blockCount;

                // initialize variables
                imageName       = String_new();
                deltaSourceName = String_new();

                // get values
                parseOK = TRUE;
                parseOK |= StringMap_getString(argumentMap,"name",imageName,NULL);
                parseOK |= StringMap_getUInt64(argumentMap,"size",&imageSize,0LL);
                parseOK |= StringMap_getUInt64(argumentMap,"archiveFileSize",&archiveFileSize,0LL);
                parseOK |= StringMap_getEnum(argumentMap,"deltaCompressAlgorithm",&deltaCompressAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,COMPRESS_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"byteCompressAlgorithm",&byteCompressAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,COMPRESS_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"cryptAlgorithm",&cryptAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"cryptType",&cryptType,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_TYPE_NONE);
                parseOK |= StringMap_getString(argumentMap,"deltaSourceName",deltaSourceName,NULL);
                parseOK |= StringMap_getUInt64(argumentMap,"deltaSourceSize",&deltaSourceSize,0LL);
                parseOK |= StringMap_getUInt(argumentMap,"blockSize",&blockSize,0);
                parseOK |= StringMap_getUInt64(argumentMap,"blockOffset",&blockOffset,0LL);
                parseOK |= StringMap_getUInt64(argumentMap,"blockCount",&blockCount,0LL);

                // output
                if (parseOK)
                {
                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,imageName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,imageName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add file info to list
                      addListImageInfo(Storage_getName(storageSpecifier,NULL),
                                      imageName,
                                      imageSize,
                                      archiveFileSize,
                                      deltaCompressAlgorithm,
                                      byteCompressAlgorithm,
                                      cryptAlgorithm,
                                      cryptType,
                                      deltaSourceName,
                                      deltaSourceSize,
                                      blockSize,
                                      blockOffset,
                                      blockCount
                                     );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                        printedInfoFlag = TRUE;
                      }

                      // output file info
                      printImageInfo(NULL,
                                     imageName,
                                     imageSize,
                                     archiveFileSize,
                                     deltaCompressAlgorithm,
                                     byteCompressAlgorithm,
                                     cryptAlgorithm,
                                     cryptType,
                                     deltaSourceName,
                                     deltaSourceSize,
                                     blockSize,
                                     blockOffset,
                                     blockCount
                                    );
                    }
                    fileCount++;
                  }
                }
                else
                {
                  printWarning("Parse 'image' entry '%s' fail\n",String_cString(arguments));
                }

                // free resources
                String_delete(deltaSourceName);
                String_delete(imageName);
              }
              else if (String_equalsCString(type,"DIRECTORY"))
              {
                String          directoryName;
                uint64          dateTime;
                uint32          userId,groupId;
                FilePermission  permission;
                CryptAlgorithms cryptAlgorithm;
                CryptTypes      cryptType;

                // initialize variables
                directoryName = String_new();

                // get values
                parseOK = TRUE;
                parseOK |= StringMap_getString(argumentMap,"name",directoryName,NULL);
                parseOK |= StringMap_getUInt64(argumentMap,"dateTime",&dateTime,0LL);
                parseOK |= StringMap_getUInt(argumentMap,"userId",&userId,0);
                parseOK |= StringMap_getUInt(argumentMap,"groupId",&groupId,0);
                parseOK |= StringMap_getUInt(argumentMap,"permission",&permission,0);
                parseOK |= StringMap_getEnum(argumentMap,"cryptAlgorithm",&cryptAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"cryptType",&cryptType,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_TYPE_NONE);

                // output
                if (parseOK)
                {
                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add directory info to list
                      addListDirectoryInfo(Storage_getName(storageSpecifier,NULL),
                                           directoryName,
                                           dateTime,
                                           userId,
                                           groupId,
                                           permission,
                                           cryptAlgorithm,
                                           cryptType
                                          );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                        printedInfoFlag = TRUE;
                      }

                      // output file info
                      printDirectoryInfo(NULL,
                                         directoryName,
                                         dateTime,
                                         userId,
                                         groupId,
                                         permission,
                                         cryptAlgorithm,
                                         cryptType
                                        );
                    }
                    fileCount++;
                  }
                }
                else
                {
                  printWarning("Parse 'directory' entry '%s' fail\n",String_cString(arguments));
                }

                // free resources
                String_delete(directoryName);
              }
              else if (String_equalsCString(type,"LINK"))
              {
                String          linkName,fileName;
                uint64          dateTime;
                uint32          userId,groupId;
                FilePermission  permission;
                CryptAlgorithms cryptAlgorithm;
                CryptTypes      cryptType;

                // initialize variables
                linkName = String_new();
                fileName = String_new();

                // get values
                parseOK = TRUE;
                parseOK |= StringMap_getString(argumentMap,"name",linkName,NULL);
                parseOK |= StringMap_getString(argumentMap,"name",fileName,NULL);
                parseOK |= StringMap_getUInt64(argumentMap,"dateTime",&dateTime,0LL);
                parseOK |= StringMap_getUInt(argumentMap,"userId",&userId,0);
                parseOK |= StringMap_getUInt(argumentMap,"groupId",&groupId,0);
                parseOK |= StringMap_getUInt(argumentMap,"permission",&permission,0);
                parseOK |= StringMap_getEnum(argumentMap,"cryptAlgorithm",&cryptAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"cryptType",&cryptType,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_TYPE_NONE);

                // output
                if (parseOK)
                {
                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add linkinfo to list
                      addListLinkInfo(Storage_getName(storageSpecifier,NULL),
                                      linkName,
                                      fileName,
                                      dateTime,
                                      userId,
                                      groupId,
                                      permission,
                                      cryptAlgorithm,
                                      cryptType
                                     );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                        printedInfoFlag = TRUE;
                      }

                      // output file info
                      printLinkInfo(NULL,
                                    linkName,
                                    fileName,
                                    dateTime,
                                    userId,
                                    groupId,
                                    permission,
                                    cryptAlgorithm,
                                    cryptType
                                   );
                    }
                    fileCount++;
                  }
                }
                else
                {
                  printWarning("Parse 'link' entry '%s' fail\n",String_cString(arguments));
                }

                // free resources
                String_delete(fileName);
                String_delete(linkName);
              }
              else if (String_equalsCString(type,"HARDLINK"))
              {
                String               fileName;
                uint64               fileSize;
                uint64               dateTime;
                uint32               userId,groupId;
                FilePermission       permission;
                uint64               archiveFileSize;
                CompressAlgorithms   deltaCompressAlgorithm;
                CompressAlgorithms   byteCompressAlgorithm;
                CryptAlgorithms      cryptAlgorithm;
                CryptTypes           cryptType;
                String               deltaSourceName;
                uint64               deltaSourceSize;
                uint64               fragmentOffset,fragmentSize;

                // initialize variables
                fileName        = String_new();
                deltaSourceName = String_new();

                // get values
                parseOK = TRUE;
                parseOK |= StringMap_getString(argumentMap,"name",fileName,NULL);
                parseOK |= StringMap_getUInt64(argumentMap,"size",&fileSize,0LL);
                parseOK |= StringMap_getUInt64(argumentMap,"dateTime",&dateTime,0LL);
                parseOK |= StringMap_getUInt(argumentMap,"userId",&userId,0);
                parseOK |= StringMap_getUInt(argumentMap,"groupId",&groupId,0);
                parseOK |= StringMap_getUInt(argumentMap,"permission",&permission,0);
                parseOK |= StringMap_getUInt64(argumentMap,"archiveFileSize",&archiveFileSize,0LL);
                parseOK |= StringMap_getEnum(argumentMap,"deltaCompressAlgorithm",&deltaCompressAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,COMPRESS_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"byteCompressAlgorithm",&byteCompressAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,COMPRESS_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"cryptAlgorithm",&cryptAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"cryptType",&cryptType,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_TYPE_NONE);
                parseOK |= StringMap_getString(argumentMap,"deltaSourceName",deltaSourceName,NULL);
                parseOK |= StringMap_getUInt64(argumentMap,"deltaSourceSize",&deltaSourceSize,0LL);
                parseOK |= StringMap_getUInt64(argumentMap,"fragmentOffset",&fragmentOffset,0LL);
                parseOK |= StringMap_getUInt64(argumentMap,"fragmentSize",&fragmentSize,0LL);

                // output
                if (parseOK)
                {
                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add file info to list
                      addListHardLinkInfo(Storage_getName(storageSpecifier,NULL),
                                          fileName,
                                          fileSize,
                                          dateTime,
                                          userId,
                                          groupId,
                                          permission,
                                          archiveFileSize,
                                          deltaCompressAlgorithm,
                                          byteCompressAlgorithm,
                                          cryptAlgorithm,
                                          cryptType,
                                          deltaSourceName,
                                          deltaSourceSize,
                                          fragmentOffset,
                                          fragmentSize
                                         );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                        printedInfoFlag = TRUE;
                      }

                      // output file info
                      printHardLinkInfo(NULL,
                                        fileName,
                                        fileSize,
                                        dateTime,
                                        userId,
                                        groupId,
                                        permission,
                                        archiveFileSize,
                                        deltaCompressAlgorithm,
                                        byteCompressAlgorithm,
                                        cryptAlgorithm,
                                        cryptType,
                                        deltaSourceName,
                                        deltaSourceSize,
                                        fragmentOffset,
                                        fragmentSize
                                       );
                    }
                    fileCount++;
                  }
                }
                else
                {
                  printWarning("Parse 'hardlink' entry '%s' fail\n",String_cString(arguments));
                }

                // free resources
                String_delete(deltaSourceName);
                String_delete(fileName);
              }
              else if (String_equalsCString(type,"SPECIAL"))
              {
                String           fileName;
                uint32           userId,groupId;
                FilePermission   permission;
                CryptAlgorithms  cryptAlgorithm;
                CryptTypes       cryptType;
                FileSpecialTypes fileSpecialType;
                uint32           major;
                uint32           minor;

                // initialize variables
                fileName = String_new();

                // get values
                parseOK = TRUE;
                parseOK |= StringMap_getString(argumentMap,"name",fileName,NULL);
                parseOK |= StringMap_getUInt(argumentMap,"userId",&userId,0);
                parseOK |= StringMap_getUInt(argumentMap,"groupId",&groupId,0);
                parseOK |= StringMap_getUInt(argumentMap,"permission",&permission,0);
                parseOK |= StringMap_getEnum(argumentMap,"cryptAlgorithm",&cryptAlgorithm,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_ALGORITHM_UNKNOWN);
                parseOK |= StringMap_getEnum(argumentMap,"cryptType",&cryptType,(StringMapParseEnumFunction)StringMap_parseEnumNumber,CRYPT_TYPE_NONE);
                parseOK |= StringMap_getEnum(argumentMap,"fileSpecialType",&fileSpecialType,(StringMapParseEnumFunction)StringMap_parseEnumNumber,FILE_SPECIAL_TYPE_OTHER);
                parseOK |= StringMap_getUInt(argumentMap,"major",&major,0);
                parseOK |= StringMap_getUInt(argumentMap,"minor",&minor,0);

                // output
                if (parseOK)
                {
                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add special info to list
                      addListSpecialInfo(Storage_getName(storageSpecifier,NULL),
                                         fileName,
                                         userId,
                                         groupId,
                                         permission,
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
                        printArchiveListHeader(Storage_getPrintableName(storageSpecifier,archiveName));
                        printedInfoFlag = TRUE;
                      }

                      // output file info
                      printSpecialInfo(NULL,
                                       fileName,
                                       userId,
                                       groupId,
                                       permission,
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
                  printWarning("Parse 'special' entry '%s' fail\n",String_cString(arguments));
                }

                // free resources
                String_delete(fileName);
              }
              else
              {
                printWarning("Unknown entry '%s' fail\n",String_cString(line));
              }
            }
          }
          if (isPrintInfo(4))
          {
            while (!Network_executeEOF(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,60*1000))
            {
              Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,line,0);
              if (String_length(line)>0) fprintf(stderr,"%s\n",String_cString(line));
            }
          }
        }
        while (error != ERROR_NONE);
        String_delete(type);
        StringMap_delete(argumentMap);
        String_delete(arguments);
        String_delete(line);

        // close connection
        exitcode = Network_terminate(&networkExecuteHandle);
        if (exitcode != 0)
        {
          error = ERROR_NETWORK_EXECUTE_FAIL;
        }

        // free resources
      }
      break;
    case STORAGE_TYPE_DEVICE:
      printError("List archives on device is not supported!\n");
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  if (printedInfoFlag)
  {
    printArchiveListFooter(fileCount);
  }

  // output grouped list
  if (globalOptions.groupFlag)
  {
    printArchiveListHeader(NULL);
    printArchiveList();
    printArchiveListFooter(List_count(&archiveContentList));
  }

  return error;
}

/***********************************************************************\
* Name   : printDirectoryListHeader
* Purpose: print directory list header
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printDirectoryListHeader(ConstString storageName)
{
  const TextMacro MACROS[] =
  {
    TEXT_MACRO_CSTRING("%type",      "Type",      NULL),
    TEXT_MACRO_CSTRING("%size",      "Size",      NULL),
    TEXT_MACRO_CSTRING("%dateTime",  "Date/Time" ,NULL),
    TEXT_MACRO_CSTRING("%user",      "User",      NULL),
    TEXT_MACRO_CSTRING("%group",     "Group",     NULL),
    TEXT_MACRO_CSTRING("%permission","Permission",NULL),
    TEXT_MACRO_CSTRING("%name",      "Name",      NULL)
  };

  String line;

  if (!globalOptions.noHeaderFooterFlag)
  {
    // init variables
    line = String_new();

    // header
    if (storageName != NULL)
    {
      printInfo(0,"List directory '%s':\n",String_cString(storageName));
      printInfo(0,"\n");
    }

    // print title line
    printInfo(0,
              "%s\n",
              String_cString(Misc_expandMacros(line,
                                               DEFAULT_DIRECTORY_LIST_FORMAT_TITLE,
                                               EXPAND_MACRO_MODE_STRING,
                                               MACROS,SIZE_OF_ARRAY(MACROS),
                                               TRUE
                                              )
                            )
             );
    printInfo(0,
              "%s\n",
              String_cString(String_fillChar(line,
                                             Misc_getConsoleColumns(),
                                             '-'
                                            )
                            )
             );

    // free resources
    String_delete(line);
  }
}

/***********************************************************************\
* Name   : printDirectoryListFooter
* Purpose: print directory list footer
* Input  : fileCount - number of files listed
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printDirectoryListFooter(ulong fileCount)
{
  String line;

  if (!globalOptions.noHeaderFooterFlag)
  {
    line = String_new();

    String_fillChar(line,Misc_getConsoleColumns(),'-');
    printInfo(0,"%s\n",String_cString(line));
    printInfo(0,"%lu %s\n",fileCount,(fileCount == 1) ? "entry" : "entries");
    printInfo(0,"\n");

    String_delete(line);
  }
}

/***********************************************************************\
* Name   : listDirectoryContent
* Purpose: list directory content
* Input  : storageDirectoryListHandle - storage directory list handle
*          storageSpecifier           - storage specifier
*          includeEntryList           - include entry list
*          excludePatternList         - exclude pattern list
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors listDirectoryContent(StorageDirectoryListHandle *storageDirectoryListHandle,
                                  StorageSpecifier           *storageSpecifier,
                                  const EntryList            *includeEntryList,
                                  const PatternList          *excludePatternList
                                 )
{
  String    fileName,dateTimeString;
  ulong     fileCount;
  String    line;
  char      userName[12],groupName[12];
  char      permissionString[10];
  TextMacro textMacros[7];
  Errors    error;
  FileInfo  fileInfo;
  char      buffer[16];

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  printDirectoryListHeader(Storage_getPrintableName(storageSpecifier,NULL));

  fileName       = String_new();
  dateTimeString = String_new();
  fileCount      = 0;
  line           = String_new();
  while (!Storage_endOfDirectoryList(storageDirectoryListHandle))
  {
    // read next directory entry
    error = Storage_readDirectoryList(storageDirectoryListHandle,fileName,&fileInfo);
    if (error != ERROR_NONE)
    {
      String_delete(line);
      String_delete(dateTimeString);
      String_delete(fileName);
      return error;
    }

    if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
        && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
       )
    {
      // format
      switch (fileInfo.type)
      {
        case FILE_TYPE_NONE:
          break;
        case FILE_TYPE_FILE:
        case FILE_TYPE_HARDLINK:
          TEXT_MACRO_N_CSTRING(textMacros[0],"%type","FILE",NULL);
          if (globalOptions.humanFormatFlag)
          {
            getHumanSizeString(buffer,sizeof(buffer),fileInfo.size);
          }
          else
          {
            snprintf(buffer,sizeof(buffer),"%llu",fileInfo.size);
          }
          TEXT_MACRO_N_CSTRING(textMacros[1],"%size",buffer,NULL);
          break;
        case FILE_TYPE_DIRECTORY:
          TEXT_MACRO_N_CSTRING(textMacros[0],"%type","DIR",NULL);
          TEXT_MACRO_N_CSTRING(textMacros[1],"%size","",   NULL);
          break;
        case FILE_TYPE_LINK:
          TEXT_MACRO_N_CSTRING(textMacros[0],"%type","LINK",NULL);
          TEXT_MACRO_N_CSTRING(textMacros[1],"%size","",    NULL);
          break;
        case FILE_TYPE_SPECIAL:
          TEXT_MACRO_N_CSTRING(textMacros[0],"%type","SEPCIAL",NULL);
          TEXT_MACRO_N_CSTRING(textMacros[1],"%size","",       NULL);
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break; /* not reached */
      }
      Misc_formatDateTime(dateTimeString,fileInfo.timeModified,NULL);

      if (globalOptions.numericUIDGIDFlag)
      {
        snprintf(userName,sizeof(userName),"%d",fileInfo.userId);
        snprintf(groupName,sizeof(groupName),"%d",fileInfo.groupId);
      }
      else
      {
        File_userIdToUserName(userName,sizeof(userName),fileInfo.userId);
        File_groupIdToGroupName(groupName,sizeof(groupName),fileInfo.groupId);
      }
      if (globalOptions.numericPermissionFlag)
      {
        snprintf(permissionString,sizeof(permissionString),"%4o",fileInfo.permission & FILE_PERMISSION_ALL);
      }
      else
      {
        File_permissionToString(permissionString,sizeof(permissionString),fileInfo.permission);
      }

      TEXT_MACRO_N_STRING (textMacros[2],"%dateTime",  dateTimeString,  NULL);
      TEXT_MACRO_N_CSTRING(textMacros[3],"%user",      userName,        NULL);
      TEXT_MACRO_N_CSTRING(textMacros[4],"%group",     groupName,       NULL);
      TEXT_MACRO_N_STRING (textMacros[5],"%permission",permissionString,NULL);
      TEXT_MACRO_N_STRING (textMacros[6],"%name",      fileName,        NULL);

      // print
      printInfo(0,"%s\n",
                String_cString(Misc_expandMacros(String_clear(line),
                                                 DEFAULT_DIRECTORY_LIST_FORMAT,
                                                 EXPAND_MACRO_MODE_STRING,
                                                 textMacros,SIZE_OF_ARRAY(textMacros),
                                                 TRUE
                                                )
                              )
               );

      // next
      fileCount++;
    }
  }
  String_delete(line);
  String_delete(dateTimeString);
  String_delete(fileName);

  printDirectoryListFooter(fileCount);

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Command_list(StringList          *storageNameList,
                    const EntryList     *includeEntryList,
                    const PatternList   *excludePatternList,
                    JobOptions          *jobOptions,
                    GetPasswordFunction getPasswordFunction,
                    void                *getPasswordUserData,
                    LogHandle           *logHandle
                   )
{
  String                     storageName;
  StorageSpecifier           storageSpecifier;
  Errors                     failError;
  Errors                     error;
  StorageDirectoryListHandle storageDirectoryListHandle;
  Pattern                    pattern;
  String                     fileName;

  assert(storageNameList != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

  // init variables
  List_init(&archiveContentList);
  storageName = String_new();
  Storage_initSpecifier(&storageSpecifier);

  // list archive content
  failError = ERROR_NONE;
  while (!StringList_isEmpty(storageNameList))
  {
    StringList_removeFirst(storageNameList,storageName);

    // parse storage name
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error != ERROR_NONE)
    {
      printError("Invalid storage '%s' (error: %s)!\n",
                 String_cString(storageName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }

    error = ERROR_UNKNOWN;

    if (error != ERROR_NONE)
    {
      if (String_isEmpty(storageSpecifier.archivePatternString))
      {
        // list archive content
        error = listArchiveContent(&storageSpecifier,
                                   NULL,  // archiveName
                                   includeEntryList,
                                   excludePatternList,
                                   jobOptions,
                                   getPasswordFunction,
                                   getPasswordUserData,
                                   logHandle
                                  );
        if (failError == ERROR_NONE) failError = error;
      }
    }
    if (error != ERROR_NONE)
    {
      // open directory list
      error = Storage_openDirectoryList(&storageDirectoryListHandle,
                                        &storageSpecifier,
                                        NULL,  // archiveName
                                        jobOptions,
                                        SERVER_CONNECTION_PRIORITY_HIGH
                                       );
      if (error == ERROR_NONE)
      {
        if (String_isEmpty(storageSpecifier.archivePatternString))
        {
          // list directory
          error = listDirectoryContent(&storageDirectoryListHandle,
                                       &storageSpecifier,
                                       includeEntryList,
                                       excludePatternList
                                      );
        }
        else
        {
          // list archive content of matching files
          error = Pattern_init(&pattern,storageSpecifier.archivePatternString,
                               jobOptions->patternType,
                               PATTERN_FLAG_NONE
                              );
          if (error == ERROR_NONE)
          {
            fileName = String_new();
            while (!Storage_endOfDirectoryList(&storageDirectoryListHandle) && (error == ERROR_NONE))
            {
              // read next directory entry
              error = Storage_readDirectoryList(&storageDirectoryListHandle,fileName,NULL);
              if (error != ERROR_NONE)
              {
                continue;
              }

              // match pattern
              if (!Pattern_match(&pattern,fileName,PATTERN_MATCH_MODE_EXACT))
              {
                continue;
              }

              // list archive content
              error = listArchiveContent(&storageSpecifier,
                                         fileName,
                                         includeEntryList,
                                         excludePatternList,
                                         jobOptions,
                                         getPasswordFunction,
                                         getPasswordUserData,
                                         logHandle
                                        );
            }
            String_delete(fileName);
            Pattern_done(&pattern);
          }
        }
        Storage_closeDirectoryList(&storageDirectoryListHandle);

        if (error != ERROR_NONE)
        {
          printError("Cannot open storage '%s' (error: %s)!\n",
                     String_cString(storageName),
                     Error_getText(error)
                    );
          if (failError == ERROR_NONE) failError = error;
          continue;
        }
      }
    }
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(storageName);
  List_done(&archiveContentList,(ListNodeFreeFunction)freeArchiveContentNode,NULL);

  return failError;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
