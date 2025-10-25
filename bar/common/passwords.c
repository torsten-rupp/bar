/***********************************************************************\
*
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

#include "common/global.h"
#include "common/strings.h"
#include "common/misc.h"

#include "errors.h"

#include "common/passwords.h"

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
    /* if libgrypt is not available a simple obfuscator is used here to
       avoid plain text passwords in memory as much as possible
    */
    #if   defined(PLATFORM_LINUX)
      srandom((unsigned int)time(NULL));
    #elif defined(PLATFORM_WINDOWS)
      srand((unsigned int)time(NULL));
    #endif /* PLATFORM_... */
    for (size_t i = 0; i < MAX_PASSWORD_LENGTH; i++)
    {
      #if   defined(PLATFORM_LINUX)
        obfuscator[i] = (char)(random()%256);
      #elif defined(PLATFORM_WINDOWS)
        obfuscator[i] = (char)(rand()%256);
      #endif /* PLATFORM_... */
    }
  #endif /* not HAVE_GCRYPT */

  return ERROR_NONE;
}

void Password_doneAll(void)
{
}

#ifdef NDEBUG
  void Password_init(Password *password)
#else /* not NDEBUG */
  void __Password_init(const char *__fileName__,
                       size_t     __lineNb__,
                       Password   *password
                      )
#endif /* NDEBUG */
{
  assert(password != NULL);

  password->data = allocSecure(MAX_PASSWORD_LENGTH+1);
  if (password->data == NULL)
  {
    HALT_INSUFFICIENT_MEMORY();
  }
  password->data[0]    = '\0';
  password->dataLength = 0;

  #ifdef NDEBUG
    DEBUG_ADD_RESOURCE_TRACE(password,Password);
  #else
    DEBUG_ADD_RESOURCE_TRACEX(__fileName__,__lineNb__,password,Password);
  #endif
}

#ifdef NDEBUG
  void Password_initDuplicate(Password *password, const Password *fromPassword)
#else /* not NDEBUG */
  void __Password_initDuplicate(const char     *__fileName__,
                                size_t         __lineNb__,
                                Password       *password,
                                const Password *fromPassword
                               )
#endif /* NDEBUG */
{
  assert(password != NULL);
  assert(fromPassword != NULL);

  #ifdef NDEBUG
    Password_init(password);
  #else
    __Password_init(__fileName__,__lineNb__,password);
  #endif

  Password_set(password,fromPassword);
}

#ifdef NDEBUG
  void Password_done(Password *password)
#else /* not NDEBUG */
  void __Password_done(const char *__fileName__,
                       size_t     __lineNb__,
                       Password   *password
                      )
#endif /* NDEBUG */
{
  assert(password != NULL);
  assert(password->data != NULL);

  #ifdef NDEBUG
    DEBUG_REMOVE_RESOURCE_TRACE(password,Password);
  #else
    DEBUG_REMOVE_RESOURCE_TRACEX(__fileName__,__lineNb__,password,Password);
  #endif

  freeSecure(password->data);
}

#ifdef NDEBUG
  Password *Password_new(void)
#else /* not NDEBUG */
  Password *__Password_new(const char *__fileName__,
                           size_t     __lineNb__
                          )
#endif /* NDEBUG */
{
  Password *password = (Password*)malloc(sizeof(Password));
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
                                 size_t       __lineNb__,
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
                                  size_t     __lineNb__,
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
                                 size_t         __lineNb__,
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
                         size_t     __lineNb__,
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

  if (!Password_isEmpty(fromPassword))
  {
    memmove(password->data,fromPassword->data,MAX_PASSWORD_LENGTH+1);
    password->dataLength = fromPassword->dataLength;
  }
  else
  {
    Password_clear(password);
  }
}

void Password_setString(Password *password, const String string)
{
  assert(password != NULL);

  uint length = MIN(String_length(string),MAX_PASSWORD_LENGTH);
  #ifdef HAVE_GCRYPT
    memcpy(password->data,String_cString(string),length);
  #else /* not HAVE_GCRYPT */
    for (uint i = 0; i < length; i++)
    {
      password->data[i] = String_index(string,i)^obfuscator[i];
    }
  #endif /* HAVE_GCRYPT */
  password->data[length] = '\0';
  password->dataLength   = length;
}

void Password_setCString(Password *password, const char *s)
{
  assert(password != NULL);

  uint length = MIN(strlen(s),MAX_PASSWORD_LENGTH);
  #ifdef HAVE_GCRYPT
    memcpy(password->data,s,length);
  #else /* not HAVE_GCRYPT */
    for (uint i = 0; i < length; i++)
    {
      password->data[i] = s[i]^obfuscator[i];
    }
  #endif /* HAVE_GCRYPT */
  password->data[length] = '\0';
  password->dataLength   = length;
}

void Password_setBuffer(Password *password, const void *buffer, uint length)
{
  assert(password != NULL);

  length = MIN(length,MAX_PASSWORD_LENGTH);
  #ifdef HAVE_GCRYPT
    memmove(password->data,buffer,length);
  #else /* not HAVE_GCRYPT */
    char * p = (char*)buffer;
    for (uint i = 0; i < length; i++)
    {
      password->data[i] = p[i]^obfuscator[i];
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
  assert(password != NULL);

  password->dataLength = MIN(length,MAX_PASSWORD_LENGTH);
  #ifdef HAVE_GCRYPT
    gcry_create_nonce((unsigned char*)password->data,password->dataLength);
  #else /* not HAVE_GCRYPT */
    #if   defined(PLATFORM_LINUX)
      srandom((unsigned int)time(NULL));
    #elif defined(PLATFORM_WINDOWS)
      srand((unsigned int)time(NULL));
    #endif /* PLATFORM_... */
    for (uint i = 0; i < password->dataLength; i++)
    {
      #if   defined(PLATFORM_LINUX)
        password->data[i] = (char)(random()%256)^obfuscator[i];
      #elif defined(PLATFORM_WINDOWS)
        password->data[i] = (char)(rand()%256)^obfuscator[i];
      #endif /* PLATFORM_... */
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

  assert(password != NULL);

  uint browniePoints    = 0;
  uint maxBrowniePoints = 0;

  // length >= 8
  CHECK(password->dataLength >= 8);

  bool flag0,flag1;

  // contain numbers
  flag0 = FALSE;
  for (uint i = 0; i < password->dataLength; i++)
  {
    flag0 |= isdigit(password->data[i]);
  }
  CHECK(flag0);

  // contain special characters
  flag0 = FALSE;
  for (uint i = 0; i < password->dataLength; i++)
  {
    flag0 |= (strchr(" !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",password->data[i]) != NULL);
  }
  CHECK(flag0);

  // capital/non-capital letters
  flag0 = FALSE;
  flag1 = FALSE;
  for (uint i = 0; i < password->dataLength; i++)
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
  if (password != NULL)
  {
    assert(password->dataLength <= MAX_PASSWORD_LENGTH);

    #ifdef HAVE_GCRYPT
      return password->data;
    #else /* not HAVE_GCRYPT */
      char *plain = allocSecure(password->dataLength+1);
      if (plain == NULL)
      {
        return NULL;
      }
      for (uint i = 0; i < password->dataLength; i++)
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

void Password_undeploy(const Password *password, const char *plainPassword)
{
  if (password != NULL)
  {
    #ifdef HAVE_GCRYPT
      UNUSED_VARIABLE(password);
      UNUSED_VARIABLE(plainPassword);
    #else /* not HAVE_GCRYPT */
      memClear((char*)plainPassword,password->dataLength);
      freeSecure((char*)plainPassword);
    #endif /* HAVE_GCRYPT */
  }
}

bool Password_equals(const Password *password0, const Password *password1)
{
  if (   (password0 != NULL)
      && (password1 != NULL)
      && (password0->dataLength == password1->dataLength)
     )
  {
    #ifdef HAVE_GCRYPT
      return memcmp(password0->data,password1->data,password0->dataLength) == 0;
    #else /* not HAVE_GCRYPT */
      for (uint i = 0; i < password0->dataLength; i++)
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

#if (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#endif
bool Password_input(Password   *password,
                    const char *message,
                    uint       modes
                   )
{
  assert(password != NULL);

  Password_clear(password);

  bool okFlag = FALSE;

  // input via SSH_ASKPASS program
  if (((modes & PASSWORD_INPUT_MODE_GUI) != 0) && !okFlag)
  {
    const char * sshAskPassword = getenv("SSH_ASKPASS");
    if (!stringIsEmpty(sshAskPassword))
    {
      // open pipe to external password program
      String command = String_newCString(sshAskPassword);
      if (message != NULL)
      {
        String_appendFormat(command," %\"s:",message);
      }
      FILE *file = popen(String_cString(command),"r");
      if (file == NULL)
      {
        String_delete(command);
        return FALSE;
      }
      String_delete(command);

      // read password, discard last LF
      bool eolFlag = FALSE;
      do
      {
        char ch = getc(file);
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

      // close pipe
      pclose(file);

      okFlag = TRUE;
    }
  }

  // input via console
  #if   defined(PLATFORM_LINUX)
    if (((modes & PASSWORD_INPUT_MODE_CONSOLE) != 0) && !okFlag)
    {
      if (isatty(STDIN_FILENO) == 1)
      {
        // read data from interactive input
        if (message != NULL)
        {
          ssize_t writtenBytes = write(STDOUT_FILENO,message,strlen(message));
          (void)writtenBytes;
          writtenBytes = write(STDOUT_FILENO,": ",2);
          (void)writtenBytes;
        }

        // save current console settings
        struct termios oldTermioSettings;
        if (tcgetattr(STDIN_FILENO,&oldTermioSettings) != 0)
        {
          return FALSE;
        }

        // disable echo
        struct termios termioSettings;
        memcpy(&termioSettings,&oldTermioSettings,sizeof(struct termios));
        termioSettings.c_lflag &= ~ECHO;
        if (tcsetattr(STDIN_FILENO,TCSANOW,&termioSettings) != 0)
        {
          return FALSE;
        }

        // input password
        bool eolFlag = FALSE;
        bool eofFlag = FALSE;
        do
        {
          char ch;
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
          size_t writtenBytes = write(STDOUT_FILENO,"\n",1);
          (void)writtenBytes;
        }
      }
      else
      {
        // read data from non-interactive input
        bool eolFlag = FALSE;
        bool eofFlag = FALSE;
        do
        {
          /* Note: sometimes FIONREAD does not return available characters
                   immediately. Thus delay program execution and try again.
          */
          int n = 0;
          ioctl(STDIN_FILENO,FIONREAD,(char*)&n);
          if (n <= 0)
          {
            Misc_udelay(1000);
            ioctl(STDIN_FILENO,FIONREAD,(char*)&n);
          }
          if (n > 0)
          {
            char ch;
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
          else
          {
            eofFlag = TRUE;
          }
        }
        while (!eolFlag && !eofFlag);
      }
      okFlag = TRUE;
    }
  #elif defined(PLATFORM_WINDOWS)
// NYI ???
#ifndef WERROR
#warning no console input on windows
#endif
  #endif /* PLATFORM_... */

  return okFlag;
}
#if (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#endif

bool Password_inputVerify(const Password *password,
                          const char     *message,
                          uint           modes
                         )
{
  // read passsword again
  Password verifyPassword;
  Password_init(&verifyPassword);
  if (!Password_input(&verifyPassword,message,modes))
  {
    Password_done(&verifyPassword);
    return FALSE;
  }

  // verify password
  bool equalFlag = TRUE;
  if (password->dataLength != verifyPassword.dataLength) equalFlag = FALSE;
  if (memcmp(password->data,verifyPassword.data,password->dataLength) != 0) equalFlag = FALSE;

  // free resources
  Password_done(&verifyPassword);

  return equalFlag;
}

#ifndef NDEBUG
void Password_dump(const Password *password)
{
  assert(password != NULL);

  fprintf(stderr,"Password:\n");
  for (uint i = 0; i < password->dataLength; i++)
  {
    #ifdef HAVE_GCRYPT
      fprintf(stderr,"%02x",(byte)password->data[i]);
    #else /* not HAVE_GCRYPT */
      fprintf(stderr,"%02x",(byte)(password->data[i]^obfuscator[i]));
    #endif /* HAVE_GCRYPT */
  }
  fputs("\n",stderr);
  for (uint i = 0; i < password->dataLength; i++)
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
