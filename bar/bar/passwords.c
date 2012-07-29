/***********************************************************************\
*
* $Revision$
* $Date$
* $Author$
* Contents: functions for secure storage of passwords
* Systems: all
*
\***********************************************************************/

/****************************** Includes *******************************/
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#ifdef HAVE_GCRYPT
  #include <gcrypt.h>
#endif /* HAVE_GCRYPT */
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>

#include "global.h"
#include "strings.h"

#include "errors.h"
#include "bar.h"

#include "passwords.h"

/****************** Conditional compilation switches *******************/

/***************************** Constants *******************************/

/***************************** Datatypes *******************************/

#ifndef HAVE_GCRYPT
  typedef struct
  {
    ulong size;
  } MemoryHeader;
#endif

/***************************** Variables *******************************/
#ifndef HAVE_GCRYPT
  LOCAL char obfuscator[MAX_PASSWORD_LENGTH];
#endif /* not HAVE_GCRYPT */

/****************************** Macros *********************************/

/***************************** Forwards ********************************/

/***************************** Functions *******************************/

#ifdef __cplusplus
  extern "C" {
#endif

Errors Password_initAll(void)
{
  #ifndef HAVE_GCRYPT
    int z;
  #endif /* not HAVE_GCRYPT */

  #ifndef HAVE_GCRYPT
    /* if libgrypt is not available a simple obfuscator is used here to
       avoid plain text passwords in memory as much as possible
    */
    srandom((unsigned int)time(NULL));
    for (z = 0; z < MAX_PASSWORD_LENGTH; z++)
    {
      obfuscator[z] = (char)(random()%256);
    }
  #endif /* not HAVE_GCRYPT */

  return ERROR_NONE;
}

void Password_doneAll(void)
{
}

void *Password_allocSecure(ulong size)
{
  void *p;
  #ifndef HAVE_GCRYPT
    MemoryHeader *memoryHeader;
  #endif

  #ifdef HAVE_GCRYPT
    p = gcry_malloc_secure(size);
  #else /* not HAVE_GCRYPT */
    memoryHeader = (MemoryHeader*)malloc(sizeof(MemoryHeader) + size);
    memoryHeader->size = size;
    p = (byte*)memoryHeader + sizeof(MemoryHeader);
  #endif /* HAVE_GCRYPT */

  return p;
}

void Password_freeSecure(void *p)
{
  #ifndef HAVE_GCRYPT
    MemoryHeader *memoryHeader;
  #endif

  assert(p != NULL);

  #ifdef HAVE_GCRYPT
    gcry_free(p);
  #else /* not HAVE_GCRYPT */
    memoryHeader = (MemoryHeader*)((byte*)p - sizeof(MemoryHeader));
    memset(memoryHeader,0,sizeof(memoryHeader) + memoryHeader->size);
    free(memoryHeader);
  #endif /* HAVE_GCRYPT */
}

void Password_init(Password *password)
{
  assert(password != NULL);

  password->data = Password_allocSecure(MAX_PASSWORD_LENGTH+1);
  if (password->data == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  password->length = 0;
}

void Password_done(Password *password)
{
  assert(password != NULL);
  assert(password->data != NULL);

  Password_freeSecure(password->data);
}

Password *Password_new(void)
{
  Password *password;

  password = (Password*)malloc(sizeof(Password));
  if (password == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  Password_init(password);

  return password;
}

Password *Password_newString(const String string)
{
  Password *password;

  password = Password_new();
  if (password != NULL)
  {
    Password_setString(password,string);
  }

  return password;
}

Password *Password_newCString(const char *s)
{
  Password *password;

  password = Password_new();
  if (password != NULL)
  {
    Password_setCString(password,s);
  }

  return password;
}

Password *Password_duplicate(const Password *fromPassword)
{
  Password *password;

  if (fromPassword != NULL)
  {
    password = Password_new();
    assert(password != NULL);
    Password_set(password,fromPassword);
  }
  else
  {
    password = NULL;
  }

  return password;
}

void Password_delete(Password *password)
{
  if (password != NULL)
  {
    Password_done(password);
    free(password);
  }
}

void Password_clear(Password *password)
{
  assert(password != NULL);

  password->length = 0;
  password->data[0] = '\0';
}

void Password_set(Password *password, const Password *fromPassword)
{
  assert(password != NULL);

  if (fromPassword != NULL)
  {
    memcpy(password->data,fromPassword->data,MAX_PASSWORD_LENGTH+1);
    password->length = fromPassword->length;
  }
  else
  {
    password->length = 0;
  }
}

void Password_setString(Password *password, const String string)
{
  uint length;
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    uint z;
  #endif /* HAVE_GCRYPT */

  assert(password != NULL);

  length = MIN(String_length(string),MAX_PASSWORD_LENGTH);
  #ifdef HAVE_GCRYPT
    memcpy(password->data,String_cString(string),length);
  #else /* not HAVE_GCRYPT */
    for (z = 0; z < length; z++)
    {
      password->data[z] = String_index(string,z)^obfuscator[z];
    }
  #endif /* HAVE_GCRYPT */
  password->data[length] = '\0';
  password->length = length;
}

void Password_setCString(Password *password, const char *s)
{
  uint length;
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    uint z;
  #endif /* HAVE_GCRYPT */

  assert(password != NULL);

  length = MIN(strlen(s),MAX_PASSWORD_LENGTH);
  #ifdef HAVE_GCRYPT
    memcpy(password->data,s,length);
  #else /* not HAVE_GCRYPT */
    for (z = 0; z < length; z++)
    {
      password->data[z] = s[z]^obfuscator[z];
    }
  #endif /* HAVE_GCRYPT */
  password->data[length] = '\0';
  password->length = length;
}

void Password_setBuffer(Password *password, const void *buffer, uint length)
{
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    char *p;
    uint z;
  #endif /* HAVE_GCRYPT */

  assert(password != NULL);

  length = MIN(length,MAX_PASSWORD_LENGTH);
  #ifdef HAVE_GCRYPT
    memcpy(password->data,buffer,length);
  #else /* not HAVE_GCRYPT */
    p = (char*)buffer;
    for (z = 0; z < length; z++)
    {
      password->data[z] = p[z]^obfuscator[z];
    }
  #endif /* HAVE_GCRYPT */
  password->data[length] = '\0';
  password->length = length;
}

void Password_appendChar(Password *password, char ch)
{
  assert(password != NULL);

  if (password->length < MAX_PASSWORD_LENGTH)
  {
    #ifdef HAVE_GCRYPT
      password->data[password->length] = ch;
    #else /* not HAVE_GCRYPT */
      password->data[password->length] = ch^obfuscator[password->length];
    #endif /* HAVE_GCRYPT */
    password->length++;
    password->data[password->length] = '\0';
  }
}

void Password_random(Password *password, uint length)
{
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    uint z;
  #endif /* HAVE_GCRYPT */

  assert(password != NULL);

  password->length = MIN(length,MAX_PASSWORD_LENGTH);
  #ifdef HAVE_GCRYPT
    gcry_randomize((unsigned char*)password->data,password->length,GCRY_STRONG_RANDOM);
  #else /* not HAVE_GCRYPT */
    srandom((unsigned int)time(NULL));
    for (z = 0; z < password->length; z++)
    {
      password->data[z] = (char)(random()%256)^obfuscator[z];
    }
  #endif /* HAVE_GCRYPT */
}

uint Password_length(const Password *password)
{
  return (password != NULL)?password->length:0;
}

bool Password_empty(const Password *password)
{
  return (password == NULL) || (password->length == 0);
}

char Password_getChar(const Password *password, uint index)
{
  if ((password != NULL) && (index < password->length))
  {
    #ifdef HAVE_GCRYPT
      return password->data[index];
    #else /* not HAVE_GCRYPT */
      return password->data[index]^obfuscator[index];
    #endif /* HAVE_GCRYPT */
  }
  else
  {
    return '\0';
  }
}

double Password_getQualityLevel(const Password *password)
{
  #define CHECK(condition) \
    do { \
      if (condition) browniePoints++; \
      maxBrowniePoints++; \
    } while (0)

  uint browniePoints,maxBrowniePoints;
  bool flag0,flag1;
  uint z;

  assert(password != NULL);

  browniePoints    = 0;
  maxBrowniePoints = 0;

  // length >= 8
  CHECK(password->length >= 8);

  // contain numbers
  flag0 = FALSE;
  for (z = 0; z < password->length; z++)
  {
    flag0 |= isdigit(password->data[z]);
  }
  CHECK(flag0);

  // contain special characters
  flag0 = FALSE;
  for (z = 0; z < password->length; z++)
  {
    flag0 |= (strchr(" !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",password->data[z]) != NULL);
  }
  CHECK(flag0);

  // capital/non-capital letters
  flag0 = FALSE;
  flag1 = FALSE;
  for (z = 0; z < password->length; z++)
  {
    flag0 |= (toupper(password->data[z]) != password->data[z]);
    flag1 |= (tolower(password->data[z]) != password->data[z]);
  }
  CHECK(flag0 && flag1);

  // estimate entropie by compression
// ToDo

  return (double)browniePoints/(double)maxBrowniePoints;

  #undef CHECK
}

const char *Password_deploy(Password *password)
{
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    uint z;
  #endif /* HAVE_GCRYPT */

  if (password != NULL)
  {
    assert(password->length <= MAX_PASSWORD_LENGTH);

    #ifdef HAVE_GCRYPT
      return password->data;
    #else /* not HAVE_GCRYPT */
      for (z = 0; z < password->length; z++)
      {
        password->plain[z] = password->data[z]^obfuscator[z];
      }
      password->plain[password->length] = '\0';
      return password->plain;
    #endif /* HAVE_GCRYPT */
  }
  else
  {
    return "";
  }
}

void Password_undeploy(Password *password)
{
  if (password != NULL)
  {
    #ifdef HAVE_GCRYPT
      UNUSED_VARIABLE(password);
    #else /* not HAVE_GCRYPT */
      memset(password->plain,0,MAX_PASSWORD_LENGTH);
    #endif /* HAVE_GCRYPT */
  }
}

bool Password_input(Password   *password,
                    const char *message,
                    uint       modes
                   )
{
  bool okFlag;

  assert(password != NULL);

  Password_clear(password);

  okFlag = FALSE;

  // input via SSH_ASKPASS program
  if (((modes & PASSWORD_INPUT_MODE_GUI) != 0) && !okFlag)
  {
    const char *sshAskPassword;
    String     command;
    FILE       *file;
    bool       eolFlag;
    int        ch;

    sshAskPassword = getenv("SSH_ASKPASS");
    if ((sshAskPassword != NULL) && (strcmp(sshAskPassword,"") != 0))
    {
      // open pipe to external password program
      command = String_newCString(sshAskPassword);
      if (message != NULL)
      {
        String_format(command," %\"s:",message);
      }
      file = popen(String_cString(command),"r");
      if (file == NULL)
      {
        String_delete(command);
        return FALSE;
      }
      String_delete(command);

      // read password, discard last LF
      printInfo(2,"Wait for password...");
      eolFlag = FALSE;
      do
      {
        ch = getc(file);
        if (ch != EOF)
        {
          switch ((char)ch)
          {
            case '\n':
            case '\r':
              eolFlag = TRUE;
              break;
            default:
              Password_appendChar(password,(char)ch);
              break;
          }
        }
        else
        {
          eolFlag = TRUE;
        }
      }
      while (!eolFlag);
      printInfo(2,"ok\n");

      // close pipe
      pclose(file);

      okFlag = TRUE;
    }
  }

  // input via console
  if (((modes & PASSWORD_INPUT_MODE_CONSOLE) != 0) && !okFlag)
  {
    int            n;
    struct termios oldTermioSettings;
    struct termios termioSettings;
    bool           eolFlag;
    char           ch;

    if (isatty(STDIN_FILENO) == 1)
    {
      // read data from interactive input
      if (message != NULL)
      {
        fprintf(stderr,"%s: ",message); fflush(stderr);
      }

      // save current console settings
      if (tcgetattr(STDIN_FILENO,&oldTermioSettings) != 0)
      {
        return FALSE;
      }

      // disable echo
      memcpy(&termioSettings,&oldTermioSettings,sizeof(struct termios));
      termioSettings.c_lflag &= ~ECHO;
      if (tcsetattr(STDIN_FILENO,TCSANOW,&termioSettings) != 0)
      {
        return FALSE;
      }

      // input password
      eolFlag = FALSE;
      do
      {
        if (read(STDIN_FILENO,&ch,1) == 1)
        {
          switch (ch)
          {
            case '\r':
              break;
            case '\n':
              eolFlag = TRUE;
              break;
            default:
              Password_appendChar(password,ch);
              break;
          }
        }
        else
        {
          eolFlag = TRUE;
        }
      }
      while (!eolFlag);

      // restore console settings
      tcsetattr(STDIN_FILENO,TCSANOW,&oldTermioSettings);

      if (message != NULL)
      {
        fprintf(stderr,"\n");
      }
    }
    else
    {
      // read data from non-interactive input
      eolFlag = FALSE;
      do
      {
        ioctl(STDIN_FILENO,FIONREAD,(char*)&n);
        if (n > 0)
        {
          if (read(STDIN_FILENO,&ch,1) == 1)
          {
            switch (ch)
            {
              case '\r':
                break;
              case '\n':
                eolFlag = TRUE;
                break;
              default:
                Password_appendChar(password,ch);
                break;
            }
          }
          else
          {
            eolFlag = TRUE;
          }
        }
      }
      while (!eolFlag && (n > 0));
    }

    okFlag = TRUE;
  }

  return okFlag;
}

bool Password_inputVerify(const Password *password,
                          const char     *message,
                          uint           modes
                         )
{
  Password verifyPassword;
  bool     equalFlag;

  // read passsword again
  Password_init(&verifyPassword);
  if (!Password_input(&verifyPassword,message,modes))
  {
    Password_done(&verifyPassword);
    return FALSE;
  }

  // verify password
  equalFlag = TRUE;
  if (password->length != verifyPassword.length) equalFlag = FALSE;
  if (memcmp(password->data,verifyPassword.data,password->length) != 0) equalFlag = FALSE;

  // free resources
  Password_done(&verifyPassword);

  return equalFlag;
}

#if 0
void Password_dump(const char *text, Password *password)
{
  uint z;

  assert(password != NULL);

  fprintf(stderr,text);
  for (z = 0; z < password->length; z++)
  {
    #ifdef HAVE_GCRYPT
      fprintf(stderr,"%02x",(byte)password->data[z]);
    #else /* not HAVE_GCRYPT */
      fprintf(stderr,"%02x",(byte)(password->data[z]^obfuscator[z]));
    #endif /* HAVE_GCRYPT */
  }
  fprintf(stderr,"\n");
}
#endif /* 0 */


#ifdef __cplusplus
  }
#endif

/* end of file */
