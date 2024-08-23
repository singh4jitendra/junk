#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "maint.h"
#include "newmaint.h"
#include "sharemem.h"
#include "maintc.h"

void WDOCUPDINV(FUNCPARMS *parameters)
{
    int fifofd;                                 /* fifo file descriptor      */
    struct stat fifostatus;                     /* file info for fifo        */
    char fifoname[FILENAMELENGTH];              /* fifo filename             */
    char holdfile[FILENAMELENGTH];              /* hold filename             */
    char buffer[256];
    int testfd;
    LINEITEM record;
    mode_t oldmask;

    oldmask = umask(0);

    GetPrivateProfileString("IQ", "fifoname", fifoname, sizeof(fifoname),
        FIFO_FILENAME, WESCOMINI);

    GetPrivateProfileString("IQ", "holdfile", holdfile, sizeof(holdfile),
        BACK_FILENAME, WESCOMINI);

    /* check to see if fifo exists; if not create it */
    if (stat(fifoname, &fifostatus) < 0)
        if (errno == ENOENT)
            mkfifo(fifoname, READWRITE);
        else {
            sprintf(buffer, "error opening fifo - %d: %s", errno,
                strerror(errno));
            mailme(buffer);
            return;
        }

    if ((testfd = open(fifoname, O_WRONLY|O_NONBLOCK)) < 0) {
       if (errno == ENXIO) {
           if ((fifofd = open(holdfile, O_CREAT|O_WRONLY|O_APPEND, 0666)) < 0) {
               sprintf(buffer, "file error - %d: %s %s", errno,
                   strerror(errno), holdfile);
               mailme(buffer);
               umask(oldmask);
               return;
           }
           chmod(holdfile, READWRITE);
       }
       else {
            sprintf(buffer, "fifo error - %d: %s %s", errno,
                strerror(errno), fifoname);
            mailme(buffer);
            umask(oldmask);
            return;
       }
    }
    else
        fifofd = open(fifoname, O_WRONLY);

    strncpy(record.transaction, parameters->transaction, 
        sizeof(parameters->transaction));
    memcpy(record.branch, parameters->details, sizeof(DETAILS));

    if (writen(fifofd, &record, sizeof(record)) < 0) {
        sprintf(buffer, "write error - %d: %s", errno, strerror(errno));
        mailme(buffer);
        umask(oldmask);
        return;
    }

    umask(oldmask);
    close(testfd);
    close(fifofd);
}

int mailme(char *parm)
{
    int fd = creat("/tmp/wdocupdinv.mail", 0666);

    if (fd < 0)
        return -1;

    write(fd, parm, strlen(parm));
    close(fd);
    system("rmail root < /tmp/wdocupdinv.mail");

    return 0;
}
