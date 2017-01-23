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

#include "bar_global.h"
#include "passwords.h"

#include "slave.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

#define SERVER_PROTOCOL_VERSION_MAJOR 5
#define SERVER_PROTOCOL_VERSION_MINOR 0

/***************************** Datatypes *******************************/

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
* Name   : Server_runtypedef struct ResultNode
{
  LIST_NODE_HEADER(struct ResultNode);

  uint   commandId;
  Errors error;
  bool   completedFlag;
  String data;
} ResultNode;

typedef struct
{
  LIST_HEADER(ResultNode);
} ResultList;

* Purpose: run network server
* Input  : mode                  - server mode; see SERVER_MODE_...
*          port                  - server port (or 0 to disable)
*          tlsPort               - server TLS (SSL) port (or 0 to disable)
*          ca                    - CA data or NULL
*          cert                  - TLS cerificate or NULL
*          key                   - TLS key or NULL
*          password              - server authenfication password
*          maxConnections        - max. number of connections or 0
*          jobsDirectory         - jobs directory
*          defaultJobOptions     - default job options
*          indexDatabaseFileName - index database file name or NULL
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Server_run(ServerModes       mode,
                  uint              port,
                  uint              tlsPort,
                  const Certificate *ca,
                  const Certificate *cert,
                  const Key         *key,
                  const Password    *password,
                  uint              maxConnections,
                  const char        *jobsDirectory,
                  const char        *indexDatabaseFileName,
                  const JobOptions  *defaultJobOptions
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
