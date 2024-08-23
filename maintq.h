#ifndef _MAINTQ_H_
#define _MAINTQ_H_

/* keys for ipc's */
#define SHM_MAINTQ    (('W' << 16) + 22)
#define SHM_MAINTQCP  (('W' << 16) + 33)

/* keys for semaphores */
#define SEM_MAINTQ    (('W' << 16) + 40)
#define SEM_MAINTQCP  (('W' << 16) + 41)

# if defined (_MAINTSERVERQ_CP)
#    define MQ_INI_SECTION                 "maintserverqcp"
#    define LOG_FILENAME                   "translogcp.msq"
#    define DBG_FILENAME                   "maintserverqcp.dbg"
#    define MQ_SERVICE                     "WDOCUPDQCP"
#    define MQ_SEMAPHORE_KEY               SEM_MAINTQCP
#    define MQ_SHM_KEY                     SHM_MAINTQCP

# else

#    define MQ_INI_SECTION                 "maintserverq"
#    define LOG_FILENAME                   "translog.msq"
#    define DBG_FILENAME                   "maintserverq.dbg"
#    define MQ_SERVICE                     "WDOCUPDQ"
#    define MQ_SEMAPHORE_KEY               SEM_MAINTQ
#    define MQ_SHM_KEY                     SHM_MAINTQ

# endif

/* status codes for transmitter */
#define MSQ_INITIALIZING    0x00
#define MSQ_RUNNING         0x01
#define MSQ_SHUTTINGDOWN    0x02
#define MSQ_TERMINATED      0x03

/* mail message codes */
#define MSG_COBOLERROR      0x01

/* command line option codes */
#define Q_DEBUGLOG          0x01
#define Q_DEBUGSOCKET       0x02
#define Q_LOGTRANS          0x04

/* MSQconfig command line options */
#define MQO_DBGFILE         0x0001
#define MQO_INTERACTIVE     0x0002
#define MQO_LOGTRANS        0x0004
#define MQO_SHUTDOWN        0x0008
#define MQO_VIEWMEM         0x0010
#define MQO_MAILTO          0x0020
#define MQO_TIMEOUT         0x0040
#define MQO_RMSHMID         0x0080
#define MQO_MAXCONN         0x0100
#define MQO_CURCONN         0x0200

/* temporary mail file */
#define MSQ_MAILFILE    "/tmp/MSQmail.txt"


/* number of versions supported */
#define NUMVERSIONSUPPORTED    3

#define SEM_EXIT 10

/* max buffer size for listen()  */
/* this may be overridden on the */
/* command line for cc with the  */
/* option -DMAXRECEIVES=nn where */
/* nn is the number you desire.  */
#ifndef MAXRECEIVES
#define MAXRECEIVES         50
#endif

/* structure that contains the program data */
typedef struct {
    short timeout;               /* network timeout                   */
    short status;                /* return code                       */
    short mailedmsgs;            /* messages sent to administrator    */
    short numconnections;        /* number of current connections     */
    short maxconnections;        /* maximum connections allowed       */
    bool_t logtransactions;        /* flag for logging transactions     */
    bool_t debuglog;               /* flag for debug log output         */
    bool_t debugsocket;            /* flag for socket debugging options */
    char mailto[256];            /* mail address for error messages   */
    char service[12];            /* service in /etc/services          */
    char protocol[12];           /* networking protocol               */
    pid_t pid;                   /* process id of maintserverQ        */
} MSQDATA;

/* structure for transaction report */
typedef struct {
    char branch[BRANCHLENGTH+1];
    char sku[SKULENGTH+1];
    char *starttime;
} RPTDATA;

#if !defined(NOTABLE)
void MSQ060503(char *, char *, int);
void MSQ0605P1(char *, char *, int);
void MSQ060400(char *, char *, int);

SERVERVERSIONCONTROL msqversions[] = {
    { "06.05.03", MSQ060503, "UPDINVCPQ", 1024 },
    { "06.05.P1", MSQ0605P1, "UPDINVQ", 1024 },
    { "06.04.00", MSQ060400, "UPDINVQ", 1024 }
};
#endif

#endif
