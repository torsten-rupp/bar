/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Backup ARchiver server
* Systems: all
*
\***********************************************************************/

#ifndef __SERVER__
#define __SERVER__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "arrays.h"
#include "stringmaps.h"

#include "passwords.h"

#include "remote.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define SERVER_PROTOCOL_VERSION_MAJOR 4
#define SERVER_PROTOCOL_VERSION_MINOR 0

#define SESSION_ID_LENGTH 64      // max. length of session id

/***************************** Datatypes *******************************/

// session id
typedef byte SessionId[SESSION_ID_LENGTH];

// server info
typedef struct
{
  SessionId    sessionId;
  CryptKey     publicKey,secretKey;

  uint         commandId;

  // connection
  String       name;
  uint         port;
  SocketHandle socketHandle;
} ServerInfo;

// create status info data
typedef struct
{
  ulong  doneEntries;                      // number of entries processed
  uint64 doneBytes;                        // number of bytes processed
  ulong  totalEntries;                     // total number of entries
  uint64 totalBytes;                       // total bytes
  bool   collectTotalSumDone;              // TRUE iff all file sums are collected
  ulong  skippedEntries;                   // number of skipped enttries
  uint64 skippedBytes;                     // sum of skipped bytes
  ulong  errorEntries;                     // number of enttries with errors
  uint64 errorBytes;                       // sum of byste in entries with errors
  uint64 archiveBytes;                     // number of bytes in stored in archive
  double compressionRatio;                 // compression ratio
  String entryName;                        // current entry name
  uint64 entryDoneBytes;                   // number of bytes processed of current entry
  uint64 entryTotalBytes;                  // total number of bytes of current entry
  String storageName;                      // current storage name
  uint64 storageDoneBytes;                 // number of bytes processed of current archive
  uint64 storageTotalBytes;                // total bytes of current archive
  uint   volumeNumber;                     // current volume number
  double volumeProgress;                   // current volume progress [0..100]
} CreateStatusInfo;

/***********************************************************************\
* Name   : CreateStatusInfoFunction
* Purpose: create status info call-back
* Input  : userData         - user data
*          error            - error code
*          createStatusInfo - create status info
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*CreateStatusInfoFunction)(void                   *userData,
                                        Errors                 error,
                                        const CreateStatusInfo *createStatusInfo
                                       );

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Server_initAll
* Purpose: initialize server
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Server_initAll(void);

/***********************************************************************\
* Name   : Server_doneAll
* Purpose: deinitialize server
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Server_doneAll(void);

/***********************************************************************\
* Name   : Server_run
* Purpose: run network server
* Input  : serverPort          - server port (or 0 to disable)
*          serverTLSPort       - server TLS (SSL) port (or 0 to disable)
*          caFileName          - file with TLS CA or NULL
*          certFileName        - file with TLS cerificate or NULL
*          keyFileName         - file with TLS key or NULL
*          serverPassword      - server authenfication password
*          serverJobsDirectory - server jobs directory
*          defaultJobOptions   - default job options
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Server_run(uint             serverPort,
                  uint             serverTLSPort,
                  const char       *caFileName,
                  const char       *certFileName,
                  const char       *keyFileName,
                  const Password   *serverPassword,
                  const char       *serverJobsDirectory,
                  const JobOptions *defaultJobOptions
                 );

/***********************************************************************\
* Name   : Server_batch
* Purpose: run batch server
* Input  : inputDescriptor  - input file descriptor
*          outputDescriptor - input file descriptor
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Server_batch(int inputDescriptor,
                    int outputDescriptor
                   );

#if 0
/***********************************************************************\
* Name   : Server_addJob
* Purpose: add new job to server for execution
* Input  : jobType            - job type
           name               - name of job
*          archiveName        - archive name
*          includePatternList - include pattern list
*          excludePatternList - exclude pattern list
*          jobOptions         - job options
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Server_addJob(JobTypes          jobType,
                     const String      name,
                     const String      archiveName,
                     const PatternList *includePatternList,
                     const PatternList *excludePatternList,
                     const JobOptions  *jobOptions
                    );
#endif /* 0 */

#ifdef __cplusplus
  }
#endif

#endif /* __SERVER__ */

/* end of file */
