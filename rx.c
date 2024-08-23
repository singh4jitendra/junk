/* hep 940920 - blow off remaining output files if we get an error on an
   crc ack from the other side.  Set txcrcerr flag. 

   hep 940920 On receive side, delete the file if crc error.

   hep 940920 On crc error and subsequent rewind of tx file, we double
              crc the resent bytes.  This is bad.  Use tx_highcrc to
              avoid it.

*/ 

/* handler for received frames */
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

#else
#include <io.h>
#endif

#include <errno.h>
#include <sys/stat.h>
#include "cdial.h"
#include "xfer.h"
#include "crc.h"
#include "line.h"

#define printf tprintf
#define fprintf tfprintf



static char tbuf[120];

void start_receive(struct DIAL_PARMS *, char *);
void start_receive_ok(struct DIAL_PARMS *);
void start_receive_restart(struct DIAL_PARMS *, WORD32, WORD32);
void xfer_next_data(struct DIAL_PARMS *, struct FCB *);
void rename_txcomplete(struct DIAL_PARMS *);
void receive_eof(struct DIAL_PARMS *, WORD32);
void receive_abort(struct DIAL_PARMS *, int);
void receive_data(struct DIAL_PARMS *, char *, int, int);
void receive_ack(struct DIAL_PARMS *, int );


void rx_sm_init(struct DIAL_PARMS *p)
{
	p->rx_state = RX_IDLE;
		/* make sure we timeout if nothing is coming */
	p->read_timer = time(NULL)+READ_TIMEOUT;  
	p->rxdb = 0;
}

void tx_sm_init(struct DIAL_PARMS *p)
{
	p->tx_state = TX_IDLE;
	p->write_timer = 0;
	p->tx_retries = 0;
	p->tx_timeouts = 0;
	p->txdb = 0;
	p->tput_timer = time(NULL)+THROUGHPUT_TIME;
}




/* Called when a frame has been received.  It has already been CRC
checked. */

void rx_sm (struct DIAL_PARMS *p, struct FCB *fcb, char type, WORD32 len, WORD32 offset)
{
	WORD32 crc;
	char *tmp;

	if (p->debug>20) {
		printf("rx_sm: type:%02x, len:%d, offset:%d\n",type, len, offset);
	}

	p->rxdb += len;	/* add in number of characters received */
	switch(type) {

		case FTYPE_START_RECEIVE:
		/* sent when the other side wants us to receive a file.  The zero
			terminated file name is in the data.  We ALWAYS prepend our 
			path to the given name */

			start_receive(p,fcb->data);
			break;

		case FTYPE_START_RECEIVE_OK:
		/* sent in response to our request to send a file.  */

			start_receive_ok(p);
			break;

		case FTYPE_START_RECEIVE_RESTART:
		/* The file exists on the other side.  This is a request to check
			the CRC of the portion of the file that was already received.
		*/

			tmp = fcb->data;
			move4out(&tmp,&crc);
			start_receive_restart(p,offset,crc);
			break;

		case FTYPE_ABEND_ACK:
			if (p->tx_state != TX_WF_ABEND_ACK) {
				sprintf(tbuf,"ERROR: unexpected state/event: %u,%u\n",p->tx_state, type); 
				pinfo(p,tbuf);
				takeitdown(p);
				retry(p);
			}
/*
use this line if you want to continue sending
			else start_next_out_file(p);
use these lines if you want to stop on a bad file
*/
			retry(p);
			p->write_timer = 0;
			p->tx_state = TX_DONE;
			if (p->rx_state == RX_DONE) takeitdown(p);

			break;

		case FTYPE_END_ACK:
			if (p->tx_state != TX_WF_END_ACK) {
				sprintf(tbuf,"ERROR: unexpected state/event: %u,%u\n",p->tx_state, type); 
				pinfo(p,tbuf);
				takeitdown(p);
				retry(p);
			}
			else {
				if (offset==0) {
					/* there was no error.  Rename this file to reflect its completed
					state */
					close(p->tx_fds);
					p->tx_fds=-1;
					rename_txcomplete(p);
				}
				else {
					sprintf(tbuf,"ERROR: EOF error from other side (%d): %s\n",
						offset,p->nhead->name);
					pinfo(p,tbuf);
					sprintf(tbuf,"ERROR: Aborting remaining tx files\n");
					pinfo(p,tbuf);
					close(p->tx_fds);
					p->tx_fds=-1;
					p->nhead = NULL;
					p->txcrcerr = 1;
					retry(p);
				}


				p->write_timer = 0;
				advance_nhead(p);
				start_next_out_file(p);
			}
			break;

		case FTYPE_ABORT_ACK:
			if (p->tx_state != TX_WF_ABORT_ACK)  {
				sprintf(tbuf,"ERROR: unexpected state/event: %u,%u\n",p->tx_state, type); 
				pinfo(p,tbuf);
				takeitdown(p);
				retry(p);
			}
			else {

				p->write_timer = 0;
				advance_nhead(p);
				start_next_out_file(p);
			}
			break;

		case FTYPE_END_RECEIVE:
			receive_eof(p,offset);
			break;

		case FTYPE_ABORT_RECEIVE:
			receive_abort(p,offset);
			break;

		case FTYPE_TX_DONE:

			pinfo(p,"INFO: RECEIVED_TX_DONE\n");	

			if ((p->rx_state != RX_IDLE) &&  (p->rx_state != RX_DONE)
					&&  (p->rx_state != RX_INVITE)) {
				sprintf(tbuf,"ERROR: unexpected state/event: %u,%u\n",p->tx_state, type); 
				pinfo(p,tbuf);
				takeitdown(p);
				retry(p);
			}
			send_ack_frame(p,NULL, FTYPE_DONE_ACK, 0, 0, NULL);
			p->rx_state = RX_DONE;
			p->read_timer = time(NULL)+10;  /* short timer, allow ack to get there */

			break;

		case FTYPE_DONE_ACK:
			if (p->debug>3) {
				printf("Received TX_DONE_ACK\n");
				fflush(stdout);
			}
			if (p->tx_state != TX_WF_DONE_ACK) {
				sprintf(tbuf,"ERROR: unexpected state/event: %u,%u\n",p->tx_state, type); 
				pinfo(p,tbuf);
				takeitdown(p);
				retry(p);
			}
			p->write_timer = 0;
			p->tx_state = TX_DONE;
			if (p->rx_state == RX_DONE) takeitdown(p);
			break;


		case FTYPE_FRMR:
			sprintf(tbuf,"ERROR: Protocol error indication received from other side: %d\n",offset);
			pinfo(p,tbuf);
			takeitdown(p);
			retry(p);
			break;

		case FTYPE_START_RECEIVE_ERR:
			sprintf(tbuf,"ERROR: Error from other side on start of transmission: %d\n",offset);
			pinfo(p,tbuf);
			close(p->tx_fds);
			p->tx_fds=-1;
			advance_nhead(p);
			start_next_out_file(p);
			break;

		case FTYPE_DATA:
			receive_data(p, fcb->data, len, offset);
			break;

		case FTYPE_ACK:
			receive_ack(p,offset);
			break;

		case FTYPE_RX_INVITE:
			if (p->debug>3) printf("receive rx invite\n");
			init_out_file(p);
			start_next_out_file(p);
			break;

		default:
			sprintf(tbuf,"ERROR: Unknown frame type: %02x, ignored\n",type);
			pinfo(p,tbuf);
			break;

	}
	freeup(fcb);

}

void start_receive(struct DIAL_PARMS *p, char *s)
{

	/* we've been asked to receive a file.  The file goes in our path/in */
	WORD32 crc;
	char fname[120];
	char fname2[120];
	char buf[256];
	char *tmp,*tmp2;
	WORD32 len,rlen;
	int i;

	p->read_timer = time(NULL)+READ_TIMEOUT;
	if ((p->rx_state != RX_IDLE) && (p->rx_state != RX_INVITE)){
		if ( (p->rx_state==RX_RECEIVING_DATA) && (p->rx_offset==0)) {
			/*allow for a retried rx_idle */
			close(p->rx_fds);
			p->rx_fds=-1;
			if (p->debug>3) printf("Received repeated start_receive request \n");
		}
		else {
			sprintf(tbuf,"ERROR: Received start_receive request when not in idle state\n");
			pinfo(p,tbuf);
			send_ack_frame(p,NULL, FTYPE_FRMR, 0, FRMR_1, NULL);
			return;
		}
	}

	p->rx_offset = 0;

	sprintf(tbuf,"INFO: START_RECEIVE: %s\n",s);
	pinfo(p,tbuf);
	strcpy(fname, p->path);
	strcat(fname,"/in/");
	strcat(fname,s);
	strcpy(p->rxname,fname);

	/* first, see if a completed file with that name exists */

	strcpy(fname2,fname);
	strcat(fname2,".in");

	p->rx_fds = open (fname,O_RDONLY);
	if (p->rx_fds==-1) {
		/* we expect "does not exist" */
		if (errno!=ENOENT) {
			/* send the error back, can't deal with this file */
			send_ack_frame(p,NULL,FTYPE_START_RECEIVE_ERR,0,errno,NULL);
			sprintf(tbuf,"ERROR: Can't receive (1): %s\n",s);
			perror_out(p,tbuf,errno); 
			return;
		}
	}

/* now try the inprogress name */

	strcat(fname,".prog");

	p->rx_fds = open (fname,O_RDONLY);
	if (p->rx_fds==-1) {
		/* we expect "does not exist" */
		if (errno==ENOENT) {
			p->rx_state = RX_RECEIVING_DATA;
			/* prepare the file for write */
			close (p->rx_fds);
			p->rx_fds=-1;
			if ((p->rx_fds = open (fname, O_WRONLY | O_CREAT, 
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1) {
				send_ack_frame(p,NULL,FTYPE_START_RECEIVE_ERR,0,errno,NULL);
				sprintf(tbuf,"ERROR: Can't receive(2): %s\n",s);
				perror_out(p,tbuf,errno); 
			}
			else {
				send_ack_frame(p,NULL,FTYPE_START_RECEIVE_OK,0,0,NULL);
				p->rx_state = RX_RECEIVING_DATA;
				p->read_timer = time(NULL)+READ_TIMEOUT;
			}
		}
		else {
			/* send the error back, can't deal with this file */

			send_ack_frame(p,NULL,FTYPE_START_RECEIVE_ERR,0,errno,NULL);
			sprintf(tbuf,"ERROR: Can't receive(3): %s\n",s);
			perror_out(p,tbuf,errno);
		}
	}
	else {
		/* the file exists */
		/* read it, crc it, send back the restart message */

		crc = CRCINIT;
		len = 0;
		while ( (rlen=read(p->rx_fds,buf,sizeof(buf))) > 0 ) {
			len += rlen;
			tmp = buf;
			for (i=0;i<rlen;i++,tmp++) {
				crc = UPDC32 (*tmp,crc);
			}
		}
		if (rlen==-1) {
			/* we got an error (not EOF).  Blow this one off */
			send_ack_frame(p,NULL,FTYPE_START_RECEIVE_ERR,0,errno,NULL);
			sprintf(tbuf,"ERROR: Can't receive(4): %s\n",s);
			perror_out(p,tbuf,errno);
		}

		else {
			/* prepare the file for append */
			close (p->rx_fds);
			p->rx_fds=-1;
			if ((p->rx_fds = open (fname, O_WRONLY | O_APPEND)) == -1) {
				send_ack_frame(p,NULL,FTYPE_START_RECEIVE_ERR,0,errno,NULL);
				sprintf(tbuf,"ERROR: Can't receive(5): %s\n",s);
				perror_out(p,tbuf,errno);
			}
			else {
				/* send back the restart message with the length we have in offset,
				and the crc in the data field */
				tmp2 = tmp = malloc(4+4);
				if (tmp==NULL) return;
				move4in(&tmp,crc);
				sprintf(tbuf,"INFO: RESTART_RECEIVE %s at %d, crc %08x\n",s,len,crc);
				pinfo(p,tbuf);
				p->rx_offset = len;
				send_ack_frame(p,tmp2,FTYPE_START_RECEIVE_RESTART,4,p->rx_offset,NULL);
				p->rx_state = RX_RECEIVING_DATA;
				p->read_timer = time(NULL)+READ_TIMEOUT;
			}
		}
	}
}
			
 
/* We got the start message back from the other side.  Send the first
block */
	
void start_receive_ok(struct DIAL_PARMS *p)
{
	if (p->tx_state != TX_WF_OK) {
		sprintf(tbuf,"ERROR: Received start_receive_ok when not in waiting state\n");
		pinfo(p,tbuf);
		send_ack_frame(p,NULL, FTYPE_FRMR, 0, FRMR_2, NULL);
		return;
	}

	p->tx_last_ack = 0;
	p->tx_offset = 0;
	p->eof = 0;	
	p->tx_retries = 0;
	p->tx_highcrc = 0;
	p->tx_state = TX_SENDING;
	p->tx_crc = CRCINIT;

	if (p->debug>3) {
		printf("start ok on %s\n",p->nhead->name);
		fflush(stdout);
	}
	xfer_next_data(p,NULL);
}


/********************************************************
xfer_next_data

Send the next data block of this file

*********************************************************/
void xfer_next_data(struct DIAL_PARMS *p,struct FCB *fcb)
{
	char *buf;
	int len;
	int i;

	if (fcb!=NULL) freeup(fcb);

			/* don't send anything else if we've got the pipe full.  We'll
			get kicked off again by the timer, or some other write complete */

	if ( (p->tx_offset - p->tx_last_ack) > MAXDOUT) return;


			/* don't send anything else if we're at end of file.  We'll get
				kicked off by the timer or the ack for the last frame */

	if (p->eof) return;

	p->write_timer = time(NULL)+WRITE_TIMEOUT;

	buf = malloc(MAXDATA+4);
	if (buf==NULL) return;  /* the timer will call us */

	len = read(p->tx_fds, buf, MAXDATA);

	if (len==0) {
		p->eof = 1;
		if ( (p->tx_offset==0) /* empty file */
				|| ( p->tx_offset == p->tx_last_ack)) /* restarting at end */ {
			/* an empty file.  Send the EOF from here.  Put it on the data queue
				so that it is in sync */	
			if (p->debug>3) printf("empty file or restart from end: %s\n",p->nhead->name);
			send_data_frame(p, NULL, FTYPE_END_RECEIVE, 0, p->tx_crc, NULL);
			p->tx_state = TX_WF_END_ACK;
		}
		free(buf);
		/* else do nothing until we get the last ack */
	}
	else if (len<0) {
		sprintf("File error on read of %s",p->nhead->name);
		perror_out(p,tbuf,errno); 
		p->tx_errno = errno;
		send_data_frame(p,NULL, FTYPE_ABEND, 0, p->tx_errno, NULL);
		p->write_timer = time(NULL)+WRITE_TIMEOUT;
		p->tx_state = TX_WF_ABEND_ACK;
		free(buf);
	}
	else {
		if (p->debug>3) {
			printf("sending offset %d\n",p->tx_offset);
			fflush(stdout);
		}
				/* only crc if this is a new frame */
		if (p->tx_offset >= p->tx_highcrc) {
			for (i=0;i<len;i++) p->tx_crc = UPDC32 (buf[i],p->tx_crc);
		}

		send_data_frame(p,buf,FTYPE_DATA, len, p->tx_offset, xfer_next_data);
		p->tx_offset += len;
		if (p->tx_offset > p->tx_highcrc) p->tx_highcrc = p->tx_offset;
	}
}


void read_timeout(struct DIAL_PARMS *p)
{
	p->read_timer = 0;

	switch (p->rx_state) {

		case RX_INVITE:
			if (p->debug>3) printf("rx invite timeout\n");
			if (++p->rx_retries > MAX_RETRIES) {
				if (p->debug>3) printf("rx invite retry exceeded\n");
				takeitdown(p);
				retry(p);
			}
			else invite_send(p);
			break;

		case RX_DONE:
			if ( (p->tx_state == TX_IDLE) || (p->tx_state == TX_DONE)) {
				if (p->debug>3) printf("rx and tx done\n");
				takeitdown(p);
			}
			else {
				if (p->debug>3) printf("read timeout with rx done\n");
			}
			break;

		default:
			if (p->debug>3) printf("unknown read timer timeout\n");
			takeitdown(p);
			retry(p);
			break;
	}
}


/* called when the write timer expires */

void write_timeout(struct DIAL_PARMS *p)
{
	int err;

	if (++p->tx_retries > MAX_RETRIES) {
		sprintf(tbuf,"ERROR: retry limit exceeded by transmit, state:%d\n",p->tx_state);
		pinfo(p,tbuf);
		takeitdown(p);
		retry(p);
		return;
	}
	if (++p->tx_timeouts > MAX_TIMEOUTS) {
		sprintf(tbuf,"ERROR: too many errors, state:%d\n",p->tx_state);
		pinfo(p,tbuf);
		takeitdown(p);
		retry(p);
		return;
	}

	switch (p->tx_state) {
	case TX_SENDING:

				/* backup the file to the last offset acked */
		p->tx_offset = p->tx_last_ack;
		p->eof = 0;
		if (p->debug>3) printf("backing up to %d\n",p->tx_offset);
		err = lseek(p->tx_fds, p->tx_offset, SEEK_SET);
		if (err==-1)  {
			perror_out(p,"error in seek",errno);
			p->tx_errno = errno;
			send_data_frame(p,NULL, FTYPE_ABEND, 0, p->tx_errno, NULL);
			p->write_timer = time(NULL)+WRITE_TIMEOUT;
			p->tx_state = TX_WF_ABEND_ACK;
		}
		else xfer_next_data(p,NULL);
		break;

	case TX_WF_ABEND_ACK:
			/* resend the abend again */
		send_data_frame(p,NULL, FTYPE_ABEND, 0, p->tx_errno, NULL);
		p->write_timer = time(NULL)+WRITE_TIMEOUT;
		break;

	case TX_WF_END_ACK:
			/* resend the eof again */
		send_data_frame(p, NULL, FTYPE_END_RECEIVE, 0, p->tx_crc, NULL);
		p->write_timer = time(NULL)+WRITE_TIMEOUT;
		break;

	case TX_WF_OK:
		close(p->tx_fds);
		p->tx_fds=-1;
		start_next_out_file(p);
		break;

	case TX_WF_DONE_ACK:
		send_tx_done(p);
		break;

	case TX_WF_ABORT_ACK:
		send_ack_frame(p,NULL, FTYPE_ABORT_RECEIVE, 0, p->tx_errno, NULL);
		p->write_timer = time(NULL)+WRITE_TIMEOUT;
		break;

	default:
		break;
	}	

}

void takeitdown(struct DIAL_PARMS *p)
{
	p->connected = 0;
	dtr_down(p);
#if !defined(_HPUX_SOURCE) && !defined(_GNU_SOURCE) && !defined(_GNU_SOURCE)
	sleep(1);
	close(p->fdev);
	open_line(p); 
#endif

}

void receive_eof(struct DIAL_PARMS *p, WORD32 crc)
{
	int err;
	int rlen;
	char in[120];
	char out[120];
	char buf[256];
	struct stat sbuf;
	WORD32 rcrc;
	int i;

	if (p->rx_state==RX_IDLE) {
			/* repeat */
		send_ack_frame(p,NULL, FTYPE_END_ACK, 0, p->rx_errno, NULL);
	}
	else if (p->rx_state != RX_RECEIVING_DATA) {
		sprintf(tbuf,"ERROR: Received eof when not in receive state\n");
		pinfo(p,tbuf);
		send_ack_frame(p,NULL, FTYPE_FRMR, 0, FRMR_3, NULL);
	}

	else {
		
		/* close the file */
		err = close(p->rx_fds);		
		p->rx_fds=-1;

		if (!err) {
			/* now rename it to the input complete name */
			strcpy(in, p->rxname);

			strcpy(out,in);
			strcat(out,".in");
			strcat(in,".prog");

			err = rename(in, out);
		}
		if (err) p->rx_errno = errno;
		else p->rx_errno=0;

		if (p->rx_errno == 0) {
			/* no errors.  Open the file, read it, checksum it, and close it.
      in this way, if we signal back that the file had no errors, we're
      pretty sure we mean it */
			p->rx_fds = open (out, O_RDONLY);
			if (p->rx_fds==-1) p->rx_errno = errno;
			else {
				rcrc = CRCINIT;
				while ( (rlen=read(p->rx_fds,buf,sizeof(buf))) > 0 ) {
					for (i=0;i<rlen;i++) {
						rcrc = UPDC32 (buf[i],rcrc);
					}
				}
				close(p->rx_fds);
				p->rx_fds=-1;
				if (rlen==-1) p->rx_errno = errno;
				else if (rcrc != crc) {
					sprintf(tbuf,"ERROR: Final CRC mismatch in %s, us:%08x, them: %08x\n",out,
						rcrc,crc);
					pinfo(p,tbuf);
					p->rx_errno = -1;
					remove(out);	/* delete this file */
				}
			}
		}
		
		send_ack_frame(p,NULL, FTYPE_END_ACK, 0, p->rx_errno, NULL);
		if (p->rx_errno==0) {
			if (stat(out,&sbuf)==-1) sbuf.st_size = 0;
			sprintf(tbuf,"INFO: RECEIVE_COMPLETE %s %d code=%d\n",out,sbuf.st_size,
				p->rx_errno);
		}
		else {
			sprintf(tbuf,"ERROR: RECEIVE_ERROR %s code=%d\n",out,
				p->rx_errno);
		}
				
		pinfo(p,tbuf);
	}

	p->rx_state = RX_IDLE;

}

void receive_abort(struct DIAL_PARMS *p, int code)
{
	int err;

	if (p->rx_state!=RX_RECEIVING_DATA) {
		sprintf(tbuf,"ERROR: Received abort when not in receive state\n");
		pinfo(p,tbuf);
		send_ack_frame(p,NULL, FTYPE_FRMR, 0, FRMR_3, NULL);
		close (p->rx_fds);
		p->rx_fds=-1;
	}

	else {
		/* close the file */
		err = close(p->rx_fds);		
		p->rx_fds=-1;

		send_ack_frame(p,NULL, FTYPE_ABORT_ACK, 0, 0, NULL);
		sprintf(tbuf,"INFO: received abort, code=%d\n",code);
		pinfo(p,tbuf);
	}

	p->rx_state = RX_IDLE;
	p->read_timer = time(NULL)+READ_TIMEOUT;

}
				
void rename_txcomplete(struct DIAL_PARMS *p)
{
	char in[120];
	char out[120];
	struct stat sbuf;

	strcpy(in,p->path);
	strcat(in,"/out/");
	strcpy(out,in);

	strcat(in,p->nhead->name);
	strcat(out,p->nhead->name);

	strcat(out,".sent");

	if (rename(in,out)) {
		sprintf(tbuf,"ERROR: error on rename of %s to %s\n",in,out);
		perror_out(p,tbuf,errno);
	}
	else {
		if (stat(out,&sbuf)==-1) sbuf.st_size = 0;
	
		sprintf(tbuf,"INFO: SEND_COMPLETE %s %d\n",out,sbuf.st_size);
		pinfo(p,tbuf);
	}
}

void receive_data(struct DIAL_PARMS *p, char *data, int len, int offset)
{
	int wlen;

	if (p->debug>3) {
		printf("receiving offset %d len %d\n",offset,len);
		fflush(stdout);
	}
	if (p->rx_state!=RX_RECEIVING_DATA) return; 

	if (p->rx_offset != offset) {
		/* not the offset we're looking for.  Either we've missed a frame,
		or this is a resend 'cause the other side missed the ack.  Resend
		the ack */
		
		if (p->debug>3) {
			printf("sending dup ack %d len %d\n",p->rx_offset,len);
			fflush(stdout);
		}
		send_ack_frame (p, NULL, FTYPE_ACK, 0, p->rx_offset, NULL);
	}

	else if ( (wlen = write(p->rx_fds, data, len))!= len) {
		if (wlen>=0) errno = ENOSPC;
		sprintf(tbuf,"ERROR: Error writing %s",p->rxname);
		perror_out(p,tbuf,errno);
		send_data_frame(p,NULL, FTYPE_ABEND, 0, p->rx_errno, NULL);
		p->rx_state = RX_IDLE;
	}
	else {
		p->rx_offset += len;
		if (p->debug>3) {
			printf("sending ack %d len %d\n",p->rx_offset,len);
			fflush(stdout);
		}
		send_ack_frame (p, NULL, FTYPE_ACK, 0, p->rx_offset, NULL);
		p->read_timer = time(NULL)+READ_TIMEOUT;
	}
}

void receive_ack(struct DIAL_PARMS *p, int offset)
{
		/* If this ack is higher than the highest, take it */

	if (p->debug>3) {
		printf("receiving ack %d \n",offset);
		fflush(stdout);
	}
	
	if (p->tx_last_ack< offset) {
		p->tx_last_ack = offset;
		p->tx_retries = 0;
	}

	if (p->tx_last_ack == p->tx_offset) { /* all acked */
		if (p->eof) { /* we're at end of file */
			send_data_frame(p, NULL, FTYPE_END_RECEIVE, 0, p->tx_crc, NULL);
			p->write_timer = time(NULL)+WRITE_TIMEOUT;
			p->tx_state = TX_WF_END_ACK;
			if (p->debug>3) printf("sending EOF for %s\n",p->nhead->name);
		}
	}


	xfer_next_data(p,NULL);

}

void start_receive_restart(struct DIAL_PARMS * p , WORD32 offset, WORD32 crc)
{
	WORD32 newcrc;
	int rlen;
	int remsz;
	int readsz;
	int i;										
	int tsize=0;
	

	if (p->debug>3) printf("start_receive_restart %s, len %d, crc %08x\n",p->nhead->name,offset,crc);
		/* read "offset" bytes, make sure crc matches to that point */

	newcrc = CRCINIT;
	remsz=offset;

	while (remsz>0) {
		if (remsz>sizeof(tbuf)) readsz=sizeof(tbuf);
		else readsz=remsz;

		rlen = read(p->tx_fds, tbuf, readsz);
		if (rlen>0) {
			tsize+=rlen;
			remsz-=rlen;
		}		

		if (rlen<0) {
			p->tx_state = TX_WF_ABORT_ACK;
			p->tx_errno = errno;
			send_ack_frame(p,NULL, FTYPE_ABORT_RECEIVE, 0, p->tx_errno, NULL);
			p->write_timer = time(NULL)+WRITE_TIMEOUT;
			sprintf(tbuf,"ERROR: error on restart read of %s at %d us:%d\n",
					p->nhead->name,tsize);
			pinfo(p,tbuf);
			close(p->tx_fds);
			return;
		}


		if (rlen!=readsz) {
			p->tx_state = TX_WF_ABORT_ACK;
			p->tx_errno = RESTART_SHORT;
			send_ack_frame(p,NULL, FTYPE_ABORT_RECEIVE, 0, p->tx_errno, NULL);
			p->write_timer = time(NULL)+WRITE_TIMEOUT;
			sprintf(tbuf,"ERROR: restart size mismatch on %s: them:%d us:%d\n",
					p->nhead->name,offset,tsize);
			pinfo(p,tbuf);
			close(p->tx_fds);
			return;
		}

		for (i=0;i<rlen;i++) newcrc = UPDC32 (tbuf[i],newcrc);
	}

	if (newcrc != crc) {
		p->tx_state = TX_WF_ABORT_ACK;
		p->tx_errno = RESTART_CRC;
		send_ack_frame(p,NULL, FTYPE_ABORT_RECEIVE, 0, p->tx_errno, NULL);
		p->write_timer = time(NULL)+WRITE_TIMEOUT;
		sprintf(tbuf,"ERROR: restart crc mismatch on %s: them:%08x us:%08x\n",
				p->nhead->name,crc,newcrc);
		pinfo(p,tbuf);
		close(p->tx_fds);
		return;
	}
	else {
		p->eof = 0;
		p->tx_offset = offset;
		p->tx_last_ack = offset;
		p->tx_state = TX_SENDING;
		p->tx_crc = newcrc;
		sprintf(tbuf,"INFO: RESTARTING %s from %d\n",p->nhead->name, offset);
		pinfo(p,tbuf); 
		xfer_next_data(p,NULL);
	}
}

void rx_sm_close(struct DIAL_PARMS *p)
{
	if (p->rx_fds!=-1) close(p->rx_fds);
}	

void tx_sm_close(struct DIAL_PARMS *p)
{
	if (p->tx_fds!=-1) close(p->tx_fds);
}	

/* called by the dialer when it wants to solict files from the client */
void invite_send( struct DIAL_PARMS *p)
{
	if (p->debug>3) printf("invite rx sent\n");
	send_ack_frame(p,NULL, FTYPE_RX_INVITE, 0, 0, NULL);
	p->read_timer = time(NULL) + WRITE_TIMEOUT;  
																/* yes, we're using the read timer for
                                  this one, becase this is really a
                                  "read side" operation, the tx side may
                                  be sending files */
	p->rx_state = RX_INVITE;
}


