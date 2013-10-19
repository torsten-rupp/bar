/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: index database functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "database.h"
#include "errors.h"

#include "bar.h"
#include "storage.h"
#include "index_definition.h"

#include "index.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/
LOCAL const struct
{
  const char  *name;
  IndexStates indexState;
} INDEX_STATES[] =
{
  { "none",             INDEX_STATE_NONE             },
  { "ok",               INDEX_STATE_OK               },
  { "create",           INDEX_STATE_CREATE           },
  { "update_requested", INDEX_STATE_UPDATE_REQUESTED },
  { "update",           INDEX_STATE_UPDATE           },
  { "error",            INDEX_STATE_ERROR            },
  { "*",                INDEX_STATE_ALL              }
};

LOCAL const struct
{
  const char *name;
  IndexModes indexMode;
} INDEX_MODES[] =
{
  { "MANUAL", INDEX_MODE_MANUAL },
  { "AUTO",   INDEX_MODE_AUTO   },
  { "*",      INDEX_MODE_ALL    }
};

// current index database version
#define CURRENT_INDEX_VERSION 2

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : getIndexStateSetString
* Purpose: get index state filter string
* Input  : string        - string variable
*          indexStateSet - index state set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

LOCAL String getIndexStateSetString(String string, IndexStateSet indexStateSet)
{
  uint i;

  String_clear(string);
  for (i = INDEX_STATE_OK; i < INDEX_STATE_UNKNOWN; i++)
  {
    if ((indexStateSet & (1 << i)) != 0)
    {
      if (!String_isEmpty(string)) String_appendCString(string,",");
      String_format(string,"%d",i);
    }
  }

  return string;
}

/***********************************************************************\
* Name   : getIndexModeSetString
* Purpose: get index state filter string
* Input  : string       - string variable
*          indexModeSet - index mode set
* Output : -
* Return : string for IN statement
* Notes  : -
\***********************************************************************/

LOCAL String getIndexModeSetString(String string, IndexModeSet indexModeSet)
{
  uint i;

  String_clear(string);
  for (i = INDEX_MODE_MANUAL; i < INDEX_MODE_UNKNOWN; i++)
  {
    if ((indexModeSet & (1 << i)) != 0)
    {
      if (!String_isEmpty(string)) String_appendCString(string,",");
      String_format(string,"%d",i);
    }
  }

  return string;
}

/***********************************************************************\
* Name   : getREGEXPString
* Purpose: get REGEXP filter string
* Input  : string      - string variable
*          columnName  - column name
*          patternText - pattern text
* Output : -
* Return : string for WHERE statement
* Notes  : -
\***********************************************************************/

LOCAL String getREGEXPString(String string, const char *columnName, const String patternText)
{
  StringTokenizer stringTokenizer;
  String          token;
  ulong           z;
  char            ch;

  String_setCString(string,"1");
  if (patternText != NULL)
  {
    String_initTokenizer(&stringTokenizer,
                         patternText,
                         STRING_BEGIN,
                         STRING_WHITE_SPACES,
                         STRING_QUOTES,
                         TRUE
                        );
    while (String_getNextToken(&stringTokenizer,&token,NULL))
    {
      String_appendCString(string," AND REGEXP('");
      z = 0;
      while (z < String_length(token))
      {
        ch = String_index(token,z);
        switch (ch)
        {
          case '.':
          case '[':
          case ']':
          case '(':
          case ')':
          case '{':
          case '}':
          case '+':
          case '|':
          case '^':
          case '$':
          case '\\':
            String_appendChar(string,'\\');
            String_appendChar(string,ch);
            z++;
            break;
          case '*':
            String_appendCString(string,".*");
            z++;
            break;
          case '?':
            String_appendChar(string,'.');
            z++;
            break;
          case '\'':
            String_appendCString(string,"''");
            z++;
            break;
          default:
            String_appendChar(string,ch);
            z++;
            break;
        }
      }
      String_format(string,"',0,%s)",columnName);
    }
    String_doneTokenizer(&stringTokenizer);
  }

  return string;
}

/*---------------------------------------------------------------------*/

Errors Index_initAll(void)
{
  return ERROR_NONE;
}

void Index_doneAll(void)
{
}

const char *Index_stateToString(IndexStates indexState, const char *defaultValue)
{
  uint       z;
  const char *name;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_STATES))
         && (INDEX_STATES[z].indexState != indexState)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_STATES))
  {
    name = INDEX_STATES[z].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseState(const char *name, IndexStates *indexState)
{
  uint z;

  assert(name != NULL);
  assert(indexState != NULL);

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_STATES))
         && !stringEqualsIgnoreCase(INDEX_STATES[z].name,name)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_STATES))
  {
    (*indexState) = INDEX_STATES[z].indexState;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

const char *Index_modeToString(IndexModes indexMode, const char *defaultValue)
{
  uint       z;
  const char *name;

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_MODES))
         && (INDEX_MODES[z].indexMode != indexMode)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_MODES))
  {
    name = INDEX_MODES[z].name;
  }
  else
  {
    name = defaultValue;
  }

  return name;
}

bool Index_parseMode(const char *name, IndexModes *indexMode)
{
  uint z;

  assert(name != NULL);
  assert(indexMode != NULL);

  z = 0;
  while (   (z < SIZE_OF_ARRAY(INDEX_MODES))
         && !stringEqualsIgnoreCase(INDEX_MODES[z].name,name)
        )
  {
    z++;
  }
  if (z < SIZE_OF_ARRAY(INDEX_MODES))
  {
    (*indexMode) = INDEX_MODES[z].indexMode;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

#ifdef NDEBUG
  Errors Index_init(DatabaseHandle *indexDatabaseHandle,
                    const char     *indexDatabaseFileName
                   )
#else /* not NDEBUG */
  Errors __Index_init(const char     *__fileName__,
                      uint           __lineNb__,
                      DatabaseHandle *indexDatabaseHandle,
                      const char     *indexDatabaseFileName
                     )
#endif /* NDEBUG */
{
  Errors error;
  int64  indexVersion;

  // open/create datbase
  if (File_existsCString(indexDatabaseFileName))
  {
    // open index database
    #ifdef NDEBUG
      error = Database_open(indexDatabaseHandle,indexDatabaseFileName,DATABASE_OPENMODE_READWRITE);
    #else /* not NDEBUG */
      error = __Database_open(__fileName__,__lineNb__,indexDatabaseHandle,indexDatabaseFileName,DATABASE_OPENMODE_READWRITE);
    #endif /* NDEBUG */
    if (error != ERROR_NONE)
    {
      return error;
    }
  }
  else
  {
    // create index database
    #ifdef NDEBUG
      error = Database_open(indexDatabaseHandle,indexDatabaseFileName,DATABASE_OPENMODE_CREATE);
    #else /* not NDEBUG */
      error = __Database_open(__fileName__,__lineNb__,indexDatabaseHandle,indexDatabaseFileName,DATABASE_OPENMODE_CREATE);
    #endif /* NDEBUG */
    if (error != ERROR_NONE)
    {
      return error;
    }
    error = Database_execute(indexDatabaseHandle,
                             NULL,
                             NULL,
                             INDEX_TABLE_DEFINITION
                            );
    if (error != ERROR_NONE)
    {
      #ifdef NDEBUG
        Database_close(indexDatabaseHandle);
      #else /* not NDEBUG */
        __Database_close(__fileName__,__lineNb__,indexDatabaseHandle);
      #endif /* NDEBUG */
      return error;
    }
  }

  // disable synchronous mode and journal to increase transaction speed
  Database_execute(indexDatabaseHandle,
                   NULL,
                   NULL,
                   "PRAGMA synchronous=OFF;"
                  );
  Database_execute(indexDatabaseHandle,
                   NULL,
                   NULL,
                   "PRAGMA journal_mode=OFF;"
                  );

  // get database version
  error = Database_getInteger64(indexDatabaseHandle,
                                &indexVersion,
                                "meta",
                                "value",
                                "WHERE name='version'"
                               );
  if (error != ERROR_NONE)
  {
    #ifdef NDEBUG
      Database_close(indexDatabaseHandle);
    #else /* not NDEBUG */
      __Database_close(__fileName__,__lineNb__,indexDatabaseHandle);
    #endif /* NDEBUG */
    return error;
  }

  // upgrade database structure
  while (indexVersion < CURRENT_INDEX_VERSION)
  {
    switch (indexVersion)
    {
      case 1:
        // upgrade version 1 -> 2

        // add table hardlinks
        error = Database_execute(indexDatabaseHandle,
                                 NULL,
                                 NULL,
                                 INDEX_TABLE_DEFINITION_HARDLINKS
                                );
        if (error != ERROR_NONE)
        {
          #ifdef NDEBUG
            Database_close(indexDatabaseHandle);
          #else /* not NDEBUG */
            __Database_close(__fileName__,__lineNb__,indexDatabaseHandle);
          #endif /* NDEBUG */
          return error;
        }

        // set version
        error = Database_setInteger64(indexDatabaseHandle,
                                      2,
                                      "meta",
                                      "value",
                                      "WHERE name='version'"
                                     );
        if (error != ERROR_NONE)
        {
          #ifdef NDEBUG
            Database_close(indexDatabaseHandle);
          #else /* not NDEBUG */
            __Database_close(__fileName__,__lineNb__,indexDatabaseHandle);
          #endif /* NDEBUG */
          return error;
        }

        indexVersion = 2;
      case 2:
        // OK!
        break;
      #ifndef NDEBUG
        default:
          HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          break;
      #endif
    }
  }

  return ERROR_NONE;
}

#ifdef NDEBUG
  void Index_done(DatabaseHandle *indexDatabaseHandle)
#else /* not NDEBUG */
  void __Index_done(const char     *__fileName__,
                    uint           __lineNb__,
                    DatabaseHandle *indexDatabaseHandle
                   )
#endif /* NDEBUG */
{
  assert(indexDatabaseHandle != NULL);

  #ifdef NDEBUG
    Database_close(indexDatabaseHandle);
  #else /* not NDEBUG */
    __Database_close(__fileName__,__lineNb__,indexDatabaseHandle);
  #endif /* NDEBUG */
}

bool Index_findById(DatabaseHandle *databaseHandle,
                    int64          storageId,
                    String         name,
                    IndexStates    *indexState,
                    uint64         *lastChecked
                   )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  bool                result;

  assert(storageId != DATABASE_ID_NONE);
  assert(databaseHandle != NULL);

  error = Database_prepare(&databaseQueryHandle,
                           databaseHandle,
                           "SELECT name, \
                                   state, \
                                   STRFTIME('%%s',lastChecked) \
                            FROM storage \
                            WHERE id=%lld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return FALSE;
  }
  result = Database_getNextRow(&databaseQueryHandle,
                               "%S %d %llu",
                               &name,
                               indexState,
                               lastChecked
                              );
  Database_finalize(&databaseQueryHandle);

  return result;
}

bool Index_findByName(DatabaseHandle *databaseHandle,
                      StorageTypes   storageType,
                      const String   hostName,
                      const String   loginName,
                      const String   deviceName,
                      const String   fileName,
                      int64          *storageId,
                      IndexStates    *indexState,
                      uint64         *lastChecked
                     )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  String              storageName;
  StorageSpecifier    storageSpecifier;
  String              storageFileName;
  bool                foundFlag;

  assert(storageId != NULL);
  assert(databaseHandle != NULL);

  (*storageId) = DATABASE_ID_NONE;

  error = Database_prepare(&databaseQueryHandle,
                           databaseHandle,
                           "SELECT id, \
                                   name, \
                                   state, \
                                   STRFTIME('%%s',lastChecked) \
                            FROM storage \
                           "
                          );
  if (error != ERROR_NONE)
  {
    return FALSE;
  }

  storageName     = String_new();
  Storage_initSpecifier(&storageSpecifier);
  storageFileName = String_new();
  foundFlag       = FALSE;
  while (   Database_getNextRow(&databaseQueryHandle,
                                "%lld %S %d %llu",
                                 storageId,
                                 &storageName,
                                 indexState,
                                 lastChecked
                                )
         && !foundFlag
        )
  {
    if (Storage_parseName(storageName,&storageSpecifier,storageFileName) == ERROR_NONE)
    {
      switch (storageSpecifier.type)
      {
        case STORAGE_TYPE_FILESYSTEM:
          foundFlag =     ((storageType == STORAGE_TYPE_UNKNOWN) || (storageType == STORAGE_TYPE_FILESYSTEM))
                      && ((fileName == NULL) || String_equals(fileName,storageFileName));
          break;
        case STORAGE_TYPE_FTP:
          foundFlag =    ((storageType == STORAGE_TYPE_UNKNOWN) || (storageType == STORAGE_TYPE_FTP))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName ))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageFileName           ));
          break;
        case STORAGE_TYPE_SSH:
        case STORAGE_TYPE_SCP:
          foundFlag =    ((storageType == STORAGE_TYPE_UNKNOWN) || (storageType == STORAGE_TYPE_SSH) || (storageType == STORAGE_TYPE_SCP))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName ))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageFileName           ));
          break;
        case STORAGE_TYPE_SFTP:
          foundFlag =    ((storageType == STORAGE_TYPE_UNKNOWN) || (storageType == STORAGE_TYPE_SFTP))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName ))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageFileName           ));
          break;
        case STORAGE_TYPE_WEBDAV:
          foundFlag =    ((storageType == STORAGE_TYPE_UNKNOWN) || (storageType == STORAGE_TYPE_WEBDAV))
                      && ((hostName  == NULL) || String_equals(hostName, storageSpecifier.hostName ))
                      && ((loginName == NULL) || String_equals(loginName,storageSpecifier.loginName))
                      && ((fileName  == NULL) || String_equals(fileName, storageFileName           ));
          break;
        case STORAGE_TYPE_CD:
          foundFlag =    ((storageType == STORAGE_TYPE_UNKNOWN) || (storageType == STORAGE_TYPE_CD))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageFileName            ));
          break;
        case STORAGE_TYPE_DVD:
          foundFlag =    ((storageType == STORAGE_TYPE_UNKNOWN) || (storageType == STORAGE_TYPE_DVD))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageFileName            ));
          break;
        case STORAGE_TYPE_BD:
          foundFlag =    ((storageType == STORAGE_TYPE_UNKNOWN) || (storageType == STORAGE_TYPE_BD))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageFileName            ));
          break;
        case STORAGE_TYPE_DEVICE:
          foundFlag =    ((storageType == STORAGE_TYPE_UNKNOWN) || (storageType == STORAGE_TYPE_DEVICE))
                      && ((deviceName == NULL) || String_equals(deviceName,storageSpecifier.deviceName))
                      && ((fileName   == NULL) || String_equals(fileName,  storageFileName            ));
          break;
        default:
          #ifndef NDEBUG
            HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
          #endif /* NDEBUG */
          break;
      }
    }
  }
  String_delete(storageFileName);
  Storage_doneSpecifier(&storageSpecifier);
  String_delete(storageName);

  Database_finalize(&databaseQueryHandle);

  return foundFlag;
}

bool Index_findByState(DatabaseHandle *databaseHandle,
                       IndexStateSet  indexStateSet,
                       int64          *storageId,
                       String         name,
                       uint64         *lastChecked
                      )
{
  String              indexStateSetString;
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  bool                result;

  assert(storageId != NULL);
  assert(databaseHandle != NULL);

  (*storageId) = DATABASE_ID_NONE;
  if (name != NULL) String_clear(name);
  if (lastChecked != NULL) (*lastChecked) = 0LL;

  indexStateSetString = String_new();
  error = Database_prepare(&databaseQueryHandle,
                           databaseHandle,
                           "SELECT id, \
                                   name, \
                                   STRFTIME('%%s',lastChecked) \
                            FROM storage \
                            WHERE state IN (%S) \
                           ",
//                            WHERE state=%d
                           getIndexStateSetString(indexStateSetString,indexStateSet)
//                           indexState
                          );
  if (error != ERROR_NONE)
  {
    String_delete(indexStateSetString);
    return FALSE;
  }
  String_delete(indexStateSetString);
  result = Database_getNextRow(&databaseQueryHandle,
                               "%lld %S %llu",
                               storageId,
                               &name,
                               lastChecked
                              );
  Database_finalize(&databaseQueryHandle);

  return result;
}

Errors Index_create(DatabaseHandle *databaseHandle,
                    const String   name,
                    IndexStates    indexState,
                    IndexModes     indexMode,
                    int64          *storageId
                   )
{
  Errors error;

  assert(storageId != NULL);
  assert(databaseHandle != NULL);

  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "INSERT INTO storage \
                              (\
                               name,\
                               created,\
                               size,\
                               state,\
                               mode,\
                               lastChecked\
                              ) \
                            VALUES \
                             (\
                              %'S,\
                              DATETIME('now'),\
                              0,\
                              %d,\
                              %d,\
                              DATETIME('now')\
                             ); \
                           ",
                           name,
                           indexState,
                           indexMode
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  (*storageId) = Database_getLastRowId(indexDatabaseHandle);

  return ERROR_NONE;
}

Errors Index_delete(DatabaseHandle *databaseHandle,
                    int64          storageId
                   )
{
  Errors error;

  assert(databaseHandle != NULL);
  assert(storageId != 0LL);

  // Note: do in single steps to avoid long-time-locking of database!

  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM files WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM images WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM directories WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM links WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM hardlinks WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM special WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM storage WHERE id=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;

  return ERROR_NONE;
}

Errors Index_clear(DatabaseHandle *databaseHandle,
                   int64          storageId
                  )
{
  Errors error;

  assert(databaseHandle != NULL);
  assert(storageId != 0LL);

  // Note: do in single steps to avoid long-time-locking of database!

  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM files WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM images WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM directories WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM links WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM hardlinks WHERE storageId=%ld;",
                           storageId
                          );
  if (error != ERROR_NONE) return error;
  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "DELETE FROM special WHERE storageId=%ld;",
                           storageId
                          );

  return ERROR_NONE;
}

Errors Index_update(DatabaseHandle *databaseHandle,
                    int64          storageId,
                    String         name,
                    uint64         size
                   )
{
  Errors error;

  assert(databaseHandle != NULL);

  if (name != NULL)
  {
    error = Database_execute(databaseHandle,
                             NULL,
                             NULL,
                             "UPDATE storage \
                              SET name=%'S,\
                                  size=%ld \
                              WHERE id=%ld;\
                             ",
                             name,
                             size,
                             storageId
                            );
  }
  else
  {
    error = Database_execute(databaseHandle,
                             NULL,
                             NULL,
                             "UPDATE storage \
                              SET size=%ld \
                              WHERE id=%ld;\
                             ",
                             size,
                             storageId
                            );
  }
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

Errors Index_getState(DatabaseHandle *databaseHandle,
                      int64          storageId,
                      IndexStates    *indexState,
                      uint64         *lastChecked,
                      String         errorMessage
                     )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;

  error = Database_prepare(&databaseQueryHandle,
                           databaseHandle,
                           "SELECT state, \
                                   STRFTIME('%%s',lastChecked), \
                                   errorMessage \
                            FROM storage \
                            WHERE id=%ld \
                           ",
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }
  if (!Database_getNextRow(&databaseQueryHandle,
                           "%d %llu %S",
                           indexState,
                           lastChecked,
                           errorMessage
                          )
     )
  {
    (*indexState) = INDEX_STATE_UNKNOWN;
    if (errorMessage != NULL) String_clear(errorMessage);
  }
  Database_finalize(&databaseQueryHandle);

  return ERROR_NONE;
}

Errors Index_setState(DatabaseHandle *databaseHandle,
                      int64          storageId,
                      IndexStates    indexState,
                      uint64         lastChecked,
                      const char     *errorMessage,
                      ...
                     )
{
  Errors  error;
  va_list arguments;
  String  s;


  assert(databaseHandle != NULL);

  error = Database_execute(databaseHandle,
                           NULL,
                           NULL,
                           "UPDATE storage \
                            SET state=%d, \
                                errorMessage=NULL \
                            WHERE id=%ld; \
                           ",
                           indexState,
                           storageId
                          );
  if (error != ERROR_NONE)
  {
    return error;
  }

  if (lastChecked != 0LL)
  {
    error = Database_execute(databaseHandle,
                             NULL,
                             NULL,
                             "UPDATE storage \
                              SET lastChecked=DATETIME(%llu,'unixepoch') \
                              WHERE id=%ld; \
                             ",
                             lastChecked,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      return error;
    }
  }

  if (errorMessage != NULL)
  {
    va_start(arguments,errorMessage);
    s = String_vformat(String_new(),errorMessage,arguments);
    va_end(arguments);

    error = Database_execute(databaseHandle,
                             NULL,
                             NULL,
                             "UPDATE storage \
                              SET errorMessage=%'S \
                              WHERE id=%ld; \
                             ",
                             s,
                             storageId
                            );
    if (error != ERROR_NONE)
    {
      String_delete(s);
      return error;
    }

    String_delete(s);
  }
  else
  {
  }

  return ERROR_NONE;
}

long Index_countState(DatabaseHandle *databaseHandle,
                      IndexStates    indexState
                     )
{
  DatabaseQueryHandle databaseQueryHandle;
  Errors              error;
  long                count;

  assert(databaseHandle != NULL);

  error = Database_prepare(&databaseQueryHandle,
                           databaseHandle,
                           "SELECT COUNT(id) \
                            FROM storage \
                            WHERE (%d=%d OR state=%d) \
                           ",
                           indexState,
                           INDEX_STATE_ALL,
                           indexState
                          );
  if (error != ERROR_NONE)
  {
    return -1L;
  }
  if (!Database_getNextRow(&databaseQueryHandle,
                           "%ld",
                           &count
                          )
     )
  {
    Database_finalize(&databaseQueryHandle);
    return -1L;
  }
  Database_finalize(&databaseQueryHandle);

  return count;
}

Errors Index_initListStorage(DatabaseQueryHandle *databaseQueryHandle,
                             DatabaseHandle      *databaseHandle,
                             IndexStateSet       indexStateSet,
                             String              pattern
                            )
{
  String regexpString,indexStateSetString;
  Errors error;

  assert(databaseQueryHandle != NULL);
  assert(databaseHandle != NULL);

  regexpString        = String_new();
  indexStateSetString = String_new();
  error = Database_prepare(databaseQueryHandle,
                           databaseHandle,
                           "SELECT id, \
                                   name, \
                                   STRFTIME('%%s',created), \
                                   size, \
                                   state, \
                                   mode, \
                                   STRFTIME('%%s',lastChecked), \
                                   errorMessage \
                            FROM storage \
                            WHERE %S \
                                  AND state IN (%S) \
                            ORDER BY created DESC \
                           ",
                           getREGEXPString(regexpString,"name",pattern),
                           getIndexStateSetString(indexStateSetString,indexStateSet),
                           INDEX_STATE_CREATE
                          );
  String_delete(indexStateSetString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextStorage(DatabaseQueryHandle *databaseQueryHandle,
                          DatabaseId          *databaseId,
                          String              storageName,
                          uint64              *createDateTime,
                          uint64              *size,
                          IndexStates         *indexState,
                          IndexModes          *indexMode,
                          uint64              *lastCheckedDateTime,
                          String              errorMessage
                         )
{
  assert(databaseQueryHandle != NULL);

  return Database_getNextRow(databaseQueryHandle,
                             "%lld %S %llu %llu %d %d %llu %S",
                             databaseId,
                             &storageName,
                             createDateTime,
                             size,
                             indexState,
                             indexMode,
                             lastCheckedDateTime,
                             &errorMessage
                            );
}

Errors Index_initListFiles(DatabaseQueryHandle *databaseQueryHandle,
                           DatabaseHandle      *databaseHandle,
                           const DatabaseId    *storageIds,
                           uint                storageIdCount,
                           String              pattern
                          )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(databaseQueryHandle != NULL);
  assert(databaseHandle != NULL);

  regexpString = getREGEXPString(String_new(),"files.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(0");
    for (z = 0; z < storageIdCount; z++)
    {
      String_format(storageIdsString," OR (storage.id=%d)",storageIds[z]);
    }
    String_appendCString(storageIdsString,")");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(databaseQueryHandle,
                           databaseHandle,
                           "SELECT files.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   files.name, \
                                   files.size, \
                                   files.timeModified, \
                                   files.userId, \
                                   files.groupId, \
                                   files.permission, \
                                   files.fragmentOffset, \
                                   files.fragmentSize\
                            FROM files\
                              LEFT JOIN storage ON storage.id=files.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextFile(DatabaseQueryHandle *databaseQueryHandle,
                       DatabaseId          *databaseId,
                       String              storageName,
                       uint64              *storageDateTime,
                       String              fileName,
                       uint64              *size,
                       uint64              *timeModified,
                       uint32              *userId,
                       uint32              *groupId,
                       uint32              *permission,
                       uint64              *fragmentOffset,
                       uint64              *fragmentSize
                      )
{
  assert(databaseQueryHandle != NULL);

  return Database_getNextRow(databaseQueryHandle,
                             "%lld %S %llu %S %llu %llu %d %d %d %llu %llu",
                             databaseId,
                             &storageName,
                             storageDateTime,
                             &fileName,
                             size,
                             timeModified,
                             userId,
                             groupId,
                             permission,
                             fragmentOffset,
                             fragmentSize
                            );
}

Errors Index_initListImages(DatabaseQueryHandle *databaseQueryHandle,
                            DatabaseHandle      *databaseHandle,
                            const DatabaseId    *storageIds,
                            uint                storageIdCount,
                            String              pattern
                           )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(databaseQueryHandle != NULL);
  assert(databaseHandle != NULL);

  regexpString = getREGEXPString(String_new(),"images.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(0");
    for (z = 0; z < storageIdCount; z++)
    {
      String_format(storageIdsString," OR (storage.id=%d)",storageIds[z]);
    }
    String_appendCString(storageIdsString,")");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(databaseQueryHandle,
                           databaseHandle,
                           "SELECT images.id, \
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   images.name, \
                                   images.size, \
                                   images.blockOffset, \
                                   images.blockCount \
                            FROM images \
                              LEFT JOIN storage ON storage.id=images.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextImage(DatabaseQueryHandle *databaseQueryHandle,
                        DatabaseId          *databaseId,
                        String              storageName,
                        uint64              *storageDateTime,
                        String              imageName,
                        uint64              *size,
                        uint64              *blockOffset,
                        uint64              *blockCount
                       )
{
  assert(databaseQueryHandle != NULL);

  return Database_getNextRow(databaseQueryHandle,
                             "%lld %S %llu %S %llu %llu %llu",
                             databaseId,
                             &storageName,
                             storageDateTime,
                             &imageName,
                             size,
                             blockOffset,
                             blockCount
                            );
}

Errors Index_initListDirectories(DatabaseQueryHandle *databaseQueryHandle,
                                 DatabaseHandle      *databaseHandle,
                                 const DatabaseId    *storageIds,
                                 uint                storageIdCount,
                                 String              pattern
                                )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(databaseQueryHandle != NULL);
  assert(databaseHandle != NULL);

  regexpString = getREGEXPString(String_new(),"directories.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(0");
    for (z = 0; z < storageIdCount; z++)
    {
      String_format(storageIdsString," OR (storage.id=%d)",storageIds[z]);
    }
    String_appendCString(storageIdsString,")");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(databaseQueryHandle,
                           databaseHandle,
                           "SELECT directories.id,\
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   directories.name, \
                                   directories.timeModified, \
                                   directories.userId, \
                                   directories.groupId, \
                                   directories.permission \
                            FROM directories \
                              LEFT JOIN storage ON storage.id=directories.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextDirectory(DatabaseQueryHandle *databaseQueryHandle,
                            DatabaseId          *databaseId,
                            String              storageName,
                            uint64              *storageDateTime,
                            String              directoryName,
                            uint64              *timeModified,
                            uint32              *userId,
                            uint32              *groupId,
                            uint32              *permission
                           )
{
  assert(databaseQueryHandle != NULL);

  return Database_getNextRow(databaseQueryHandle,
                             "%lld %S %llu %S %llu %d %d %d",
                             databaseId,
                             &storageName,
                             storageDateTime,
                             &directoryName,
                             timeModified,
                             userId,
                             groupId,
                             permission
                            );
}

Errors Index_initListLinks(DatabaseQueryHandle *databaseQueryHandle,
                           DatabaseHandle      *databaseHandle,
                           const DatabaseId    *storageIds,
                           uint                storageIdCount,
                           String              pattern
                          )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(databaseQueryHandle != NULL);
  assert(databaseHandle != NULL);

  regexpString = getREGEXPString(String_new(),"links.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(0");
    for (z = 0; z < storageIdCount; z++)
    {
      String_format(storageIdsString," OR (storage.id=%d)",storageIds[z]);
    }
    String_appendCString(storageIdsString,")");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(databaseQueryHandle,
                           databaseHandle,
                           "SELECT links.id,\
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   links.name, \
                                   links.destinationName, \
                                   links.timeModified, \
                                   links.userId, \
                                   links.groupId, \
                                   links.permission \
                            FROM links \
                              LEFT JOIN storage ON storage.id=links.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextLink(DatabaseQueryHandle *databaseQueryHandle,
                       DatabaseId          *databaseId,
                       String              storageName,
                       uint64              *storageDateTime,
                       String              linkName,
                       String              destinationName,
                       uint64              *timeModified,
                       uint32              *userId,
                       uint32              *groupId,
                       uint32              *permission
                      )
{
  assert(databaseQueryHandle != NULL);

  return Database_getNextRow(databaseQueryHandle,
                             "%lld %S %llu %S %S %llu %d %d %d",
                             databaseId,
                             &storageName,
                             storageDateTime,
                             &linkName,
                             &destinationName,
                             timeModified,
                             userId,
                             groupId,
                             permission
                            );
}

Errors Index_initListHardLinks(DatabaseQueryHandle *databaseQueryHandle,
                               DatabaseHandle      *databaseHandle,
                               const DatabaseId    *storageIds,
                               uint                storageIdCount,
                               String              pattern
                              )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(databaseQueryHandle != NULL);
  assert(databaseHandle != NULL);

  regexpString = getREGEXPString(String_new(),"hardlinks.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(0");
    for (z = 0; z < storageIdCount; z++)
    {
      String_format(storageIdsString," OR (storage.id=%d)",storageIds[z]);
    }
    String_appendCString(storageIdsString,")");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(databaseQueryHandle,
                           databaseHandle,
                           "SELECT hardlinks.id,\
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   hardlinks.name, \
                                   hardlinks.size, \
                                   hardlinks.timeModified, \
                                   hardlinks.userId, \
                                   hardlinks.groupId, \
                                   hardlinks.permission, \
                                   hardlinks.fragmentOffset, \
                                   hardlinks.fragmentSize\
                            FROM hardlinks \
                              LEFT JOIN storage ON storage.id=hardlinks.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextHardLink(DatabaseQueryHandle *databaseQueryHandle,
                           DatabaseId          *databaseId,
                           String              storageName,
                           uint64              *storageDateTime,
                           String              fileName,
                           uint64              *size,
                           uint64              *timeModified,
                           uint32              *userId,
                           uint32              *groupId,
                           uint32              *permission,
                           uint64              *fragmentOffset,
                           uint64              *fragmentSize
                          )
{
  assert(databaseQueryHandle != NULL);

  return Database_getNextRow(databaseQueryHandle,
                             "%lld %S %llu %S %llu %llu %d %d %d %llu %llu",
                             databaseId,
                             &storageName,
                             storageDateTime,
                             &fileName,
                             size,
                             timeModified,
                             userId,
                             groupId,
                             permission,
                             fragmentOffset,
                             fragmentSize
                            );
}

Errors Index_initListSpecial(DatabaseQueryHandle *databaseQueryHandle,
                             DatabaseHandle      *databaseHandle,
                             const DatabaseId    *storageIds,
                             uint                storageIdCount,
                             String              pattern
                            )
{
  String regexpString;
  String storageIdsString;
  uint   z;
  Errors error;

  assert(databaseQueryHandle != NULL);
  assert(databaseHandle != NULL);

  regexpString = getREGEXPString(String_new(),"special.name",pattern);

  if (storageIds != NULL)
  {
    storageIdsString = String_newCString("(0");
    for (z = 0; z < storageIdCount; z++)
    {
      String_format(storageIdsString," OR (storage.id=%d)",storageIds[z]);
    }
    String_appendCString(storageIdsString,")");
  }
  else
  {
    storageIdsString = String_newCString("1");
  }

  error = Database_prepare(databaseQueryHandle,
                           databaseHandle,
                           "SELECT special.id,\
                                   storage.name, \
                                   STRFTIME('%%s',storage.created), \
                                   special.name, \
                                   special.timeModified, \
                                   special.userId, \
                                   special.groupId, \
                                   special.permission \
                            FROM special \
                              LEFT JOIN storage ON storage.id=special.storageId \
                            WHERE     %S \
                                  AND %S \
                           ",
                           regexpString,
                           storageIdsString
                          );

  String_delete(storageIdsString);
  String_delete(regexpString);

  return error;
}

bool Index_getNextSpecial(DatabaseQueryHandle *databaseQueryHandle,
                          DatabaseId          *databaseId,
                          String              storageName,
                          uint64              *storageDateTime,
                          String              name,
                          uint64              *timeModified,
                          uint32              *userId,
                          uint32              *groupId,
                          uint32              *permission
                         )
{
  assert(databaseQueryHandle != NULL);

  return Database_getNextRow(databaseQueryHandle,
                             "%lld %S %llu %S %llu %d %d %d",
                             databaseId,
                             &storageName,
                             storageDateTime,
                             &name,
                             timeModified,
                             userId,
                             groupId,
                             permission
                            );
}

void Index_doneList(DatabaseQueryHandle *databaseQueryHandle)
{
  assert(databaseQueryHandle != NULL);

  Database_finalize(databaseQueryHandle);
}

Errors Index_addFile(DatabaseHandle *databaseHandle,
                     int64          storageId,
                     const String   fileName,
                     uint64         size,
                     uint64         timeLastAccess,
                     uint64         timeModified,
                     uint64         timeLastChanged,
                     uint32         userId,
                     uint32         groupId,
                     uint32         permission,
                     uint64         fragmentOffset,
                     uint64         fragmentSize
                    )
{
  assert(databaseHandle != NULL);
  assert(fileName != NULL);

  return Database_execute(databaseHandle,
                          NULL,
                          NULL,
                          "INSERT INTO files \
                             (\
                              storageId,\
                              name,\
                              size,\
                              timeLastAccess,\
                              timeModified,\
                              timeLastChanged,\
                              userId,\
                              groupId,\
                              permission,\
                              fragmentOffset,\
                              fragmentSize\
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %u,\
                              %u,\
                              %u,\
                              %lu,\
                              %lu\
                             ); \
                          ",
                          storageId,
                          fileName,
                          size,
                          timeLastAccess,
                          timeModified,
                          timeLastChanged,
                          userId,
                          groupId,
                          permission,
                          fragmentOffset,
                          fragmentSize
                         );
}

Errors Index_addImage(DatabaseHandle *databaseHandle,
                      int64          storageId,
                      const String   imageName,
                      int64          size,
                      ulong          blockSize,
                      uint64         blockOffset,
                      uint64         blockCount
                     )
{
  assert(databaseHandle != NULL);
  assert(imageName != NULL);

  return Database_execute(databaseHandle,
                          NULL,
                          NULL,
                          "INSERT INTO images \
                             (\
                              storageId,\
                              name,\
                              fileSystemType,\
                              size,\
                              blockSize,\
                              blockOffset,\
                              blockCount\
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %d,\
                              %lu,\
                              %u,\
                              %lu,\
                              %lu\
                             );\
                          ",
                          storageId,
                          imageName,
                          size,
                          blockSize,
                          blockOffset,
                          blockCount
                         );
}

Errors Index_addDirectory(DatabaseHandle *databaseHandle,
                          int64          storageId,
                          String         directoryName,
                          uint64         timeLastAccess,
                          uint64         timeModified,
                          uint64         timeLastChanged,
                          uint32         userId,
                          uint32         groupId,
                          uint32         permission
                         )
{
  assert(databaseHandle != NULL);
  assert(directoryName != NULL);

  return Database_execute(databaseHandle,
                          NULL,
                          NULL,
                          "INSERT INTO directories \
                             (\
                              storageId,\
                              name,\
                              timeLastAccess,\
                              timeModified,\
                              timeLastChanged,\
                              userId,\
                              groupId,\
                              permission\
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %u,\
                              %u,\
                              %u \
                             );\
                          ",
                          storageId,
                          directoryName,
                          timeLastAccess,
                          timeModified,
                          timeLastChanged,
                          userId,
                          groupId,
                          permission
                         );
}

Errors Index_addLink(DatabaseHandle *databaseHandle,
                     int64          storageId,
                     const String   linkName,
                     const String   destinationName,
                     uint64         timeLastAccess,
                     uint64         timeModified,
                     uint64         timeLastChanged,
                     uint32         userId,
                     uint32         groupId,
                     uint32         permission
                    )
{
  assert(databaseHandle != NULL);
  assert(linkName != NULL);
  assert(destinationName != NULL);

  return Database_execute(databaseHandle,
                          NULL,
                          NULL,
                          "INSERT INTO links \
                             (\
                              storageId,\
                              name,\
                              destinationName,\
                              timeLastAccess,\
                              timeModified,\
                              timeLastChanged,\
                              userId,\
                              groupId,\
                              permission\
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %'S,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %u,\
                              %u,\
                              %u\
                             );\
                           ",
                          storageId,
                          linkName,
                          destinationName,
                          timeLastAccess,
                          timeModified,
                          timeLastChanged,
                          userId,
                          groupId,
                          permission
                         );
}

Errors Index_addHardLink(DatabaseHandle *databaseHandle,
                         int64          storageId,
                         const String   fileName,
                         uint64         size,
                         uint64         timeLastAccess,
                         uint64         timeModified,
                         uint64         timeLastChanged,
                         uint32         userId,
                         uint32         groupId,
                         uint32         permission,
                         uint64         fragmentOffset,
                         uint64         fragmentSize
                        )
{
  assert(databaseHandle != NULL);
  assert(fileName != NULL);

  return Database_execute(databaseHandle,
                          NULL,
                          NULL,
                          "INSERT INTO hardlinks \
                             (\
                              storageId,\
                              name,\
                              size,\
                              timeLastAccess,\
                              timeModified,\
                              timeLastChanged,\
                              userId,\
                              groupId,\
                              permission,\
                              fragmentOffset,\
                              fragmentSize\
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %u,\
                              %u,\
                              %u,\
                              %lu,\
                              %lu\
                             ); \
                          ",
                          storageId,
                          fileName,
                          size,
                          timeLastAccess,
                          timeModified,
                          timeLastChanged,
                          userId,
                          groupId,
                          permission,
                          fragmentOffset,
                          fragmentSize
                         );
}

Errors Index_addSpecial(DatabaseHandle   *databaseHandle,
                        int64            storageId,
                        const String     name,
                        FileSpecialTypes specialType,
                        uint64           timeLastAccess,
                        uint64           timeModified,
                        uint64           timeLastChanged,
                        uint32           userId,
                        uint32           groupId,
                        uint32           permission,
                        uint32           major,
                        uint32           minor
                       )
{
  assert(databaseHandle != NULL);
  assert(name != NULL);

  return Database_execute(databaseHandle,
                          NULL,
                          NULL,
                          "INSERT INTO special \
                             (\
                              storageId,\
                              name,\
                              specialType,\
                              timeLastAccess,\
                              timeModified,\
                              timeLastChanged,\
                              userId,\
                              groupId,\
                              permission,\
                              major,\
                              minor \
                             ) \
                           VALUES \
                             (\
                              %lu,\
                              %'S,\
                              %u,\
                              %lu,\
                              %lu,\
                              %lu,\
                              %u,\
                              %u,\
                              %u,\
                              %d,\
                              %u\
                             );\
                          ",
                          storageId,
                          name,
                          specialType,
                          timeLastAccess,
                          timeModified,
                          timeLastChanged,
                          userId,
                          groupId,
                          permission,
                          major,
                          minor
                         );
}

#ifdef __cplusplus
  }
#endif

/* end of file */
