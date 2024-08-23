#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#if defined(_DEBUG_ME_BABY)
#   include <unistd.h>
#   include <stdio.h>
#endif

#include "msdefs.h"

int recvdata(wesco_socket_t s, wesco_void_ptr_t buffer, size_t buffsize,
	int flags)
{
	int fd_flags;
	fd_set in_set;
	int fds_ready;
	int total = 0;
	int numbytes = 0;
	char *buf = buffer;
	int max_fd = s + 1;
	struct timeval timeout = { 120, 0 };

	fd_flags = fcntl(s, F_GETFD);
	fcntl(s, F_SETFD, fd_flags|O_NONBLOCK);

	while (buffsize > 0) {
		FD_ZERO(&in_set);
		FD_SET(s, &in_set);

		fds_ready = select(max_fd, &in_set, (fd_set *)NULL, (fd_set *)NULL,
			&timeout);
		if (fds_ready < 0) {          /* an error has occurred */
			fcntl(s, F_SETFD, fd_flags);
			return -1;
		}
		else if (fds_ready == 0) {    /* timeout has occurred */
			fcntl(s, F_SETFD, fd_flags);
			return total;
		}
		else {                        /* data received, read it */
			numbytes = recv(s, buf, buffsize, flags);
			if (numbytes < 0) {        /* an error has occurred */
				fcntl(s, F_SETFD, fd_flags);
				return -1;
			}
			else if (numbytes == 0) {  /* received EOF */
				fcntl(s, F_SETFD, fd_flags);
				return total;
			}
			else {                     /* data read successfully */
				buffsize -= numbytes;
				buf += numbytes;
				total += numbytes;
			}
		}
	}

	fcntl(s, F_SETFD, fd_flags);

	return total;
}
