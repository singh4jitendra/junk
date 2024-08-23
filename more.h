#ifndef _MORE_H_
#define _MORE_H_

#ifndef _SYS_SOCKET_H_
#include <sys/socket.h>
#endif

#define SENDBUFFER      SO_SNDBUF
#define RECEIVEBUFFER   SO_RCVBUF

#define OFF             0
#define ON              1

#if !defined(_HPUX_SOURCE)
int sendfile(int s, int filedes);
#endif

int spipe(int spair[2]);
int recvfile(int s);
int setsocketbuffersize(int s, int whichbuffer, int size);
int setsocketdebug(int s, int state);
int setsocketkeepalive(int s, int state);
int setsocketkeepidle(int s, int seconds);
int setsocketkeepinterval(int s, int seconds);
int setsocketnumkeeps(int s, int max);
int setsocketkeepoptions(int s, char *key, char *filename);
int setsocketlinger(int s, int state, int seconds);

#endif
