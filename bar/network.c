/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/network.c,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: 
* Systems :
*
\***********************************************************************/

/****************************** Includes *******************************/
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "global.h"
#include "strings.h"
#include "errors.h"

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

Errors Network_init(void)
{
  return ERROR_NONE;
}

void Network_done(void)
{
}

Errors Network_connect(int    *socketHandle,
                       String hostName,
                       uint   hostPort
                      )
{
  struct hostent     *hostAddressEntry;
  in_addr_t          ipAddress;
  struct sockaddr_in socketAddress;

  assert(socketHandle != NULL);

  /* get host IP address */
  hostAddressEntry = gethostbyname(String_cString(hostName));
  if (hostAddressEntry != NULL)
  {
    assert(hostAddressEntry->h_length > 0);
    ipAddress = (*((in_addr_t*)hostAddressEntry->h_addr_list[0]));
  }
  else
  {
    ipAddress = inet_addr(String_cString(hostName));
  }
  if (ipAddress == INADDR_NONE)
  {
    return ERROR_HOST_NOT_FOUND;
  }

  /* connect */
  (*socketHandle) = socket(AF_INET,SOCK_STREAM,0);
  if ((*socketHandle) == -1)
  {
    return ERROR_CONNECT_FAIL;
  }
  socketAddress.sin_family      = AF_INET;
  socketAddress.sin_addr.s_addr = ipAddress;
  socketAddress.sin_port        = htons(hostPort);
  if (connect(*socketHandle,
              (struct sockaddr*)&socketAddress,
              sizeof(socketAddress)
             ) != 0
     )
  {
    close(*socketHandle);
    return ERROR_CONNECT_FAIL;
  }

  return ERROR_NONE;
}

void Network_disconnect(int socketHandle)
{
  assert(socketHandle >= 0);

  close(socketHandle);
}

Errors Network_send(int        socketHandle,
                    const void *buffer,
                    ulong      length
                   )
{
  assert(socketHandle >= 0);

  return (write(socketHandle,buffer,length) == length)?ERROR_NONE:ERROR_NETWORK_SEND;
}

Errors Network_receive(int   socketHandle,
                       void  *buffer,
                       ulong maxLength,
                       ulong *receivedBytes
                      )
{
  assert(socketHandle >= 0);
  assert(receivedBytes != NULL);

  (*receivedBytes) = read(socketHandle,buffer,maxLength);

  return ((*receivedBytes) >= 0)?ERROR_NONE:ERROR_NETWORK_RECEIVE;
}

#ifdef __cplusplus
  }
#endif

/* end of file */
