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
#include <assert.h>

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/**************************** Datatypes ********************************/

/**************************** Variables ********************************/

/****************************** Macros *********************************/

/**************************** Functions ********************************/

// callback for symbol information
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
                              uint       lineNb,
                              void       *userData
                             );

/***********************************************************************\
* Name   : getSymbolInfo
* Purpose: get symbol information
* Input  : executableFileName - executable name
*          addresses          - addresses
*          addressCount       - number of addresses
*          symbolFunction     - callback function for symbol
*          symbolUserData     - callback user data
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Stacktrace_getSymbolInfo(const char     *executableFileName,
                              const void     *addresses[],
                              uint           addressCount,
                              SymbolFunction symbolFunction,
                              void           *symbolUserData
                             );

#endif /* __STACKTRACES__ */

/* end of file */
