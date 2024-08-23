/*
 ******************************************************************************
 * Development History:
 * 16068  mjb  11/17/06 - Removed the "signal" function call from the signal
 *                        handlers.  This was confusing Linux.
 ******************************************************************************
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "common.h"
#include "newmaint.h"
#include "maint.h"
#include "maintc.h"
#include "sharemem.h"

void doloop(void);
int mailmsg(char *, ...);
void soundalarm(int);
void myfuneral(int);
void backonline(int);
void cleanitup(void);

#ifndef WESCOMINI
#define WESCOMINI     "/IMOSDATA/2/wescom.ini"
#endif

#define NETWORKLOG    (-1)

MAINTTRANSDATA *prgdata;       /* global memory pointer for data */
int shmid;
int debuglog;
int debugsocket;
volatile int alarmed;
char version[VERSIONLENGTH];
int fakefd;
int fifofd;
struct sigaction alrmact;
struct sigaction usr1act;
struct sigaction usr2act;
mode_t oldmask;
int db, debugFile; /* fix link error to libwesco */

main(int argc, char *argv[])
{
    char logname[FILENAMELENGTH];  /* debug log filename             */
    char fifoname[FILENAMELENGTH]; /* fifo filename                  */
    int option;
    struct stat fifostat;
    bool_t flag = FALSE;

    daemoninit(argv[0]);
    oldmask = umask(~(S_IRWXU|S_IRWXG|S_IRWXO));

    GetPrivateProfileString("IQcontrol", "debugfile", logname,
        sizeof(logname), "debug.iq", WESCOMINI);

    if ((debuglog = creat(logname, READWRITE)) < 0)
        err_sys("can not create debug log\n");

    /* parse the command line options */
    while ((option = getopt(argc, argv, "f")) != EOF) {
        switch (option) {
            case 'f':
                flag = TRUE;
                wescolog(debuglog, "log created\n");
                break;
            default:
                printf("usage: IQcontrol -f\n\n");
                exit(1);
        }
    }

    if (flag)
        wescolog(debuglog, "initializing application\n");

    /* get shared memory and read in initial values */
    prgdata = (MAINTTRANSDATA *)GetSharedMemory(&shmid, SHM_NEWMAINT,
        sizeof(MAINTTRANSDATA), IPC_CREAT|READWRITE);
    if (!prgdata) {
        if (errno == ENOENT) {
			prgdata = (MAINTTRANSDATA *)GetSharedMemory(&shmid, SHM_NEWMAINT,
				sizeof(MAINTTRANSDATA), READWRITE);
            if (!prgdata)
                err_sys("error getting shared memory\n");
            else {
                memset(prgdata, 0, sizeof(MAINTTRANSDATA));
                prgdata->txstatus = MNT_TERMINATED;
            } 
        }
        else
            err_sys("error getting shared memory\n");
    }

    if (prgdata->pidqueue) {
        if (kill(prgdata->pidqueue, SIGWINCH) > -1)
            err_quit("can not have more than one instance of IQcontrol\n");

	     if (errno == EPERM)
            err_quit("can not have more than one instance of IQcontrol\n");
	 }

    prgdata->maxwait = GetPrivateProfileInt("IQcontrol", "wait", 30, 
        WESCOMINI);
    prgdata->msgblocksize = GetPrivateProfileInt("IQcontrol", "blocksize", 30,
        WESCOMINI);
    prgdata->mailedmsgs = 0;
    prgdata->debugqueue = flag;
    GetPrivateProfileString("IQ", "mail-to", prgdata->mailto,
        sizeof(prgdata->mailto), "root", WESCOMINI);
    prgdata->queuestatus = MNT_INITIALIZING;

    if (prgdata->debugqueue)
        wescolog(debuglog, "shared memory initialization complete\n");

    GetPrivateProfileString("IQ", "fifoname", fifoname, sizeof(fifoname),
        FIFO_FILENAME, WESCOMINI);

    /* check to see if the fifo exists; if not then create it */
    if (stat(fifoname, &fifostat) < 0)
        if (errno == ENOENT) {
            mkfifo(fifoname, READWRITE);
            if (prgdata->debugqueue)
                wescolog(debuglog, "creating fifo %s\n", fifoname);
        }
        else
            err_sys("fifo error\n");

    /* the fifo will be opened for listening to trick WDOCUPDATE into */
    /* sending the information there.                                 */
    if ((fakefd = open(fifoname, O_RDWR)) < 0)
        err_sys("error opening file\n");

#if 0
    daemoninit(argv[0]);
#endif

    prgdata->pidqueue = getpid();

    atexit(cleanitup);

    doloop();

    unlink("mntbuffer");
    if (prgdata->debugqueue) {
        wescolog(debuglog, "shutting down\n");
        close(debuglog);
    }
}

void doloop(void)
{
    short count;
    LINEITEM record;
    int buffd;
    char bufname[FILENAMELENGTH];
    char fifoname[FILENAMELENGTH];
    char holdfile[FILENAMELENGTH];
    char bufname2[FILENAMELENGTH];
    char queuedir[FILENAMELENGTH];
    char qprefix[FILENAMELENGTH];
    short blksize;
    bool_t transmitted;
    bool_t waitforack;
    char ack;
    int s;
    int bytes;
    struct stat fs;

    /* set up SIGALRM handler */
    alrmact.sa_handler = soundalarm;
    sigemptyset(&alrmact.sa_mask);
    sigaddset(&alrmact.sa_mask, SIGUSR1);  /* blocks SIGUSR1 */
    sigaddset(&alrmact.sa_mask, SIGUSR2);  /* blocks SIGUSR2 */
    alrmact.sa_flags = 0;       /* no flags       */
    sigaction(SIGALRM, &alrmact, NULL);

    /* set up SIGUSR1 handler */
    usr1act.sa_handler = myfuneral;
    sigemptyset(&usr1act.sa_mask);
    sigaddset(&usr1act.sa_mask, SIGALRM);  /* blocks SIGALRM */
    sigaddset(&usr1act.sa_mask, SIGUSR2);  /* blocks SIGUSR2 */
    usr1act.sa_flags = 0;       /* no flags       */
    sigaction(SIGUSR1, &usr1act, NULL);

    /* set up SIGUSR2 handler */
    usr2act.sa_handler = backonline;
    sigemptyset(&usr2act.sa_mask);
    sigaddset(&usr2act.sa_mask, SIGALRM);  /* blocks SIGALRM */
    sigaddset(&usr2act.sa_mask, SIGUSR1);  /* blocks SIGUSR2 */
    usr2act.sa_flags = 0;       /* no flags       */
    sigaction(SIGUSR2, &usr2act, NULL);

    /* read .INI file */
    GetPrivateProfileString("IQ", "fifoname", fifoname, sizeof(fifoname),
        FIFO_FILENAME, WESCOMINI);
    GetPrivateProfileString("IQ", "holdfile", holdfile, sizeof(holdfile),
        BACK_FILENAME, WESCOMINI);
    GetPrivateProfileString("IQ", "queuedir", queuedir, sizeof(queuedir),
        "/IMOSDATA/2/QUEUES", WESCOMINI);
    GetPrivateProfileString("IQ", "queueprefix", qprefix, sizeof(qprefix),
        "queue", WESCOMINI);

    count = strlen(queuedir) - 1;
    if (queuedir[count] != '/' && count < FILENAMELENGTH - 1)
        strcat(queuedir, "/");

    prgdata->queuestatus = MNT_RUNNING;

    /* open the fifo */
    if ((fifofd = open(holdfile, O_CREAT|O_RDWR)) < 0) {
        wescolog(debuglog, "can not open fifo %s\n", holdfile);
        exit(1);
    }

    chmod(holdfile, READWRITE);

    if (prgdata->debugqueue)
        wescolog(debuglog, "the application is running\n");

    if (prgdata->debugqueue)
        wescolog(debuglog, "looking for %d byte records\n", sizeof(LINEITEM));

    while (1) {
    /* set up SIGALRM handler */
    alrmact.sa_handler = soundalarm;
    sigemptyset(&alrmact.sa_mask);
    sigaddset(&alrmact.sa_mask, SIGUSR1);  /* blocks SIGUSR1 */
    sigaddset(&alrmact.sa_mask, SIGUSR2);  /* blocks SIGUSR2 */
    alrmact.sa_flags = 0;       /* no flags       */
    sigaction(SIGALRM, &alrmact, NULL);

        if (prgdata->queuestatus == MNT_SHUTTINGDOWN) {
            backup(fifofd);
            return;
        }

        if (prgdata->debugqueue)
             wescolog(debuglog, "waiting for %d records or %d seconds\n",
                 prgdata->msgblocksize, prgdata->maxwait);

        if ((time(NULL) % 3600) < 2)
            prgdata->mailedmsgs = 0;

        strcpy(bufname, queuedir);
        getqueuefilename(bufname+strlen(bufname), qprefix);

        if ((buffd = creat(bufname, READWRITE)) < 0) {
            if (!(prgdata->mailedmsgs & MSG_OPENBUFFER)) {
                mailmsg("Error opening %s: %d - %s\n", bufname, errno,
                    strerror(errno));
                prgdata->mailedmsgs |= MSG_OPENBUFFER;
            }
            sleep(1);
            continue;
        }

        if (prgdata->debugqueue)
            wescolog(debuglog, "%s was opened\n", bufname);

        alarmed = FALSE;
        if (!prgdata->wesdocdown) {
            /* the alarm should go off only if wesdoc is up */
            alarm(prgdata->maxwait);
            if (prgdata->debugqueue)
                wescolog(debuglog, "alarm in %d seconds\n", prgdata->maxwait);
        }

        blksize = prgdata->msgblocksize;
        count = 0;
        do {
            /* read from fifo and place data into a file until all records */
            /* are read from the fifo or the wait timer expires.           */
            bytes = read(fifofd, &record, sizeof(record));
            if (prgdata->debugqueue)
                wescolog(debuglog, "readn returned %d\n", bytes);

            if (prgdata->debugqueue)
                wescolog(debuglog, "alarmed = %d\n", alarmed);

            if (bytes == sizeof(record)) {
                while (write(buffd, &record, sizeof(record)) == -1 &&
                       errno == EINTR);
                count++;
                prgdata->numtransactions++;
            }
            else if (bytes == 0 && prgdata->queuestatus == MNT_RUNNING) {
                /* A return of 0 bytes means the end of file was */
                /* encountered.  Close the current file and open */
                /* the pipe.                                     */
                close(fifofd);

                /* remove the hold file */
                unlink(holdfile);
 
                /* open the fifo */
               if ((fifofd = open(fifoname, O_RDWR)) < 0) {
                   wescolog(debuglog, "can not open fifo %s\n", fifoname);
                   mailmsg("can not open %s\n", fifoname);
               }

            }
            else if (bytes < 0) {
                if (errno != EINTR) {
                    if (!(prgdata->mailedmsgs & MSG_READERROR)) {
                        mailmsg("Error reading fifo: %d - %s\n", errno,
                            strerror(errno));
                        prgdata->mailedmsgs |= MSG_READERROR;
                    }
                }
                else {
                    if (prgdata->queuestatus == MNT_SHUTTINGDOWN) {
                        close(buffd);
                        strcpy(bufname2, queuedir);
                        strcat(bufname2, "towesdoc");
                        strcat(bufname2, strstr(bufname, "queue.")+5);
                        rename(bufname, bufname2);
                        backup(fifofd);
                        close(fifofd);
                        return;
                    }
                }
            }
            else if (bytes == 0) {
                close(fifofd);
                return;
            }
        } while ((count < prgdata->msgblocksize) && (!alarmed));
        
        if (!alarmed)
            alarm(0);    /* cancel alarm */

        fstat(buffd, &fs);
        if (fs.st_size < sizeof(LINEITEM)) {
            close(buffd);
            unlink(bufname);
        }
        else {
            close(buffd);
            strcpy(bufname2, queuedir);
            strcat(bufname2, "towesdoc");
            strcat(bufname2, strstr(bufname, "queue.")+5);
            if (prgdata->debugqueue)
                wescolog(debuglog, "rename to %s\n", bufname2);

            rename(bufname, bufname2);
        }
    } 
}

int mailmsg(char *fmt, ...)
{
    int fd;
    va_list ap;
    char buffer[256];

    if (prgdata->mailto[0] == 0)
        return -1;

    if ((fd = open(MAIL_FILENAME, O_CREAT|O_WRONLY, READWRITE)) < 0)
        return -1;

    strcpy(buffer, "IQcontrol has produced the following error:\n\n\n");
    write(fd, buffer, strlen(buffer));

    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    strcat(buffer, "\n");
    write(fd, buffer, strlen(buffer));
    va_end(ap);

    close(fd);

    sprintf(buffer, "mail %s < %s", prgdata->mailto, MAIL_FILENAME);
    system(buffer);

    if (prgdata->debugqueue)
        wescolog(debuglog, "mail sent to %s\n", prgdata->mailto);

    return 0;
}

void soundalarm(int status)
{
    alarmed = TRUE;
}

void backonline(int status)
{
    alarmed = TRUE;
}

void myfuneral(int status)
{
    prgdata->queuestatus = MNT_SHUTTINGDOWN;
}

int backup(int fd)
{
    bool_t done = FALSE;
    struct stat filestatus;
    LINEITEM buffer;
    int fd2;
	 int flags;
    char holdfile[FILENAMELENGTH];

    /* close fake-out file descriptor */
    close(fakefd);

    /* turn off any alarms */
    alarm(0);

    if (fstat(fd, &filestatus) < 0)
        return -1;    /* bad file descriptor */

    /* set file to non-blocking mode */
	 flags = fcntl(fd, F_GETFL);
	 flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);

    /* don't process if not a FIFO file */
    if (!(S_ISFIFO(filestatus.st_mode)))
        return 1;

    GetPrivateProfileString("IQ", "holdfile", holdfile, sizeof(holdfile),
        BACK_FILENAME, WESCOMINI);

    /* open the backup file */
    if ((fd2 = open(holdfile, O_CREAT|O_RDWR, READWRITE)) < 0) {
        wescolog(NETWORKLOG, "can not open file %s\n", holdfile);
        return -1;
    }

    while (!done) {
        if (read(fd, &buffer, sizeof(buffer)) < 1) {
            close(fd);
            done = TRUE;
        }
        else
            write(fd2, &buffer, sizeof(buffer));
    }

    return 0;
}

void cleanitup(void)
{
    prgdata->queuestatus = MNT_TERMINATED;
    prgdata->pidqueue = (pid_t)0;
    shmdt(prgdata);
    umask(oldmask);
}
