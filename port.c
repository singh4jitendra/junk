#include <netdb.h>
#include <string.h>

#if !defined(_WIN32)
#   include "msdefs.h"
#endif

#include "westcp.h"

/*
 *****************************************************************************
 *
 * Function:    initaddress
 *
 * Description: This function sets up the address for a server.  The service
 *              specified in the parameters is looked up in the
 *              /etc/services file.  The port for that service is used as
 *              the port for the socket.
 *
 * Parameters:  sa       - Pointer to socket address structure
 *              service  - Pointer to service string.
 *              protocol - Pointer to protocol string.
 *
 * Returns:     0 if successful, -1 on error
 *
 * Changes:     Removed a call to free that tried to deallocate static
 *              memory used by getservbyname.  - mjb 9/20/99
 *****************************************************************************
*/
int initaddress(struct sockaddr_in *sa, char *service, char *protocol)
{
    struct servent *sp;

    /* zero out memory */
    memset((char *)sa, 0, sizeof(struct sockaddr_in));

    /* look up service in /etc/services file */
    if ((sp = getservbyname(service, protocol)) == NULL) {
        err_ret("service %s / %s is not in /etc/services", service, protocol);
        return -1;
    }

    /* set socket address information */
    sa->sin_family = AF_INET;
    sa->sin_addr.s_addr = INADDR_ANY;
    sa->sin_port = sp->s_port;

    return 0;
}
