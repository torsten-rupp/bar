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

// database types
typedef enum
{
  DATABASE_TYPE_NONE,

  DATABASE_TYPE_PRIMARY_KEY,
  DATABASE_TYPE_FOREIGN_KEY,

  DATABASE_TYPE_INT64,
  DATABASE_TYPE_DOUBLE,
  DATABASE_TYPE_DATETIME,
  DATABASE_TYPE_TEXT,
  DATABASE_TYPE_BLOB,

  DATABASE_TYPE_UNKNOWN
} DatabaseTypes;

// special database ids
#define DATABASE_ID_NONE  0LL
#define DATABASE_ID_ANY  -1LL

// ordering mode
typedef enum
{
  DATABASE_ORDERING_ASCENDING,
  DATABASE_ORDERING_DESCENDING
} DatabaseOrdering;

/***************************** Datatypes *******************************/

// database handle
typedef struct
{
  sqlite3    *handle;
  #ifndef NDEBUG
    char fileName[256];
    struct
    {
      char   text[8*1024];
      uint   lineNb;
      uint64 t0,t1;
    } locked;
  #endif /* not NDEBUG */
} DatabaseHandle;

// database query handle
typedef struct
{
  DatabaseHandle *databaseHandle;
  sqlite3_stmt   *handle;
  #ifndef NDEBUG
    String sqlString;
    uint64 t0,t1;
    uint64 dt;
  #endif /* not NDEBUG */
} DatabaseQueryHandle;

// execute row callback function
typedef bool(*DatabaseRowFunction)(void *userData, uint count, const char* names[], const char* vales[]);

// database id
typedef int64 DatabaseId;

// table column definition list
typedef struct DatabaseColumnNode
{
  LIST_NODE_HEADER(struct DatabaseColumnNode);

  char          *name;
  DatabaseTypes type;
  union
  {
    // Note: data values are kept as strings to avoid conversion problems e.g. date/time -> integer
    int64  id;
    String i;
    String d;
    String text;
    struct
    {
      const void *data;
      ulong      length;
    }      blob;
  } value;
} DatabaseColumnNode;

typedef struct
{
  LIST_HEADER(DatabaseColumnNode);
} DatabaseColumnList;

// execute copy table row callback function
typedef Errors(*DatabaseCopyTableFunction)(const DatabaseColumnList *fromColumnList, const DatabaseColumnList *toColumnList, void *userData);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#define DATABASE_TRANSFER_OPERATION_COPY(fromName,toName,type) DATABASE_TRANSFER_OPERATION_COPY,fromName,toName,type
#define DATABASE_TRANSFER_OPERATION_SET(toName,type,value)     DATABASE_TRANSFER_OPERATION_SET, toName,  value, type
#define DATABASE_TRANSFER_OPERATION_END()                      DATABASE_TRANSFER_OPERATION_NONE,NULL,    0,     0

#ifndef NDEBUG
  #define Database_open(...)     __Database_open(__FILE__,__LINE__,__VA_ARGS__)
  #define Database_close(...)    __Database_close(__FILE__,__LINE__,__VA_ARGS__)
  #define Database_prepare(...)  __Database_prepare(__FILE__,__LINE__,__VA_ARGS__)
  #define Database_finalize(...) __Database_finalize(__FILE__,__LINE__,__VA_ARGS__)
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
*          fileName       - file name or NULL for "in memory"
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
* Purpose: lock database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_lock(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_unlock
* Purpose: unlock database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_unlock(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_setEnabledSync
* Purpose: enable/disable synchronous mode
* Input  : databaseHandle - database handle
*          enabled        - TRUE to enable synchronous mode
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_setEnabledSync(DatabaseHandle *databaseHandle,
                               bool           enabled
                              );

/***********************************************************************\
* Name   : Database_setEnabledForeignKeys
* Purpose: enable/disable foreign key checks
* Input  : databaseHandle - database handle
*          enabled        - TRUE to enable foreign key checks
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Database_setEnabledForeignKeys(DatabaseHandle *databaseHandle,
                                      bool           enabled
                                     );

/***********************************************************************\
* Name   : Database_copyTable
* Purpose: copy table content
* Input  : fromDatabaseHandle - from-database handle
*          toDatabaseHandle   - fo-database handle
*          tableName          - table name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_copyTable(DatabaseHandle            *fromDatabaseHandle,
                          DatabaseHandle            *toDatabaseHandle,
                          const char                *tableName,
                          DatabaseCopyTableFunction preCopyTableFunction,
                          void                      *preCopyTableUserData,
                          DatabaseCopyTableFunction postCopyTableFunction,
                          void                      *postCopyTableUserData,
                          const char                *fromAdditional,
                          ...
                         );

int64 Database_getTableColumnListInt64(const DatabaseColumnList *columnList, const char *columnName, int64 defaultValue);
double Database_getTableColumnListDouble(const DatabaseColumnList *columnList, const char *columnName, double defaultValue);
uint64 Database_getTableColumnListDateTime(const DatabaseColumnList *columnList, const char *columnName, uint64 defaultValue);
String Database_getTableColumnList(const DatabaseColumnList *columnList, const char *columnName, String value, const char *defaultValue);
const char *Database_getTableColumnListCString(const DatabaseColumnList *columnList, const char *columnName, const char *defaultValue);
void Database_getTableColumnListBlob(const DatabaseColumnList *columnList, const char *columnName, void *data, uint length);

bool Database_setTableColumnListInt64(const DatabaseColumnList *columnList, const char *columnName, int64 value);
bool Database_setTableColumnListDouble(const DatabaseColumnList *columnList, const char *columnName, double value);
bool Database_setTableColumnListDateTime(const DatabaseColumnList *columnList, const char *columnName, uint64 value);
bool Database_setTableColumnList(const DatabaseColumnList *columnList, const char *columnName, ConstString value);
bool Database_setTableColumnListCString(const DatabaseColumnList *columnList, const char *columnName, const char *value);
bool Database_setTableColumnListBlob(const DatabaseColumnList *columnList, const char *columnName, const void *data, uint length);

/***********************************************************************\
* Name   : Database_addColumn
* Purpose: add column to table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          columnType     - column data type
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_addColumn(DatabaseHandle *databaseHandle,
                          const char     *tableName,
                          const char     *columnName,
                          DatabaseTypes  columnType
                         );

/***********************************************************************\
* Name   : Database_addColumn
* Purpose: remove column from table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_removeColumn(DatabaseHandle *databaseHandle,
                             const char     *tableName,
                             const char     *columnName
                            );

/***********************************************************************\
* Name   : Database_execute
* Purpose: execute SQL statement
* Input  : databaseHandle - database handle
*          databaseRowFunction - callback function for row data
*          databaseRowUserData - user data for callback function
*          command             - SQL command string with %[l]d, %[']S,
*                                %[']s
*          ...                 - optional arguments for SQL command
*                                string
*                                special functions:
*                                  REGEXP(pattern,case-flag,text)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_execute(DatabaseHandle      *databaseHandle,
                        DatabaseRowFunction databaseRowFunction,
                        void                *databaseRowUserData,
                        const char          *command,
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

#ifdef NDEBUG
  Errors Database_prepare(DatabaseQueryHandle *databaseQueryHandle,
                          DatabaseHandle      *databaseHandle,
                          const char          *command,
                          ...
                         );
#else /* not NDEBUG */
  Errors __Database_prepare(const char          *__fileName__,
                            uint                __lineNb__,
                            DatabaseQueryHandle *databaseQueryHandle,
                            DatabaseHandle      *databaseHandle,
                            const char          *command,
                            ...
                           );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_getNextRow
* Purpose: get next row from query result
* Input  : databaseQueryHandle - database query handle
*          format - format string
* Output : ... - variables
* Return : TRUE if row read, FALSE if not more rows
* Notes  : Support format types:
*            %b              - bool
*            %[<n>][(ll|l)\d - int
*            %[<n>][(ll|l)\u - unsigned int
*            %[<n>][l\f      - float/double
*            %c              - char
*            %[<n>]s         - char*
*            %S              - string
*            %p              - pointer
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

#ifdef NDEBUG
  void Database_finalize(DatabaseQueryHandle *databaseQueryHandle);
#else /* not NDEBUG */
  void __Database_finalize(const char        *__fileName__,
                           uint              __lineNb__,
                           DatabaseQueryHandle *databaseQueryHandle
                          );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_exists
* Purpose: check if value exists database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : -
* Return : TRUE iff value exists
* Notes  : -
\***********************************************************************/

bool Database_exists(DatabaseHandle *databaseHandle,
                     const char     *tableName,
                     const char     *columnName,
                     const char     *additional,
                     ...
                    );

/***********************************************************************\
* Name   : Database_getInteger64
* Purpose: get int64 value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : value - int64 value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getInteger64(DatabaseHandle *databaseHandle,
                             int64          *value,
                             const char     *tableName,
                             const char     *columnName,
                             const char     *additional,
                             ...
                            );

/***********************************************************************\
* Name   : Database_setInteger64
* Purpose: isnert or update int64 value in database table
* Input  : databaseHandle - database handle
*          value          - int64 value
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
                             int64          value,
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
* Output : value - string value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getString(DatabaseHandle *databaseHandle,
                          String         value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         );

/***********************************************************************\
* Name   : Database_setString
* Purpose: insert or update string value in database table
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

#ifndef NDEBUG

/***********************************************************************\
* Name   : Database_debugEnable
* Purpose: enable/disable debug output
* Input  : enabled - TRUE to enable debug output
* Output : -
* Return : -
* Notes  : For debugging only!
\***********************************************************************/

void Database_debugEnable(bool enabled);

/***********************************************************************\
* Name   : Database_debugPrintQueryInfo
* Purpose: print query info
* Input  : databaseQueryHandle - database query handle
* Output : -
* Return : -
* Notes  : For debugging only!
\***********************************************************************/

void Database_debugPrintQueryInfo(DatabaseQueryHandle *databaseQueryHandle);

#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __DATABASE__ */

/* end of file */
