#ifndef _WESNET_H
#define _WESNET_H

#if !defined(_COMMON_H_)
#	include "common.h"
#endif

#define WSN_NO_TIMEOUT (-2)

int ws_accept(int s, struct sockaddr * cliaddr, SOCKLEN_T *addrlen, int seconds);
int ws_connect(int s, struct sockaddr * cliaddr, SOCKLEN_T addrlen, int seconds);

#endif
