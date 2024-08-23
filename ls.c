#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

/*
 ****************************************************************************
 *
 * Function:    getlistensocket
 *
 * Description: This function creates a listen socket for a server.  The
 *              socket is bound to a specified address and the listen
 *              queue is set up.
 *
 * Parameters:  sa          - address to bind the port to
 *              maxreceives - size of the listen queue
 *
 * Returns:     descriptor for the socket or in case of error:
 *              -1 for a socket() error,
 *              -2 for a bind() error,
 *              -3 for a listen() error
 *
 ****************************************************************************
*/
int getlistensocket(struct sockaddr_in *sa, int maxreceives)
{
    int one = 1;
    int listensocket;

    /* create an TCP/IP socket */
    if ((listensocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    setsockopt(listensocket, SOL_SOCKET, SO_REUSEADDR,
       (char *)&one, sizeof(one));
	
    /* bind the socket to the specified address */
    if (bind(listensocket, (struct sockaddr *)sa, sizeof(struct sockaddr_in)) < 0) {
        close(listensocket);
        return -2;
    }

    /* set the size of the listen queue */
    if (listen(listensocket, maxreceives) < 0) {
        close(listensocket);
        return -3;
    }

    return listensocket;
}
