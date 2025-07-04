/***********************************************************************\
*
* Contents: continuous functions
* Systems: all
*
\***********************************************************************/

#ifndef __CONTINUOUS__
#define __CONTINUOUS__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"
#include "common/strings.h"
#include "common/database.h"
#include "errors.h"

#include "entrylists.h"
#include "common/patternlists.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

#ifndef NDEBUG
#endif /* not NDEBUG */

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Continuous_initAll
* Purpose: initialize continuous functions
* Input  : -
* Output : -
* Return : ERROR_NONE or errorcode
* Notes  : -
\***********************************************************************/

Errors Continuous_initAll(void);

/***********************************************************************\
* Name   : Continuous_doneAll
* Purpose: deinitialize continuous functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Continuous_doneAll(void);

/***********************************************************************\
* Name   : Continuous_isAvailable
* Purpose: check if continuous is available
* Input  : -
* Output : -
* Return : TRUE iff continuous is available
* Notes  : -
\***********************************************************************/

bool Continuous_isAvailable(void);

/***********************************************************************\
* Name   : Continuous_init
* Purpose: init continuous
* Input  : databaseFileName - database file name or NULL for in-memory
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

Errors Continuous_init(const char *databaseFileName);

/***********************************************************************\
* Name   : Continuous_done
* Purpose: done continuous
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Continuous_done(void);

/***********************************************************************\
* Name   : Continuous_initNotify
* Purpose: init notifies for job
* Input  : name              - job name
*          jobUUID           - job UUID
*          scheduleUUID      - schedule UUID (can be NULL)
*          date              - schedule date
*          weekDaySet        - schedule weekday set
*          beginTime,endTime - begin/end time
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_initNotify(ConstString     name,
                             const char      *jobUUID,
                             const char      *scheduleUUID,
                             ScheduleDate    date,
                             WeekDaySet      weekDaySet,
                             ScheduleTime    beginTime,
                             ScheduleTime    endTime,
                             const EntryList *entryList
                            );

/***********************************************************************\
* Name   : Continuous_doneNotify
* Purpose: done notifies for job
* Input  : jobUUID      - job UUID
*          scheduleUUID - schedule UUID (can be NULL)
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_doneNotify(ConstString name,
                             const char  *jobUUID,
                             const char  *scheduleUUID
                            );

/***********************************************************************\
* Name   : Continuous_open
* Purpose: open continuous database
* Input  : databaseHandle - database handle variable
* Output : databaseHandle - database handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_open(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Continuous_close
* Purpose: done notify for job
* Input  : databaseHandle - database handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Continuous_close(DatabaseHandle *databaseHandle);

/***********************************************************************\
* Name   : Continuous_addEntry
* Purpose: add continuous entry
* Input  : databaseHandle    - database handle
*          indexHandle       - index handle
*          jobUUID           - job UUID
*          scheduleUUID      - schedule UUID
*          beginTime,endTime - begin/end time (can be NULL)
*          name              - name
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_addEntry(DatabaseHandle *databaseHandle,
                           const char     *jobUUID,
                           const char     *scheduleUUID,
                           ScheduleTime   *beginTime,
                           ScheduleTime   *endTime,
                           ConstString    name
                          );

/***********************************************************************\
* Name   : Continuous_discardEntries
* Purpose: discard all entries
* Input  : databaseHandle - database handle
*          jobUUID        - job UUID
*          scheduleUUID   - schedule UUID
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Continuous_discardEntries(DatabaseHandle *databaseHandle,
                               const char     *jobUUID,
                               const char     *scheduleUUID
                              );

/***********************************************************************\
* Name   : Continuous_isEntryAvailable
* Purpose: check if continuous entries available
* Input  : databaseHandle - database handle
*          jobUUID        - job UUID
*          scheduleUUID   - schedule UUID
* Output : -
* Return : TRUE if continuous entries available
* Notes  : -
\***********************************************************************/

bool Continuous_isEntryAvailable(DatabaseHandle *databaseHandle,
                                 const char     *jobUUID,
                                 const char     *scheduleUUID
                                );

/***********************************************************************\
* Name   : Continuous_initList
* Purpose: list continuous entries
* Input  : databaseHandle - database handle
*          jobUUID        - job UUID
*          scheduleUUID   - schedule UUID
* Output : databaseStatementHandle - database query handle
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_initList(DatabaseStatementHandle *databaseStatementHandle,
                           DatabaseHandle          *databaseHandle,
                           const char              *jobUUID,
                           const char              *scheduleUUID
                          );

/***********************************************************************\
* Name   : Continuous_doneList
* Purpose: done index list
* Input  : databaseStatementHandle - database query handle
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Continuous_doneList(DatabaseStatementHandle *databaseStatementHandle);

/***********************************************************************\
* Name   : Continuous_getNext
* Purpose: get next continuous entry
* Input  : databaseStatementHandle - database query handle
* Output : databaseId - database id of entry
*          name       - name
* Return : TRUE if entry read, FALSE otherwise
* Notes  : -
\***********************************************************************/

bool Continuous_getNext(DatabaseStatementHandle *databaseStatementHandle,
                        DatabaseId              *databaseId,
                        String                  name
                       );

/***********************************************************************\
* Name   : Continuous_markEntryStored
* Purpose: mark entry as stored
* Input  : databaseHandle - database handle
*          databaseId     - database id
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Continuous_markEntryStored(DatabaseHandle *databaseHandle,
                                  DatabaseId     databaseId
                                 );

#ifndef NDEBUG
/***********************************************************************\
* Name   : Continuous_dumpEntries
* Purpose: dump continuous entries
* Input  : databaseHandle - database handle
*          jobUUID        - job UUID
*          scheduleUUID   - schedule UUID
* Output : -
* Return : -
* Notes  : debug only
\***********************************************************************/

void Continuous_dumpEntries(DatabaseHandle *databaseHandle,
                            const char     *jobUUID,
                            const char     *scheduleUUID
                           );

/***********************************************************************\
* Name   : Continuous_debugPrintStatistics
* Purpose: print statistics
* Input  : -
* Output : -
* Return : -
* Notes  : debug only
\***********************************************************************/

void Continuous_debugPrintStatistics(void);
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

#endif /* __CONTINUOUS__ */

/* end of file */
