/*	proccntl - This function will read in the disk file containing the
	           the programs to be used in the process manager system.
	           The disk file is more like a table containing pertanent
	           information needed for this system to work. Below is the
	           format of the records contained in this disk file.

		1-64	Full path name of program
		65	Flag to send message when program is up 1 = yes
		66-69	Message queue number to use on shutdown
		70-133	Current working directory
		134	Flag whether process is included in "ALL" 1 =yes
		135-142	Program mnemonic
		143	1 = up, 0 = down
		144-150 Queue name
		151	1 = read queue, 0 = write queue, 2 = read & write
		152	Number of arguments to pass Max=3
		153-160	Argument 1
		161-168	Argument 2
		169-176	Argument 3
		177	\n

	C. Adamski - G. T. Systems - 9/25/91

Compiling instructions ....
	For the 650:
		cc.10 proccntl.c -oproccntl
		copy proccntl to /IMOSDATA/5
		copy proccntl to /SOURCE/TAPE.DIR/05.08.00/IMOSDATA/5
	For the 825:
		cc proccntl.c -oproccntl
		copy proccntl to /SOURCE/TAPE.DIR/825.050800/IMOSDATA/5
/*
	MJB - 06/02/03 Changed macro to create grepman in /tmp
*/

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#define MAXPRG 15
#define RECLEN 177
#define PATHLEN 64
#define FLGLEN 1
#define QUELEN 4
#define MNELEN 8

#define FSTRING1 "%64.64s"
#define FSTRING2 "%1.1s"
#define FSTRING3 "%4.4s"
#define FSTRING4 "%8.8s"
#define FSTRING5 "%7.7s"
#define FSTRING6 "ps -ef | grep '"
#define FSTRING7 "' |grep -v 'grep'| grep -v 'UP' > /tmp/grepman" 

struct msgargs {	/* Buffer used for programs with arguments */
   long mtype;
   char marg1[9];
   char marg2[9];
   char marg3[9];
} msga;

char pathname [MAXPRG][65], messflg [MAXPRG][2], quenum   [MAXPRG][5];
char currdir  [MAXPRG][65], procflg [MAXPRG][2], progmnem [MAXPRG][9];
char arg1     [MAXPRG][9],  arg2    [MAXPRG][9], arg3     [MAXPRG][9];
char updwn    [MAXPRG][2],  quename [MAXPRG][8], rdwrt    [MAXPRG][2];
char noarg    [MAXPRG][2];
char bldstg   [RECLEN-1],   upchar[2],  dwnchar[2], opnname[120];
char *getenv(), *qdname;

long flg;
int i, j, msqid, prgcnt=0, curptr, endpos=0;
int noargx[MAXPRG], flepos=0, rtn=0;

struct msqid_ds msqbuf;
FILE *dsktable;

main(argc,argv)
int argc;
char *argv[];
{

if((qdname = getenv ("QDATA")) == NULL) {
    printf ("ERROR: Directory environment params not set\n");
    exit(0);
}
strcpy (opnname,qdname);
strcat (opnname,"/pmgrtbl.dat");

strcpy (upchar,"1");
strcpy (dwnchar,"0");

/* Open disk file containing the table information */

if ((dsktable = (FILE *)fopen(opnname,"r+")) == NULL) {
   printf ("BAD OPEN - proccntl\n");
   exit(0);
}

/* Find out how big the file is */

fseek (dsktable,-1,2);		/* find EOF */
endpos = ftell (dsktable);
endpos++;
curptr = 0;

/* Read in a record of the table from disk */

readagain:
fseek(dsktable,curptr,0);
if ((fread(bldstg,RECLEN-1,1,dsktable)) < 0) {
   printf("BAD EOF READ 1 - proccntl\n");
   exit (0);
}

/* Strip off the path name of the program. This should contain the path of 
   where the named program is located. */

sprintf(pathname[prgcnt],FSTRING1,bldstg);
for (j=63; j>=0 && pathname[prgcnt][j] == ' '; j--);
j++;
pathname[prgcnt][j] = 0;

/* Strip off the message flag. If this flag is equal to "1" then when the 
   program is started wi will print a message to the screen. */

flepos = PATHLEN;
sprintf(messflg[prgcnt],FSTRING2,bldstg+(flepos));
messflg[prgcnt][1] = 0;

/* Strip off the Message Queue number. If this field is NOT EQUAL to 
   "0000" then a message will be sent to this queue telling whoever 
   to shutdown. */

flepos = flepos + FLGLEN;
sprintf(quenum[prgcnt],FSTRING3,bldstg+(flepos));
quenum[prgcnt][4] = 0;

/* Strip off the current working directory. This field is really not used yet.*/

flepos = flepos + QUELEN;
sprintf(currdir[prgcnt],FSTRING1,bldstg+(flepos));
for (j=63; j>=0 && currdir[prgcnt][j] == ' '; j--);
j++;
currdir[prgcnt][j] = 0;
 
/* Strip off the process flag. If this equals "1" and "ALL" was passed in 
   as a command, the named program will be included in the process 
   stated (UP or DOWN). */

flepos = flepos + PATHLEN;
sprintf(procflg[prgcnt],FSTRING2,bldstg+(flepos));
procflg[prgcnt][1] = 0;

/* Strip off the program mnemonic. This is the calling name of the program. */

flepos = flepos + FLGLEN;
sprintf(progmnem[prgcnt],FSTRING4,bldstg+(flepos));
for (j=0; progmnem[prgcnt][j] != ' ' && j<8; j++);
progmnem[prgcnt][j] = 0;

/* Strip off the up/down flag. If this is "1" then the program is UP, 
   if this is "0" then the program is DOWN. */

flepos = flepos + MNELEN;
sprintf(updwn[prgcnt],FSTRING2,bldstg+(flepos));
updwn[prgcnt][1] = 0;

/* Strip off the queue name. This should contain the name of the queue, 
   if any, that will be used by the named program in the Queue Manager. */

flepos = flepos + FLGLEN;
sprintf(quename[prgcnt],FSTRING5,bldstg+(flepos));
for (j=0; quename[prgcnt][j] != ' ' && j<7; j++);
quename[prgcnt][j] = 0;

/* Strip off the read/write switch. If this is "1" then the program reads 
   the above queue file, if this is "0" then the program writes to the 
   above queue file, and if this is "2" the program reads and writes to 
   the above queue file. */

flepos = flepos + (MNELEN - 1);
sprintf(rdwrt[prgcnt],FSTRING2,bldstg+(flepos));
rdwrt[prgcnt][1] = 0;

/* Strip off the number of arguments. This will contain the number of 
   arguments to pass to the named program on start up. */

flepos = flepos + FLGLEN;
sprintf(noarg[prgcnt],FSTRING2,bldstg+(flepos));
noarg[prgcnt][1] = 0;
noargx[prgcnt] = atoi(noarg[prgcnt]);
if (noargx[prgcnt] > 0) {

	/* Strip off the first argument */

	flepos = flepos + FLGLEN;
	sprintf(arg1[prgcnt],FSTRING4,bldstg+(flepos));
	for (j=0; arg1[prgcnt][j] != ' ' && j<8; j++);
	arg1[prgcnt][j] = 0;

	/* Strip off the second argument */

	flepos = flepos + MNELEN;
	sprintf(arg2[prgcnt],FSTRING4,bldstg+(flepos));
	for (j=0; arg2[prgcnt][j] != ' ' && j<8; j++);
	arg2[prgcnt][j] = 0;

	/* Strip off the third argument */

	flepos = flepos + MNELEN;
	sprintf(arg3[prgcnt],FSTRING4,bldstg+(flepos));
	for (j=0; arg3[prgcnt][j] != ' ' && j<8; j++);
	arg3[prgcnt][j] = 0;
}


/* Check for the End-of-File */

curptr += RECLEN;
if (curptr >= endpos) goto allready;

/* Go read in next record */

prgcnt++;
goto readagain;

/* Close the control file and find out what they want us to do */

allready:
fclose (dsktable);

/* See if ALL was passed via the command line */

if ((strcmp(argv[1],"ALL")) == 0) {
  if((strcmp(argv[2], "UP")) == 0 || (strcmp(argv[2],"DOWN")) == 0) {

     for (i=0; i<prgcnt+1; ++i) {

	/* If included in "ALL PROCESS" process the command */

	if ((strcmp(procflg[i],"1")) == 0) {
	    rtn = procprog(i,argv); 
	    if (rtn != 0 && rtn != 98 && rtn != 99) {
		printf ("rtn = %d\n",rtn);
		puts("QUEUE PROBLEM");
	    }
	}
    }
  exit (0);
  }
}

/* Process which ever program name was passed in via the command line */

j = 0;
if((strcmp(argv[2], "UP")) == 0 || (strcmp(argv[2],"DOWN")) == 0) {
   for (i=0; i<prgcnt+1; ++i) {
	if ((strcmp(argv[1],progmnem[i])) == 0) {
	   j = 1;
	   rtn = procprog(i,argv);
	   if (rtn != 0 && rtn != 98 && rtn != 99) {
		printf ("rtn = %d\n",rtn);
		puts("QUEUE PROBLEM");
	   }
	}
   }
}
if (j != 1) puts ("INVALID REQUEST");
exit (0);
}
/*--------------------------------------------------------------------*/

procprog(n,argv)
char *argv[];
int n;
{

struct msgbuff {	/* Buffer used for message queues */
   long mtype;
   char pathname[65];
   char messflg[2];
   char quenum[5];
   char currdir[65];
   char procflg[2];
   char progmnem[9];
   char updwn[2];
   char quename[8];
   char rdwrt[2];
   int args;
   int status;
} msgs;

int retval, runswch, retryit;
char bldit [100], bufr [513];
 
/* Get the message queue and set up the message */

msgs.args   = 0;
msgs.status = 0;
msgs.mtype  = 1;

if ((msqid = msgget (1001,0666)) < 0) {
   printf ("MSGGET - proccntl\n");
   exit (0);
}

/* Copy all the necessary information into the message buffer */

strcpy (msgs.pathname,pathname[n]);
strcpy (msgs.messflg,messflg[n]);
strcpy (msgs.quenum,quenum[n]);
strcpy (msgs.currdir,currdir[n]);
strcpy (msgs.procflg,procflg[n]);
strcpy (msgs.progmnem,progmnem[n]);
strcpy (msgs.updwn,updwn[n]);
strcpy (msgs.quename,quename[n]);
strcpy (msgs.rdwrt,rdwrt[n]);
msgs.args = noargx[n];

/* Check to see if program is up. */

strcpy (bldit,FSTRING6);
strcat (bldit,msgs.pathname);
strcat (bldit,FSTRING7);
retval = system (bldit);

retryit=0;
runswch = 0;

grepagain:
dsktable = fopen ("/tmp/grepman","r+");
if ((fgets(bufr,512,dsktable)) == NULL) {  /* If NULL, then EOF */
     retryit++;
     if (retryit >= 5) {
        goto donegrep;
     }
     goto grepagain;
}
runswch = 1;

donegrep:
fclose (dsktable);
unlink("/tmp/grepman");

/* START the current program */

if ((strcmp(argv[2],"UP")) == 0) {

   /* If it is already running but the data file says it is not,
      change the data file */

   if (runswch == 1 && (strcmp(updwn[n],"0") == 0)) goto chngstat;

   /* If it isn't running, start it! */

   if (runswch == 0) {

      /* Send information to the process manager (procmgr) */

      msgs.mtype = 1;
      strcpy (msgs.updwn,"0");
      if (msgsnd(msqid,&msgs,(sizeof(msgs)-sizeof(msgs.mtype)),0) < 0) {
          printf ("MSGSND - proccntl\n");
          exit (0);
      }
	
      /* Send the arguments if necessary */

      if (msgs.args > 0) {
          strcpy (msga.marg1,arg1[n]);
	  strcpy (msga.marg2,arg2[n]);
	  strcpy (msga.marg3,arg3[n]);
	  msga.mtype = 2;
	  if (msgsnd(msqid,&msga,(sizeof(msga)-sizeof(msga.mtype)),0) <0) {
	      printf ("MSGSND - proccntl\n");
	      exit (0);
	  }
      }

      /* Wait for message from process manager saying program is up */

      if (msgrcv(msqid,&msgs,(sizeof(msgs)-sizeof(msgs.mtype)),3,0) < 0) {
	  printf ("MSGRCV - proccntl\n");
	  exit (0);
      }
	
      /* If the program is up and running .... */

      if (msgs.status == 0) {

	   /* If message flag is on print message to screen */

	   if ((strcmp(messflg[n],"1")) == 0) {
		strcpy(bldstg,progmnem[n]);
		strcat(bldstg," is UP.");
		puts (bldstg);
	   }
	
chngstat:
	   /* Change the table to mark this program is "UP" */

	   if ((dsktable = (FILE *)fopen(opnname,"r+")) == NULL) {
	      printf("BAD OPEN 2 - proccntl\n");
	      exit (0);
	   }
 
	   /* If this the Queue Manager under RESTART conditions...   
	         we will mark "qmgr" in the pmgrtbl.dat file as UP
	         since this is the program that will be shutdown. 
	         (The qmgr and qmgr RESTART both shutdown the same
	          way, so we will only use the qmgr shutdown.) */

	   if ((strcmp(progmnem[n],"qmgrR")) == 0) {
		for (i=0; i<prgcnt+1; ++i) {
		   if ((strcmp(progmnem[i],"qmgr")) == 0) {
			curptr = (i * RECLEN) + (RECLEN - 35);
		   }
		}
	   }
	   else curptr = (n * RECLEN) + (RECLEN - 35);
	   fseek(dsktable,curptr,0);
	   fwrite(upchar,1,1,dsktable);
	   fclose(dsktable);
	   strcpy(updwn[n],"1");
	}
	return(msgs.status);
   }
}

/* STOP the current program if it is not already running */

if ((strcmp(argv[2],"DOWN")) == 0) {

   /* If it is not running but the data file says it is,
      change the data file */

   if (runswch == 0 && (strcmp(updwn[n],"1") == 0)) goto chngstt2;

   /* If it is running, kill it! */

   if (runswch == 1) {

	/* Send information to the process manager (procmgr) */

	msgs.mtype = 1;
	strcpy (msgs.updwn,"1");
	if (msgsnd(msqid,&msgs,(sizeof(msgs)-sizeof(msgs.mtype)),0) < 0) {
	   printf ("MSGSND - proccntl\n");
	   exit (0);
	}

	/* Wait for message from process manager saying the program
	   was brought down. */

	if (msgrcv(msqid,&msgs,(sizeof(msgs)-sizeof(msgs.mtype)),3,0) < 0) {
	   printf ("MSGRCV - proccntl\n");
	   exit (0);
	}
	
chngstt2:
	/* Change the table to mark this program is "DOWN" */

	if (msgs.status != 98) {
	   if ((dsktable = (FILE *)fopen(opnname,"r+")) == NULL) {
		printf("BAD OPEN 2 - proccntl\n");
		exit (0);
	   }

	   /* If this the Queue Manager under RESTART conditions...   
	         we will mark "qmgr" in the pmgrtbl.dat file as DOWN
	         since this is the program that will be shutdown. 
	         (The qmgr and qmgr RESTART both shutdown the same
	          way, so we will only use the qmgr shutdown.) */

	   if ((strcmp(progmnem[n],"qmgrR")) == 0) {
		for (i=0; i<prgcnt+1; ++i) {
		   if ((strcmp(progmnem[i],"qmgr")) == 0) {
			curptr = (i * RECLEN) + (RECLEN - 35);
		   }
		}
	   }
	   else curptr = (n * RECLEN) + (RECLEN - 35);
	   fseek(dsktable,curptr,0);
	   fwrite(dwnchar,1,1,dsktable);
	   strcpy(updwn[n],"0");

	   /* If a program relies on the Queue Manager on being UP
	      and we are shutting the Queue Manager DOWN, we must also
	      mark these programs as DOWN in the pmgrtbl.dat file. */
	
	   if ((strcmp(progmnem[n],"qmgr")) == 0 ||
	       (strcmp(progmnem[n],"qmgrR")) ==0) {
		for (i=0; i<prgcnt+1; ++i) {
		   if ((strcmp(quenum[n],quenum[i])) == 0) {
			curptr = (i * RECLEN) + (RECLEN - 35);
	   		fseek(dsktable,curptr,0);
			fwrite(dwnchar,1,1,dsktable);
			strcpy(updwn[i],"0");
		   }
		}
	   }
	   fclose(dsktable);
	}
	return(msgs.status);
   }
   return(99);
}
return(0);
}
