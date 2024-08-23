
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "common.h"
#include "newmaint.h"
#include "sharemem.h"

void viewmemory(MAINTTRANSDATA *);

int db, debugFile; /* resolves linker error while linking with libwesco */

main(int argc, char *argv[])
{
    int option;
    short flag = 0;
    int newblock = 0;
    int newwait = 0;
    int newtimeout = 0;
    int newretries = 0;
    char *shutwho;
    char *txdebug;
    char *queuedebug;
    char *delfiles;
    char *mailto;
    extern char *optarg;
    int shmid;
    MAINTTRANSDATA *prgdata;

    if (argc < 2)
        flag |= OPT_VIEWMEM;

    while ((option = getopt(argc, argv, "a:b:d:km:n:q:rs:t:vw:")) != EOF) {
        switch (option) {
            case 'a':
                newretries = atoi(optarg);
                flag |= OPT_MAXRETRIES;
                break;
            case 'b':
                newblock = atoi(optarg);
                flag |= OPT_BLOCKSIZE;
                break;
            case 'd':
                delfiles = optarg;
                flag |= OPT_DELETEFILES;
                break;
            case 'k':
                flag |= OPT_KICKSTART;
                break;
            case 'm':
                mailto = optarg;
                flag |= OPT_MAILTO;
                break;
            case 'n':
                newtimeout = atoi(optarg);
                flag |= OPT_TIMEOUT;
                break;
            case 'q':
                queuedebug = optarg;
                flag |= OPT_QUEUEDBG;
                break;
            case 'r':
                flag |= OPT_RMSHMID;
                break;
            case 's':
                shutwho = optarg;
                flag |= OPT_SHUTDOWN;
                break;
            case 't':
                txdebug = optarg;
                flag |= OPT_TRANSMITDBG;
                break;
            case 'v':
                flag |= OPT_VIEWMEM;
                break;
            case 'w':
                newwait = atoi(optarg);
                flag |= OPT_NEWWAIT;
                break;
            default:
                printf("usage: IQconfig -bdkmnqrstvw\n");
                exit(1);
        }
    }

    prgdata = (MAINTTRANSDATA *)GetSharedMemory(&shmid, SHM_NEWMAINT,
        sizeof(MAINTTRANSDATA), READWRITE);
    if (prgdata == NULL)
        err_sys("shared memory segment does not exist\n");

    if (flag & OPT_RMSHMID) {
        if (flag != OPT_RMSHMID)
            err_quit("-r must be used by itself\n");
        else if (prgdata->pidtx && prgdata->pidqueue)
            err_quit("all IQ processes must be shut down first\n");
        else
            shmctl(shmid, IPC_RMID, (struct shmid_ds *)NULL);
    }
            
    if (flag & OPT_BLOCKSIZE)
        prgdata->msgblocksize = (short)newblock;

    if (flag & OPT_SHUTDOWN) {
        if (!strcmp(shutwho, "IQxmit")) {
            if (prgdata->pidtx) {
                if (kill(prgdata->pidtx, SIGUSR1))
						 if (errno == EPERM) err_quit("You don't have permissions to shut down IQxmit!");
				}
        }
        else if (!strcmp(shutwho, "IQcontrol")) {
            if (prgdata->pidqueue) {
                if (kill(prgdata->pidqueue, SIGUSR1))
						 if (errno == EPERM) err_quit("You don't have permissions to shut down IQcontrol!");
				}
        }
        else if (!strcmp(shutwho, "both")) {
            if (prgdata->pidtx) {
                if (kill(prgdata->pidtx, SIGUSR1))
						 if (errno == EPERM) err_quit("You don't have permissions to shut down IQxmit!");
				}

            if (prgdata->pidqueue) {
                if (kill(prgdata->pidqueue, SIGUSR1))
						 if (errno == EPERM) err_quit("You don't have permissions to shut down IQcontrol!");
				}
        }
        else
            err_quit("%s is invalid for -s; use <IQcontrol/IQxmit/both>\n");
    }

    if (flag & OPT_DELETEFILES) {
        if (!strcmp(delfiles, "on"))
            prgdata->deletefiles = TRUE;
        else if (!strcmp(delfiles, "off"))
            prgdata->deletefiles = FALSE;
        else
            err_quit("%s is invalid for -d; use <on/off>\n");
    }

    if (flag & OPT_QUEUEDBG) {
        if (!strcmp(queuedebug, "on"))
            prgdata->debugqueue = TRUE;
        else if (!strcmp(queuedebug, "off"))
            prgdata->debugqueue = FALSE;
        else
            err_quit("%s is invalid for -q; use <on/off>\n");
    }

    if (flag & OPT_TRANSMITDBG) {
        if (!strcmp(txdebug, "on"))
            prgdata->debugtx = TRUE;
        else if (!strcmp(txdebug, "off"))
            prgdata->debugtx = FALSE;
        else
            err_quit("%s is invalid for -t; use <on/off>\n");
    }

    if (flag & OPT_MAILTO) {
        if (!strcmp(mailto, "nobody"))
            memset(prgdata->mailto, 0, sizeof(prgdata->mailto));
        else
            strcpy(prgdata->mailto, mailto);
    }

    if (flag & OPT_KICKSTART)
        if (prgdata->pidqueue != 0)
            kill(prgdata->pidqueue, SIGUSR2);

    if (flag & OPT_NEWWAIT)
        prgdata->maxwait = (short)newwait;

    if (flag & OPT_TIMEOUT)
        prgdata->timeout = (short)newtimeout;

    if (flag & OPT_MAXRETRIES)
        prgdata->maxretries = (short)newretries;

    if (flag & OPT_VIEWMEM)
        viewmemory(prgdata);

    shmdt(prgdata);

    return 0;
}

void viewmemory(MAINTTRANSDATA *prgdata)
{
    printf("maximum messages per transmission = %d\n", prgdata->msgblocksize);
    printf("maximum time to wait for records = %d\n", prgdata->maxwait);
    printf("network timeout value = %d\n", prgdata->timeout);
    printf("number of transactions = %d\n", prgdata->numtransactions);
    printf("number of rejections = %d\n", prgdata->rejections);
    printf("maximum application error retries = %d\n", prgdata->maxretries);
    printf("debug logging for IQxmit = %s\n", 
        (prgdata->debugtx) ? "TRUE" : "FALSE");
    printf("debug logging for IQcontrol = %s\n",
        (prgdata->debugqueue) ? "TRUE" : "FALSE");
    printf("delete queue files = %s\n", (prgdata->deletefiles) ? "TRUE" :
        "FALSE");
    printf("wesdoc down flag = %s\n", (prgdata->wesdocdown) ? "TRUE" :
        "FALSE");
    printf("send mail to %s\n", prgdata->mailto);
    printf("process id of IQxmit %d\n", prgdata->pidtx);
    printf("process id of IQcontrol %d\n", prgdata->pidqueue);
    printf("IQxmit is ");
    switch (prgdata->txstatus) {
        case MNT_INITIALIZING:
            printf("initializing\n");
            break;
        case MNT_RUNNING:
            printf("running\n");
            break;
        case MNT_SHUTTINGDOWN:
            printf("shutting down\n");
            break;
        case MNT_TERMINATED:
            printf("not running\n");
            break;
    }
    printf("IQcontrol is ");
    switch (prgdata->queuestatus) {
        case MNT_INITIALIZING:
            printf("initializing\n");
            break;
        case MNT_RUNNING:
            printf("running\n");
            break;
        case MNT_SHUTTINGDOWN:
            printf("shutting down\n");
            break;
        case MNT_TERMINATED:
            printf("not running\n");
            break;
    }
}
