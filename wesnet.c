#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "wesnet.h"

int ws_accept(int s, struct sockaddr * cliaddr, SOCKLEN_T *addrlen, int seconds)
{
	int cs;
	int ready;
	fd_set set;
	struct timeval * pto;
	struct timeval timeout;

	if (seconds == WSN_NO_TIMEOUT) {
		pto = NULL;
	}
	else {
		pto = &timeout;
		pto->tv_sec = seconds;
		pto->tv_usec = 0;
	}

	FD_ZERO(&set);
	FD_SET(s, &set);

	ready = select(s + 1, &set, NULL, NULL, &timeout);
	if (ready <= 0) {
		cs = -1;
		if (ready == 0) {
			/* timeout occurred, treat as if interrupted by a system call */
			errno = EINTR;
		}
	}
	else {
		cs = accept(s, cliaddr, addrlen);
	}

	return cs;
}

int ws_connect(int s, struct sockaddr * rmtaddr, SOCKLEN_T addrlen, int seconds)
{
	int rv;
	int flags;
	int ready;
	fd_set set;
	struct timeval * pto;
	struct timeval timeout;

	if (seconds == WSN_NO_TIMEOUT) {
		pto = NULL;
	}
	else {
		pto = &timeout;
		pto->tv_sec = seconds;
		pto->tv_usec = 0;
	}

	/* for this to work, the socket must be set to nonblocking */
	flags = fcntl(s, F_GETFL);

	if (!(flags & O_NONBLOCK)) {
		fcntl(s, F_SETFL, flags | O_NONBLOCK);
	}

	rv = connect(s, rmtaddr, addrlen);
	if ((rv == -1) && (errno == EINPROGRESS)) {
		FD_ZERO(&set);
		FD_SET(s, &set);
		
		ready = select(s + 1, NULL, &set, NULL, &timeout);
		if (ready <= 0) {
			rv = -1;
			if (ready == 0) {
				/* timeout occurred, treat as if interrupted by a system call */
				errno = EINTR;
			}
		}
		else {
			rv = connect(s, rmtaddr, addrlen);
		}
	}

	/* reset the file status flags */
	fcntl(s, F_SETFL, flags);

	return rv;
}
