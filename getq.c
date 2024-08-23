/****  getq.c - This function is used to read data into an application
                from a flat file used in the WDC queuing system.


Compiling instructions ....
	For the 650:				For the 825:
		cc.10 getq.c -c			     cc getq.c -c
		copy getq.o to /WESCO/obj	     copy getq.o to 
						       /usr/acct/caroline/relsrc

    Author - C. Adamski GT Systems.

    Date   - 08/16/91

*/

#include <sys/types.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

/****  The following structure defines the data that is passed between
       the application and this function.   */

struct msgbuff {
   long mtype;
   long command;
   char fname[64];
   char user[16];
   char qname[8];
   long status;
   int rtnque;
} msg;

struct passdata {
   char command;
   char trantype[8];
   char inbuff[512];
   char qname[8];
   char recno[10];
   char retcode[2];
};

/****  Because of the nature of this function, most of its variables 
       must be static and are therefore declared outside of the 
       function.  */

struct passdata *indata;

char savqname[16], savcomm[10], qdatname[120];
char *getenv(), *name;
int GETQ(),opnfile(), delrec();

/*----------------------------------------------------------------*/
int GETQ(in)
struct passdata *in;
{
int i;

indata = in;
strcpy(indata->retcode,"0");

for (i=0;i<8 && indata->qname[i] != ' ';i++);
indata->qname[i] =0;
for (i=7;indata->trantype[i] != ' ';i--);
indata->trantype[i] =0;
for (i=9;indata->recno[i] != ' ';i--);
indata->recno[i] =0;
strcpy(savqname,indata->qname);

/****  Let's determine what the application wants us to do.  */

switch(indata->command) {

	       /* Return FIRST transaction file name */

   case 'S':   msg.command = 4;
               strcpy(indata->recno,"0");
               strcpy(savcomm,"W");
	       opnfile();
               break;

	       /*  Return current transaction file name -  Wait if none
	           available */

   case 'W':   strcpy(savcomm,"W");

	       /*  Return current transaction file name -  don't wait */

   case 'F':   msg.command = 2;  	/* Don't Wait */
               strcpy(indata->recno,"0");
	       opnfile();    
               break;

	       /* Mark queue record for deletion */

   case 'D':   msg.command = 3;
               strcpy(indata->retcode,"0");
	       delrec();     /*  Mark queue record for deletion */
               break;
   default:    strcpy(indata->retcode,"9");
}
return(0);
}

/*----------------------------------------------------------------*/
/**** This routine is to call the message queue and get a name of a
      file containing transactions. If there is a problem with the
      queue manager we return to the calling routine with a status
      of 1 or 2.*/

int opnfile()
{


int rtnid, qmgrid;
int retry=0, i, mypid;
struct msqid_ds msqbuf;


/**** Setup record so queue manager knows who is calling */

msg.status = 0;
msg.mtype = 1;
if ((strcmp(savcomm,"W")) == 0) {
	msg.mtype = 2;
}
mypid = getpid();

/**** Access message queue */

if ((qmgrid = msgget(1000,0666)) < 0) {
   strcpy(indata->retcode,"1");
   return(0);
}

/**** Access return message queue */

getagain:
if ((msg.rtnque = msgget(1000+mypid,0666 | IPC_CREAT | IPC_EXCL)) < 0) {
   if (errno == EEXIST) {
	msg.rtnque = msgget (1000+mypid,0666);
	msgctl(msg.rtnque,IPC_RMID,&msqbuf);
	goto getagain;
	}
   strcpy(indata->retcode,"1");
   return(0);
}
rtnid = msg.rtnque;

/**** Send message to message queue telling queue manager 
      what we need */

retry_it:
strcpy(msg.qname,savqname);
if (msgsnd(qmgrid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0) < 0) {
   strcpy(indata->retcode,"2");
   return(0);
}

/**** Wait until queue manager tells us message was received.
      The filename we need should also be included in the response. */

if (msgrcv(rtnid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0,0) < 0) {
   strcpy(indata->retcode,"2");
   return(0);
}

/**** If the status is 98, then return to the calling program with an
      'A' in the retcode. This means the calling program should shutdown. */

if (msg.status == 98 || msg.status == 7) {
	strcpy(indata->retcode,"A");
	msgctl(rtnid,IPC_RMID,&msqbuf);     /* Remove return message queue */
	return(0);
}

/**** If process is having trouble with queue understanding our
      commands, try again */

if (retry < 3 && msg.status == 8) {
	retry++;
	goto retry_it;
}

msgctl(rtnid,IPC_RMID,&msqbuf);     /* Remove return message queue */

sprintf (indata->recno,"%d",msg.command);
sprintf (indata->retcode,"%d",msg.status);

if (msg.status != 0) return(0);


/**** Fix up name of file to be sent back to calling routine */

strcpy(indata->inbuff,msg.fname);
for (i=510;indata->inbuff[i] == '0';i--) {
	indata->inbuff[i] = ' ';
}
for (i=7;indata->qname[i] == 0;i--)
   indata->qname[i] = ' ';
for (i=7;indata->trantype[i] == 0; i--)
   indata->trantype[i] = ' ';
for (i=9;indata->recno[i] == 0;i--)
   indata->recno[i] = ' ';
return(0);
}


/*----------------------------------------------------------------*/
/**** This routine will tell queue manager to mark records in queue file
      for deletion. */

int delrec()
{


FILE *delfile;

char buff [80];
int rtnid, qmgrid, i, mypid;
struct msqid_ds msqbuf;
mypid = getpid();

/**** Access message queue */

if ((qmgrid = msgget(1000,0666)) < 0) {
   strcpy(indata->retcode,"1");
   goto delsav;
}

/**** Access return message queue */

getagain2:
if ((msg.rtnque = msgget(1000+mypid,0666 | IPC_CREAT | IPC_EXCL)) < 0) {
   if (errno == EEXIST) {
	msg.rtnque = msgget (1000+mypid,0666);
	msgctl(msg.rtnque,IPC_RMID,&msqbuf);
	goto getagain2;
	}
   strcpy(indata->retcode,"1");
   goto delsav;
}

rtnid = msg.rtnque;

/**** Setup record so queue manager knows who is calling */

msg.status = atoi(indata->recno);

msg.mtype = 1;

/**** Send message to message queue telling queue manager to
      mark record for deletion */

strcpy(msg.qname,savqname);
if (msgsnd(qmgrid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0) < 0) {
   strcpy(indata->retcode,"2");
   goto delsav;
}

/**** Wait until queue manager tells us message was received. */

if (msgrcv(rtnid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0,0) < 0) {
   strcpy(indata->retcode,"2");
   goto delsav;
}

/**** If the status is 98, then return to the calling program with an
      'A' in the retcode. This means the calling program should shutdown. */

if (msg.status == 98) strcpy(indata->retcode,"A");
msgctl(rtnid,IPC_RMID,&msqbuf);
return(0);

delsav:

/**** The below statements are for when the Queue Manager 
      goes down between the call of "opnfile" and "delrec".
      We will write out the necessary info and when the Queue
      Manager comes back these files will be deleted. */


/**** Open temporary save file. */

if((name = getenv("QDATA")) == NULL) {
    printf ("ERROR: Directory environment params not set\n");
    strcpy(indata->retcode,"3");
    return(0);
} 
strcpy (qdatname,name);

strcat (qdatname,"/");
strcat(qdatname,"delfile");
if ((delfile = (FILE *)fopen(qdatname,"a+")) == NULL) {
   strcpy(indata->retcode,"3");
   return(0);
}
msg.status = atoi(indata->recno);
fprintf(delfile,"%d %s\n",msg.status,savqname);
fclose (delfile);
return(0);
}
