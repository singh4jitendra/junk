#ifndef __SOCKETQUEUEH__
#define __SOCKETQUEUEH__

struct SOCKETQUEUE {
    int socket;
    struct SOCKETQUEUE *next;
};

typedef struct SOCKETQUEUE SOCKETQUEUE;

int AddSocket(SOCKETQUEUE **, int newSocket);
int GetSocket(SOCKETQUEUE **);

#endif
