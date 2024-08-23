/* non-blocking I/O routines for bi-directional file transfer 

*/

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#ifndef DOS

#ifdef _GNU_SOURCE
#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif
#else
#include <macros.h>
#endif

#endif
#include <signal.h>
#include <errno.h>
#include "client.h"
#include "cdial.h"
#include "crc.h"
#include "xfer.h"
#include "line.h"

#define printf tprintf
#define fprintf tfprintf


#define MASK(f) (1<<(f))

static int RSM_gotflag;      /* set to 1 if we have a header flag*/
static int RSM_gotheader;    /* set to 1 if we have a header flag*/
static int RSM_index;
static struct FCB *RSM_fcb;
static struct FCB *oq_head; 
static struct FCB *oq_tail;

static struct FCB *ackq_head; 
static struct FCB *ackq_tail;

static struct FCB *iq_head;
static struct FCB *iq_tail;

#define RSM_FLAG_LEN 4


/*static char header_flag[RSM_FLAG_LEN] = 0xfe, 0xaa, 0x43, 0x01;*/
static char header_flag[RSM_FLAG_LEN] = {'t','e','s','t'};

static struct {
   struct FCB *fcb;
   int len;
   int state;
   char *p;
   int blocked;
} cw;

#define CW_IDLE 0
#define CW_HEADER 1
#define CW_DATA 2


int write_sm(struct DIAL_PARMS *);
void read_sm(struct DIAL_PARMS *);
void rx_sm (struct DIAL_PARMS *, struct FCB *, char, WORD32, WORD32);
void init_writesm(void);
void init_readsm(void);
void print_throughput(struct DIAL_PARMS *);

/*******************************************************************
nonblock_wait

Once the initial state is set up, the mainline calls us.  We spin here
until all the input and outputs are handled.

This may all appear upside down, but this is the main loop of the file
transfer process.  We wait here if the i/o is blocked, otherwise, we
pass out the work.

When a read comes in, we call the read handler.  This will eventually
finish a transfer and start a new one. 

When there is something on the write queue, we set the select bit for
write_available.  This is only done if there is something to write so
it doesn't keep waking us up. 

Either the inbound transfer process or the outbound transfer process
can place frames on the output queue, we don't care.

We exit this process by:
  Noticing that DCD has gone away. 

*/

void nonblock_wait(struct DIAL_PARMS *p)
{
   int readfds,readmask;
   int writefds,writemask;
   struct timeval timeout;
   int i;
   time_t ltime;

   readmask = MASK(p->fdev);
   if (p->debug>11)
      printf("nbreadmask=%08lx, nbwritemask=%08lx\n",readmask, writemask);

   timeout.tv_sec = 2; /* wait 2 secs */
   timeout.tv_usec=0;

   while (p->connected) {
      if ( (cw.blocked) || (cw.state!=CW_IDLE) || (oq_head) || (ackq_head))
         writemask = MASK(p->fdev);
      else writemask=0;

      readfds=readmask;
      writefds=writemask;
      if (p->debug>9)
         printf("nbread=%08lx nbwrite=%08lx\n",readfds,writefds);

      i = select(p->fdev+1, &readfds,&writefds,NULL,&timeout);

      if (p->debug>9)
         printf("i=%d, nbread=%08lx nbwrite=%08lx\n",i,readfds,writefds);

      if ((p->debug>10) && (i<0))
         printf("oops***********i=%d\n",i);

      /* check for loss of DCD before doing any I/O.  */
      if (!get_dcd(p)) {
         p->connected = 0;
      }
      else {
         if (i>0) {
            if (readfds)
               read_sm(p);   /* the read handler */

            if (writefds) {
               while ( write_sm(p));  /* Call until it returns 0 */
            }
         }

         /* now check the timeout timers */
         ltime = time(NULL);
         if ( (p->read_timer!=0) && (ltime > p->read_timer) )
            read_timeout(p);

         if ( (p->write_timer!=0) && (ltime > p->write_timer) )
            write_timeout(p);

         if ( (!p->client) && (p->tput_timer!=0) && (ltime > p->tput_timer)) 
            print_throughput(p);
      }
   }

   init_readsm();
   init_writesm();

   /* we only end up here if we are disconnected */
}


/* add an output frame to the queue.  Build a header, compute the CRC,
build an fcb, link it to the queue.   Call the output starter 
The frame must have four slack bytes at the end to place the CRC in */

void send_data_frame(struct DIAL_PARMS *p, char *data, char type, WORD32 len,
   WORD32 offset, void (*callback) (struct DIAL_PARMS *,struct FCB *))
{
   char *xh;
   char *tmp;
   WORD32 crc;
   struct FCB *fcb;
   int i;

   /* build the header */
   fcb = malloc(sizeof(struct FCB));
   if (fcb==NULL) {
      printf("ERROR: no memory in send_data_frame\n");
      exit(EXIT_NO_MEM);
   }

   fcb->p = p;
   fcb->callback = callback;
   tmp = xh = malloc(HEADERLEN+4);  /* include space for flag */

   for (i=0;i<RSM_FLAG_LEN;i++)
      move1in(&xh,header_flag[i]);

   move1in(&xh,type);
   move4in(&xh,len);
   move4in(&xh,offset);

   /* now compute the crc */
   crc = CRCINIT;
   xh = tmp ;
   tmp+=RSM_FLAG_LEN;   /* skip flag in crc */

   for (i=0;i<HEADERLEN-4;i++,tmp++) {
      crc = UPDC32 (*tmp,crc);
   }

   fflush(stdout);
   move4in(&tmp,crc);

   fcb->header = xh;

   if (len==0) {
      fcb->data = NULL;
      fcb->len = 0;
      if (data!=NULL)
         free(data);
   }
   else {
      fcb->data = data;
      fcb->len = len+4;   /* add in crc */

      /* now compute the crc */
      crc = CRCINIT;
      tmp = data;
      for (i=0;i<len;i++,tmp++) {
         crc = UPDC32 (*tmp,crc);
      }

      move4in(&tmp,crc);
   }

   /* now add this to the data queue */
   if (oq_head==NULL) {
      oq_head = fcb;
      oq_tail = fcb;
      fcb->next = NULL;
   }
   else {
      oq_tail->next = fcb;
      oq_tail = fcb;
      fcb->next = NULL;
   }
}

/* send an ack frame to the queue.  Build a header, compute the CRC,
build an fcb, link it to the queue.   Call the output starter 
The frame must have four slack bytes at the end to place the CRC in */

void send_ack_frame(struct DIAL_PARMS *p,char *data, char type, WORD32 len,
   WORD32 offset, void (*callback) (struct DIAL_PARMS *p, struct FCB *fcb))
{
   char *xh;
   char *tmp;
   WORD32 crc;
   struct FCB *fcb;
   int i;

   /* build the header */
   fcb = malloc(sizeof(struct FCB));
   if (fcb==NULL) {
      printf("ERROR: no memory in send_ack_frame\n");
      exit(EXIT_NO_MEM);
   }

   fcb->p = p;
   fcb->callback = callback;
   tmp = xh = malloc(HEADERLEN+4);  /* include space for flag */

   for (i=0;i<RSM_FLAG_LEN;i++)
      move1in(&xh,header_flag[i]);

   move1in(&xh,type);
   move4in(&xh,len);
   move4in(&xh,offset);

   /* now compute the crc */
   crc = CRCINIT;
   xh = tmp;
   tmp+=RSM_FLAG_LEN;   /* skip flag in crc */

   for (i=0;i<HEADERLEN-4;i++,tmp++) {
      crc = UPDC32 (*tmp,crc);
   }

   fflush(stdout);
   move4in(&tmp,crc);

   fcb->header = xh;
   if (len==0) {
      fcb->data = NULL;
      fcb->len = 0;
      if (data!=NULL)
         free(data);
   }
   else {
      fcb->data = data;
      fcb->len = len+4;   /* add in crc */

      /* now compute the crc */
      crc = CRCINIT;
      tmp = data;
      for (i=0;i<len;i++,tmp++) {
         crc = UPDC32 (*tmp,crc);
      }

      move4in(&tmp,crc);

      /* now add this to the ack queue */
   }

   if (ackq_head==NULL) {
      ackq_head = fcb;
      ackq_tail = fcb;
      fcb->next = NULL;
   }
   else {
      ackq_tail->next = fcb;
      ackq_tail = fcb;
      fcb->next = NULL;
   }
}

void move4in( char ** ptr, WORD32 val)
{
   int i;

   for (i=0;i<4;i++)  {
      *(*ptr) = (char) ((val>>(8*i)) & 0xff);
      (*ptr)++;
   }
}

void move1in( char ** ptr, char val)
{
   int i;

   *(*ptr) = (char) (val & 0xff);
   (*ptr)++;
}

void move4out( char ** ptr, WORD32 *val)
{
   int i;

   *val=0;
   for (i=0;i<4;i++)  {
      *val |= ((unsigned char) **ptr) << (8*i);
      (*ptr)++;
   }
}

void move1out( char ** ptr, char *val)
{
   int i;

   *val=**ptr;
   (*ptr)++;
}

/***************************************************
Called when we suspect there may be room to write.  We're in nonblock
mode, so write will return the actual number of bytes written, which
may be zero.
****************************************************/

int write_sm(struct DIAL_PARMS *p)
{
   int len;
   int retval = 0;

   cw.blocked = 0;

   /* setup a new frame if we haven't got one */
   if (cw.state == CW_IDLE) {
      cw.fcb = NULL;
      if (ackq_head!=NULL) {
         cw.fcb = ackq_head;
         ackq_head = ackq_head->next;
      }
      else if (oq_head!=NULL) {
         cw.fcb = oq_head;
         oq_head = oq_head->next;
      }

      if (cw.fcb) { 
         cw.state = CW_HEADER;
         cw.len = HEADERLEN+4; /* include flag */
         cw.p = cw.fcb->header;
      }
      else
         return 0; /* no need to call us again */
   }

   /* this is either a new frame if we've gone through the above if, or
      we're continuing a frame that there wasn't room for last time.
   */
   len = write(p->fdev, cw.p, cw.len);
   if (cw.state == CW_DATA)
      p->txdb += len;

   if (p->debug>20) {
      printf("write: ");
      for (retval=0;retval<len;retval++) {
         printf("%02x ",(unsigned char) *(cw.p+retval));
      }
      printf("\n");
      fflush(stdout);
   }

   if (len==-1) {
      if (errno=EAGAIN)
         len=0;
      else {
         perror_out(p,"on nonblock write",errno);
         exit(EXIT_IO_ERR);
      }
   }

   cw.len -= len;
   if (cw.len<0) {
      printf("ERROR: len went below zero in write_sm: %d\n",len); 
      exit(EXIT_IO_ERR);
   }

   if (cw.len == 0) {
      /* we finished that piece.  See if there is any more */
      if (cw.state==CW_HEADER) {
         if (cw.fcb->data) {
            cw.state=CW_DATA;
            cw.len = cw.fcb->len;
            cw.p = cw.fcb->data;
            return 1; /* call us again to finish it */
         }
      }
   }
   else {
      /* didn't get it all written.  Adjust state */
      cw.p += len;
      cw.blocked=1;
      return 0;   /* we're blocked */
   }

   /* We get here only if this frame is done.  
      Call the callback routine, set up another */

   cw.fcb->endstatus = END_OK;
   if (cw.fcb->callback!=NULL)
      cw.fcb->callback(p,cw.fcb); 
   else {
      if (cw.fcb->data!=NULL)
         free (cw.fcb->data);

      if (cw.fcb->header!=NULL)
         free (cw.fcb->header);

      free (cw.fcb);
   }

   cw.fcb = NULL;
   cw.state = CW_IDLE;

   if (oq_head || ackq_head)
      return 1;   /* more to be done */
   else
      return 0;   /* nothing to be done */
}


/*************************************************************************

read_sm 

reads bytes from input stream.  

*************************************************************************/
void read_sm(struct DIAL_PARMS *p)
{
   int len;
   int i;
   WORD32 crc;
   WORD32 tcrc;
   static char nhtype;
   static WORD32 nhlen;
   static WORD32 nhoffset;
   char *tmp;

   if (RSM_fcb == NULL) {
      RSM_fcb  = calloc(1,sizeof(struct FCB));

      if (RSM_fcb == NULL) {
         printf("ERROR: no memory in read_sm\n");
         exit (EXIT_NO_MEM);
      }

      RSM_fcb->p = p;
      RSM_fcb->header = malloc(HEADERLEN);
      if (RSM_fcb->header==NULL) {
         printf("ERROR: no memory in read_sm\n");
         exit (EXIT_NO_MEM);
      }

      init_readsm();
   }

   if (!RSM_gotflag) {
      /* look for the flag.  Use the header buffer as a scratch area,
         we don't return the flag as part of the header */
      len = read(p->fdev, RSM_fcb->header, RSM_FLAG_LEN);
      if (p->debug>20) {
         printf("read#1 l:%d\n",len);
         printf("read: ");
         for (i=0;i<len;i++) {
            printf("%02x ",(unsigned char) *(RSM_fcb->header+i));
         }
         printf("\n");
         fflush(stdout);
      }

      if (len==-1) {
         if (errno!=EAGAIN) {
            perror_out(p, "on read buffer",errno);
            exit(EXIT_IO_ERR);
         }
         else
            len=0;
      }

      if (len==0)
         return;

      for (i=0;i<len;i++) {
         if (p->debug>20)
            printf("%02x:%02x, i:%d, index:%d\n",
               RSM_fcb->header[i], header_flag[RSM_index], i, RSM_index);

         if (RSM_fcb->header[i] == header_flag[RSM_index]) {
            if ((++RSM_index) >= RSM_FLAG_LEN ) {
               /* got the start of the packet.  Shuffle the rest of the
                  bytes down */
               RSM_gotflag=1;
               i++;
               memmove(RSM_fcb->header, RSM_fcb->header+i, len-i);
               RSM_fcb->len = len-i;
               break;
            }
         }
         else
            RSM_index=0;   /* reset to start of header */

         if (p->debug>20)
            printf("%02x:%02x, i:%d, index:%d\n",
               RSM_fcb->header[i], header_flag[RSM_index], i, RSM_index);
      }

      if (!RSM_gotflag)
         return;
   }

   /* we only get here if we have a flag.  Now get the header.  We'll only
      have a few bytes in the buffer, if any.  Read again.  fcb->len has the
      number of bytes received so far.  Read enough to get the header if we
      don't have it yet.*/
   if (!RSM_gotheader) {
      len = read(p->fdev, RSM_fcb->header+RSM_fcb->len, HEADERLEN-RSM_fcb->len);
      if (p->debug>20) {
         printf("read#2 l:%d\n",len);
         printf("read: ");
         for (i=0;i<len;i++) {
            printf("%02x ",(unsigned char) *(RSM_fcb->header+RSM_fcb->len+i));
         }
         printf("\n");
         fflush(stdout);
      }

      if (len==-1) {
         if (errno!=EAGAIN) {
            perror_out(p, "on read buffer",errno);
            exit(EXIT_IO_ERR);
         }
         else
            len=0;
      }

      if (len==0)
         return;

      RSM_fcb->len+=len;
      if (RSM_fcb->len == HEADERLEN) {
         /* check the header CRC */
         crc = CRCINIT;
         for (i=0;i<HEADERLEN-4;i++) {
            crc = UPDC32 (RSM_fcb->header[i],crc);
         }

         tmp = RSM_fcb->header;
         move1out(&tmp, &nhtype);
         move4out(&tmp, &nhlen);
         move4out(&tmp, &nhoffset);
         move4out(&tmp, &tcrc);

         if (tcrc!=crc) {
            if (p->debug>3) {
               printf("Bad header crc received after flag detect\n");
            }

            init_readsm();
            return;
         }

         RSM_gotheader = 1;
      }
      else
         return;  /* need more bytes for the header */

      /* we have a header, and crc was ok. */
      if (nhlen > MAXDATA) {
         printf("length in received packet exceeds max: %d\n",nhlen);
         init_readsm();
         return;
      }

      if (nhlen>0) {
         RSM_fcb->rxrem = nhlen+4;   /* include crc */
         RSM_fcb->data = malloc(RSM_fcb->rxrem);
         if (RSM_fcb->data ==NULL) {
            printf("no mem to receive data buffer\n");
            exit(EXIT_NO_MEM);
         }

         RSM_fcb->len = 0; /* len also serves as the inset point */
      }
      else {
         RSM_fcb->data = NULL;
         RSM_fcb->len = 0;
         RSM_fcb->rxrem = 0;
      }
   }

   /* we get here if we're getting the rest of the data frame */
   if (RSM_fcb->rxrem!=0) {
      len = read(p->fdev, RSM_fcb->data+RSM_fcb->len, RSM_fcb->rxrem);
      if (p->debug>20) {
         printf("read#3 l:%d\n",len);
         printf("read: ");
         for (i=0;i<len;i++) {
            printf("%02x ",(unsigned char) *(RSM_fcb->data+RSM_fcb->len+i));
         }
         printf("\n");
         fflush(stdout);
      }

      if (len==-1) {
         if (errno!=EAGAIN) {
            perror_out(p, "on read buffer",errno);
            exit(EXIT_IO_ERR);
         }
         else
            len=0;
      }

      RSM_fcb->len += len;
      RSM_fcb->rxrem -= len;
   }

   /* If we still don't have the complete frame, return */
   if (RSM_fcb->rxrem)
      return;

   /* otherwise, check the CRC if there is data in the frame */
   if (RSM_fcb->len > 0) {
      crc = CRCINIT;
      for (i=0;i<RSM_fcb->len-4;i++) {
         crc = UPDC32 (RSM_fcb->data[i],crc);
      }

      tmp = RSM_fcb->data+RSM_fcb->len-4;
      move4out(&tmp, &tcrc);

      if (tcrc != crc)  {
         printf("bad crc on data buffer\n");
         init_readsm();
         return;
      }
   }

   /* if the CRC was ok (or no data) , pass on the frame. */
   rx_sm (p,RSM_fcb, nhtype, nhlen, nhoffset);
   RSM_fcb = NULL;
}

void init_readsm()
{
   RSM_gotflag = 0;
   RSM_gotheader = 0;
   RSM_index = 0;

   if (RSM_fcb!=NULL) {
      if (RSM_fcb->data !=NULL) {
         free (RSM_fcb->data);
         RSM_fcb->data =NULL;
      }
   }
}

/* clear out the write process.  Cycle frames back to the caller */
void init_writesm()
{
   struct FCB *fcbt;

   if (cw.fcb!=NULL) {
      cw.fcb->endstatus = END_DISCON;
      if (cw.fcb->callback!=NULL)
         cw.fcb->callback(cw.fcb->p,cw.fcb); 
      else
         freeup(cw.fcb);
   }

   cw.fcb = NULL;
   cw.state = CW_IDLE;

   while (oq_head!=NULL) {
      fcbt = oq_head;
      oq_head = oq_head->next;
      fcbt->endstatus = END_DISCON;
      if (fcbt->callback!=NULL)
         fcbt->callback(fcbt->p,fcbt); 
      else
         freeup(fcbt);
   }

   while (ackq_head!=NULL) {
      fcbt = ackq_head;
      ackq_head = ackq_head->next;
      fcbt->endstatus = END_DISCON;
      if (fcbt->callback!=NULL)
         fcbt->callback(fcbt->p,fcbt); 
      else
         freeup(fcbt);
   }
}

void freeup(struct FCB *fcb)
{
   if (fcb->data!=NULL)
      free(fcb->data);

   if (fcb->header!=NULL)
      free(fcb->header);

   free (fcb);
}

void print_throughput(struct DIAL_PARMS *p)
{
   char buf [120];
   time_t interval;

   interval = (time(NULL) - p->tput_timer) + THROUGHPUT_TIME;

   p->txdbT += p->txdb;
   p->rxdbT += p->rxdb;

   if ((p->rx_state==RX_DONE) && (p->tx_state==TX_DONE)) 
      sprintf(buf, "THROUGH: r:done t:done %uk/%uk\n", p->rxdbT/1024,
         p->txdbT/1024);
   else if (p->tx_state==TX_DONE) 
      sprintf(buf, "THROUGH: r:%04d t:done %uk/%uk\n", p->rxdb/interval,
         p->rxdbT/1024,p->txdbT/1024);
   else if (p->rx_state==RX_DONE)
      sprintf(buf, "THROUGH: r:done t:%04d %uk/%uk\n", p->txdb/interval,
         p->rxdbT/1024,p->txdbT/1024);
   else
      sprintf(buf, "THROUGH: r:%04d t:%04d %uk/%uk\n", p->rxdb/interval,
         p->txdb/interval,p->rxdbT/1024,p->txdbT/1024);

   pinfo(p,buf);
   p->txdb = 0;
   p->rxdb = 0;
   p->tput_timer = time(NULL)+THROUGHPUT_TIME;
}
