/****  qmgrup - This function is used to check if queue manager is
                up and running.

    Author - R. Wood GT Systems.

    Date   - 06/12/91

*/

/* These INCLUDE files should be in the routine where this function is included
   Right now the only routine using this function is PUTQ

#include <sys/types.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
*/

#ifndef _GNU_SOURCE
extern char *sys_errlist[];
#endif


/*----------------------------------------------------------------*/
qmgrup()
{

/*  This is the format of the messages being passed back and forth in
    the system queues */

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
int retry=0;
struct msqid_ds msqbuf;


if((cuserid(NULL)) == NULL) {
   strcpy(msg.user,"XXXXX");
}
else strcpy(msg.user,cuserid(NULL));
msg.status = 0; strcpy(msg.qname,indata->qname);
msg.command = 0;
msg.mtype = 1;
mypid = getpid();

/*  See if we can get the system queue "1000"
    If not then return a status of 1 */

if ((qmgrid = msgget(1000,0666)) < 0) {
   return(1);
}

/*  See if we can get the system queue "1000+mypid" for return messages
    If not then return a status of 1 */

if ((msg.rtnque = msgget(1000+mypid,0666 | IPC_CREAT)) < 0) {
   return(1);
}

/*  Try and send a message to the return queue
    This message will be sitting in the queue until a "msgrcv" is called
    If not then return a status of 2 */

if (msgsnd(qmgrid,&msg,(sizeof(msg)-sizeof(msg.mtype)),0) < 0) {
	printf("qmgr: msgsnd: %s\n", strerror(errno));
	fflush(stdout);
   return(2);
}

/*  Try and receive a message from the return queue
    The above message should be sitting in queue 
    If not then try to receive 5 times before returning a status of 2 */

retry_it:
if (msgrcv(msg.rtnque,&msg,(sizeof(msg)-sizeof(msg.mtype)),
	0,IPC_NOWAIT) < 0) {
   if (errno == ENOMSG) {
      usleep(100);
      if (retry > 25) {
			printf("qmgrup: msgrcv: %s\n", strerror(errno));
			fflush(stdout);
         return(2);
      }
      else retry++;
      goto retry_it;
   }
   else {
      return(2);
   }
}

/*  Remove return queue from system */

msgctl(msg.rtnque,IPC_RMID,&msqbuf);
return(0);
}
