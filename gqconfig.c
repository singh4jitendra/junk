#if defined(_HP_TARGET)
#   define _INCLUDE_POSIX_SOURCE
#   define _INCLUDE_XOPEN_SOURCE
#   define _INCLUDE_HPUX_SOURCE
#endif

#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "common.h"
#include "gqconfig.h"
#include "sharemem.h"

void viewmemory(GQDATA *);

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
    GQDATA *prgdata;

    if (argc < 2)
        flag |= OPT_VIEWMEM;

    while ((option = getopt(argc, argv, "a:d:f:m:n:rst:vw:")) != EOF) {
        switch (option) {
            case 'a':
                newretries = atoi(optarg);
                flag |= OPT_MAXRETRIES;
                break;
            case 'd':
                delfiles = optarg;
                flag |= OPT_DELETEFILES;
                break;
            case 'm':
                mailto = optarg;
                flag |= OPT_MAILTO;
                break;
            case 'n':
                newtimeout = atoi(optarg);
                flag |= OPT_TIMEOUT;
                break;
            case 'r':
                flag |= OPT_RMSHMID;
                break;
            case 's':
                flag |= OPT_SHUTDOWN;
                break;
            case 'f':
                txdebug = optarg;
                flag |= OPT_DEBUG;
                break;
            case 'v':
                flag |= OPT_VIEWMEM;
                break;
            case 'w':
                newwait = atoi(optarg);
                flag |= OPT_NEWWAIT;
                break;
            default:
                printf("usage: IQconfig -dfmnrsvw\n");
                exit(1);
        }
    }

    prgdata = (GQDATA *)GetSharedMemory(&shmid, SHM_GQCONFIG, sizeof(GQDATA),
        READWRITE);
    if (prgdata == NULL)
        err_sys("shared memory segment does not exist\n");

    if (flag & OPT_RMSHMID) {
        if (flag != OPT_RMSHMID)
            err_quit("-r must be used by itself\n");
        else if (prgdata->pid)
            err_quit("GQxmit must be shut down first\n");
        else
            shmctl(shmid, IPC_RMID, 0);
    }
            
    if (flag & OPT_SHUTDOWN)
        if (prgdata->pid)
            kill(prgdata->pid, SIGUSR1);

    if (flag & OPT_DELETEFILES) {
        if (!strcmp(delfiles, "on"))
            prgdata->deletefiles = TRUE;
        else if (!strcmp(delfiles, "off"))
            prgdata->deletefiles = FALSE;
        else
            err_quit("%s is invalid for -d; use <on/off>\n");
    }

    if (flag & OPT_DEBUG) {
        if (!strcmp(txdebug, "on"))
            prgdata->debug = TRUE;
        else if (!strcmp(txdebug, "off"))
            prgdata->debug = FALSE;
        else
            err_quit("%s is invalid for -f; use <on/off>\n", txdebug);
    }

    if (flag & OPT_MAILTO) {
        if (!strcmp(mailto, "nobody"))
            memset(prgdata->mailto, 0, sizeof(prgdata->mailto));
        else
            strcpy(prgdata->mailto, mailto);
    }

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

void viewmemory(GQDATA *prgdata)
{
    printf("maximum time to wait for records = %d\n", prgdata->maxwait);
    printf("network timeout value = %d\n", prgdata->timeout);
    printf("maximum application error retries = %d\n", prgdata->maxretries);
    printf("debug logging for GQxmit = %s\n", 
        (prgdata->debug) ? "True" : "False");
    printf("delete queue files = %s\n", (prgdata->deletefiles) ? "True" :
        "False");
    printf("send mail to %s\n", prgdata->mailto);
    printf("process id of GQxmit %d\n", prgdata->pid);
    printf("GQxmit is ");
    switch (prgdata->status) {
        case GQ_INITIALIZING:
            printf("initializing\n");
            break;
        case GQ_RUNNING:
            printf("running\n");
            break;
        case GQ_SHUTTINGDOWN:
            printf("shutting down\n");
            break;
        case GQ_TERMINATED:
            printf("not running\n");
            break;
    }
}
