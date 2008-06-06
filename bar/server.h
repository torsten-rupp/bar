/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/server.h,v $
* $Revision: 1.12 $
* $Author: torsten $
* Contents: Backup ARchiver server
* Systems: all
*
\***********************************************************************/

#ifndef __SERVER__
#define __SERVER__

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "arrays.h"

#include "passwords.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/* backup/restore job */
typedef enum
{
  JOB_TYPE_BACKUP,
  JOB_TYPE_RESTORE,
} JobTypes;

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
* Input  : serverPort         - server port (or 0 to disable)
*          serverTLSPort      - server TLS (SSL) port (or 0 to disable)
*          caFileName         - file with TLS CA or NULL
*          certFileName       - file with TLS cerificate or NULL
*          keyFileName        - file with TLS key or NULL
*          serverPassword     - server authenfication password
*          serverJobDirectory - server job directory
*          defaultJobOptions  - default job options
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
                  const char       *serverJobDirectory,
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

#ifdef __cplusplus
  }
#endif

#endif /* __SERVER__ */

/* end of file */
