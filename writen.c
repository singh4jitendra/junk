#include <sys/errno.h>
#include <string.h>

#include "westcp.h"

extern int errno;

/*
 ******************************************************************************
 *
 * Name:        writen
 *
 * Description: This function writes a buffer out to a specified file
 *              descriptor.
 *
 * Parameters:  fd       - File descriptor.
 *              buffer   - Pointer to buffer to write out.
 *              buffsize - Size of the buffer.
 *
 * Returns:     The number of bytes written; or, -1 on error.
 *
 ******************************************************************************
*/
int writen(int fd, wesco_void_ptr_t buffer, size_t buffsize)
{
    int numbytes;

    /* loop until all data is sent or an error has occurred */
    /* EINTR errors are ignored */
    while ((numbytes = write(fd, (char *)buffer, buffsize)) < buffsize) {
      if (numbytes < 0)
        if (errno == EINTR)
          continue;
        else
          return -1;

      buffsize -= numbytes;
    }

    return buffsize;
}
