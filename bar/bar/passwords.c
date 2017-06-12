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
#include <config.h>  // use <...> to support separated build directory

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#ifdef HAVE_GCRYPT
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  #include <gcrypt.h>
  #pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif /* HAVE_GCRYPT */
#ifdef HAVE_TERMIOS_H
  #include <termios.h>
#endif /* HAVE_TERMIOS */
#include <unistd.h>
#ifdef HAVE_SYS_IOCTL_H
  #include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */
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

#if !defined(NDEBUG) || !defined(HAVE_GCRYPT)
  typedef struct
  {
    ulong size;
  } MemoryHeader;
#endif /* !NDEBUG || !HAVE_GCRYPT */

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

void *Password_allocSecure(size_t size)
{
  void *p;
  #if !defined(NDEBUG) || !defined(HAVE_GCRYPT)
    MemoryHeader *memoryHeader;
  #endif

  #ifdef HAVE_GCRYPT
    #ifndef NDEBUG
      memoryHeader = gcry_malloc_secure(sizeof(MemoryHeader)+size);
      if (memoryHeader == NULL)
      {
        return NULL;
      }
      memoryHeader->size = size;
      p = (byte*)memoryHeader+sizeof(MemoryHeader);
    #else
      p = gcry_malloc_secure(size);
      if (p == NULL)
      {
        return NULL;
      }
    #endif
    memset(p,0,size);
  #else /* not HAVE_GCRYPT */
    memoryHeader = (MemoryHeader*)calloc(1,sizeof(MemoryHeader)+size);
    if (memoryHeader == NULL)
    {
      return NULL;
    }
    memoryHeader->size = size;
    p = (byte*)memoryHeader+sizeof(MemoryHeader);
  #endif /* HAVE_GCRYPT */

  #ifndef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(p,sizeof(MemoryHeader));
  #endif

  return p;
}

void Password_freeSecure(void *p)
{
  #if !defined(NDEBUG) || !defined(HAVE_GCRYPT)
    MemoryHeader *memoryHeader;
  #endif

  assert(p != NULL);

  #ifndef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(p,sizeof(MemoryHeader));
  #endif

  #ifdef HAVE_GCRYPT
    #ifndef NDEBUG
      memoryHeader = (MemoryHeader*)((byte*)p - sizeof(MemoryHeader));
      gcry_free(memoryHeader);
    #else
      gcry_free(p);
    #endif
  #else /* not HAVE_GCRYPT */
    memoryHeader = (MemoryHeader*)((byte*)p - sizeof(MemoryHeader));
    memset(memoryHeader,0,sizeof(memoryHeader) + memoryHeader->size);
    free(memoryHeader);
  #endif /* HAVE_GCRYPT */
}

#ifdef NDEBUG
  void Password_init(Password *password)
#else /* not NDEBUG */
  void __Password_init(const char *__fileName__,
                       ulong      __lineNb__,
                       Password   *password
                      )
#endif /* NDEBUG */
{
  assert(password != NULL);

  password->data = Password_allocSecure(MAX_PASSWORD_LENGTH+1);
  if (password->data == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  password->data[0]    = '\0';
  password->dataLength = 0;

  #ifndef NDEBUG
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,password,sizeof(Password));
  #endif
}

#ifdef NDEBUG
  void Password_done(Password *password)
#else /* not NDEBUG */
  void __Password_done(const char *__fileName__,
                       ulong      __lineNb__,
                       Password   *password
                      )
#endif /* NDEBUG */
{
  assert(password != NULL);
  assert(password->data != NULL);

  #ifndef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,password,sizeof(Password));
  #endif

  Password_freeSecure(password->data);
}

#ifdef NDEBUG
  Password *Password_new(void)
#else /* not NDEBUG */
  Password *__Password_new(const char *__fileName__,
                           ulong      __lineNb__
                          )
#endif /* NDEBUG */
{
  Password *password;

  password = (Password*)malloc(sizeof(Password));
  if (password == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  #ifndef NDEBUG
    __Password_init(__fileName__,__lineNb__,password);
  #else /* not NDEBUG */
    Password_init(password);
  #endif /* NDEBUG */

  return password;
}

#ifdef NDEBUG
  Password *Password_newString(const String string)
#else /* not NDEBUG */
  Password *__Password_newString(const char   *__fileName__,
                                 ulong        __lineNb__,
                                 const String string
                                )
#endif /* NDEBUG */
{
  Password *password;

  #ifndef NDEBUG
    password = __Password_new(__fileName__,__lineNb__);
  #else /* not NDEBUG */
    password = Password_new();
  #endif /* NDEBUG */
  if (password != NULL)
  {
    Password_setString(password,string);
  }

  return password;
}

#ifdef NDEBUG
  Password *Password_newCString(const char *s)
#else /* not NDEBUG */
  Password *__Password_newCString(const char *__fileName__,
                                  ulong      __lineNb__,
                                  const char *s
                                 )
#endif /* NDEBUG */
{
  Password *password;

  #ifndef NDEBUG
    password = __Password_new(__fileName__,__lineNb__);
  #else /* not NDEBUG */
    password = Password_new();
  #endif /* NDEBUG */
  if (password != NULL)
  {
    Password_setCString(password,s);
  }

  return password;
}

#ifdef NDEBUG
  Password *Password_duplicate(const Password *fromPassword)
#else /* not NDEBUG */
  Password *__Password_duplicate(const char     *__fileName__,
                                 ulong          __lineNb__,
                                 const Password *fromPassword
                                )
#endif /* NDEBUG */
{
  Password *password;

  if (fromPassword != NULL)
  {
    #ifndef NDEBUG
      password = __Password_new(__fileName__,__lineNb__);
    #else /* not NDEBUG */
      password = Password_new();
    #endif /* NDEBUG */
    assert(password != NULL);
    Password_set(password,fromPassword);
  }
  else
  {
    password = NULL;
  }

  return password;
}

#ifdef NDEBUG
  void Password_delete(Password *password)
#else /* not NDEBUG */
  void __Password_delete(const char *__fileName__,
                         ulong      __lineNb__,
                         Password   *password
                        )
#endif /* NDEBUG */
{
  if (password != NULL)
  {
    #ifndef NDEBUG
      __Password_done(__fileName__,__lineNb__,password);
    #else /* not NDEBUG */
      Password_done(password);
    #endif /* NDEBUG */
    free(password);
  }
}

void Password_clear(Password *password)
{
  assert(password != NULL);

  password->data[0]    = '\0';
  password->dataLength = 0;
}

void Password_set(Password *password, const Password *fromPassword)
{
  assert(password != NULL);

  if (fromPassword != NULL)
  {
    memmove(password->data,fromPassword->data,MAX_PASSWORD_LENGTH+1);
    password->dataLength = fromPassword->dataLength;
  }
  else
  {
    password->data[0]    = '\0';
    password->dataLength = 0;
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
  password->dataLength   = length;
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
  password->dataLength   = length;
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
    memmove(password->data,buffer,length);
  #else /* not HAVE_GCRYPT */
    p = (char*)buffer;
    for (z = 0; z < length; z++)
    {
      password->data[z] = p[z]^obfuscator[z];
    }
  #endif /* HAVE_GCRYPT */
  password->data[length] = '\0';
  password->dataLength   = length;
}

void Password_appendChar(Password *password, char ch)
{
  assert(password != NULL);

  if (password->dataLength < MAX_PASSWORD_LENGTH)
  {
    #ifdef HAVE_GCRYPT
      password->data[password->dataLength] = ch;
    #else /* not HAVE_GCRYPT */
      password->data[password->dataLength] = ch^obfuscator[password->dataLength];
    #endif /* HAVE_GCRYPT */
    password->dataLength++;
    password->data[password->dataLength] = '\0';
  }
}

void Password_random(Password *password, uint length)
{
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    uint i;
  #endif /* HAVE_GCRYPT */

  assert(password != NULL);

  password->dataLength = MIN(length,MAX_PASSWORD_LENGTH);
  #ifdef HAVE_GCRYPT
    gcry_create_nonce((unsigned char*)password->data,password->dataLength);
  #else /* not HAVE_GCRYPT */
    srandom((unsigned int)time(NULL));
    for (i = 0; i < password->dataLength; i++)
    {
      password->data[i] = (char)(random()%256)^obfuscator[i];
    }
  #endif /* HAVE_GCRYPT */
}

uint Password_length(const Password *password)
{
  return (password != NULL)?password->dataLength:0;
}

bool Password_isEmpty(const Password *password)
{
  return (password == NULL) || (password->dataLength == 0);
}

char Password_getChar(const Password *password, uint index)
{
  if ((password != NULL) && (index < password->dataLength))
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
  uint i;

  assert(password != NULL);

  browniePoints    = 0;
  maxBrowniePoints = 0;

  // length >= 8
  CHECK(password->dataLength >= 8);

  // contain numbers
  flag0 = FALSE;
  for (i = 0; i < password->dataLength; i++)
  {
    flag0 |= isdigit(password->data[i]);
  }
  CHECK(flag0);

  // contain special characters
  flag0 = FALSE;
  for (i = 0; i < password->dataLength; i++)
  {
    flag0 |= (strchr(" !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",password->data[i]) != NULL);
  }
  CHECK(flag0);

  // capital/non-capital letters
  flag0 = FALSE;
  flag1 = FALSE;
  for (i = 0; i < password->dataLength; i++)
  {
    flag0 |= (toupper(password->data[i]) != password->data[i]);
    flag1 |= (tolower(password->data[i]) != password->data[i]);
  }
  CHECK(flag0 && flag1);

  // estimate entropie by compression
// ToDo

  return (double)browniePoints/(double)maxBrowniePoints;

  #undef CHECK
}

const char *Password_deploy(const Password *password)
{
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    char *plain;
    uint i;
  #endif /* HAVE_GCRYPT */

  if (password != NULL)
  {
    assert(password->dataLength <= MAX_PASSWORD_LENGTH);

    #ifdef HAVE_GCRYPT
      return password->data;
    #else /* not HAVE_GCRYPT */
      plain = Password_allocSecure(password->dataLength+1);
      if (plain == NULL)
      {
        return NULL;
      }
      for (i = 0; i < password->dataLength; i++)
      {
        plain[i] = password->data[i]^obfuscator[i];
      }
      plain[password->dataLength] = '\0';
      return plain;
    #endif /* HAVE_GCRYPT */
  }
  else
  {
    return "";
  }
}

void Password_undeploy(const Password *password, const char *plain)
{
  if (password != NULL)
  {
    #ifdef HAVE_GCRYPT
      UNUSED_VARIABLE(password);
      UNUSED_VARIABLE(plain);
    #else /* not HAVE_GCRYPT */
      memset((char*)plain,0,MAX_PASSWORD_LENGTH);
      Password_freeSecure(plain);
    #endif /* HAVE_GCRYPT */
  }
}

bool Password_equals(const Password *password0, const Password *password1)
{
  #ifdef HAVE_GCRYPT
  #else /* not HAVE_GCRYPT */
    uint i;
  #endif /* HAVE_GCRYPT */

  if (   (password0 != NULL)
      && (password1 != NULL)
      && (password0->dataLength == password1->dataLength)
     )
  {
    #ifdef HAVE_GCRYPT
      return memcmp(password0->data,password1->data,password0->dataLength) == 0;
    #else /* not HAVE_GCRYPT */
      for (i = 0; i < password0->dataLength; i++)
      {
        if ((password0->data[i]^obfuscator[i]) != (password1->data[i]^obfuscator[i])) return FALSE;
      }
    #endif /* HAVE_GCRYPT */
  }
  else
  {
    return FALSE;
  }

  return TRUE;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
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
      printInfo(2,"OK\n");

      // close pipe
      pclose(file);

      okFlag = TRUE;
    }
  }

  // input via console
  #if   defined(PLATFORM_LINUX)
    if (((modes & PASSWORD_INPUT_MODE_CONSOLE) != 0) && !okFlag)
    {
      int            n;
      struct termios oldTermioSettings;
      struct termios termioSettings;
      bool           eolFlag,eofFlag;
      char           ch;

      if (isatty(STDIN_FILENO) == 1)
      {
        // read data from interactive input
        if (message != NULL)
        {
          (void)write(STDOUT_FILENO,message,strlen(message));
          (void)write(STDOUT_FILENO,": ",2);
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
        eofFlag = FALSE;
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
            eofFlag = TRUE;
          }
        }
        while (!eolFlag && !eofFlag);

        // restore console settings
        tcsetattr(STDIN_FILENO,TCSANOW,&oldTermioSettings);

        if (message != NULL)
        {
          (void)write(STDOUT_FILENO,"\n",1);
        }
      }
      else
      {
        // read data from non-interactive input
        eolFlag = FALSE;
        eofFlag = FALSE;
        do
        {
          /* Note: sometimes FIONREAD does not return available characters
                   immediately. Thus delay program execution and try again.
          */
          ioctl(STDIN_FILENO,FIONREAD,(char*)&n);
          if (n <= 0)
          {
            Misc_udelay(1000);
            ioctl(STDIN_FILENO,FIONREAD,(char*)&n);
          }
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
              eofFlag = TRUE;
            }
          }
        }
        while (!eolFlag && !eofFlag);
      }
      okFlag = TRUE;
    }
  #elif defined(PLATFORM_WINDOWS)
// NYI ???
#warning no console input on windows
  #endif /* PLATFORM_... */

  return okFlag;
}
#pragma GCC diagnostic pop

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
  if (password->dataLength != verifyPassword.dataLength) equalFlag = FALSE;
  if (memcmp(password->data,verifyPassword.data,password->dataLength) != 0) equalFlag = FALSE;

  // free resources
  Password_done(&verifyPassword);

  return equalFlag;
}

#ifndef NDEBUG
void Password_dump(Password *password)
{
  uint i;

  assert(password != NULL);

  for (i = 0; i < password->dataLength; i++)
  {
    #ifdef HAVE_GCRYPT
      fprintf(stderr,"%02x",(byte)password->data[i]);
    #else /* not HAVE_GCRYPT */
      fprintf(stderr,"%02x",(byte)(password->data[i]^obfuscator[i]));
    #endif /* HAVE_GCRYPT */
  }
  fputs("\n",stderr);
  for (i = 0; i < password->dataLength; i++)
  {
    #ifdef HAVE_GCRYPT
      fprintf(stderr,"%c ",isprint(password->data[i]) ? password->data[i] : '.');
    #else /* not HAVE_GCRYPT */
      fprintf(stderr,"%c ",isprint(password->data[i]^obfuscator[i]) ? password->data[i]^obfuscator[i] : '.');
    #endif /* HAVE_GCRYPT */
  }
  fputs("\n",stderr);
}
#endif /* not NDEBUG */

#ifdef __cplusplus
  }
#endif

/* end of file */
