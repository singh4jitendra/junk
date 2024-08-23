#ifndef __SOCKETQUEUEH__
#define __SOCKETQUEUEH__

typedef struct wesco_socket_t {
    int descriptor;
    wesco_socket_t *next;
} wesco_socket_t;

wesco_socket_t *heap = NULL,
       *list = NULL;

wesco_socket_t *PushSocket(int descriptor);
int PopSocket(void);

#endif
