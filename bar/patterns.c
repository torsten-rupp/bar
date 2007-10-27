/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/patterns.c,v $
* $Revision: 1.8 $
* $Author: torsten $
* Contents: Backup ARchiver pattern functions
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "global.h"
#include "lists.h"

#include "bar.h"

#include "patterns.h"

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
* Name   : compilePattern
* Purpose: compile pattern
* Input  : pattern     - pattern to compile
*          patternType - pattern type
* Output : regexBegin - regular expression for matching begin
*          regexEnd   - regular expression for matching end
*          regexExcat - regular expression for exact matching
* Return : ERROR_NONE or error code
* Notes  : -
\***********************************************************************/

LOCAL Errors compilePattern(const char   *pattern,
                            PatternTypes patternType,
                            regex_t      *regexBegin,
                            regex_t      *regexEnd,
                            regex_t      *regexExact
                           )
{
  String matchString;
  String regexString;
  int    regexFlags;
  ulong  z;

  matchString = String_new();
  regexString = String_new();

  regexFlags = REG_ICASE|REG_NOSUB;
  switch (patternType)
  {
    case PATTERN_TYPE_GLOB:
      z = 0;
      while (pattern[z] != '\0')
      {
        switch (pattern[z])
        {
          case '*':
            String_appendCString(matchString,".*");
            break;
          case '?':
            String_appendChar(matchString,'.');
            break;
          case '.':
            String_appendCString(matchString,"\\.");
            break;
          case '\\':
            String_appendChar(matchString,'\\');
            z++;
            if (pattern[z] != '\0') String_appendChar(matchString,pattern[z]);
            break;
          default:
            String_appendChar(matchString,pattern[z]);
            break;
        }
        z++;
      }     
      break;
    case PATTERN_TYPE_REGEX:
      String_setCString(matchString,pattern);
      break;
    case PATTERN_TYPE_EXTENDED_REGEX:
      regexFlags |= REG_EXTENDED;
      String_setCString(matchString,pattern);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  String_set(regexString,matchString);
  if (String_index(regexString,STRING_BEGIN) != '^') String_insertChar(regexString,STRING_BEGIN,'^');
  if (regcomp(regexBegin,String_cString(regexString),regexFlags) != 0)
  {
    String_delete(regexString);
    String_delete(matchString);
    return ERROR_INVALID_PATTERN;
  }

  String_set(regexString,matchString);
  if (String_index(regexString,STRING_END) != '$') String_insertChar(regexString,STRING_BEGIN,'$');
  if (regcomp(regexEnd,String_cString(regexString),regexFlags) != 0)
  {
    regfree(regexBegin);
    String_delete(regexString);
    String_delete(matchString);
    return ERROR_INVALID_PATTERN;
  }

  String_set(regexString,matchString);
  if (String_index(regexString,STRING_BEGIN) != '^') String_insertChar(regexString,STRING_BEGIN,'^');
  if (String_index(regexString,STRING_END) != '$') String_insertChar(regexString,STRING_END,'$');
  if (regcomp(regexExact,String_cString(regexString),regexFlags) != 0)
  {
    regfree(regexEnd);
    regfree(regexBegin);
    String_delete(regexString);
    String_delete(matchString);
    return ERROR_INVALID_PATTERN;
  }

  /* free resources */
  String_delete(regexString);
  String_delete(matchString);

  return ERROR_NONE;
}

/***********************************************************************\
* Name   : copyPatternNode
* Purpose: copy allocated pattern node
* Input  : patterNode - pattern node
* Output : -
* Return : copied pattern node
* Notes  : -
\***********************************************************************/

LOCAL PatternNode *copyPatternNode(PatternNode *patternNode,
                                   void        *userData
                                  )
{
  PatternNode *newPatternNode;
  Errors      error;

  assert(patternNode != NULL);

  UNUSED_VARIABLE(userData);

  /* allocate pattern node */
  newPatternNode = LIST_NEW_NODE(PatternNode);
  if (newPatternNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  newPatternNode->type    = patternNode->type;
  newPatternNode->pattern = String_copy(patternNode->pattern);

  /* compile pattern */
  error = compilePattern(String_cString(patternNode->pattern),
                         patternNode->type,
                         &newPatternNode->regexBegin,
                         &newPatternNode->regexEnd,
                         &newPatternNode->regexExact
                        );
  if (error != ERROR_NONE)
  {
    LIST_DELETE_NODE(newPatternNode);
    return NULL;
  }

  return newPatternNode;
}

/***********************************************************************\
* Name   : freePatternNode
* Purpose: free allocated pattern node
* Input  : patterNode - pattern node
* Output : -
* Return : -
* Notes  : -
\***********************************************************************/

LOCAL void freePatternNode(PatternNode *patternNode,
                           void        *userData
                          )
{
  assert(patternNode != NULL);

  UNUSED_VARIABLE(userData);

  regfree(&patternNode->regexExact);
  regfree(&patternNode->regexEnd);
  regfree(&patternNode->regexBegin);
  String_delete(((PatternNode*)patternNode)->pattern);
}

/*---------------------------------------------------------------------*/

Errors Pattern_initAll(void)
{
  return ERROR_NONE;
}

void Pattern_doneAll(void)
{
}

void Pattern_initList(PatternList *patternList)
{
  assert(patternList != NULL);

  List_init(patternList);
}

void Pattern_doneList(PatternList *patternList)
{
  assert(patternList != NULL);

  List_done(patternList,(ListNodeFreeFunction)freePatternNode,NULL);
}

void Pattern_clearList(PatternList *patternList)
{
  assert(patternList != NULL);

  List_clear(patternList,(ListNodeFreeFunction)freePatternNode,NULL);
}

void Pattern_copyList(const PatternList *fromPatternList, PatternList *toPatternList)
{
  assert(fromPatternList != NULL);
  assert(toPatternList != NULL);

  List_copy(fromPatternList,toPatternList,NULL,NULL,NULL,(ListNodeCopyFunction)copyPatternNode,NULL);
}

void Pattern_moveList(PatternList *fromPatternList, PatternList *toPatternList)
{
  assert(fromPatternList != NULL);
  assert(toPatternList != NULL);

  List_move(fromPatternList,toPatternList,NULL,NULL,NULL);
}

Errors Pattern_appendList(PatternList  *patternList,
                          const char   *pattern,
                          PatternTypes patternType
                         )
{
  PatternNode *patternNode;
  Errors      error;

  assert(patternList != NULL);
  assert(pattern != NULL);

  /* allocate pattern node */
  patternNode = LIST_NEW_NODE(PatternNode);
  if (patternNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  patternNode->type    = patternType;
  patternNode->pattern = String_newCString(pattern);

  /* compile pattern */
  error = compilePattern(pattern,
                         patternType,
                         &patternNode->regexBegin,
                         &patternNode->regexEnd,
                         &patternNode->regexExact
                        );
  if (error != ERROR_NONE)
  {
    LIST_DELETE_NODE(patternNode);
    return error;
  }

  /* add to list */
  List_append(patternList,patternNode);

  return ERROR_NONE;
}

/*
void Pattern_freeList(PatternList *patternList)
{
HALT_INTERNAL_ERROR_STILL_NOT_IMPLEMENTED();
}
*/

bool Pattern_match(PatternNode       *patternNode,
                   String            s,
                   PatternMatchModes patternMatchMode
                  )
{
  bool matchFlag;

  assert(patternNode != NULL);
  assert(s != NULL);

  switch (patternMatchMode)
  {
    case PATTERN_MATCH_MODE_BEGIN:
      matchFlag = (regexec(&patternNode->regexBegin,String_cString(s),0,NULL,0) == 0);
      break;
    case PATTERN_MATCH_MODE_END:
      matchFlag = (regexec(&patternNode->regexEnd,String_cString(s),0,NULL,0) == 0);
      break;
    case PATTERN_MATCH_MODE_EXACT:
      matchFlag = (regexec(&patternNode->regexExact,String_cString(s),0,NULL,0) == 0);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }

  return matchFlag;
}

bool Pattern_matchList(PatternList       *patternList,
                       String            s,
                       PatternMatchModes patternMatchMode
                      )
{
  bool        matchFlag;
  PatternNode *patternNode;

  assert(patternList != NULL);
  assert(s != NULL);

  matchFlag = FALSE;
  patternNode = patternList->head;
  while ((patternNode != NULL) && !matchFlag)
  {
    matchFlag = Pattern_match(patternNode,s,patternMatchMode);
    patternNode = patternNode->next;
  }

  return matchFlag;
}

bool Pattern_checkIsPattern(String s)
{
  const char *PATTERNS_CHARS = "*?[{";

  long z;
  bool patternFlag;

  assert(s != NULL);

  z = 0;
  patternFlag = FALSE;
  while ((z < String_length(s)) && !patternFlag)
  {
    if (String_index(s,z) != '\\')
    {
      patternFlag = (strchr(PATTERNS_CHARS,String_index(s,z)) != NULL);
    }
    else
    {
      z++;
    }
    z++;
  }

  return patternFlag;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
