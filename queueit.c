/****  queueit.c - This function is used to pass information
                   to the WDC queuing system.
		
		   The first argument passed is the name of the transaction
		   file. The second argument passed is the name of queue file.

    Author - R. Wood GT Systems.

    Date   - 06/12/91


#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
*/
#define TOOBIG 56


extern char *getcwd();

queueit(passname,quename)
char *passname, *quename;
{

struct msgbuff {
   long mtype;
   long command;
   char fname[64];
   char user[16];
   char qname[8];
   long status;
   int rtnque;
} msg;

int qmgrid;
int mypid;
struct msqid_ds msqbuf;

/**** Set up information that will be sent to the queue manager */


strcpy(msg.qname,quename);
strcpy(msg.fname,passname);
if((cuserid(NULL)) == NULL) {
   strcpy(msg.user,"XXXXX");
}
else strcpy(msg.user,cuserid(NULL));
msg.command = 1;
msg.mtype = 1;
mypid = getpid();

if ((qmgrid = msgget(1000,0666)) < 0) {
   return(1);
}
if ((msg.rtnque = msgget(1000+mypid,0666 | IPC_CREAT)) < 0) {
   return(1);
}
if (msgsnd(qmgrid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0) < 0) {
   return(2);
}
if (msgrcv(msg.rtnque,&msg,(sizeof(msg)-sizeof(msg.mtype)),0,0) < 0) {
    return(2);
}

/**** If the status is 98, then return to the calling program with an
      'A' in the retcode. This means the calling program should shutdown. */

if (msg.status == 98) strcpy(indata->retcode,"A");

msgctl(msg.rtnque,IPC_RMID,&msqbuf); 
if(msg.status == 5) return (5);
return(0);
}
