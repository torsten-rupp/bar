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
#include <stdarg.h>
#include <semaphore.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "threads.h"
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

// priorities
#define DATABASE_PRIORITY_IMMEDIATE 3
#define DATABASE_PRIORITY_HIGH      2
#define DATABASE_PRIORITY_MEDIUM    1
#define DATABASE_PRIORITY_LOW       0

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
  DATABASE_ORDERING_NONE,
  DATABASE_ORDERING_ASCENDING,
  DATABASE_ORDERING_DESCENDING
} DatabaseOrdering;

/***************************** Datatypes *******************************/

// database handle
typedef struct
{
  uint          priority;                   // access priority
  Semaphore     lock;                       // lock (Note: do not use sqlite mutex, because of debug facilities in semaphore.c)
  sqlite3       *handle;                    // SQlite3 handle
  long          timeout;                    // timeout [ms]
  sem_t         wakeUp;                     // unlock wake-up
  #ifndef NDEBUG
    char fileName[256];
    struct
    {
      ThreadId   threadId;                  // thread who aquired lock
      const char *fileName;
      uint       lineNb;
      char       text[8*1024];
      uint64     t0,t1;                     // lock start/end timestamp [s]
    } locked;
    struct
    {
      ThreadId   threadId;                  // thread who started transaction
      const char *fileName;
      uint       lineNb;
      void const *stackTrace[16];
      int        stackTraceSize;
    } transaction;
  #endif /* not NDEBUG */
} DatabaseHandle;

// database query handle
typedef struct
{
  DatabaseHandle *databaseHandle;
  sqlite3_stmt   *statementHandle;
  #ifndef NDEBUG
    String sqlString;
    uint64 t0,t1;
    uint64 dt;
  #endif /* not NDEBUG */
} DatabaseQueryHandle;

//
/***********************************************************************\
* Name   : DatabaseRowFunction
* Purpose: execute row callback function
* Input  : count    - number of columns
*          names    - column names
*          values   - column values
*          userData - user data
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

typedef Errors(*DatabaseRowFunction)(uint count, const char* names[], const char* values[], void *userData);

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
    int64  id;      // primary key
    String i;       // integer, date/time
    String d;       // double
    String text;    // text
    struct
    {
      void  *data;
      ulong length;
    }      blob;
  } value;
  bool          usedFlag;
} DatabaseColumnNode;

typedef struct
{
  LIST_HEADER(DatabaseColumnNode);
} DatabaseColumnList;

/***********************************************************************\
* Name   : DatabaseCopyTableFunction
* Purpose: execute copy table row callback function
* Input  : fromColumnList - from column list
*          toColumnList   - to column list
*          userData       - user data
* Output : -
* Return : TRUE iff pause
* Notes  : -
\***********************************************************************/

typedef Errors(*DatabaseCopyTableFunction)(const DatabaseColumnList *fromColumnList,
                                           const DatabaseColumnList *toColumnList,
                                           void                     *userData
                                          );

/***********************************************************************\
* Name   : DatabasePauseCallbackFunction
* Purpose: call back to check for pausing
* Input  : userData - user data
* Output : -
* Return : TRUE iff pause
* Notes  : -
\***********************************************************************/

typedef bool(*DatabasePauseCallbackFunction)(void *userData);

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#define DATABASE_TRANSFER_OPERATION_COPY(fromName,toName,type) DATABASE_TRANSFER_OPERATION_COPY,fromName,toName,type
#define DATABASE_TRANSFER_OPERATION_SET(toName,type,value)     DATABASE_TRANSFER_OPERATION_SET, toName,  value, type
#define DATABASE_TRANSFER_OPERATION_END()                      DATABASE_TRANSFER_OPERATION_NONE,NULL,    0,     0

#ifndef NDEBUG
  #define Database_open(...)             __Database_open(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_close(...)            __Database_close(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_lock(...)             __Database_lock(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_unlock(...)           __Database_unlock(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_beginTransaction(...) __Database_beginTransaction(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_prepare(...)          __Database_prepare(__FILE__,__LINE__, ## __VA_ARGS__)
  #define Database_finalize(...)         __Database_finalize(__FILE__,__LINE__, ## __VA_ARGS__)
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Database_initAll
* Purpose: init database
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_initAll(void);

/***********************************************************************\
* Name   : Database_doneAll
* Purpose: done database
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_doneAll(void);

/***********************************************************************\
* Name   : Database_open
* Purpose: open database
* Input  : databaseHandle   - database handle variable
*          fileName         - file name or NULL for "in memory"
*          databaseOpenMode - open mode; see DatabaseOpenModes
*          priority         - priority (0=highest)
*          timeout          - timeout [ms]
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Database_open(DatabaseHandle    *databaseHandle,
                       const char        *fileName,
                       DatabaseOpenModes databaseOpenMode,
                       uint              priority,
                       long              timeout
                      );
#else /* not NDEBUG */
  Errors __Database_open(const char        *__fileName__,
                         uint              __lineNb__,
                         DatabaseHandle    *databaseHandle,
                         const char        *fileName,
                         DatabaseOpenModes databaseOpenMode,
                         uint              priority,
                         long              timeout
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

//bool Database_isHigherRequestPending(uint priority);

/***********************************************************************\
* Name   : Database_interrupt
* Purpose: interrupt currently running database command
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_interrupt(DatabaseHandle *databaseHandle);

//TODO: remove
#if 0
/***********************************************************************\
* Name   : Database_request
* Purpose: request long-run database access
* Input  : databaseHandle - database handle
*          timeout        - timeout request long-run [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Database_request(DatabaseHandle *databaseHandle, ulong timeout);

/***********************************************************************\
* Name   : Database_release
* Purpose: release long-run database access
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Database_release(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_yield
* Purpose: yield long-run database access if access with higher priority
*          is pending
* Input  : databaseHandle - database handle
* Output : yieldStart     - yield start call-back code (can be NULL)
*          userDataStart  - yield start user data
*          yieldEnd       - yield end call-back code (can be NULL)
*          userDataEnd    - yield end user data
* Return : -
* Notes  : -
\***********************************************************************/

void Database_yield(DatabaseHandle *databaseHandle,
                    void           (*yieldStart)(void*),
                    void           *userDataStart,
                    void           (*yieldEnd)(void*),
                    void           *userDataEnd
                   );
#endif

/***********************************************************************\
* Name   : Database_lock
* Purpose: lock database exclusive for this handle
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Database_lock(DatabaseHandle *databaseHandle);
#else /* not NDEBUG */
  void __Database_lock(const char   *__fileName__,
                       uint         __lineNb__,
                       DatabaseHandle *databaseHandle
                      );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_unlock
* Purpose: unlock database
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  void Database_unlock(DatabaseHandle *databaseHandle);
#else /* not NDEBUG */
  void __Database_unlock(const char   *__fileName__,
                         uint         __lineNb__,
                         DatabaseHandle *databaseHandle
                        );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_isLocked
* Purpose: check if database is locked
* Input  : databaseHandle - database handle
* Output : -
* Return : TRUE iff locked
* Notes  : -
\***********************************************************************/

INLINE bool Database_isLocked(DatabaseHandle *databaseHandle);
#if defined(NDEBUG) || defined(__DATABASE_IMPLEMENATION__)
INLINE bool Database_isLocked(DatabaseHandle *databaseHandle)
{
  return Semaphore_isLocked(&databaseHandle->lock);
}
#endif /* NDEBUG || __DATABASE_IMPLEMENATION__ */

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
* Name   : Database_compare
* Purpose: compare database structure
* Input  : databaseHandleReference - reference database handle
*          databaseHandle          - database handle 1
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_compare(DatabaseHandle *databaseHandleReference,
                        DatabaseHandle *databaseHandle
                       );

/***********************************************************************\
* Name   : Database_copyTable
* Purpose: copy table content
* Input  : fromDatabaseHandle    - from-database handle
*          toDatabaseHandle      - fo-database handle
*          fromTableName         - from-table name
*          toTableName           - to-table name
*          preCopyTableFunction  - pre-copy call-back function
*          preCopyTableUserData  - user data for pre-copy call-back
*          postCopyTableFunction - pre-copy call-back function
*          postCopyTableUserData - user data for post-copy call-back
*          fromAdditional        - additional SQL condition
*          ...                   - optional arguments for additional
*                                  SQL condition
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_copyTable(DatabaseHandle                *fromDatabaseHandle,
                          DatabaseHandle                *toDatabaseHandle,
                          const char                    *fromTableName,
                          const char                    *toTableName,
                          bool                          transactionFlag,
                          DatabaseCopyTableFunction     preCopyTableFunction,
                          void                          *preCopyTableUserData,
                          DatabaseCopyTableFunction     postCopyTableFunction,
                          void                          *postCopyTableUserData,
                          DatabasePauseCallbackFunction pauseCallbackFunction,
                          void                          *pauseCallbackUserData,
                          const char                    *fromAdditional,
                          ...
                         );

/***********************************************************************\
* Name   : Database_getTableColumnList*
* Purpose: get table column list entry
* Input  : columnList   - column list
*          columnName   - column name
*          defaultValue - default value
* Output : -
* Return : value
* Notes  : -
\***********************************************************************/

int Database_getTableColumnListInt(const DatabaseColumnList *columnList, const char *columnName, int defaultValue);
uint Database_getTableColumnListUInt(const DatabaseColumnList *columnList, const char *columnName, uint defaultValue);
int64 Database_getTableColumnListInt64(const DatabaseColumnList *columnList, const char *columnName, int64 defaultValue);
uint64 Database_getTableColumnListUInt64(const DatabaseColumnList *columnList, const char *columnName, uint64 defaultValue);
double Database_getTableColumnListDouble(const DatabaseColumnList *columnList, const char *columnName, double defaultValue);
uint64 Database_getTableColumnListDateTime(const DatabaseColumnList *columnList, const char *columnName, uint64 defaultValue);
String Database_getTableColumnList(const DatabaseColumnList *columnList, const char *columnName, String value, const char *defaultValue);
const char *Database_getTableColumnListCString(const DatabaseColumnList *columnList, const char *columnName, const char *defaultValue);
void Database_getTableColumnListBlob(const DatabaseColumnList *columnList, const char *columnName, void *data, uint length);

/***********************************************************************\
* Name   : Database_setTableColumnList*
* Purpose: set table column list entry
* Input  : columnList - column list
*          columnName - column name
*          value      - value
* Output : -
* Return : TRUE iff set
* Notes  : -
\***********************************************************************/

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
* Name   : Database_beginTransaction
* Purpose: begin transaction
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

#ifdef NDEBUG
  Errors Database_beginTransaction(DatabaseHandle *databaseHandle);
#else /* not NDEBUG */
  Errors __Database_beginTransaction(const char     *__fileName__,
                                     uint           __lineNb__,
                                     DatabaseHandle *databaseHandle
                                    );
#endif /* NDEBUG */

/***********************************************************************\
* Name   : Database_endTransaction
* Purpose: end transaction (commit)
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_endTransaction(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_rollbackTransaction
* Purpose: rollback transcation (discard)
* Input  : databaseHandle - database handle
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_rollbackTransaction(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Database_execute
* Purpose: execute SQL statement
* Input  : databaseHandle - database handle
*          databaseRowFunction - callback function for row data (can be
*                                NULL)
*          databaseRowUserData - user data for callback function
*          changedRowCount     - number of changd rows (can be NULL)
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
                        ulong               *changedRowCount,
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
* Name   : Database_getId, Database_vgetId
* Purpose: get database id of value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : value - database id or DATABASE_ID_NONE
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getId(DatabaseHandle *databaseHandle,
                      DatabaseId     *value,
                      const char     *tableName,
                      const char     *additional,
                      ...
                     );
Errors Database_vgetId(DatabaseHandle *databaseHandle,
                       DatabaseId     *value,
                       const char     *tableName,
                       const char     *additional,
                       va_list        arguments
                      );

/***********************************************************************\
* Name   : Database_getInteger64, Database_vgetInteger64
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
Errors Database_vgetInteger64(DatabaseHandle *databaseHandle,
                              int64          *value,
                              const char     *tableName,
                              const char     *columnName,
                              const char     *additional,
                              va_list        arguments
                             );

/***********************************************************************\
* Name   : Database_setInteger64, Database_vsetInteger64
* Purpose: insert or update int64 value in database table
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
Errors Database_vsetInteger64(DatabaseHandle *databaseHandle,
                              int64          value,
                              const char     *tableName,
                              const char     *columnName,
                              const char     *additional,
                              va_list        arguments
                             );

/***********************************************************************\
* Name   : Database_getDouble, Database_vgetDouble
* Purpose: get int64 value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : value - double value
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_getDouble(DatabaseHandle *databaseHandle,
                          double         *value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         );
Errors Database_vgetDouble(DatabaseHandle *databaseHandle,
                           double         *value,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
                          );

/***********************************************************************\
* Name   : Database_setDouble, Database_vsetDouble
* Purpose: insert or update double value in database table
* Input  : databaseHandle - database handle
*          value          - double value
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Database_setDouble(DatabaseHandle *databaseHandle,
                          double         value,
                          const char     *tableName,
                          const char     *columnName,
                          const char     *additional,
                          ...
                         );
Errors Database_vsetDouble(DatabaseHandle *databaseHandle,
                           double         value,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
                          );

/***********************************************************************\
* Name   : Database_getString, Database_vgetString
* Purpose: get string value from database table
* Input  : databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          additional     - additional string (e. g. WHERE...)
*                           special functions:
*                             REGEXP(pattern,case-flag,text)
* Output : string - string value
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
Errors Database_vgetString(DatabaseHandle *databaseHandle,
                           String         string,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
                          );

/***********************************************************************\
* Name   : Database_setString, Database_vsetString
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
Errors Database_vsetString(DatabaseHandle *databaseHandle,
                           const String   string,
                           const char     *tableName,
                           const char     *columnName,
                           const char     *additional,
                           va_list        arguments
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
