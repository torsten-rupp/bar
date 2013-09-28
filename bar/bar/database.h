/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: database functions (SQLite3)
* Systems: all
*
\***********************************************************************/

#ifndef __DATABASE__
#define __DATABASE__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "semaphores.h"
#include "errors.h"

#include "sqlite3.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

// database open modes
typedef enum
{
  DATABASE_OPENMODE_CREATE,
  DATABASE_OPENMODE_READ,
  DATABASE_OPENMODE_READWRITE,
} DatabaseOpenModes;

// no id
#define DATABASE_ID_NONE -1LL

/***************************** Datatypes *******************************/

// database handle
typedef struct
{
  Semaphore lock;
  sqlite3   *handle;
} DatabaseHandle;

// database query handle
typedef struct
{
  DatabaseHandle *databaseHandle;
  sqlite3_stmt   *handle;
} DatabaseQueryHandle;

// execute callback function
typedef bool(*DatabaseFunction)(void *userData, uint count, const char* names[], const char* vales[]);

typedef int64 DatabaseId;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : DATABASE_LOCKED_DO
* Purpose: execute block with database locked
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : usage:
*            DATABASE_LOCKED_DO(databaseHandle)
*            {
*              ...
*            }
\***********************************************************************/

#define DATABASE_LOCKED_DO(databaseHandle) \
  for (Database_lock(databaseHandle); Database_isLocked(databaseHandle); Database_unlock(databaseHandle))

#ifndef NDEBUG
  #define Database_open(...)  __Database_open(__FILE__,__LINE__,__VA_ARGS__)
  #define Database_close(...) __Database_close(__FILE__,__LINE__,__VA_ARGS__)
#endif /* not NDEBUG */

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

#ifdef NDEBUG
  Errors Database_open(DatabaseHandle    *databaseHandle,
                       const char        *fileName,
                       DatabaseOpenModes databaseOpenMode
                      );
#else /* not NDEBUG */
  Errors __Database_open(const char        *__fileName__,
                         uint              __lineNb__,
                         DatabaseHandle    *databaseHandle,
                         const char        *fileName,
                         DatabaseOpenModes databaseOpenMode
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_close
* Purpose: close database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Database_close(DatabaseHandle *databaseHandle);
#else /* not NDEBUG */
  void __Database_close(const char   *__fileName__,
                        uint         __lineNb__,
                        DatabaseHandle *databaseHandle
                       );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_lock
* Purpose: lock database access
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_lock(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_unlock
* Purpose: unlock database access
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_unlock(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_isLocked
* Purpose: check if database access is locked
* Input  : databaseHandle - database handle
* Output : -
* Return : TRUE if database access is locked, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Database_isLocked(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_execute
* Purpose: execute SQL statement
* Input  : databaseHandle - database handle
*          databaseFunction - callback function for row data
*          databaseUserData - user data for callback function
*          command          - SQL command string with %[l]d, %[']S,
*                             %[']s
*          ...              - optional arguments for SQL command string
*                             special functions:
*                               REGEXP(pattern,case-flag,text)
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
* Name   : Database_prepare
* Purpose: prepare database query
* Input  : databaseHandle - database handle
*          command        - SQL command string with %[l]d, %[']S, %[']s
*          ...            - optional arguments for SQL command string
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : databaseQueryHandle - initialized database query handle
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_prepare(DatabaseQueryHandle *databaseQueryHandle,
                        DatabaseHandle      *databaseHandle,
                        const char          *command,
                        ...
                       );

/***********************************************************************\
* Name   : Database_getNextRow
* Purpose: get next row from query result
* Input  : databaseQueryHandle - database query handle
* Output : format - format string with %[l]d, %[l]f, %s, %s, %S
* Return : TRUE if row read, FALSE if not more rows
* Notes  : -
\***********************************************************************/

bool Database_getNextRow(DatabaseQueryHandle *databaseQueryHandle,
                         const char          *format,
                         ...
                        );

/***********************************************************************\
* Name   : Database_finalize
* Purpose: done database query
* Input  : databaseQueryHandle - database query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_finalize(DatabaseQueryHandle *databaseQueryHandle);

/***********************************************************************\
* Name   : Database_getInteger64
* Purpose: get int64 value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : l - int64 value or DATABASE_ID_NONE if not found
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getInteger64(DatabaseHandle *databaseHandle,
                             int64          *l,
                             const char     *tableName,
                             const char     *columnName,
                             const char     *additional,
                             ...
                            );

/***********************************************************************\
* Name   : Database_getString
* Purpose: get string value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : string - string value or empty if not found
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getString(DatabaseHandle *databaseHandle,
                          String         string,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         );

/***********************************************************************\
* Name   : Database_setInteger64
* Purpose: get int64 value from database table
* Input  : databaseHandle - database handle
*          l              - int64 value
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setInteger64(DatabaseHandle *databaseHandle,
                             int64          l,
                             const char     *tableName,
                             const char     *columnName,
                             const char     *additional,
                             ...
                            );

/***********************************************************************\
* Name   : Database_setString
* Purpose: set string value from database table
* Input  : databaseHandle - database handle
*          string         - string value
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setString(DatabaseHandle *databaseHandle,
                          const String   string,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
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

int64 Database_getLastRowId(DatabaseHandle *databaseHandle);

#ifdef __cplusplus
  }
#endif

#endif /* __DATABASE__ */

/* end of file */
