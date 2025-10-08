/***********************************************************************\
*
* Contents: BitSet functions
* Systems: all
*
\***********************************************************************/

#ifndef __BITSETS__
#define __BITSETS__

/****************************** Includes *******************************/
#include <stdlib.h>
#include <assert.h>

#include "common/global.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

typedef struct
{
  size_t size;
  byte   *data;
} BitSet;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : BitSet_init
* Purpose: initialize bit set
* Input  : bitSet - bit set variable
*          size   - size [bit]
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

bool BitSet_init(BitSet *bitSet, size_t size);

/***********************************************************************\
* Name   : BitSet_done
* Purpose: deinitialize bit set
* Input  : bitSet - bit set variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void BitSet_done(BitSet *bitSet);

/***********************************************************************\
* Name   : BitSet_new
* Purpose: create new bit set
* Input  : bitSet - bit set variable
*          size   - size [bit]
* Output : -
* Return : bit set
* Notes  : -
\***********************************************************************/

BitSet* BitSet_new(size_t size);

/***********************************************************************\
* Name   : BitSet_delete
* Purpose: delete bit set
* Input  : bitSet - bit set variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void BitSet_delete(BitSet *bitSet);

/***********************************************************************\
* Name   : BitSet_setAll
* Purpose: set all bits in bit set
* Input  : bitSet - bit set variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void BitSet_setAll(BitSet *bitSet);

/***********************************************************************\
* Name   : BitSet_clearAll
* Purpose: clear all bits in bit set
* Input  : bitSet - bit set variable
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void BitSet_clearAll(BitSet *bitSet);

/***********************************************************************\
* Name   : BitSet_set
* Purpose: set bits in bit set
* Input  : bitSet - bit set variable
*          n      - start bit
*          length - number of bits to set
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void BitSet_set(BitSet *bitSet, size_t n, size_t length);

/***********************************************************************\
* Name   : BitSet_clear
* Purpose: clear bits in bit set
* Input  : bitSet - bit set variable
*          n      - start bit
*          length - number of bits to clear
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

void BitSet_clear(BitSet *bitSet, size_t n, size_t length);

/***********************************************************************\
* Name   : BitSet_isSet
* Purpose: check if bit is set
* Input  : bitSet - bit set variable
*          bit    - bit to test
* Output : -
* Return : TRUE iff bit is set
* Notes  : -
\***********************************************************************/

INLINE bool BitSet_isSet(const BitSet *bitSet, size_t bit);
#if defined(NDEBUG) || defined(__BITSETS_IMPLEMENTATION__)
INLINE bool BitSet_isSet(const BitSet *bitSet, size_t bit)
{
  assert(bitSet != NULL);
  assert(bitSet->data != NULL);

  bool isSet;
  if (bit < bitSet->size)
  {
    isSet = ((bitSet->data[bit / 8] & (1 << (bit % 8))) != 0);
  }
  else
  {
    isSet = FALSE;
  }

  return isSet;
}
#endif // defined(NDEBUG) || defined(__BITSETS_IMPLEMENTATION__)

/***********************************************************************\
* Name   : BitSet_isCleared
* Purpose: check if bit is cleared
* Input  : bitSet - bit set variable
*          bit    - bit to test
* Output : -
* Return : TRUE iff bit is cleared
* Notes  : -
\***********************************************************************/

INLINE bool BitSet_isCleared(const BitSet *bitSet, size_t bit);
#if defined(NDEBUG) || defined(__BITSETS_IMPLEMENTATION__)
INLINE bool BitSet_isCleared(const BitSet *bitSet, size_t bit)
{
  assert(bitSet != NULL);
  assert(bitSet->data != NULL);

  bool isCleared;
  if (bit < bitSet->size)
  {
    isCleared = ((bitSet->data[bit / 8] & (1 << (bit % 8))) == 0);
  }
  else
  {
    isCleared = TRUE;
  }

  return isCleared;
}
#endif // defined(NDEBUG) || defined(__BITSETS_IMPLEMENTATION__)

#ifdef __cplusplus
  }
#endif

#endif /* __BITSETS__ */

/* end of file */
