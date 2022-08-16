/***********************************************************************\
*
* $Revision: 3843 $
* $Date: 2015-03-26 21:07:41 +0100 (Thu, 26 Mar 2015) $
* $Author: torsten $
* Contents: stacktrace functions
* Systems: Linux
*
\***********************************************************************/

#ifndef __STACKTRACES__
#define __STACKTRACES__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/**************************** Datatypes ********************************/
typedef struct
{
  int        signalNumber;
  const char *signalName;
} SignalHandlerInfo;

typedef struct
{
  const char *symbolName;
  const char *fileName;
  ulong      lineNb;
} SymbolInfo;

/***********************************************************************\
* Name   : SignalHandlerFunction
* Purpose: signal handler function
* Input  : signalNumber   - signal number
*          signalName     - signal name
*          stackTrace     - stacktrace
*          stackTraceSize - stacktrace size
*          userData       - user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*SignalHandlerFunction)(int        signalNumber,
                                     const char *signalName,
                                     void const *stackTrace[],
                                     uint       stackTraceSize,
                                     void       *userData
                                    );


/***********************************************************************\
* Name   : SymbolFunction
* Purpose: callback for symbol information
* Input  : address - symbol address
*          fileName   - file name or NULL
*          symbolName - symbol name or NULL
*          lineNb     - line number or 0
*          userData   - callback user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

typedef void(*SymbolFunction)(const void *address,
                              const char *fileName,
                              const char *symbolName,
                              ulong      lineNb,
                              void       *userData
                             );

/**************************** Variables ********************************/

/****************************** Macros *********************************/

/**************************** Functions ********************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Stacktrace_init
* Purpose: init stacktrace
* Input  : signalHandlerInfo      - signal handler info
*          signalHandlerInfoCount - number signal handl info
*          signalHandlerFunction  - signal handler function
*          signalHandlerUserData  - signal handler user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Stacktrace_init(const SignalHandlerInfo *signalHandlerInfo,
                     uint                    signalHandlerInfoCount,
                     SignalHandlerFunction   signalHandlerFunction,
                     void                    *signalHandlerUserData
                    );

/***********************************************************************\
* Name   : Stacktrace_done
* Purpose: done stacktrace
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Stacktrace_done(void);

/***********************************************************************\
* Name   : Stacktrace_getSymbols
* Purpose: get symbol information
* Input  : executableFileName - executable name
*          addresses          - addresses
*          addressCount       - number of addresses
* Output : symbolInfo         - symbol info array
*          symbolInfoCount    - length of symbol info array
* Return : -
* Notes  : -
\***********************************************************************/

void Stacktrace_getSymbols(const char         *executableFileName,
                           const void * const addresses[],
                           uint               addressCount,
                           SymbolInfo         *symbolInfo
                          );

/***********************************************************************\
* Name   : Stacktrace_freeSymbols
* Purpose: free symbol information
* Input  : symbolInfo      - symbol info array
*          symbolInfoCount - length of symbol info array
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Stacktrace_freeSymbols(SymbolInfo *symbolInfo,
                            uint       symbolInfoCount
                           );


/***********************************************************************\
* Name   : Stacktrace_getSymbolInfo
* Purpose: get symbol information
* Input  : executableFileName     - executable name
*          addresses              - addresses
*          addressCount           - number of addresses
*          symbolFunction         - callback function for symbol
*          symbolUserData         - callback user data
*          printErrorMessagesFlag - TRUE to print error messages on
*                                   stderr
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Stacktrace_getSymbolInfo(const char         *executableFileName,
                              const void * const addresses[],
                              uint               addressCount,
                              SymbolFunction     symbolFunction,
                              void               *symbolUserData,
                              bool               printErrorMessagesFlag
                             );

#ifdef __cplusplus
  }
#endif

#endif /* __STACKTRACES__ */

/* end of file */
