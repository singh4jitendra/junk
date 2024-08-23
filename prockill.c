/*	prockill.c - this program will shutdown the process manager and 
	             process control.



	C. Adamski - G. T. Systems - 9/25/91

*/
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

struct msgbuff {	/* Buffer used for message queue */
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
}  msg;

main ()
{

int i, j, msqid;

struct msqid_ds msqbuf;

if ((msqid = msgget (1001,0666)) < 0) {
   perror ("MSGGET - prockill");
   exit (0);
}
msg.mtype = 1;
msg.status = 99;
if (msgsnd(msqid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0) < 0) {
   perror ("MSGSND - prockill");
   exit (0);
}
exit (0);
}
