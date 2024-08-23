/*
   Changed: 12/22/04 mjb - Added a preconditional compile statement to pass
                           three arguments to shmctl when _GNU_SOURCE is
                           defined.
*/

#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "common.h"
#include "maintq.h"

void viewmemory(void);

int newtimeout = 0;
int maxconnections;
int curconnections;
char *debug;
char *logtrans;
char *mailto;
extern char *optarg;
int shmid;
MSQDATA *prgdata;

main(int argc, char *argv[])
{
    int option;
    short flag = 0;
    int semid;
    struct sembuf locksem[] = {
        {0, 0, SEM_UNDO},
        {0, 1, SEM_UNDO},
};

    if (argc < 2)
        flag |= MQO_VIEWMEM;

    while ((option = getopt(argc, argv, "M:c:d:f:l:m:n:rst:v")) != EOF) {
        switch (option) {
            case 'M':
                maxconnections = atol(optarg);
                flag |= MQO_MAXCONN;
                break;
            case 'c':
                curconnections = atol(optarg);
                flag |= MQO_CURCONN;
                break;
            case 'd':
            case 'f':
                debug = optarg;
                flag |= MQO_DBGFILE;
                break;
            case 'l':
            case 't':
                logtrans = optarg;
                flag |= MQO_LOGTRANS;
                break;
            case 'm':
                mailto = optarg;
                flag |= MQO_MAILTO;
                break;
            case 'n':
                newtimeout = atoi(optarg);
                flag |= MQO_TIMEOUT;
                break;
            case 'r':
                flag |= MQO_RMSHMID;
                break;
            case 's':
                flag |= MQO_SHUTDOWN;
                break;
            case 'v':
                flag |= MQO_VIEWMEM;
                break;
            default:
                printf("usage: IQconfig -dlmnrsv\n");
                exit(1);
        }
    }

    prgdata = (MSQDATA *)GetSharedMemory(&shmid, MQ_SHM_KEY,
        sizeof(MSQDATA), READWRITE);
    if (prgdata == NULL)
        err_sys("shared memory segment does not exist\n");

    if ((semid = semget(MQ_SEMAPHORE_KEY,1,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
        perror(argv[0]);
        exit(1);
    }

    if (semop(semid, locksem,2) <0) {
        perror (argv[0]);  
        exit(1);
    }

    if (flag & MQO_MAXCONN) {
        if (maxconnections < 10) {
            fprintf(stderr, "maxconnections must be at least 10\n");
            exit(1);
        }

        prgdata->maxconnections = (short)maxconnections;
    }

    if (flag & MQO_CURCONN) {
        prgdata->numconnections = (short)curconnections;
    }

    if (flag & MQO_RMSHMID) {
        if (flag != MQO_RMSHMID)
            err_quit("-r must be used by itself\n");
        else if (prgdata->pid)
            err_quit("maintserverQ must be shut down first\n");
        else
#ifdef _GNU_SOURCE
            /* 04041 */
            shmctl(shmid, IPC_RMID, (struct shmid_ds *)NULL);
#else
            shmctl(shmid, 0, IPC_RMID, 0);
#endif
    }
            
    if (flag & MQO_SHUTDOWN) {
        if (prgdata->pid)
            kill(prgdata->pid, SIGUSR1);
    }

    if (flag & MQO_DBGFILE) {
        if (!strcmp(debug, "on"))
            prgdata->debuglog = TRUE;
        else if (!strcmp(debug, "off"))
            prgdata->debuglog = FALSE;
        else
            err_quit("%s is invalid for -d; use <on/off>\n");
    }

    if (flag & MQO_LOGTRANS) {
        if (!strcmp(logtrans, "on"))
            prgdata->logtransactions = TRUE;
        else if (!strcmp(logtrans, "off"))
            prgdata->logtransactions = FALSE;
        else
            err_quit("%s is invalid for -l; use <on/off>\n");
    }

    if (flag & MQO_MAILTO) {
        if (!strcmp(mailto, "nobody"))
            memset(prgdata->mailto, 0, sizeof(prgdata->mailto));
        else
            strcpy(prgdata->mailto, mailto);
    }

    if (flag & MQO_TIMEOUT)
        prgdata->timeout = (short)newtimeout;

    if (flag & MQO_VIEWMEM)
        viewmemory();

    shmdt(prgdata);
    
    semctl(semid, 0, SETVAL, 0);

}

void viewmemory(void)
{
    printf("network timeout value = %d\n", prgdata->timeout);
    printf("debug logging = %s\n", 
        (prgdata->debuglog) ? "TRUE" : "FALSE");
    printf("transaction logging = %s\n",
        (prgdata->logtransactions) ? "TRUE" : "FALSE");
    printf("maximum connections allowed = %d\n", prgdata->maxconnections);
    printf("current number of connections = %d\n", prgdata->numconnections);
    printf("send mail to %s\n", prgdata->mailto);
    printf("process id = %d\n", prgdata->pid);
    printf("maintserverQ is ");
    switch (prgdata->status) {
        case MSQ_INITIALIZING:
            printf("initializing\n");
            break;
        case MSQ_RUNNING:
            printf("running\n");
            break;
        case MSQ_SHUTTINGDOWN:
            printf("shutting down\n");
            break;
        case MSQ_TERMINATED:
            printf("not running\n");
            break;
    }
}
