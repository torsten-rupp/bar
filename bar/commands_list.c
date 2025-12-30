/***********************************************************************\
*
* Contents: Backup ARchiver archive list functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "common/global.h"
#include "common/cstrings.h"
#include "common/autofree.h"
#include "common/strings.h"
#include "common/stringmaps.h"
#include "common/lists.h"
#include "common/stringlists.h"
#include "common/arrays.h"
#include "common/network.h"
#include "common/misc.h"
#include "common/patterns.h"
#include "common/patternlists.h"
#include "common/files.h"

#include "bar.h"
#include "errors.h"
#include "entrylists.h"
#include "archives.h"
#include "storage.h"
#include "server.h"

#include "commands_list.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_GROUP_PREFIX    "%storageName"
#define DEFAULT_ARCHIVE_LIST_FORMAT_LONG_GROUP_PREFIX     "%storageName"

#define DEFAULT_ARCHIVE_LIST_FORMAT_LONG_TITLE            "%type:-12 %user:-12 %group:-12 %permission:-10 %size:-12 %dateTime:-32 %{part                 :-32} %compress:-15 %ratio:  -7  %crypt:-10 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_LONG_FILE             "%type:-12 %user:-12 %group:-12 %permission:-10 %size: 12 %dateTime:-32 %{partFrom:15}..%{partTo:15} %compress:-15 %ratio: 7.1%% %crypt:-10 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_LONG_IMAGE            "%type:-12 %    :-12 %     :-12 %          :-10 %size: 12 %        :-32 %{partFrom:15}..%{partTo:15} %compress:-15 %ratio: 7.1%% %crypt:-10 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_LONG_DIR              "%type:-12 %user:-12 %group:-12 %permission:-10 %    : 12 %dateTime:-32 %{        :15}  %{      :15} %        :-15 %     :   7  %crypt:-10 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_LONG_LINK             "%type:-12 %user:-12 %group:-12 %permission:-10 %    : 12 %dateTime:-32 %{        :15}  %{      :15} %        :-15 %     :   7  %crypt:-10 %name -> %destinationName"
#define DEFAULT_ARCHIVE_LIST_FORMAT_LONG_HARDLINK         "%type:-12 %user:-12 %group:-12 %permission:-10 %size: 12 %dateTime:-32 %{partFrom:15}..%{partTo:15} %compress:-15 %ratio: 7.1%% %crypt:-10 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_LONG_SPECIAL_CHAR     "%type:-12 %user:-12 %group:-12 %permission:-10 %    : 12 %        :-32 %{        :15}  %{      :15} %        :-15 %     :   7  %crypt:-10 %name, %major %minor"
#define DEFAULT_ARCHIVE_LIST_FORMAT_LONG_SPECIAL_BLOCK    "%type:-12 %user:-12 %group:-12 %permission:-10 %    : 12 %        :-32 %{        :15}  %{      :15} %        :-15 %     :   7  %crypt:-10 %name, %major %minor"
#define DEFAULT_ARCHIVE_LIST_FORMAT_LONG_SPECIAL_FIFO     "%type:-12 %user:-12 %group:-12 %permission:-10 %    : 12 %        :-32 %{        :15}  %{      :15} %        :-15 %     :   7  %crypt:-10 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_LONG_SPECIAL_SOCKET   "%type:-12 %user:-12 %group:-12 %permission:-10 %    : 12 %        :-32 %{        :15}  %{      :15} %        :-15 %     :   7  %crypt:-10 %name"

// TODO: normal/long
#define DEFAULT_ARCHIVE_LIST_FORMAT_DELTA_SOURCE          "  delta source: %deltaSourceName, %deltaSourceSize"

#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_TITLE          "%type:-12 %size:-12 %dateTime:-32 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_FILE           "%type:-12 %size: 12 %dateTime:-32 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_IMAGE          "%type:-12 %size: 12 %        :-32 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_DIR            "%type:-12 %    : 12 %dateTime:-32 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_LINK           "%type:-12 %    : 12 %dateTime:-32 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_HARDLINK       "%type:-12 %size: 12 %dateTime:-32 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_CHAR   "%type:-12 %    : 12 %        :-32 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_BLOCK  "%type:-12 %    : 12 %        :-32 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_FIFO   "%type:-12 %    : 12 %        :-32 %name"
#define DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_SOCKET "%type:-12 %    : 12 %        :-32 %name"

#define DEFAULT_DIRECTORY_LIST_FORMAT_TITLE               "%type:-12 %size:-12 %dateTime:-32 %name"

#define DEFAULT_DIRECTORY_LIST_FORMAT                     "%type:-12 %size: 12 %dateTime:-32 %name"

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
      ConstString        name;
      uint64             size;
      uint64             timeModified;
      uint32             userId;
      uint32             groupId;
      FilePermissions    permissions;
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
      ConstString        name;
      uint64             size;
      uint64             archiveSize;
      CompressAlgorithms deltaCompressAlgorithm;
      CompressAlgorithms byteCompressAlgorithm;
      CryptAlgorithms    cryptAlgorithm;
      CryptTypes         cryptType;
      String             deltaSourceName;
      uint64             deltaSourceSize;
      FileSystemTypes    fileSystemType;
      uint               blockSize;
      uint64             blockOffset;
      uint64             blockCount;
    } image;
    struct
    {
      ConstString     name;
      uint64          timeModified;
      uint32          userId;
      uint32          groupId;
      FilePermissions permissions;
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
      FilePermissions permissions;
      CryptAlgorithms cryptAlgorithm;
      CryptTypes      cryptType;
    } link;
    struct
    {
      ConstString        name;
      uint64             size;
      uint64             timeModified;
      uint32             userId;
      uint32             groupId;
      FilePermissions    permissions;
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
      ConstString      name;
      uint32           userId;
      uint32           groupId;
      FilePermissions  permissions;
      CryptAlgorithms  cryptAlgorithm;
      CryptTypes       cryptType;
      FileSpecialTypes fileSpecialType;
      uint32           major;
      uint32           minor;
    } special;
  };

//  String       name;
  SocketHandle socketHandle;
} ArchiveContentNode;

// archive content list
typedef struct
{
  LIST_HEADER(ArchiveContentNode);
} ArchiveContentList;

//TODO
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
* Name   : printSeparator
* Purpose: print separator line
* Input  : ch - character
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printSeparator(char ch)
{
  String line = String_new();

  printConsole(stdout,
               0,
               "%s\n",
               String_cString(String_fillChar(line,
                                              Misc_getConsoleColumns(),
                                              ch
                                             )
                             )
              );

  String_delete(line);
}

/***********************************************************************\
* Name   : printArchiveName
* Purpose: print archive name
* Input  : showEntriesFlag - TRUE to show entries
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printArchiveName(ConstString printableStorageName, bool showEntriesFlag)
{
  assert(printableStorageName != NULL);

  if (showEntriesFlag)
  {
    if (!globalOptions.groupFlag && !globalOptions.noHeaderFooterFlag)
    {
      printConsole(stdout,0,"List storage '%s':\n",String_cString(printableStorageName));
    }
  }
  else
  {
    if (!globalOptions.groupFlag)
    {
      printConsole(stdout,0,"Storage '%s': ",String_cString(printableStorageName));
    }
  }
}

/***********************************************************************\
* Name   : printMetaInfo
* Purpose: print archive meta data
* Input  : hostName               - host name
*          userName               - user name
*          jobUUID                - job UUID
*          entityUUID             - entity UUID
*          archiveType            - archive type
*          createdDateTime        - create date/time [s]
*          comment                - comment
*          allCryptSignatureState - all crypt signature state
*
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printMetaInfo(ConstString          hostName,
                         ConstString          userName,
                         ConstString          jobUUID,
                         ConstString          entityUUID,
                         ArchiveTypes         archiveType,
                         uint64               createdDateTime,
                         ConstString          comment,
                         CryptSignatureStates allCryptSignatureState
                        )
{
  assert(hostName != NULL);
  assert(userName != NULL);
  assert(jobUUID != NULL);
  assert(entityUUID != NULL);
  assert(comment != NULL);

  // print info
  char            dateTimeString[64];
  printConsole(stdout,0,"\n");
  printConsole(stdout,0,"Host name  : %s\n",String_cString(hostName));
  printConsole(stdout,0,"User name  : %s\n",String_cString(userName));
  printConsole(stdout,0,"Job UUID   : %s\n",!String_isEmpty(jobUUID) ? String_cString(jobUUID) : "-");
  printConsole(stdout,0,"Entity UUID: %s\n",!String_isEmpty(entityUUID) ? String_cString(entityUUID) : "-");
  printConsole(stdout,0,"Type       : %s\n",Archive_archiveTypeToString(archiveType));
  printConsole(stdout,0,"Created at : %s\n",Misc_formatDateTimeCString(dateTimeString,sizeof(dateTimeString),createdDateTime,TIME_TYPE_LOCAL,NULL));
  printConsole(stdout,0,"Signatures : ");
  switch (allCryptSignatureState)
  {
    case CRYPT_SIGNATURE_STATE_NONE   : printConsole(stdout,0,"none available\n"); break;
    case CRYPT_SIGNATURE_STATE_OK     : printConsole(stdout,0,"OK\n");             break;
    case CRYPT_SIGNATURE_STATE_INVALID: printConsole(stdout,0,"INVALID!\n");       break;
    case CRYPT_SIGNATURE_STATE_SKIPPED: printConsole(stdout,0,"skipped\n");        break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  printConsole(stdout,0,"Comment      :");
  StringTokenizer stringTokenizer;
  String_initTokenizer(&stringTokenizer,comment,STRING_BEGIN,"\n",STRING_QUOTES,FALSE);
  ConstString     token;
  if (String_getNextToken(&stringTokenizer,&token,NULL))
  {
    printConsole(stdout,0," %s",String_cString(token));
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      printConsole(stdout,0,"\n");
      printConsole(stdout,0,"               %s",String_cString(token));
    }
  }
  String_doneTokenizer(&stringTokenizer);
  printConsole(stdout,0,"\n");

  // free resources
}

/***********************************************************************\
* Name   : printArchiveContentListHeader
* Purpose: print archive list header lines
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printArchiveContentListHeader(uint prefixWidth)
{
  const TextMacro MACROS[] =
  {
    TEXT_MACRO_CSTRING("storageName","Storage",            NULL),
    TEXT_MACRO_CSTRING("type",       "Type",               NULL),
    TEXT_MACRO_CSTRING("size",       "Size",               NULL),
    TEXT_MACRO_CSTRING("dateTime",   "Date/Time",          NULL),
    TEXT_MACRO_CSTRING("user",       "User",               NULL),
    TEXT_MACRO_CSTRING("group",      "Group",              NULL),
    TEXT_MACRO_CSTRING("permission", "Permission",         NULL),
    TEXT_MACRO_CSTRING("part",       "Part [bytes..bytes]",NULL),
    TEXT_MACRO_CSTRING("compress",   "Compress",           NULL),
    TEXT_MACRO_CSTRING("ratio",      "Ratio",              NULL),
    TEXT_MACRO_CSTRING("crypt",      "Crypt",              NULL),
    TEXT_MACRO_CSTRING("name",       "Name",               NULL)
  };

  if (!globalOptions.noHeaderFooterFlag)
  {
    // init variables
    String line = String_new();

    // title line
    const char *prefixTemplate = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_TITLE_GROUP_PREFIX : NULL;
    const char *template;
    if (globalOptions.longFormatFlag)
    {
      template = DEFAULT_ARCHIVE_LIST_FORMAT_LONG_TITLE;
    }
    else
    {
      template = DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_TITLE;
    }
    printConsole(stdout,0,"\n");
    if (prefixTemplate != NULL)
    {
        printConsole(stdout,
                     prefixWidth+1,
                     "%s",
                     String_cString(Misc_expandMacros(line,
                                                      prefixTemplate,
                                                      EXPAND_MACRO_MODE_STRING,
                                                      MACROS,SIZE_OF_ARRAY(MACROS),
                                                      TRUE,
                                                      TRUE
                                                     )
                                   )
                    );
    }
    printConsole(stdout,
                 0,
                 "%s\n",
                 String_cString(Misc_expandMacros(line,
                                                  template,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  MACROS,SIZE_OF_ARRAY(MACROS),
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
    printSeparator('-');

    // free resources
    String_delete(line);
  }
}

/***********************************************************************\
* Name   : printArchiveContentListFooter
* Purpose: print archive list footer lines
* Input  : fileCount - number of files listed
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printArchiveContentListFooter(ulong fileCount)
{
  if (!globalOptions.noHeaderFooterFlag)
  {
    String line = String_new();

    printSeparator('-');
    printConsole(stdout,0,"%lu %s\n",fileCount,(fileCount == 1) ? "entry" : "entries");
    printConsole(stdout,0,"\n");

    String_delete(line);
  }
}

/***********************************************************************\
* Name   : printFileInfo
* Purpose: print file information
* Input  : prefixWidth            - prefix width (or 0)
*          storageName            - storage name or NULL if storage name
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

LOCAL void printFileInfo(uint               prefixWidth,
                         ConstString        storageName,
                         ConstString        fileName,
                         uint64             size,
                         uint64             timeModified,
                         uint32             userId,
                         uint32             groupId,
                         FilePermissions    permissions,
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
  assert(fileName != NULL);

  // format
  char dateTimeString[64];
  Misc_formatDateTimeCString(dateTimeString,sizeof(dateTimeString),timeModified,TIME_TYPE_LOCAL,NULL);

  char sizeString[32];
  char deltaSourceSizeString[32];
  if (globalOptions.humanFormatFlag)
  {
    getHumanSizeString(sizeString,sizeof(sizeString),size);
    getHumanSizeString(deltaSourceSizeString,sizeof(deltaSourceSizeString),deltaSourceSize);
  }
  else
  {
    stringFormat(sizeString,sizeof(sizeString),"%"PRIu64,size);
    stringFormat(deltaSourceSizeString,sizeof(deltaSourceSizeString),"%"PRIu64,deltaSourceSize);
  }

  char userName[12],groupName[12];
  if (globalOptions.numericUIDGIDFlag)
  {
    stringFormat(userName,sizeof(userName),"%d",userId);
    stringFormat(groupName,sizeof(groupName),"%d",groupId);
  }
  else
  {
    Misc_userIdToUserName(userName,sizeof(userName),userId);
    Misc_groupIdToGroupName(groupName,sizeof(groupName),groupId);
  }
  char permissionString[10];
  if (globalOptions.numericPermissionsFlag)
  {
    stringFormat(permissionString,sizeof(permissionString),"%4o",permissions & FILE_PERMISSION_ALL);
  }
  else
  {
    File_permissionToString(permissionString,sizeof(permissionString),permissions,FALSE);
  }

  const char *prefixTemplate = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_LONG_GROUP_PREFIX : NULL;
  const char *template;
  String     compressString = String_new();
  String     cryptString    = String_new();
  if (globalOptions.longFormatFlag)
  {
    template = DEFAULT_ARCHIVE_LIST_FORMAT_LONG_FILE;
    if      ((Compress_isCompressed(deltaCompressAlgorithm) && Compress_isCompressed(byteCompressAlgorithm)))
    {
      String_appendFormat(compressString,
                          "%s+%s",
                          Compress_algorithmToString(deltaCompressAlgorithm,NULL),
                          Compress_algorithmToString(byteCompressAlgorithm, NULL)
                         );
    }
    else if (Compress_isCompressed(deltaCompressAlgorithm))
    {
      String_appendFormat(compressString,
                          "%s",
                          Compress_algorithmToString(deltaCompressAlgorithm,NULL)
                         );
    }
    else if (Compress_isCompressed(byteCompressAlgorithm))
    {
      String_appendFormat(compressString,
                          "%s",
                          Compress_algorithmToString(byteCompressAlgorithm,NULL)
                         );
    }
    else
    {
      String_appendFormat(compressString,
                          "%s",
                          Compress_algorithmToString(deltaCompressAlgorithm,NULL)
                         );
    }

    String_appendFormat(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
  }
  else
  {
    template = DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_FILE;
  }

  double ratio;
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

  TextMacros (textMacros,15);
  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING   ("storageName",    storageName,                                                          NULL);
    TEXT_MACRO_X_CSTRING  ("type",           "FILE",                                                               NULL);
    TEXT_MACRO_X_CSTRING  ("size",           sizeString,                                                           NULL);
    TEXT_MACRO_X_CSTRING  ("dateTime",       dateTimeString,                                                       NULL);
    TEXT_MACRO_X_CSTRING  ("user",           userName,                                                             NULL);
    TEXT_MACRO_X_CSTRING  ("group",          groupName,                                                            NULL);
    TEXT_MACRO_X_CSTRING  ("permission",     permissionString,                                                     NULL);
    TEXT_MACRO_X_INT64    ("partFrom",       fragmentOffset,                                                       NULL);
    TEXT_MACRO_X_INT64    ("partTo",         (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1 : fragmentOffset,NULL);
    TEXT_MACRO_X_STRING   ("compress",       compressString,                                                       NULL);
    TEXT_MACRO_X_DOUBLE   ("ratio",          ratio,                                                                NULL);
    TEXT_MACRO_X_STRING   ("crypt",          cryptString,                                                          NULL);
    TEXT_MACRO_X_STRING   ("name",           fileName,                                                             NULL);
    TEXT_MACRO_X_STRING   ("deltaSourceName",deltaSourceName,                                                      NULL);
    TEXT_MACRO_X_CSTRING  ("deltaSourceSize",deltaSourceSizeString,                                                NULL);
  }

  // print
  String line = String_new();
  if (prefixTemplate != NULL)
  {
    printConsole(stdout,
                 prefixWidth+1,
                 "%s",
                 String_cString(Misc_expandMacros(line,
                                                  prefixTemplate,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  textMacros.data,
                                                  textMacros.count,
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
  }
  printConsole(stdout,
               0,
               "%s\n",
               String_cString(Misc_expandMacros(line,
                                                template,
                                                EXPAND_MACRO_MODE_STRING,
                                                textMacros.data,
                                                textMacros.count,
                                                TRUE,
                                                TRUE
                                               )
                             )
              );
  if (Compress_isCompressed(deltaCompressAlgorithm) && isPrintInfo(2))
  {
    printConsole(stdout,
                 0,
                 "%s\n",
                 String_cString(Misc_expandMacros(line,
                                                  DEFAULT_ARCHIVE_LIST_FORMAT_DELTA_SOURCE,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  textMacros.data,
                                                  textMacros.count,
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
  }
  String_delete(line);

  // free resources
  String_delete(cryptString);
  String_delete(compressString);
}

/***********************************************************************\
* Name   : printImageInfo
* Purpose: print image information
* Input  : prefixWidth            - prefix width (or 0)
*          storageName            - storage name or NULL if storage name
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
*          fragmentOffset         - fragment offset (0..n-1)
*          fragmentSize           - fragment length
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printImageInfo(uint               prefixWidth,
                          ConstString        storageName,
                          ConstString        imageName,
                          uint64             size,
                          uint64             archiveSize,
                          CompressAlgorithms deltaCompressAlgorithm,
                          CompressAlgorithms byteCompressAlgorithm,
                          CryptAlgorithms    cryptAlgorithm,
                          CryptTypes         cryptType,
                          ConstString        deltaSourceName,
                          uint64             deltaSourceSize,
                          FileSystemTypes    fileSystemType,
                          uint64             fragmentOffset,
                          uint64             fragmentSize
                         )
{
  assert(imageName != NULL);

  // format
  char sizeString[32];
  char deltaSourceSizeString[32];
  if (globalOptions.humanFormatFlag)
  {
    getHumanSizeString(sizeString,sizeof(sizeString),size);
    getHumanSizeString(deltaSourceSizeString,sizeof(deltaSourceSizeString),deltaSourceSize);
  }
  else
  {
    stringFormat(sizeString,sizeof(sizeString),"%"PRIu64,size);
    stringFormat(deltaSourceSizeString,sizeof(deltaSourceSizeString),"%"PRIu64,deltaSourceSize);
  }

  const char *prefixTemplate = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_LONG_GROUP_PREFIX : NULL;
  const char *template;
  char       type[32];
  if (fileSystemType != FILE_SYSTEM_TYPE_NONE)
  {
    stringSet(type,sizeof(type),FileSystem_typeToString(fileSystemType,"IMAGE"));
  }
  else
  {
    stringSet(type,sizeof(type),"IMAGE");
  }
  String     compressString = String_new();
  String     cryptString    = String_new();
  if (globalOptions.longFormatFlag)
  {
    template = DEFAULT_ARCHIVE_LIST_FORMAT_LONG_IMAGE;
    if      ((Compress_isCompressed(deltaCompressAlgorithm) && Compress_isCompressed(byteCompressAlgorithm)))
    {
      String_appendFormat(compressString,
                          "%s+%s",
                          Compress_algorithmToString(deltaCompressAlgorithm,NULL),
                          Compress_algorithmToString(byteCompressAlgorithm, NULL)
                         );
    }
    else if (Compress_isCompressed(deltaCompressAlgorithm))
    {
      String_appendFormat(compressString,
                          "%s",
                          Compress_algorithmToString(deltaCompressAlgorithm,NULL)
                         );
    }
    else if (Compress_isCompressed(byteCompressAlgorithm))
    {
      String_appendFormat(compressString,
                          "%s",
                          Compress_algorithmToString(byteCompressAlgorithm,NULL)
                         );
    }
    else
    {
      String_appendFormat(compressString,
                          "%s",
                          Compress_algorithmToString(deltaCompressAlgorithm,NULL)
                         );
    }

    String_appendFormat(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
  }
  else
  {
    template = DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_IMAGE;
  }

  double ratio;
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

  TextMacros (textMacros,11);
  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING ("storageName",    storageName,                                                                                    NULL);
    TEXT_MACRO_X_CSTRING("type",           type,                                                                                           NULL);
    TEXT_MACRO_X_CSTRING("size",           sizeString,                                                                                     NULL);
    TEXT_MACRO_X_INT64  ("partFrom",       fragmentOffset,                                                       NULL);
    TEXT_MACRO_X_INT64  ("partTo",         (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1 : fragmentOffset,NULL);
    TEXT_MACRO_X_STRING ("compress",       compressString,                                                                                 NULL);
    TEXT_MACRO_X_DOUBLE ("ratio",          ratio,                                                                                          NULL);
    TEXT_MACRO_X_STRING ("crypt",          cryptString,                                                                                    NULL);
    TEXT_MACRO_X_STRING ("name",           imageName,                                                                                      NULL);
    TEXT_MACRO_X_STRING ("deltaSourceName",deltaSourceName,                                                                                NULL);
    TEXT_MACRO_X_STRING ("deltaSourceSize",deltaSourceSizeString,                                                                          NULL);
  }

  // print
  String line = String_new();
  if (prefixTemplate != NULL)
  {
    printConsole(stdout,
                 prefixWidth+1,
                 "%s",
                 String_cString(Misc_expandMacros(line,
                                                  prefixTemplate,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  textMacros.data,
                                                  textMacros.count,
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
  }
  printConsole(stdout,
               0,
               "%s\n",
               String_cString(Misc_expandMacros(line,
                                                template,
                                                EXPAND_MACRO_MODE_STRING,
                                                textMacros.data,
                                                textMacros.count,
                                                TRUE,
                                                TRUE
                                               )
                             )
              );
  if (Compress_isCompressed(deltaCompressAlgorithm) && isPrintInfo(2))
  {
    printConsole(stdout,
                 0,
                 "%s\n",
                 String_cString(Misc_expandMacros(line,
                                                  DEFAULT_ARCHIVE_LIST_FORMAT_DELTA_SOURCE,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  textMacros.data,
                                                  textMacros.count,
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
  }
  String_delete(line);

  // free resources
  String_delete(cryptString);
  String_delete(compressString);
}

/***********************************************************************\
* Name   : printDirectoryInfo
* Purpose: print directory information
* Input  : prefixWidth     - prefix width (or 0)
*          storageName     - storage name or NULL if storage name should
*                            not be printed
*          directoryName   - directory name
*          timeModified    - file modified time
*          userId          - user id
*          groupId         - group id
*          permissions     - permissions
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printDirectoryInfo(uint            prefixWidth,
                              ConstString     storageName,
                              ConstString     directoryName,
                              uint64          timeModified,
                              uint32          userId,
                              uint32          groupId,
                              FilePermissions permissions,
                              CryptAlgorithms cryptAlgorithm,
                              CryptTypes      cryptType
                             )
{
  assert(directoryName != NULL);

  // format
  char dateTimeString[64];
  Misc_formatDateTimeCString(dateTimeString,sizeof(dateTimeString),timeModified,TIME_TYPE_LOCAL,NULL);

  char userName[12],groupName[12];
  if (globalOptions.numericUIDGIDFlag)
  {
    stringFormat(userName,sizeof(userName),"%d",userId);
    stringFormat(groupName,sizeof(groupName),"%d",groupId);
  }
  else
  {
    Misc_userIdToUserName(userName,sizeof(userName),userId);
    Misc_groupIdToGroupName(groupName,sizeof(groupName),groupId);
  }
  char permissionString[10];
  if (globalOptions.numericPermissionsFlag)
  {
    stringFormat(permissionString,sizeof(permissionString),"%4o",permissions & FILE_PERMISSION_ALL);
  }
  else
  {
    File_permissionToString(permissionString,sizeof(permissionString),permissions,FALSE);
  }

  const char *prefixTemplate = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_LONG_GROUP_PREFIX : NULL;
  const char *template;
  String     cryptString = String_new();
  if (globalOptions.longFormatFlag)
  {
    template = DEFAULT_ARCHIVE_LIST_FORMAT_LONG_DIR;
    String_appendFormat(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
  }
  else
  {
    template = DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_DIR;
  }

  TextMacros (textMacros,8);
  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING ("storageName",storageName,     NULL);
    TEXT_MACRO_X_CSTRING("type",       "DIR",           NULL);
    TEXT_MACRO_X_CSTRING("dateTime",   dateTimeString,  NULL);
    TEXT_MACRO_X_CSTRING("user",       userName,        NULL);
    TEXT_MACRO_X_CSTRING("group",      groupName,       NULL);
    TEXT_MACRO_X_CSTRING("permission", permissionString,NULL);
    TEXT_MACRO_X_STRING ("crypt",      cryptString,     NULL);
    TEXT_MACRO_X_STRING ("name",       directoryName,   NULL);
  }

  // print
  String line = String_new();
  if (prefixTemplate != NULL)
  {
    printConsole(stdout,
                 prefixWidth+1,
                 "%s",
                 String_cString(Misc_expandMacros(line,
                                                  prefixTemplate,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  textMacros.data,
                                                  textMacros.count,
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
  }
  printConsole(stdout,
               0,
               "%s\n",
               String_cString(Misc_expandMacros(String_clear(line),
                                                template,
                                                EXPAND_MACRO_MODE_STRING,
                                                textMacros.data,
                                                textMacros.count,
                                                TRUE,
                                                TRUE
                                               )
                             )
              );
  String_delete(line);

  // free resources
  String_delete(cryptString);
}

/***********************************************************************\
* Name   : printLinkInfo
* Purpose: print link information
* Input  : prefixWidth     - prefix width (or 0)
*          storageName     - storage name or NULL if storage name should
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

LOCAL void printLinkInfo(uint            prefixWidth,
                         ConstString     storageName,
                         ConstString     linkName,
                         ConstString     destinationName,
                         uint64          timeModified,
                         uint32          userId,
                         uint32          groupId,
                         FilePermissions permissions,
                         CryptAlgorithms cryptAlgorithm,
                         CryptTypes      cryptType
                        )
{
  assert(linkName != NULL);
  assert(destinationName != NULL);

  // format
  char       dateTimeString[64];
  Misc_formatDateTimeCString(dateTimeString,sizeof(dateTimeString),timeModified,TIME_TYPE_LOCAL,NULL);

  char userName[12],groupName[12];
  if (globalOptions.numericUIDGIDFlag)
  {
    stringFormat(userName,sizeof(userName),"%d",userId);
    stringFormat(groupName,sizeof(groupName),"%d",groupId);
  }
  else
  {
    Misc_userIdToUserName(userName,sizeof(userName),userId);
    Misc_groupIdToGroupName(groupName,sizeof(groupName),groupId);
  }
  char permissionString[10];
  if (globalOptions.numericPermissionsFlag)
  {
    stringFormat(permissionString,sizeof(permissionString),"%4o",permissions & FILE_PERMISSION_ALL);
  }
  else
  {
    File_permissionToString(permissionString,sizeof(permissionString),permissions,FALSE);
  }

  const char *prefixTemplate = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_LONG_GROUP_PREFIX : NULL;
  const char *template;
  String     cryptString = String_new();
  if (globalOptions.longFormatFlag)
  {
    template = DEFAULT_ARCHIVE_LIST_FORMAT_LONG_LINK;
    String_appendFormat(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
  }
  else
  {
    template = DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_LINK;
  }

  TextMacros (textMacros,9);
  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING ("storageName",    storageName,     NULL);
    TEXT_MACRO_X_CSTRING("type",           "LINK",          NULL);
    TEXT_MACRO_X_CSTRING("dateTime",       dateTimeString,  NULL);
    TEXT_MACRO_X_CSTRING("user",           userName,        NULL);
    TEXT_MACRO_X_CSTRING("group",          groupName,       NULL);
    TEXT_MACRO_X_CSTRING("permission",     permissionString,NULL);
    TEXT_MACRO_X_STRING ("crypt",          cryptString,     NULL);
    TEXT_MACRO_X_STRING ("name",           linkName,        NULL);
    TEXT_MACRO_X_STRING ("destinationName",destinationName, NULL);
  }

  // print
  String line = String_new();
  if (prefixTemplate != NULL)
  {
    printConsole(stdout,
                 prefixWidth+1,
                 "%s",
                 String_cString(Misc_expandMacros(line,
                                                  prefixTemplate,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  textMacros.data,
                                                  textMacros.count,
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
  }
  printConsole(stdout,
               0,
               "%s\n",
               String_cString(Misc_expandMacros(line,
                                                template,
                                                EXPAND_MACRO_MODE_STRING,
                                                textMacros.data,
                                                textMacros.count,
                                                TRUE,
                                                TRUE
                                               )
                             )
              );
  String_delete(line);

  // free resources
  String_delete(cryptString);
}

/***********************************************************************\
* Name   : printHardLinkInfo
* Purpose: print hard link information
* Input  : prefixWidth            - prefix width (or 0)
*          storageName            - storage name or NULL if storage name
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

LOCAL void printHardLinkInfo(uint               prefixWidth,
                             ConstString        storageName,
                             ConstString        fileName,
                             uint64             size,
                             uint64             timeModified,
                             uint32             userId,
                             uint32             groupId,
                             FilePermissions    permissions,
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
  assert(fileName != NULL);

  // format
  char dateTimeString[64];
  Misc_formatDateTimeCString(dateTimeString,sizeof(dateTimeString),timeModified,TIME_TYPE_LOCAL,NULL);

  char sizeString[32];
  char deltaSourceSizeString[32];
  if (globalOptions.humanFormatFlag)
  {
    getHumanSizeString(sizeString,sizeof(sizeString),size);
    getHumanSizeString(deltaSourceSizeString,sizeof(deltaSourceSizeString),deltaSourceSize);
  }
  else
  {
    stringFormat(sizeString,sizeof(sizeString),"%"PRIu64,size);
    stringFormat(deltaSourceSizeString,sizeof(deltaSourceSizeString),"%"PRIu64,deltaSourceSize);
  }

  char userName[12],groupName[12];
  char permissionString[10];
  if (globalOptions.numericUIDGIDFlag)
  {
    stringFormat(userName,sizeof(userName),"%d",userId);
    stringFormat(groupName,sizeof(groupName),"%d",groupId);
  }
  else
  {
    Misc_userIdToUserName(userName,sizeof(userName),userId);
    Misc_groupIdToGroupName(groupName,sizeof(groupName),groupId);
  }
  if (globalOptions.numericPermissionsFlag)
  {
    stringFormat(permissionString,sizeof(permissionString),"%4o",permissions & FILE_PERMISSION_ALL);
  }
  else
  {
    File_permissionToString(permissionString,sizeof(permissionString),permissions,FALSE);
  }

  const char *prefixTemplate = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_LONG_GROUP_PREFIX : NULL;
  const char *template;
  String     compressString = String_new();
  String     cryptString    = String_new();
  if (globalOptions.longFormatFlag)
  {
    template = DEFAULT_ARCHIVE_LIST_FORMAT_LONG_HARDLINK;
    if      ((Compress_isCompressed(deltaCompressAlgorithm) && Compress_isCompressed(byteCompressAlgorithm)))
    {
      String_appendFormat(compressString,
                          "%s+%s",
                          Compress_algorithmToString(deltaCompressAlgorithm,NULL),
                          Compress_algorithmToString(byteCompressAlgorithm, NULL)
                         );
    }
    else if (Compress_isCompressed(deltaCompressAlgorithm))
    {
      String_appendFormat(compressString,
                          "%s",
                          Compress_algorithmToString(deltaCompressAlgorithm,NULL)
                         );
    }
    else if (Compress_isCompressed(byteCompressAlgorithm))
    {
      String_appendFormat(compressString,
                          "%s",
                          Compress_algorithmToString(byteCompressAlgorithm,NULL)
                         );
    }
    else
    {
      String_appendFormat(compressString,
                          "%s",
                          Compress_algorithmToString(deltaCompressAlgorithm,NULL)
                         );
    }

    String_appendFormat(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
  }
  else
  {
    template = DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_HARDLINK;
  }

  double ratio;
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

  TextMacros (textMacros,15);
  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING ("storageName",    storageName,                                                          NULL);
    TEXT_MACRO_X_CSTRING("type",           "HARDLINK",                                                           NULL);
    TEXT_MACRO_X_CSTRING("size",           sizeString,                                                           NULL);
    TEXT_MACRO_X_CSTRING("dateTime",       dateTimeString,                                                       NULL);
    TEXT_MACRO_X_CSTRING("user",           userName,                                                             NULL);
    TEXT_MACRO_X_CSTRING("group",          groupName,                                                            NULL);
    TEXT_MACRO_X_CSTRING("permission",     permissionString,                                                     NULL);
    TEXT_MACRO_X_INT64  ("partFrom",       fragmentOffset,                                                       NULL);
    TEXT_MACRO_X_INT64  ("partTo",         (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1 : fragmentOffset,NULL);
    TEXT_MACRO_X_STRING ("compress",       compressString,                                                       NULL);
    TEXT_MACRO_X_DOUBLE ("ratio",          ratio,                                                                NULL);
    TEXT_MACRO_X_STRING ("crypt",          cryptString,                                                          NULL);
    TEXT_MACRO_X_STRING ("name",           fileName,                                                             NULL);
    TEXT_MACRO_X_STRING ("deltaSourceName",deltaSourceName,                                                      NULL);
    TEXT_MACRO_X_CSTRING("deltaSourceSize",deltaSourceSizeString,                                                NULL);
  }

  // print
  String line = String_new();
  if (prefixTemplate != NULL)
  {
    printConsole(stdout,
                 prefixWidth+1,
                 "%s",
                 String_cString(Misc_expandMacros(line,
                                                  prefixTemplate,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  textMacros.data,
                                                  textMacros.count,
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
  }
  printConsole(stdout,
               0,
               "%s\n",
               String_cString(Misc_expandMacros(String_clear(line),
                                                template,
                                                EXPAND_MACRO_MODE_STRING,
                                                textMacros.data,
                                                textMacros.count,
                                                TRUE,
                                                TRUE
                                               )
                             )
              );
  if (Compress_isCompressed(deltaCompressAlgorithm) && isPrintInfo(2))
  {
    printConsole(stdout,
                 0,
                 "%s\n",
                 String_cString(Misc_expandMacros(line,
                                                  DEFAULT_ARCHIVE_LIST_FORMAT_DELTA_SOURCE,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  textMacros.data,
                                                  textMacros.count,
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
  }
  String_delete(line);

  // free resources
  String_delete(cryptString);
  String_delete(compressString);
}

/***********************************************************************\
* Name   : printSpecialInfo
* Purpose: print special information
* Input  : prefixWidth     - prefix width (or 0)
*          storageName     - storage name or NULL if storage name should
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

LOCAL void printSpecialInfo(uint             prefixWidth,
                            ConstString      storageName,
                            ConstString      fileName,
                            uint32           userId,
                            uint32           groupId,
                            FilePermissions  permissions,
                            CryptAlgorithms  cryptAlgorithm,
                            CryptTypes       cryptType,
                            FileSpecialTypes fileSpecialType,
                            uint32           major,
                            uint32           minor
                           )
{
  assert(fileName != NULL);

  // format
  const char *prefixTemplate = (globalOptions.groupFlag) ? DEFAULT_ARCHIVE_LIST_FORMAT_LONG_GROUP_PREFIX : NULL;
  const char *template       = NULL;
  const char *type           = NULL;
  String     cryptString = String_new();
  switch (fileSpecialType)
  {
    case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
      if (globalOptions.longFormatFlag)
      {
        template = DEFAULT_ARCHIVE_LIST_FORMAT_LONG_SPECIAL_CHAR;
        String_appendFormat(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
      }
      else
      {
        template = DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_CHAR;
      }
      type = "CHAR";
      break;
    case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
      if (globalOptions.longFormatFlag)
      {
        template = DEFAULT_ARCHIVE_LIST_FORMAT_LONG_SPECIAL_BLOCK;
        String_appendFormat(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
      }
      else
      {
        template = DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_BLOCK;
      }
      type = "BLOCK";
      break;
    case FILE_SPECIAL_TYPE_FIFO:
      if (globalOptions.longFormatFlag)
      {
        template = DEFAULT_ARCHIVE_LIST_FORMAT_LONG_SPECIAL_FIFO;
        String_appendFormat(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
      }
      else
      {
        template = DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_FIFO;
      }
      type = "FIFO";
      break;
    case FILE_SPECIAL_TYPE_SOCKET:
      if (globalOptions.longFormatFlag)
      {
        template = DEFAULT_ARCHIVE_LIST_FORMAT_LONG_SPECIAL_SOCKET;
        String_appendFormat(cryptString,"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType == CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
      }
      else
      {
        template = DEFAULT_ARCHIVE_LIST_FORMAT_NORMAL_SPECIAL_SOCKET;
      }
      type = "SOCKET";
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }

  char userName[12],groupName[12];
  char permissionsString[10];
  if (globalOptions.numericUIDGIDFlag)
  {
    stringFormat(userName,sizeof(userName),"%d",userId);
    stringFormat(groupName,sizeof(groupName),"%d",groupId);
  }
  else
  {
    Misc_userIdToUserName(userName,sizeof(userName),userId);
    Misc_groupIdToGroupName(groupName,sizeof(groupName),groupId);
  }
  if (globalOptions.numericPermissionsFlag)
  {
    stringFormat(permissionsString,sizeof(permissionsString),"%4o",permissions & FILE_PERMISSION_ALL);
  }
  else
  {
    File_permissionToString(permissionsString,sizeof(permissionsString),permissions,FALSE);
  }

  TextMacros (textMacros,10);
  TEXT_MACROS_INIT(textMacros)
  {
    TEXT_MACRO_X_STRING ("storageName",storageName,      NULL);
    TEXT_MACRO_X_CSTRING("type",       type,             NULL);
    TEXT_MACRO_X_CSTRING("user",       userName,         NULL);
    TEXT_MACRO_X_CSTRING("group",      groupName,        NULL);
    TEXT_MACRO_X_CSTRING("permission", permissionsString,NULL);
    TEXT_MACRO_X_CSTRING("permissions",permissionsString,NULL);
    TEXT_MACRO_X_STRING ("crypt",      cryptString,      NULL);
    TEXT_MACRO_X_STRING ("name",       fileName,         NULL);
    TEXT_MACRO_X_INT    ("major",      major,            NULL);
    TEXT_MACRO_X_INT    ("minor",      minor,            NULL);
  }

  // print
  String line = String_new();
  if (prefixTemplate != NULL)
  {
    printConsole(stdout,
                 prefixWidth+1,
                 "%s",
                 String_cString(Misc_expandMacros(line,
                                                  prefixTemplate,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  textMacros.data,
                                                  textMacros.count,
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
  }
  printConsole(stdout,
               0,
               "%s\n",
               String_cString(Misc_expandMacros(String_clear(line),
                                                template,
                                                EXPAND_MACRO_MODE_STRING,
                                                textMacros.data,
                                                textMacros.count,
                                                TRUE,
                                                TRUE
                                               )
                             )
              );
  String_delete(line);

  // free resources
  String_delete(cryptString);
}

/***********************************************************************\
* Name   : addListFileInfo
* Purpose: add file info to archive content list
* Input  : storageName            - storage name
*          fileName               - file name
*          fileSize               - file size [bytes]
*          timeModified           - file modified time
*          userId                 - user id
*          groupId                - group id
*          permissions            - permissions
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
                           FilePermissions    permissions,
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
  // allocate node
  ArchiveContentNode *archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
  if (archiveContentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init node
  archiveContentNode->storageName                 = String_duplicate(storageName);
  archiveContentNode->type                        = ARCHIVE_ENTRY_TYPE_FILE;
  archiveContentNode->file.name                   = String_duplicate(fileName);
  archiveContentNode->file.size                   = fileSize;
  archiveContentNode->file.timeModified           = timeModified;
  archiveContentNode->file.userId                 = userId;
  archiveContentNode->file.groupId                = groupId;
  archiveContentNode->file.permissions            = permissions;
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
* Purpose: add image info to archive content list
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
                            FileSystemTypes    fileSystemType,
                            uint               blockSize,
                            uint64             blockOffset,
                            uint64             blockCount
                           )
{
  // allocate node
  ArchiveContentNode *archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
  if (archiveContentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init node
  archiveContentNode->storageName                  = String_duplicate(storageName);
  archiveContentNode->type                         = ARCHIVE_ENTRY_TYPE_IMAGE;
  archiveContentNode->image.name                   = String_duplicate(imageName);
  archiveContentNode->image.size                   = imageSize;
  archiveContentNode->image.archiveSize            = archiveSize;
  archiveContentNode->image.deltaCompressAlgorithm = deltaCompressAlgorithm;
  archiveContentNode->image.byteCompressAlgorithm  = byteCompressAlgorithm;
  archiveContentNode->image.cryptAlgorithm         = cryptAlgorithm;
  archiveContentNode->image.cryptType              = cryptType;
  archiveContentNode->image.deltaSourceName        = String_duplicate(deltaSourceName);
  archiveContentNode->image.deltaSourceSize        = deltaSourceSize;
  archiveContentNode->image.fileSystemType         = fileSystemType;
  archiveContentNode->image.blockSize              = blockSize;
  archiveContentNode->image.blockOffset            = blockOffset;
  archiveContentNode->image.blockCount             = blockCount;

  // append to list
  List_append(&archiveContentList,archiveContentNode);
}

/***********************************************************************\
* Name   : addListDirectoryInfo
* Purpose: add directory info to archive content list
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
                                FilePermissions permissions,
                                CryptAlgorithms cryptAlgorithm,
                                CryptTypes      cryptType
                               )
{
  // allocate node
  ArchiveContentNode *archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
  if (archiveContentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init node
  archiveContentNode->storageName              = String_duplicate(storageName);
  archiveContentNode->type                     = ARCHIVE_ENTRY_TYPE_DIRECTORY;
  archiveContentNode->directory.name           = String_duplicate(directoryName);
  archiveContentNode->directory.timeModified   = timeModified;
  archiveContentNode->directory.userId         = userId;
  archiveContentNode->directory.groupId        = groupId;
  archiveContentNode->directory.permissions    = permissions;
  archiveContentNode->directory.cryptAlgorithm = cryptAlgorithm;
  archiveContentNode->directory.cryptType      = cryptType;

  // append to list
  List_append(&archiveContentList,archiveContentNode);
}

/***********************************************************************\
* Name   : addListLinkInfo
* Purpose: add link info to archive content list
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
                           FilePermissions permissions,
                           CryptAlgorithms cryptAlgorithm,
                           CryptTypes      cryptType
                          )
{
  // allocate node
  ArchiveContentNode *archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
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
  archiveContentNode->link.permissions     = permissions;
  archiveContentNode->link.cryptAlgorithm  = cryptAlgorithm;
  archiveContentNode->link.cryptType       = cryptType;

  // append to list
  List_append(&archiveContentList,archiveContentNode);
}

/***********************************************************************\
* Name   : addListHardLinkInfo
* Purpose: add hard link info to archive content list
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
                               FilePermissions    permissions,
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
  // allocate node
  ArchiveContentNode *archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
  if (archiveContentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init node
  archiveContentNode->storageName                     = String_duplicate(storageName);
  archiveContentNode->type                            = ARCHIVE_ENTRY_TYPE_HARDLINK;
  archiveContentNode->hardLink.name                   = String_duplicate(fileName);
  archiveContentNode->hardLink.size                   = fileSize;
  archiveContentNode->hardLink.timeModified           = timeModified;
  archiveContentNode->hardLink.userId                 = userId;
  archiveContentNode->hardLink.groupId                = groupId;
  archiveContentNode->hardLink.permissions            = permissions;
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
* Purpose: add special info to archive content list
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
                              FilePermissions  permissions,
                              CryptAlgorithms  cryptAlgorithm,
                              CryptTypes       cryptType,
                              FileSpecialTypes fileSpecialType,
                              uint32           major,
                              uint32           minor
                             )
{
  // allocate node
  ArchiveContentNode *archiveContentNode = LIST_NEW_NODE(ArchiveContentNode);
  if (archiveContentNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }

  // init node
  archiveContentNode->storageName             = String_duplicate(storageName);
  archiveContentNode->type                    = ARCHIVE_ENTRY_TYPE_SPECIAL;
  archiveContentNode->special.name            = String_duplicate(fileName);
  archiveContentNode->special.userId          = userId;
  archiveContentNode->special.groupId         = groupId;
  archiveContentNode->special.permissions     = permissions;
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
* Purpose: free archive content node
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
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNREACHABLE();
      #endif /* NDEBUG */
      break;
    case ARCHIVE_ENTRY_TYPE_FILE:
      String_delete(archiveContentNode->file.name);
      String_delete(archiveContentNode->file.deltaSourceName);
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      String_delete(archiveContentNode->image.name);
      String_delete(archiveContentNode->image.deltaSourceName);
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      String_delete(archiveContentNode->directory.name);
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      String_delete(archiveContentNode->link.destinationName);
      String_delete(archiveContentNode->link.linkName);
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      String_delete(archiveContentNode->hardLink.name);
      String_delete(archiveContentNode->hardLink.deltaSourceName);
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      String_delete(archiveContentNode->special.name);
      break;
    case ARCHIVE_ENTRY_TYPE_META:
    case ARCHIVE_ENTRY_TYPE_SALT:
    case ARCHIVE_ENTRY_TYPE_KEY:
    case ARCHIVE_ENTRY_TYPE_SIGNATURE:
      break;
    case ARCHIVE_ENTRY_TYPE_UNKNOWN:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNREACHABLE();
      #endif /* NDEBUG */
      break;
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
  assert(archiveContentNode1 != NULL);
  assert(archiveContentNode2 != NULL);

  UNUSED_VARIABLE(userData);

  // get data
  ConstString name1         = NULL;
  uint64      modifiedTime1 = 0LL;
  uint64      offset1       = 0LL;
  ConstString name2         = NULL;
  uint64      modifiedTime2 = 0LL;
  uint64      offset2       = 0LL;
  switch (archiveContentNode1->type)
  {
    case ARCHIVE_ENTRY_TYPE_NONE:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNREACHABLE();
      #endif /* NDEBUG */
      break; /* not reached */
    case ARCHIVE_ENTRY_TYPE_FILE:
      name1         = archiveContentNode1->file.name;
      modifiedTime1 = archiveContentNode1->file.timeModified;
      offset1       = archiveContentNode1->file.fragmentOffset;
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      name1         = archiveContentNode1->image.name;
      modifiedTime1 = 0LL;
      offset1       = archiveContentNode1->image.blockOffset*(uint64)archiveContentNode1->image.blockSize;
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      name1         = archiveContentNode1->directory.name;
      modifiedTime1 = 0LL;
      offset1       = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      name1         = archiveContentNode1->link.linkName;
      modifiedTime1 = 0LL;
      offset1       = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      name1         = archiveContentNode1->hardLink.name;
      modifiedTime1 = archiveContentNode1->hardLink.timeModified;
      offset1       = archiveContentNode1->hardLink.fragmentOffset;
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      name1         = archiveContentNode1->special.name;
      modifiedTime1 = 0LL;
      offset1       = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_META:
    case ARCHIVE_ENTRY_TYPE_SALT:
    case ARCHIVE_ENTRY_TYPE_KEY:
    case ARCHIVE_ENTRY_TYPE_SIGNATURE:
      break;
    case ARCHIVE_ENTRY_TYPE_UNKNOWN:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNREACHABLE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  switch (archiveContentNode2->type)
  {
    case ARCHIVE_ENTRY_TYPE_NONE:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNREACHABLE();
      #endif /* NDEBUG */
      break; /* not reached */
    case ARCHIVE_ENTRY_TYPE_FILE:
      name2         = archiveContentNode2->file.name;
      modifiedTime2 = archiveContentNode2->file.timeModified;
      offset2       = archiveContentNode2->file.fragmentOffset;
      break;
    case ARCHIVE_ENTRY_TYPE_IMAGE:
      name2         = archiveContentNode2->image.name;
      modifiedTime2 = 0LL;
      offset2       = archiveContentNode2->image.blockOffset*(uint64)archiveContentNode2->image.blockSize;
      break;
    case ARCHIVE_ENTRY_TYPE_DIRECTORY:
      name2         = archiveContentNode2->directory.name;
      modifiedTime2 = 0LL;
      offset2       = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_LINK:
      name2         = archiveContentNode2->link.linkName;
      modifiedTime2 = 0LL;
      offset2       = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_HARDLINK:
      name2         = archiveContentNode2->hardLink.name;
      modifiedTime2 = archiveContentNode2->hardLink.timeModified;
      offset2       = archiveContentNode2->hardLink.fragmentOffset;
      break;
    case ARCHIVE_ENTRY_TYPE_SPECIAL:
      name2         = archiveContentNode2->special.name;
      modifiedTime2 = 0LL;
      offset2       = 0LL;
      break;
    case ARCHIVE_ENTRY_TYPE_META:
    case ARCHIVE_ENTRY_TYPE_SALT:
    case ARCHIVE_ENTRY_TYPE_KEY:
    case ARCHIVE_ENTRY_TYPE_SIGNATURE:
      break;
    case ARCHIVE_ENTRY_TYPE_UNKNOWN:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNREACHABLE();
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
      break;
  }
  if      (modifiedTime1 > modifiedTime2) return -1;
  else if (modifiedTime1 < modifiedTime2) return  1;
  if      (offset1 < offset2) return -1;
  else if (offset1 > offset2) return  1;

  return 0;
}

/***********************************************************************\
* Name   : printArchiveContentList
* Purpose: sort, group and print archive content list
* Input  : prefixWidth - prefix width
* Output : -
* Return : number of entries
* Notes  : -
\***********************************************************************/

LOCAL uint printArchiveContentList(uint prefixWidth)
{
  uint count = 0;

  // sort list
  List_sort(&archiveContentList,
            CALLBACK_((ListNodeCompareFunction)compareArchiveContentNode,NULL)
           );

  // output list
  ConstString              name                    = NULL;
  uint64                   partTo                  = 0LL;
  uint64                   fragmentSize            = 0LL;
  const ArchiveContentNode *nextArchiveContentNode = archiveContentList.head;
  while (nextArchiveContentNode != NULL)
  {
    const ArchiveContentNode *archiveContentNode = nextArchiveContentNode;
    ArchiveEntryTypes        archiveEntryType    = nextArchiveContentNode->type;
    switch (nextArchiveContentNode->type)
    {
      case ARCHIVE_ENTRY_TYPE_NONE:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNREACHABLE();
        #endif /* NDEBUG */
        break; /* not reached */
      case ARCHIVE_ENTRY_TYPE_FILE:
        name         = nextArchiveContentNode->file.name;
        partTo       = nextArchiveContentNode->file.fragmentOffset+nextArchiveContentNode->file.fragmentSize;
        fragmentSize = nextArchiveContentNode->file.fragmentSize;
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        name         = nextArchiveContentNode->image.name;
        partTo       = (nextArchiveContentNode->image.blockOffset+nextArchiveContentNode->image.blockCount)*(uint64)nextArchiveContentNode->image.blockSize;
        fragmentSize = nextArchiveContentNode->image.blockCount*(uint64)nextArchiveContentNode->image.blockSize;
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        name         = nextArchiveContentNode->directory.name;
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        name         = nextArchiveContentNode->link.linkName;
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        name         = nextArchiveContentNode->hardLink.name;
        partTo       = nextArchiveContentNode->hardLink.fragmentOffset+nextArchiveContentNode->hardLink.fragmentSize;
        fragmentSize = nextArchiveContentNode->hardLink.fragmentSize;
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        name         = nextArchiveContentNode->special.name;
        break;
      case ARCHIVE_ENTRY_TYPE_META:
      case ARCHIVE_ENTRY_TYPE_SALT:
      case ARCHIVE_ENTRY_TYPE_KEY:
      case ARCHIVE_ENTRY_TYPE_SIGNATURE:
        break;
      case ARCHIVE_ENTRY_TYPE_UNKNOWN:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNREACHABLE();
        #endif /* NDEBUG */
        break;
    }

    // next entry
    nextArchiveContentNode = nextArchiveContentNode->next;
    if (!globalOptions.allFlag)
    {
      bool newFlag = FALSE;
      while ((nextArchiveContentNode != NULL) && !newFlag)
      {
        switch (nextArchiveContentNode->type)
        {
          case ARCHIVE_ENTRY_TYPE_NONE:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNREACHABLE();
            #endif /* NDEBUG */
            break; /* not reached */
          case ARCHIVE_ENTRY_TYPE_FILE:
            if (   (archiveEntryType != ARCHIVE_ENTRY_TYPE_FILE)
                || !String_equals(name,nextArchiveContentNode->file.name)
               )
            {
              newFlag = TRUE;
            }
            else
            {
              partTo       = nextArchiveContentNode->file.fragmentOffset+nextArchiveContentNode->file.fragmentSize;
              fragmentSize += nextArchiveContentNode->file.fragmentSize;
            }
            break;
          case ARCHIVE_ENTRY_TYPE_IMAGE:
            if (   globalOptions.allFlag
                || (archiveEntryType != ARCHIVE_ENTRY_TYPE_IMAGE)
                || !String_equals(name,nextArchiveContentNode->image.name)
                || partTo < (nextArchiveContentNode->image.blockOffset*(uint64)nextArchiveContentNode->image.blockSize)
               )
            {
              newFlag = TRUE;
            }
            else
            {
              partTo       = (nextArchiveContentNode->image.blockOffset+nextArchiveContentNode->image.blockCount)*(uint64)nextArchiveContentNode->image.blockSize;
              fragmentSize += nextArchiveContentNode->image.blockCount*(uint64)nextArchiveContentNode->image.blockSize;
            }
            break;
          case ARCHIVE_ENTRY_TYPE_DIRECTORY:
            if (   globalOptions.allFlag
                || (archiveEntryType != ARCHIVE_ENTRY_TYPE_DIRECTORY)
                || !String_equals(name,nextArchiveContentNode->directory.name)
               )
            {
              newFlag = TRUE;
            }
            else
            {
            }
            break;
          case ARCHIVE_ENTRY_TYPE_LINK:
            if (   globalOptions.allFlag
                || (archiveEntryType != ARCHIVE_ENTRY_TYPE_LINK)
                || !String_equals(name,nextArchiveContentNode->link.linkName)
               )
            {
              newFlag = TRUE;
            }
            else
            {
            }
            break;
          case ARCHIVE_ENTRY_TYPE_HARDLINK:
            if (   globalOptions.allFlag
                || (archiveEntryType != ARCHIVE_ENTRY_TYPE_HARDLINK)
                || !String_equals(name,nextArchiveContentNode->hardLink.name)
               )
            {
              newFlag = TRUE;
            }
            else
            {
              partTo       = nextArchiveContentNode->hardLink.fragmentOffset+nextArchiveContentNode->hardLink.fragmentSize;
              fragmentSize += nextArchiveContentNode->hardLink.fragmentSize;
            }
            break;
          case ARCHIVE_ENTRY_TYPE_SPECIAL:
            if (   globalOptions.allFlag
                || (archiveEntryType != ARCHIVE_ENTRY_TYPE_SPECIAL)
                || !String_equals(name,nextArchiveContentNode->special.name)
               )
            {
              newFlag = TRUE;
            }
            else
            {
            }
            break;
          case ARCHIVE_ENTRY_TYPE_META:
          case ARCHIVE_ENTRY_TYPE_SALT:
          case ARCHIVE_ENTRY_TYPE_KEY:
          case ARCHIVE_ENTRY_TYPE_SIGNATURE:
            break;
          case ARCHIVE_ENTRY_TYPE_UNKNOWN:
            #ifndef NDEBUG
              HALT_INTERNAL_ERROR_UNREACHABLE();
            #endif /* NDEBUG */
            break; /* not reached */
        }

        // next entry
        nextArchiveContentNode = nextArchiveContentNode->next;
      }
    }

    // output
    switch (archiveContentNode->type)
    {
      case ARCHIVE_ENTRY_TYPE_NONE:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNREACHABLE();
        #endif /* NDEBUG */
        break; /* not reached */
      case ARCHIVE_ENTRY_TYPE_FILE:
        printFileInfo(prefixWidth,
                      archiveContentNode->storageName,
                      archiveContentNode->file.name,
                      archiveContentNode->file.size,
                      archiveContentNode->file.timeModified,
                      archiveContentNode->file.userId,
                      archiveContentNode->file.groupId,
                      archiveContentNode->file.permissions,
                      archiveContentNode->file.archiveSize,
                      archiveContentNode->file.deltaCompressAlgorithm,
                      archiveContentNode->file.byteCompressAlgorithm,
                      archiveContentNode->file.cryptAlgorithm,
                      archiveContentNode->file.cryptType,
                      archiveContentNode->file.deltaSourceName,
                      archiveContentNode->file.deltaSourceSize,
                      archiveContentNode->file.fragmentOffset,
                      fragmentSize
                     );
        break;
      case ARCHIVE_ENTRY_TYPE_IMAGE:
        printImageInfo(prefixWidth,
                       archiveContentNode->storageName,
                       archiveContentNode->image.name,
                       archiveContentNode->image.size,
                       archiveContentNode->image.archiveSize,
                       archiveContentNode->image.deltaCompressAlgorithm,
                       archiveContentNode->image.byteCompressAlgorithm,
                       archiveContentNode->image.cryptAlgorithm,
                       archiveContentNode->image.cryptType,
                       archiveContentNode->image.deltaSourceName,
                       archiveContentNode->image.deltaSourceSize,
                       archiveContentNode->image.fileSystemType,
                       archiveContentNode->image.blockOffset*(uint64)archiveContentNode->image.blockSize,
                       fragmentSize
                      );
        break;
      case ARCHIVE_ENTRY_TYPE_DIRECTORY:
        printDirectoryInfo(prefixWidth,
                           archiveContentNode->storageName,
                           archiveContentNode->directory.name,
                           archiveContentNode->directory.timeModified,
                           archiveContentNode->directory.userId,
                           archiveContentNode->directory.groupId,
                           archiveContentNode->directory.permissions,
                           archiveContentNode->directory.cryptAlgorithm,
                           archiveContentNode->directory.cryptType
                          );
        break;
      case ARCHIVE_ENTRY_TYPE_LINK:
        printLinkInfo(prefixWidth,
                      archiveContentNode->storageName,
                      archiveContentNode->link.linkName,
                      archiveContentNode->link.destinationName,
                      archiveContentNode->link.timeModified,
                      archiveContentNode->link.userId,
                      archiveContentNode->link.groupId,
                      archiveContentNode->link.permissions,
                      archiveContentNode->link.cryptAlgorithm,
                      archiveContentNode->link.cryptType
                     );
        break;
      case ARCHIVE_ENTRY_TYPE_HARDLINK:
        printHardLinkInfo(prefixWidth,
                          archiveContentNode->storageName,
                          archiveContentNode->hardLink.name,
                          archiveContentNode->hardLink.size,
                          archiveContentNode->hardLink.timeModified,
                          archiveContentNode->hardLink.userId,
                          archiveContentNode->hardLink.groupId,
                          archiveContentNode->hardLink.permissions,
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
        break;
      case ARCHIVE_ENTRY_TYPE_SPECIAL:
        printSpecialInfo(prefixWidth,
                         archiveContentNode->storageName,
                         archiveContentNode->special.name,
                         archiveContentNode->special.userId,
                         archiveContentNode->special.groupId,
                         archiveContentNode->special.permissions,
                         archiveContentNode->special.cryptAlgorithm,
                         archiveContentNode->special.cryptType,
                         archiveContentNode->special.fileSpecialType,
                         archiveContentNode->special.major,
                         archiveContentNode->special.minor
                        );
        break;
      case ARCHIVE_ENTRY_TYPE_META:
      case ARCHIVE_ENTRY_TYPE_SALT:
      case ARCHIVE_ENTRY_TYPE_KEY:
      case ARCHIVE_ENTRY_TYPE_SIGNATURE:
        break;
      case ARCHIVE_ENTRY_TYPE_UNKNOWN:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNREACHABLE();
        #endif /* NDEBUG */
        break; /* not reached */
    }
    count++;
  }

  return count;
}

/***********************************************************************\
* Name   : listArchiveContent
* Purpose: list archive content
* Input  : storageSpecifier         - storage specifier
*          archiveName              - archive name (can be NULL)
*          includeEntryList         - include entry list
*          excludePatternList       - exclude pattern list
*          showEntriesFlag          - TRUE to show entries
*          jobOptions               - job options
*          getNamePasswordFunction  - get password call back
*          getNamePasswordUserData  - user data for get password
*          logHandle                - log handle (can be NULL)
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL Errors listArchiveContent(StorageSpecifier        *storageSpecifier,
                                ConstString             archiveName,
                                const EntryList         *includeEntryList,
                                const PatternList       *excludePatternList,
                                bool                    showEntriesFlag,
                                JobOptions              *jobOptions,
                                GetNamePasswordFunction getNamePasswordFunction,
                                void                    *getNamePasswordUserData,
                                LogHandle               *logHandle
                               )
{
  assert(storageSpecifier != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

// NYI ???
#ifndef WERROR
bool remoteBarFlag=FALSE;
#endif

  Errors error = ERROR_NONE;

  // get printable storage name
  String printableStorageName = Storage_getPrintableName(String_new(),storageSpecifier,archiveName);

  CryptSignatureStates allCryptSignatureState = CRYPT_SIGNATURE_STATE_NONE;
  bool                 printedHeaderFlag      = FALSE;
  bool                 printedMetaInfoFlag    = FALSE;
  ulong                fileCount              = 0L;
  switch (storageSpecifier->type)
  {
    case STORAGE_TYPE_FILESYSTEM:
    case STORAGE_TYPE_FTP:
    case STORAGE_TYPE_SCP:
    case STORAGE_TYPE_SFTP:
    case STORAGE_TYPE_WEBDAV:
    case STORAGE_TYPE_WEBDAVS:
    case STORAGE_TYPE_SMB:
    case STORAGE_TYPE_CD:
    case STORAGE_TYPE_DVD:
    case STORAGE_TYPE_BD:
      {
        // init storage
        StorageInfo storageInfo;
        error = Storage_init(&storageInfo,
NULL, // masterSocketHandle
                             storageSpecifier,
                             jobOptions,
                             &globalOptions.maxBandWidthList,
                             SERVER_CONNECTION_PRIORITY_HIGH,
                             CALLBACK_(NULL,NULL),  // storageUpdateProgress
                             CALLBACK_(NULL,NULL),  // getPassword
                             CALLBACK_(NULL,NULL),  // requestVolume
                             CALLBACK_(NULL,NULL),  // isPause
                             CALLBACK_(NULL,NULL),  // isAborted
                             NULL  // logHandle
                            );
        if (error != ERROR_NONE)
        {
          break;
        }

        // open archive
        ArchiveHandle archiveHandle;
        error = Archive_open(&archiveHandle,
                             &storageInfo,
                             archiveName,
                             NULL,  // deltaSourceList
                             ARCHIVE_FLAG_SKIP_UNKNOWN_CHUNKS|ARCHIVE_FLAG_PRINT_UNKNOWN_CHUNKS,
                             CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                             logHandle
                            );
        if (error != ERROR_NONE)
        {
          (void)Storage_done(&storageInfo);
          break;
        }

        // check signatures
        if (!jobOptions->skipVerifySignaturesFlag)
        {
          error = Archive_verifySignatures(&archiveHandle,
                                           &allCryptSignatureState
                                          );
          if (error != ERROR_NONE)
          {
            if (!jobOptions->forceVerifySignaturesFlag && (Error_getCode(error) == ERROR_CODE_NO_PUBLIC_SIGNATURE_KEY))
            {
              allCryptSignatureState = CRYPT_SIGNATURE_STATE_SKIPPED;
            }
            else
            {
              // signature error
              (void)Archive_close(&archiveHandle,FALSE);
              (void)Storage_done(&storageInfo);
              break;
            }
          }
          if (!Crypt_isValidSignatureState(allCryptSignatureState))
          {
            if (jobOptions->forceVerifySignaturesFlag)
            {
              // signature error
              printError(_("invalid signature in '%s'!"),
                         String_cString(printableStorageName)
                        );
              (void)Archive_close(&archiveHandle,FALSE);
              (void)Storage_done(&storageInfo);
              String_delete(printableStorageName);
              return ERROR_INVALID_SIGNATURE;
            }
            else
            {
              // print signature warning
              printWarning(_("invalid signature in '%s'!"),
                           String_cString(printableStorageName)
                          );
            }
          }
        }
        else
        {
          allCryptSignatureState = CRYPT_SIGNATURE_STATE_SKIPPED;
        }

        // list contents
        error = ERROR_NONE;
        printArchiveName(printableStorageName,showEntriesFlag);
        while (   !Archive_eof(&archiveHandle)
               && (error == ERROR_NONE)
              )
        {
          // get next archive entry type
          ArchiveEntryTypes archiveEntryType;
          error = Archive_getNextArchiveEntry(&archiveHandle,
                                              &archiveEntryType,
                                              NULL,  // archiveCryptInfo
                                              NULL,  // offset
                                              NULL  // size
                                             );
          if (error != ERROR_NONE)
          {
            break;
          }

          switch (archiveEntryType)
          {
            case ARCHIVE_ENTRY_TYPE_NONE:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNREACHABLE();
              #endif /* NDEBUG */
              break; /* not reached */
            case ARCHIVE_ENTRY_TYPE_FILE:
              {

                if (showEntriesFlag)
                {
                  // read archive file
                  ArchiveEntryInfo   archiveEntryInfo;
                  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
                  CryptTypes         cryptType;
                  CryptAlgorithms    cryptAlgorithm;
                  String             fileName = String_new();
                  FileInfo           fileInfo;
                  String             deltaSourceName = String_new();
                  uint64             deltaSourceSize;
                  uint64             fragmentOffset,fragmentSize;
                  error = Archive_readFileEntry(&archiveEntryInfo,
                                                &archiveHandle,
                                                &deltaCompressAlgorithm,
                                                &byteCompressAlgorithm,
                                                &cryptType,
                                                &cryptAlgorithm,
                                                NULL,  // cryptSalt
                                                NULL,  // cryptKey
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
//TODO: remove
#if 0
                    printError(_("cannot read 'file' entry from storage '%s' (error: %s)!"),
                               String_cString(storageSpecifier,fileName),
                               Error_getText(error)
                              );
#endif
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
                      addListFileInfo(Storage_getName(NULL,storageSpecifier,NULL),
                                      fileName,
                                      fileInfo.size,
                                      fileInfo.timeModified,
                                      fileInfo.permissions,
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
                      // output info
                      if (!printedHeaderFlag && !globalOptions.groupFlag)
                      {
                        printArchiveContentListHeader(0);
                        printedHeaderFlag = TRUE;
                      }
                      printFileInfo(0,  // prefixWidth
                                    NULL,
                                    fileName,
                                    fileInfo.size,
                                    fileInfo.timeModified,
                                    fileInfo.userId,
                                    fileInfo.groupId,
                                    fileInfo.permissions,
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

                  // close archive entry, free resources
                  error = Archive_closeEntry(&archiveEntryInfo);
                  if (error != ERROR_NONE)
                  {
                    printWarning(_("close 'file' entry fail (error: %s)"),Error_getText(error));
                  }

                  // free resources
                  String_delete(deltaSourceName);
                  String_delete(fileName);
                }
                else
                {
                  error = Archive_skipNextEntry(&archiveHandle);
                }
              }
              break;
            case ARCHIVE_ENTRY_TYPE_IMAGE:
              {
                if (showEntriesFlag)
                {
                  // read archive image
                  ArchiveEntryInfo   archiveEntryInfo;
                  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
                  CryptTypes         cryptType;
                  CryptAlgorithms    cryptAlgorithm;
                  String             deviceName = String_new();
                  DeviceInfo         deviceInfo;
                  FileSystemTypes    fileSystemType;
                  String             deltaSourceName = String_new();
                  uint64             deltaSourceSize;
                  uint64             blockOffset,blockCount;
                  error = Archive_readImageEntry(&archiveEntryInfo,
                                                 &archiveHandle,
                                                 &deltaCompressAlgorithm,
                                                 &byteCompressAlgorithm,
                                                 &cryptType,
                                                 &cryptAlgorithm,
                                                 NULL,  // cryptSalt
                                                 NULL,  // cryptKey
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
//TODO: remove
#if 0
                    printError(_("cannot read 'image' entry from storage '%s' (error: %s)!"),
                               String_cString(storageSpecifier,fileName),
                               Error_getText(error)
                              );
#endif
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
                      addListImageInfo(Storage_getName(NULL,storageSpecifier,NULL),
                                       deviceName,
                                       deviceInfo.size,
                                       archiveEntryInfo.image.chunkImageData.info.size,
                                       deltaCompressAlgorithm,
                                       byteCompressAlgorithm,
                                       cryptAlgorithm,
                                       cryptType,
                                       deltaSourceName,
                                       deltaSourceSize,
                                       fileSystemType,
                                       deviceInfo.blockSize,
                                       blockOffset,
                                       blockCount
                                      );
                    }
                    else
                    {
                      // output info
                      if (!printedHeaderFlag && !globalOptions.groupFlag)
                      {
                        printArchiveContentListHeader(0);
                        printedHeaderFlag = TRUE;
                      }
                      printImageInfo(0,  // prefixWidth
                                     NULL,
                                     deviceName,
                                     deviceInfo.size,
                                     archiveEntryInfo.image.chunkImageData.info.size,
                                     deltaCompressAlgorithm,
                                     byteCompressAlgorithm,
                                     cryptAlgorithm,
                                     cryptType,
                                     deltaSourceName,
                                     deltaSourceSize,
                                     fileSystemType,
                                     blockOffset*(uint64)deviceInfo.blockSize,
                                     blockCount*(uint64)deviceInfo.blockSize
                                    );
                    }
                    fileCount++;
                  }

                  // close archive entry, free resources
                  error = Archive_closeEntry(&archiveEntryInfo);
                  if (error != ERROR_NONE)
                  {
                    printWarning(_("close 'image' entry fail (error: %s)"),Error_getText(error));
                  }

                  // free resources
                  String_delete(deltaSourceName);
                  String_delete(deviceName);
                }
                else
                {
                  error = Archive_skipNextEntry(&archiveHandle);
                }
              }
              break;
            case ARCHIVE_ENTRY_TYPE_DIRECTORY:
              {
                if (showEntriesFlag)
                {
                  // read archive directory entry
                  ArchiveEntryInfo archiveEntryInfo;
                  CryptTypes       cryptType;
                  CryptAlgorithms  cryptAlgorithm;
                  String           directoryName = String_new();
                  FileInfo         fileInfo;
                  error = Archive_readDirectoryEntry(&archiveEntryInfo,
                                                     &archiveHandle,
                                                     &cryptType,
                                                     &cryptAlgorithm,
                                                     NULL,  // cryptSalt
                                                     NULL,  // cryptKey
                                                     directoryName,
                                                     &fileInfo,
                                                     NULL   // fileExtendedAttributeList
                                                    );
                  if (error != ERROR_NONE)
                  {
//TODO: remove
#if 0
                    printError(_("cannot read 'directory' entry from storage '%s' (error: %s)!"),
                               String_cString(storageSpecifier,fileName),
                               Error_getText(error)
                              );
#endif
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
                      addListDirectoryInfo(Storage_getName(NULL,storageSpecifier,NULL),
                                           directoryName,
                                           fileInfo.timeModified,
                                           fileInfo.userId,
                                           fileInfo.groupId,
                                           fileInfo.permissions,
                                           cryptAlgorithm,
                                           cryptType
                                          );
                    }
                    else
                    {
                      // output info
                      if (!printedHeaderFlag && !globalOptions.groupFlag)
                      {
                        printArchiveContentListHeader(0);
                        printedHeaderFlag = TRUE;
                      }
                      printDirectoryInfo(0,  // prefixWidth
                                         NULL,
                                         directoryName,
                                         fileInfo.timeModified,
                                         fileInfo.userId,
                                         fileInfo.groupId,
                                         fileInfo.permissions,
                                         cryptAlgorithm,
                                         cryptType
                                        );
                    }
                    fileCount++;
                  }

                  // close archive entry, free resources
                  error = Archive_closeEntry(&archiveEntryInfo);
                  if (error != ERROR_NONE)
                  {
                    printWarning(_("close 'directory' entry fail (error: %s)"),Error_getText(error));
                  }

                  // free resources
                  String_delete(directoryName);
                }
                else
                {
                  error = Archive_skipNextEntry(&archiveHandle);
                }
              }
              break;
            case ARCHIVE_ENTRY_TYPE_LINK:
              {
                if (showEntriesFlag)
                {
                  // read archive link
                  ArchiveEntryInfo archiveEntryInfo;
                  CryptTypes       cryptType;
                  CryptAlgorithms  cryptAlgorithm;
                  String           linkName = String_new();
                  String           fileName = String_new();
                  FileInfo         fileInfo;
                  error = Archive_readLinkEntry(&archiveEntryInfo,
                                                &archiveHandle,
                                                &cryptType,
                                                &cryptAlgorithm,
                                                NULL,  // cryptSalt
                                                NULL,  // cryptKey
                                                linkName,
                                                fileName,
                                                &fileInfo,
                                                NULL   // fileExtendedAttributeList
                                               );
                  if (error != ERROR_NONE)
                  {
//TODO: remove
#if 0
                    printError(_("cannot read 'link' entry from storage '%s' (error: %s)!"),
                               String_cString(storageSpecifier,fileName),
                               Error_getText(error)
                              );
#endif
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
                      addListLinkInfo(Storage_getName(NULL,storageSpecifier,NULL),
                                      linkName,
                                      fileName,
                                      fileInfo.timeModified,
                                      fileInfo.userId,
                                      fileInfo.groupId,
                                      fileInfo.permissions,
                                      cryptAlgorithm,
                                      cryptType
                                     );
                    }
                    else
                    {
                      // output info
                      if (!printedHeaderFlag && !globalOptions.groupFlag)
                      {
                        printArchiveContentListHeader(0);
                        printedHeaderFlag = TRUE;
                      }
                      printLinkInfo(0,  // prefixWidth
                                    NULL,
                                    linkName,
                                    fileName,
                                    fileInfo.timeModified,
                                    fileInfo.userId,
                                    fileInfo.groupId,
                                    fileInfo.permissions,
                                    cryptAlgorithm,
                                    cryptType
                                   );
                    }
                    fileCount++;
                  }

                  // close archive entry, free resources
                  error = Archive_closeEntry(&archiveEntryInfo);
                  if (error != ERROR_NONE)
                  {
                    printWarning(_("close 'link' entry fail (error: %s)"),Error_getText(error));
                  }

                  // free resources
                  String_delete(fileName);
                  String_delete(linkName);
                }
                else
                {
                  error = Archive_skipNextEntry(&archiveHandle);
                }
              }
              break;
            case ARCHIVE_ENTRY_TYPE_HARDLINK:
              {
                if (showEntriesFlag)
                {
                  // read archive hard link
                  ArchiveEntryInfo   archiveEntryInfo;
                  CompressAlgorithms deltaCompressAlgorithm,byteCompressAlgorithm;
                  CryptTypes         cryptType;
                  CryptAlgorithms    cryptAlgorithm;
                  StringList         fileNameList;
                  StringList_init(&fileNameList);
                  FileInfo           fileInfo;
                  String             deltaSourceName = String_new();
                  uint64             deltaSourceSize;
                  uint64             fragmentOffset,fragmentSize;
                  error = Archive_readHardLinkEntry(&archiveEntryInfo,
                                                    &archiveHandle,
                                                    &deltaCompressAlgorithm,
                                                    &byteCompressAlgorithm,
                                                    &cryptType,
                                                    &cryptAlgorithm,
                                                    NULL,  // cryptSalt
                                                    NULL,  // cryptKey
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
//TODO: remove
#if 0
                    printError(_("cannot read 'hard link' entry from storage '%s' (error: %s)!"),
                               String_cString(storageSpecifier,fileName),
                               Error_getText(error)
                              );
#endif
                    String_delete(deltaSourceName);
                    StringList_done(&fileNameList);
                    break;
                  }

                  ConstString fileName;
                  STRINGLIST_ITERATE(&fileNameList,fileName)
                  {
                    if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                        && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                       )
                    {
                      if (globalOptions.groupFlag)
                      {
                        // add file info to list
                        addListHardLinkInfo(Storage_getName(NULL,storageSpecifier,NULL),
                                            fileName,
                                            fileInfo.size,
                                            fileInfo.timeModified,
                                            fileInfo.userId,
                                            fileInfo.groupId,
                                            fileInfo.permissions,
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
                        // output info
                        if (!printedHeaderFlag && !globalOptions.groupFlag)
                        {
                          printArchiveContentListHeader(0);
                          printedHeaderFlag = TRUE;
                        }
                        printHardLinkInfo(0,  // prefixWidth
                                          NULL,
                                          fileName,
                                          fileInfo.size,
                                          fileInfo.timeModified,
                                          fileInfo.userId,
                                          fileInfo.groupId,
                                          fileInfo.permissions,
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

                  // close archive entry, free resources
                  error = Archive_closeEntry(&archiveEntryInfo);
                  if (error != ERROR_NONE)
                  {
                    printWarning(_("close 'hard link' entry fail (error: %s)"),Error_getText(error));
                  }

                  // free resources
                  String_delete(deltaSourceName);
                  StringList_done(&fileNameList);
                }
                else
                {
                  error = Archive_skipNextEntry(&archiveHandle);
                }
              }
              break;
            case ARCHIVE_ENTRY_TYPE_SPECIAL:
              {
                if (showEntriesFlag)
                {
                  // open archive lin
                  ArchiveEntryInfo archiveEntryInfo;
                  CryptTypes       cryptType;
                  CryptAlgorithms  cryptAlgorithm;
                  String           fileName = String_new();
                  FileInfo         fileInfo;
                  error = Archive_readSpecialEntry(&archiveEntryInfo,
                                                   &archiveHandle,
                                                   &cryptType,
                                                   &cryptAlgorithm,
                                                   NULL,  // cryptSalt
                                                   NULL,  // cryptKey
                                                   fileName,
                                                   &fileInfo,
                                                   NULL   // fileExtendedAttributeList
                                                  );
                  if (error != ERROR_NONE)
                  {
//TODO: remove
#if 0
                    printError(_("cannot read 'special' entry from storage '%s' (error: %s)!"),
                               String_cString(storageSpecifier,fileName),
                               Error_getText(error)
                              );
#endif
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
                      addListSpecialInfo(Storage_getName(NULL,storageSpecifier,NULL),
                                         fileName,
                                         fileInfo.userId,
                                         fileInfo.groupId,
                                         fileInfo.permissions,
                                         cryptAlgorithm,
                                         cryptType,
                                         fileInfo.specialType,
                                         fileInfo.major,
                                         fileInfo.minor
                                        );
                    }
                    else
                    {
                      // output info
                      if (!printedHeaderFlag && !globalOptions.groupFlag)
                      {
                        printArchiveContentListHeader(0);
                        printedHeaderFlag = TRUE;
                      }
                      printSpecialInfo(0,  // prefixWidth
                                       NULL,
                                       fileName,
                                       fileInfo.userId,
                                       fileInfo.groupId,
                                       fileInfo.permissions,
                                       cryptAlgorithm,
                                       cryptType,
                                       fileInfo.specialType,
                                       fileInfo.major,
                                       fileInfo.minor
                                      );
                    }
                    fileCount++;
                  }

                  // close archive entry, free resources
                  error = Archive_closeEntry(&archiveEntryInfo);
                  if (error != ERROR_NONE)
                  {
                    printWarning(_("close 'special' entry fail (error: %s)"),Error_getText(error));
                  }

                  // free resources
                  String_delete(fileName);
                }
                else
                {
                  error = Archive_skipNextEntry(&archiveHandle);
                }
              }
              break;
            case ARCHIVE_ENTRY_TYPE_META:
              {
                if (!showEntriesFlag && !globalOptions.groupFlag)
                {
                  // read archive file
                  ArchiveEntryInfo archiveEntryInfo;
                  String           hostName = String_new();
                  String           userName = String_new();
                  StaticString     (jobUUID,MISC_UUID_STRING_LENGTH);
                  StaticString     (scheduleUUID,MISC_UUID_STRING_LENGTH);
                  ArchiveTypes     archiveType;
                  uint64           createdDateTime;
                  String           comment  = String_new();
                  error = Archive_readMetaEntry(&archiveEntryInfo,
                                                &archiveHandle,
                                                NULL,  // cryptType
                                                NULL,  // cryptAlgorithm
                                                NULL,  // cryptSalt
                                                NULL,  // cryptKey
                                                hostName,
                                                userName,
                                                jobUUID,
                                                scheduleUUID,
                                                &archiveType,
                                                &createdDateTime,
                                                comment
                                               );
                  if (error != ERROR_NONE)
                  {
//TODO: remove
#if 0
                    printError(_("cannot read 'meta' entry from storage '%s' (error: %s)!"),
                               Storage_getPrintableNameCString(storageSpecifier,fileName),
                               Error_getText(error)
                              );
#endif
                    String_delete(comment);
                    String_delete(userName);
                    String_delete(hostName);
                    break;
                  }

                  // output meta data
                  if (!printedMetaInfoFlag)
                  {
                    printMetaInfo(hostName,
                                  userName,
                                  jobUUID,
                                  scheduleUUID,
                                  archiveType,
                                  createdDateTime,
                                  comment,
                                  allCryptSignatureState
                                 );
                    printf("\n");
                    printedMetaInfoFlag = TRUE;
                  }

                  // close archive entry, free resources
                  error = Archive_closeEntry(&archiveEntryInfo);
                  if (error != ERROR_NONE)
                  {
                    printWarning(_("close 'special' entry fail (error: %s)"),Error_getText(error));
                  }

                  // free resources
                  String_delete(comment);
                  String_delete(userName);
                  String_delete(hostName);
                }
                else
                {
                  error = Archive_skipNextEntry(&archiveHandle);
                }
              }
              break;
            case ARCHIVE_ENTRY_TYPE_SALT:
            case ARCHIVE_ENTRY_TYPE_KEY:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNREACHABLE();
              #else
                error = Archive_skipNextEntry(&archiveHandle);
              #endif /* NDEBUG */
              break;
            case ARCHIVE_ENTRY_TYPE_SIGNATURE:
              error = Archive_skipNextEntry(&archiveHandle);
              break;
            case ARCHIVE_ENTRY_TYPE_UNKNOWN:
              #ifndef NDEBUG
                HALT_INTERNAL_ERROR_UNREACHABLE();
              #endif /* NDEBUG */
              break; /* not reached */
          }
        }

        // close archive
        (void)Archive_close(&archiveHandle,FALSE);

        // done storage
        (void)Storage_done(&storageInfo);
      }
      break;
    case STORAGE_TYPE_DEVICE:
      error = ERROR_FUNCTION_NOT_SUPPORTED;
      break;
    default:
      #ifndef NDEBUG
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
      #endif /* NDEBUG */
      break; /* not reached */
  }
  if (printedHeaderFlag)
  {
    printArchiveContentListFooter(fileCount);
  }
  if (error != ERROR_NONE)
  {
    String_delete(printableStorageName);
    return error;
  }

  // output signature state
  if (   (showEntriesFlag || globalOptions.groupFlag)
      && !globalOptions.noHeaderFooterFlag
     )
  {
    switch (allCryptSignatureState)
    {
      case CRYPT_SIGNATURE_STATE_NONE   : printConsole(stdout,0,"no signatures available\n"); break;
      case CRYPT_SIGNATURE_STATE_OK     : printConsole(stdout,0,"signatures OK\n");           break;
      case CRYPT_SIGNATURE_STATE_INVALID: printConsole(stdout,0,"signatures INVALID!\n");     break;
      case CRYPT_SIGNATURE_STATE_SKIPPED: printConsole(stdout,0,"signatures skipped\n");      break;
      default:
        #ifndef NDEBUG
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        #endif /* NDEBUG */
        break; /* not reached */
    }
  }

  // free resources
  String_delete(printableStorageName);

  return (   jobOptions->forceVerifySignaturesFlag
          && !Crypt_isValidSignatureState(allCryptSignatureState)
         )
           ? ERROR_INVALID_SIGNATURE
           : ERROR_NONE;
}

/***********************************************************************\
* Name   : printDirectoryListHeader
* Purpose: print directory list header
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printDirectoryListHeader(ConstString printableStorageName)
{
  const TextMacro MACROS[] =
  {
    TEXT_MACRO_CSTRING("type",      "Type",      NULL),
    TEXT_MACRO_CSTRING("size",      "Size",      NULL),
    TEXT_MACRO_CSTRING("dateTime",  "Date/Time" ,NULL),
    TEXT_MACRO_CSTRING("user",      "User",      NULL),
    TEXT_MACRO_CSTRING("group",     "Group",     NULL),
    TEXT_MACRO_CSTRING("permission","Permission",NULL),
    TEXT_MACRO_CSTRING("name",      "Name",      NULL)
  };

  if (!globalOptions.noHeaderFooterFlag)
  {
    // init variables
    String line = String_new();

    // header
    if (printableStorageName != NULL)
    {
      printConsole(stdout,0,"List directory '%s':\n",String_cString(printableStorageName));
      printConsole(stdout,0,"\n");
    }

    // print title line
    printConsole(stdout,
                 0,
                 "%s\n",
                 String_cString(Misc_expandMacros(line,
                                                  DEFAULT_DIRECTORY_LIST_FORMAT_TITLE,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  MACROS,SIZE_OF_ARRAY(MACROS),
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
    printSeparator('-');

    // free resources
    String_delete(line);
  }
}

/***********************************************************************\
* Name   : printDirectoryListFooter
* Purpose: print directory list footer
* Input  : fileCount     - number of files listed
*          totalFileSize - total file sizes [bytes]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printDirectoryListFooter(ulong fileCount, uint64 totalFileSize)
{
  if (!globalOptions.noHeaderFooterFlag)
  {
    String line = String_new();

    char sizeString[32];
    getHumanSizeString(sizeString,sizeof(sizeString),totalFileSize);

    printSeparator('-');
    printConsole(stdout,0,"%lu %s, %s (%"PRIu64" bytes)\n",
                 fileCount,
                 (fileCount == 1) ? "entry" : "entries",
                 sizeString,
                 totalFileSize
                );
    printConsole(stdout,0,"\n");

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
  typedef struct DirectoryEntryNode
  {
    LIST_NODE_HEADER(struct DirectoryEntryNode);

    String   fileName;
    FileInfo fileInfo;
  } DirectoryEntryNode;

  typedef struct
  {
    LIST_HEADER(DirectoryEntryNode);
  } DirectoryEntryList;

  /***********************************************************************\
  * Name   : freeDirectoryEntryNode
  * Purpose: free directory entry node
  * Input  : entryNode - entry node
  *          userData  - not used
  * Output : -
  * Return : -
  * Notes  : -
  \***********************************************************************/

  auto void freeDirectoryEntryNode(DirectoryEntryNode *directoryEntryNode, void *userData);
  void freeDirectoryEntryNode(DirectoryEntryNode *directoryEntryNode, void *userData)
  {
    assert(directoryEntryNode != NULL);

    UNUSED_VARIABLE(userData);

    String_delete(directoryEntryNode->fileName);
  }

  assert(storageDirectoryListHandle != NULL);
  assert(storageSpecifier != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);

  Errors error = ERROR_NONE;

  // get printable storage name
  String printableStorageName = Storage_getPrintableName(String_new(),storageSpecifier,NULL);

  // read directory content
  DirectoryEntryList directoryEntryList;
  List_init(&directoryEntryList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeDirectoryEntryNode,NULL));
  String             fileName = String_new();
  while (!Storage_endOfDirectoryList(storageDirectoryListHandle))
  {
    // read next directory entry
    FileInfo fileInfo;
    error = Storage_readDirectoryList(storageDirectoryListHandle,fileName,&fileInfo);
    if (error != ERROR_NONE)
    {
      String_delete(fileName);
      List_done(&directoryEntryList);
      String_delete(printableStorageName);
      return error;
    }

    if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
        && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
       )
    {
      DirectoryEntryNode *directoryEntryNode = LIST_NEW_NODE(DirectoryEntryNode);
      if (directoryEntryNode == NULL)
      {
        HALT_INSUFFICIENT_MEMORY();
      }

      directoryEntryNode->fileName = String_duplicate(fileName);
      directoryEntryNode->fileInfo = fileInfo;

      List_append(&directoryEntryList,directoryEntryNode);
    }
  }

  // sort
  List_sort(&directoryEntryList,(ListNodeCompareFunction)CALLBACK_INLINE(int,(const DirectoryEntryNode *directoryEntryNode1, const DirectoryEntryNode *directoryEntryNode2, void *userData),
  {
    assert(directoryEntryNode1 != NULL);
    assert(directoryEntryNode2 != NULL);

    UNUSED_VARIABLE(userData);

    return String_compare(directoryEntryNode1->fileName,directoryEntryNode2->fileName,NULL,NULL);
  },NULL));

  // output directory content
  printDirectoryListHeader(printableStorageName);
  DirectoryEntryNode *directoryEntryNode;
  TextMacros         (textMacros,8);
  uint64             totalFileSize = 0LL;
  String             line = String_new();
  LIST_ITERATE(&directoryEntryList,directoryEntryNode)
  {
    TEXT_MACROS_INIT(textMacros)
    {
      // format
      char dateTimeString[64];
      char userName[12],groupName[12];
      char permissionsString[10];
      char sizeString[32];
      switch (directoryEntryNode->fileInfo.type)
      {
        case FILE_TYPE_NONE:
          break;
        case FILE_TYPE_FILE:
        case FILE_TYPE_HARDLINK:
          TEXT_MACRO_X_CSTRING("type","FILE",NULL);
          if (globalOptions.humanFormatFlag)
          {
            getHumanSizeString(sizeString,sizeof(sizeString),directoryEntryNode->fileInfo.size);
          }
          else
          {
            stringFormat(sizeString,sizeof(sizeString),"%"PRIu64,directoryEntryNode->fileInfo.size);
          }
          TEXT_MACRO_X_CSTRING("size",sizeString,NULL);
          totalFileSize += directoryEntryNode->fileInfo.size;
          break;
        case FILE_TYPE_DIRECTORY:
          TEXT_MACRO_X_CSTRING("type","DIR",NULL);
          TEXT_MACRO_X_CSTRING("size","",   NULL);
          break;
        case FILE_TYPE_LINK:
          TEXT_MACRO_X_CSTRING("type","LINK",NULL);
          TEXT_MACRO_X_CSTRING("size","",    NULL);
          break;
        case FILE_TYPE_SPECIAL:
          TEXT_MACRO_X_CSTRING("type","SPECIAL",NULL);
          TEXT_MACRO_X_CSTRING("size","",       NULL);
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break;
      }
      Misc_formatDateTimeCString(dateTimeString,sizeof(dateTimeString),directoryEntryNode->fileInfo.timeModified,TIME_TYPE_LOCAL,NULL);
      if (globalOptions.numericUIDGIDFlag)
      {
        stringFormat(userName,sizeof(userName),"%d",directoryEntryNode->fileInfo.userId);
        stringFormat(groupName,sizeof(groupName),"%d",directoryEntryNode->fileInfo.groupId);
      }
      else
      {
        Misc_userIdToUserName(userName,sizeof(userName),directoryEntryNode->fileInfo.userId);
        Misc_groupIdToGroupName(groupName,sizeof(groupName),directoryEntryNode->fileInfo.groupId);
      }
      if (globalOptions.numericPermissionsFlag)
      {
        stringFormat(permissionsString,sizeof(permissionsString),"%4o",directoryEntryNode->fileInfo.permissions & FILE_PERMISSION_ALL);
      }
      else
      {
        File_permissionToString(permissionsString,sizeof(permissionsString),directoryEntryNode->fileInfo.permissions,FALSE);
      }
      TEXT_MACRO_X_CSTRING("dateTime",   dateTimeString,              NULL);
      TEXT_MACRO_X_CSTRING("user",       userName,                    NULL);
      TEXT_MACRO_X_CSTRING("group",      groupName,                   NULL);
      TEXT_MACRO_X_STRING ("permission" ,permissionsString,           NULL);
      TEXT_MACRO_X_STRING ("permissions",permissionsString,           NULL);
      TEXT_MACRO_X_STRING ("name",       directoryEntryNode->fileName,NULL);
    }

    // print
    printConsole(stdout,
                 0,
                 "%s\n",
                 String_cString(Misc_expandMacros(String_clear(line),
                                                  DEFAULT_DIRECTORY_LIST_FORMAT,
                                                  EXPAND_MACRO_MODE_STRING,
                                                  textMacros.data,
                                                  textMacros.count,
                                                  TRUE,
                                                  TRUE
                                                 )
                               )
                );
  }
  printDirectoryListFooter(List_count(&directoryEntryList),totalFileSize);

  // free resources
  String_delete(line);
  String_delete(fileName);
  List_done(&directoryEntryList);
  String_delete(printableStorageName);

  return ERROR_NONE;
}

/*---------------------------------------------------------------------*/

Errors Command_list(StringList              *storageNameList,
                    const EntryList         *includeEntryList,
                    const PatternList       *excludePatternList,
                    bool                    showEntriesFlag,
                    JobOptions              *jobOptions,
                    GetNamePasswordFunction getNamePasswordFunction,
                    void                    *getNamePasswordUserData,
                    LogHandle               *logHandle
                   )
{
  assert(storageNameList != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

  Errors error;

  // init variables
  List_init(&archiveContentList,CALLBACK_(NULL,NULL),CALLBACK_((ListNodeFreeFunction)freeArchiveContentNode,NULL));
  StorageSpecifier storageSpecifier;
  Storage_initSpecifier(&storageSpecifier);

  // list archive content
  Errors      failError    = ERROR_NONE;
  bool        anyFileFound = FALSE;
  ConstString storageName;
  STRINGLIST_ITERATE(storageNameList,storageName)
  {
    bool someFileFound = FALSE;

    // parse storage name
    error = Storage_parseName(&storageSpecifier,storageName);
    if (error != ERROR_NONE)
    {
      printError(_("invalid storage '%s' (error: %s)!"),
                 String_cString(storageName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }

    error = ERROR_UNKNOWN;

    // try list archives content
    if (error != ERROR_NONE)
    {
      if (   !String_isEmpty(storageSpecifier.archiveName)
          && String_isEmpty(storageSpecifier.archivePatternString)
         )
      {
        error = listArchiveContent(&storageSpecifier,
                                   NULL,  // fileName
                                   includeEntryList,
                                   excludePatternList,
                                   showEntriesFlag,
                                   jobOptions,
                                   CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                                   logHandle
                                  );
        if (error == ERROR_NONE)
        {
          someFileFound = TRUE;
        }
      }
    }
    if (error != ERROR_NONE)
    {
      StorageDirectoryListHandle storageDirectoryListHandle;
      if (Storage_openDirectoryList(&storageDirectoryListHandle,
                                    &storageSpecifier,
                                    NULL,  // fileName
                                    jobOptions,
                                    SERVER_CONNECTION_PRIORITY_HIGH
                                   ) == ERROR_NONE
         )
      {
        if (String_isEmpty(storageSpecifier.archivePatternString))
        {
          // list directory
          error = listDirectoryContent(&storageDirectoryListHandle,
                                       &storageSpecifier,
                                       includeEntryList,
                                       excludePatternList
                                      );
          if (error == ERROR_NONE)
          {
            someFileFound = TRUE;
          }
        }
        else
        {
          // list content of matching archives
          String fileName = String_new();
          while (!Storage_endOfDirectoryList(&storageDirectoryListHandle))
          {
            // read next directory entry
            FileInfo fileInfo;
            error = Storage_readDirectoryList(&storageDirectoryListHandle,fileName,&fileInfo);
            if (error != ERROR_NONE)
            {
              continue;
            }

            // match pattern
            if (!String_isEmpty(storageSpecifier.archivePatternString))
            {
              if (!Pattern_match(&storageSpecifier.archivePattern,fileName,STRING_BEGIN,PATTERN_MATCH_MODE_EXACT,NULL,NULL))
              {
                continue;
              }
            }

            // list archive content
            if (   (fileInfo.type == FILE_TYPE_FILE)
                || (fileInfo.type == FILE_TYPE_LINK)
                || (fileInfo.type == FILE_TYPE_HARDLINK)
               )
            {
              error = listArchiveContent(&storageSpecifier,
                                         fileName,
                                         includeEntryList,
                                         excludePatternList,
                                         showEntriesFlag,
                                         jobOptions,
                                         CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                                         logHandle
                                        );
            }

            someFileFound = TRUE;
          }
          String_delete(fileName);
        }

        Storage_closeDirectoryList(&storageDirectoryListHandle);
      }
    }

    // try list directory content
    if (!someFileFound)
    {
      // open directory list
      StorageDirectoryListHandle storageDirectoryListHandle;
      if (Storage_openDirectoryList(&storageDirectoryListHandle,
                                    &storageSpecifier,
                                    NULL,  // fileName
                                    jobOptions,
                                    SERVER_CONNECTION_PRIORITY_HIGH
                                   ) == ERROR_NONE
         )
      {
        if (String_isEmpty(storageSpecifier.archivePatternString))
        {
          // list directory
          error = listDirectoryContent(&storageDirectoryListHandle,
                                       &storageSpecifier,
                                       includeEntryList,
                                       excludePatternList
                                      );
          if (error == ERROR_NONE)
          {
            someFileFound = TRUE;
          }
        }
        else
        {
          // list content of matching archives
          String fileName = String_new();
          while (!Storage_endOfDirectoryList(&storageDirectoryListHandle) && (error == ERROR_NONE))
          {
            // read next directory entry
            error = Storage_readDirectoryList(&storageDirectoryListHandle,fileName,NULL);
            if (error != ERROR_NONE)
            {
              continue;
            }

            // match pattern
            if (!String_isEmpty(storageSpecifier.archivePatternString))
            {
              if (!Pattern_match(&storageSpecifier.archivePattern,fileName,STRING_BEGIN,PATTERN_MATCH_MODE_EXACT,NULL,NULL))
              {
                continue;
              }
            }

            // list archive content
            error = listArchiveContent(&storageSpecifier,
                                       fileName,
                                       includeEntryList,
                                       excludePatternList,
                                       showEntriesFlag,
                                       jobOptions,
                                       CALLBACK_(getNamePasswordFunction,getNamePasswordUserData),
                                       logHandle
                                      );
            if (error == ERROR_NONE)
            {
              someFileFound = TRUE;
            }
          }
          String_delete(fileName);
        }
        Storage_closeDirectoryList(&storageDirectoryListHandle);
      }
    }

    if (error != ERROR_NONE)
    {
      printError(_("cannot read storage '%s' (error: %s)!"),
                 String_cString(storageName),
                 Error_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
    }

    anyFileFound |= someFileFound;
  }
  if ((failError == ERROR_NONE) && !StringList_isEmpty(storageNameList) && !anyFileFound)
  {
    printError(_("no matching storage files found!"));
    failError = ERROR_FILE_NOT_FOUND_;
  }

  // output grouped list
  if (globalOptions.groupFlag)
  {
    // get prefix width (max. storage name length)
    uint                     prefixWidth = 0;
    const ArchiveContentNode *archiveContentNode;
    LIST_ITERATE(&archiveContentList,archiveContentNode)
    {
      prefixWidth = MAX(String_length(archiveContentNode->storageName),prefixWidth);
    }

    // print grouped list
    printArchiveContentListHeader(prefixWidth);
    uint count = printArchiveContentList(prefixWidth);
    printArchiveContentListFooter(count);
  }

  // free resources
  Storage_doneSpecifier(&storageSpecifier);
  List_done(&archiveContentList);

  return failError;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
