#if defined(_HP_TARGET)
#   define _INCLUDE_POSIX_SOURCE
#   define _INCLUDE_XOPEN_SOURCE
#   define _INCLUDE_HPUX_SOURCE
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

#include "westcp.h"

/*
 ******************************************************************************
 *
 * Name:        senddata
 *
 * Description: This function sends data on a connected socket.
 *
 * Parameters:  s        - Socket descriptor.
 *              buffer   - Pointer to buffer to send.
 *              buffsize - Size of the buffer (in bytes).
 *              flags    - Flags for sending data (ex. MSG_OOB).
 *
 * Returns:     The number of bytes sent; or, -1 on error.
 *
 ******************************************************************************
*/
int senddata(int s, wesco_void_ptr_t buffer, size_t buffsize, int flags)
{
    int numbytes;
    int total = 0;

    /* loop until all data is sent or an error has occurred */
    /* EWOULDBLOCK and EINTR errors are ignored */
    while ((numbytes = send(s, (char *)buffer + total, buffsize, flags)) < buffsize) {
      if (numbytes < 0)
        if ((errno == EWOULDBLOCK) || (errno == EINTR))
          continue;
        else
          return -1;

      buffsize -= numbytes;
      total += numbytes;
    }

    total += numbytes;

    return buffsize;
}
