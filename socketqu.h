#ifndef __SOCKETQUEUEH__
#define __SOCKETQUEUEH__

struct SOCKETQUEUE {
    int socket;
    struct SOCKETQUEUE *next;
};

typedef struct SOCKETQUEUE SOCKETQUEUE;

int Push(SOCKETQUEUE **, int newSocket);
int Pop(SOCKETQUEUE **);

#endif
