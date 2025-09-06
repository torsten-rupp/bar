/***********************************************************************\
*
* Contents: Backup ARchiver common functions
* Systems: all
*
\***********************************************************************/

#define __BAR_COMMON_IMPLEMENTATION__

/****************************** Includes *******************************/
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef HAVE_ICU
  #include <unicode/utypes.h>
  #include <unicode/ustring.h>
  #include <unicode/uclean.h>
  #include <unicode/ucnv.h>
  #include <unicode/udat.h>
  #include <unicode/ucal.h>
#endif
#include <assert.h>

#include "forward.h"         // required for JobOptions

#include "common/global.h"
#include "common/cstrings.h"

#include "configuration.h"
#include "archives.h"

#include "bar_common.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

/***************************** Variables *******************************/
String tmpDirectory;

#ifdef HAVE_ICU
  struct
  {
    bool       isInitialized;

    bool       isUTF8System;

    Semaphore  lock;
    UConverter *systemConverter;
    uint8      systemMaxCharSize;
    UConverter *consoleConverter;
    uint8      consoleMaxCharSize;
    UConverter *utf8Converter;
    uint8      utf8MaxCharSize;
    UChar      *unicodeChars;
    size_t     unicodeCharSize;
    char       *buffer;
    size_t     bufferSize;
  } encodingConverter;
#endif

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

/***********************************************************************\
* Name   : convertToUnicode
* Purpose: convert to unicode characters
* Input  : converter - converter to use
*          source    - string in system encoding to convert
* Output : -
* Return : length of unicode string or -1 on error
* Notes  : -
\***********************************************************************/

LOCAL int32_t convertToUnicode(UConverter *converter, ConstString source)
{
  assert(converter != NULL);
  assert(source != NULL);

  // extend Unicode buffer if needed
  if ((2*String_length(source)) > encodingConverter.unicodeCharSize)
  {
    size_t newUnicodeCharsSize = ALIGN(2*String_length(source),64);
    UChar  *newUnicodeChars    = (UChar*)realloc(encodingConverter.unicodeChars,newUnicodeCharsSize);
    if (newUnicodeChars == NULL)
    {
      return -1;
    }
    encodingConverter.unicodeChars    = newUnicodeChars;
    encodingConverter.unicodeCharSize = newUnicodeCharsSize;
  }

  // convert to Unicode encoding
  UErrorCode errorCode = U_ZERO_ERROR;
  int32_t unicodeLength = ucnv_toUChars(converter,
                                        encodingConverter.unicodeChars,
                                        encodingConverter.unicodeCharSize,
                                        String_cString(source),
                                        String_length(source),
                                        &errorCode
                                       );
  if ((unicodeLength < 0) || !U_SUCCESS(errorCode))
  {
    return -1;
  }
  assert(unicodeLength <= (int32_t)encodingConverter.unicodeCharSize);

  return unicodeLength;
}

/***********************************************************************\
* Name   : convertFromUnicode
* Purpose: convert from unicode characters
* Input  : destination   - destination string
*          converter     - converter to use
*          unicodeLength - unicode length
*          maxCharSize   - convert max. char size
* Output : destination - converted string
* Return : length of string or -1 on error
* Notes  : -
\***********************************************************************/

LOCAL int32_t convertFromUnicode(String destination, UConverter *converter, int32_t unicodeLength, uint8_t maxCharSize)
{
  assert(destination != NULL);
  assert(converter != NULL);

  // extend buffer if needed
  if (encodingConverter.bufferSize < (size_t)UCNV_GET_MAX_BYTES_FOR_STRING(unicodeLength,maxCharSize))
  {
    size_t newBufferSize = ALIGN(UCNV_GET_MAX_BYTES_FOR_STRING(unicodeLength,maxCharSize),64);
    char   *newBuffer    = (char*)realloc(encodingConverter.buffer,newBufferSize);
    if (newBuffer == NULL)
    {
      return -1;
    }
    encodingConverter.buffer     = newBuffer;
    encodingConverter.bufferSize = newBufferSize;
  }

  // convert to destination encoding
  UErrorCode errorCode = U_ZERO_ERROR;
  int32_t length = ucnv_fromUChars(converter,
                                   encodingConverter.buffer,
                                   encodingConverter.bufferSize,
                                   encodingConverter.unicodeChars,
                                   unicodeLength,
                                   &errorCode
                                  );
  if ((length < 0) || !U_SUCCESS(errorCode))
  {
    return -1;
  }
  assert(length <= (int32_t)encodingConverter.bufferSize);

  String_appendBuffer(destination,encodingConverter.buffer,(ulong)length);

  return length;
}

// ---------------------------------------------------------------------

Errors Common_initAll(void)
{
  // init variables
  tmpDirectory = String_new();

  #ifdef HAVE_ICU
    /* init ICU library
       Note: try to load data from executable directory; required for
             Windows 64bit: included data library do not work?
    */
    UErrorCode errorCode = U_ZERO_ERROR;
    String executablePath = Misc_getProgramFilePath(String_new());
    File_getDirectoryName(executablePath,executablePath);
    u_setDataDirectory(String_cString(executablePath));
    u_init(&errorCode);
    String_delete(executablePath);
  #endif

  return ERROR_NONE;
}

void Common_doneAll(void)
{
  #ifdef HAVE_ICU
    // done ICU library
    u_cleanup();
  #endif

  String_delete(tmpDirectory);
}

void initRunningInfo(RunningInfo *runningInfo)
{
  assert(runningInfo != NULL);

  runningInfo->error                        = ERROR_NONE;

  runningInfo->progress.done.count          = 0L;
  runningInfo->progress.done.size           = 0LL;
  runningInfo->progress.total.count         = 0L;
  runningInfo->progress.total.size          = 0LL;
  runningInfo->progress.collectTotalSumDone = FALSE;
  runningInfo->progress.skipped.count       = 0L;
  runningInfo->progress.skipped.size        = 0LL;
  runningInfo->progress.error.count         = 0L;
  runningInfo->progress.error.size          = 0LL;
  runningInfo->progress.archiveSize         = 0LL;
  runningInfo->progress.compressionRatio    = 0.0;
  runningInfo->progress.entry.name          = String_new();
  runningInfo->progress.entry.doneSize      = 0LL;
  runningInfo->progress.entry.totalSize     = 0LL;
  runningInfo->progress.storage.name        = String_new();
  runningInfo->progress.storage.doneSize    = 0LL;
  runningInfo->progress.storage.totalSize   = 0LL;
  runningInfo->progress.volume.number       = 0;
  runningInfo->progress.volume.done         = 0.0;
  runningInfo->message.code                 = MESSAGE_CODE_NONE;
  runningInfo->message.text                 = String_new();

  runningInfo->lastErrorCode                = ERROR_CODE_NONE;
  runningInfo->lastErrorNumber              = 0;
  runningInfo->lastErrorData                = String_new();

  runningInfo->lastExecutedDateTime         = 0LL;

  Misc_performanceFilterInit(&runningInfo->entriesPerSecondFilter,     10*60);
  Misc_performanceFilterInit(&runningInfo->bytesPerSecondFilter,       10*60);
  Misc_performanceFilterInit(&runningInfo->storageBytesPerSecondFilter,10*60);

  runningInfo->entriesPerSecond             = 0.0;
  runningInfo->bytesPerSecond               = 0.0;
  runningInfo->storageBytesPerSecond        = 0.0;
  runningInfo->estimatedRestTime            = 0L;
}

void resetRunningInfo(RunningInfo *runningInfo)
{
  assert(runningInfo != NULL);

  runningInfo->error                        = ERROR_NONE;

  runningInfo->progress.done.count          = 0L;
  runningInfo->progress.done.size           = 0LL;
  runningInfo->progress.total.count         = 0L;
  runningInfo->progress.total.size          = 0LL;
  runningInfo->progress.collectTotalSumDone = FALSE;
  runningInfo->progress.skipped.count       = 0L;
  runningInfo->progress.skipped.size        = 0LL;
  runningInfo->progress.error.count         = 0L;
  runningInfo->progress.error.size          = 0LL;
  runningInfo->progress.archiveSize         = 0LL;
  runningInfo->progress.compressionRatio    = 0.0;
  String_clear(runningInfo->progress.entry.name);
  runningInfo->progress.entry.doneSize      = 0LL;
  runningInfo->progress.entry.totalSize     = 0LL;
  String_clear(runningInfo->progress.storage.name);
  runningInfo->progress.storage.doneSize    = 0LL;
  runningInfo->progress.storage.totalSize   = 0LL;
  runningInfo->progress.volume.number       = 0;
  runningInfo->progress.volume.done         = 0.0;
  runningInfo->message.code                 = MESSAGE_CODE_NONE;
  String_clear(runningInfo->message.text);

  runningInfo->lastErrorCode                = ERROR_CODE_NONE;
  runningInfo->lastErrorNumber              = 0;
  String_clear(runningInfo->lastErrorData);

  runningInfo->lastExecutedDateTime         = 0LL;

  Misc_performanceFilterClear(&runningInfo->entriesPerSecondFilter     );
  Misc_performanceFilterClear(&runningInfo->bytesPerSecondFilter       );
  Misc_performanceFilterClear(&runningInfo->storageBytesPerSecondFilter);

  runningInfo->entriesPerSecond             = 0.0;
  runningInfo->bytesPerSecond               = 0.0;
  runningInfo->storageBytesPerSecond        = 0.0;
  runningInfo->estimatedRestTime            = 0L;
}

void doneRunningInfo(RunningInfo *runningInfo)
{
  assert(runningInfo != NULL);

  Misc_performanceFilterDone(&runningInfo->entriesPerSecondFilter);
  Misc_performanceFilterDone(&runningInfo->bytesPerSecondFilter);
  Misc_performanceFilterDone(&runningInfo->storageBytesPerSecondFilter);

  String_delete(runningInfo->lastErrorData);
  String_delete(runningInfo->message.text);
  String_delete(runningInfo->progress.storage.name);
  String_delete(runningInfo->progress.entry.name);
}

void setRunningInfo(RunningInfo *runningInfo, const RunningInfo *fromRunningInfo)
{
  assert(runningInfo != NULL);
  assert(runningInfo->progress.entry.name != NULL);
  assert(runningInfo->progress.storage.name != NULL);
  assert(runningInfo->message.text != NULL);
  assert(runningInfo->lastErrorData != NULL);
  assert(fromRunningInfo != NULL);
  assert(fromRunningInfo->progress.entry.name != NULL);
  assert(fromRunningInfo->progress.storage.name != NULL);
  assert(fromRunningInfo->message.text != NULL);
  assert(fromRunningInfo->lastErrorData != NULL);

  runningInfo->error                        = fromRunningInfo->error;

  runningInfo->progress.done.count          = fromRunningInfo->progress.done.count;
  runningInfo->progress.done.size           = fromRunningInfo->progress.done.size;
  runningInfo->progress.total.count         = fromRunningInfo->progress.total.count;
  runningInfo->progress.total.size          = fromRunningInfo->progress.total.size;
  runningInfo->progress.collectTotalSumDone = fromRunningInfo->progress.collectTotalSumDone;
  runningInfo->progress.skipped.count       = fromRunningInfo->progress.skipped.count;
  runningInfo->progress.skipped.size        = fromRunningInfo->progress.skipped.size;
  runningInfo->progress.error.count         = fromRunningInfo->progress.error.count;
  runningInfo->progress.error.size          = fromRunningInfo->progress.error.size;
  runningInfo->progress.archiveSize         = fromRunningInfo->progress.archiveSize;
  runningInfo->progress.compressionRatio    = fromRunningInfo->progress.compressionRatio;
  String_set(runningInfo->progress.entry.name,fromRunningInfo->progress.entry.name);
  runningInfo->progress.entry.doneSize      = fromRunningInfo->progress.entry.doneSize;
  runningInfo->progress.entry.totalSize     = fromRunningInfo->progress.entry.totalSize;
  String_set(runningInfo->progress.storage.name,fromRunningInfo->progress.storage.name);
  runningInfo->progress.storage.doneSize    = fromRunningInfo->progress.storage.doneSize;
  runningInfo->progress.storage.totalSize   = fromRunningInfo->progress.storage.totalSize;
  runningInfo->progress.volume.number       = fromRunningInfo->progress.volume.number;
  runningInfo->progress.volume.done         = fromRunningInfo->progress.volume.done;
  runningInfo->message.code                 = fromRunningInfo->message.code;
  String_set(runningInfo->message.text,fromRunningInfo->message.text);

  runningInfo->lastErrorCode                = fromRunningInfo->lastErrorCode;
  runningInfo->lastErrorNumber              = fromRunningInfo->lastErrorNumber;
  String_set(runningInfo->lastErrorData,fromRunningInfo->lastErrorData);

  runningInfo->lastExecutedDateTime         = fromRunningInfo->lastExecutedDateTime;

  runningInfo->entriesPerSecond             = fromRunningInfo->entriesPerSecond;
  runningInfo->bytesPerSecond               = fromRunningInfo->bytesPerSecond;
  runningInfo->storageBytesPerSecond        = fromRunningInfo->storageBytesPerSecond;
  runningInfo->estimatedRestTime            = fromRunningInfo->estimatedRestTime;
}

const char *volumeRequestToString(VolumeRequests volumeRequest)
{
  const char *VOLUME_REQUEST_TEXT[] =
  {
    "NONE",
    "INITIAL",
    "REPLACEMENT"
  };

  assert(volumeRequest >= VOLUME_REQUEST_MIN);
  assert(volumeRequest <= VOLUME_REQUEST_MAX);
  assert((VOLUME_REQUEST_MAX-VOLUME_REQUEST_MIN+1) == SIZE_OF_ARRAY(VOLUME_REQUEST_TEXT));

  return VOLUME_REQUEST_TEXT[(uint)volumeRequest];
}

void messageSet(Message *message, MessageCodes messageCode, ConstString messageText)
{
  assert(message != NULL);

  message->code = messageCode;
  String_set(message->text,messageText);
}

void messageClear(Message *message)
{
  assert(message != NULL);

  message->code = MESSAGE_CODE_NONE;
  String_clear(message->text);
}

const char *messageCodeToString(MessageCodes messageCode)
{
  const char *MESSAGE_CODE_TEXT[] =
  {
    "NONE",
    "WAIT_FOR_TEMPORARY_SPACE",
    "BLANK_VOLUME",
    "CREATE_IMAGE",
    "ADD_ERROR_CORRECTION_CODES",
    "WRITE_VOLUME",
    "VERIFY_VOLUME"
  };

  assert(messageCode >= MESSAGE_CODE_MIN);
  assert(messageCode <= MESSAGE_CODE_MAX);
  assert((MESSAGE_CODE_MAX-MESSAGE_CODE_MIN+1) == SIZE_OF_ARRAY(MESSAGE_CODE_TEXT));

  return MESSAGE_CODE_TEXT[(uint)messageCode];
}

void templateInit(TemplateHandle   *templateHandle,
                  const char       *templateString,
                  ExpandMacroModes expandMacroMode,
                  uint64           dateTime
                 )
{
  assert(templateHandle != NULL);

  // init variables
  templateHandle->templateString  = templateString;
  templateHandle->expandMacroMode = expandMacroMode;
  templateHandle->dateTime        = dateTime;
  templateHandle->textMacros      = NULL;
  templateHandle->textMacroCount  = 0;
}

void templateMacros(TemplateHandle   *templateHandle,
                    const TextMacro  textMacros[],
                    uint             textMacroCount
                   )
{
  assert(templateHandle != NULL);

  // add macros
  uint      newTextMacroCount = templateHandle->textMacroCount+textMacroCount;
  TextMacro *newTextMacros    = (TextMacro*)realloc((void*)templateHandle->textMacros,newTextMacroCount*sizeof(TextMacro));
  if (newTextMacros == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  memcpy(&newTextMacros[templateHandle->textMacroCount],textMacros,textMacroCount*sizeof(TextMacro));
  templateHandle->textMacros     = newTextMacros;
  templateHandle->textMacroCount = newTextMacroCount;
}

String templateDone(TemplateHandle *templateHandle,
                    String         string
                   )
{
  assert(templateHandle != NULL);

  // init variables
  if (string == NULL) string = String_new();

  // get local time
  #ifdef HAVE_LOCALTIME_R
    struct tm tmBuffer;
    struct tm *tm;
    tm = localtime_r((const time_t*)&templateHandle->dateTime,&tmBuffer);
  #else /* not HAVE_LOCALTIME_R */
    struct tm *tm;
    tm = localtime((const time_t*)&templateHandle->dateTime);
  #endif /* HAVE_LOCALTIME_R */
  assert(tm != NULL);

  // get week numbers
  char buffer[256];
  strftime(buffer,sizeof(buffer)-1,"%U",tm); buffer[sizeof(buffer)-1] = '\0';
  uint weekNumberU = (uint)atoi(buffer);
  strftime(buffer,sizeof(buffer)-1,"%W",tm); buffer[sizeof(buffer)-1] = '\0';
  uint weekNumberW = (uint)atoi(buffer);

  // add week macros
  uint      newTextMacroCount = templateHandle->textMacroCount+4;
  TextMacro *newTextMacros    = (TextMacro*)realloc((void*)templateHandle->textMacros,newTextMacroCount*sizeof(TextMacro));
  if (newTextMacros == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  TEXT_MACRO_N_UINT(newTextMacros[templateHandle->textMacroCount+0],"%U2",(weekNumberU%2)+1,"[12]"  );
  TEXT_MACRO_N_UINT(newTextMacros[templateHandle->textMacroCount+1],"%U4",(weekNumberU%4)+1,"[1234]");
  TEXT_MACRO_N_UINT(newTextMacros[templateHandle->textMacroCount+2],"%W2",(weekNumberW%2)+1,"[12]"  );
  TEXT_MACRO_N_UINT(newTextMacros[templateHandle->textMacroCount+3],"%W4",(weekNumberW%4)+1,"[1234]");
  templateHandle->textMacros     = newTextMacros;
  templateHandle->textMacroCount = newTextMacroCount;

  // expand macros
  Misc_expandMacros(string,
                    templateHandle->templateString,
                    templateHandle->expandMacroMode,
                    templateHandle->textMacros,
                    templateHandle->textMacroCount,
                    FALSE
                   );

  // expand date/time macros, replace %% -> %
  size_t i = 0L;
  char   format[4];
  while (i < String_length(string))
  {
    switch (String_index(string,i))
    {
      case '%':
        if ((i+1) < String_length(string))
        {
          switch (String_index(string,i+1))
          {
            case '%':
              // %% -> %
              String_remove(string,i,1);
              i += 1L;
              break;
            case 'a':
            case 'A':
            case 'b':
            case 'B':
            case 'c':
            case 'C':
            case 'd':
            case 'D':
            case 'e':
            case 'E':
            case 'F':
            case 'g':
            case 'G':
            case 'h':
            case 'H':
            case 'I':
            case 'j':
            case 'k':
            case 'l':
            case 'm':
            case 'M':
            case 'n':
            case 'O':
            case 'p':
            case 'P':
            case 'r':
            case 'R':
            case 's':
            case 'S':
            case 't':
            case 'T':
            case 'u':
            case 'U':
            case 'V':
            case 'w':
            case 'W':
            case 'x':
            case 'X':
            case 'y':
            case 'Y':
            case 'z':
            case 'Z':
            case '+':
              // format date/time part
              switch (String_index(string,i+1))
              {
                case 'E':
                case 'O':
                  // %Ex, %Ox: extended date/time macros
                  format[0] = '%';
                  format[1] = String_index(string,i+1);
                  format[2] = String_index(string,i+2);
                  format[3] = '\0';

                  String_remove(string,i,3);
                  break;
                default:
                  // %x: date/time macros
                  format[0] = '%';
                  format[1] = String_index(string,i+1);
                  format[2] = '\0';

                  String_remove(string,i,2);
                  break;
              }
              size_t length = strftime(buffer,sizeof(buffer)-1,format,tm); buffer[sizeof(buffer)-1] = '\0';

              // insert into string
              switch (templateHandle->expandMacroMode)
              {
                case EXPAND_MACRO_MODE_STRING:
                  String_insertBuffer(string,i,buffer,length);
                  i += length;
                  break;
                case EXPAND_MACRO_MODE_PATTERN:
                  for (size_t z = 0 ; z < length; z++)
                  {
                    if (strchr("*+?{}():[].^$|",buffer[z]) != NULL)
                    {
                      String_insertChar(string,i,'\\');
                      i += 1L;
                    }
                    String_insertChar(string,i,buffer[z]);
                    i += 1L;
                  }
                  break;
                #ifndef NDEBUG
                  default:
                    HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
                    break; /* not reached */
                  #endif /* NDEBUG */
              }
              break;
            default:
              // keep %x
              i += 2L;
              break;
          }
        }
        else
        {
          // keep % at end of string
          i += 1L;
        }
        break;
      default:
        i += 1L;
        break;
    }
  }

  // free resources
  free((void*)templateHandle->textMacros);

  return string;
}

String expandTemplate(const char       *templateString,
                      ExpandMacroModes expandMacroMode,
                      time_t           timestamp,
                      const TextMacro  textMacros[],
                      uint             textMacroCount
                     )
{
  TemplateHandle templateHandle;
  templateInit(&templateHandle,
               templateString,
               expandMacroMode,
               timestamp
              );
  templateMacros(&templateHandle,
                 textMacros,
                 textMacroCount
                );

  return templateDone(&templateHandle,
                      NULL  // string
                     );
}

Errors executeTemplate(const char        *templateString,
                       time_t            timestamp,
                       const TextMacro   textMacros[],
                       uint              textMacroCount,
                       ExecuteIOFunction executeIOFunction,
                       void              *executeIOUserData,
                       uint              timeout
                      )
{
  Errors error;

  if (!stringIsEmpty(templateString))
  {
    String script = expandTemplate(templateString,
                                   EXPAND_MACRO_MODE_STRING,
                                   timestamp,
                                   textMacros,
                                   textMacroCount
                                  );
    if (!String_isEmpty(script))
    {
      // execute script
      error = Misc_executeScript(String_cString(script),
                                 CALLBACK_(executeIOFunction,executeIOUserData),
                                 CALLBACK_(executeIOFunction,executeIOUserData),
                                 timeout
                                );
      String_delete(script);
    }
    else
    {
      error = ERROR_EXPAND_TEMPLATE;
    }
  }
  else
  {
    error = ERROR_NONE;
  }

  return error;
}

// ----------------------------------------------------------------------

bool parseBandWidthNumber(ConstString s, ulong *n)
{
  const StringUnit UNITS[] =
  {
    {"T",1024LL*1024LL*1024LL*1024LL},
    {"G",1024LL*1024LL*1024LL},
    {"M",1024LL*1024LL},
    {"K",1024LL},
    {NULL,0LL}
  };

  assert(s != NULL);
  assert(n != NULL);

  (*n) = (ulong)String_toInteger64(s,STRING_BEGIN,NULL,UNITS,SIZE_OF_ARRAY(UNITS));

  return TRUE;
}

ulong getBandWidth(BandWidthList *bandWidthList)
{
  assert(bandWidthList != NULL);

  ulong n = 0L;

  // get current date/time values
  uint64        currentDateTime = Misc_getCurrentDateTime();
  uint          currentYear,currentMonth,currentDay;
  WeekDays      currentWeekDay;
  uint          currentHour,currentMinute;
  Misc_splitDateTime(currentDateTime,
                     TIME_TYPE_LOCAL,
                     &currentYear,
                     &currentMonth,
                     &currentDay,
                     &currentHour,
                     &currentMinute,
                     NULL,  // second
                     &currentWeekDay,
                     NULL  // currentIsDayLightSaving
                    );

  // find best matching band width node
  uint          matchingDateTime       = 0LL;
  BandWidthNode *matchingBandWidthNode = NULL;
  BandWidthNode *bandWidthNode;
  LIST_ITERATE(bandWidthList,bandWidthNode)
  {
    int year    = (bandWidthNode->year   != DATE_ANY) ? (uint)bandWidthNode->year   : currentYear;
    int month   = (bandWidthNode->month  != DATE_ANY) ? (uint)bandWidthNode->month  : currentMonth;
    int day     = (bandWidthNode->day    != DATE_ANY) ? (uint)bandWidthNode->day    : currentDay;
    uint hour   = (bandWidthNode->hour   != TIME_ANY) ? (uint)bandWidthNode->hour   : currentHour;
    uint minute = (bandWidthNode->minute != TIME_ANY) ? (uint)bandWidthNode->minute : currentMinute;

    uint64 dateTime = Misc_makeDateTime(TIME_TYPE_LOCAL,year,month,day,hour,minute,0,DAY_LIGHT_SAVING_MODE_AUTO);

    if (   (currentDateTime >= dateTime)
        && (   (matchingBandWidthNode == NULL)
            || (dateTime > matchingDateTime)
           )
       )
    {
      matchingDateTime      = dateTime;
      matchingBandWidthNode = bandWidthNode;
    }
  }

  if (matchingBandWidthNode != NULL)
  {
    // read band width
    if (matchingBandWidthNode->fileName != NULL)
    {
      // read from external file
      uint64 timestamp = Misc_getTimestamp();
      if (timestamp > (bandWidthList->lastReadTimestamp+5*US_PER_SECOND))
      {
        bandWidthList->n = 0LL;

        // open file
        FileHandle fileHandle;
        if (File_open(&fileHandle,matchingBandWidthNode->fileName,FILE_OPEN_READ) == ERROR_NONE)
        {
          String line = String_new();
          while (File_getLine(&fileHandle,line,NULL,"#;"))
          {
            // parse band width
            if (!parseBandWidthNumber(line,&bandWidthList->n))
            {
              continue;
            }
          }
          String_delete(line);

          // close file
          (void)File_close(&fileHandle);

          // store timestamp of last read
          bandWidthList->lastReadTimestamp = timestamp;
        }
      }

      n = bandWidthList->n;
    }
    else
    {
      // use value
      n = matchingBandWidthNode->n;
    }
  }

  return n;
}

bool isInTimeRange(uint hour, uint minute, int beginHour, int beginMinute, int endHour, int endMinute)
{
  if (TIME_BEGIN(beginHour,beginMinute) <= TIME_END(endHour,endMinute))
  {
    return    (TIME(hour,minute) >= TIME_BEGIN(beginHour,beginMinute))
           && (TIME(hour,minute) <= TIME_END  (endHour,  endMinute  ));
  }
  else
  {
    return    (TIME(hour,minute) >= TIME_BEGIN(beginHour,beginMinute))
           || (TIME(hour,minute) <= TIME_END  (endHour,  endMinute  ));
  }
}

Errors initEncodingConverter(const char *systemEncoding, const char *consoleEncoding)
{
  encodingConverter.isInitialized = FALSE;

  #ifdef HAVE_ICU
    encodingConverter.isUTF8System =    stringEqualsIgnoreCase((systemEncoding != NULL) ? systemEncoding : ucnv_getDefaultName(),"UTF-8")
                                     && ((consoleEncoding == NULL) || stringEqualsIgnoreCase(consoleEncoding,"UTF-8"));
    if (!encodingConverter.isUTF8System)
    {
      Semaphore_init(&encodingConverter.lock, SEMAPHORE_TYPE_BINARY);

      #if defined(PLATFORM_WINDOWS)
        char win32ConsoleEncoding[16];

        // work-around for Windows brain dead: console use a different encoding than the graphical system...
        if (consoleEncoding == NULL)
        {
          UINT codePage = GetConsoleCP();
          if (codePage != 0)
          {
            stringFormat(win32ConsoleEncoding,sizeof(win32ConsoleEncoding),"cp%u",codePage);
            consoleEncoding = win32ConsoleEncoding;
          }
        }
      #endif

      UErrorCode errorCode = U_ZERO_ERROR;
      encodingConverter.systemConverter = ucnv_open(systemEncoding, &errorCode);
      if (encodingConverter.systemConverter == NULL)
      {
        Semaphore_done(&encodingConverter.lock);
        return ERRORX_(CONVERT_CHARS,errorCode,"%s",u_errorName(errorCode));
      }
      encodingConverter.systemMaxCharSize = ucnv_getMaxCharSize(encodingConverter.systemConverter);

      encodingConverter.consoleConverter = ucnv_open(consoleEncoding, &errorCode);
      if (encodingConverter.consoleConverter == NULL)
      {
        Semaphore_done(&encodingConverter.lock);
        ucnv_close(encodingConverter.systemConverter);
        return ERRORX_(CONVERT_CHARS,errorCode,"%s",u_errorName(errorCode));
      }
      encodingConverter.consoleMaxCharSize = ucnv_getMaxCharSize(encodingConverter.consoleConverter);

      encodingConverter.utf8Converter = ucnv_open("utf-8", &errorCode);
      if (encodingConverter.utf8Converter == NULL)
      {
        ucnv_close(encodingConverter.consoleConverter);
        ucnv_close(encodingConverter.systemConverter);
        Semaphore_done(&encodingConverter.lock);
        return ERRORX_(CONVERT_CHARS,errorCode,"%s",u_errorName(errorCode));
      }
      encodingConverter.utf8MaxCharSize = ucnv_getMaxCharSize(encodingConverter.utf8Converter);

      encodingConverter.unicodeCharSize = 256;
      encodingConverter.unicodeChars = (UChar*)malloc(encodingConverter.unicodeCharSize*sizeof(UChar));
      if (encodingConverter.unicodeChars == NULL)
      {
        ucnv_close(encodingConverter.utf8Converter);
        ucnv_close(encodingConverter.consoleConverter);
        ucnv_close(encodingConverter.systemConverter);
        Semaphore_done(&encodingConverter.lock);
        return ERRORX_(CONVERT_CHARS,errorCode,"%s",u_errorName(errorCode));
      }
      encodingConverter.bufferSize = 256;
      encodingConverter.buffer = (char*)malloc(encodingConverter.bufferSize*sizeof(char));
      if (encodingConverter.buffer == NULL)
      {
        free(encodingConverter.unicodeChars);
        ucnv_close(encodingConverter.utf8Converter);
        ucnv_close(encodingConverter.consoleConverter);
        ucnv_close(encodingConverter.systemConverter);
        Semaphore_done(&encodingConverter.lock);
        return ERRORX_(CONVERT_CHARS,errorCode,"%s",u_errorName(errorCode));
      }
    }
  #else
    encodingConverter.isUTF8System = TRUE;
  #endif // HAVE_ICU

  encodingConverter.isInitialized = TRUE;

  return ERROR_NONE;
}

void doneEncodingConverter(void)
{
  #ifdef HAVE_ICU
    if (encodingConverter.isInitialized && !encodingConverter.isUTF8System)
    {
      free(encodingConverter.buffer);
      free(encodingConverter.unicodeChars);
      ucnv_close(encodingConverter.utf8Converter);
      ucnv_close(encodingConverter.consoleConverter);
      ucnv_close(encodingConverter.systemConverter);
      Semaphore_done(&encodingConverter.lock);
    }
  #endif // HAVE_ICU
}

String convertSystemToUTF8Encoding(String destination, ConstString source)
{
  assert(destination != NULL);

  if (source != NULL)
  {
    #ifdef HAVE_ICU
      if (encodingConverter.isInitialized && !encodingConverter.isUTF8System)
      {
        SEMAPHORE_LOCKED_DO(&encodingConverter.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          // convert to Unicode encoding
          int32_t unicodeLength = convertToUnicode(encodingConverter.systemConverter,source);
          if (unicodeLength < 0)
          {
            Semaphore_unlock(&encodingConverter.lock);
            String_clear(destination);
            return destination;
          }

          // convert to UTF-8 encoding
          int32_t length = convertFromUnicode(destination,encodingConverter.utf8Converter,unicodeLength,encodingConverter.utf8MaxCharSize);
          if (length < 0)
          {
            Semaphore_unlock(&encodingConverter.lock);
            String_clear(destination);
            return destination;
          }

          String_setBuffer(destination,encodingConverter.buffer,length);
        }
      }
      else
      {
        String_set(destination,source);
      }
    #else
      String_set(destination,source);
    #endif
  }
  else
  {
    String_clear(destination);
  }

  return destination;
}

String convertUTF8ToSystemEncoding(String destination, ConstString source)
{
  assert(destination != NULL);

  if (source != NULL)
  {
    #ifdef HAVE_ICU
      if (encodingConverter.isInitialized && !encodingConverter.isUTF8System)
      {
        SEMAPHORE_LOCKED_DO(&encodingConverter.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          // convert to Unicode encoding
          int32_t unicodeLength = convertToUnicode(encodingConverter.utf8Converter,source);
          if (unicodeLength < 0)
          {
            Semaphore_unlock(&encodingConverter.lock);
            String_clear(destination);
            return destination;
          }

          // convert to system encoding
          int32_t length = convertFromUnicode(destination,encodingConverter.systemConverter,unicodeLength,encodingConverter.systemMaxCharSize);
          if (length < 0)
          {
            Semaphore_unlock(&encodingConverter.lock);
            String_clear(destination);
            return destination;
          }
        }
      }
      else
      {
        String_set(destination,source);
      }
    #else
      String_set(destination,source);
    #endif
  }
  else
  {
    String_clear(destination);
  }

  return destination;
}

String convertSystemToConsoleEncodingAppend(String destination, ConstString source)
{
  assert(destination != NULL);

  if (source != NULL)
  {
    #ifdef HAVE_ICU
      if (encodingConverter.isInitialized && !encodingConverter.isUTF8System)
      {
        SEMAPHORE_LOCKED_DO(&encodingConverter.lock,SEMAPHORE_LOCK_TYPE_READ_WRITE,WAIT_FOREVER)
        {
          // convert to Unicode encoding
          int32_t unicodeLength = convertToUnicode(encodingConverter.systemConverter,source);
          if (unicodeLength < 0)
          {
            Semaphore_unlock(&encodingConverter.lock);
            String_clear(destination);
            return destination;
          }

          // convert to console encoding
          int32_t length = convertFromUnicode(destination,encodingConverter.consoleConverter,unicodeLength,encodingConverter.consoleMaxCharSize);
          if (length < 0)
          {
            Semaphore_unlock(&encodingConverter.lock);
            String_clear(destination);
            return destination;
          }
        }
      }
      else
      {
        String_append(destination,source);
      }
    #else
      String_append(destination,source);
    #endif
  }
  else
  {
    String_clear(destination);
  }

  return destination;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
