/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/bitmaps.h,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Bitmap functions
* Systems: all
*
\***********************************************************************/

#ifndef __BITMAPS__
#define __BITMAPS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct
{
  ulong size;
  byte  *data;
} Bitmap;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

bool Bitmap_init(Bitmap *bitmap, uint64 size);

void Bitmap_done(Bitmap *bitmap);

Bitmap* Bitmap_new(uint64 size);

void Bitmap_delete(Bitmap *bitmap);

void Bitmap_clear(Bitmap *bitmap);

void Bitmap_set(Bitmap *bitmap, uint64 n);

void Bitmap_reset(Bitmap *bitmap, uint64 n);

bool Bitmap_get(Bitmap *bitmap, uint64 n);

#ifdef __cplusplus
  }
#endif

#endif /* __BITMAPS__ */

/* end of file */
