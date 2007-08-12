/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/archive.c,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: 
* Systems :
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "lists.h"

#include "bar.h"
#include "archive_format.h"
#include "chunks.h"
#include "files.h"

#include "archive.h"

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

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool endOfFile(void *userData)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  return files_eof(&archiveInfo->fileHandle);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool readFile(void *userData, void *buffer, ulong bufferLength)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  return (files_read(&archiveInfo->fileHandle,buffer,bufferLength) == ERROR_NONE);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool writeFile(void *userData, const void *buffer, ulong bufferLength)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  return (files_write(&archiveInfo->fileHandle,buffer,bufferLength) == ERROR_NONE);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool tellFile(void *userData, uint64 *offset)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);
  assert(offset != NULL);

  return (files_tell(&archiveInfo->fileHandle,offset) == ERROR_NONE);
}

/***********************************************************************\
* Name   : 
* Purpose: 
* Input  : -
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL bool seekFile(void *userData, uint64 offset)
{
  ArchiveInfo *archiveInfo = (ArchiveInfo*)userData;

  assert(archiveInfo != NULL);

  return (files_seek(&archiveInfo->fileHandle,offset) == ERROR_NONE);
}

/*---------------------------------------------------------------------*/

Errors archive_create(ArchiveInfo *archiveInfo,
                      const char  *archiveFileName,
                      uint64      partSize
                     )
{
  assert(archiveInfo != NULL);
  assert(archiveFileName != NULL);

  if (!chunks_initF(endOfFile,
                   readFile,
                   writeFile,
                   tellFile,
                   seekFile
                  )
     )
  {
    return ERROR_INIT;
  }

  archiveInfo->fileName          = String_newCString(archiveFileName); 
  archiveInfo->partSize          = partSize;                           

  archiveInfo->partNumber        = 0;
  archiveInfo->fileHandle.handle = -1;
  archiveInfo->fileHandle.size   = 0;

  return ERROR_NONE;
}

Errors archive_open(ArchiveInfo *archiveInfo,
                    const char  *archiveFileName
                   )
{
  Errors error;

  assert(archiveInfo != NULL);
  assert(archiveFileName != NULL);

  if (!chunks_initF(endOfFile,
                   readFile,
                   writeFile,
                   tellFile,
                   seekFile
                  )
     )
  {
    return ERROR_INIT;
  }

  /* init */
  archiveInfo->fileName          = String_newCString(archiveFileName);
  archiveInfo->partSize          = 0;
  archiveInfo->partNumber        = 0;
  archiveInfo->fileHandle.handle = -1;
  archiveInfo->fileHandle.size   = 0;

  /* open file */
  error = files_open(&archiveInfo->fileHandle,archiveInfo->fileName);
  if (error != ERROR_NONE)
  {
    return error;
  }

  return ERROR_NONE;
}

bool archive_eof(ArchiveInfo *archiveInfo)
{
  assert(archiveInfo != NULL);

  return files_eof(&archiveInfo->fileHandle);
}

Errors archive_done(ArchiveInfo *archiveInfo)
{
  assert(archiveInfo != NULL);
  assert(archiveInfo->fileName != NULL);

  String_delete(archiveInfo->fileName);

  chunks_doneF();

  return ERROR_NONE;
}

Errors archive_newFile(ArchiveInfo     *archiveInfo,
                       ArchiveFileInfo *archiveFileInfo,
                       const FileInfo  *fileInfo
                      )
{
  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo                    = archiveInfo;
  archiveFileInfo->mode                           = FILE_MODE_WRITE;

  archiveFileInfo->chunkFileEntry.fileType        = 0;
  archiveFileInfo->chunkFileEntry.size            = fileInfo->size;
  archiveFileInfo->chunkFileEntry.timeLastAccess  = fileInfo->timeLastAccess;
  archiveFileInfo->chunkFileEntry.timeModified    = fileInfo->timeModified;
  archiveFileInfo->chunkFileEntry.timeLastChanged = fileInfo->timeLastChanged;
  archiveFileInfo->chunkFileEntry.userId          = fileInfo->userId;
  archiveFileInfo->chunkFileEntry.groupId         = fileInfo->groupId;
  archiveFileInfo->chunkFileEntry.permission      = fileInfo->permission;
  archiveFileInfo->chunkFileEntry.name            = String_copy(fileInfo->name);

  archiveFileInfo->chunkFileData.partOffset       = 0;
  archiveFileInfo->chunkFileData.partSize         = 0;

  archiveFileInfo->headerLength                   = 0;
  archiveFileInfo->headerWrittenFlag              = FALSE;

  /* init file-chunks */
  if (!chunks_init(&archiveFileInfo->chunkInfoFile,
                   NULL,
                   archiveInfo,
                   CHUNK_ID_FILE,
                   CHUNK_DEFINITION_FILE
                  )
     )
  {
    return ERROR_IO_ERROR;
  }
  if (!chunks_init(&archiveFileInfo->chunkInfoFileEntry,
                   &archiveFileInfo->chunkInfoFile,
                   archiveInfo,
                   CHUNK_ID_FILE_ENTRY,
                   CHUNK_DEFINITION_FILE_ENTRY
                  )
     )
  {
    chunks_done(&archiveFileInfo->chunkInfoFile);
    return ERROR_IO_ERROR;
  }
  if (!chunks_init(&archiveFileInfo->chunkInfoFileData,
                   &archiveFileInfo->chunkInfoFile,
                   archiveInfo,
                   CHUNK_ID_FILE_DATA,
                   CHUNK_DEFINITION_FILE_DATA
                  )
     )
  {
    chunks_done(&archiveFileInfo->chunkInfoFileEntry);
    chunks_done(&archiveFileInfo->chunkInfoFile);
    return ERROR_IO_ERROR;
  }

  /* calculate header length */
  archiveFileInfo->headerLength = chunks_getSize(&archiveFileInfo->chunkInfoFile,     &archiveFileInfo->chunkFile)+
                                  chunks_getSize(&archiveFileInfo->chunkInfoFileEntry,&archiveFileInfo->chunkFileEntry)+
                                  chunks_getSize(&archiveFileInfo->chunkInfoFileData, &archiveFileInfo->chunkFileData);

  return ERROR_NONE;
}

Errors archive_readFile(ArchiveInfo     *archiveInfo,
                        ArchiveFileInfo *archiveFileInfo,
                        FileInfo        *fileInfo,
                        uint64          *partOffset,
                        uint64          *partSize
                       )
{
  ChunkHeader chunkHeader;
  bool        foundFileEntry,foundFileData;

  assert(archiveInfo != NULL);
  assert(archiveFileInfo != NULL);
  assert(fileInfo != NULL);

  /* init archive file info */
  archiveFileInfo->archiveInfo = archiveInfo;
  archiveFileInfo->mode        = FILE_MODE_READ;

  /* init file-chunk */
  if (!chunks_init(&archiveFileInfo->chunkInfoFile,
                   NULL,
                   archiveInfo,
                   CHUNK_ID_FILE,
                   CHUNK_DEFINITION_FILE
                  )
     )
  {
    return ERROR_IO_ERROR;
  }
  if (!chunks_init(&archiveFileInfo->chunkInfoFileEntry,
                   &archiveFileInfo->chunkInfoFile,
                   archiveInfo,
                   CHUNK_ID_FILE_ENTRY,
                   CHUNK_DEFINITION_FILE_ENTRY
                  )
     )
  {
    chunks_done(&archiveFileInfo->chunkInfoFile);
    return ERROR_IO_ERROR;
  }
  if (!chunks_init(&archiveFileInfo->chunkInfoFileData,
                   &archiveFileInfo->chunkInfoFile,
                   archiveInfo,
                   CHUNK_ID_FILE_DATA,
                   CHUNK_DEFINITION_FILE_DATA
                  )
     )
  {
    chunks_done(&archiveFileInfo->chunkInfoFileEntry);
    chunks_done(&archiveFileInfo->chunkInfoFile);
    return ERROR_IO_ERROR;
  }

  /* find file chunk */
  do
  {
    if (chunks_eof(archiveInfo))
    {
      chunks_done(&archiveFileInfo->chunkInfoFileData);
      chunks_done(&archiveFileInfo->chunkInfoFileEntry);
      chunks_done(&archiveFileInfo->chunkInfoFile);
      return ERROR_END_OF_ARCHIVE;
    }

    if (!chunks_next(archiveInfo,&chunkHeader))
    {
      chunks_done(&archiveFileInfo->chunkInfoFileData);
      chunks_done(&archiveFileInfo->chunkInfoFileEntry);
      chunks_done(&archiveFileInfo->chunkInfoFile);
      return ERROR_IO_ERROR;
    }

    if (chunkHeader.id != CHUNK_ID_FILE)
    {
      chunks_skip(archiveInfo,&chunkHeader);
      continue;
    }
  }
  while (chunkHeader.id != CHUNK_ID_FILE);

  /* read file chunk, find file data */
  if (!chunks_open(&archiveFileInfo->chunkInfoFile,
                   &chunkHeader,
                   &archiveFileInfo->chunkFile
                  )
     )
  {
    return ERROR_IO_ERROR;
  }
  foundFileEntry = FALSE;
  foundFileData  = FALSE;
  while (   !chunks_eofSub(&archiveFileInfo->chunkInfoFile)
         && (!foundFileEntry || !foundFileData)         
        )
  {
    if (!chunks_nextSub(&archiveFileInfo->chunkInfoFile,&chunkHeader))
    {
      chunks_done(&archiveFileInfo->chunkInfoFileData);
      chunks_done(&archiveFileInfo->chunkInfoFileEntry);
      chunks_done(&archiveFileInfo->chunkInfoFile);
      return ERROR_IO_ERROR;
    }

    switch (chunkHeader.id)
    {
      case CHUNK_ID_FILE_ENTRY:
        if (!chunks_open(&archiveFileInfo->chunkInfoFileEntry,
                         &chunkHeader,
                         &archiveFileInfo->chunkFileEntry
                        )
           )
        {
          chunks_done(&archiveFileInfo->chunkInfoFileData);
          chunks_done(&archiveFileInfo->chunkInfoFileEntry);
          chunks_done(&archiveFileInfo->chunkInfoFile);
          return ERROR_IO_ERROR;
        }

        String_set(fileInfo->name,archiveFileInfo->chunkFileEntry.name);
        fileInfo->size            = archiveFileInfo->chunkFileEntry.size;
        fileInfo->timeLastAccess  = archiveFileInfo->chunkFileEntry.timeLastAccess;
        fileInfo->timeModified    = archiveFileInfo->chunkFileEntry.timeModified;
        fileInfo->timeLastChanged = archiveFileInfo->chunkFileEntry.timeLastChanged;
        fileInfo->userId          = archiveFileInfo->chunkFileEntry.userId;
        fileInfo->groupId         = archiveFileInfo->chunkFileEntry.groupId;
        fileInfo->permission      = archiveFileInfo->chunkFileEntry.permission;

        foundFileEntry = TRUE;
        break;
      case CHUNK_ID_FILE_DATA:
        if (!chunks_open(&archiveFileInfo->chunkInfoFileData,
                         &chunkHeader,
                         &archiveFileInfo->chunkFileData
                        )
           )
        {
          if (foundFileEntry) String_delete(archiveFileInfo->chunkFileEntry.name);
          chunks_done(&archiveFileInfo->chunkInfoFileData);
          chunks_done(&archiveFileInfo->chunkInfoFileEntry);
          chunks_done(&archiveFileInfo->chunkInfoFile);
          return ERROR_IO_ERROR;
        }

        if (partOffset != NULL) (*partOffset) = archiveFileInfo->chunkFileData.partOffset;
        if (partSize   != NULL) (*partSize)   = archiveFileInfo->chunkFileData.partSize;

        foundFileData = TRUE;
        break;
      default:
        chunks_skipSub(&archiveFileInfo->chunkInfoFile,&chunkHeader);
        break;
    }
  }
  if (!foundFileEntry)
  {
    if (foundFileEntry) String_delete(archiveFileInfo->chunkFileEntry.name);
    chunks_done(&archiveFileInfo->chunkInfoFileData);
    chunks_done(&archiveFileInfo->chunkInfoFileEntry);
    chunks_done(&archiveFileInfo->chunkInfoFile);
    return ERROR_NO_FILE_ENTRY;
  }
  if (!foundFileData)
  {
    if (foundFileEntry) String_delete(archiveFileInfo->chunkFileEntry.name);
    chunks_done(&archiveFileInfo->chunkInfoFileData);
    chunks_done(&archiveFileInfo->chunkInfoFileEntry);
    chunks_done(&archiveFileInfo->chunkInfoFile);
    return ERROR_NO_FILE_DATA;
  }

  return ERROR_NONE;
}

Errors archive_closeFile(ArchiveFileInfo *archiveFileInfo)
{
  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);

  /* update data */
  switch (archiveFileInfo->mode)
  {
    case FILE_MODE_WRITE:
      /* update part offset, part size */
      if (!chunks_update(&archiveFileInfo->chunkInfoFileData,
                         &archiveFileInfo->chunkFileData
                        )
         )
      {
        return ERROR_IO_ERROR;
      }
      break;
    case FILE_MODE_READ:
      break;
  }

  /* close chunks */
  if (!chunks_close(&archiveFileInfo->chunkInfoFileData))
  {
    return ERROR_IO_ERROR;
  }
  if (!chunks_close(&archiveFileInfo->chunkInfoFileEntry))
  {
    return ERROR_IO_ERROR;
  }
  if (!chunks_close(&archiveFileInfo->chunkInfoFile))
  {
    return ERROR_IO_ERROR;
  }
//  archiveFileInfo->archiveInfo->index               = archiveFileInfo->archiveInfo->chunkInfoData.index;
  archiveFileInfo->headerWrittenFlag = FALSE;

  /* free resources */
  chunks_done(&archiveFileInfo->chunkInfoFileData);
  chunks_done(&archiveFileInfo->chunkInfoFileEntry);
  chunks_done(&archiveFileInfo->chunkInfoFile);
  String_delete(archiveFileInfo->chunkFileEntry.name);

  return ERROR_NONE;
}

Errors archive_writeFileData(ArchiveFileInfo *archiveFileInfo, const void *buffer, ulong bufferLength)
{
  const char *p;
  ulong      length;
  uint64     size;
  bool       newPartFlag;
  ulong      restLength;
  ulong      partLength;
  ulong      n;
  String     fileName;
  Errors     error;
  void       *writeBuffer;
  ulong      writeLength;

  assert(archiveFileInfo != NULL);
  assert(archiveFileInfo->archiveInfo != NULL);

  p      = (char*)buffer;
  length = 0;
  while (length < bufferLength)
  {
    /* split, calculate rest-length */
    if (archiveFileInfo->archiveInfo->partSize > 0)
    {
      /* check if new file-part is needed */
      newPartFlag = FALSE;
      if (archiveFileInfo->archiveInfo->fileHandle.handle == -1)
      {
        /* not open -> new part */
        newPartFlag = TRUE;
      }
      else
      {
        /* get current size */
        if (!tellFile(archiveFileInfo->archiveInfo,&size))
        {
          return ERROR_IO_ERROR;
        }
        
        if      (   !archiveFileInfo->headerWrittenFlag
                 && (size + archiveFileInfo->headerLength >= archiveFileInfo->archiveInfo->partSize)
                )
        {
          /* file header cannot be written -> new part */
          newPartFlag = TRUE;
        }
        else if (size >= archiveFileInfo->archiveInfo->partSize)
        {
          /* part is full -> new part */
          newPartFlag = TRUE;
        }
      }
      if (newPartFlag)
      {
        if (archiveFileInfo->archiveInfo->fileHandle.handle >= 0)
        {
          /* close file, prepare for next part */
          if (archiveFileInfo->headerWrittenFlag)
          {
            /* update part offset, part size */
            if (!chunks_update(&archiveFileInfo->chunkInfoFileData,
                               &archiveFileInfo->chunkFileData
                              )
               )
            {
              return ERROR_IO_ERROR;
            }

            /* close chunks */
            if (!chunks_close(&archiveFileInfo->chunkInfoFileData))
            {
              return ERROR_IO_ERROR;
            }
            if (!chunks_close(&archiveFileInfo->chunkInfoFileEntry))
            {
              return ERROR_IO_ERROR;
            }
            if (!chunks_close(&archiveFileInfo->chunkInfoFile))
            {
              return ERROR_IO_ERROR;
            }
            archiveFileInfo->headerWrittenFlag = FALSE;
          }

          files_close(&archiveFileInfo->archiveInfo->fileHandle);
          archiveFileInfo->archiveInfo->fileHandle.handle = -1;
        }
      }

      /* get size of space to reserve for chunk-header */
      if (!archiveFileInfo->headerWrittenFlag || newPartFlag)
      {
        n = archiveFileInfo->headerLength;
      }
      else
      {
        n = 0;
      }

      /* calculate max. length of data which can be written into this part */
      restLength = archiveFileInfo->archiveInfo->partSize-(size+n);

      /* calculate length of data to write */
      partLength = (restLength < (bufferLength-length))?restLength:bufferLength-length;
    }
    else
    {
      partLength = bufferLength - length;
    }

    /* compress */
writeBuffer=(char*)buffer;
writeLength=partLength;
    if (1)
    {
    }

    /* encrypt */
    if (0)
    {
    }

    /* open file */
    if (archiveFileInfo->archiveInfo->fileHandle.handle == -1)
    {
      /* get output filename */
      if (archiveFileInfo->archiveInfo->partSize > 0)
      {
        fileName = String_format(String_new(),"%S.%06d",archiveFileInfo->archiveInfo->fileName,archiveFileInfo->archiveInfo->partNumber);
        archiveFileInfo->archiveInfo->partNumber++;
      }
      else
      {
        fileName = String_copy(archiveFileInfo->archiveInfo->fileName);
      }

      /* create file */
      error = files_create(&archiveFileInfo->archiveInfo->fileHandle,fileName);
      if (error != ERROR_NONE)
      {
        return error;
      }

//      archiveFileInfo->archiveInfo->index               = 0;
//      archiveFileInfo->archiveInfo->size                = 0;
      archiveFileInfo->headerWrittenFlag = FALSE;

      String_delete(fileName);
    }

    /* write chunk-header */
    if (!archiveFileInfo->headerWrittenFlag)
    {
      if (!chunks_new(&archiveFileInfo->chunkInfoFile,
                      NULL
                     )
         )
      {
        return ERROR_IO_ERROR;
      }
//      archiveFileInfo->archiveInfo->size += CHUNK_HEADER_SIZE;

      if (!chunks_new(&archiveFileInfo->chunkInfoFileEntry,
                      &archiveFileInfo->chunkFileEntry
                     )
         )
      {
        return ERROR_IO_ERROR;
      }
//      archiveFileInfo->archiveInfo->size += chunks_getSize(&archiveFileInfo->chunkInfoFileEntry,&chunkFileEntry);

      if (!chunks_new(&archiveFileInfo->chunkInfoFileData,
                      &archiveFileInfo->chunkFileData
                     )
         )
      {
        return ERROR_IO_ERROR;
      }
//      archiveFileInfo->archiveInfo->size += chunks_getSize(&archiveFileInfo->chunkInfoFileData,&chunkFileData);

      archiveFileInfo->chunkFileData.partOffset = archiveFileInfo->chunkFileData.partOffset+archiveFileInfo->chunkFileData.partSize;
      archiveFileInfo->chunkFileData.partSize   = 0;

      archiveFileInfo->headerWrittenFlag = TRUE;
    }

    /* write */
    if (!chunks_writeData(&archiveFileInfo->chunkInfoFileData,writeBuffer,writeLength))
    {
      return ERROR_IO_ERROR;
    }
//    archiveFileInfo->archiveInfo->size += writeLength;
    archiveFileInfo->chunkFileData.partSize += partLength;

    length += partLength;
  }

  return ERROR_NONE;
}

Errors archive_readFileData(ArchiveFileInfo *archiveFileInfo, void *buffer, ulong bufferLength)
{
  assert(archiveFileInfo != NULL);

  return ERROR_NONE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
