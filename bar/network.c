/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/network.c,v $
* $Revision: 1.3 $
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

#include "network.h"

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

Errors Network_connect(SocketHandle *socketHandle,
                       String       hostName,
                       uint         hostPort
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
  if (connect((*socketHandle),
              (struct sockaddr*)&socketAddress,
              sizeof(socketAddress)
             ) != 0
     )
  {
    close(*socketHandle);
    return ERROR_CONNECT_FAIL;
  }

  /* enable non-blocking */
  fcntl(*socketHandle,F_SETFL,O_NONBLOCK);

  return ERROR_NONE;
}

void Network_disconnect(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);

  close(*socketHandle);
}

int Network_getSocket(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);

  return (int)(*socketHandle);
}

Errors Network_send(SocketHandle *socketHandle,
                    const void   *buffer,
                    ulong        length
                   )
{
  assert(socketHandle != NULL);

  return (send((*socketHandle),buffer,length,0) == length)?ERROR_NONE:ERROR_NETWORK_SEND;
}

Errors Network_receive(SocketHandle *socketHandle,
                       void         *buffer,
                       ulong        maxLength,
                       ulong        *receivedBytes
                      )
{
  long n;

  assert(socketHandle != NULL);
  assert(receivedBytes != NULL);

  n = recv((*socketHandle),buffer,maxLength,0);
  (*receivedBytes) = (n >= 0)?n:0;

  return ((*receivedBytes) >= 0)?ERROR_NONE:ERROR_NETWORK_RECEIVE;
}

Errors Network_initServer(SocketHandle *socketHandle,
                          uint         port
                         )
{
  struct sockaddr_in socketAddress;
  int                n;

  assert(socketHandle != NULL);

  (*socketHandle) = socket(AF_INET,SOCK_STREAM,0);
  if ((*socketHandle) == -1)
  {
    return ERROR_CONNECT_FAIL;
  }

  n = 1;
  if (setsockopt((*socketHandle),SOL_SOCKET,SO_REUSEADDR,&n,sizeof(int)) != 0)
  {
    close(*socketHandle);
    return ERROR_CONNECT_FAIL;
  }

  socketAddress.sin_family      = AF_INET;
  socketAddress.sin_addr.s_addr = INADDR_ANY;
  socketAddress.sin_port        = htons(port);
  if (bind((*socketHandle),
           (struct sockaddr*)&socketAddress,
           sizeof(socketAddress)
          ) != 0
     )
  {
    close(*socketHandle);
    return ERROR_CONNECT_FAIL;
  }
  listen((int)(*socketHandle),5);

  return ERROR_NONE;
}

void Network_doneServer(SocketHandle *socketHandle)
{
  assert(socketHandle != NULL);

  close((*socketHandle));
}

Errors Network_accept(SocketHandle *socketHandle,
                      SocketHandle *serverSocketHandle
                     )
{
  struct sockaddr_in socketAddress;
  socklen_t          socketAddressLength;

  assert(socketHandle != NULL);
  assert(serverSocketHandle != NULL);

  /* accept */
  socketAddressLength = sizeof(socketAddress);
  (*socketHandle) = accept((*serverSocketHandle),
                           (struct sockaddr*)&socketAddress,
                           &socketAddressLength
                          );
  if ((*socketHandle) == -1)
  {
    close(*socketHandle);
    return ERROR_CONNECT_FAIL;
  }

  /* enable non-blocking */
  fcntl(*socketHandle,F_SETFL,O_NONBLOCK);

  return ERROR_NONE;
}

void Network_getLocalInfo(SocketHandle *socketHandle,
                          String       name,
                          uint         *port
                         )
{
  struct sockaddr_in socketAddress;
  socklen_t          socketAddressLength;

  assert(socketHandle != NULL);
  assert(name != NULL);
  assert(port != NULL);

  socketAddressLength = sizeof(socketAddress);
  if (getsockname((*socketHandle),
                  (struct sockaddr*)&socketAddress,
                  &socketAddressLength
                 ) == 0
     )
  {
    String_setCString(name,inet_ntoa(socketAddress.sin_addr));
    (*port) = ntohs(socketAddress.sin_port);
  }
  else
  {
    String_setCString(name,"unknown");
    (*port) = 0;
  }
}

void Network_getRemoteInfo(SocketHandle *socketHandle,
                           String       name,
                           uint         *port
                          )
{
  struct sockaddr_in socketAddress;
  socklen_t          socketAddressLength;

  assert(socketHandle != NULL);
  assert(name != NULL);
  assert(port != NULL);

  socketAddressLength = sizeof(socketAddress);
  if (getpeername((*socketHandle),
                  (struct sockaddr*)&socketAddress,
                  &socketAddressLength
                 ) == 0
     )
  {
    String_setCString(name,inet_ntoa(socketAddress.sin_addr));
    (*port) = ntohs(socketAddress.sin_port);
  }
  else
  {
    String_setCString(name,"unknown");
    (*port) = 0;
  }
}

#ifdef __cplusplus
  }
#endif

/* end of file */
