#ifndef _NEWMAINT_H_
#define _NEWMAINT_H_
#include <sys/types.h>

/* keys for ipc's */
#define SHM_NEWMAINT    (('W' << 8) + 20)
#define SEM_NEWMAINT    (('W' << 8) + 21)

/* fifo filename */
#define FIFO_FILENAME    "/IMOSDATA/2/QUEUES/maintq.fifo"

/* temporary mail file */
#define MAIL_FILENAME    "/tmp/IQmail.txt"

/* backlog file */
#define BACK_FILENAME    "/IMOSDATA/2/QUEUES/maintq.hold"

/* queue file prefix */
#define QF_PREFIX        "towesdoc"

/* processed queue file prefix */
#define QF_SENT          "senttowesdoc"

/* status codes for transmitter */
#define MNT_INITIALIZING     0x00
#define MNT_RUNNING          0x01
#define MNT_SHUTTINGDOWN     0x02
#define MNT_TERMINATED       0x03

/* mail message codes - currently 6 are used; there is a capacity for 16 */
#define MSG_OPENBUFFER       0x0001
#define MSG_READERROR        0x0002
#define MSG_NOTHOST          0x0004
#define MSG_NOTSERVICE       0x0008
#define MSG_SOCKET           0x0010
#define MSG_CONNECT          0x0020
#define MSG_BADVERSION       0x0040
#define MSG_BADQUEUEDIR      0x0080

/* options for IQconfig */
#define OPT_BLOCKSIZE        0x0001
#define OPT_INTERACTIVE      0x0002
#define OPT_SHUTDOWN         0x0004
#define OPT_NEWWAIT          0x0008
#define OPT_VIEWMEM          0x0010
#define OPT_RMSHMID          0x0020
#define OPT_QUEUEDBG         0x0040
#define OPT_TRANSMITDBG      0x0080
#define OPT_MAILTO           0x0100
#define OPT_DELETEFILES      0x0200
#define OPT_TIMEOUT          0x0400
#define OPT_KICKSTART        0x0800
#define OPT_MAXRETRIES       0x1000

/* structure that contains the program data */
typedef struct {
    short msgblocksize;             /* # of records in a block              */
    short maxwait;                  /* seconds to wait for records          */
    short timeout;                  /* network time-out value               */
    short mailedmsgs;               /* flags for mailed messages            */
    short txstatus;                 /* execution status of transmitter      */
    short queuestatus;              /* execution status of queue controller */
    bool_t debugtx;                   /* log to transmitter debug file        */
    bool_t debugqueue;                /* log to queue controller debug file   */
    bool_t deletefiles;               /* delete queue files                   */
    bool_t wesdocdown;                /* ignore maxwait; wesdoc down          */
    u_int32_t numtransactions;  /* number of transactions               */
    u_int32_t rejections;       /* wesdoc recjections                   */
    char mailto[256];               /* mail to this address                 */
    pid_t pidtx;                    /* process id of transmitter            */
    pid_t pidqueue;                 /* process id of queue controller       */
    short maxretries;               /* maximum # of retries                 */
} MAINTTRANSDATA;

#endif
