/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver archive create functions
* Systems: all
*
\***********************************************************************/

#ifndef __COMMANDS_CREATE__
#define __COMMANDS_CREATE__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "errors.h"
#include "entrylists.h"
#include "patternlists.h"
#include "deltasourcelists.h"
#include "compress.h"
#include "crypt.h"
#include "storage.h"
#include "server.h"
#include "bar_global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
// create status info data
typedef struct
{
  ulong  doneCount;                        // number of entries processed
  uint64 doneSize;                         // number of bytes processed
  ulong  totalEntryCount;                  // total number of entries
  uint64 totalEntrySize;                   // total size of entries [bytes]
  bool   collectTotalSumDone;              // TRUE iff all file sums are collected
  ulong  skippedEntryCount;                // number of skipped entries
  uint64 skippedEntrySize;                 // sum of skipped bytes
  ulong  errorEntryCount;                  // number of entries with errors
  uint64 errorEntrySize;                   // sum of bytes of entries with errors
  uint64 archiveSize;                      // number of bytes stored in archive
  double compressionRatio;                 // compression ratio
  String entryName;                        // current entry name
  uint64 entryDoneSize;                    // number of bytes processed of current entry
  uint64 entryTotalSize;                   // total number of bytes of current entry
  String storageName;                      // current storage name
  uint64 storageDoneSize;                  // number of bytes processed of current archive
  uint64 storageTotalSize;                 // total bytes of current archive
  uint   volumeNumber;                     // current volume number
  double volumeProgress;                   // current volume progress [0..100]
} CreateStatusInfo;

/***********************************************************************\
* Name   : CreateStatusInfoFunction
* Purpose: create status info call-back
* Input  : error            - error code
*          createStatusInfo - create status info
*          userData         - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*CreateStatusInfoFunction)(Errors                 error,
                                        const CreateStatusInfo *createStatusInfo,
                                        void                   *userData
                                       );

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Command_create
* Purpose: create archive
* Input  : jobUUID                      - unique job id to store or NULL
*          scheduleUUID                 - unique schedule id to store or
*                                         NULL
*          storageName                  - storage name
*          includeEntryList             - include entry list
*          excludePatternList           - exclude pattern list
*          mountList                    - mount list or NULL
*          compressExcludePatternList   - exclude compression pattern
*                                         list
*          deltaSourceList              - delta soruce list
*          jobOptions                   - job options
*          entityId                     - entityId or INDEX_ID_NONE
*          archiveType                  - archive type; see
*                                         ArchiveTypes (normal/full/
*                                         incremental)
*          getNamePasswordFunction      - get password call back (can
*                                         be NULL)
*          getNamePasswordUserData      - user data for get password
*                                         call back
*          createStatusInfoFunction     - status info call back
*                                         function (can be NULL)
*          createStatusInfoUserData     - user data for status info
*                                         function
*          storageRequestVolumeFunction - request volume call back
*                                         function (can be NULL)
*          storageRequestVolumeUserData - user data for request
*                                         volume
*          pauseCreateFlag              - pause creation flag (can
*                                         be NULL)
*          pauseStorageFlag             - pause storage flag (can
*                                         be NULL)
*          requestedAbortFlag           - request abort flag (can be
*                                         NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Command_create(ConstString                  jobUUID,
                      ConstString                  scheduleUUID,
                      ServerIO                     *masterIO,
                      ConstString                  storageName,
                      const EntryList              *includeEntryList,
                      const PatternList            *excludePatternList,
                      MountList                    *mountList,
                      const PatternList            *compressExcludePatternList,
                      DeltaSourceList              *deltaSourceList,
                      JobOptions                   *jobOptions,
                      ArchiveTypes                 archiveType,
                      ConstString                  scheduleTitle,
                      ConstString                  scheduleCustomText,
                      GetNamePasswordFunction      getNamePasswordFunction,
                      void                         *getNamePasswordUserData,
                      CreateStatusInfoFunction     createStatusInfoFunction,
                      void                         *createStatusInfoUserData,
                      StorageRequestVolumeFunction storageRequestVolumeFunction,
                      void                         *storageRequestVolumeUserData,
                      bool                         *pauseCreateFlag,
                      bool                         *pauseStorageFlag,
                      bool                         *requestedAbortFlag,
                      LogHandle                    *logHandle
                     );

#ifdef __cplusplus
  }
#endif

#endif /* __COMMANDS_CREATE__ */

/* end of file */
