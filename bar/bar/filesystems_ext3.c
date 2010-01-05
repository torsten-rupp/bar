/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/bar/filesystems_ext3.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: Backup ARchiver EXT2 file system plug in
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/
typedef struct
{
} EXT3Handle;

/***************************** Variables *******************************/

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

LOCAL bool EXT3_init(DeviceHandle *deviceHandle, EXT3Handle *ext3Handle)
{
UNUSED_VARIABLE(deviceHandle);
UNUSED_VARIABLE(ext3Handle);
  return FALSE;
}

LOCAL void EXT3_done(DeviceHandle *deviceHandle, EXT3Handle *ext3Handle)
{
UNUSED_VARIABLE(deviceHandle);
UNUSED_VARIABLE(ext3Handle);
}

LOCAL bool EXT3_blockIsUsed(DeviceHandle *deviceHandle, EXT3Handle *ext3Handle, uint64 blockOffset)
{
UNUSED_VARIABLE(deviceHandle);
UNUSED_VARIABLE(ext3Handle);
UNUSED_VARIABLE(blockOffset);
  return FALSE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
