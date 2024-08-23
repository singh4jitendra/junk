#ifndef XFER_H
#define XFER_H

/* defined for the BDFTP protocpol */

#define XferTimeout 4   /* seconds */
#ifdef DOS
#define WORD32 long
#else
#define WORD32 int
#endif

struct FCB {
   char *header; 
   char *data;
   WORD32 len;
   WORD32 type;
   WORD32 rxrem;
   void (*callback) (struct DIAL_PARMS *p, struct FCB *fcb);
   WORD32 endstatus;
   struct DIAL_PARMS *p;
   struct FCB *next;
};

#define END_OK 1
#define END_DISCON 2

#define FCB_ACK 1
#define FCB_DATA 2

/* notional header
int32 flag
char type
int32 len
int32 offset
int32 header_Crc
*/

#define FTYPE_START_RECEIVE 1
#define FTYPE_START_RECEIVE_OK 2
#define FTYPE_START_RECEIVE_RESTART 3
#define FTYPE_FRMR 4
#define FTYPE_START_RECEIVE_ERR 5
#define FTYPE_END_RECEIVE 6
#define FTYPE_END_ACK 7
#define FTYPE_ABEND 8
#define FTYPE_ABEND_ACK 9
#define FTYPE_DATA 10
#define FTYPE_TX_DONE 11
#define FTYPE_DONE_ACK 12
#define FTYPE_ACK 13
#define FTYPE_RX_INVITE 14
#define FTYPE_ABORT_RECEIVE 15
#define FTYPE_ABORT_ACK 16

#define FRMR_1 1
#define FRMR_2 2
#define FRMR_3 3

#define HEADERLEN 13 /* does not include flag */
#define MAXDATA 1024
#define MAXDOUT 3*MAXDATA

   /* allow 20 seconds for an outstanding frame to
      be acked.  Frames shouldn't be missed, the only 
      reason for a delay is because the modems are retrying, or
      there is a software bug. */

#define WRITE_TIMEOUT 20
#define READ_TIMEOUT 60 
#define MAX_RETRIES 3
#define MAX_TIMEOUTS 10
#define THROUGHPUT_TIME 10

#define RESTART_SHORT -2
#define RESTART_CRC -3

void nonblock_wait(struct DIAL_PARMS *p);
void rx_sm_init(struct DIAL_PARMS *);
void tx_sm_init(struct DIAL_PARMS *);
void rx_sm_close(struct DIAL_PARMS *);
void tx_sm_close(struct DIAL_PARMS *);
void send_data_frame(struct DIAL_PARMS *, char *, char, WORD32, WORD32, 
   void (*) (struct DIAL_PARMS *p, struct FCB *fcb) );
void send_ack_frame(struct DIAL_PARMS *,char *, char, WORD32, WORD32, 
   void (*) (struct DIAL_PARMS *p, struct FCB *fcb) );
void read_timeout(struct DIAL_PARMS *);
void write_timeout(struct DIAL_PARMS *);
void move4in( char ** , WORD32 );
void move1in( char ** , char );
void move4out( char **, WORD32 *);
void move1out( char **, char *);
void freeup(struct FCB *);
void start_next_out_file(struct DIAL_PARMS *);
void takeitdown(struct DIAL_PARMS *);
void send_tx_done(struct DIAL_PARMS *);
void advance_nhead(struct DIAL_PARMS *);
void rx_sm_close(struct DIAL_PARMS *);
void tx_sm_close(struct DIAL_PARMS *);
void invite_send (struct DIAL_PARMS *);
void pinfo(struct DIAL_PARMS *, char *);

#endif
