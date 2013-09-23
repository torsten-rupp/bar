/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive list function
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

#define DEFAULT_FORMAT_TITLE_NORMAL_LONG "%type:-8s %size:-10s %date:-25s %part:-22s %compress:-15s %ratio:-7s %crypt:-10s %name:s"
#define DEFAULT_FORMAT_TITLE_GROUP_LONG  "%archiveName:-20s %type:-8s %size:-10s %date:-25s %part:-22s %compress:-15s %ratio:-7s %crypt:-10s %name:s"
#define DEFAULT_FORMAT_TITLE_NORMAL      "%type:-8s %size:-10s %date:-25s %name:s"
#define DEFAULT_FORMAT_TITLE_GROUP       "%archiveName:-20s %type:-8s %size:-10s %date:-25s %name:s"

#define DEFAULT_FORMAT_NORMAL_LONG       "%type:-8s %size:-10s %date:-25s %part:-22s %compress:-15s %ratio:-7s %crypt:-10s %name:s"
#define DEFAULT_FORMAT_GROUP_LONG        "%archiveName:-20s %type:-8s %size:-10s %date:-25s %part:-22s %compress:-15s %ratio:-7s %crypt:-10s %name:s"
#define DEFAULT_FORMAT_NORMAL            "%type:-8s %size:-10s %date:-25s %name:s"
#define DEFAULT_FORMAT_GROUP             "%archiveName:-20s %type:-8s %size:-10s %date:-25s %name:s"

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
      String             fileName;
      uint64             size;
      uint64             timeModified;
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
      CryptAlgorithms  cryptAlgorithm;
      CryptTypes       cryptType;
      FileSpecialTypes fileSpecialType;
      ulong            major;
      ulong            minor;
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
* Name   : printHeader
* Purpose: print list header
* Input  : storageName - storage file name or NULL if archive should not
*                        be printed
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printHeader(const String storageName)
{
// ??? use macro templates for all output?
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
  };

  String     string;
  const char *template;

  string = String_new();

  if (!globalOptions.noHeaderFooterFlag)
  {
    // header
    if (storageName != NULL)
    {
      printInfo(0,"List storage '%s':\n",String_cString(storageName));
      printInfo(0,"\n");
    }

    // title line
    if (globalOptions.longFormatFlag)
    {
      template = (storageName != NULL) ? DEFAULT_FORMAT_TITLE_NORMAL_LONG : DEFAULT_FORMAT_TITLE_GROUP_LONG;
    }
    else
    {
      template = (storageName != NULL) ? DEFAULT_FORMAT_TITLE_NORMAL : DEFAULT_FORMAT_TITLE_GROUP;
    }
    Misc_expandMacros(String_clear(string),template,MACROS,SIZE_OF_ARRAY(MACROS));
    printInfo(0,"%s\n",String_cString(string));
    if (globalOptions.longFormatFlag)
    {
      printInfo(0,"------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    }
    else
    {
      printInfo(0,"--------------------------------------------------------------------------------------------------------------\n");
    }
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
    if (globalOptions.longFormatFlag)
    {
      printInfo(0,"------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    }
    else
    {
      printInfo(0,"--------------------------------------------------------------------------------------------------------------\n");
    }
    printInfo(0,"%lu %s\n",fileCount,(fileCount == 1) ? "entry" : "entries");
    printInfo(0,"\n");
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

LOCAL void printFileInfo(const String       storageName,
                         const String       fileName,
                         uint64             size,
                         uint64             timeModified,
                         uint64             archiveSize,
                         CompressAlgorithms deltaCompressAlgorithm,
                         CompressAlgorithms byteCompressAlgorithm,
                         CryptAlgorithms    cryptAlgorithm,
                         CryptTypes         cryptType,
                         const String       deltaSourceName,
                         uint64             deltaSourceSize,
                         uint64             fragmentOffset,
                         uint64             fragmentSize
                        )
{
  String dateTime;
  double ratio;
  char   buffer[16];
  String compressString;
  String cryptString;

  assert(fileName != NULL);

  dateTime = Misc_formatDateTime(String_new(),timeModified,NULL);

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

  if (storageName != NULL)
  {
    printf("%-20s ",String_cString(storageName));
  }
  printf("FILE     ");
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
    if      ((Compress_isCompressed(deltaCompressAlgorithm) && Compress_isCompressed(byteCompressAlgorithm)))
    {
      compressString = String_format(String_new(),
                                     "%s+%s",
                                     Compress_getAlgorithmName(deltaCompressAlgorithm),
                                     Compress_getAlgorithmName(byteCompressAlgorithm)
                                    );
    }
    else if (Compress_isCompressed(deltaCompressAlgorithm))
    {
      compressString = String_format(String_new(),
                                     "%s",
                                     Compress_getAlgorithmName(deltaCompressAlgorithm)
                                    );
    }
    else if (Compress_isCompressed(byteCompressAlgorithm))
    {
      compressString = String_format(String_new(),
                                     "%s",
                                     Compress_getAlgorithmName(byteCompressAlgorithm)
                                    );
    }
    else
    {
      compressString = String_format(String_new(),
                                     "%s",
                                     Compress_getAlgorithmName(deltaCompressAlgorithm)
                                    );
    }
    cryptString = String_format(String_new(),"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType==CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
    printf(" %-25s %10llu..%10llu %-15s %6.1f%% %-10s %s\n",
           String_cString(dateTime),
           fragmentOffset,
           (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1 : fragmentOffset,
           String_cString(compressString),
           ratio,
           String_cString(cryptString),
           String_cString(fileName)
          );

    String_delete(cryptString);
    String_delete(compressString);

    if (Compress_isCompressed(deltaCompressAlgorithm) && (globalOptions.verboseLevel > 1))
    {
      printf("                                                                     source: %s, %llu bytes\n",String_cString(deltaSourceName),deltaSourceSize);
    }
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

LOCAL void printImageInfo(const String       storageName,
                          const String       imageName,
                          uint64             size,
                          uint64             archiveSize,
                          CompressAlgorithms deltaCompressAlgorithm,
                          CompressAlgorithms byteCompressAlgorithm,
                          CryptAlgorithms    cryptAlgorithm,
                          CryptTypes         cryptType,
                          const String       deltaSourceName,
                          uint64             deltaSourceSize,
                          uint               blockSize,
                          uint64             blockOffset,
                          uint64             blockCount
                         )
{
  double ratio;
  char   buffer[16];
  String compressString;
  String cryptString;

  assert(imageName != NULL);

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

  if (storageName != NULL)
  {
    printf("%-20s ",String_cString(storageName));
  }
  printf("IMAGE    ");
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
    if      ((Compress_isCompressed(deltaCompressAlgorithm) && Compress_isCompressed(byteCompressAlgorithm)))
    {
      compressString = String_format(String_new(),
                                     "%s+%s",
                                     Compress_getAlgorithmName(deltaCompressAlgorithm),
                                     Compress_getAlgorithmName(byteCompressAlgorithm)
                                    );
    }
    else if (Compress_isCompressed(deltaCompressAlgorithm))
    {
      compressString = String_format(String_new(),
                                     "%s",
                                     Compress_getAlgorithmName(deltaCompressAlgorithm)
                                    );
    }
    else if (Compress_isCompressed(byteCompressAlgorithm))
    {
      compressString = String_format(String_new(),
                                     "%s",
                                     Compress_getAlgorithmName(byteCompressAlgorithm)
                                    );
    }
    else
    {
      compressString = String_format(String_new(),
                                     "%s",
                                     Compress_getAlgorithmName(deltaCompressAlgorithm)
                                    );
    }
    cryptString = String_format(String_new(),"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType==CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
    printf("                           %10llu..%10llu %-15s %6.1f%% %-10s %s\n",
           blockOffset*(uint64)blockSize,
           (blockOffset+blockCount)*(uint64)blockSize-((blockCount > 0) ? 1 : 0),
           String_cString(compressString),
           ratio,
           String_cString(cryptString),
           String_cString(imageName)
          );
    String_delete(cryptString);
    String_delete(compressString);

    if (Compress_isCompressed(deltaCompressAlgorithm) && (globalOptions.verboseLevel > 1))
    {
      printf("                                                                     source: %s, %llu bytes\n",String_cString(deltaSourceName),deltaSourceSize);
    }
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
* Input  : storageName     - storage name or NULL if storage name should
*                            not be printed
*          directoryName   - directory name
*          timeModified    - file modified time
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printDirectoryInfo(const String    storageName,
                              const String    directoryName,
                              uint64          timeModified,
                              CryptAlgorithms cryptAlgorithm,
                              CryptTypes      cryptType
                             )
{
  String dateTime;
  String cryptString;

  assert(directoryName != NULL);

  dateTime = Misc_formatDateTime(String_new(),timeModified,NULL);

  if (storageName != NULL)
  {
    printf("%-20s ",String_cString(storageName));
  }
  if (globalOptions.longFormatFlag)
  {
    cryptString = String_format(String_new(),"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType==CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
    printf("DIR                 %-25s                                                %-10s %s\n",
           String_cString(dateTime),
           String_cString(cryptString),
           String_cString(directoryName)
          );
    String_delete(cryptString);
  }
  else
  {
    printf("DIR                 %-25s %s\n",
           String_cString(dateTime),
           String_cString(directoryName)
          );
  }

  String_delete(dateTime);
}

/***********************************************************************\
* Name   : printLinkInfo
* Purpose: print link information
* Input  : storageName     - storage name or NULL if storage name should
*                            not be printed
*          linkName        - link name
*          destinationName - name of referenced file
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printLinkInfo(const String    storageName,
                         const String    linkName,
                         const String    destinationName,
                         CryptAlgorithms cryptAlgorithm,
                         CryptTypes      cryptType
                        )
{
  String cryptString;

  assert(linkName != NULL);
  assert(destinationName != NULL);

  if (storageName != NULL)
  {
    printf("%-20s ",String_cString(storageName));
  }
  if (globalOptions.longFormatFlag)
  {
    cryptString = String_format(String_new(),"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType==CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
    printf("LINK                                                                                         %-10s %s -> %s\n",
           String_cString(cryptString),
           String_cString(linkName),
           String_cString(destinationName)
          );
    String_delete(cryptString);
  }
  else
  {
    printf("LINK                                          %s -> %s\n",
           String_cString(linkName),
           String_cString(destinationName)
          );
  }
}

/***********************************************************************\
* Name   : printHardLinkInfo
* Purpose: print hard link information
* Input  : storageName            - storage name or NULL if storage name
*                                   should not be printed
*          fileName               - file name
*          size                   - file size [bytes]
*          timeModified           - file modified time
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

LOCAL void printHardLinkInfo(const String       storageName,
                             const String       fileName,
                             uint64             size,
                             uint64             timeModified,
                             uint64             archiveSize,
                             CompressAlgorithms deltaCompressAlgorithm,
                             CompressAlgorithms byteCompressAlgorithm,
                             CryptAlgorithms    cryptAlgorithm,
                             CryptTypes         cryptType,
                             const String       deltaSourceName,
                             uint64             deltaSourceSize,
                             uint64             fragmentOffset,
                             uint64             fragmentSize
                            )
{
  String dateTime;
  double ratio;
  char   buffer[16];
  String compressString;
  String cryptString;

  assert(fileName != NULL);

  dateTime = Misc_formatDateTime(String_new(),timeModified,NULL);

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

  if (storageName != NULL)
  {
    printf("%-20s ",String_cString(storageName));
  }
  printf("HARDLINK ");
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
    if      ((Compress_isCompressed(deltaCompressAlgorithm) && Compress_isCompressed(byteCompressAlgorithm)))
    {
      compressString = String_format(String_new(),
                                     "%s+%s",
                                     Compress_getAlgorithmName(deltaCompressAlgorithm),
                                     Compress_getAlgorithmName(byteCompressAlgorithm)
                                    );
    }
    else if (Compress_isCompressed(deltaCompressAlgorithm))
    {
      compressString = String_format(String_new(),
                                     "%s",
                                     Compress_getAlgorithmName(deltaCompressAlgorithm)
                                    );
    }
    else if (Compress_isCompressed(byteCompressAlgorithm))
    {
      compressString = String_format(String_new(),
                                     "%s",
                                     Compress_getAlgorithmName(byteCompressAlgorithm)
                                    );
    }
    else
    {
      compressString = String_format(String_new(),
                                     "%s",
                                     Compress_getAlgorithmName(deltaCompressAlgorithm)
                                    );
    }
    cryptString = String_format(String_new(),"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType==CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
    printf(" %-25s %10llu..%10llu %-15s %6.1f%% %-10s %s\n",
           String_cString(dateTime),
           fragmentOffset,
           (fragmentSize > 0LL) ? fragmentOffset+fragmentSize-1 : fragmentOffset,
           String_cString(compressString),
           ratio,
           String_cString(cryptString),
           String_cString(fileName)
          );
    String_delete(cryptString);
    String_delete(compressString);

    if (Compress_isCompressed(deltaCompressAlgorithm) && (globalOptions.verboseLevel > 1))
    {
      printf("                                                                     source: %s, %llu bytes\n",String_cString(deltaSourceName),deltaSourceSize);
    }
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
* Name   : printSpecialInfo
* Purpose: print special information
* Input  : storageName     - storage name or NULL if storage name should
*                            not be printed
*          fileName        - file name
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
*          fileSpecialType - special file type
*          major,minor     - special major/minor number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printSpecialInfo(const String     storageName,
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

  if (storageName != NULL)
  {
    printf("%-20s ",String_cString(storageName));
  }
  switch (fileSpecialType)
  {
    case FILE_SPECIAL_TYPE_CHARACTER_DEVICE:
      if (globalOptions.longFormatFlag)
      {
        cryptString = String_format(String_new(),"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType==CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
        printf("CHAR                                                                                         %-10s %s, %lu %lu\n",
               String_cString(cryptString),
               String_cString(fileName),
               major,
               minor
              );
        String_delete(cryptString);
      }
      else
      {
        printf("CHAR                                          %s\n",
               String_cString(fileName)
              );
      }
      break;
    case FILE_SPECIAL_TYPE_BLOCK_DEVICE:
      if (globalOptions.longFormatFlag)
      {
        cryptString = String_format(String_new(),"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType==CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
        printf("BLOCK                                                                                       %-10s %s, %lu %lu\n",
               String_cString(cryptString),
               String_cString(fileName),
               major,
               minor
              );
        String_delete(cryptString);
      }
      else
      {
        printf("BLOCK                                         %s\n",
               String_cString(fileName)
              );
      }
      break;
    case FILE_SPECIAL_TYPE_FIFO:
      if (globalOptions.longFormatFlag)
      {
        cryptString = String_format(String_new(),"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType==CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
        printf("FIFO                                                                                         %-10s %s\n",
               String_cString(cryptString),
               String_cString(fileName)
              );
        String_delete(cryptString);
      }
      else
      {
        printf("FIFO                                          %s\n",
               String_cString(fileName)
              );
      }
      break;
    case FILE_SPECIAL_TYPE_SOCKET:
      if (globalOptions.longFormatFlag)
      {
        cryptString = String_format(String_new(),"%s%c",Crypt_algorithmToString(cryptAlgorithm,"unknown"),(cryptType==CRYPT_TYPE_ASYMMETRIC) ? '*' : ' ');
        printf("SOCKET                                                                                      %-10s %s\n",
               String_cString(cryptString),
               String_cString(fileName)
              );
        String_delete(cryptString);
      }
      else
      {
        printf("SOCKET                                        %s\n",
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
* Input  : storageName            - storage name
*          fileName               - file name
*          fileSize               - file size [bytes]
*          timeModified           - file modified time
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

LOCAL void addListFileInfo(const String       storageName,
                           const String       fileName,
                           uint64             fileSize,
                           uint64             timeModified,
                           uint64             archiveSize,
                           CompressAlgorithms deltaCompressAlgorithm,
                           CompressAlgorithms byteCompressAlgorithm,
                           CryptAlgorithms    cryptAlgorithm,
                           CryptTypes         cryptType,
                           const String       deltaSourceName,
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

LOCAL void addListImageInfo(const String       storageName,
                            const String       imageName,
                            uint64             imageSize,
                            uint64             archiveSize,
                            CompressAlgorithms deltaCompressAlgorithm,
                            CompressAlgorithms byteCompressAlgorithm,
                            CryptAlgorithms    cryptAlgorithm,
                            CryptTypes         cryptType,
                            const String       deltaSourceName,
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
*          cryptAlgorithm - used crypt algorithm
*          cryptType      - crypt type; see CRYPT_TYPES
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListDirectoryInfo(const String    storageName,
                                const String    directoryName,
                                uint64          timeModified,
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
*          cryptAlgorithm  - used crypt algorithm
*          cryptType       - crypt type; see CRYPT_TYPES
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListLinkInfo(const String    storageName,
                           const String    linkName,
                           const String    destinationName,
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

LOCAL void addListHardLinkInfo(const String       storageName,
                               const String       fileName,
                               uint64             fileSize,
                               uint64             timeModified,
                               uint64             archiveSize,
                               CompressAlgorithms deltaCompressAlgorithm,
                               CompressAlgorithms byteCompressAlgorithm,
                               CryptAlgorithms    cryptAlgorithm,
                               CryptTypes         cryptType,
                               const String       deltaSourceName,
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
*          cryptAlgorithm   - used crypt algorithm
*          cryptType        - crypt type; see CRYPT_TYPES
*          FileSpecialTypes - special type
*          major,minor      - special major/minor number
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void addListSpecialInfo(const String     storageName,
                              const String     fileName,
                              CryptAlgorithms  cryptAlgorithm,
                              CryptTypes       cryptType,
                              FileSpecialTypes fileSpecialType,
                              ulong            major,
                              ulong            minor
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

LOCAL int compareArchiveContentNode(ArchiveContentNode *archiveContentNode1, ArchiveContentNode *archiveContentNode2, void *userData)
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
* Name   : printList
* Purpose: sort, group and print list with entries
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void printList(void)
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

/*---------------------------------------------------------------------*/

Errors Command_list(StringList                      *storageNameList,
                    EntryList                       *includeEntryList,
                    PatternList                     *excludePatternList,
                    JobOptions                      *jobOptions,
                    ArchiveGetCryptPasswordFunction archiveGetCryptPasswordFunction,
                    void                            *archiveGetCryptPasswordUserData
                   )
{
  AutoFreeList     autoFreeList;
  String           storageName;
  StorageSpecifier storageSpecifier;
  String           printableStorageName;
  String           storageFileName;
  bool             printedInfoFlag;
  ulong            fileCount;
  Errors           failError;
  Errors           error;
  bool             retryFlag;
bool         remoteBarFlag;
//  SSHSocketList sshSocketList;
//  SSHSocketNode *sshSocketNode;
  SocketHandle socketHandle;

  assert(storageNameList != NULL);
  assert(includeEntryList != NULL);
  assert(excludePatternList != NULL);
  assert(jobOptions != NULL);

// NYI ???
remoteBarFlag=FALSE;

  // init variables
  AutoFree_init(&autoFreeList);
  List_init(&archiveContentList);
  storageName          = String_new();
  Storage_initSpecifier(&storageSpecifier);
  storageFileName      = String_new();
  printableStorageName = String_new();
  AUTOFREE_ADD(&autoFreeList,printableStorageName,{ String_delete(printableStorageName); });
  AUTOFREE_ADD(&autoFreeList,storageFileName,{ String_delete(storageFileName); });
  AUTOFREE_ADD(&autoFreeList,&storageSpecifier,{ Storage_doneSpecifier(&storageSpecifier); });
  AUTOFREE_ADD(&autoFreeList,storageName,{ String_delete(storageName); });
  AUTOFREE_ADD(&autoFreeList,&archiveContentList,{ List_done(&archiveContentList,(ListNodeFreeFunction)freeArchiveContentNode,NULL); });

  // list archive content
  failError = ERROR_NONE;
  while (!StringList_isEmpty(storageNameList))
  {
    StringList_getFirst(storageNameList,storageName);

    // parse storage name, get printable name
    error = Storage_parseName(storageName,&storageSpecifier,storageFileName);
    if (error != ERROR_NONE)
    {
      printError("Invalid storage '%s' (error: %s)!\n",
                 String_cString(storageName),
                 Errors_getText(error)
                );
      if (failError == ERROR_NONE) failError = error;
      continue;
    }
    Storage_getPrintableName(printableStorageName,&storageSpecifier,storageFileName);

    printedInfoFlag = FALSE;
    fileCount       = 0L;
    switch (storageSpecifier.type)
    {
      case STORAGE_TYPE_FILESYSTEM:
      case STORAGE_TYPE_FTP:
      case STORAGE_TYPE_SFTP:
      case STORAGE_TYPE_WEBDAV:
      case STORAGE_TYPE_CD:
      case STORAGE_TYPE_DVD:
      case STORAGE_TYPE_BD:
        {
          ArchiveInfo       archiveInfo;
          ArchiveEntryInfo  archiveEntryInfo;
          ArchiveEntryTypes archiveEntryType;

          // open archive
          error = Archive_open(&archiveInfo,
                               &storageSpecifier,
                               storageFileName,
                               jobOptions,
                               &globalOptions.maxBandWidthList,
                               archiveGetCryptPasswordFunction,
                               archiveGetCryptPasswordUserData
                              );
          if (error != ERROR_NONE)
          {
            printError("Cannot open storage '%s' (error: %s)!\n",
                       String_cString(printableStorageName),
                       Errors_getText(error)
                      );
            if (failError == ERROR_NONE) failError = error;
            continue;
          }

          // list contents
          while (   !Archive_eof(&archiveInfo,TRUE)
                 && (failError == ERROR_NONE)
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
                         String_cString(printableStorageName),
                         Errors_getText(error)
                        );
              if (failError == ERROR_NONE) failError = error;
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
                  uint64             delteSourceSize;
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
                                                &delteSourceSize,
                                                &fragmentOffset,
                                                &fragmentSize
                                               );
                  if (error != ERROR_NONE)
                  {
                    printError("Cannot read 'file' content from storage '%s' (error: %s)!\n",
                               String_cString(printableStorageName),
                               Errors_getText(error)
                              );
                    String_delete(deltaSourceName);
                    String_delete(fileName);
                    if (failError == ERROR_NONE) failError = error;
                    break;
                  }

                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add file info to list
                      addListFileInfo(storageName,
                                      fileName,
                                      fileInfo.size,
                                      fileInfo.timeModified,
                                      archiveEntryInfo.file.chunkFileData.info.size,
                                      deltaCompressAlgorithm,
                                      byteCompressAlgorithm,
                                      cryptAlgorithm,
                                      cryptType,
                                      deltaSourceName,
                                      delteSourceSize,
                                      fragmentOffset,
                                      fragmentSize
                                     );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printHeader(printableStorageName);
                        printedInfoFlag = TRUE;
                      }

                      // output file info
                      printFileInfo(NULL,
                                    fileName,
                                    fileInfo.size,
                                    fileInfo.timeModified,
                                    archiveEntryInfo.file.chunkFileData.info.size,
                                    deltaCompressAlgorithm,
                                    byteCompressAlgorithm,
                                    cryptAlgorithm,
                                    cryptType,
                                    deltaSourceName,
                                    delteSourceSize,
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
                    printWarning("close 'file' entry fail (error: %s)\n",Errors_getText(error));
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
                  String             deltaSourceName;
                  uint64             delteSourceSize;
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
                                                 deltaSourceName,
                                                 &delteSourceSize,
                                                 &blockOffset,
                                                 &blockCount
                                                );
                  if (error != ERROR_NONE)
                  {
                    printError("Cannot read 'image' content from storage '%s' (error: %s)!\n",
                               String_cString(printableStorageName),
                               Errors_getText(error)
                              );
                    String_delete(deltaSourceName);
                    String_delete(deviceName);
                    if (failError == ERROR_NONE) failError = error;
                    break;
                  }

                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,deviceName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,deviceName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add image info to list
                      addListImageInfo(storageName,
                                       deviceName,
                                       deviceInfo.size,
                                       archiveEntryInfo.image.chunkImageData.info.size,
                                       deltaCompressAlgorithm,
                                       byteCompressAlgorithm,
                                       cryptAlgorithm,
                                       cryptType,
                                       deltaSourceName,
                                       delteSourceSize,
                                       deviceInfo.blockSize,
                                       blockOffset,
                                       blockCount
                                      );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printHeader(printableStorageName);
                        printedInfoFlag = TRUE;
                      }

                      // output file info
                      printImageInfo(NULL,
                                     deviceName,
                                     deviceInfo.size,
                                     archiveEntryInfo.image.chunkImageData.info.size,
                                     deltaCompressAlgorithm,
                                     byteCompressAlgorithm,
                                     cryptAlgorithm,
                                     cryptType,
                                     deltaSourceName,
                                     delteSourceSize,
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
                    printWarning("close 'image' entry fail (error: %s)\n",Errors_getText(error));
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
                               String_cString(printableStorageName),
                               Errors_getText(error)
                              );
                    String_delete(directoryName);
                    if (failError == ERROR_NONE) failError = error;
                    break;
                  }

                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add directory info to list
                      addListDirectoryInfo(storageName,
                                           directoryName,
                                           fileInfo.timeModified,
                                           cryptAlgorithm,
                                           cryptType
                                          );
                    }
                    else
                    {
                      if (!printedInfoFlag)
                      {
                        printHeader(printableStorageName);
                        printedInfoFlag = TRUE;
                      }

                      // output file info
                      printDirectoryInfo(NULL,
                                         directoryName,
                                         fileInfo.timeModified,
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
                    printWarning("close 'directory' entry fail (error: %s)\n",Errors_getText(error));
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
                               String_cString(printableStorageName),
                               Errors_getText(error)
                              );
                    String_delete(fileName);
                    String_delete(linkName);
                    if (failError == ERROR_NONE) failError = error;
                    break;
                  }

                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add link info to list
                      addListLinkInfo(storageName,
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
                        printHeader(printableStorageName);
                        printedInfoFlag = TRUE;
                      }

                      // output file info
                      printLinkInfo(NULL,
                                    linkName,
                                    fileName,
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
                    printWarning("close 'link' entry fail (error: %s)\n",Errors_getText(error));
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
                               String_cString(printableStorageName),
                               Errors_getText(error)
                              );
                    String_delete(deltaSourceName);
                    StringList_done(&fileNameList);
                    if (failError == ERROR_NONE) failError = error;
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
                        addListHardLinkInfo(storageName,
                                            fileName,
                                            fileInfo.size,
                                            fileInfo.timeModified,
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
                        if (!printedInfoFlag)
                        {
                          printHeader(printableStorageName);
                          printedInfoFlag = TRUE;
                        }

                        // output file info
                        printHardLinkInfo(NULL,
                                          fileName,
                                          fileInfo.size,
                                          fileInfo.timeModified,
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
                    printWarning("close 'hard link' entry fail (error: %s)\n",Errors_getText(error));
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
                               String_cString(printableStorageName),
                               Errors_getText(error)
                              );
                    String_delete(fileName);
                    if (failError == ERROR_NONE) failError = error;
                    break;
                  }

                  if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                      && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                     )
                  {
                    if (globalOptions.groupFlag)
                    {
                      // add special info to list
                      addListSpecialInfo(storageName,
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
                        printHeader(printableStorageName);
                        printedInfoFlag = TRUE;
                      }

                      // output file info
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

                  // close archive file, free resources
                  error = Archive_closeEntry(&archiveEntryInfo);
                  if (error != ERROR_NONE)
                  {
                    printWarning("close 'special' entry fail (error: %s)\n",Errors_getText(error));
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
        }
        break;
      case STORAGE_TYPE_SSH:
      case STORAGE_TYPE_SCP:
        {
          SSHServer            sshServer;
          String               s;
          Password             sshPassword;
          NetworkExecuteHandle networkExecuteHandle;
          String               line;
          uint                 majorVersion,minorVersion;
          Errors               error;
          int                  id,errorCode;
          bool                 completedFlag;
          char                 type[32];
          String               arguments;
          int                  exitcode;

          // start remote BAR via SSH (if not already started)
          if (!remoteBarFlag)
          {
            // get SSH server settings
            getSSHServerSettings(storageSpecifier.hostName,jobOptions,&sshServer);
            if (storageSpecifier.hostPort == 0) storageSpecifier.hostPort = sshServer.port;
            if (String_isEmpty(storageSpecifier.loginName)) String_set(storageSpecifier.loginName,sshServer.loginName);

            // connect to SSH server
            error = Network_connect(&socketHandle,
                                    SOCKET_TYPE_SSH,
                                    storageSpecifier.hostName,
                                    storageSpecifier.hostPort,
                                    storageSpecifier.loginName,
                                    sshServer.password,
                                    sshServer.publicKeyFileName,
                                    sshServer.privateKeyFileName,
                                    0
                                   );
            if (error != ERROR_NONE)
            {
              Password_init(&sshPassword);

              s = String_newCString("SSH login password for ");
              if (!String_isEmpty(storageSpecifier.loginName)) String_format(s,"%S@",storageSpecifier.loginName);
              String_format(s,"%S",storageSpecifier.hostName);
              Password_input(&sshPassword,String_cString(s),PASSWORD_INPUT_MODE_ANY);
              String_delete(s);

              error = Network_connect(&socketHandle,
                                      SOCKET_TYPE_SSH,
                                      storageSpecifier.hostName,
                                      storageSpecifier.hostPort,
                                      storageSpecifier.loginName,
                                      &sshPassword,
                                      sshServer.publicKeyFileName,
                                      sshServer.privateKeyFileName,
                                      0
                                     );

              Password_done(&sshPassword);
            }
            if (error != ERROR_NONE)
            {
              printError("Cannot connect to '%s:%d' (error: %s)!\n",
                         String_cString(storageSpecifier.hostName),
                         storageSpecifier.hostPort,
                         Errors_getText(error)
                        );
              if (failError == ERROR_NONE) failError = error;
              break;
            }

            remoteBarFlag = TRUE;
          }

          // start remote BAR in batch mode
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
                       Errors_getText(error)
                      );
            String_delete(line);
            if (failError == ERROR_NONE) failError = error;
            break;
          }
          if (Network_executeEOF(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,60*1000))
          {
            Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,line,0);
            exitcode = Network_terminate(&networkExecuteHandle);
            printError("No response from remote BAR program (error: %s, exitcode %d)!\n",!String_isEmpty(line) ? String_cString(line) : "unknown",exitcode);
            String_delete(line);
            if (failError == ERROR_NONE) failError = ERROR_NO_RESPONSE;
            break;
          }
          Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,line,0);
          if (!String_parse(line,STRING_BEGIN,"BAR VERSION %d %d",NULL,&majorVersion,&minorVersion))
          {
            Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,line,0);
            exitcode = Network_terminate(&networkExecuteHandle);
            printError("Invalid response from remote BAR program (error: %s, exitcode %d)!\n",!String_isEmpty(line) ? String_cString(line) : "unknown",exitcode);
            String_delete(line);
            if (failError == ERROR_NONE) failError = ERROR_INVALID_RESPONSE;
            break;
          }
          String_delete(line);
          UNUSED_VARIABLE(majorVersion);
          UNUSED_VARIABLE(minorVersion);

          // read archive content
          arguments = String_new();
          line            = String_new();
          do
          {
            // send decrypt password
            if (!Password_isEmpty(jobOptions->cryptPassword))
            {
              String_format(String_clear(line),"1 SET crypt-password %'s",jobOptions->cryptPassword);
              Network_executeWriteLine(&networkExecuteHandle,line);
            }

            // send list archive command
            String_format(String_clear(line),"2 ARCHIVE_LIST %S",storageFileName);
            Network_executeWriteLine(&networkExecuteHandle,line);
            Network_executeSendEOF(&networkExecuteHandle);

            // list contents
            while (!Network_executeEOF(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,60*1000))
            {
              // read line
              Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDOUT,line,60*1000);
//fprintf(stderr,"%s, %d: line=%s\n",__FILE__,__LINE__,String_cString(line));

              // parse and output list
              if      (String_parse(line,
                                    STRING_BEGIN,
                                    "%d %d %y %32s % S",
                                    NULL,
                                    &id,
                                    &errorCode,
                                    &completedFlag,
                                    &type,
                                    arguments
                                   )
                      )
              {
                if (   (errorCode == ERROR_NONE)
                    && !completedFlag
                   )
                {
                  if      (strcmp(type,"FILE") == 0)
                  {
                    String               fileName;
                    uint64               fileSize;
                    uint64               timeModified;
                    uint64               archiveFileSize;
                    CompressAlgorithms   deltaCompressAlgorithm;
                    CompressAlgorithms   byteCompressAlgorithm;
                    CryptAlgorithms      cryptAlgorithm;
                    CryptTypes           cryptType;
                    String               deltaSourceName;
                    uint64               deltaSourceSize;
                    uint64               fragmentOffset,fragmentLength;
//fprintf(stderr,"%s, %d: type=#%s# arguments=%s\n",__FILE__,__LINE__,type,String_cString(arguments));

                    // initialize variables
                    fileName        = String_new();
                    deltaSourceName = String_new();

                    // parse file entry
                    if (String_parse(arguments,
                                     STRING_BEGIN,
                                     "%'S %llu %llu %d %d %d %d %d %'S %llu %llu %llu",
                                     NULL,
                                     fileName,
                                     &fileSize,
                                     &timeModified,
                                     &archiveFileSize,
                                     &deltaCompressAlgorithm,
                                     &byteCompressAlgorithm,
                                     &cryptAlgorithm,
                                     &cryptType,
                                     deltaSourceName,
                                     &deltaSourceSize,
                                     &fragmentOffset,
                                     &fragmentLength
                                    )
                       )
                    {
                      if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                          && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                         )
                      {
                        if (globalOptions.groupFlag)
                        {
                          // add file info to list
                          addListFileInfo(storageName,
                                          fileName,
                                          fileSize,
                                          timeModified,
                                          archiveFileSize,
                                          deltaCompressAlgorithm,
                                          byteCompressAlgorithm,
                                          cryptAlgorithm,
                                          cryptType,
                                          deltaSourceName,
                                          deltaSourceSize,
                                          fragmentOffset,
                                          fragmentLength
                                         );
                        }
                        else
                        {
                          if (!printedInfoFlag)
                          {
                            printHeader(printableStorageName);
                            printedInfoFlag = TRUE;
                          }

                          // output file info
                          printFileInfo(NULL,
                                        fileName,
                                        fileSize,
                                        timeModified,
                                        archiveFileSize,
                                        deltaCompressAlgorithm,
                                        byteCompressAlgorithm,
                                        cryptAlgorithm,
                                        cryptType,
                                        deltaSourceName,
                                        deltaSourceSize,
                                        fragmentOffset,
                                        fragmentLength
                                       );
                        }
                        fileCount++;
                      }
                    }
                    else
                    {
                      printWarning("Parsing 'file' entry '%s' fail\n",String_cString(arguments));
                    }

                    // free resources
                    String_delete(deltaSourceName);
                    String_delete(fileName);
                  }
                  else if (strcmp(type,"IMAGE") == 0)
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
//fprintf(stderr,"%s, %d: type=#%s# arguments=%s\n",__FILE__,__LINE__,type,String_cString(arguments));

                    // initialize variables
                    imageName       = String_new();
                    deltaSourceName = String_new();

                    // parse image entry
                    if (String_parse(arguments,
                                     STRING_BEGIN,
                                     "%'S %llu %llu %d %d %d %d %'S %llu %d %llu %llu",
                                     NULL,
                                     imageName,
                                     &imageSize,
                                     &archiveFileSize,
                                     &deltaCompressAlgorithm,
                                     &byteCompressAlgorithm,
                                     &cryptAlgorithm,
                                     &cryptType,
                                     deltaSourceName,
                                     &deltaSourceSize,
                                     &blockSize,
                                     &blockOffset,
                                     &blockCount
                                    )
                       )
                    {
                      if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,imageName,PATTERN_MATCH_MODE_EXACT))
                          && !PatternList_match(excludePatternList,imageName,PATTERN_MATCH_MODE_EXACT)
                         )
                      {
                        if (globalOptions.groupFlag)
                        {
                          // add file info to list
                          addListImageInfo(storageName,
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
                            printHeader(printableStorageName);
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
                      printWarning("Parsing 'image' entry '%s' fail\n",String_cString(arguments));
                    }

                    // free resources
                    String_delete(deltaSourceName);
                    String_delete(imageName);
                  }
                  else if (strcmp(type,"DIRECTORY") == 0)
                  {
                    String          directoryName;
                    uint64          timeModified;
                    CryptAlgorithms cryptAlgorithm;
                    CryptTypes      cryptType;
//fprintf(stderr,"%s, %d: type=#%s# arguments=%s\n",__FILE__,__LINE__,type,String_cString(arguments));

                    // initialize variables
                    directoryName = String_new();

                    // parse directory entry
                    if (String_parse(arguments,
                                     STRING_BEGIN,
                                     "%'S %llu %d %d",
                                     NULL,
                                     directoryName,
                                     &timeModified,
                                     &cryptAlgorithm,
                                     &cryptType
                                    )
                       )
                    {
                      if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,directoryName,PATTERN_MATCH_MODE_EXACT))
                          && !PatternList_match(excludePatternList,directoryName,PATTERN_MATCH_MODE_EXACT)
                         )
                      {
                        if (globalOptions.groupFlag)
                        {
                          // add directory info to list
                          addListDirectoryInfo(storageName,
                                               directoryName,
                                               timeModified,
                                               cryptAlgorithm,
                                               cryptType
                                              );
                        }
                        else
                        {
                          if (!printedInfoFlag)
                          {
                            printHeader(printableStorageName);
                            printedInfoFlag = TRUE;
                          }

                          // output file info
                          printDirectoryInfo(NULL,
                                             directoryName,
                                             timeModified,
                                             cryptAlgorithm,
                                             cryptType
                                            );
                        }
                        fileCount++;
                      }
                    }
                    else
                    {
                      printWarning("Parsing 'file' entry '%s' fail\n",String_cString(arguments));
                    }

                    // free resources
                    String_delete(directoryName);
                  }
                  else if (strcmp(type,"LINK") == 0)
                  {
                    String          linkName,fileName;
                    CryptAlgorithms cryptAlgorithm;
                    CryptTypes      cryptType;
//fprintf(stderr,"%s, %d: type=#%s# arguments=%s\n",__FILE__,__LINE__,type,String_cString(arguments));

                    // initialize variables
                    linkName = String_new();
                    fileName = String_new();

                    // parse symbolic link entry
                    if (String_parse(arguments,
                                     STRING_BEGIN,
                                     "%'S %'S %d %d",
                                     NULL,
                                     linkName,
                                     fileName,
                                     &cryptAlgorithm,
                                     &cryptType
                                    )
                       )
                    {
                      if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,linkName,PATTERN_MATCH_MODE_EXACT))
                          && !PatternList_match(excludePatternList,linkName,PATTERN_MATCH_MODE_EXACT)
                         )
                      {
                        if (globalOptions.groupFlag)
                        {
                          // add linkinfo to list
                          addListLinkInfo(storageName,
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
                            printHeader(printableStorageName);
                            printedInfoFlag = TRUE;
                          }

                          // output file info
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
                    else
                    {
                      printWarning("Parsing 'file' entry '%s' fail\n",String_cString(arguments));
                    }

                    // free resources
                    String_delete(fileName);
                    String_delete(linkName);
                  }
                  else if (strcmp(type,"HARDLINK") == 0)
                  {
                    String               fileName;
                    uint64               fileSize;
                    uint64               timeModified;
                    uint64               archiveFileSize;
                    CompressAlgorithms   deltaCompressAlgorithm;
                    CompressAlgorithms   byteCompressAlgorithm;
                    CryptAlgorithms      cryptAlgorithm;
                    CryptTypes           cryptType;
                    String               deltaSourceName;
                    uint64               deltaSourceSize;
                    uint64               fragmentOffset,fragmentLength;
//fprintf(stderr,"%s, %d: type=#%s# arguments=%s\n",__FILE__,__LINE__,type,String_cString(arguments));

                    // initialize variables
                    fileName        = String_new();
                    deltaSourceName = String_new();

                    // parse hard link entry
                    if (String_parse(arguments,
                                     STRING_BEGIN,
                                     "%'S %llu %llu %d %d %d %d %d %'S %llu %llu %llu",
                                     NULL,
                                     fileName,
                                     &fileSize,
                                     &timeModified,
                                     &archiveFileSize,
                                     &deltaCompressAlgorithm,
                                     &byteCompressAlgorithm,
                                     &cryptAlgorithm,
                                     &cryptType,
                                     deltaSourceName,
                                     &deltaSourceSize,
                                     &fragmentOffset,
                                     &fragmentLength
                                    )
                       )
                    {
                      if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                          && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                         )
                      {
                        if (globalOptions.groupFlag)
                        {
                          // add file info to list
                          addListHardLinkInfo(storageName,
                                              fileName,
                                              fileSize,
                                              timeModified,
                                              archiveFileSize,
                                              deltaCompressAlgorithm,
                                              byteCompressAlgorithm,
                                              cryptAlgorithm,
                                              cryptType,
                                              deltaSourceName,
                                              deltaSourceSize,
                                              fragmentOffset,
                                              fragmentLength
                                             );
                        }
                        else
                        {
                          if (!printedInfoFlag)
                          {
                            printHeader(printableStorageName);
                            printedInfoFlag = TRUE;
                          }

                          // output file info
                          printHardLinkInfo(NULL,
                                            fileName,
                                            fileSize,
                                            timeModified,
                                            archiveFileSize,
                                            deltaCompressAlgorithm,
                                            byteCompressAlgorithm,
                                            cryptAlgorithm,
                                            cryptType,
                                            deltaSourceName,
                                            deltaSourceSize,
                                            fragmentOffset,
                                            fragmentLength
                                           );
                        }
                        fileCount++;
                      }
                    }
                    else
                    {
                      printWarning("Parsing 'file' entry '%s' fail\n",String_cString(arguments));
                    }

                    // free resources
                    String_delete(deltaSourceName);
                    String_delete(fileName);
                  }
                  else if (strcmp(type,"SPECIAL") == 0)
                  {
                    String           fileName;
                    CryptAlgorithms  cryptAlgorithm;
                    CryptTypes       cryptType;
                    FileSpecialTypes fileSpecialType;
                    ulong            major;
                    ulong            minor;
//fprintf(stderr,"%s, %d: type=#%s# arguments=%s\n",__FILE__,__LINE__,type,String_cString(arguments));

                    // initialize variables
                    fileName = String_new();

                    // parse special entry
                    if (String_parse(arguments,
                                     STRING_BEGIN,
                                     "%'S %d %d %d %ld %ld",
                                     NULL,
                                     fileName,
                                     &cryptAlgorithm,
                                     &cryptType,
                                     &fileSpecialType,
                                     &major,
                                     &minor
                                    )
                       )
                    {
                      if (   (List_isEmpty(includeEntryList) || EntryList_match(includeEntryList,fileName,PATTERN_MATCH_MODE_EXACT))
                          && !PatternList_match(excludePatternList,fileName,PATTERN_MATCH_MODE_EXACT)
                         )
                      {
                        if (globalOptions.groupFlag)
                        {
                          // add special info to list
                          addListSpecialInfo(storageName,
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
                            printHeader(printableStorageName);
                            printedInfoFlag = TRUE;
                          }

                          // output file info
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
                      printWarning("Parsing 'file' entry '%s' fail\n",String_cString(arguments));
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
              else
              {
                printWarning("Parsing entry '%s' fail\n",String_cString(line));
              }
            }
            if (globalOptions.verboseLevel >= 4)
            {
              while (!Network_executeEOF(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,60*1000))
              {
                Network_executeReadLine(&networkExecuteHandle,NETWORK_EXECUTE_IO_TYPE_STDERR,line,0);
                if (String_length(line)>0) fprintf(stderr,"%s\n",String_cString(line));
              }
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
          String_delete(arguments);
          String_delete(line);

          // close connection
          exitcode = Network_terminate(&networkExecuteHandle);
          if (exitcode != 0)
          {
            printError("Remote BAR program return exitcode %d!\n",exitcode);
            if (failError == ERROR_NONE) failError = ERROR_NETWORK_EXECUTE_FAIL;
          }
        }
        break;
      case STORAGE_TYPE_DEVICE:
        printError("List archives on device is not supported!\n");
        failError = ERROR_FUNCTION_NOT_SUPPORTED;
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

  // output grouped list
  if (globalOptions.groupFlag)
  {
    printHeader(NULL);
    printList();
    printFooter(List_count(&archiveContentList));
  }

  // free resources
  String_delete(printableStorageName);
  String_delete(storageFileName);
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(storageName);
  List_done(&archiveContentList,(ListNodeFreeFunction)freeArchiveContentNode,NULL);
  AutoFree_done(&autoFreeList);

  return failError;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
