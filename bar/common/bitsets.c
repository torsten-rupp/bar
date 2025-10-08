/***********************************************************************\
*
* Contents: BitSet functions
* Systems: all
*
\***********************************************************************/
#define __BITSETS_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "common/global.h"

#include "bitsets.h"

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

bool BitSet_init(BitSet *bitSet, size_t size)
{
  assert(bitSet != NULL);

  bitSet->size = size;
  bitSet->data = (byte*)calloc((size + 8 - 1) / 8,1);
  if (bitSet->data == NULL)
  {
    #ifdef HALT_ON_INSUFFICIENT_MEMORY
      HALT_INSUFFICIENT_MEMORY();
    #else /* not HALT_ON_INSUFFICIENT_MEMORY */
      return FALSE;
    #endif /* HALT_ON_INSUFFICIENT_MEMORY */
  }

  return TRUE;
}

void BitSet_done(BitSet *bitSet)
{
  assert(bitSet != NULL);

  free(bitSet->data);
}

BitSet* BitSet_new(size_t size)
{
  BitSet *bitSet = (BitSet*)malloc(sizeof(BitSet));
  if (bitSet != NULL)
  {
    if (!BitSet_init(bitSet,size))
    {
      #ifdef HALT_ON_INSUFFICIENT_MEMORY
        HALT_INSUFFICIENT_MEMORY();
      #else /* not HALT_ON_INSUFFICIENT_MEMORY */
        free(bitSet);
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

  return bitSet;
}

void BitSet_delete(BitSet *bitSet)
{
  assert(bitSet != NULL);
  assert(bitSet->data != NULL);

  free(bitSet->data);
  free(bitSet);
}

void BitSet_setAll(BitSet *bitSet)
{
  assert(bitSet != NULL);
  assert(bitSet->data != NULL);

  memFill(bitSet->data,(bitSet->size + 8 - 1) / 8,0xFF);
}

void BitSet_clearAll(BitSet *bitSet)
{
  assert(bitSet != NULL);
  assert(bitSet->data != NULL);

  memFill(bitSet->data,(bitSet->size + 8 - 1) / 8,0x00);
}

void BitSet_set(BitSet *bitSet, size_t n, size_t length)
{
  assert(bitSet != NULL);
  assert(bitSet->data != NULL);
  assert((n+length) <= bitSet->size);

  /*

  example: n=3, length=7
  
  i0     b0       i1   b1
  +---------------+-----------------+
  | | | |X|X|X|X|X|X|X| | | | | | | |
  +---------------+-----------------+
   0 1 2 3 4 5 6 7 0 1 2  

  */

  size_t i0 = n / 8;           // first byte
  size_t i1 = (n+length) / 8;  // first bit to set
  size_t b0 = n % 8;           // last byte
  size_t b1 = (n+length) % 8;  // first bite after set
//fprintf(stderr,"%s:%d: n=%d length=%d i0=%d b0=%d i1=%d b1=%d\n",__FILE__,__LINE__,n,length,i0,b0,i1,b1);
  if (i1 > i0)
  {
    if (b0 > 0)
    {
      // fill start byte
      for (size_t i = b0; i < 8; i++)
      {
        bitSet->data[i0] |= (1 << (i % 8));
      }
      
      // fill full bytes
      memset(&bitSet->data[i0+1],0xFF,i1-i0-1);
    }
    else
    {
      // fill full bytes
      memset(&bitSet->data[i0],0xFF,i1-i0);
    }
    
    // fill end byte
    for (size_t i = 0; i < b1; i++)
    {
      bitSet->data[i1] |= (1 << (i % 8));
    }

#if 0
    for (size_t i = 0; i < (bitSet->size+7)/8; i++)
    {
    printf(" %02x",bitSet->data[i]);
    }
    printf("\n");
#endif
  }
  else
  {
    for (size_t i = n; i < (n+length); i++)
    {
      bitSet->data[i / 8] |= (1 << (i % 8));
    }
  }
}

void BitSet_clear(BitSet *bitSet, size_t n, size_t length)
{
  assert(bitSet != NULL);
  assert(bitSet->data != NULL);
  assert((n+length) <= bitSet->size);

  /*

  example: n=3, length=7
  
  i0     b0       i1   b1
  +---------------+-----------------+
  | | | |X|X|X|X|X|X|X| | | | | | | |
  +---------------+-----------------+
   0 1 2 3 4 5 6 7 0 1 2  

  */

  size_t i0 = n / 8;           // first byte
  size_t i1 = (n+length) / 8;  // first bit to clear
  size_t b0 = n % 8;           // last byte
  size_t b1 = (n+length) % 8;  // first bite after clear
//fprintf(stderr,"%s:%d: n=%d length=%d i0=%d b0=%d i1=%d b1=%d\n",__FILE__,__LINE__,n,length,i0,b0,i1,b1);
  if (i1 > i0)
  {
    if (b0 > 0)
    {
      // fill start byte
      for (size_t i = b0; i < 8; i++)
      {
        bitSet->data[i0] &= ~(1 << (i % 8));
      }
      
      // fill full bytes
      memset(&bitSet->data[i0+1],0x00,i1-i0-1);
    }
    else
    {
      // fill full bytes
      memset(&bitSet->data[i0],0x00,i1-i0);
    }

    // fill end byte
    for (size_t i = 0; i < b1; i++)
    {
      bitSet->data[i1] &= ~(1 << (i % 8));
    }
#if 0
    for (size_t i = 0; i < (bitSet->size+7)/8; i++)
    {
    printf(" %02x",bitSet->data[i]);
    }
    printf("\n");
#endif
  }
  else
  {
    for (size_t i = n; i < (n+length); i++)
    {
      bitSet->data[i / 8] &= ~(1 << (i % 8));
    }
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
