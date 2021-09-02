/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: Bitmap functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"

#include "bitmaps.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

bool Bitmap_init(Bitmap *bitmap, uint64 size)
{
  assert(bitmap != NULL);

  bitmap->size = size;
  bitmap->data = (byte*)malloc((size+8-1)/8);
  if (bitmap->data == NULL)
  {
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return FALSE;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }

  return TRUE;
}

void Bitmap_done(Bitmap *bitmap)
{
  assert(bitmap != NULL);

  free(bitmap->data);
}

Bitmap* Bitmap_new(uint64 size)
{
  Bitmap *bitmap;

  bitmap = (Bitmap*)malloc(sizeof(Bitmap));
  if (bitmap != NULL)
  {
    if (!Bitmap_init(bitmap,size))
    {
      #ifdef HALT_ON_INSUFFICIENT_MEMORY
        HALT_INSUFFICIENT_MEMORY();
      #else /* not HALT_ON_INSUFFICIENT_MEMORY */
        free(bitmap);
        return NULL;
      #endif /* HALT_ON_INSUFFICIENT_MEMORY */
    }
  }
  else
  {
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return NULL;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }

  return bitmap;
}

void Bitmap_delete(Bitmap *bitmap)
{
  assert(bitmap != NULL);
  assert(bitmap->data != NULL);

  free(bitmap->data);
  free(bitmap);
}

void Bitmap_clear(Bitmap *bitmap)
{
  assert(bitmap != NULL);
  assert(bitmap->data != NULL);

  memClear(bitmap->data,(bitmap->size+8-1)/8);
}

void Bitmap_set(Bitmap *bitmap, uint64 n)
{
  uint64 index;
  int    bit;

  assert(bitmap != NULL);
  assert(bitmap->data != NULL);

  if (n < bitmap->size)
  {
    index = bitmap->size/8LL;
    bit   = (int)(bitmap->size%8LL);
    bitmap->data[index] |= (1 << bit);
  }
}

void Bitmap_reset(Bitmap *bitmap, uint64 n)
{
  uint64 index;
  int    bit;

  assert(bitmap != NULL);
  assert(bitmap->data != NULL);

  if (n < bitmap->size)
  {
    index = bitmap->size/8LL;
    bit   = (int)(bitmap->size%8LL);
    bitmap->data[index] &= ~(1 << bit);
  }
}

bool Bitmap_get(Bitmap *bitmap, uint64 n)
{
  uint64 index;
  int    bit;

  assert(bitmap != NULL);
  assert(bitmap->data != NULL);

  if (n < bitmap->size)
  {
    index = bitmap->size/8LL;
    bit   = (int)(bitmap->size%8LL);
    return (bitmap->data[index] & (1 << bit)) != 0;
  }
  else
  {
    return FALSE;
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
