/*** qmgr.c - This routine will take care of receiving and sending messages
		between the tandem and tower.

Compiling instructions ....
	For the 650 and 825:
		cc.10 qmgr.c -oqmgr
		copy qmgr to /IMOSDATA/5
		copy qmgr to /SOURCE/TAPE.DIR/825.050800/IMOSDATA/5
		copy qmgr to /SOURCE/TAPE.DIR/05.08.00/IMOSDATA/5


     C. Adamski - G.T. Systems - 8/16/91

		Below is the format of the intermediate flat file....

		When the format of this record changes, change the 
		value of RECLEN and the format statements in the
		#define's.

			col. 1-2	Delete Flag	 1 = not deleted
							77 = deleted
			     3-66	Transaction
					File
			    67-82	User id
			    83-106	Date Record was Written
			   107-130	Date Record was Read
			   131-154	Date Record was Deleted
			   155		\n (End of Line)  
*/

/*  J. Shaver - Now that we're using a PC pkg on a Novell LAN, there will
    12/95       be multiple transactions in each file.  Therefore, qmgr may
                tell someone to shutdown in the middle of the file.  So we
                need to be able to reuse (backup) the same record in the
                .que file after a shutdown.
*/

/*  mb / jls  - Try solving a perplexing problem.  The .que files are getting
    05/99       messed up badly - when EOD comes up deleting files.
                SO.....
                Zero out memory in delete77 so the next string doesn't get 
                erroneously put in the .que file!!!!!                
*/


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#define RECLEN 155 
#define N_QUES 15
#define QUE_LEN 64
#define FSTRING  "%2d%-64s%-16s%24.24s%-24c%-24c\n"
#define FSTRING2 "%24.24s"
#define FSTRING3 "%-154s\n"

extern int errno;

/**** array of queue files (flat files) 
      que_ptr contains the current read  position [0] 
                       the current write position [1]
                       the file open/close status [2] 
                   and the file    waiting status [3] */

static char qnmes[N_QUES][QUE_LEN], sav_quename[QUE_LEN];
static int  que_ptr[N_QUES][4], rdwrt[N_QUES][2];
int nmelen=64, totque=0, do_shutdown=0;

char tmpbuf[RECLEN], buff[512];
char *getenv(), *qdname, *qsname, *qtname, *qbname;
FILE *savfile, *savque, *outfile[N_QUES];

struct msgbuff {	/**** Buffer used for message queue */
   long mtype;
   long command;
   char fname[64];
   char user[16];
   char qname[8];
   long status;
   int rtnque;
} msg;

#ifndef _GNU_SOURCE
extern char *sys_errlist[];
#endif

main(argc, argv)
int	argc;
char	*argv[];
{
long endpos, intime;

int retry=0, restart, waitflg[N_QUES], waitlist[N_QUES][2];
int cnt, i, j, msqid, savrtn, retval, curptr=0;
int flglen=2, dtecol=106, delcol=130;

char waitname[N_QUES][QUE_LEN];
char incom[3], c = ' ', tmpfname[64];

struct tm *mytime;
struct msqid_ds msqbuf;

void tzset(); 
int delete77();

tzset();
for (i=0; i<N_QUES; ++i) {
   outfile[i] = 0;
   waitflg[i] = 0;
}

restart = 0;
for (i=0; i<argc; ++i) {
    if (strcmp(argv[i],"RESTART") == 0) restart = 1;
}

/* Get the directory names from environment parameters */

if((qdname = getenv ("QDATA")) == NULL) {
    printf ("ERROR: Directory environment params not set\n");
    exit(0);
}
    
if((qbname = getenv ("QBACKUP")) == NULL) {
    printf ("ERROR: Directory environment params not set\n");
    exit(0);
}
if((qsname = getenv ("QUEUES")) == NULL) {
    printf ("ERROR: Directory environment params not set\n");
    exit(0);
}
if((qtname = getenv ("QTRANS")) == NULL) {
    printf ("ERROR: Directory environment params not set\n");
    exit(0);
}


/**** This call is intended to create a message queue if one 
      does not already exist. The "0666" is the file permissions. ****/

i = 0;
top:
if((msqid = msgget(1000,IPC_CREAT |IPC_EXCL | 0666)) < 0) {
   if (errno == EEXIST) {
	msqid = msgget(1000,0666);
	
	/**** If original file was created under a different owner
	      and the Queue Manager was abnormally aborted before
	      the queue was deleted then we will have trouble deleting
	      old queue because of privileges. */

	if((i = msgctl(msqid,IPC_RMID,&msqbuf)) < 0) {
	   perror("MSGGET delete privilege - qmgr");
	   exit(0);
	   }
	goto top;
	}
   if(retry < 3) {
	retry++;
	goto top;
   }
   perror("MSGGET - qmgr");
   exit(0);
}


memset(tmpbuf, 0, sizeof(tmpbuf));


/**** If this is a RESTART do not "clean up" */

if (restart == 1) goto noclean;

/**** First we check to see if there is a "savfile" out there. If there
      is then this means the Queue Manager went down in between the
      write and the close in PUTQ. In that case all we have to do is
      que the information to the Queue Manager. */

strcpy (tmpbuf,qdname);
strcat (tmpbuf,"/");
strcat (tmpbuf,"savfile");
if ((savfile = fopen (tmpbuf,"r")) != NULL) {
morechk:
	if((fscanf(savfile,"%s",msg.user)) == EOF) goto donechk;
	if((fscanf(savfile,"%s",msg.qname)) == EOF) goto donechk;
	strcpy(tmpfname,qdname);
	strcat(tmpfname,"/");
	strcat(tmpfname,msg.qname);
	strcat(tmpfname,".que");
	if((fscanf(savfile,"%s",msg.fname)) == EOF) goto donechk;

	if ((savque = fopen (tmpfname,"a+")) != NULL) {
	intime = time ((long *) 0);
	mytime = localtime (&intime);

	/**** Go to next spot on file to write */

	msg.command = 1;
	fprintf(savque,FSTRING,
	  	msg.command,msg.fname,msg.user,asctime((const struct tm *)mytime),c,c);
	fclose(savque);
	}
	goto morechk;
}
donechk:
unlink(tmpbuf);

/**** Next we check to see if there is a "delfile" out there. If there
      is then this means the Queue Manager went down in between the
      read and the delete in GETQ. In that case all we have to do is
      mark the records for deletion. */

strcpy (tmpbuf,qdname);
strcat (tmpbuf,"/");
strcat (tmpbuf,"delfile");
if ((savfile = fopen (tmpbuf,"r")) != NULL) {
moredel:
   if((fscanf(savfile,"%s",msg.fname)) == EOF) goto donedel;
   msg.status = atoi(msg.fname);
   if((fscanf(savfile,"%s",msg.qname)) == EOF) goto donedel;
   strcpy(tmpfname,qdname);
   strcat(tmpfname,"/");
   strcat(tmpfname,msg.qname);
   strcat(tmpfname,".que");

   if ((savque = fopen (tmpfname,"r+")) != NULL) {
	rewind(savque);
	intime = time ((long *) 0);
	mytime = localtime (&intime);

	/**** Go to the spot on file to write */

	fseek (savque,msg.status,0);
	fprintf(savque,"%s","77");
	fseek (savque,msg.status+delcol,0);
	fprintf(savque,FSTRING2,asctime((const struct tm *)mytime));
	fclose(savque);
   }
   goto moredel;
}
donedel:
unlink(tmpbuf);

/*** Copy the transaction files into the backup directory */

system("/IMOSDATA/5/cleanq.sh");

/*** We will not copy QUEUES to QBACKUP.  This was creating   */
/*** subdirectories under QBACKUP.                            */
/*** sprintf(buff,"cd %s;find . -print | cpio -pd %s",qsname,qbname); */
/*** system(buff);                                                    */

/**** Next we will go through any existing que files and delete
      all records marked with a "logical delete" of 77. */

noclean:
if (restart == 1) {
	delete77(1);
}
else {
	delete77(0);
}


puts("Queue Manager is up and ready");

/**** Queue manager is waiting for message.... ****/

readq:
if (msgrcv(msqid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0,0) < 0) {
   perror("msgrcv - qmgr");
   exit(0);
}

/**** If command equals 98, then this means we will send a message
      to a program to shutdown. The message received will contain the
      name of the queue and the read/write status. */

if (msg.command == 98) {
    strcpy (tmpbuf,qdname);
    strcat (tmpbuf,"/");
    strcat (tmpbuf,msg.qname);
    strcat (tmpbuf,".que");
    
    /*  JLS - Add code to save info to pass back next msg that comes in!  */
    /*        So, if it came from PUTQ...   */
    if (msg.status == 0)  {
        strcpy(sav_quename, msg.qname);
        do_shutdown = 1;
    }

    for (i=0; i <= totque; ++i) {
	if (strcmp(qnmes[i],tmpbuf) == 0) {
	   msg.rtnque = rdwrt[i][msg.status];
           if (msg.rtnque != 0) {
              /* JLS - if it didn't come from PUTQ  */
	      if (msg.status != 0) {
                  /* JLS - now backup in file  */
                  que_ptr[i][0] -= RECLEN;
                  if (que_ptr[i][0] < 0) que_ptr[i][0] = 0;

                  if (msgsnd(msg.rtnque,&msg, 
                     (sizeof(msg)-sizeof(msg.mtype)),0) < 0) {
		      perror("98 msgsnd - qmgr");
                      printf("rtnque = %d, addr = %0lX\n",msg.rtnque,&msg);
		      goto readq;
	           }
               }
           }

           /**** If this program is waiting for a filename, remove
                 it from the waiting list ***/

           waitflg[i] = 0;
	   goto readq;
	}
    }
}
savrtn = msg.rtnque;

/**** Check for shutdown command ****/

if (msg.command == 99) goto shutdown;

/**** Pull off name of queue file (flat file) to open.
      Store name in array if not already there.        */


i = strlen (msg.qname);
if (i == 0) strcpy(msg.qname,"qmgr.log");
strcpy (tmpbuf,qdname);
strcat (tmpbuf,"/");
strcat (tmpbuf,msg.qname);
strcat (tmpbuf,".que");
for (i=0; i <= totque; ++i) {
	if (strcmp(qnmes[i],tmpbuf) == 0) {
		curptr = i;
		que_ptr[curptr][2] = 1;
		goto writeque;
	}
}
msg.status = 5; /**** NO QUE FILES */
goto clsfle;

/**** Write out information to queue file (flat file) to be used later ****/

writeque:

if (msg.command == 1) {
	rdwrt[curptr][0] = msg.rtnque;
	intime = time ((long *) 0);
	mytime = localtime (&intime);

	/**** Go to next spot on file to write */

	fseek (outfile[curptr], que_ptr[curptr][1], 0);	

	fprintf(outfile[curptr],FSTRING,
	  	msg.command,msg.fname,msg.user,asctime((const struct tm *)mytime),c,c);
	msg.status = 0;
	
	/**** Move pointer up to new write spot */

	que_ptr[curptr][1] = que_ptr[curptr][1] + RECLEN;

        /*  JLS - if we are to shutdown... */
        if (do_shutdown && (strcmp(msg.qname, sav_quename) == 0))  {
            do_shutdown = 0;
            msg.status = 98;
        }

	}

/**** Read information from queue file (flat file) ****/

else if (msg.command == 2 || msg.command == 4) {
	rdwrt[curptr][1] = msg.rtnque;
	
	/**** Check to see if any records on file */

	if (que_ptr[curptr][0] == 0 &&
	    que_ptr[curptr][1] == 0) {
chkwait:
		/**** See if we should wait for a message */

		if (msg.mtype == 1) {
			msg.status = 5;
			goto clsfle;
		}

		/**** Yes, Add to waiting list */

		waitflg[curptr] = 1;
		waitlist[curptr][0] = msg.command;
		waitlist[curptr][1] = msg.rtnque;
		strcpy(waitname[curptr],qnmes[curptr]);
		que_ptr[curptr][3] = 1;
		que_ptr[curptr][2] = 0;
		if (outfile[curptr] != 0) fclose (outfile[curptr]);
		outfile[curptr] = (FILE *)fopen(qnmes[curptr],"a+");
		goto readq;
	}
endwait:
	if (outfile[curptr] != 0) fclose (outfile[curptr]);
	if((outfile[curptr] = (FILE *)fopen(qnmes[curptr],"r+")) == NULL) {
		perror ("open read msg.qname - qmgr");
		msg.status = 3;
		goto clsfle;
	}

	/**** Go to next spot on file to read */

	if (msg.command == 4) que_ptr[curptr][0] = 0;
readq2:
	fseek (outfile[curptr],-1,2);
	endpos = ftell (outfile[curptr]);
	endpos++;
	if (que_ptr[curptr][0] >= endpos) goto chkwait;

	fseek (outfile[curptr], que_ptr[curptr][0], 0);	
	if (fread (msg.fname,2,1,outfile[curptr]) < 0) {;
	   perror ("outfile read - qmgr");
	   msg.status = 4;
	   goto clsfle;
	}
	msg.fname[2] = 0;
	if (strcmp(msg.fname,"77") == 0) {
		que_ptr[curptr][0] += RECLEN;
		goto readq2;
	}

	/**** Mark the record with a process date stamp */

	intime = time ((long *) 0);	
	mytime = localtime (&intime);
	fseek (outfile[curptr], que_ptr[curptr][0]+dtecol, 0);
	fprintf(outfile[curptr],FSTRING2,asctime((const struct tm *)mytime));

	/**** Pull off transaction file name to open */

	fseek (outfile[curptr], que_ptr[curptr][0]+flglen, 0);
	if(fread (msg.fname, nmelen, 1, outfile[curptr]) < 0) {
	   perror ("outfile read - qmgr");
	   msg.status = 4;
	   goto clsfle;
	}
	for (j=63; j>=0 && msg.fname[j] == ' '; j--);
	j++;
	msg.fname[j] = 0;
	msg.status = 0;

	/**** Return the record pointer to the calling routine.
	      It will be used later to mark record for deletion */

	msg.command = que_ptr[curptr][0];
	
	/**** Move pointer up to new read spot */

	que_ptr[curptr][0] = que_ptr[curptr][0] + RECLEN;
	goto clsfle;
	}

/**** Insert a "logical delete" on record in queue file (flat file) 
      The "logical delete" will be a '77' in the command field ****/

else if (msg.command == 3) {
	if (outfile[curptr] != 0) fclose (outfile[curptr]);
	if((outfile[curptr] = (FILE *)fopen(qnmes[curptr],"r+")) == NULL) {
		perror ("open qnmes - qmgr");
		msg.status = 3;
		goto clsfle;
	}
	/**** The deletion record pointer is in msg.status */

	fseek (outfile[curptr],msg.status,0);
	fprintf(outfile[curptr],"%s","77");
	
	/**** Mark the record with a date stamp */

	intime = time ((long *) 0);
	mytime = localtime (&intime);
	fseek (outfile[curptr],msg.status+delcol, 0);
	fprintf (outfile[curptr], FSTRING2,asctime((const struct tm *)mytime));
	}


else msg.status = 8; /* Unrecognized command */

clsfle:
que_ptr[curptr][2] = 0;
if (outfile[curptr] != 0) fclose (outfile[curptr]);
outfile[curptr] = (FILE *)fopen(qnmes[curptr],"a+");

/**** Let calling process know that message was received and saved ****/

msg.rtnque = savrtn;
if (msgsnd(msg.rtnque,&msg,(sizeof(msg)-sizeof(msg.mtype)),0) < 0) {
   printf("rtnque = %d, addr = %0lX\n",msg.rtnque,&msg);
   perror("clsfle:msgsnd - qmgr");
   goto readq;
}

/**** See if there is anyone waiting for a message */

if (msg.command != 1) goto readq;
if (waitflg[curptr] == 1) {
   waitflg[curptr] = 0;
   msg.command = waitlist[curptr][0];
   msg.rtnque = waitlist[curptr][1];
   savrtn = waitlist[curptr][1];
   que_ptr[curptr][3] = 0;
   goto writeque;
}

/**** Close current queue file (flat file) and go back and wait for next
      message. ****/

goto readq;

/**** Shutdown queue manager. Delete current queue and make sure all
      files are closed.  ****/

shutdown:

/**** If there are any processes waiting for messages, return a status
      of 7 so they know the Queue Manager is shutting down */

for (j=0; j <= N_QUES; ++j) {
   if (waitflg[j] == 1) {
      msg.status = 7;
      msg.command = waitlist[j][0];
      msg.rtnque = waitlist[j][1];
      msgsnd(msg.rtnque,&msg,(sizeof(msg)-sizeof(msg.mtype)),0);
   }
}

/**** Remove the message queue identifier and destroy the
      message queue       */

msgctl(msqid,IPC_RMID,&msqbuf);
puts("Queue Manager is shutting down");

for (i=0; i<=totque; ++i) {
	fclose(outfile[i]); 
	if (que_ptr[i][1] == 0) unlink (qnmes[i]);
}
exit(0);
}

/*----------------------------------------------------------------*/
/* This routine will physically delete any records in que files with
   a 77 in columns 1 and 2. */

int delete77(n)
int n;
{
long endpos;

int i=0, j=0, k=0, l=0, retval;
char incom[3];
FILE *tmpfile, *quefile;

char cpystr[RECLEN], tmpbuf[RECLEN+1];

totque = -1;
for (i=0; i<N_QUES; ++i) outfile[i] = 0;

/**** Open file that contains all the names of the que files */

strcpy (tmpbuf,qdname);
strcat (tmpbuf,"/");
strcat (tmpbuf,"quefiles");
if ((quefile = (FILE *)fopen(tmpbuf,"r+")) == NULL) {
   return(0);
}

/**** Spin through quefiles deleting appropriate records */

queagain:
totque++;
tmpbuf[0] = '\0';
if ((fscanf(quefile,"%s",tmpbuf)) == EOF) {
	if (totque != 0) totque = totque - 1;
	goto top77;
}
strcpy(qnmes[totque],qdname);
strcat(qnmes[totque],"/");
strcat(qnmes[totque],tmpbuf);

if ((outfile[totque] = (FILE *)fopen(qnmes[totque],"r+")) == NULL) {
     outfile[totque] = (FILE *)fopen(qnmes[totque],"a+");
     goto queagain;
}
fclose (outfile[totque]);
outfile[totque] = 0;

/**** Make a temporary copy of the current que file */

strcpy(cpystr,"cp ");
strcat(cpystr,qnmes[totque]);
strcat(cpystr," ");
strcat(cpystr,qsname);
strcat(cpystr,"/quetmp");
retval = system (cpystr);

/**** Open que file and temporary file */

strcpy (cpystr,qsname);
strcat (cpystr,"/quetmp");
tmpfile = fopen (cpystr,"r+");
outfile[totque] = fopen (qnmes[totque],"w+");

/**** If file is empty, go get another que file */

fseek (tmpfile, -1, 2);
endpos = ftell (tmpfile);
endpos = endpos + 1;
if (endpos == 1) {
	fclose (outfile[totque]);
	outfile[totque] = fopen (qnmes[totque],"a+");
	que_ptr[totque][0] = 0;
	que_ptr[totque][1] = 0;
	fclose (tmpfile);
	goto queagain;
}
rewind (tmpfile);
k = 0;
j = 0;

/**** Read in first 2 characters to see if they contain a 77 */

delagain:
	fread (incom,2,1,tmpfile);
	incom[2] = 0;
	msg.command = atoi(incom);
	fread (msg.fname, nmelen, 1, tmpfile);


/**** If they do contain a 77 do not write this record out and
      go read in another. */

	if(msg.command == 77 && n != 1) {
		for (l=63; l>=0 && msg.fname[l] == ' '; l--);
		l++;
		msg.fname[l] = 0;
		unlink (msg.fname);
		k = k + RECLEN;
		if (k >= endpos) goto donedel;
		fseek (tmpfile, k, 0);
		goto delagain;
	}

/**** Write out records that have not been processed as of yet */

	memset(tmpbuf, 0, sizeof(tmpbuf));
	fseek (tmpfile, k, 0);
	fread (tmpbuf,RECLEN-1,1,tmpfile);
	fseek (outfile[totque], j, 0);
	fprintf (outfile[totque], FSTRING3, tmpbuf);

/**** Increment counters for input */

	j = j + RECLEN;
	k = k + RECLEN;
	if (k >= endpos) goto donedel;
	fseek (tmpfile, k, 0);
	goto delagain;

/**** Done with this que file. Reset pointers in array for read and write.
      Go read another que file. */

donedel:
	fclose (tmpfile);
        strcpy (cpystr,qsname);
	strcpy (cpystr,"/quetmp");
	unlink (cpystr);
	que_ptr[totque][0] = 0;
	fseek (outfile[totque], -1, 2);
	endpos = ftell (outfile[totque]);
	endpos = endpos + 1;
	que_ptr[totque][1] = endpos;
	if (endpos == 1) {
		que_ptr[totque][0] = 0;
		que_ptr[totque][1] = 0;
	}
	fclose (outfile[totque]);
	outfile[totque] = fopen (qnmes[totque],"a+");
	goto queagain;

/**** All Done reading que files */

top77:
if (quefile != 0) fclose (quefile);
return(0);
}
