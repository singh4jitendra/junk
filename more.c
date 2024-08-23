#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef _GNU_SOURCE
#include <stropts.h>
#endif
#include <netinet/in.h>
#include <string.h>

#ifndef _GNU_SOURCE
#include <sys/tiuser.h>
#endif

#include <time.h>

#if !defined(_HPUX_SOURCE) && !defined(_GNU_SOURCE)
#   include <netinet/tcp_timer.h>
#endif

#include "more.h"

/*
 ****************************************************************************
 *
 * Function:    spipe
 *
 * Description: Creates a socket pipe.  The socket is a Unix domain stream
 *              socket.
 *
 * Parameters:  spair - Array containing the descriptors for the two sockets
 *                      that were created for the pipe.
 *
 * Returns:     -1 on error; otherwise, the descriptor of the second socket
 *              that was created.
 *
 ****************************************************************************
*/
int spipe(int spair[2])
{
    return socketpair(AF_UNIX, SOCK_STREAM, 0, spair);
}

/*
 ****************************************************************************
 *
 * Function:    sendfile
 *
 * Description: Sends an "open file" to another process via a socket pipe.
 *              The "open file" is the descriptor for either a file or a
 *              socket.  This allows one process to open "the" file and then
 *              pass it to another process which can use the "file" without
 *              having to open it.
 *
 * Parameters:  s       - Descriptor of socket on which to send the message.
 *              filedes - Descriptor of the file or socket to send.
 *
 * Returns:     -1 on error; otherwise, the number of bytes sent.
 *
 ****************************************************************************
*/
/*int sendfile(int s, int filedes)
{
    return ioctl(s, I_SENDFD, filedes);
}
*/
/*
 ****************************************************************************
 *
 * Function:    recvfile
 *
 * Description: Receives a "file" descriptor from another process.  This
 *              "file" is already opened.  It can be either a file or a
 *              socket.  This allows one process to open "the" file and then
 *              pass it to another process which can use the "file" without
 *              having to open it.
 *
 * Parameters:  s       - Descriptor of socket on which to send the message.
 *
 * Returns:     -1 on error; otherwise, the number of bytes received.
 *
 ****************************************************************************
*/
/*int recvfile(int s)
{
    struct strrecvfd recvfd;

    if (ioctl(s, I_RECVFD, (char *)&recvfd) < 0)
        return -1;

    return recvfd.fd;
}
*/
/*
 ****************************************************************************
 *
 * Function:    setsocketbuffersize
 *
 * Description: Sets the socket option for the send and receive buffer
 *              sizes.
 *
 * Parameters:  s            - Descriptor of socket for which to set the
 *                             option.
 *              whichbuffer  - Specify which buffer, send (SENDBUFFFER) or
 *                             receive (RECEIVEBUFFFER).
 *              size         - New size of the buffer.
 *
 * Returns:     -1 on error; otherwise, 0.
 *
 ****************************************************************************
*/
int setsocketbuffersize(int s, int whichbuffer, int size)
{
    return setsockopt(s, SOL_SOCKET, whichbuffer, (char *)&size, 
        sizeof(size));
}

/*
 ****************************************************************************
 *
 * Function:    setsocketdebug
 *
 * Description: Sets the debug socket option to on or off.
 *
 * Parameters:  s     - Descriptor of socket for which to send the option.
 *              state - State to set option.  (ON or OFF).
 *
 * Returns:     -1 on error; otherwise, 0.
 *
 ****************************************************************************
*/
int setsocketdebug(int s, int state)
{
    return setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&state, 
        sizeof(state));
}

/*
 ****************************************************************************
 *
 * Function:    setsocketkeepalive
 *
 * Description: Sets the keep alive socket option to on or off.
 *
 * Parameters:  s     - Descriptor of socket for which to send the option.
 *              state - State to set option.  (ON or OFF).
 *
 * Returns:     -1 on error; otherwise, 0.
 *
 ****************************************************************************
*/
int setsocketkeepalive(int s, int state)
{
    return setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&state, 
        sizeof(state));
}

int setsocketkeepidle(int s, int seconds)
{
#if 0
	if (seconds < TCPTV_MIN_KEEPIDLE)
		return -1;

	return setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, (char *)&seconds,
		sizeof(seconds));
#else
	return 0;
#endif
}

int setsocketkeepinterval(int s, int seconds)
{
#if 0
	if ((seconds < TCPTV_MIN_KEEPINTVL) && (seconds != 0))
		return -1;

	return setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, (char *)&seconds,
		sizeof(seconds));
#else
	return 0;
#endif
}

int setsocketnumkeeps(int s, int max)
{
#if 0
	if (max < 1)
		return -1;

	return setsockopt(s, IPPROTO_TCP, TCP_NKEEP, (char *)&max, sizeof(max));
#else
	return 0;
#endif
}

int setsocketkeepoptions(int s, char *key, char * filename)
{
	int keepidle, keepinterval, numkeeps;

	setsocketkeepalive(s, 1);

	keepidle = GetPrivateProfileInt(key, "keep-idle", -1, filename);
	setsocketkeepidle(s, keepidle);

	keepinterval = GetPrivateProfileInt(key, "keep-interval", -1, filename);
	setsocketkeepinterval(s, keepinterval);

	numkeeps= GetPrivateProfileInt(key, "num-keeps", -1, filename);
	setsocketnumkeeps(s, numkeeps);

	return 0;
}

/*
 ****************************************************************************
 *
 * Function:    setsocketlinger
 *
 * Description: Sets the debug socket option to on or off.
 *
 * Parameters:  s       - Descriptor of socket for which to send the option.
 *              state   - State to set option.  (ON or OFF).
 *              seconds - Number of seconds to linger.
 *
 * Returns:     -1 on error; otherwise, 0.
 *
 ****************************************************************************
*/
int setsocketlinger(int s, int state, int seconds)
{
    struct linger socketlinger;

    socketlinger.l_onoff = state;
    socketlinger.l_linger = seconds;

    return setsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&socketlinger,
        sizeof(socketlinger));
}

/*
 ****************************************************************************
 *
 * Function:    noblock
 *
 * Description: Sets the socket to non-blocking.
 *
 * Parameters:  s       - Descriptor of socket.
 *
 * Returns:     -1 on error.
 *
 ****************************************************************************
*/
int noblock(int s)
{
    return fcntl(s, F_SETFL, O_NONBLOCK);
}

/*
 ****************************************************************************
 *
 * Function:    writetolog
 *
 * Description: Writes a buffer to a log file.
 *
 * Parameters:  fd       - Descriptor of log file.
 *              buffer   - Pointer to character buffer.
 *              buffsize - Number of bytes to write.
 *
 * Returns:     -1 on error.
 *
 ****************************************************************************
*/
int writetolog(int fd, char *buffer, int buffsize, int printtime)
{
    char *timeptr;
    time_t timestamp;

    if (printtime) {
        /* Write time to the log */
        time(&timestamp);
        timeptr = ctime(&timestamp);

        /* Print all but the trailing carriage return */
        write(fd, timeptr, strlen(timeptr) - 1);
        write(fd, " - ", 3);

/*        free(timeptr); */
    }

    /* Write buffer and then unlock file */
    write(fd, buffer, buffsize);

    return 0;
}
