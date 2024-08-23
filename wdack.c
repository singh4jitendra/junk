#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>

#define WESCOM_CLIENT    1

#include "common.h"
#include "receive.h"
#include "update.h"
#include "wdack.h"

/*
 ****************************************************************************
 *
 * Function:    WDOCGETACKN
 *
 * Description: This fuction gets the PO acknowledgments for a given SKU #.
 *
 * Parameters:  parameters - This is a structure that defines the data that
 *                           was passed by the COBOL function.
 *
 * Returns:     Nothing
 *
 ****************************************************************************
*/
void WDOCGETACKN(FUNCPARMS *parameters)
{
    int s;
    int totalBytes = 0;
    int result = 0;
    struct hostent *hp;
    struct servent *sp;
    struct sockaddr_in peeraddr;
    char service[25];
    char protocol[25];
    char version[VERSIONLENGTH];
    char c_HostName[HOSTNAMELENGTH+1] = {0}; 
    int timeout;
    bool_t done = FALSE;
    int attempt = 0;
    struct sigaction alrmact;

    /* set up SIGALRM handler */
    alrmact.sa_handler = TCPAlarm;
    sigemptyset(&alrmact.sa_mask);
    alrmact.sa_flags = 0;
    sigaction(SIGALRM, &alrmact, NULL);

   GetPrivateProfileString("ackserver", "version", version, sizeof(version),
      "06.05.03", WESCOMINI);
   GetPrivateProfileString("ackserver", "service", service, sizeof(service),
      "WDOCACK", WESCOMINI);
   GetPrivateProfileString("ackserver", "protocol", protocol,
      sizeof(protocol), "tcp", WESCOMINI);
   timeout = GetPrivateProfileInt("ackserver", "timeout", 30, WESCOMINI);

    CobolToC(c_HostName, parameters->hostname, HOSTNAMELENGTH+1,
        HOSTNAMELENGTH);

    /* Zero address structures. */
    memset((char *)&peeraddr, 0, sizeof(struct sockaddr_in));

    /* Resolve host name to entry in /etc/hosts file */
    if ((hp = ws_gethostbyname(c_HostName)) == NULL) {
        strncpy(parameters->status, "01", 2);
        return;
    } 

    /* Resolve service to entry in /etc/services file */
    if ((sp = getservbyname(service, "tcp")) == NULL) {
        strncpy(parameters->status, "09", 2);
        return;
    }

    peeraddr.sin_family = AF_INET;
    peeraddr.sin_port = sp->s_port;
    peeraddr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

    /* Create socket */
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        strncpy(parameters->status, "03", 2);
        strncpy(parameters->retname, "socket", 6);
        strncpy(parameters->retdesc, "can not create", 14);
        return;
    }

    /* Connect to remote computer */
    alarm(timeout);
    if (connect(s, (struct sockaddr *)&peeraddr, sizeof(struct sockaddr_in)) < 0) {
        strncpy(parameters->status, "02", 2);
        return;
    }
    alarm(0);

    while (!done) {
        /* Send the version */
        strcpy(version, ackVersionTable[attempt].number);
        alarm(timeout);
        if (send(s, version, VERSIONLENGTH, 0) != VERSIONLENGTH) {
            strncpy(parameters->status, "03", 2);
            strncpy(parameters->retname, "send", 4);
            strncpy(parameters->retdesc, "version", 7);
            return;
        }
        alarm(0);

        /* Get the return code */
        totalBytes = ReceiveData(s, version, VERSIONLENGTH, 0);
        if (totalBytes == -1) {
            strncpy(parameters->status, "03", 2);
            strncpy(parameters->retname, "receive", 7);
            strncpy(parameters->retdesc, "version", 7);
            return;
        }

        if (!strncmp(version, "OK", 2))
            done = TRUE;
        else if (attempt < NUMVERSIONSUPPORTED)
            attempt++;
        else {
            result = strcmp(ackVersionTable[0].number, version);
            if (result > 0)
                strncpy(parameters->status, "14", 2);
            else
                strncpy(parameters->status, "13", 2);
            close(s);
            return;
        }
    }

    ackVersionTable[attempt].Client(s, parameters,
        sizeof(FUNCPARMS));

    close(s);
}

/*
 ****************************************************************************
 *
 * Function:    TWDOCGETACKN
 *
 * Description: This fuction gets the PO acknowledgments for a given SKU #.
 *
 * Parameters:  parameters - This is a structure that defines the data that
 *                           was passed by the COBOL function.
 *
 * Returns:     Nothing
 *
 ****************************************************************************
*/
void TWDOCGETACKN(FUNCPARMS *parameters)
{
    int s;
    int totalBytes = 0;
    int result = 0;
    struct hostent *hp;
    struct servent *sp;
    struct sockaddr_in peeraddr;
    char service[25];
    char protocol[25];
    char version[VERSIONLENGTH];
    char c_HostName[HOSTNAMELENGTH+1] = {0}; 
    int timeout;
    bool_t done = FALSE;
    int attempt = 0;
    struct sigaction alrmact;

    /* set up SIGALRM handler */
    alrmact.sa_handler = TCPAlarm;
    sigemptyset(&alrmact.sa_mask);
    alrmact.sa_flags = 0;
    sigaction(SIGALRM, &alrmact, NULL);

   GetPrivateProfileString("ackserver", "version", version, sizeof(version),
      "060500P3", WESCOMINI);
   GetPrivateProfileString("ackserver", "service", service, sizeof(service),
      "TWDOCACK", WESCOMINI);
   GetPrivateProfileString("ackserver", "protocol", protocol,
      sizeof(protocol), "tcp", WESCOMINI);
   timeout = GetPrivateProfileInt("ackserver", "timeout", 30, WESCOMINI);

    CobolToC(c_HostName, parameters->hostname, HOSTNAMELENGTH+1,
        HOSTNAMELENGTH);

    /* Zero address structures. */
    memset((char *)&peeraddr, 0, sizeof(struct sockaddr_in));

    /* Resolve host name to entry in /etc/hosts file */
    if ((hp = ws_gethostbyname(c_HostName)) == NULL) {
        strncpy(parameters->status, "01", 2);
        return;
    } 

    /* Resolve service to entry in /etc/services file */
    if ((sp = getservbyname(service, "tcp")) == NULL) {
        strncpy(parameters->status, "09", 2);
        return;
    }

    peeraddr.sin_family = AF_INET;
    peeraddr.sin_port = sp->s_port;
    peeraddr.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;

    /* Create socket */
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        strncpy(parameters->status, "03", 2);
        strncpy(parameters->retname, "socket", 6);
        strncpy(parameters->retdesc, "can not create", 14);
        return;
    }

    /* Connect to remote computer */
    alarm(timeout);
    if (connect(s, (struct sockaddr *)&peeraddr, sizeof(struct sockaddr_in)) < 0) {
        strncpy(parameters->status, "02", 2);
        return;
    }
    alarm(0);

    while (!done) {
        /* Send the version */
        strcpy(version, ackVersionTable[attempt].number);
        alarm(timeout);
        if (send(s, version, VERSIONLENGTH, 0) != VERSIONLENGTH) {
            strncpy(parameters->status, "03", 2);
            strncpy(parameters->retname, "send", 4);
            strncpy(parameters->retdesc, "version", 7);
            return;
        }
        alarm(0);

        /* Get the return code */
        totalBytes = ReceiveData(s, version, VERSIONLENGTH, 0);
        if (totalBytes == -1) {
            strncpy(parameters->status, "03", 2);
            strncpy(parameters->retname, "receive", 7);
            strncpy(parameters->retdesc, "version", 7);
            return;
        }

        if (!strncmp(version, "OK", 2))
            done = TRUE;
        else if (attempt < NUMVERSIONSUPPORTED)
            attempt++;
        else {
            result = strcmp(ackVersionTable[0].number, version);
            if (result > 0)
                strncpy(parameters->status, "14", 2);
            else
                strncpy(parameters->status, "13", 2);
            close(s);
            return;
        }
    }

    ackVersionTable[attempt].Client(s, parameters,
        sizeof(FUNCPARMS));

    close(s);
}

void AckClient060503(int s, FUNCPARMS *parameters, int bufferSize)
{
    int destFile;
    char c_FileName[FILENAMELENGTH+1]; 
    char record[256];
    int totalBytes;
    bool_t done = FALSE;
    CCACKR11DETAIL *details;
    mode_t oldmask;

    memset(c_FileName, 0, sizeof(c_FileName));
    memset(record, 0, sizeof(record));

    details = (CCACKR11DETAIL *)parameters->details;
    
    CobolToC(c_FileName, details->filename, FILENAMELENGTH+1,
        FILENAMELENGTH);

    /* Send the data */
    alarm(30);
    if (!SendBuffer(NULL, s, (char*)parameters, bufferSize)) {
        strncpy(parameters->status, "03", 2);
        return;
    }
    alarm(0);

    /* Get the return code */
    totalBytes = ReceiveData(s, (char*)parameters, sizeof(FUNCPARMS), 0);
    if (totalBytes == -1) {
        strncpy(parameters->status, "03", 2);
        return;
    }

    /* If return code is 00 open file and log data */
    if (!strncmp(parameters->status, "00", 2)) {
        oldmask = umask(0);
        if ((destFile = creat(c_FileName, 0777)) == NULL) {
            strncpy(parameters->status, "04", 2);
            umask(oldmask);
            return;
        }
    
        /* Receive data from remote computer */
        while (!done) {
            totalBytes = ReceiveData(s, record, PORECORDLENGTH, 0);
            if (totalBytes == -1) {
                strncpy(parameters->status, "03", 2);
                close(destFile);
                umask(oldmask);
                return;
            }
            else if (totalBytes == 0)
                done = 1;
            else {
                write(destFile, record, PORECORDLENGTH);
                write(destFile, "\n", 1);
            }
        }
        
        close(destFile);
        umask(oldmask);
    }
}

void AckClient060400(int s, FUNCPARMS *parameters, int bufferSize)
{
    int destFile;
    char c_FileName[FILENAMELENGTH+1]; 
    char record[256];
    int totalBytes;
    bool_t done = FALSE;
    CCACKR11DETAIL *details;
    mode_t oldmask;
    OLDFUNCPARMS oldparms;

    memset(c_FileName, 0, sizeof(c_FileName));
    memset(record, 0, sizeof(record));

    memcpy(&oldparms, parameters, sizeof(oldparms));

    /* turn 3 digit partition into 2 digit partition */
    memcpy(&oldparms.status, parameters->status, sizeof(oldparms) - 30);

    details = (CCACKR11DETAIL *)parameters->details;

    CobolToC(c_FileName, details->filename, FILENAMELENGTH+1,
        FILENAMELENGTH);

    /* Send the data */
    alarm(30);
    if (!SendBuffer(NULL, s, (char*)&oldparms, sizeof(oldparms))) {
        strncpy(parameters->status, "03", 2);
        strncpy(parameters->retname, "send", 4);
        strncpy(parameters->retdesc, "buffer", 6);
        return;
    }
    alarm(0);

    /* Get the return code */
    totalBytes = ReceiveData(s, (char*)&oldparms, sizeof(oldparms), 0);
    if (totalBytes == -1) {
        strncpy(parameters->status, "03", 2);
        strncpy(parameters->retname, "receive", 7);
        strncpy(parameters->retdesc, "buffer", 6);
        return;
    }

    /* turn 2 digit partition into 3 digit partition */
    memcpy(parameters->status, oldparms.status, sizeof(oldparms) - 30);

    /* If return code is 00 open file and log data */
    if (!strncmp(parameters->status, "00", 2)) {
        oldmask = umask(0);
        if ((destFile = creat(c_FileName, 0777)) == NULL) {
            strncpy(parameters->status, "04", 2);
            umask(oldmask);
            return;
        }
    
        /* Receive data from remote computer */
        while (!done) {
            totalBytes = ReceiveData(s, record, PORECORDLENGTH, 0);
            if (totalBytes == -1) {
                strncpy(parameters->status, "03", 2);
                strncpy(parameters->retname, "receive", 7);
                strncpy(parameters->retdesc, "file", 4);
                close(destFile);
                return;
            }
            else if (totalBytes == 0)
                done = TRUE;
            else {
                write(destFile, record, PORECORDLENGTH);
                write(destFile, "\n", 1);
            }
        }
        
        close(destFile);
        umask(oldmask);
    }
}

