#ifndef _NEWMAINT_H_
#define _NEWMAINT_H_

/* keys for ipc's */
#define SHM_GQCONFIG    (('W' << 8) + 22)

/* temporary mail file */
#define MAIL_FILENAME    "/tmp/GQmail.txt"

/* status codes for transmitter */
#define GQ_INITIALIZING     0x00
#define GQ_RUNNING          0x01
#define GQ_SHUTTINGDOWN     0x02
#define GQ_TERMINATED       0x03

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
#define OPT_SHUTDOWN         0x0001
#define OPT_NEWWAIT          0x0002
#define OPT_VIEWMEM          0x0004
#define OPT_RMSHMID          0x0008
#define OPT_DEBUG            0x0010
#define OPT_MAILTO           0x0020
#define OPT_DELETEFILES      0x0040
#define OPT_TIMEOUT          0x0080
#define OPT_MAXRETRIES       0x0100

/* structure that contains the program data */
typedef struct {
    short msgblocksize;             /* # of records in a block              */
    short maxwait;                  /* seconds to wait for records          */
    short timeout;                  /* network time-out value               */
    short mailedmsgs;               /* flags for mailed messages            */
    short status;                   /* execution status of transmitter      */
    short maxretries;               /* max # of retries (application error) */
    bool_t debug;                     /* log to transmitter debug file        */
    bool_t deletefiles;               /* delete queue files                   */
    char mailto[256];               /* mail to this address                 */
    pid_t pid;                      /* process id of transmitter            */
} GQDATA;

/* transmission error structure */
typedef struct {
   char filename[FILENAMELENGTH];
   short retries;
} ERROR_RETRANSMIT;

#endif
