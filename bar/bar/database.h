/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/database.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: database functions (SQLite3)
* Systems: all
*
\***********************************************************************/

#ifndef __DATABASE__
#define __DATABASE__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "errors.h"

#include "sqlite3.h"  

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct
{
  sqlite3 *handle;
} DatabaseHandle;

/* execute callback function */
typedef bool(*DatabaseFunction)(void *userData, int count, const char* names[], const char* vales[]);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Database_open
* Purpose: open database
* Input  : databaseHandle - database handle variable
*          fileName       - file name
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_open(DatabaseHandle *databaseHandle,
                     const char     *fileName
                    );

/***********************************************************************\
* Name   : Database_close
* Purpose: close database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_close(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_insert(DatabaseHandle *databaseHandle
                      );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_delete(DatabaseHandle *databaseHandle
                      );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_select(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_execute
* Purpose: execute SQL statement
* Input  : databaseHandle - database handle
*          databaseFunction - callback function for row data
*          databaseUserData - user data for callback function
*          command          - SQL command string with %[l]d, %S, %s
*          ...              - optional arguments for SQL command string
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_execute(DatabaseHandle   *databaseHandle,
                        DatabaseFunction databaseFunction,
                        void             *databaseUserData,
                        const char       *command,
                        ...
                       );

/***********************************************************************\
* Name   : Database_getLastRowId
* Purpose: get row id of last insert command
* Input  : databaseHandle - database handle
* Output : -
* Return : row id
* Notes  : -
\***********************************************************************/

uint64 Database_getLastRowId(DatabaseHandle *databaseHandle);

#ifdef __cplusplus
  }
#endif

#endif /* __DATABASE__ */

/* end of file */
