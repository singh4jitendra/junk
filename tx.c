#include <stdlib.h>
#include <stdio.h> 

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

#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#ifndef DOS
#include <dirent.h>
#endif
#include "cdial.h"
#include "xfer.h"

#define printf tprintf
#define fprintf tfprintf

static char tbuf[120];
static int have_files ;
extern int errno;

void insert_alpha(struct DIAL_PARMS *, struct FNAMES *);

int init_out_file(struct DIAL_PARMS *p)
{
	char dirname[120];
	DIR *dir;
	struct dirent *dp;
	struct FNAMES *fn;
	char *cf;
	int num;

	p->nhead = NULL; 

	strcpy(dirname,p->path);
	strcat(dirname,"/out");

		/* build up a list of names to send.  Do this now 'cause the 
		directory may change in realtime. */

	if ( (dir = opendir(dirname)) == NULL) {
		sprintf(tbuf,"Can't open output directory %s",dirname);
		perror_out(p,tbuf,errno);
		noretry(p);
		exit (EXIT_FILE_ERR); 
	}
	else {

		while ( (dp=readdir(dir)) !=NULL) {
			if (!interesting_name(dp->d_name)) continue;
			fn = malloc(sizeof(struct FNAMES));
			if (fn==NULL) {
				pinfo(p,"ERROR: no memory to build file list \n");
				exit(EXIT_NO_MEM);
			}
			fn->name = malloc(strlen(dp->d_name)+1);
			if (fn->name==NULL) {
				pinfo(p,"no memory to build file list \n");
				exit(EXIT_NO_MEM);
			}
			strcpy(fn->name, dp->d_name);
			insert_alpha(p,fn);
			num++;
		}
		closedir(dir);
	}

	if (p->debug>20) {
		fn=p->nhead;
		while (fn!=NULL) {
			printf("%s\n",fn->name);
			fn=fn->next;
		}
	}
	return num;
}

	/* pick up the head of the list and start a transfer */
void start_next_out_file(struct DIAL_PARMS * p)
{

	char *buf;
	char fname[120];

	while (p->nhead!=NULL) {
		strcpy(fname,p->path);
		strcat(fname,"/out/");
		strcat(fname,p->nhead->name);

		if ( (p->tx_fds = open(fname,O_RDONLY)) == -1) {
			sprintf(tbuf,"Can't open %s for transmission, skipping\n",
				fname);
			perror_out (p,tbuf,errno);
			advance_nhead(p);
			if (p->nhead == NULL) {
				p->tx_state = TX_DONE;
				break;
			}
		}
		else {		
			p->eof = 0;
			p->tx_offset = 0;
			p->tx_last_ack = 0;
			p->tx_state = TX_WF_OK;
			p->write_timer = time(NULL)+WRITE_TIMEOUT;

			buf = malloc(strlen(p->nhead->name)+4+1);
			if (buf==NULL) return;
			strcpy(buf,p->nhead->name);
			send_data_frame(p,buf, FTYPE_START_RECEIVE, strlen(buf)+1, 0, NULL);
			sprintf(tbuf,"INFO: START_SENDING %s\n",buf);
			pinfo(p,tbuf);	
			break;
		}
	}
	if (p->nhead == NULL) {
		send_tx_done(p);
	}
}


void send_tx_done(struct DIAL_PARMS *p)
{	

	pinfo(p,"INFO: SENT_TX_DONE\n");	

	p->tx_state = TX_WF_DONE_ACK;
	p->write_timer = time(NULL)+WRITE_TIMEOUT;
	send_data_frame(p,NULL,FTYPE_TX_DONE, 0, 0, NULL);
	
}


void advance_nhead (struct DIAL_PARMS *p)
{	
	struct FNAMES *tfh;

	tfh = p->nhead;
	p->nhead = p->nhead->next;
	free (tfh->name);
	free (tfh);
}


/* here we reject ., .., and already sent files.  Return 0 for files
we don't want to send */
int interesting_name(char *s)
{
	if (strcmp(s,".")==0) return 0;
	if (strcmp(s,"..")==0) return 0;

	/* reject files with a tag of .sent */
	if (strcmp(s+strlen(s)-5, ".sent")==0) return 0;

	return 1;

}

/* put this in alphabetic order, since the files may have a relation
to each other based on ascending name */

void insert_alpha(struct DIAL_PARMS *p,struct FNAMES *fn)
{
	struct FNAMES *np;
	struct FNAMES *lp;

		/* list is empty? */
	if (p->nhead == NULL) {
		p->nhead = fn;
		fn->next = NULL;
		return;
	}

		/* new entry is first */
	if (strcmp(fn->name, p->nhead->name)<1) {
			fn->next = p->nhead;
			p->nhead = fn;
			return;
	}

		/* search for proper place */

	lp=p->nhead;		
	np = p->nhead->next;

	while (np!=NULL) {
		if (strcmp(fn->name, np->name)<1) {
			fn->next = np;
			lp->next = fn;
			return;
		}
		lp=np;
		np=np->next;
	}

	/* add at end */
	lp->next = fn;
	fn->next = NULL;
}


		

