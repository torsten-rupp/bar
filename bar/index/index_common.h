/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index common functions
* Systems: all
*
\***********************************************************************/

#ifndef __INDEX_COMMON__
#define __INDEX_COMMON__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#include "common/global.h"
#include "common/dictionaries.h"
#include "common/threads.h"
#include "common/strings.h"
#include "common/database.h"
#include "common/arrays.h"
#include "common/files.h"
#include "common/filesystems.h"
#include "common/misc.h"
#include "errors.h"

#include "bar_common.h"
#include "storage.h"
#include "server_io.h"
#include "index_definition.h"
#include "archive.h"

#include "index/index.h"

/****************** Conditional compilation switches *******************/
// switch off for debugging only!
#define INDEX_INTIIAL_CLEANUP
#define INDEX_IMPORT_OLD_DATABASE
#define INDEX_SUPPORT_DELETE

#ifndef INDEX_IMPORT_OLD_DATABASE
  #warning Index import old databases disabled!
#endif
#ifndef INDEX_INTIIAL_CLEANUP
  #warning Index initial cleanup disabled!
#endif
#ifndef INDEX_SUPPORT_DELETE
  #warning Index delete storages disabled!
#endif

/***************************** Constants *******************************/
#define DEFAULT_DATABASE_NAME "bar"

extern const char *DATABASE_SAVE_EXTENSIONS[];
extern const char *DATABASE_SAVE_PATTERNS[];

// index open mask
#define INDEX_OPEN_MASK_MODE  0x0000000F
#define INDEX_OPEN_MASK_FLAGS 0xFFFF0000

//TODO: 5
#define DATABASE_TIMEOUT (5*120L*MS_PER_SECOND)

// TODO:
#define MAX_SQL_COMMAND_LENGTH (2*4096)

#if 0
extern const struct
{
  const char  *name;
  IndexStates indexState;
} INDEX_STATES[];

extern const struct
{
  const char *name;
  IndexModes indexMode;
} INDEX_MODES[];

extern const struct
{
  const char *name;
  IndexTypes indexType;
} INDEX_TYPES[];

extern const struct
{
  const char           *name;
  IndexEntitySortModes sortMode;
} INDEX_ENTITY_SORT_MODES[];

extern const struct
{
  const char            *name;
  IndexStorageSortModes sortMode;
} INDEX_STORAGE_SORT_MODES[];

extern const struct
{
  const char          *name;
  IndexEntrySortModes sortMode;
} INDEX_ENTRY_SORT_MODES[];
#endif

extern const char *INDEX_ENTITY_SORT_MODE_COLUMNS[];

extern const char *INDEX_STORAGE_SORT_MODE_COLUMNS[];

extern const char *INDEX_ENTRY_SORT_MODE_COLUMNS[];
extern const char *INDEX_ENTRY_NEWEST_SORT_MODE_COLUMNS[];

// time for index clean-up [s]
#define TIME_INDEX_CLEANUP (4*S_PER_HOUR)

// sleep times [s]
#define SLEEP_TIME_INDEX_CLEANUP_THREAD 120L
#define SLEEP_TIME_PURGE                  2L

// server i/o
#define SERVER_IO_DEBUG_LEVEL 1
#define SERVER_IO_TIMEOUT     (10LL*MS_PER_MINUTE)

// single step purge limit
//  const uint SINGLE_STEP_PURGE_LIMIT = 64;
#define SINGLE_STEP_PURGE_LIMIT 4096

#ifdef INDEX_DEBUG_IMPORT_OLD_DATABASE
  #define IMPORT_INDEX_LOG_FILENAME "import_index.log"
#endif /* INDEX_DEBUG_IMPORT_OLD_DATABASE */

/***************************** Datatypes *******************************/

// index open modes
typedef enum
{
  INDEX_OPEN_MODE_READ,
  INDEX_OPEN_MODE_READ_WRITE,
  INDEX_OPEN_MODE_CREATE
} IndexOpenModes;

// additional index open mode flags
#define INDEX_OPEN_MODE_NO_JOURNAL   (1 << 16)
#define INDEX_OPEN_MODE_KEYS (1 << 17)

// thread info
typedef struct
{
  ThreadId   threadId;
//TODO: remove
//  uint       count;
//  const char *fileName;
//  uint       lineNb;
//  uint64     cycleCounter;
  #ifdef INDEX_DEBUG_LOCK
    ThreadLWPId threadLWPId;
    #if !defined(NDEBUG) && defined(HAVE_BACKTRACE)
      void const *stackTrace[16];
      uint       stackTraceSize;
    #endif /* defined(NDEBUG) && defined(HAVE_BACKTRACE) */
  #endif /* INDEX_DEBUG_LOCK */
} ThreadInfo;

/***************************** Variables *******************************/

extern bool                       indexInitializedFlag;
extern IndexIsMaintenanceTime     indexIsMaintenanceTimeFunction;
extern void                       *indexIsMaintenanceTimeUserData;
extern Semaphore                  indexLock;
extern Array                      indexUsedBy;
extern Thread                     indexThread;    // upgrade/clean-up thread
extern Semaphore                  indexThreadTrigger;
extern IndexHandle                *indexThreadIndexHandle;
extern Semaphore                  indexClearStorageLock;
extern bool                       indexQuitFlag;

/****************************** Macros *********************************/

/***********************************************************************\
* Name   : INDEX_DO
* Purpose: index block-operation
* Input  : indexHandle - index handle
*          block       - code block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define INDEX_DO(indexHandle,block) \
  do \
  { \
    IndexCommon_addIndexInUseThreadInfo(); \
    if (!Thread_isCurrentThread(Thread_getId(&indexThread))) \
    { \
      IndexCommon_indexThreadInterrupt(); \
    } \
    ({ \
      auto void __closure__(void); \
      \
      void __closure__(void)block; __closure__; \
    })(); \
    IndexCommon_removeIndexInUseThreadInfo(); \
  } \
  while (0)

/***********************************************************************\
* Name   : INDEX_DOX
* Purpose: index block-operation
* Input  : indexHandle - index handle
*          block       - code block
* Output : result - result
* Return : -
* Notes  : -
\***********************************************************************/

#define INDEX_DOX(result,indexHandle,block) \
  do \
  { \
    IndexCommon_addIndexInUseThreadInfo(); \
    if (!Thread_isCurrentThread(Thread_getId(&indexThread))) \
    { \
      IndexCommon_indexThreadInterrupt(); \
    } \
    result = ({ \
               auto typeof(result) __closure__(void); \
               \
               typeof(result) __closure__(void)block; __closure__; \
             })(); \
    IndexCommon_removeIndexInUseThreadInfo(); \
  } \
  while (0)


/***********************************************************************\
* Name   : INDEX_INTERRUPTABLE_OPERATION_DOX
* Purpose: index interruptable block-operation
* Input  : error           - error code
*          indexHandle     - index handle
*          transactionFlag - transaction flag
*          block           - code block
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define INDEX_INTERRUPTABLE_OPERATION_DOX(error,indexHandle,transactionFlag,block) \
  do \
  { \
    error = IndexCommon_beginInterruptableOperation(indexHandle,&transactionFlag); \
    if (error == ERROR_NONE) \
    { \
      error = ({ \
                auto typeof(error) __closure__(void); \
                \
                typeof(error) __closure__(void)block; __closure__; \
              })(); \
      (void)IndexCommon_endInterruptableOperation(indexHandle,&transactionFlag); \
    } \
  } \
  while (0)

/***********************************************************************\
* Name   : WAIT_NOT_IN_USE
* Purpose: wait until index is unused
* Input  : time - wait delta time [ms]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define WAIT_NOT_IN_USE(time) \
  do \
  { \
    while (   IndexCommon_isIndexInUse() \
           && !indexQuitFlag \
          ) \
    { \
      Misc_udelay(time*US_PER_MS); \
    } \
  } \
  while (0)

/***********************************************************************\
* Name   : WAIT_NOT_IN_USEX
* Purpose: wait until index is unused
* Input  : time      - wait delta time [ms]
*          condition - condition to check
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

#define WAIT_NOT_IN_USEX(time,condition) \
  do \
  { \
    while (   (condition) \
           && IndexCommon_isIndexInUse() \
           && !indexQuitFlag \
          ) \
    { \
      Misc_udelay(time*US_PER_MS); \
    } \
  } \
  while (0)

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : IndexCommon_addIndexInUseThreadInfo
* Purpose: add in use thread info
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void IndexCommon_addIndexInUseThreadInfo(void);
#if defined(NDEBUG) || defined(__INDEX_COMMON_IMPLEMENTATION__)
INLINE void IndexCommon_addIndexInUseThreadInfo(void)
{
  ThreadInfo threadInfo;

  threadInfo.threadId = Thread_getCurrentId();
  #ifdef INDEX_DEBUG_LOCK
    threadInfo.threadLWPId = Thread_getCurrentLWPId();
    #if defined(NDEBUG) && defined(HAVE_BACKTRACE)
      BACKTRACE(threadInfo.stackTrace,threadInfo.stackTraceSize);
    #endif /* defined(NDEBUG) && defined(HAVE_BACKTRACE) */
  #endif /* INDEX_DEBUG_LOCK */

  SEMAPHORE_LOCKED_DO(&indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    Array_append(&indexUsedBy,&threadInfo);
  }
//Index_debugPrintInUseInfo();
}
#endif /* NDEBUG || __INDEX_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : IndexCommon_removeIndexInUseThreadInfo
* Purpose: remove in use thread info
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

INLINE void IndexCommon_removeIndexInUseThreadInfo(void);
#if defined(NDEBUG) || defined(__INDEX_COMMON_IMPLEMENTATION__)
INLINE void IndexCommon_removeIndexInUseThreadInfo(void)
{
  ThreadId threadId;
  long     i;

  threadId = Thread_getCurrentId();

  SEMAPHORE_LOCKED_DO(&indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    i = Array_find(&indexUsedBy,
                   ARRAY_FIND_BACKWARD,
                   NULL,
                   CALLBACK_INLINE(int,(const void *data1, const void *data2, void *userData),
                   {
                     const ThreadInfo *threadInfo = (ThreadInfo*)data1;

                     assert(threadInfo != NULL);
                     assert(data2 == NULL);

                     UNUSED_VARIABLE(data2);
                     UNUSED_VARIABLE(userData);

                     return Thread_equalThreads(threadInfo->threadId,threadId) ? 0 : -1;
                   },NULL)
                  );
    if (i >= 0)
    {
      Array_remove(&indexUsedBy,i);
    }
  }
//Index_debugPrintInUseInfo();
}
#endif /* NDEBUG || __INDEX_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : IndexCommon_isIndexInUse
* Purpose: check if index is in use by some other thread
* Input  : -
* Output : -
* Return : TRUE iff index is in use by some other thread
* Notes  : -
\***********************************************************************/

INLINE bool IndexCommon_isIndexInUse(void);
#if defined(NDEBUG) || defined(__INDEX_COMMON_IMPLEMENTATION__)
INLINE bool IndexCommon_isIndexInUse(void)
{
  ThreadId      threadId;
  bool          indexInUse;
  ArrayIterator arrayIterator;
  ThreadInfo    threadInfo;

  threadId   = Thread_getCurrentId();
  indexInUse = FALSE;
  SEMAPHORE_LOCKED_DO(&indexLock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
  {
    ARRAY_ITERATEX(&indexUsedBy,arrayIterator,threadInfo,!indexInUse)
    {
      if (!Thread_equalThreads(threadInfo.threadId,threadId))
      {
        indexInUse = TRUE;
      }
    }
  }

  return indexInUse;
}
#endif /* NDEBUG || __INDEX_IMPLEMENTATION__ */

const char *IndexCommon_getIndexInUseInfo(void);

/***********************************************************************\
* Name   : IndexCommon_isMaintenanceTime
* Purpose: check if maintenance time
* Input  : dateTime - date/time
* Output : -
* Return : TRUE iff maintenance time
* Notes  : -
\***********************************************************************/

INLINE bool IndexCommon_isMaintenanceTime(uint64 dateTime);
#if defined(NDEBUG) || defined(__INDEX_COMMON_IMPLEMENTATION__)
INLINE bool IndexCommon_isMaintenanceTime(uint64 dateTime)
{
  return    (indexIsMaintenanceTimeFunction == NULL)
         || indexIsMaintenanceTimeFunction(dateTime,indexIsMaintenanceTimeUserData);
}
#endif /* NDEBUG || __INDEX_IMPLEMENTATION__ */

/***********************************************************************\
* Name   : IndexCommon_indexThreadInterrupt
* Purpose: interrupt index thread
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void IndexCommon_indexThreadInterrupt(void);

/***********************************************************************\
* Name   : IndexCommon_getIndexStateSetString
* Purpose: get index state filter string
* Input  : string        - string variable
*          indexStateSet - index state set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

String IndexCommon_getIndexStateSetString(String string, IndexStateSet indexStateSet);

/***********************************************************************\
* Name   : IndexCommon_getIndexModeSetString
* Purpose: get index mode filter string
* Input  : string       - string variable
*          indexModeSet - index mode set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

String IndexCommon_getIndexModeSetString(String string, IndexModeSet indexModeSet);

/***********************************************************************\
* Name   : IndexCommon_getPostgreSQLFTSTokens
* Purpose: get PostgreSQL full-text-seach tokens from text
* Input  : string - token string variable
*          text   - text (can be NULL)
* Output : -
* Return : token string
* Notes  : -
\***********************************************************************/

String IndexCommon_getPostgreSQLFTSTokens(String string, ConstString text);

/***********************************************************************\
* Name   : IndexCommon_getFTSMatchString
* Purpose: get full-text-search filter match string
* Input  : string         - string variable
*          databaseHandle - database handle
*          tableName      - table name
*          columnName     - column name
*          patternText    - pattern text
* Output : -
* Return : string for WHERE filter-statement
* Notes  : -
\***********************************************************************/

String IndexCommon_getFTSMatchString(String         string,
                                     DatabaseHandle *databaseHandle,
                                     const char     *tableName,
                                     const char     *columnName,
                                     ConstString    patternText
                                    );

/***********************************************************************\
* Name   : IndexCommon_appendOrdering
* Purpose: append to SQL ordering string
* Input  : filterString - filter string
*          condition    - append iff true
*          columnName   - column name
*          ordering     - database ordering
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

// TODO: replace by separate parameter in select
void IndexCommon_appendOrdering(String          orderString,
                                bool            condition,
                                const char      *columnName,
                                DatabaseOrdering ordering
                               );

/***********************************************************************\
* Name   : IndexCommon_initIndexQueryHandle
* Purpose: init index query handle
* Input  : indexQueryHandle - index query handle
*          indexHandle - index handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void IndexCommon_initIndexQueryHandle(IndexQueryHandle *indexQueryHandle, IndexHandle *indexHandle);

/***********************************************************************\
* Name   : IndexCommon_doneIndexQueryHandle
* Purpose: done index query handle
* Input  : indexQueryHandle - index query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void IndexCommon_doneIndexQueryHandle(IndexQueryHandle *indexQueryHandle);

/***********************************************************************\
* Name   : IndexCommon_beginInterruptableOperation
* Purpose: begin interruptable operation
* Input  : indexHandle     - index handle
*          transactionFlag - transaction variable
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexCommon_beginInterruptableOperation(IndexHandle *indexHandle, bool *transactionFlag);

/***********************************************************************\
* Name   : IndexCommon_endInterruptableOperation
* Purpose: end interruptable operation
* Input  : indexHandle     - index handle
*          transactionFlag - transaction variable
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexCommon_endInterruptableOperation(IndexHandle *indexHandle, bool *transactionFlag);

/***********************************************************************\
* Name   : IndexCommon_interruptOperation
* Purpose: interrupt operation, temporary close transcation and wait
*          until index is unused
* Input  : indexHandle     - index handle
*          transactionFlag - transaction variable or NULL
*          time            - interruption wait delta time [ms[
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexCommon_interruptOperation(IndexHandle *indexHandle, bool *transactionFlag, ulong time);

/***********************************************************************\
* Name   : purge
* Purpose: delete with delay/check if index-usage
* Input  : indexHandle    - index handle
*          doneFlag       - done flag variable (can be NULL)
*          deletedCounter - deleted entries count variable (can be NULL)
*          tableName      - table name
*          filter         - filter string
*          ...            - optional arguments for filter
* Output : doneFlag       - set to FALSE if delete not completely done
*          deletedCounter - updated deleted entries count
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexCommon_delete(IndexHandle          *indexHandle,
                          bool                 *doneFlag,
                          ulong                *deletedCounter,
                          const char           *tableName,
                          const char           *filter,
                          const DatabaseFilter filters[],
                          uint                 filterCount
                         );

/***********************************************************************\
* Name   : IndexCommon_deleteByIds
* Purpose: delete by ids
* Input  : indexHandle       - index handle
*          changedRowCount   - deleted entries count variable (can be NULL)
*          tableName         - table name,
*          columnName        - column name,
*          ids               - ids array
*          idCount           - length of ids array
* Output : doneFlag       - set to FALSE if delete not completely done
*          deletedCounter - updated deleted entries count
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors IndexCommon_deleteByIds(IndexHandle      *indexHandle,
                               bool             *doneFlag,
                               ulong            *deletedCounter,
                               const char       *tableName,
                               const char       *columnName,
                               const DatabaseId ids[],
                               ulong            idCount
                              );

// TODO: comment
void IndexCommon_verify(IndexHandle *indexHandle,
                        const char  *tableName,
                        const char  *columnName,
                        int64       value,
                        const char  *condition,
                        ...
                       );

#ifdef __cplusplus
  }
#endif

#endif /* __INDEX_COMMON__ */

/* end of file */
