#include "sharemem.h"
#include <stddef.h>

/*
 ******************************************************************************
 *
 * Name:        GetSharedMemory
 *
 * Description: This function gets a segment of shared memory and attaches
 *              to it.
 *
 * Parameters:  id    - Pointer to shared memory id.
 *              key   - Key for shared memory.
 *              size  - Size of shared memory (in bytes).
 *              flags - Special flags (ex. IPC_CREATE).
 *
 * Returns: 
 *
 ******************************************************************************
*/
void *GetSharedMemory(int *id, key_t key, int size, int flags)
{
    void *retVal;

    /* Get the shared memory */
    if ((*id = shmget(key, size, flags)) < 0)
        return (void *)0;

    /* Attach to the shared memory */
    if ((retVal = shmat((*id), (void *)NULL, 0)) == (void *)-1)
        return (void *)0;

    return retVal;
}
