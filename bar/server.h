/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/server.h,v $
* $Revision: 1.10 $
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

#if 0
obsolet
typedef enum
{
  SERVER_RESULT_TYPE_NONE,
  SERVER_RESULT_TYPE_INT,
  SERVER_RESULT_TYPE_INT64,
  SERVER_RESULT_TYPE_DOUBLE,
  SERVER_RESULT_TYPE_CSTRING,
  SERVER_RESULT_TYPE_STRING,
} ServerResultTypes;

typedef struct
{
  ServerResultTypes type;
  union
  {
    int    i;
    int64  l;
    double d;
    char   s[256];
    String string;
  };
} ServerResult;
#endif /* 0 */

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
* Input  : serverPort     - server port (or 0 to disable)
*          serverTLSPort  - server TLS (SSL) port (or 0 to disable)
*          caFileName     - file with TLS CA or NULL
*          certFileName   - file with TLS cerificate or NULL
*          keyFileName    - file with TLS key or NULL
*          serverPassword - server authenfication password
*          jobDirectory   - server job directory
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Server_run(uint           serverPort,
                  uint           serverTLSPort,
                  const char     *caFileName,
                  const char     *certFileName,
                  const char     *keyFileName,
                  const Password *serverPassword,
                  const char     *jobDirectory
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
