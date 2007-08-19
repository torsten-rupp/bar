/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/compress.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: backup archive compress functions
* Systems : all
*
\***********************************************************************/

#ifndef __COMPRESS__
#define __COMPRESS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "bar.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct
{
  uint blockLength;

  void *buffer;
  uint bufferIndex;
  uint bufferLength;
} CompressInfo;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : Compress_init
* Purpose: initialize compress functions
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_init(void);

/***********************************************************************\
* Name   : Compress_done
* Purpose: deinitialize compress functions
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Compress_done(void);

/***********************************************************************\
* Name   : Compress_new
* Purpose: create new compress handle
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_new(CompressInfo *compressInfo,
                    uint         blockLength
                   );

/***********************************************************************\
* Name   : Compress_delete
* Purpose: delete compress handle
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

void Compress_delete(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : Compress_deflate
* Purpose: deflate data
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_deflate(CompressInfo *compressInfo,
                        byte         data
                       );

/***********************************************************************\
* Name   : Compress_inflate
* Purpose: inflate data
* Input  : -
* Output : -
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

Errors Compress_inflate(CompressInfo *compressInfo,
                        byte         *data
                       );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Compress_checkBlockIsFull(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Compress_checkBlockIsEmpty(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool Compress_checkEndOfBlock(CompressInfo *compressInfo);

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Compress_getBlock(CompressInfo *compressInfo,
                       void         *buffer,
                       ulong        *bufferLength
                      );

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void Compress_putBlock(CompressInfo *compressInfo,
                       void         *buffer,
                       ulong        bufferLength
                      );

#ifdef __cplusplus
  }
#endif

#endif /* __CRYPT__ */

/* end of file */
