#include <sys/types.h>
#include <string.h>

#include "westcp.h"

extern int errno;

/*
 ******************************************************************************
 *
 * Name:        readn
 *
 * Description: This function reads a specified number of bytes into a buffer
 *              from a file descriptor.
 *
 * Parameters:  fd       - File descriptor.
 *              buffer   - Pointer to buffer.
 *              buffsize - Size of buffer (in bytes).
 *
 * Returns: The number of bytes read; or -1 on error.
 *
 ******************************************************************************
*/
int readn(int fd, wesco_void_ptr_t buffer, size_t buffsize)
{
    int numbytes = 0;              /* Bytes read in from read()  */             
    int total = 0;                 /* Total bytes read in        */
    char *b;                       /* Current position in buffer */
    short readbytes = buffsize;    /* Bytes left to read in      */

    b = (char *)buffer;

    /* read bytes into buffer */
    while (readbytes > 0) {
      if ((numbytes = read(fd, b, readbytes)) < 0) {
        if (errno == EINTR)
          continue;
        else
          return -1;
      } else if (numbytes == 0)
        return 0;

      readbytes -= numbytes;
      b += numbytes;
      total += numbytes;
    }

    return total;
}
