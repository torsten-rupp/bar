/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/patterns.c,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: Backup ARchiver pattern functions
* Systems : all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "global.h"
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

  String_delete(((PatternNode*)patternNode)->matchString);
  String_delete(((PatternNode*)patternNode)->pattern);
}

/*---------------------------------------------------------------------*/

Errors Patterns_init(void)
{
  return ERROR_NONE;
}

void Patterns_done(void)
{
}

void Patterns_newList(PatternList *patternList)
{
  assert(patternList != NULL);

  Lists_init(patternList);
}

void Patterns_deleteList(PatternList *patternList)
{
  assert(patternList != NULL);

  Lists_done(patternList,(void(*)(void *,void *))freePatternNode,NULL);
}

Errors Patterns_addList(PatternList  *patternList,
                        const char   *pattern,
                        PatternTypes patternType
                       )
{
  PatternNode *patternNode;
  long        z;
  regex_t     regex;

  assert(patternList != NULL);
  assert(pattern != NULL);

  /* allocate pattern node */
  patternNode = (PatternNode*)malloc(sizeof(PatternNode));
  if (patternNode == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  patternNode->pattern = String_newCString(pattern);

  /* compile pattern */
  patternNode->matchString = String_new();
  patternNode->matchFlags  = REG_ICASE|REG_NOSUB;
  switch (patternType)
  {
    case PATTERN_TYPE_GLOB:
      z = 0;
      while (pattern[z] != '\0')
      {
        switch (pattern[z])
        {
          case '*':
            String_appendCString(patternNode->matchString,".*");
            break;
          case '?':
            String_appendChar(patternNode->matchString,'.');
            break;
          case '.':
            String_appendCString(patternNode->matchString,"\\.");
            break;
          case '\\':
            String_appendChar(patternNode->matchString,'\\');
            z++;
            if (pattern[z] != '\0') String_appendChar(patternNode->matchString,pattern[z]);
            break;
          default:
            String_appendChar(patternNode->matchString,pattern[z]);
            break;
        }
        z++;
      }     
      break;
    case PATTERN_TYPE_BASIC:
      String_setCString(patternNode->matchString,pattern);
      break;
    case PATTERN_TYPE_EXTENDED:
      patternNode->matchFlags |= REG_EXTENDED;
      String_setCString(patternNode->matchString,pattern);
      break;
    #ifndef NDEBUG
      default:
        HALT_INTERNAL_ERROR_UNHANDLED_SWITCH_CASE();
        break; /* not reached */
    #endif /* NDEBUG */
  }
  if (regcomp(&regex,String_cString(patternNode->matchString),patternNode->matchFlags) != 0)
  {
    return ERROR_INVALID_PATTERN;
  }
  regfree(&regex);

  /* add to list */
  Lists_add(patternList,patternNode);

  return ERROR_NONE;
}

void Patterns_freeList(PatternList *patternList)
{
}

bool Patterns_match(PatternNode *patternNode,
                    String      s
                   )
{
  regex_t regex;
  bool    matchFlag;

  assert(patternNode != NULL);
  assert(s != NULL);

  if (regcomp(&regex,String_cString(patternNode->matchString),patternNode->matchFlags) != 0)
  {
    HALT(EXITCODE_FATAL_ERROR,"cannot compile regular expression '%s'!",String_cString(patternNode->pattern));
  }
  matchFlag = (regexec(&regex,String_cString(s),0,NULL,0) == 0);
  regfree(&regex);

  return matchFlag;
}

bool Patterns_matchList(PatternList *patternList,
                        String      s
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
    matchFlag = Patterns_match(patternNode,s);
    patternNode = patternNode->next;
  }

  return matchFlag;
}

bool Patterns_checkIsPattern(String s)
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
