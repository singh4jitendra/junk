#ifndef _WESTCP_H_
#define _WESTCP_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "msdefs.h"

int recvdata(int s, wesco_void_ptr_t buffer, size_t buffsize, int flags);
int senddata(int s, wesco_void_ptr_t buffer, size_t buffsize, int flags);
int readn(int fd, wesco_void_ptr_t buffer, size_t buffsize);
int writen(int fd, wesco_void_ptr_t buffer, size_t buffsize);

#endif
