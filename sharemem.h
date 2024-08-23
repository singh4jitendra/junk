#ifndef __SHAREMEMH__
#define __SHAREMEMH__

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/* Returns a pointer to the shared memory segment */
void *GetSharedMemory(int *id, key_t key, int size, int flags);

#endif
