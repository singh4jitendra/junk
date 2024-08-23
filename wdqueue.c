#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#if !defined(_HPUX_SOURCE) && !defined(_GNU_SOURCE) && !defined(_GNU_SOURCE)

#ifndef _GNU_SOURCE
#   include <sys/locking.h>
#endif

#   include <sys/flock.h>
#endif    /* _HPUX_SOURCE */

#include <sys/ipc.h>
#include <sys/sem.h>
#include <dirent.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#include "common.h"
#include "receive.h"
#include "queue.h"

bool_t CopyFile(char *, char *);

#define COPYSUCCESS     0
#define OPENFAIL        1

void WDOCQUEUE(FUNCPARMS *);
void TWDOCQUEUE(FUNCPARMS *);

/*
 ****************************************************************************
 *
 * Function:    WDOCQUEUE
 *
 * Description: This fuction queues the specified file.
 *
 * Parameters:  parameters - This is a structure that defines the data that
 *                           was passed by the COBOL function.
 *
 * Returns:     Nothing
 *
 ****************************************************************************
*/
void WDOCQUEUE(FUNCPARMS *parameters)
{
    char *tempQ;
    char c_HostName[HOSTNAMELENGTH+1], c_FileName[FILENAMELENGTH+1]; 
    char destFilename[FILENAMELENGTH], seqFilename[FILENAMELENGTH];
    char record[50], seqNumber[7], c_Transaction[TRANSACTIONSIZE+1];
    char QPath[FILENAMELENGTH]; 
    int pos, destFile;
    u_int32_t nextNumber;
    QUEUEDETAILS *details;
    int queuefd;
    struct passwd *passwordEntry;
    struct group *groupEntry;
    struct flock destLock;

    /* Clear the character buffers */
    memset(c_HostName, 0, sizeof(c_HostName));
    memset(c_FileName, 0, sizeof(c_FileName));
    memset(destFilename, 0, sizeof(destFilename));
    memset(seqFilename, 0, sizeof(seqFilename));
    memset(c_Transaction, 0, sizeof(c_Transaction));
    memset(record, 0, sizeof(record));

    details = (QUEUEDETAILS *)parameters->details;

    CobolToC(c_HostName, parameters->hostname, sizeof(c_HostName),
        sizeof(parameters->hostname));
    CobolToC(c_FileName, details->filename, sizeof(c_FileName),
        sizeof(details->filename));
    CobolToC(c_Transaction, parameters->transaction, sizeof(c_Transaction),
        sizeof(parameters->transaction));

    /*
        Get the environmental variable value.
    */
    tempQ = getenv("WDOCQDIR");
    if (tempQ == NULL) {
        strncpy(parameters->status, "10", 2);
        return;
    }
    strcpy(QPath, tempQ);

    /* 
        Create the filename for the file that will be transmitted. 
    */
    strcpy(destFilename, QPath);
    pos = strlen(destFilename);
    if (destFilename[pos] != '/')
        destFilename[pos] = '/';
    strcpy(seqFilename, destFilename);
    strcat(destFilename, c_HostName);
    strcat(destFilename, "/");
    strcat(seqFilename, "seqnum");

    /*
        Check to see if the destination directory exists.  If not then
        create it.
    */
    if (CheckDirectory(destFilename) == DIRECTORY_CREATED) {
        /* get wescom group entry */
        groupEntry = getgrnam("wescom");

        /* get super password entry */
        passwordEntry = getpwnam("super");

        chown(destFilename, passwordEntry->pw_uid, groupEntry->gr_gid);
    }

    /*
        Open the sequence file to get the next number so a filename
        can be created.
    */
    if ((destFile = open(seqFilename, O_RDWR)) < 0) {
        strncpy(parameters->status, "07", 2);
        return;
    }

    /* 
       Lock the file to ensure that another process can not use the same
       number that this process is using.
    */
    destLock.l_type = F_WRLCK;
    destLock.l_whence = 0;
    destLock.l_start = (off_t)0;
    destLock.l_len = (off_t)0;
    if (fcntl(destFile, F_SETLKW, &destLock) < 0) {
       strncpy(parameters->status, "06", 2);
       return;
    }

    /*
        Read the file into memory.  Create the destination filename for the
        queueing mechanism.  Increase the sequence number and write the new
        one out to disk.
    */
    read(destFile, seqNumber, 6);
    seqNumber[6] = 0;
    strcat(destFilename, seqNumber);
    sscanf(seqNumber, "%6lu", &nextNumber);
    if ((++nextNumber) > 999999)
        nextNumber = 0;
    sprintf(seqNumber, "%000006lu", nextNumber);
    lseek(destFile, 0, 0);
    write(destFile, seqNumber, 6);
    destLock.l_type = F_UNLCK;
    fcntl(destFile, F_SETLK, &destLock);
    
    /*
        If a filename is specified copy the data file to the queueing
        directory.
    */
    if (details->filename[0] != ' ') {
        strcat(destFilename, ".dat");
        if (!CopyFile(c_FileName, destFilename)) {
            strncpy(parameters->status, "08", 2);
            return;
        }
        pos = strlen(destFilename) - 3;
    }
    else
        pos = strlen(destFilename);
    
    close(destFile);
    
    destFilename[pos] = 'X';
    destFilename[pos+1] = 0;
    
    strcpy(record, c_Transaction);
    strcat(record, " ");
    if (details->filename[0] != ' ')
        strcat(record, "F");
    else
        strcat(record, "N");

    if ((queuefd = creat(destFilename, READWRITE)) < 0) {
        strncpy(parameters->status, "08", 2);
        return;
    }

    write(queuefd, record, strlen(record));
    close(queuefd);

    strcpy(c_FileName, destFilename);
    pos = strlen(c_FileName) - 1;
    c_FileName[pos] = 'Q';
    rename(destFilename, c_FileName);
}

/*
 ****************************************************************************
 *
 * Function:    TWDOCQUEUE
 *
 * Description: This fuction queues the specified file.
 *
 * Parameters:  parameters - This is a structure that defines the data that
 *                           was passed by the COBOL function.
 *
 * Returns:     Nothing
 *
 ****************************************************************************
*/
void TWDOCQUEUE(FUNCPARMS *parameters)
{
    char *tempQ;
    char c_HostName[HOSTNAMELENGTH+1], c_FileName[FILENAMELENGTH+1]; 
    char destFilename[FILENAMELENGTH], seqFilename[FILENAMELENGTH];
    char record[256], seqNumber[7], c_Transaction[TRANSACTIONSIZE+1];
    char QPath[FILENAMELENGTH]; 
    int pos, destFile;
    u_int32_t nextNumber;
    QUEUEDETAILS *details;
    int queuefd;
    struct flock destLock;

    /* Clear the character buffers */
    memset(c_HostName, 0, HOSTNAMELENGTH+1);
    memset(c_FileName, 0, FILENAMELENGTH+1);
    memset(destFilename, 0, FILENAMELENGTH);
    memset(seqFilename, 0, FILENAMELENGTH+1);
    memset(c_Transaction, 0, TRANSACTIONSIZE+1);
    memset(record, 0, 256);

    details = (QUEUEDETAILS *)parameters->details;

    CobolToC(c_HostName, parameters->hostname, HOSTNAMELENGTH+1,
        HOSTNAMELENGTH);
    CobolToC(c_FileName, details->filename, FILENAMELENGTH+1,
        FILENAMELENGTH);
    CobolToC(c_Transaction, parameters->transaction, TRANSACTIONSIZE+1,
        TRANSACTIONSIZE);

    /*
        Get the environmental variable value.
    */
    tempQ = getenv("WDOCQDIR");
    if (tempQ == NULL) {
        strncpy(parameters->status, "10", 2);
        return;
    }
    strcpy(QPath, tempQ);

    /* 
        Create the filename for the file that will be transmitted. 
    */
    strcpy(destFilename, QPath);
    pos = strlen(destFilename);
    if (destFilename[pos] != '/')
        destFilename[pos] = '/';
    strcpy(seqFilename, destFilename);
    strcat(destFilename, c_HostName);
    strcat(destFilename, "/");
    strcat(seqFilename, "seqnum");

    /*
        Check to see if the destination directory exists.  If not then
        create it.
    */
    CheckDirectory(destFilename);

    /*
        Open the sequence file to get the next number so a filename
        can be created.
    */
    if ((destFile = open(seqFilename, O_RDWR)) == -1) {
        strncpy(parameters->status, "07", 2);
        return;
    }

    /* 
       Lock the file to ensure that another process can not use the same
       number that this process is using.
    */
    destLock.l_type = F_WRLCK;
    destLock.l_whence = 0;
    destLock.l_start = (off_t)0;
    destLock.l_len = (off_t)0;
    if (fcntl(destFile, F_SETLKW, &destLock) < 0) {
        strncpy(parameters->status, "06", 2);
        return;
    }

    /*
        Read the file into memory.  Create the destination filename for the
        queueing mechanism.  Increase the sequence number and write the new
        one out to disk.
    */
    read(destFile, seqNumber, 6);
    seqNumber[6] = 0;
    strcat(destFilename, seqNumber);
    sscanf(seqNumber, "%6uld", &nextNumber);
    if ((++nextNumber) > 999999)
        nextNumber = 0;
    sprintf(seqNumber, "%000006uld", nextNumber);
    lseek(destFile, 0, 0);
    write(destFile, seqNumber, 6);
    destLock.l_type = F_UNLCK;
    fcntl(destFile, F_SETLK, &destLock);
    
    /*
        If a filename is specified copy the data file to the queueing
        directory.
    */
    if (details->filename[0] != ' ') {
        strcat(destFilename, ".dat");
        if (!CopyFile(c_FileName, destFilename)) {
            strncpy(parameters->status, "08", 2);
            return;
        }
        pos = strlen(destFilename) - 3;
    }
    else
        pos = strlen(destFilename);
    
    close(destFile);
    
    destFilename[pos] = 'X';
    destFilename[pos+1] = 0;
    
    strcpy(record, c_Transaction);
    strcat(record, " ");
    if (details->filename[0] != ' ')
        strcat(record, "F");
    else
        strcat(record, "N");
         
    if ((queuefd = creat(destFilename, READWRITE)) < 0) {
        strncpy(parameters->status, "08", 2);
        return;
    }

    write(queuefd, record, strlen(record));
    close(queuefd);

    strcpy(c_FileName, destFilename);
    pos = strlen(c_FileName) - 1;
    c_FileName[pos] = 'Q';
    rename(destFilename, c_FileName);
}

/*
 ****************************************************************************
 *
 * Function:    CopyFile
 *
 * Description: This fuction copies a file.
 *
 * Parameters:  source - This is the name of the source file
 *              dest   - This is the name of the destination file
 *
 * Returns:     FALSE if could not open file, else TRUE
 *
 ****************************************************************************
*/
bool_t CopyFile(char *source, char *dest)
{
    int srcFile, dstFile, bytesRead;
    bool_t done = FALSE;
    char buffer[1024];

    if ((srcFile = open(source, O_RDONLY)) < 0) { 
        wescolog(NETWORKLOG, "WDQUEUE: error opening %s", source);
        return FALSE;
    }

    if ((dstFile = creat(dest, READWRITE)) < 0) {
        wescolog(NETWORKLOG, "WDQUEUE: error opening %s", dest);
        return FALSE;
    }

    while (!done) {
        bytesRead = read(srcFile, buffer, 1024);
        write(dstFile, buffer, bytesRead);
        if (bytesRead < 1024)
            done = TRUE;
    }

    close(srcFile);
    close(dstFile);

    return TRUE;
}
