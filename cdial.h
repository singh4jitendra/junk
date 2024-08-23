#ifndef CDIAL_H
#define CDIAL_H

/* common dial structs between dialer and client */

struct FNAMES {
   struct FNAMES *next;
   char *name;
};



struct DIAL_PARMS {
   int client;           /* set to 1 if client side */
   int baud;             /* baud rate */
   char *line;           /* pointer to line name */
   char *path;           /* pointer to file transfer directory path */
   char *script;         /* pointer to alternate script file */
   char *phone;          /* pointer to phone number string */
   char *log;            /* pointer to log file */
   int debug;            /* debug flags */
   int verbose;          /* veberose flag */
   int fdev;             /* device file for this line */
   int flow;             /* flow control state, used to disable flow on close */
   char direction;       /* in, out, or both */
   int connected;        /* connection exists */
   int rx_state;         /* receive side state */
   int tx_state;         /* transmit side state */
   int rx_fds;           /* receive file descriptor */
   int tx_fds;           /* transmit file descriptor */
   int rx_offset;        /* next expected offset */
   int tx_offset;        /* last sent frame's offset */
   int tx_highcrc;       /* highest crc'd frame */
   int tx_last_ack;      /* last acked frame's offset */
   int eof;              /* tx file at eof */
   int tx_retries;       /* transmit frame retries */
   int rx_retries;       /* used for invite frame, send on receive side */
   int tx_errno;         /* errno for tx side, remember t/o error for resend */
   int rx_errno;         /* errno, used for invite */
   int tx_crc;           /* whole-file crc, used for end_receive frame */
   int tx_timeouts;      /* number of times a retry occurred */
   int rxdb;             /* number of data bytes received */
   int txdb;             /* number of data bytes sent */
   int rxdbT;            /* Total number of data bytes received */
   int txdbT;            /* Total number of data bytes sent */
   int txcrcerr;         /* set when eof error from other side halts tx */
   time_t tput_timer;    /* time at which we emit a throughput record */
   time_t read_timer;    /* time at which we blow off a read */
   time_t write_timer;   /* time at which we resend */
   char rxname[120];     /* name or current receive file */
   struct FNAMES *nhead; /* pointer to list of files names to transmit */
};

extern struct DIAL_PARMS *g_parmsp;

#define RX_IDLE 0
#define RX_RECEIVING_DATA 1
#define RX_DONE 2
#define RX_INVITE 3

#define TX_IDLE 0
#define TX_WF_OK 1
#define TX_SENDING 2
#define TX_WF_ABEND_ACK 3
#define TX_WF_END_ACK 4
#define TX_WF_DONE_ACK 5
#define TX_DONE 6
#define TX_WF_ABORT_ACK 7



#define SCRIPT_INIT 1
#define SCRIPT_EXIT 2
#define TIMEOUT 2
#define DEFAULT_SCRIPT "setup.scr"

int do_script(struct DIAL_PARMS *,int);
void perror_out(struct DIAL_PARMS *, char *,int);

#define DIRC_IN 'i'
#define DIRC_OUT 'o'
#define DIRC_BOTH 'b'

/* EXIT error codes */

#define EXIT_NO_PARMS 1
#define EXIT_BAD_PARM 2
#define EXIT_LINE_ERR 3
#define EXIT_NO_DIAL_RESP 4
#define EXIT_NO_MEM 5
#define EXIT_SPAWN_FAILURE 6
#define EXIT_FILE_ERR 7
#define EXIT_NO_FILES 8
#define EXIT_IO_ERR 9
#define EXIT_NO_CTS 10

#endif
