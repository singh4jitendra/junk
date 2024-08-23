/*
 ******************************************************************************
 * Change History:
 * 09/29/06 mjb 06142 - Added new version for bonded inventory.
 ******************************************************************************
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>

#define WESCOM_CLIENT    1

#include "common.h"
#include "receive.h"
#include "update.h"
#include "reserve.h"
#include "readini.h"

/*
 ****************************************************************************
 *
 * Function:    WDOCRESERVE
 *
 * Description: This function calls the reserve server to reserve a
 *              specified amount of a certain SKU # on a remote machine.
 *
 * Parameters:  parameters - The parameters passed by the calling COBOL
 *                           function.
 *
 * Returns:     Nothing
 *
 ****************************************************************************
*/
void WDOCRESERVE(FUNCPARMS *parameters)
{
    int s;
    bool_t done = FALSE;
    int attempt = 0;
    struct hostent *hp;
    struct servent *sp;
    struct sockaddr_in peeraddr_in;
    char service[25];
    char protocol[25];
    char version[VERSIONLENGTH];
    char c_HostName[HOSTNAMELENGTH+1];
    int result;
    int timeout;
    struct sigaction alrmact;

    /* set up SIGALRM handler */
    alrmact.sa_handler = TCPAlarm;
    sigemptyset(&alrmact.sa_mask);
    alrmact.sa_flags = 0;
    sigaction(SIGALRM, &alrmact, NULL);
    
    GetPrivateProfileString("rsvserver", "version", version, sizeof(version),
        "10.03.00", WESCOMINI);
    GetPrivateProfileString("rsvserver", "service", service, sizeof(service),
        "WDOCRSV", WESCOMINI);
    GetPrivateProfileString("rsvserver", "protocol", protocol,
        sizeof(protocol), "tcp", WESCOMINI);
    timeout = GetPrivateProfileInt("rsvserver", "timeout", 30, WESCOMINI);

    CobolToC(c_HostName, parameters->hostname, HOSTNAMELENGTH+1,
        HOSTNAMELENGTH);
    
    /* Zero the address structures. */
    memset((char *)&peeraddr_in, 0, sizeof(struct sockaddr_in));

    /* Resolve the host information to a host in the /etc/hosts file. */
    if ((hp = ws_gethostbyname(c_HostName)) == NULL) {
        strncpy(parameters->status, "01", 2);
        return;
    }
    
    /* Look up service information in the /etc/services file. */
    if ((sp = getservbyname(service, protocol)) == NULL) {
        strncpy(parameters->status, "09", 2);
        return;
    }
    
    peeraddr_in.sin_family = AF_INET;
    peeraddr_in.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
    peeraddr_in.sin_port = sp->s_port;

    /* Create the socket. */
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        strncpy(parameters->status, "03", 2);
        return;
    }

    /* Connect to the remote service. */
    alarm(timeout);
    if (connect(s, (struct sockaddr *)&peeraddr_in, sizeof(struct sockaddr_in)) < 0) {
        strncpy(parameters->status, "02", 2);
        close(s);
        return;
    }
    alarm(0);

    while (!done) {
        /* Send the data to the remote computer. */
        strcpy(version, versionTable[attempt].number);
        alarm(timeout);
        if (send(s, version, VERSIONLENGTH, 0) != VERSIONLENGTH) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
        }
        alarm(0);

        /* Receive data from remote computer. */
        if (ReceiveData(s, version, VERSIONLENGTH, 0) == -1) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
        }

        if (!strncmp(version, "OK", 2))
            done = TRUE;
        else if (attempt < NUMVERSIONSUPPORTED)
            attempt++;
        else {
            result = strcmp(versionTable[0].number, version);
            if (result > 0)
                strncpy(parameters->status, "14", 2);
            else
                strncpy(parameters->status, "13", 2);
            close(s);
            return;
        }
    }

    versionTable[attempt].Client(s, parameters, 
        sizeof(FUNCPARMS));

    close(s);
}

/*
 ****************************************************************************
 *
 * Function:    TWDOCRESERVE
 *
 * Description: This function calls the reserve server to reserve a
 *              specified amount of a certain SKU # on a remote machine.
 *
 * Parameters:  parameters - The parameters passed by the calling COBOL
 *                           function.
 *
 * Returns:     Nothing
 *
 ****************************************************************************
*/
void TWDOCRESERVE(FUNCPARMS *parameters)
{
    int s;
    bool_t done = FALSE;
    int attempt = 0;
    struct hostent *hp;
    struct servent *sp;
    struct sockaddr_in peeraddr_in;
    char service[25];
    char protocol[25];
    char version[VERSIONLENGTH];
    char c_HostName[HOSTNAMELENGTH+1];
    int result;
    int timeout;
    struct sigaction alrmact;

    /* set up SIGALRM handler */
    alrmact.sa_handler = TCPAlarm;
    sigemptyset(&alrmact.sa_mask);
    alrmact.sa_flags = 0;
    sigaction(SIGALRM, &alrmact, NULL);
    
    GetPrivateProfileString("rsvserver", "version", version, sizeof(version),
        "10.03.00", WESCOMINI);
    GetPrivateProfileString("rsvserver", "service", service, sizeof(service),
        "TWDOCRSV", WESCOMINI);
    GetPrivateProfileString("rsvserver", "protocol", protocol,
        sizeof(protocol), "tcp", WESCOMINI);
    timeout = GetPrivateProfileInt("rsvserver", "timeout", 30, WESCOMINI);

    CobolToC(c_HostName, parameters->hostname, HOSTNAMELENGTH+1,
        HOSTNAMELENGTH);
    
    /* Zero the address structures. */
    memset((char *)&peeraddr_in, 0, sizeof(struct sockaddr_in));

    /* Resolve the host information to a host in the /etc/hosts file. */
    if ((hp = ws_gethostbyname(c_HostName)) == (struct hostent *)NULL) {
        strncpy(parameters->status, "01", 2);
        return;
    }
    
    /* Look up service information in the /etc/services file. */
    if ((sp = getservbyname(service, protocol)) == (struct servent *)NULL) {
        strncpy(parameters->status, "09", 2);
        return;
    }
    
    peeraddr_in.sin_family = AF_INET;
    peeraddr_in.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
    peeraddr_in.sin_port = sp->s_port;

    /* Create the socket. */
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        strncpy(parameters->status, "03", 2);
        return;
    }

    /* Connect to the remote service. */
    alarm(timeout);
    if (connect(s, (struct sockaddr *)&peeraddr_in, sizeof(struct sockaddr_in)) < 0) {
        strncpy(parameters->status, "02", 2);
        close(s);
        return;
    }
    alarm(0);

    while (!done) {
        /* Send the data to the remote computer. */
        strcpy(version, versionTable[attempt].number);
        alarm(timeout);
        if (send(s, version, VERSIONLENGTH, 0) != VERSIONLENGTH) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
        }
        alarm(0);

        /* Receive data from remote computer. */
        if (ReceiveData(s, version, VERSIONLENGTH, 0) == -1) {
            strncpy(parameters->status, "03", 2);
            close(s);
            return;
        }

        if (!strncmp(version, "OK", 2))
            done = TRUE;
        else if (attempt < NUMVERSIONSUPPORTED)
            attempt++;
        else {
            result = strcmp(versionTable[0].number, version);
            if (result > 0)
                strncpy(parameters->status, "14", 2);
            else
                strncpy(parameters->status, "13", 2);
            close(s);
            return;
        }
    }

    versionTable[attempt].Client(s, parameters, 
        sizeof(FUNCPARMS));

    close(s);
}

void Client060500(int s, FUNCPARMS *parameters, int bufferSize)
{
    int timeout;

    timeout = GetPrivateProfileInt("rsvserver", "timeout", 30, WESCOMINI);

    /* Send the data to the remote computer. */
    alarm(timeout);
    if (!SendBuffer(NULL, s, (char*)parameters, bufferSize)) {
        strncpy(parameters->status, "03", 2);
        close(s);
        return;
    }
    alarm(0);

    /* Receive data from remote computer. */
    if (ReceiveData(s, (char*)parameters, bufferSize, 0) < 0) {
        strncpy(parameters->status, "03", 2);
        close(s);
        return;
    }
}

void Client060400(int s, FUNCPARMS *parameters, int bufferSize)
{
    int timeout;
    OLDFUNCPARMS oldparms;

    timeout = GetPrivateProfileInt("rsvserver", "timeout", 30, WESCOMINI);

    /* turn 3 digit partition into 2 digit partition */
    memcpy(&oldparms, parameters, 30);
    memcpy(oldparms.status, parameters->status, sizeof(oldparms) - 30);

    /* Send the data to the remote computer. */
    alarm(timeout);
    if (!SendBuffer(NULL, s, (char*)&oldparms, sizeof(oldparms))) {
        strncpy(parameters->status, "03", 2);
        close(s);
        return;
    }
    alarm(0);

    /* Receive data from remote computer. */
    if (ReceiveData(s, (char*)&oldparms, sizeof(oldparms), 0) < 0) {
        strncpy(parameters->status, "03", 2);
        close(s);
        return;
    }

    /* turn 2 digit partition into 3 digit partition */
    memcpy(parameters, &oldparms, 30);
    memcpy(parameters->status, oldparms.status, sizeof(oldparms) - 30);
}
